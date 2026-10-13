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
  self->ascent = ascent * scale + 0.5;
  self->descent = descent * scale - 0.5;
  self->lineHeight = 1;

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


const char *font_initImage(font_t *self, image_t *src, const char *glyphs,
                           int spacing) {
  /* Creates a font from an image containing the glyphs in the order of the
   * `glyphs` string, separated by columns whose top pixel has the color of
   * the image's top left pixel (as in LÖVE). Only ASCII characters are
   * supported; other (UTF-8 encoded) characters skip their glyph */
  const unsigned char *p = (const unsigned char*) glyphs;
  pixel_t spacer;
  int x = 0, i, w = src->width, h = src->height;

  memset(self, 0, sizeof(*self));
  if (w <= 0 || h <= 0) {
    return "image font has no pixels";
  }
  image_initBlank(&self->image, w, h);
  memcpy(self->image.data, src->data, w * h);
  memcpy(self->image.mask, src->mask, w * h);
  spacer = src->data[0];

  while (*p) {
    /* Next glyph: skip separator columns, then take the columns up to the
     * next separator */
    int c = *p, start, end;
    if (c >= 0xf0) p += 4; else if (c >= 0xe0) p += 3;
    else if (c >= 0xc0) p += 2; else p += 1;
    while (x < w && src->data[x] == spacer) x++;
    start = x;
    while (x < w && src->data[x] != spacer) x++;
    end = x;
    if (start == end) break;
    if (c < 128) {
      stbtt_bakedchar *g = &self->glyphs[c];
      g->x0 = start; g->x1 = end; g->y0 = 0; g->y1 = h;
      g->xoff = g->yoff = 0;
      g->xadvance = end - start + spacing;
      self->advance[c] = end - start + spacing;
    }
  }

  /* Pixels with the separator color are transparent */
  for (i = 0; i < w * h; i++) {
    if (self->image.data[i] == spacer) {
      self->image.data[i] = 0;
      self->image.mask[i] = 0xff;
    }
  }
  self->height = h;
  self->ascent = h;
  self->descent = 0;
  self->lineHeight = 1;
  self->colored = 1;
  return NULL;
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
  image_blendMode = self->colored ? IMAGE_NORMAL : IMAGE_COLOR;
  image_flip = 0;

  while (p < end) {
    if (*p == '\n') {
      x = dx;
      y += font_getLineSpacing(self);
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


int font_getLineSpacing(font_t *self) {
  return self->height * self->lineHeight + 0.5;
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


typedef struct {
  font_t *font;
  int x, y, limit, align;
  font_spanCallback callback;
  void *udata;
} layout_t;


static void layoutLine(void *udata, const char *str, int len, int width,
                       int last) {
  layout_t *p = udata;
  int x = p->x;

  if (p->align == FONT_ALIGN_JUSTIFY && !last) {
    /* Spread the remaining space over the gaps between words */
    const char *s = str, *end = str + len, *q;
    int gaps = 0, extra = p->limit - width, i = 0;
    for (q = str; q < end; q++) {
      if (*q == ' ' && q > str && q[-1] != ' ') gaps++;
    }
    while (s < end) {
      for (q = s; q < end && *q != ' '; q++);
      if (q > s) p->callback(p->udata, s, q - s, x, p->y);
      x += font_getWidth(p->font, s, q - s);
      for (s = q; q < end && *q == ' '; q++);
      x += font_getWidth(p->font, s, q - s);
      if (q < end && s > str && gaps > 0) {
        x += extra / gaps + (i++ < extra % gaps);
      }
      s = q;
    }
  } else {
    switch (p->align) {
      case FONT_ALIGN_CENTER : x += (p->limit - width) / 2; break;
      case FONT_ALIGN_RIGHT  : x += p->limit - width;       break;
    }
    if (len > 0) p->callback(p->udata, str, len, x, p->y);
  }
  p->y += font_getLineSpacing(p->font);
}


void font_layout(font_t *self, const char *str, int len, int x, int y,
                 int limit, int align, font_spanCallback callback,
                 void *udata) {
  /* Lays out text for drawing, calling `callback` with each span of text
   * and its position. If `limit` is negative the text is not wrapped (only
   * split at newlines) and `align` is ignored. `len` is only used when not
   * wrapping; wrapped text must be NUL terminated */
  layout_t p;
  if (limit < 0) {
    if (len < 0) len = strlen(str);
    callback(udata, str, len, x, y);
    return;
  }
  p.font = self;
  p.x = x;
  p.y = y;
  p.limit = limit;
  p.align = align;
  p.callback = callback;
  p.udata = udata;
  font_wrap(self, str, limit, layoutLine, &p);
}


void font_getBounds(font_t *self, const char *str, int len, int x, int y,
                    image_rect_t *rect) {
  /* Grows `rect` (x1 and y1 exclusive) to include the glyphs of the text as
   * drawn by font_blit() at `x`, `y` */
  const char *p = str;
  const char *end = str + (len < 0 ? (int) strlen(str) : len);
  int cx = x, cy = y;
  for (; p < end; p++) {
    if (*p == '\n') {
      cx = x;
      cy += font_getLineSpacing(self);
    } else {
      int c = *p & 127;
      stbtt_bakedchar *g = &self->glyphs[c];
      int gx = cx + self->xoff[c], gy = cy + self->yoff[c];
      int gw = g->x1 - g->x0, gh = g->y1 - g->y0;
      if (gw > 0 && gh > 0) {
        if (gx < rect->x0) rect->x0 = gx;
        if (gy < rect->y0) rect->y0 = gy;
        if (gx + gw > rect->x1) rect->x1 = gx + gw;
        if (gy + gh > rect->y1) rect->y1 = gy + gh;
      }
      cx += self->advance[c];
    }
  }
}
