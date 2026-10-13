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
} font_t;

const char *font_init(font_t *self, const char *filename, int ptsize);
const char *font_initEmbedded(font_t *self, int ptsize);
void font_deinit(font_t *self);
typedef void (*font_wrapCallback)(void *udata, const char *str, int len,
                                  int width, int last);

void font_blit(font_t *self, pixel_t *buf, int bufw, int bufh,
               const char *str, int len, int dx, int dy);
int font_getWidth(font_t *self, const char *str, int len);
int font_wrap(font_t *self, const char *str, int limit,
              font_wrapCallback callback, void *udata);


#endif
