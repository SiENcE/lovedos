/**
 * Copyright (c) 2017 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#ifndef DECODER_H
#define DECODER_H

#include "filesystem.h"
#include "sounddata.h"

/* Reads a .wav file in chunks, for audio which is streamed from disk rather
 * than loaded into memory */
typedef struct {
  filesystem_file_t *file;
  int samplerate, bitdepth, channels;
  long dataStart;       /* Offset of the sample data in the file */
  int length;           /* Length in frames */
  int pos;              /* Current frame */
  int bufferFrames;     /* Frames returned per decode */
} decoder_t;

const char *decoder_open(decoder_t **out, const char *filename,
                         int bufferSize);
const char *decoder_decode(decoder_t *self, sounddata_t **out);
void decoder_seek(decoder_t *self, int frame);
void decoder_close(decoder_t *self);

#endif
