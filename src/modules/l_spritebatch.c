/**
 * Copyright (c) 2017 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#include <string.h>
#include "lib/dmt/dmt.h"
#include "image.h"
#include "quad.h"
#include "luaobj.h"


#define CLASS_TYPE  LUAOBJ_TYPE_SPRITEBATCH
#define CLASS_NAME  "SpriteBatch"


/* A sprite is drawn with the fast blit unless it is rotated or scaled */
enum { SPRITE_BLIT, SPRITE_BLIT_FLIPPED, SPRITE_TRANSFORMED };

typedef struct {
  int type;
  int srcx, srcy, srcw, srch;
  double x, y, r, sx, sy, ox, oy;
} sprite_t;

typedef struct {
  image_t *image;
  int imageRef;       /* Registry reference keeping the image alive */
  sprite_t *sprites;
  int count, cap;
  int drawStart, drawCount;
} spritebatch_t;


extern image_t *graphics_canvas;
extern int love_isLove11;
void graphics_getTranslation(double *x, double *y);


static void setImage(lua_State *L, spritebatch_t *self, int idx) {
  image_t *img = luaobj_checkudata(L, idx, LUAOBJ_TYPE_IMAGE);
  lua_pushvalue(L, idx);
  if (self->imageRef != LUA_NOREF) {
    luaL_unref(L, LUA_REGISTRYINDEX, self->imageRef);
  }
  self->imageRef = luaL_ref(L, LUA_REGISTRYINDEX);
  self->image = img;
}


static void reserve(spritebatch_t *self, int n) {
  if (n > self->cap) {
    self->cap = n;
    self->sprites = dmt_realloc(self->sprites, n * sizeof(*self->sprites));
  }
}


int l_spritebatch_new(lua_State *L) {
  /* newSpriteBatch(image [, maxsprites [, usage]]) */
  int max = luaL_optinteger(L, 2, 1000);
  spritebatch_t *self = luaobj_newudata(L, sizeof(*self));
  luaobj_setclass(L, CLASS_TYPE, CLASS_NAME);
  memset(self, 0, sizeof(*self));
  self->imageRef = LUA_NOREF;
  setImage(L, self, 1);
  reserve(self, max > 0 ? max : 1);
  return 1;
}


int l_spritebatch_gc(lua_State *L) {
  spritebatch_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  luaL_unref(L, LUA_REGISTRYINDEX, self->imageRef);
  dmt_free(self->sprites);
  return 0;
}


static void readSprite(lua_State *L, spritebatch_t *self, int i, sprite_t *s) {
  /* Reads [quad,] x, y, r, sx, sy, ox, oy starting at argument `i` */
  if (!lua_isnone(L, i) && lua_type(L, i) != LUA_TNUMBER) {
    quad_t *q = luaobj_checkudata(L, i, LUAOBJ_TYPE_QUAD);
    s->srcx = q->x;
    s->srcy = q->y;
    s->srcw = q->width;
    s->srch = q->height;
    i++;
  } else {
    s->srcx = s->srcy = 0;
    s->srcw = self->image->width;
    s->srch = self->image->height;
  }
  s->x  = luaL_optnumber(L, i, 0);
  s->y  = luaL_optnumber(L, i + 1, 0);
  s->r  = luaL_optnumber(L, i + 2, 0);
  s->sx = luaL_optnumber(L, i + 3, 1);
  s->sy = luaL_optnumber(L, i + 4, s->sx);
  s->ox = luaL_optnumber(L, i + 5, 0);
  s->oy = luaL_optnumber(L, i + 6, 0);
  if (s->r == 0 && s->sy == 1 && (s->sx == 1 || s->sx == -1)) {
    s->type = s->sx < 0 ? SPRITE_BLIT_FLIPPED : SPRITE_BLIT;
  } else {
    s->type = SPRITE_TRANSFORMED;
  }
}


int l_spritebatch_add(lua_State *L) {
  /* add([quad,] x, y, r, sx, sy, ox, oy): returns the sprite's id */
  spritebatch_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  sprite_t s;
  readSprite(L, self, 2, &s);
  if (self->count == self->cap) {
    reserve(self, self->cap * 2);
  }
  self->sprites[self->count++] = s;
  /* Ids start at 1 in LÖVE 11 and at 0 in LÖVE 0.10 */
  lua_pushinteger(L, self->count - !love_isLove11);
  return 1;
}


int l_spritebatch_set(lua_State *L) {
  /* set(id, [quad,] x, y, r, sx, sy, ox, oy) */
  spritebatch_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  int id = luaL_checkinteger(L, 2) + !love_isLove11;
  if (id < 1 || id > self->count) {
    luaL_argerror(L, 2, "invalid sprite id");
  }
  readSprite(L, self, 3, &self->sprites[id - 1]);
  return 0;
}


int l_spritebatch_clear(lua_State *L) {
  spritebatch_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  self->count = 0;
  return 0;
}


int l_spritebatch_getCount(lua_State *L) {
  spritebatch_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  lua_pushinteger(L, self->count);
  return 1;
}


int l_spritebatch_getBufferSize(lua_State *L) {
  spritebatch_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  lua_pushinteger(L, self->cap);
  return 1;
}


int l_spritebatch_setBufferSize(lua_State *L) {
  spritebatch_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  int n = luaL_checkinteger(L, 2);
  if (n < 1) luaL_argerror(L, 2, "buffer size must be at least 1");
  if (n < self->count) self->count = n;
  self->cap = 0;
  reserve(self, n);
  return 0;
}


int l_spritebatch_setTexture(lua_State *L) {
  spritebatch_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  setImage(L, self, 2);
  return 0;
}


int l_spritebatch_getTexture(lua_State *L) {
  spritebatch_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  lua_rawgeti(L, LUA_REGISTRYINDEX, self->imageRef);
  return 1;
}


int l_spritebatch_setDrawRange(lua_State *L) {
  /* setDrawRange(start, count), or setDrawRange() to draw all sprites */
  spritebatch_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  if (lua_isnoneornil(L, 2)) {
    self->drawStart = self->drawCount = 0;
    return 0;
  }
  self->drawStart = luaL_checkinteger(L, 2);
  self->drawCount = luaL_checkinteger(L, 3);
  if (self->drawStart < 1 || self->drawCount < 1) {
    luaL_error(L, "invalid draw range");
  }
  return 0;
}


int l_spritebatch_getDrawRange(lua_State *L) {
  spritebatch_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  if (self->drawCount < 1) {
    lua_pushinteger(L, 1);
    lua_pushinteger(L, self->count);
  } else {
    lua_pushinteger(L, self->drawStart);
    lua_pushinteger(L, self->drawCount);
  }
  return 2;
}


int l_spritebatch_noop(lua_State *L) {
  /* flush(), setColor(): sprites are drawn directly and can't be tinted in
   * 256 colors, so these do nothing */
  luaobj_checkudata(L, 1, CLASS_TYPE);
  return 0;
}


int l_spritebatch_draw(lua_State *L) {
  /* love.graphics.draw(spritebatch, x, y): sprite batches can only be
   * translated */
  spritebatch_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  image_t *img = self->image;
  pixel_t *buf = graphics_canvas->data;
  int bufw = graphics_canvas->width;
  int bufh = graphics_canvas->height;
  image_rect_t clip = image_getClip(bufw, bufh);
  double tx, ty, bx, by;
  int i, start = 0, end = self->count;

  if ((!lua_isnoneornil(L, 4) && lua_tonumber(L, 4) != 0) ||
      (!lua_isnoneornil(L, 5) && lua_tonumber(L, 5) != 1) ||
      (!lua_isnoneornil(L, 6) && lua_tonumber(L, 6) != 1)
  ) {
    luaL_error(L, "sprite batches can only be drawn translated, not rotated "
                  "or scaled");
  }
  graphics_getTranslation(&tx, &ty);
  bx = luaL_optnumber(L, 2, 0) + tx - luaL_optnumber(L, 7, 0);
  by = luaL_optnumber(L, 3, 0) + ty - luaL_optnumber(L, 8, 0);
  if (self->drawCount > 0) {
    start = self->drawStart - 1;
    end = start + self->drawCount;
    if (end > self->count) end = self->count;
  }

  for (i = start; i < end; i++) {
    sprite_t *s = &self->sprites[i];
    switch (s->type) {
      case SPRITE_BLIT:
        image_setFlip(0);
        image_blitClipped(img, buf, bufw, &clip,
                          image_floor(bx + s->x - s->ox),
                          image_floor(by + s->y - s->oy),
                          s->srcx, s->srcy, s->srcw, s->srch);
        break;
      case SPRITE_BLIT_FLIPPED:
        image_setFlip(1);
        image_blitClipped(img, buf, bufw, &clip,
                          image_floor(bx + s->x + s->ox - s->srcw),
                          image_floor(by + s->y - s->oy),
                          s->srcx, s->srcy, s->srcw, s->srch);
        break;
      default:
        image_drawTransformed(img, buf, bufw, bufh,
                              s->srcx, s->srcy, s->srcw, s->srch,
                              bx + s->x, by + s->y, s->r, s->sx, s->sy,
                              s->ox, s->oy);
        break;
    }
  }
  image_setFlip(0);
  return 0;
}


int luaopen_spritebatch(lua_State *L) {
  luaL_Reg reg[] = {
    { "new",            l_spritebatch_new           },
    { "__gc",           l_spritebatch_gc            },
    { "add",            l_spritebatch_add           },
    { "set",            l_spritebatch_set           },
    { "clear",          l_spritebatch_clear         },
    { "getCount",       l_spritebatch_getCount      },
    { "getBufferSize",  l_spritebatch_getBufferSize },
    { "setBufferSize",  l_spritebatch_setBufferSize },
    { "setTexture",     l_spritebatch_setTexture    },
    { "getTexture",     l_spritebatch_getTexture    },
    { "setImage",       l_spritebatch_setTexture    },
    { "getImage",       l_spritebatch_getTexture    },
    { "setDrawRange",   l_spritebatch_setDrawRange  },
    { "getDrawRange",   l_spritebatch_getDrawRange  },
    { "flush",          l_spritebatch_noop          },
    { "setColor",       l_spritebatch_noop          },
    { 0, 0 },
  };
  luaobj_newclass(L, CLASS_NAME, NULL, l_spritebatch_new, reg);
  return 1;
}
