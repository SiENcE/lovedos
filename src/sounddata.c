/**
 * Copyright (c) 2017 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#include <string.h>
#include <limits.h>
#include "lib/dmt/dmt.h"
#include "filesystem.h"
#include "sounddata.h"


#define CLAMP(x, a, b) ((x) < (a) ? (a) : (x) > (b) ? (b) : (x))

#define WAV_FORMAT_PCM        0x0001
#define WAV_FORMAT_EXTENSIBLE 0xFFFE


const char *sounddata_new(sounddata_t **out, int length, int samplerate,
                          int bitdepth, int channels) {
  /* Creates a new silent sounddata */
  sounddata_t *self;
  int bytes;
  *out = NULL;
  if (length <= 0) {
    return "sample count must be larger than 0";
  }
  if (samplerate <= 0) {
    return "sample rate must be larger than 0";
  }
  if (bitdepth != 8 && bitdepth != 16) {
    return "bit depth must be 8 or 16";
  }
  if (channels != 1 && channels != 2) {
    return "channel count must be 1 or 2";
  }
  bytes = (bitdepth / 8) * channels;
  if (length > INT_MAX / bytes) {
    return "sample count is too large";
  }
  self = dmt_calloc(1, sizeof(*self));
  self->data = dmt_malloc(length * bytes);
  self->refs = 1;
  self->samplerate = samplerate;
  self->bitdepth = bitdepth;
  self->channels = channels;
  self->length = length;
  /* Silence is 0 for signed 16bit data but 128 for unsigned 8bit data */
  memset(self->data, bitdepth == 8 ? 128 : 0, length * bytes);
  *out = self;
  return NULL;
}


static unsigned read16(const unsigned char *p) {
  return p[0] | (p[1] << 8);
}


static unsigned read32(const unsigned char *p) {
  return p[0] | (p[1] << 8) | (p[2] << 16) | ((unsigned) p[3] << 24);
}


const char *sounddata_newFromMemory(sounddata_t **out, const void *data,
                                    int size) {
  /* Loads a RIFF WAVE file containing 8 or 16 bit PCM data */
  const unsigned char *p = data;
  const unsigned char *end = p + size;
  const unsigned char *fmt = NULL, *pcm = NULL;
  unsigned fmtsize = 0, pcmsize = 0;
  int format, channels, samplerate, bitdepth;
  const char *err;

  *out = NULL;
  if (size < 12 || memcmp(p, "RIFF", 4) || memcmp(p + 8, "WAVE", 4)) {
    return "unsupported audio format (only .wav files are supported)";
  }

  /* Find "fmt " and "data" chunks */
  p += 12;
  while (end - p >= 8) {
    unsigned chunksize = read32(p + 4);
    unsigned avail = end - p - 8;
    if (!memcmp(p, "fmt ", 4)) {
      fmt = p + 8;
      fmtsize = chunksize < avail ? chunksize : avail;
    } else if (!memcmp(p, "data", 4)) {
      /* Truncated files are accepted: use as much data as there is */
      pcm = p + 8;
      pcmsize = chunksize < avail ? chunksize : avail;
    }
    if (chunksize >= avail) {
      break;
    }
    /* Chunks are padded to an even size */
    p += 8 + chunksize + (chunksize & 1);
  }

  if (!fmt || fmtsize < 16) {
    return "invalid wav file: missing fmt chunk";
  }
  if (!pcm) {
    return "invalid wav file: missing data chunk";
  }

  format     = read16(fmt);
  channels   = read16(fmt + 2);
  samplerate = read32(fmt + 4);
  bitdepth   = read16(fmt + 14);
  if (format == WAV_FORMAT_EXTENSIBLE && fmtsize >= 26) {
    /* The first two bytes of the SubFormat GUID hold the actual format */
    format = read16(fmt + 24);
  }
  if (format != WAV_FORMAT_PCM ||
      (bitdepth != 8 && bitdepth != 16) ||
      (channels != 1 && channels != 2)
  ) {
    return "unsupported wav format (expected 8 or 16bit mono or stereo PCM)";
  }
  if (pcmsize < (unsigned) ((bitdepth / 8) * channels)) {
    return "invalid wav file: no audio data";
  }

  err = sounddata_new(out, pcmsize / ((bitdepth / 8) * channels),
                      samplerate, bitdepth, channels);
  if (err) {
    return err;
  }
  memcpy((*out)->data, pcm, sounddata_getSize(*out));
  return NULL;
}


const char *sounddata_newFromFile(sounddata_t **out, const char *filename) {
  int size;
  const char *err;
  void *data = filesystem_read(filename, &size);
  *out = NULL;
  if (!data) {
    return "could not open file";
  }
  err = sounddata_newFromMemory(out, data, size);
  filesystem_free(data);
  return err;
}


const char *sounddata_clone(sounddata_t **out, sounddata_t *self) {
  const char *err = sounddata_new(out, self->length, self->samplerate,
                                  self->bitdepth, self->channels);
  if (err) {
    return err;
  }
  memcpy((*out)->data, self->data, sounddata_getSize(self));
  return NULL;
}


void sounddata_retain(sounddata_t *self) {
  self->refs++;
}


void sounddata_release(sounddata_t *self) {
  if (--self->refs == 0) {
    dmt_free(self->data);
    dmt_free(self);
  }
}


int sounddata_getSize(sounddata_t *self) {
  return self->length * self->channels * (self->bitdepth / 8);
}


double sounddata_getSample(sounddata_t *self, int idx) {
  /* `idx` is the index of the (interleaved) sample, not of the frame. The
   * returned value is in the range -1 to 1 */
  if (self->bitdepth == 16) {
    return ((int16_t*) self->data)[idx] / 32767.;
  }
  return (((uint8_t*) self->data)[idx] - 128) / 127.;
}


void sounddata_setSample(sounddata_t *self, int idx, double value) {
  value = CLAMP(value, -1., 1.);
  if (self->bitdepth == 16) {
    ((int16_t*) self->data)[idx] = value * 32767.;
  } else {
    ((uint8_t*) self->data)[idx] = value * 127. + 128.;
  }
}


void sounddata_readStereo(sounddata_t *self, int frame, int count,
                          int16_t *dst) {
  /* Converts `count` frames starting at `frame` to signed 16bit interleaved
   * stereo. This is called by the mixer from inside the soundblaster's
   * interrupt handler, so it must stay free of floating point maths */
  int i;
  if (self->bitdepth == 16) {
    int16_t *src = (int16_t*) self->data + frame * self->channels;
    if (self->channels == 2) {
      memcpy(dst, src, count * 2 * sizeof(*dst));
    } else {
      for (i = 0; i < count; i++) {
        dst[i * 2] = dst[i * 2 + 1] = src[i];
      }
    }
  } else {
    uint8_t *src = (uint8_t*) self->data + frame * self->channels;
    if (self->channels == 2) {
      for (i = 0; i < count * 2; i++) {
        dst[i] = (src[i] - 128) << 8;
      }
    } else {
      for (i = 0; i < count; i++) {
        dst[i * 2] = dst[i * 2 + 1] = (src[i] - 128) << 8;
      }
    }
  }
}
