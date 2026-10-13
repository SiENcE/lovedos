/**
 * Copyright (c) 2017 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#include <string.h>
#include "lib/dmt/dmt.h"
#include "decoder.h"


static unsigned read16(const unsigned char *p) {
  return p[0] | (p[1] << 8);
}


static unsigned read32(const unsigned char *p) {
  return p[0] | (p[1] << 8) | (p[2] << 16) | ((unsigned) p[3] << 24);
}


const char *decoder_open(decoder_t **out, const char *filename,
                         int bufferSize) {
  /* Opens a RIFF WAVE file with 8 or 16 bit PCM data. `bufferSize` is the
   * size in bytes of the chunks returned by decoder_decode() */
  unsigned char hdr[40];
  long pos = 12, size, fileSize;
  int format = 0, channels = 0, samplerate = 0, bitdepth = 0, frameSize;
  long dataStart = -1, dataSize = 0;
  filesystem_file_t *f = filesystem_open(filename);
  decoder_t *self;

  *out = NULL;
  if (!f) {
    return "could not open file";
  }
  fileSize = filesystem_fsize(f);
  if (filesystem_fread(f, hdr, 12) != 12 ||
      memcmp(hdr, "RIFF", 4) || memcmp(hdr + 8, "WAVE", 4)) {
    filesystem_fclose(f);
    return "unsupported audio format (only .wav files are supported)";
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
}


void decoder_close(decoder_t *self) {
  filesystem_fclose(self->file);
  dmt_free(self);
}
