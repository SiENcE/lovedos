/**
 * Copyright (c) 2017 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#ifndef SPATIAL_H
#define SPATIAL_H

/* Positional (3D) audio, following the OpenAL model used by LÖVE: distance
 * attenuation, sound cones, stereo panning and the doppler effect. All of it
 * is computed when a source or the listener changes, never per sample, so it
 * adds no work to the mixer running in the soundblaster's interrupt */

typedef struct {
  double x, y, z;
} spatial_vec3_t;

enum {
  SPATIAL_NONE,
  SPATIAL_INVERSE,
  SPATIAL_INVERSE_CLAMPED,
  SPATIAL_LINEAR,
  SPATIAL_LINEAR_CLAMPED,
  SPATIAL_EXPONENT,
  SPATIAL_EXPONENT_CLAMPED,
};

typedef struct {
  spatial_vec3_t position;
  spatial_vec3_t velocity;
  spatial_vec3_t forward;
  spatial_vec3_t up;
  int distanceModel;
  double dopplerScale;
} spatial_listener_t;

typedef struct {
  spatial_vec3_t position;
  spatial_vec3_t velocity;
  spatial_vec3_t direction;
  int relative;
  double refDistance, maxDistance, rolloff;
  double innerCone, outerCone, outerConeVolume;
} spatial_source_t;

extern spatial_listener_t spatial_listener;

void spatial_initSource(spatial_source_t *self);
void spatial_compute(const spatial_source_t *self, double *gain, double *pan,
                     double *pitch);

#endif
