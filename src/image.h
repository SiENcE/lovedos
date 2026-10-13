/**
 * Copyright (c) 2017 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#ifndef IMAGE_H
#define IMAGE_H

#include "vga.h"
#include "luaobj.h"


typedef enum {
  IMAGE_NORMAL,
  IMAGE_FAST,
  IMAGE_AND,
  IMAGE_OR,
  IMAGE_COLOR,
} IMAGE_BLEND_MODE;


typedef struct {
  int x0, y0, x1, y1;
} image_rect_t;


typedef struct {
  pixel_t *data;
  pixel_t *mask;
  int width, height;
} image_t;


static inline
void image_setPixel(image_t* self, int x, int y, pixel_t val) {
  if (x >= 0 && x < self->width && y >= 0 && y < self->height) {
    self->data[x + y * self->width] = val;
  }
}

static inline
void image_setMaskPixel(image_t* self, int x, int y, pixel_t val) {
  if (x >= 0 && x < self->width && y >= 0 && y < self->height) {
    self->mask[x + y * self->width] = val;
  }
}

void image_setColor(pixel_t color);
void image_setBlendMode(int mode);
void image_setFlip(int mode);
void image_setClip(int x0, int y0, int x1, int y1);
image_rect_t image_getClip(int bufw, int bufh);

const char *image_init(image_t *self, const char *filename);
void image_initBlank(image_t*, int, int);
void image_blit(image_t *self, pixel_t *buf, int bufw, int bufh,
                int dx, int dy, int sx, int sy, int sw, int sh);
void image_blitClipped(image_t *self, pixel_t *buf, int bufw,
                       const image_rect_t *clip,
                       int dx, int dy, int sx, int sy, int sw, int sh);
void image_drawTransformed(image_t *self, pixel_t *buf, int bufw, int bufh,
                           int sx, int sy, int sw, int sh,
                           double x, double y, double r,
                           double scalex, double scaley,
                           double ox, double oy);
void image_deinit(image_t*);

#endif
