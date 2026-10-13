/**
 * Copyright (c) 2017 Florian Kesseler
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#ifndef SOUNDBLASTER_H
#define SOUNDBLASTER_H

#include <stdint.h>

// Error codes
#define SOUNDBLASTER_ENV_NOT_SET 1
#define SOUNDBLASTER_ENV_INVALID 2
#define SOUNDBLASTER_DOS_ERROR   3
#define SOUNDBLASTER_RESET_ERROR 4
#define SOUNDBLASTER_ALLOC_ERROR 5
#define SOUNDBLASTER_UNSUPPORTED 6

// Number of 16 bit samples requested per callback: interleaved stereo, so this
// is twice the number of frames
#define SOUNDBLASTER_SAMPLES_PER_BUFFER 2048

// Called from the interrupt handler to get the next
// SOUNDBLASTER_SAMPLES_PER_BUFFER interleaved stereo samples. It must not use
// the FPU, allocate memory or call into DOS
typedef int16_t const* (*soundblaster_getSampleProc)(void);

int soundblaster_init(soundblaster_getSampleProc sampleproc);
void soundblaster_deinit(void);
int soundblaster_getSampleRate(void);
int soundblaster_getSampleBufferSize(void);

#endif
