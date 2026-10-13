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

enum { DECODER_WAV, DECODER_MODULE };

/* Reads a .wav file in chunks, for audio which is streamed from disk rather
 * than loaded into memory, or renders a tracker module (.mod, .s3m or .xm) */
typedef struct {
  int type;
  filesystem_file_t *file;  /* DECODER_WAV */
  struct module *module;    /* DECODER_MODULE */
  struct replay *replay;
  int *mixBuf;              /* One tick of rendered module audio */
  int mixPos, mixLen;       /* Frames used and available in mixBuf */
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
int decoder_isModule(const char *filename);
void decoder_close(decoder_t *self);

#endif
