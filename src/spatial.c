/**
 * Copyright (c) 2017 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#include <math.h>
#include <float.h>
#include "spatial.h"


#define CLAMP(x, a, b) ((x) < (a) ? (a) : (x) > (b) ? (b) : (x))
#define SPEED_OF_SOUND 343.3


spatial_listener_t spatial_listener = {
  { 0, 0, 0 },          /* position */
  { 0, 0, 0 },          /* velocity */
  { 0, 0, -1 },         /* forward */
  { 0, 1, 0 },          /* up */
  SPATIAL_INVERSE_CLAMPED,
  1,                    /* doppler scale */
};


void spatial_initSource(spatial_source_t *self) {
  spatial_vec3_t zero = { 0, 0, 0 };
  self->position = zero;
  self->velocity = zero;
  self->direction = zero;
  self->relative = 0;
  self->refDistance = 1;
  self->maxDistance = FLT_MAX;
  self->rolloff = 1;
  self->innerCone = 2 * M_PI;
  self->outerCone = 2 * M_PI;
  self->outerConeVolume = 0;
}


static spatial_vec3_t sub(spatial_vec3_t a, spatial_vec3_t b) {
  spatial_vec3_t r = { a.x - b.x, a.y - b.y, a.z - b.z };
  return r;
}


static double dot(spatial_vec3_t a, spatial_vec3_t b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}


static spatial_vec3_t cross(spatial_vec3_t a, spatial_vec3_t b) {
  spatial_vec3_t r = {
    a.y * b.z - a.z * b.y,
    a.z * b.x - a.x * b.z,
    a.x * b.y - a.y * b.x,
  };
  return r;
}


static double length(spatial_vec3_t a) {
  return sqrt(dot(a, a));
}


static double distanceGain(const spatial_source_t *self, double dist) {
  double ref = self->refDistance;
  double max = self->maxDistance;
  double rolloff = self->rolloff;
  double gain;

  switch (spatial_listener.distanceModel) {
    case SPATIAL_INVERSE_CLAMPED:
    case SPATIAL_LINEAR_CLAMPED:
    case SPATIAL_EXPONENT_CLAMPED:
      /* OpenAL disables attenuation if the distances are inconsistent */
      if (max < ref) return 1;
      dist = CLAMP(dist, ref, max);
      break;
  }

  switch (spatial_listener.distanceModel) {
    case SPATIAL_INVERSE:
    case SPATIAL_INVERSE_CLAMPED:
      gain = ref + rolloff * (dist - ref);
      return gain > 0 ? ref / gain : 1;

    case SPATIAL_LINEAR:
    case SPATIAL_LINEAR_CLAMPED:
      if (max <= ref) return 1;
      dist = dist < max ? dist : max;
      gain = 1 - rolloff * (dist - ref) / (max - ref);
      return gain > 0 ? gain : 0;

    case SPATIAL_EXPONENT:
    case SPATIAL_EXPONENT_CLAMPED:
      if (dist <= 0 || ref <= 0) return 1;
      return pow(dist / ref, -rolloff);

    default:
      return 1;
  }
}


static double coneGain(const spatial_source_t *self, spatial_vec3_t toListener,
                       double dist) {
  double dirlen = length(self->direction);
  double angle;
  if (dirlen == 0 || dist == 0) {
    return 1;
  }
  angle = acos(CLAMP(dot(self->direction, toListener) / (dirlen * dist),
                     -1., 1.));
  if (angle <= self->innerCone / 2) {
    return 1;
  }
  if (angle >= self->outerCone / 2) {
    return self->outerConeVolume;
  }
  return 1 + (self->outerConeVolume - 1) * (angle - self->innerCone / 2) /
             ((self->outerCone - self->innerCone) / 2);
}


static double dopplerShift(const spatial_source_t *self,
                           spatial_vec3_t listenerVelocity,
                           spatial_vec3_t toListener, double dist) {
  /* Velocities are projected onto the source-to-listener vector, so a
   * positive velocity means moving towards the listener */
  double scale = spatial_listener.dopplerScale;
  double limit, vls, vss;
  if (scale <= 0 || dist == 0) {
    return 1;
  }
  limit = SPEED_OF_SOUND / scale;
  vls = dot(toListener, listenerVelocity) / dist;
  vss = dot(toListener, self->velocity) / dist;
  vls = vls < limit ? vls : limit;
  vss = vss < limit ? vss : limit;
  return (SPEED_OF_SOUND - scale * vls) / (SPEED_OF_SOUND - scale * vss);
}


void spatial_compute(const spatial_source_t *self, double *gain, double *pan,
                     double *pitch) {
  /* Relative sources are positioned in the listener's own coordinate system,
   * where +x is right, +y up and -z forward */
  spatial_vec3_t right = { 1, 0, 0 };
  spatial_vec3_t listenerVelocity = { 0, 0, 0 };
  spatial_vec3_t toSource = self->position;
  spatial_vec3_t toListener;
  double dist;

  if (!self->relative) {
    toSource = sub(self->position, spatial_listener.position);
    listenerVelocity = spatial_listener.velocity;
    right = cross(spatial_listener.forward, spatial_listener.up);
  }
  toListener.x = -toSource.x;
  toListener.y = -toSource.y;
  toListener.z = -toSource.z;
  dist = length(toSource);

  *gain = distanceGain(self, dist);
  if (*gain > 0) {
    *gain *= coneGain(self, toListener, dist);
  }

  *pan = 0;
  if (dist > 0) {
    double rightlen = length(right);
    if (rightlen > 0) {
      *pan = CLAMP(dot(toSource, right) / (rightlen * dist), -1., 1.);
    }
  }

  *pitch = dopplerShift(self, listenerVelocity, toListener, dist);
}
