/**
 * Copyright (c) 2017 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lib/dmt/dmt.h"
#include "lib/stb/stb_truetype.h"
#include "filesystem.h"
#include "font.h"


static const char *initFont(font_t *self, const void *data, int ptsize) {
  int i;

  /* Init font */
  stbtt_fontinfo font;
  if ( !stbtt_InitFont(&font, data, 0) ) {
    return "could not load font";
  }

  /* Get height and scale */
  int ascent, descent, lineGap;
  stbtt_GetFontVMetrics(&font, &ascent, &descent, &lineGap);
  float scale = stbtt_ScaleForMappingEmToPixels(&font, ptsize);
  self->height = (ascent - descent + lineGap) * scale + 0.5;

  /* Init image */
  int w = 128, h = 128;
retry:
  image_initBlank(&self->image, w, h);

  /* Load glyphs */
  float s = stbtt_ScaleForMappingEmToPixels(&font, 1) /
            stbtt_ScaleForPixelHeight(&font, 1);
  int res = stbtt_BakeFontBitmap(
    data, 0, ptsize * s, self->image.data, w, h, 0, 128, self->glyphs);

  /* Retry with a larger image buffer if the buffer wasn't large enough */
  if (res < 0) {
    w <<= 1;
    h <<= 1;
    image_deinit(&self->image);
    goto retry;
  }

  /* Adjust glyph yoffsets, store integer advances */
  int scaledAscent = ascent * scale + 0.5;
  for (i = 0; i < 128; i++) {
    self->glyphs[i].yoff += scaledAscent;
    self->advance[i] = self->glyphs[i].xadvance;
    self->xoff[i] = self->glyphs[i].xoff;
    self->yoff[i] = self->glyphs[i].yoff;
  }

  /* Init image data and mask */
  for (i = 0; i < w * h; i++) {
    self->image.data[i] = (self->image.data[i] > 127) ? 1 : 0;
    self->image.mask[i] = (self->image.data[i] == 0) ? 0xff : 0;
  }

  /* Return NULL for no error */
  return NULL;
}


const char *font_init(font_t *self, const char *filename, int ptsize) {
  const char *errmsg = NULL;
  void *data = NULL;
  FILE *fp = NULL;
  memset(self, 0, sizeof(*self));

  /* Load font file */
  int size;
  data = filesystem_read(filename, &size);
  if (!data) {
    errmsg = "could not open font file";
    goto fail;
  }

  /* Init font */
  errmsg = initFont(self, data, ptsize);
  if (errmsg) {
    goto fail;
  }

  /* Free font data */
  filesystem_free(data);
  data = NULL;

  return NULL;

fail:
  if (fp) fclose(fp);
  filesystem_free(data);
  return errmsg;
}


const char *font_initEmbedded(font_t *self, int ptsize) {
  #include "font_ttf.h"
  return initFont(self, font_ttf, ptsize);
}


void font_deinit(font_t *self) {
  image_deinit(&self->image);
}


extern int image_blendMode;
extern int image_flip;

void font_blit(font_t *self, pixel_t *buf, int bufw, int bufh,
               const char *str, int len, int dx, int dy
) {
  /* Draws `len` characters of `str`; a `len` of -1 draws the whole string */
  const char *p = str;
  const char *end = str + (len < 0 ? (int) strlen(str) : len);
  int x = dx;
  int y = dy;
  image_rect_t clip = image_getClip(bufw, bufh);

  int oldBlendMode = image_blendMode;
  int oldFlip = image_flip;
  image_blendMode = IMAGE_COLOR;
  image_flip = 0;

  while (p < end) {
    if (*p == '\n') {
      x = dx;
      y += self->height;
    } else {
      int c = *p & 127;
      stbtt_bakedchar *g = &self->glyphs[c];
      int w = g->x1 - g->x0;
      int h = g->y1 - g->y0;
      image_blitClipped(&self->image, buf, bufw, &clip,
                        x + self->xoff[c], y + self->yoff[c], g->x0, g->y0,
                        w, h);
      x += self->advance[c];
    }
    p++;
  }

  image_blendMode = oldBlendMode;
  image_flip = oldFlip;
}


int font_getWidth(font_t *self, const char *str, int len) {
  /* Returns the width of the widest line of the first `len` characters (or
   * the whole string if `len` is -1). Advances are truncated the same way as
   * when drawing */
  const char *p = str;
  const char *end = str + (len < 0 ? (int) strlen(str) : len);
  int width = 0, max = 0;
  for (; p < end; p++) {
    if (*p == '\n') {
      width = 0;
    } else {
      width += self->advance[*p & 127];
      if (width > max) max = width;
    }
  }
  return max;
}


int font_wrap(font_t *self, const char *str, int limit,
              font_wrapCallback callback, void *udata
) {
  /* Breaks the string into lines no wider than `limit`, splitting at spaces;
   * words wider than `limit` get a line of their own. `callback` is called
   * for each line with its text, width and whether it ends a paragraph. The
   * width of the widest line is returned */
  const char *p = str;
  int max = 0;
  for (;;) {
    const char *pend = strchr(p, '\n');
    const char *start = p, *end = p, *q = p;
    int width = 0;
    if (!pend) pend = p + strlen(p);
    while (q < pend) {
      const char *ws = q, *we;
      int w;
      while (ws < pend && *ws == ' ') ws++;
      if (ws == pend) break;
      we = ws;
      while (we < pend && *we != ' ') we++;
      /* Width of the word including the spaces since the end of the line */
      w = font_getWidth(self, end, we - end);
      if (end != start && width + w > limit) {
        if (callback) callback(udata, start, end - start, width, 0);
        if (width > max) max = width;
        start = ws;
        width = font_getWidth(self, ws, we - ws);
      } else {
        width += w;
      }
      end = q = we;
    }
    if (callback) callback(udata, start, end - start, width, 1);
    if (width > max) max = width;
    if (!*pend) break;
    p = pend + 1;
  }
  return max;
}
