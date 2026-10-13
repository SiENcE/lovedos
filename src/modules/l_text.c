/**
 * Copyright (c) 2017 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#include <string.h>
#include <limits.h>
#include "lib/dmt/dmt.h"
#include "image.h"
#include "font.h"
#include "luaobj.h"


#define CLASS_TYPE  LUAOBJ_TYPE_TEXT
#define CLASS_NAME  "Text"


/* A Text is laid out and rendered once into an image whose mask holds the
 * glyphs; drawing it is then as fast as drawing an image of the same size,
 * in the current color */

typedef struct {
  char *str;
  int x, y;
  int limit;            /* Wrap limit, -1 for no wrapping */
  int align;
} segment_t;

typedef struct {
  font_t *font;
  int fontRef;
  segment_t *segs;
  int count, cap;
  int dirty;
  image_t image;
  int hasImage;
  int ox, oy;           /* Position of the image relative to the origin */
  int width, height;
} text_t;


extern image_t *graphics_canvas;
void graphics_drawImage(lua_State *L, image_t *img, int i,
                        int srcx, int srcy, int srcw, int srch,
                        double offx, double offy);

static const char *aligns[] = { "left", "center", "right", "justify", NULL };


static const char *checkText(lua_State *L, int idx) {
  /* Accepts a string or LÖVE 11's colored text table ({color, string, ...});
   * colors can't be applied per character in 256 colors and are ignored */
  if (lua_istable(L, idx)) {
    luaL_Buffer b;
    int i, n = lua_rawlen(L, idx);
    luaL_buffinit(L, &b);
    for (i = 1; i <= n; i++) {
      lua_rawgeti(L, idx, i);
      if (lua_type(L, -1) == LUA_TSTRING) {
        luaL_addvalue(&b);
      } else {
        lua_pop(L, 1);
      }
    }
    luaL_pushresult(&b);
    lua_replace(L, idx);
  }
  return luaL_checkstring(L, idx);
}


static void setFont(lua_State *L, text_t *self, int idx) {
  font_t *font = luaobj_checkudata(L, idx, LUAOBJ_TYPE_FONT);
  lua_pushvalue(L, idx);
  if (self->fontRef != LUA_NOREF) {
    luaL_unref(L, LUA_REGISTRYINDEX, self->fontRef);
  }
  self->fontRef = luaL_ref(L, LUA_REGISTRYINDEX);
  self->font = font;
  self->dirty = 1;
}


static void clearSegments(text_t *self) {
  int i;
  for (i = 0; i < self->count; i++) {
    dmt_free(self->segs[i].str);
  }
  self->count = 0;
  self->dirty = 1;
}


static int addSegment(text_t *self, const char *str, int x, int y,
                      int limit, int align) {
  segment_t *s;
  if (self->count == self->cap) {
    self->cap = self->cap ? self->cap * 2 : 4;
    self->segs = dmt_realloc(self->segs, self->cap * sizeof(*self->segs));
  }
  s = &self->segs[self->count++];
  s->str = dmt_malloc(strlen(str) + 1);
  strcpy(s->str, str);
  s->x = x;
  s->y = y;
  s->limit = limit;
  s->align = align;
  self->dirty = 1;
  return self->count;
}


typedef struct {
  text_t *text;
  image_rect_t bounds;
} measure_t;


static void measureSpan(void *udata, const char *str, int len, int x, int y) {
  measure_t *m = udata;
  font_t *font = m->text->font;
  int i, lines = 1, w = font_getWidth(font, str, len);
  for (i = 0; i < len; i++) lines += str[i] == '\n';
  font_getBounds(font, str, len, x, y, &m->bounds);
  if (x + w > m->text->width) m->text->width = x + w;
  if (y + (lines - 1) * font_getLineSpacing(font) + font->height >
      m->text->height) {
    m->text->height = y + (lines - 1) * font_getLineSpacing(font) +
                      font->height;
  }
}


static void renderSpan(void *udata, const char *str, int len, int x, int y) {
  text_t *self = udata;
  font_blit(self->font, self->image.data, self->image.width,
            self->image.height, str, len, x - self->ox, y - self->oy);
}


static void checkFont(lua_State *L, text_t *self) {
  /* The font must not have been released */
  lua_rawgeti(L, LUA_REGISTRYINDEX, self->fontRef);
  luaobj_checkudata(L, -1, LUAOBJ_TYPE_FONT);
  lua_pop(L, 1);
}


static void render(text_t *self) {
  measure_t m;
  int i, w, h;
  image_rect_t oldClip;
  unsigned oldColor;

  if (!self->dirty) return;
  self->dirty = 0;
  if (self->hasImage) {
    image_deinit(&self->image);
    self->hasImage = 0;
  }

  /* Measure */
  m.text = self;
  m.bounds.x0 = m.bounds.y0 = INT_MAX;
  m.bounds.x1 = m.bounds.y1 = INT_MIN;
  self->width = self->height = 0;
  for (i = 0; i < self->count; i++) {
    segment_t *s = &self->segs[i];
    font_layout(self->font, s->str, -1, s->x, s->y, s->limit, s->align,
                measureSpan, &m);
  }
  if (m.bounds.x0 >= m.bounds.x1 || m.bounds.y0 >= m.bounds.y1) {
    return;
  }

  /* Render with color index 1 (or the glyphs' own colors for image fonts),
   * ignoring the scissor, then build the mask from the drawn pixels */
  w = m.bounds.x1 - m.bounds.x0;
  h = m.bounds.y1 - m.bounds.y0;
  self->ox = m.bounds.x0;
  self->oy = m.bounds.y0;
  image_initBlank(&self->image, w, h);
  self->hasImage = 1;
  oldClip = image_clip;
  oldColor = image_color;
  image_setClip(INT_MIN, INT_MIN, INT_MAX, INT_MAX);
  image_setColor(1);
  for (i = 0; i < self->count; i++) {
    segment_t *s = &self->segs[i];
    font_layout(self->font, s->str, -1, s->x, s->y, s->limit, s->align,
                renderSpan, self);
  }
  image_clip = oldClip;
  image_color = oldColor;
  for (i = 0; i < w * h; i++) {
    self->image.mask[i] = self->image.data[i] ? 0 : 0xff;
  }
}


int l_text_new(lua_State *L) {
  /* newText(font [, text]) */
  /* Read the arguments before the new object is pushed onto the stack */
  const char *str = lua_isnoneornil(L, 2) ? NULL : checkText(L, 2);
  text_t *self;
  luaobj_checkudata(L, 1, LUAOBJ_TYPE_FONT);
  self = luaobj_newudata(L, sizeof(*self));
  luaobj_setclass(L, CLASS_TYPE, CLASS_NAME);
  memset(self, 0, sizeof(*self));
  self->fontRef = LUA_NOREF;
  setFont(L, self, 1);
  if (str) {
    addSegment(self, str, 0, 0, -1, FONT_ALIGN_LEFT);
  }
  return 1;
}


int l_text_gc(lua_State *L) {
  text_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  clearSegments(self);
  dmt_free(self->segs);
  if (self->hasImage) image_deinit(&self->image);
  luaL_unref(L, LUA_REGISTRYINDEX, self->fontRef);
  return 0;
}


int l_text_set(lua_State *L) {
  text_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  clearSegments(self);
  if (!lua_isnoneornil(L, 2)) {
    addSegment(self, checkText(L, 2), 0, 0, -1, FONT_ALIGN_LEFT);
  }
  return 0;
}


int l_text_setf(lua_State *L) {
  /* setf(text, wraplimit, align) */
  text_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  const char *str = checkText(L, 2);
  int limit = luaL_checknumber(L, 3);
  int align = luaL_checkoption(L, 4, "left", aligns);
  clearSegments(self);
  addSegment(self, str, 0, 0, limit < 0 ? 0 : limit, align);
  return 0;
}


int l_text_add(lua_State *L) {
  /* add(text, x, y): returns the index of the added text */
  text_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  const char *str = checkText(L, 2);
  int x = image_floor(luaL_optnumber(L, 3, 0));
  int y = image_floor(luaL_optnumber(L, 4, 0));
  lua_pushinteger(L, addSegment(self, str, x, y, -1, FONT_ALIGN_LEFT));
  return 1;
}


int l_text_addf(lua_State *L) {
  /* addf(text, wraplimit, align, x, y): returns the index of the added text */
  text_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  const char *str = checkText(L, 2);
  int limit = luaL_checknumber(L, 3);
  int align = luaL_checkoption(L, 4, NULL, aligns);
  int x = image_floor(luaL_optnumber(L, 5, 0));
  int y = image_floor(luaL_optnumber(L, 6, 0));
  lua_pushinteger(L, addSegment(self, str, x, y, limit < 0 ? 0 : limit,
                                align));
  return 1;
}


int l_text_clear(lua_State *L) {
  text_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  clearSegments(self);
  return 0;
}


int l_text_setFont(lua_State *L) {
  text_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  setFont(L, self, 2);
  return 0;
}


int l_text_getFont(lua_State *L) {
  text_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  lua_rawgeti(L, LUA_REGISTRYINDEX, self->fontRef);
  return 1;
}


int l_text_getWidth(lua_State *L) {
  text_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  checkFont(L, self);
  render(self);
  lua_pushinteger(L, self->width);
  return 1;
}


int l_text_getHeight(lua_State *L) {
  text_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  checkFont(L, self);
  render(self);
  lua_pushinteger(L, self->height);
  return 1;
}


int l_text_getDimensions(lua_State *L) {
  text_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  checkFont(L, self);
  render(self);
  lua_pushinteger(L, self->width);
  lua_pushinteger(L, self->height);
  return 2;
}


int l_text_draw(lua_State *L) {
  /* love.graphics.draw(text, x, y, r, sx, sy, ox, oy) */
  text_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  int oldBlendMode = image_blendMode;
  checkFont(L, self);
  render(self);
  if (!self->hasImage) return 0;
  /* Like print(), text is drawn in the current color */
  image_setBlendMode(self->font->colored ? IMAGE_NORMAL : IMAGE_COLOR);
  graphics_drawImage(L, &self->image, 2, 0, 0, self->image.width,
                     self->image.height, self->ox, self->oy);
  image_setBlendMode(oldBlendMode);
  return 0;
}


int luaopen_text(lua_State *L) {
  luaL_Reg reg[] = {
    { "new",            l_text_new            },
    { "__gc",           l_text_gc             },
    { "set",            l_text_set            },
    { "setf",           l_text_setf           },
    { "add",            l_text_add            },
    { "addf",           l_text_addf           },
    { "clear",          l_text_clear          },
    { "setFont",        l_text_setFont        },
    { "getFont",        l_text_getFont        },
    { "getWidth",       l_text_getWidth       },
    { "getHeight",      l_text_getHeight      },
    { "getDimensions",  l_text_getDimensions  },
    { 0, 0 },
  };
  luaobj_newclass(L, CLASS_NAME, NULL, l_text_new, reg);
  return 1;
}
