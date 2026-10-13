/**
 * Copyright (c) 2017 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#include <string.h>
#include "lib/dmt/dmt.h"
#include "lib/ibxm/ibxm.h"
#include "soundblaster.h"
#include "decoder.h"

/* Modules larger than this aren't loaded */
#define MODULE_MAX_SIZE (4 * 1024 * 1024)
#define HEADER_SIZE 1084
#ifndef MODULE_INTERPOLATION
#define MODULE_INTERPOLATION 0
#endif
#ifndef MODULE_OVERSAMPLING
#define MODULE_OVERSAMPLING 0
#endif


static unsigned read16(const unsigned char *p) {
  return p[0] | (p[1] << 8);
}


static unsigned read32(const unsigned char *p) {
  return p[0] | (p[1] << 8) | (p[2] << 16) | ((unsigned) p[3] << 24);
}


static int isModuleHeader(const unsigned char *h, int n) {
  /* The signatures of the formats IBXM plays: FastTracker 2 XM, Scream
   * Tracker 3 S3M and ProTracker (or compatible) MOD files */
  const unsigned char *t = h + 1080;
  if (n >= 60 && !memcmp(h, "Extended Module:", 16)) return 1;
  if (n >= 96 && !memcmp(h + 44, "SCRM", 4)) return 1;
  if (n < HEADER_SIZE) return 0;
  if (!memcmp(t, "M.K.", 4) || !memcmp(t, "M!K!", 4) ||
      !memcmp(t, "FLT4", 4)) {
    return 1;
  }
  if (t[0] >= '1' && t[0] <= '9' && !memcmp(t + 1, "CHN", 3)) return 1;
  if (t[0] >= '1' && t[0] <= '3' && t[1] >= '0' && t[1] <= '9' &&
      !memcmp(t + 2, "CH", 2)) {
    return 1;
  }
  return 0;
}


int decoder_isModule(const char *filename) {
  unsigned char hdr[HEADER_SIZE];
  int n;
  filesystem_file_t *f = filesystem_open(filename);
  if (!f) return 0;
  n = filesystem_fread(f, hdr, HEADER_SIZE);
  filesystem_fclose(f);
  return isModuleHeader(hdr, n);
}


static const char *openModule(decoder_t *self, filesystem_file_t *f,
                              long fileSize) {
  /* Loads the whole module; IBXM converts its samples to 16bit and keeps
   * its own copy of the patterns, so the file's data is freed afterwards */
  struct data data;
  char message[64];
  int rate = soundblaster_getSampleRate();
  if (fileSize > MODULE_MAX_SIZE) {
    return "module file is too large";
  }
  data.length = fileSize;
  data.buffer = dmt_malloc(fileSize);
  filesystem_fseek(f, 0);
  if (filesystem_fread(f, data.buffer, fileSize) != fileSize) {
    dmt_free(data.buffer);
    return "could not read file";
  }
  message[0] = '\0';
  self->module = module_load(&data, message);
  dmt_free(data.buffer);
  if (!self->module) {
    return "invalid module file";
  }
  self->replay = new_replay(self->module, rate, MODULE_INTERPOLATION);
  if (!self->replay) {
    dispose_module(self->module);
    return "out of memory";
  }
  replay_set_oversampling(self->replay, MODULE_OVERSAMPLING);
  self->type = DECODER_MODULE;
  self->samplerate = rate;
  self->bitdepth = 16;
  self->channels = 2;
  self->length = replay_calculate_duration(self->replay);
  self->mixBuf = dmt_malloc(calculate_mix_buf_len(rate) * sizeof(int));
  if (self->length <= 0) {
    dispose_replay(self->replay);
    dispose_module(self->module);
    dmt_free(self->mixBuf);
    return "empty module";
  }
  return NULL;
}


const char *decoder_open(decoder_t **out, const char *filename,
                         int bufferSize) {
  /* Opens a RIFF WAVE file with 8 or 16 bit PCM data or a tracker module.
   * `bufferSize` is the size in bytes of the chunks returned by
   * decoder_decode() */
  unsigned char hdr[HEADER_SIZE];
  long pos = 12, size, fileSize;
  int format = 0, channels = 0, samplerate = 0, bitdepth = 0, frameSize, got;
  long dataStart = -1, dataSize = 0;
  filesystem_file_t *f = filesystem_open(filename);
  decoder_t *self;

  *out = NULL;
  if (!f) {
    return "could not open file";
  }
  fileSize = filesystem_fsize(f);
  got = filesystem_fread(f, hdr, HEADER_SIZE);
  if (isModuleHeader(hdr, got)) {
    const char *err;
    self = dmt_calloc(1, sizeof(*self));
    err = openModule(self, f, fileSize);
    filesystem_fclose(f);
    if (err) {
      dmt_free(self);
      return err;
    }
    self->bufferFrames = bufferSize / 4;
    if (self->bufferFrames < 1) self->bufferFrames = 1;
    *out = self;
    return NULL;
  }
  if (got < 12 || memcmp(hdr, "RIFF", 4) || memcmp(hdr + 8, "WAVE", 4)) {
    filesystem_fclose(f);
    return "unsupported audio format (expected .wav, .mod, .s3m or .xm)";
  }

  /* Walk the chunks to find "fmt " and "data" */
  while (pos + 8 <= fileSize) {
    filesystem_fseek(f, pos);
    if (filesystem_fread(f, hdr, 8) != 8) break;
    size = read32(hdr + 4);
    if (!memcmp(hdr, "fmt ", 4)) {
      int n = size < 40 ? size : 40;
      if (n < 16 || filesystem_fread(f, hdr, n) != n) break;
      format = read16(hdr);
      channels = read16(hdr + 2);
      samplerate = read32(hdr + 4);
      bitdepth = read16(hdr + 14);
      if (format == 0xfffe && n >= 26) format = read16(hdr + 24);
    } else if (!memcmp(hdr, "data", 4)) {
      dataStart = pos + 8;
      dataSize = size < fileSize - dataStart ? size : fileSize - dataStart;
    }
    if (format && dataStart >= 0) break;
    pos += 8 + size + (size & 1);
  }

  if (!format || dataStart < 0) {
    filesystem_fclose(f);
    return "invalid wav file";
  }
  if (format != 1 || (bitdepth != 8 && bitdepth != 16) ||
      (channels != 1 && channels != 2) || samplerate <= 0) {
    filesystem_fclose(f);
    return "unsupported wav format (expected 8 or 16bit mono or stereo PCM)";
  }

  frameSize = channels * bitdepth / 8;
  self = dmt_calloc(1, sizeof(*self));
  self->file = f;
  self->samplerate = samplerate;
  self->bitdepth = bitdepth;
  self->channels = channels;
  self->dataStart = dataStart;
  self->length = dataSize / frameSize;
  self->bufferFrames = bufferSize / frameSize;
  if (self->bufferFrames < 1) self->bufferFrames = 1;
  *out = self;
  return NULL;
}


static void renderModule(decoder_t *self, short *dst, int frames) {
  /* Renders `frames` frames of the module, one tick at a time, or skips them
   * when `dst` is NULL */
  while (frames > 0) {
    int n, i, v, *src;
    if (self->mixPos >= self->mixLen) {
      self->mixLen = replay_get_audio(self->replay, self->mixBuf, 0);
      self->mixPos = 0;
    }
    n = self->mixLen - self->mixPos;
    if (n > frames) n = frames;
    if (dst) {
      src = self->mixBuf + self->mixPos * 2;
      for (i = 0; i < n * 2; i++) {
        v = src[i];
        dst[i] = v < -32768 ? -32768 : v > 32767 ? 32767 : v;
      }
      dst += n * 2;
    }
    self->mixPos += n;
    frames -= n;
  }
}


const char *decoder_decode(decoder_t *self, sounddata_t **out) {
  /* Returns the next chunk of audio as a new sounddata, or sets `out` to NULL
   * at the end of the file */
  int frames = self->length - self->pos, size;
  const char *err;
  *out = NULL;
  if (frames > self->bufferFrames) frames = self->bufferFrames;
  if (frames <= 0) {
    return NULL;
  }
  if (self->type == DECODER_MODULE) {
    err = sounddata_new(out, frames, self->samplerate, 16, 2);
    if (err) return err;
    renderModule(self, (*out)->data, frames);
    self->pos += frames;
    return NULL;
  }
  err = sounddata_new(out, frames, self->samplerate, self->bitdepth,
                      self->channels);
  if (err) {
    return err;
  }
  size = sounddata_getSize(*out);
  filesystem_fseek(self->file, self->dataStart +
                   (long) self->pos * (size / frames));
  if (filesystem_fread(self->file, (*out)->data, size) != size) {
    sounddata_release(*out);
    *out = NULL;
    return "could not read file";
  }
  self->pos += frames;
  return NULL;
}


void decoder_seek(decoder_t *self, int frame) {
  if (frame < 0) frame = 0;
  if (frame > self->length) frame = self->length;
  self->pos = frame;
  if (self->type == DECODER_MODULE) {
    /* IBXM seeks to the start of a tick; the rest is rendered and dropped
     * so the decoder is exactly at `frame` */
    int reached = replay_seek(self->replay, frame);
    self->mixPos = self->mixLen = 0;
    renderModule(self, NULL, frame - reached);
  }
}


void decoder_close(decoder_t *self) {
  if (self->type == DECODER_MODULE) {
    dispose_replay(self->replay);
    dispose_module(self->module);
    dmt_free(self->mixBuf);
  } else {
    filesystem_fclose(self->file);
  }
  dmt_free(self);
}
