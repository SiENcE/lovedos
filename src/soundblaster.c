/**
 * Copyright (c) 2017 Florian Kesseler
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <dos.h>
#include <dpmi.h>
#include <go32.h>
#include <sys/nearptr.h>
#include "soundblaster.h"

#define BYTE(val, byte) (((val) >> ((byte) * 8)) & 0xFF)

// The DMA buffer holds two halves of SOUNDBLASTER_SAMPLES_PER_BUFFER 16 bit
// samples each (interleaved stereo); one half is refilled per interrupt
#define SAMPLE_BUFFER_SIZE (SOUNDBLASTER_SAMPLES_PER_BUFFER * sizeof(int16_t) * 2)
#define SAMPLE_RATE 22050
#define STOP_TIMEOUT_MS 500


// SB16
#define BLASTER_RESET_PORT                   0x6
#define BLASTER_READ_PORT                    0xA
#define BLASTER_WRITE_PORT                   0xC
#define BLASTER_READ_BUFFER_STATUS_PORT      0xE
#define BLASTER_INTERRUPT_ACKNOWLEDGE_8BIT   0xE
#define BLASTER_INTERRUPT_ACKNOWLEDGE_16BIT  0xF
#define BLASTER_MIXER_OUT_PORT               0x4
#define BLASTER_MIXER_IN_PORT                0x5
#define BLASTER_MIXER_INTERRUPT_STATUS       0x82
#define BLASTER_16BIT_INTERRUPT              0x02
#define BLASTER_READ_BUFFER_STATUS_AVAIL     0x80
#define BLASTER_WRITE_BUFFER_STATUS_UNAVAIL  0x80
#define BLASTER_READY_BYTE                   0xAA
#define BLASTER_SET_OUTPUT_SAMPLING_RATE     0x41
#define BLASTER_PROGRAM_16BIT_IO_CMD         0xB0
#define BLASTER_PROGRAM_FLAG_FIFO            0x02
#define BLASTER_PROGRAM_FLAG_AUTO_INIT       0x04
#define BLASTER_PROGRAM_FLAG_INPUT           0x08
#define BLASTER_PROGRAM_8BIT_IO_CMD          0xC0
#define BLASTER_PROGRAM_STEREO               0x20
#define BLASTER_PROGRAM_SIGNED               0x10
#define BLASTER_SPEAKER_ON_CMD               0xD1
#define BLASTER_SPEAKER_OFF_CMD              0xD3
#define BLASTER_EXIT_AUTO_DMA                0xD9
#define BLASTER_GET_DSP_VERSION              0xE1
#define BLASTER_MIN_DSP_VERSION              4


// PIC
#define PIC1_COMMAND   0x20
#define PIC2_COMMAND   0xA0
#define PIC1_DATA      0x21
#define PIC2_DATA      0xA1
#define PIC_EOI        0x20
#define PIC_IRQ07_MAP  0x08
#define PIC_IRQ8F_MAP  0x70


// DMA
#define DMA_DIRECTION_READ_FROM_MEMORY 0x08
#define DMA_TRANSFER_MODE_BLOCK        0x80


static const struct  {
  uint8_t startAddressRegister;
  uint8_t countRegister;
  uint8_t singleChannelMaskRegister;
  uint8_t modeRegister;
  uint8_t flipFlopResetRegister;
  uint8_t pageRegister;
} dmaRegisters[] = {
  { 0x00, 0x01, 0x0A, 0x0B, 0x0C, 0x87 },
  { 0x02, 0x03, 0x0A, 0x0B, 0x0C, 0x83 },
  { 0x04, 0x05, 0x0A, 0x0B, 0x0C, 0x81 },
  { 0x06, 0x07, 0x0A, 0x0B, 0x0C, 0x82 },
  { 0xC0, 0xC2, 0xD4, 0xD6, 0xD8, 0x8F },
  { 0xC4, 0xC6, 0xD4, 0xD6, 0xD8, 0x8B },
  { 0xC8, 0xCA, 0xD4, 0xD6, 0xD8, 0x89 },
  { 0xCC, 0xCE, 0xD4, 0xD6, 0xD8, 0x8A }
};

static volatile int  stopDma = 0;
static uint16_t     *sampleBuffer;
static int           sampleBufferSelector;
static uint16_t      baseAddress;
static uint16_t      irq;
static uint16_t      dmaChannel;
static bool          isrInstalled = false;
static int           writePage = 0;
static bool          blasterInitialized = false;
static uint8_t       oldIrqMaskBit;
static _go32_dpmi_seginfo oldBlasterHandler, newBlasterHandler;
static soundblaster_getSampleProc getSamples;


static void writeDSP(uint8_t value) {
  while((inportb(baseAddress + BLASTER_WRITE_PORT) & 
          BLASTER_WRITE_BUFFER_STATUS_UNAVAIL) != 0) {}

  outportb(baseAddress + BLASTER_WRITE_PORT, value);
}


static uint8_t readDSP() {
  uint8_t status;
  while(((status = inportb(baseAddress + BLASTER_READ_BUFFER_STATUS_PORT)) 
        & BLASTER_READ_BUFFER_STATUS_AVAIL) == 0) {
  }

  return inportb(baseAddress + BLASTER_READ_PORT);
}


static inline void delay3us() {
  uint64_t waited = 0;
  uclock_t lastTime = uclock();
  while(waited < (3*UCLOCKS_PER_SEC) / 1000000) {
    uclock_t nowTime = uclock();

    // Just ignore timer wraps. In the worst case we get a slightly
    // longer delay, but who cares?
    if(nowTime > lastTime) {
      waited += nowTime - lastTime;
    }

    lastTime = nowTime;
  }
}


static int resetBlaster(void) {
  for(int j = 0; j < 1000; ++j) {
    outportb(baseAddress + BLASTER_RESET_PORT, 1);
    delay3us();
    outportb(baseAddress + BLASTER_RESET_PORT, 0);

    if(readDSP() == BLASTER_READY_BYTE) {
      return 0;
    }
  }
  return SOUNDBLASTER_RESET_ERROR;
}


static void soundblasterISR(void) {
  outportb(baseAddress + BLASTER_MIXER_OUT_PORT, 
           BLASTER_MIXER_INTERRUPT_STATUS);
  uint8_t status = inportb(baseAddress + BLASTER_MIXER_IN_PORT);

  if(status & BLASTER_16BIT_INTERRUPT) {
    if(stopDma == 1) {
      writeDSP(BLASTER_EXIT_AUTO_DMA);
      stopDma = 2;
    } else {
      uint8_t* dst = (uint8_t*)(sampleBuffer)
        + writePage * SAMPLE_BUFFER_SIZE / 2;

      memcpy(dst, getSamples(), SAMPLE_BUFFER_SIZE / 2);

      writePage = 1 - writePage;
      inportb(baseAddress + BLASTER_INTERRUPT_ACKNOWLEDGE_16BIT);
    }
  }

  if(irq >= 8) {
    outportb(PIC2_COMMAND, PIC_EOI);
  }
  outportb(PIC1_COMMAND, PIC_EOI);
}


static uint16_t irqToInterruptVector(void) {
  // Map IRQ to interrupt number on the CPU
  return irq < 8
    ? PIC_IRQ07_MAP + irq
    : PIC_IRQ8F_MAP + (irq - 8);
}


static uint16_t picDataPort(void) {
  return irq < 8 ? PIC1_DATA : PIC2_DATA;
}


static uint8_t picIrqBit(void) {
  return 1 << (irq & 7);
}


static int setBlasterISR(void) {
  uint16_t interruptVector = irqToInterruptVector();

  _go32_dpmi_get_protected_mode_interrupt_vector(interruptVector, 
                                                 &oldBlasterHandler);

  // The ISR acknowledges the interrupt itself, so it must not be chained to
  // the previous handler (which would send a second EOI to the PIC)
  newBlasterHandler.pm_offset = (int)soundblasterISR;
  newBlasterHandler.pm_selector = _go32_my_cs();
  if(_go32_dpmi_allocate_iret_wrapper(&newBlasterHandler) != 0) {
    return SOUNDBLASTER_DOS_ERROR;
  }
  _go32_dpmi_set_protected_mode_interrupt_vector(interruptVector,
                                                 &newBlasterHandler);

  // PIC: unmask SB IRQ, remembering its previous state for deinit
  uint8_t irqmask = inportb(picDataPort());
  oldIrqMaskBit = irqmask & picIrqBit();
  outportb(picDataPort(), irqmask & ~picIrqBit());

  isrInstalled = true;
  return 0;
}


static int parseBlasterSettings(void) {
  char const* blasterEnv = getenv("BLASTER");
  if(blasterEnv == 0) {
    return SOUNDBLASTER_ENV_NOT_SET;
  }

  int res = sscanf(blasterEnv, "A%hx I%hu", &baseAddress, &irq);
  if(res < 2) {
    return SOUNDBLASTER_ENV_INVALID;
  }

  // "H" field may be preceeded by any number of other fields, so let's just search it
  char const *dmaLoc = strchr(blasterEnv, 'H');
  if(dmaLoc == NULL) {
    return SOUNDBLASTER_ENV_INVALID;
  }

  dmaChannel = atoi(dmaLoc+1);

  return 0;
}


static int allocSampleBuffer(void) {
  enum { maxRetries = 10 };
  int selectors[maxRetries];
  int count;

  for(count = 0; count < maxRetries; ++count) {
    int segment = __dpmi_allocate_dos_memory((SAMPLE_BUFFER_SIZE+15)>>4, &selectors[count]);
    if(segment == -1) {
      break;
    }

    // The DMA buffer must not cross a 64k boundary of its *physical* address
    // (16 bit DMA channels work on 128k pages, so this covers them as well)
    uint32_t bufferPhys = segment * 16;
    if(bufferPhys % 0x10000 <= 0x10000 - SAMPLE_BUFFER_SIZE) {
      sampleBuffer = (uint16_t*)(__djgpp_conventional_base + bufferPhys);
      memset(sampleBuffer, 0, SAMPLE_BUFFER_SIZE);
      sampleBufferSelector = selectors[count];
      break;
    }
  }

  // Free the unsuitable buffers allocated before the one we kept
  for(int i = 0; i < count; ++i) {
    __dpmi_free_dos_memory(selectors[i]);
  }

  if(sampleBuffer == NULL) {
    return SOUNDBLASTER_ALLOC_ERROR;
  }

  return 0;
}


static int checkDSPVersion(void) {
  writeDSP(BLASTER_GET_DSP_VERSION);
  uint8_t major = readDSP();
  uint8_t minor = readDSP();
  if(major < BLASTER_MIN_DSP_VERSION) {
    fprintf(stderr, "Sound Blaster 16 required (found DSP version %d.%02d)\n",
            major, minor);
    return SOUNDBLASTER_UNSUPPORTED;
  }
  return 0;
}


static void turnSpeakerOn(void) {
  writeDSP(BLASTER_SPEAKER_ON_CMD);
}


static void turnSpeakerOff(void) {
  writeDSP(BLASTER_SPEAKER_OFF_CMD);
}


static void dmaSetupTransfer(int      channel,
                             uint8_t  direction,
                             bool     autoReload,
                             bool     down,
                             uint8_t  mode,
                             uint32_t startAddress,
                             uint32_t count) {

  uint8_t modeByte = direction
                   | mode
                   | ((uint8_t)autoReload << 4)
                   | ((uint8_t)down       << 5)
                   | (channel & 0x03);

  uint8_t maskEnable = (channel & 0x03) | 0x04;
    
  uint32_t offset = startAddress;

  // Special handling of 16 bit DMA channels:
  // The DMA controller needs offset and count to be half the actual value and
  // internally doubles it again
  if(channel > 3) {
    offset >>= 1;
    count >>= 1;
  }

  uint8_t page = BYTE(startAddress, 2);

  outportb(dmaRegisters[channel].singleChannelMaskRegister, maskEnable);
  outportb(dmaRegisters[channel].flipFlopResetRegister,     0x00);
  outportb(dmaRegisters[channel].modeRegister,              modeByte);
  outportb(dmaRegisters[channel].startAddressRegister,      BYTE(offset, 0));
  outportb(dmaRegisters[channel].startAddressRegister,      BYTE(offset, 1));
  outportb(dmaRegisters[channel].countRegister,             BYTE(count-1, 0));
  outportb(dmaRegisters[channel].countRegister,             BYTE(count-1, 1));
  outportb(dmaRegisters[channel].pageRegister,              page);
  outportb(dmaRegisters[channel].singleChannelMaskRegister, maskEnable & 0x03);
}


static void startDMAOutput(void) {
  uint32_t offset = ((uint32_t)sampleBuffer) - __djgpp_conventional_base;

  // Number of 16 bit samples (both channels) after which the DSP interrupts
  uint32_t samples = SOUNDBLASTER_SAMPLES_PER_BUFFER;

  dmaSetupTransfer(dmaChannel, DMA_DIRECTION_READ_FROM_MEMORY, true, false,
                   DMA_TRANSFER_MODE_BLOCK, offset, SAMPLE_BUFFER_SIZE);

  // SB16 setup
  writeDSP(BLASTER_SET_OUTPUT_SAMPLING_RATE);
  writeDSP(BYTE(SAMPLE_RATE, 1));
  writeDSP(BYTE(SAMPLE_RATE, 0));
  writeDSP(BLASTER_PROGRAM_16BIT_IO_CMD
            | BLASTER_PROGRAM_FLAG_AUTO_INIT
            | BLASTER_PROGRAM_FLAG_FIFO);
  writeDSP(BLASTER_PROGRAM_SIGNED | BLASTER_PROGRAM_STEREO);
  writeDSP(BYTE(samples-1, 0));
  writeDSP(BYTE(samples-1, 1));
}


static void deallocSampleBuffer(void);


int soundblaster_init(soundblaster_getSampleProc getsamplesproc) {
  if(!__djgpp_nearptr_enable()) {
    return SOUNDBLASTER_DOS_ERROR;
  }

  int err = parseBlasterSettings();
  if(err != 0) {
    fprintf(stderr, "BLASTER environment variable not set or invalid\n");
    return err;
  }

  err = resetBlaster();
  if(err != 0) {
    fprintf(stderr, "Could not reset Soundblaster\n");
    return err;
  }

  err = checkDSPVersion();
  if(err != 0) {
    return err;
  }

  err = allocSampleBuffer();
  if(err != 0) {
    fprintf(stderr, "Could not allocate sample buffer in conventional memory\n");
    return err;
  }

  getSamples = getsamplesproc;

  err = setBlasterISR();
  if(err != 0) {
    fprintf(stderr, "Could not install Soundblaster interrupt handler\n");
    deallocSampleBuffer();
    return err;
  }
  turnSpeakerOn();
  startDMAOutput();

  blasterInitialized = true;

  return 0;
}


static void deallocSampleBuffer(void) {
  __dpmi_free_dos_memory(sampleBufferSelector);
  sampleBuffer = NULL;
}


static void resetBlasterISR(void) {
  if(isrInstalled) {
    // PIC: restore the previous mask state of the SB IRQ
    uint8_t irqmask = inportb(picDataPort());
    outportb(picDataPort(), (irqmask & ~picIrqBit()) | oldIrqMaskBit);

    _go32_dpmi_set_protected_mode_interrupt_vector(irqToInterruptVector(),
                                                   &oldBlasterHandler);
    _go32_dpmi_free_iret_wrapper(&newBlasterHandler);
    isrInstalled = false;
  }
}


static void stopDMAOutput(void) {
  // Let the ISR end auto-init DMA at the next block boundary. Give up after a
  // timeout so a missing interrupt can't hang the program on exit
  uclock_t start = uclock();
  stopDma = 1;
  while(stopDma == 1) {
    if(uclock() - start > (uclock_t)UCLOCKS_PER_SEC * STOP_TIMEOUT_MS / 1000) {
      writeDSP(BLASTER_EXIT_AUTO_DMA);
      stopDma = 2;
      break;
    }
  }
}


void soundblaster_deinit(void) {
  if(blasterInitialized) {
    turnSpeakerOff();
    stopDMAOutput();
    resetBlaster();
    resetBlasterISR();
    deallocSampleBuffer();
    blasterInitialized = false;
  }
}


int soundblaster_getSampleRate(void) {
  return SAMPLE_RATE;
}


int soundblaster_getSampleBufferSize(void) {
  return SOUNDBLASTER_SAMPLES_PER_BUFFER;
}
