/**
 * Copyright (c) 2017 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#ifndef SOUNDDATA_H
#define SOUNDDATA_H

#include <stdint.h>

/* Raw PCM audio data. It is reference counted so it can be shared by several
 * Sources (see Source:clone()); a new sounddata starts with one reference */
typedef struct {
  int refs;
  int samplerate;
  int bitdepth;   /* 8 (unsigned) or 16 (signed) */
  int channels;   /* 1 or 2 */
  int length;     /* Length in frames (samples per channel) */
  void *data;
} sounddata_t;

const char *sounddata_new(sounddata_t **out, int length, int samplerate,
                          int bitdepth, int channels);
const char *sounddata_newFromFile(sounddata_t **out, const char *filename);
const char *sounddata_newFromMemory(sounddata_t **out, const void *data,
                                    int size);
const char *sounddata_clone(sounddata_t **out, sounddata_t *self);
void sounddata_retain(sounddata_t *self);
void sounddata_release(sounddata_t *self);

int sounddata_getSize(sounddata_t *self);
double sounddata_getSample(sounddata_t *self, int idx);
void sounddata_setSample(sounddata_t *self, int idx, double value);
void sounddata_readStereo(sounddata_t *self, int frame, int count,
                          int16_t *dst);

#endif
