/**
 * Copyright (c) 2017 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#include <string.h>
#include "lib/stb/stb_image_write.h"
#include "luaobj.h"
#include "palette.h"
#include "color.h"
#include "image.h"


#define CLASS_TYPE  LUAOBJ_TYPE_IMAGE
#define CLASS_NAME  "Image"


int l_image_new(lua_State *L) {
  /* newImage(filename) or newImage(image), which copies the image */
  if (lua_type(L, 1) == LUA_TUSERDATA) {
    image_t *src = luaobj_checkudata(L, 1, CLASS_TYPE);
    image_t *self = luaobj_newudata(L, sizeof(*self));
    luaobj_setclass(L, CLASS_TYPE, CLASS_NAME);
    image_initBlank(self, src->width, src->height);
    memcpy(self->data, src->data, src->width * src->height);
    memcpy(self->mask, src->mask, src->width * src->height);
    return 1;
  }
  if (lua_istable(L, 1)) {
    /* FileData (a Lua object): load from its contents */
    size_t size;
    const char *data, *err;
    image_t *self;
    lua_getfield(L, 1, "getString");
    lua_pushvalue(L, 1);
    lua_call(L, 1, 1);
    data = luaL_checklstring(L, -1, &size);
    self = luaobj_newudata(L, sizeof(*self));
    luaobj_setclass(L, CLASS_TYPE, CLASS_NAME);
    err = image_initFromMemory(self, data, size);
    if (err) luaL_error(L, "%s", err);
    return 1;
  }
  const char *filename = luaL_checkstring(L, 1);
  image_t *self = luaobj_newudata(L, sizeof(*self));
  luaobj_setclass(L, CLASS_TYPE, CLASS_NAME);
  const char *err = image_init(self, filename);
  if (err) luaL_error(L, err);
  return 1;
}


int l_image_newCanvas(lua_State *L) {
  int width = VGA_WIDTH;
  int height = VGA_HEIGHT;
  if (lua_gettop(L) > 0) {
    width = luaL_checknumber(L, 1);
    height = luaL_checknumber(L, 2);
    if (width <= 0) luaL_argerror(L, 1, "width must be larger than 0");
    if (height <= 0) luaL_argerror(L, 2, "height must be larger than 0");
  }
  image_t *self = luaobj_newudata(L, sizeof(*self));
  luaobj_setclass(L, CLASS_TYPE, CLASS_NAME);
  image_initBlank(self, width, height);
  return 1;
}


int l_image_gc(lua_State *L) {
  image_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  image_deinit(self);
  return 0;
}


int l_image_getDimensions(lua_State *L) {
  image_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  lua_pushinteger(L, self->width);
  lua_pushinteger(L, self->height);
  return 2;
}


int l_image_getWidth(lua_State *L) {
  image_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  lua_pushinteger(L, self->width);
  return 1;
}


int l_image_getHeight(lua_State *L) {
  image_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  lua_pushinteger(L, self->height);
  return 1;
}


int l_image_getPixel(lua_State *L) {
  image_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  int x = luaL_checknumber(L, 2);
  int y = luaL_checknumber(L, 3);
  if (x < 0 || x >= self->width || y < 0 || y >= self->height) {
    /* Return `nil` for out of bounds (same as transparent) */
    lua_pushnil(L);
    return 1;
  } else {
    /* Return `nil` if color is transparent, else return 3 channel values */
    int idx = self->data[x + y * self->width];
    if (idx == 0) {
      lua_pushnil(L);
      return 1;
    } else {
      int rgb[3];
      palette_idxToColor(idx, rgb);
      return color_push(L, rgb);
    }
  }
}


int l_image_setPixel(lua_State *L) {
  image_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  int x = luaL_checknumber(L, 2);
  int y = luaL_checknumber(L, 3);
  if (lua_isnoneornil(L, 4)) {
    /* Set transparent */
    image_setPixel(self, x, y, 0);
    image_setMaskPixel(self, x, y, 0xff);
  } else {
    /* Get color, set pixel and mask */
    int idx = color_check(L, 4, NULL, NULL);
    image_setPixel(self, x, y, idx);
    image_setMaskPixel(self, x, y, 0x0);
  }
  return 0;
}


int l_image_paste(lua_State *L) {
  /* paste(source, dx, dy [, sx, sy, sw, sh]): copies pixels (including
   * transparent ones) from the source image */
  image_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  image_t *src = luaobj_checkudata(L, 2, CLASS_TYPE);
  int dx = luaL_checknumber(L, 3), dy = luaL_checknumber(L, 4);
  int sx = luaL_optnumber(L, 5, 0), sy = luaL_optnumber(L, 6, 0);
  int sw = luaL_optnumber(L, 7, src->width), sh = luaL_optnumber(L, 8, src->height);
  int y;
  /* Clip to the source and destination */
  if (sx < 0) { sw += sx; dx -= sx; sx = 0; }
  if (sy < 0) { sh += sy; dy -= sy; sy = 0; }
  if (dx < 0) { sw += dx; sx -= dx; dx = 0; }
  if (dy < 0) { sh += dy; sy -= dy; dy = 0; }
  if (sx + sw > src->width) sw = src->width - sx;
  if (sy + sh > src->height) sh = src->height - sy;
  if (dx + sw > self->width) sw = self->width - dx;
  if (dy + sh > self->height) sh = self->height - dy;
  for (y = 0; y < sh && sw > 0; y++) {
    int si = sx + (sy + y) * src->width, di = dx + (dy + y) * self->width;
    memmove(self->data + di, src->data + si, sw);
    memmove(self->mask + di, src->mask + si, sw);
  }
  return 0;
}


static void writeToBuffer(void *context, void *data, int size) {
  luaL_addlstring(context, data, size);
}


int l_image_encode(lua_State *L) {
  /* Internal: _encode(format) returns the image encoded as "png", "tga" or
   * "bmp"; used by ImageData:encode() */
  static const char *formats[] = { "png", "tga", "bmp", NULL };
  image_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  int format = luaL_checkoption(L, 2, NULL, formats);
  int i, n = self->width * self->height, ok;
  unsigned char *rgba = malloc(n * 4);
  luaL_Buffer b;
  if (!rgba) luaL_error(L, "out of memory");
  for (i = 0; i < n; i++) {
    int rgb[3];
    palette_idxToColor(self->data[i], rgb);
    rgba[i * 4] = rgb[0];
    rgba[i * 4 + 1] = rgb[1];
    rgba[i * 4 + 2] = rgb[2];
    /* Like getPixel(), color index 0 is transparent */
    rgba[i * 4 + 3] = self->data[i] ? 255 : 0;
  }
  luaL_buffinit(L, &b);
  switch (format) {
    case 0:
      ok = stbi_write_png_to_func(writeToBuffer, &b, self->width, self->height,
                                  4, rgba, self->width * 4);
      break;
    case 1:
      ok = stbi_write_tga_to_func(writeToBuffer, &b, self->width, self->height,
                                  4, rgba);
      break;
    default:
      ok = stbi_write_bmp_to_func(writeToBuffer, &b, self->width, self->height,
                                  4, rgba);
      break;
  }
  free(rgba);
  if (!ok) luaL_error(L, "could not encode image");
  luaL_pushresult(&b);
  return 1;
}


int luaopen_image(lua_State *L) {
  luaL_Reg reg[] = {
    { "new",            l_image_new           },
    { "__gc",           l_image_gc            },
    { "getDimensions",  l_image_getDimensions },
    { "getWidth",       l_image_getWidth      },
    { "getHeight",      l_image_getHeight     },
    { "getPixel",       l_image_getPixel      },
    { "setPixel",       l_image_setPixel      },
    { "paste",          l_image_paste         },
    { "_encode",        l_image_encode        },
    { 0, 0 },
  };
  luaobj_newclass(L, CLASS_NAME, NULL, l_image_new, reg);
  return 1;
}
