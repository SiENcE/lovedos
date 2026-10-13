/**
 * Copyright (c) 2017 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>

#include "lib/dmt/dmt.h"
#include "lib/stb/stb_image.h"
#include "filesystem.h"
#include "image.h"
#include "palette.h"

int image_blendMode = IMAGE_NORMAL;
int image_flip = 0;
unsigned int image_color = 0x0f0f0f0f;
image_rect_t image_clip = { INT_MIN, INT_MIN, INT_MAX, INT_MAX };


void image_setBlendMode(int mode) {
  image_blendMode = mode;
}

void image_setColor(pixel_t color) {
  image_color = color | (color << 8) | (color << 16) | (color << 24);
}

void image_setFlip(int mode) {
  image_flip = !!mode;
}

void image_setClip(int x0, int y0, int x1, int y1) {
  /* Sets the rectangle (x1 and y1 are exclusive) outside of which nothing is
   * drawn by image_blit() and image_drawTransformed() */
  image_clip.x0 = x0;
  image_clip.y0 = y0;
  image_clip.x1 = x1;
  image_clip.y1 = y1;
}

image_rect_t image_getClip(int bufw, int bufh) {
  /* Returns the clip rectangle intersected with a buffer's bounds */
  image_rect_t r = image_clip;
  if (r.x0 < 0) r.x0 = 0;
  if (r.y0 < 0) r.y0 = 0;
  if (r.x1 > bufw) r.x1 = bufw;
  if (r.y1 > bufh) r.y1 = bufh;
  return r;
}



const char *image_init(image_t *self, const char *filename) {
  /* Loads an image file into the struct and inits the mask */
  const char *errmsg = NULL;
  void *filedata = NULL;
  unsigned char *data32 = NULL;
  memset(self, 0, sizeof(*self));

  /* Load file data */
  int size;
  filedata = filesystem_read(filename, &size);
  if (!filedata) {
    errmsg = "could not read file";
    goto fail;
  }

  /* Load 32bit image data */
  int width, height, n;
  data32 = stbi_load_from_memory(filedata, size, &width, &height, &n, 4);
  if (!data32) {
    errmsg = "could not load image file";
    goto fail;
  }

  /* Free file data */
  filesystem_free(filedata);
  filedata = NULL;

  /* Set dimensions and allocate memory */
  int sz = width * height;
  self->width = width;
  self->height = height;
  self->data = dmt_malloc(sz);

  /* Load pixels into struct, converting 32bit to 8bit paletted */
  int i;
  for (i = 0; i < width * height; i++) {
    unsigned char *p = data32 + i * 4;
    int r = p[0];
    int g = p[1];
    int b = p[2];
    int a = p[3];
    int idx = palette_colorToIdx(r, g, b);
    if (idx < 0) {
      errmsg = "color palette exhausted: use fewer unique colors";
      goto fail;
    }
    self->data[i] = (a >= 127) ? idx : 0;
  }

  /* Init mask */
  self->mask = dmt_malloc(sz);
  for (i = 0; i < sz; i++) {
    self->mask[i] = (self->data[i] == 0) ? 0xFF : 0x00;
  }

  /* Free 32bit pixel data, return NULL for no error */
  free(data32);
  data32 = NULL;

  return NULL;

fail:
  filesystem_free(filedata);
  free(data32);
  return errmsg;
}


void image_initBlank(image_t *self, int width, int height) {
  /* Creates a blank zeroset image with a zeroset mask. This function can be
   * used to init the image instead of image_init() */
  memset(self, 0, sizeof(*self));
  self->data = dmt_calloc(1, width * height);
  self->width = width;
  self->height = height;
  /* Init mask */
  self->mask = dmt_calloc(1, width * height);
}


void image_blit(image_t *self, pixel_t *buf, int bufw, int bufh,
                int dx, int dy, int sx, int sy, int sw, int sh
) {
  image_rect_t clip = image_getClip(bufw, bufh);
  image_blitClipped(self, buf, bufw, &clip, dx, dy, sx, sy, sw, sh);
}


void image_blitClipped(image_t *self, pixel_t *buf, int bufw,
                       const image_rect_t *cliprect,
                       int dx, int dy, int sx, int sy, int sw, int sh
) {
  /* Like image_blit(), but takes the clip rectangle (already intersected with
   * the buffer's bounds) so it can be worked out once for many blits */
  int diff;
  image_rect_t clip = *cliprect;

  /* Clip to source buffer. When flipped the source's left edge is drawn on
   * the right, so only clipping its right edge moves the destination */
  if ((diff = -sx) > 0) { sw -= diff; sx = 0; if (!image_flip) dx += diff; }
  if ((diff = -sy) > 0) { sh -= diff; sy = 0; dy += diff; }
  if ((diff = (sx + sw) - self->width) > 0) {
    sw -= diff;
    if (image_flip) dx += diff;
  }
  if ((diff = (sy + sh) - self->height) > 0) { sh -= diff; }

  /* Clip to destination clip rectangle */
  if (!image_flip) {
    if ((diff = clip.x0 - dx) > 0) { sw -= diff; sx += diff; dx += diff; }
    if ((diff = dx + sw - clip.x1) > 0) { sw -= diff; }
  } else {
    if ((diff = clip.x0 - dx) > 0) { sw -= diff; dx += diff; }
    if ((diff = dx + sw - clip.x1) > 0) { sx += diff; sw -= diff; }
  }
  if ((diff = dy + sh - clip.y1) > 0) { sh -= diff; }
  if ((diff = clip.y0 - dy) > 0) { sh -= diff; sy += diff; dy += diff; }

  /* Return early if we're clipped entirely off the dest / source */
  if (sw <= 0 || sh <= 0) return;

  /* Blit */
  #define BLIT_LOOP_NORMAL(func)\
    {\
      int x, y;\
      int srci = sx + sy * self->width;\
      int dsti = dx + dy * bufw;\
      int srcrowdiff = self->width - sw;\
      int dstrowdiff = bufw - sw;\
      int sw32 = sw - (sw & 3);\
      for (y = 0; y < sh; y++) {\
        for (x = 0; x < sw32; x += 4) {\
          func(*(unsigned int*)&buf[dsti],\
               *(unsigned int*)&self->data[srci],\
               *(unsigned int*)&self->mask[srci])\
          srci += 4;\
          dsti += 4;\
        }\
        for (; x < sw; x++) {\
          func(buf[dsti], self->data[srci], self->mask[srci])\
          srci++;\
          dsti++;\
        }\
        srci += srcrowdiff;\
        dsti += dstrowdiff;\
      }\
    }

  #define BLIT_LOOP_FLIPPED(func)\
    {\
      int x, y;\
      int srci = sx + sy * self->width + sw - 1;\
      int dsti = dx + dy * bufw;\
      int srcrowdiff = self->width + sw;\
      int dstrowdiff = bufw - sw;\
      for (y = 0; y < sh; y++) {\
        for (x = 0; x < sw; x++) {\
          func(buf[dsti], self->data[srci], self->mask[srci])\
          srci--;\
          dsti++;\
        }\
        srci += srcrowdiff;\
        dsti += dstrowdiff;\
      }\
    }

  #define BLIT_NORMAL(dst, src, msk)\
    (dst) &= (msk);\
    (dst) |= (src);

  #define BLIT_AND(dst, src, msk)\
    (dst) &= (src);

  #define BLIT_OR(dst, src, msk)\
    (dst) |= (src);

  #define BLIT_COLOR(dst, src, msk)\
    (dst) &= (msk);\
    (dst) |= ~(msk) & image_color;

  #define BLIT(blit_loop)\
    switch (image_blendMode) {\
      default:\
      case IMAGE_NORMAL : blit_loop(BLIT_NORMAL)  break;\
      case IMAGE_AND    : blit_loop(BLIT_AND)     break;\
      case IMAGE_OR     : blit_loop(BLIT_OR)      break;\
      case IMAGE_COLOR  : blit_loop(BLIT_COLOR)   break;\
    }\

  if (!image_flip) {
    if (image_blendMode == IMAGE_FAST) {
      int y;
      int srci = sx + sy * self->width;
      int dsti = dx + dy * bufw;
      for (y = 0; y < sh; y++) {
        memcpy(buf + dsti, self->data + srci, sw);
        srci += self->width;
        dsti += bufw;
      }
    } else {
      BLIT(BLIT_LOOP_NORMAL);
    }
  } else {
    BLIT(BLIT_LOOP_FLIPPED);
  }

}


void image_drawTransformed(image_t *self, pixel_t *buf, int bufw, int bufh,
                           int sx, int sy, int sw, int sh,
                           double x, double y, double r,
                           double scalex, double scaley,
                           double ox, double oy
) {
  /* Draws the source rectangle of the image rotated by `r` radians and scaled
   * around its origin (`ox`, `oy`), which is placed at `x`, `y`. Each
   * destination pixel's centre is mapped back into the source image, so the
   * inner loop only does fixed point additions and lookups */
  double c, s, ia, ib, ic, id, minx, miny, maxx, maxy;
  int px0, py0, px1, py1, px, py, i;
  image_rect_t clip;

  /* Clip source rectangle to image */
  if (sx < 0) { sw += sx; ox += sx; sx = 0; }
  if (sy < 0) { sh += sy; oy += sy; sy = 0; }
  if (sx + sw > self->width) sw = self->width - sx;
  if (sy + sh > self->height) sh = self->height - sy;
  if (sw <= 0 || sh <= 0 || scalex == 0 || scaley == 0) return;

  /* Bounding box of the transformed rectangle */
  c = cos(r);
  s = sin(r);
  minx = miny = 1e30;
  maxx = maxy = -1e30;
  for (i = 0; i < 4; i++) {
    double u = ((i & 1) ? sw : 0) - ox;
    double v = ((i & 2) ? sh : 0) - oy;
    double tx = x + c * u * scalex - s * v * scaley;
    double ty = y + s * u * scalex + c * v * scaley;
    if (tx < minx) minx = tx;
    if (tx > maxx) maxx = tx;
    if (ty < miny) miny = ty;
    if (ty > maxy) maxy = ty;
  }
  clip = image_getClip(bufw, bufh);
  px0 = floor(minx); py0 = floor(miny);
  px1 = ceil(maxx);  py1 = ceil(maxy);
  if (px0 < clip.x0) px0 = clip.x0;
  if (py0 < clip.y0) py0 = clip.y0;
  if (px1 > clip.x1) px1 = clip.x1;
  if (py1 > clip.y1) py1 = clip.y1;
  if (px0 >= px1 || py0 >= py1) return;

  /* Inverse transform: source = M^-1 * (dest - pos) + origin */
  ia =  c / scalex;  ib = s / scalex;
  ic = -s / scaley;  id = c / scaley;

  #define FX(v) ((int) floor((v) * 65536.))
  {
    static int *rowOffset, *column;
    static int rowCap, columnCap;
    int du = FX(ia), dv = FX(ic);
    unsigned usw = sw, ush = sh;
    int width = px1 - px0;

    /* Offsets of the source rows, so that no multiplication is needed per
     * pixel (multiplications are slow on 386 and 486 CPUs) */
    if (sh > rowCap) {
      rowCap = sh;
      rowOffset = realloc(rowOffset, rowCap * sizeof(*rowOffset));
    }
    for (i = 0; i < sh; i++) {
      rowOffset[i] = sx + (sy + i) * self->width;
    }

    #define BLIT_FAST(dst, src, msk) (dst) = (src);

    if (r == 0) {
      /* Scaled but not rotated: every row of the destination maps to the same
       * source columns, so they are worked out once */
      if (width > columnCap) {
        columnCap = width;
        column = realloc(column, columnCap * sizeof(*column));
      }
      {
        int u = FX(ia * (px0 + 0.5 - x) + ox);
        for (i = 0; i < width; i++, u += du) {
          unsigned iu = u >> 16;
          column[i] = iu < usw ? (int) iu : -1;
        }
      }
      for (py = py0; py < py1; py++) {
        unsigned iv = FX(id * (py + 0.5 - y) + oy) >> 16;
        pixel_t *dst = buf + px0 + py * bufw;
        pixel_t *src, *msk;
        if (iv >= ush) continue;
        src = self->data + rowOffset[iv];
        msk = self->mask + rowOffset[iv];

        #define SCALE_LOOP(func)\
          for (i = 0; i < width; i++) {\
            int iu = column[i];\
            if (iu >= 0) {\
              func(dst[i], src[iu], msk[iu])\
            }\
          }

        switch (image_blendMode) {
          default:
          case IMAGE_NORMAL : SCALE_LOOP(BLIT_NORMAL) break;
          case IMAGE_FAST   : SCALE_LOOP(BLIT_FAST)   break;
          case IMAGE_AND    : SCALE_LOOP(BLIT_AND)    break;
          case IMAGE_OR     : SCALE_LOOP(BLIT_OR)     break;
          case IMAGE_COLOR  : SCALE_LOOP(BLIT_COLOR)  break;
        }
        #undef SCALE_LOOP
      }

    } else {
      /* Rotated: step through the source along the rotated row */
      for (py = py0; py < py1; py++) {
        double ry = py + 0.5 - y;
        double rx = px0 + 0.5 - x;
        int u = FX(ia * rx + ib * ry + ox);
        int v = FX(ic * rx + id * ry + oy);
        pixel_t *dst = buf + px0 + py * bufw;

        #define TRANSFORM_LOOP(func)\
          for (px = px0; px < px1; px++, dst++, u += du, v += dv) {\
            unsigned iu = u >> 16, iv = v >> 16;\
            if (iu < usw && iv < ush) {\
              int idx = rowOffset[iv] + iu;\
              func(*dst, self->data[idx], self->mask[idx])\
            }\
          }

        switch (image_blendMode) {
          default:
          case IMAGE_NORMAL : TRANSFORM_LOOP(BLIT_NORMAL) break;
          case IMAGE_FAST   : TRANSFORM_LOOP(BLIT_FAST)   break;
          case IMAGE_AND    : TRANSFORM_LOOP(BLIT_AND)    break;
          case IMAGE_OR     : TRANSFORM_LOOP(BLIT_OR)     break;
          case IMAGE_COLOR  : TRANSFORM_LOOP(BLIT_COLOR)  break;
        }
        #undef TRANSFORM_LOOP
      }
    }
    #undef BLIT_FAST
  }
  #undef FX
}


void image_deinit(image_t *self) {
  dmt_free(self->data);
  dmt_free(self->mask);
}
