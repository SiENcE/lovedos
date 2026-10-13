/**
 * Copyright (c) 2017 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#ifndef FONT_H
#define FONT_H

#include "image.h"
#include "lib/stb/stb_truetype.h"

typedef struct {
  image_t image;
  stbtt_bakedchar glyphs[128];
  /* Integer copies of the glyphs' metrics, avoiding float maths per glyph */
  int advance[128], xoff[128], yoff[128];
  int height;
  int colored;        /* Image fonts keep their colors, others use the
                       * current color */
} font_t;

enum { FONT_ALIGN_LEFT, FONT_ALIGN_CENTER, FONT_ALIGN_RIGHT,
       FONT_ALIGN_JUSTIFY };

const char *font_init(font_t *self, const char *filename, int ptsize);
const char *font_initEmbedded(font_t *self, int ptsize);
const char *font_initImage(font_t *self, image_t *src, const char *glyphs,
                           int spacing);
void font_deinit(font_t *self);
typedef void (*font_wrapCallback)(void *udata, const char *str, int len,
                                  int width, int last);
typedef void (*font_spanCallback)(void *udata, const char *str, int len,
                                  int x, int y);

void font_blit(font_t *self, pixel_t *buf, int bufw, int bufh,
               const char *str, int len, int dx, int dy);
int font_getWidth(font_t *self, const char *str, int len);
int font_wrap(font_t *self, const char *str, int limit,
              font_wrapCallback callback, void *udata);
void font_layout(font_t *self, const char *str, int len, int x, int y,
                 int limit, int align, font_spanCallback callback,
                 void *udata);
void font_getBounds(font_t *self, const char *str, int len, int x, int y,
                    image_rect_t *rect);


#endif
