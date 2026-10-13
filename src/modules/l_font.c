/**
 * Copyright (c) 2017 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#include <string.h>
#include "font.h"
#include "luaobj.h"

#define CLASS_TYPE  LUAOBJ_TYPE_FONT
#define CLASS_NAME  "Font"


int l_font_new(lua_State *L) {
  const char *filename;
  int ptsize = 8;
  if ( lua_isnoneornil(L, 2) ) {
    filename = NULL;
    ptsize = luaL_optnumber(L, 1, ptsize);
  } else {
    filename = luaL_checkstring(L, 1);
    ptsize = luaL_optnumber(L, 2, ptsize);
  }
  font_t *self = luaobj_newudata(L, sizeof(*self));
  luaobj_setclass(L, CLASS_TYPE, CLASS_NAME);
  if (filename) {
    const char *err = font_init(self, filename, ptsize);
    if (err) luaL_error(L, err);
  } else {
    font_initEmbedded(self, ptsize);
  }
  return 1;
}


int l_font_newImageFont(lua_State *L) {
  /* newImageFont(filename or image, glyphs [, extraspacing]) */
  const char *glyphs = luaL_checkstring(L, 2);
  int spacing = luaL_optnumber(L, 3, 0);
  image_t tmp, *img = NULL;
  const char *err;
  font_t *self;
  if (lua_type(L, 1) == LUA_TSTRING) {
    err = image_init(&tmp, lua_tostring(L, 1));
    if (err) luaL_error(L, "%s", err);
  } else {
    img = luaobj_checkudata(L, 1, LUAOBJ_TYPE_IMAGE);
  }
  self = luaobj_newudata(L, sizeof(*self));
  luaobj_setclass(L, CLASS_TYPE, CLASS_NAME);
  err = font_initImage(self, img ? img : &tmp, glyphs, spacing);
  if (!img) image_deinit(&tmp);
  if (err) {
    memset(self, 0, sizeof(*self));
    luaL_error(L, "%s", err);
  }
  return 1;
}


int l_font_gc(lua_State *L) {
  font_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  font_deinit(self);
  return 0;
}


int l_font_getWidth(lua_State *L) {
  font_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  size_t len;
  const char *str = luaL_checklstring(L, 2, &len);
  lua_pushinteger(L, font_getWidth(self, str, len));
  return 1;
}


static void pushWrapLine(void *udata, const char *str, int len, int width,
                         int last) {
  lua_State *L = udata;
  lua_pushlstring(L, str, len);
  lua_rawseti(L, -2, lua_rawlen(L, -2) + 1);
}


int l_font_getAscent(lua_State *L) {
  font_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  lua_pushinteger(L, self->ascent);
  return 1;
}


int l_font_getDescent(lua_State *L) {
  font_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  lua_pushinteger(L, self->descent);
  return 1;
}


int l_font_getLineHeight(lua_State *L) {
  font_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  lua_pushnumber(L, self->lineHeight);
  return 1;
}


int l_font_setLineHeight(lua_State *L) {
  font_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  self->lineHeight = luaL_checknumber(L, 2);
  return 0;
}


int l_font_hasGlyphs(lua_State *L) {
  /* hasGlyphs(text, ...) or hasGlyphs(codepoint, ...) */
  font_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  int i, n = lua_gettop(L);
  for (i = 2; i <= n; i++) {
    if (lua_type(L, i) == LUA_TNUMBER) {
      int c = lua_tointeger(L, i);
      if (c < 0 || c > 127 || (self->colored && !self->advance[c])) {
        lua_pushboolean(L, 0);
        return 1;
      }
    } else {
      const unsigned char *p = (const unsigned char*) luaL_checkstring(L, i);
      for (; *p; p++) {
        if (*p > 127 || (self->colored && !self->advance[*p])) {
          lua_pushboolean(L, 0);
          return 1;
        }
      }
    }
  }
  lua_pushboolean(L, 1);
  return 1;
}


int l_font_getWrap(lua_State *L) {
  font_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  const char *str = luaL_checkstring(L, 2);
  int limit = luaL_checknumber(L, 3);
  int width;
  lua_newtable(L);
  width = font_wrap(self, str, limit, pushWrapLine, L);
  lua_pushinteger(L, width);
  lua_insert(L, -2);
  return 2;
}


int l_font_getHeight(lua_State *L) {
  font_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  lua_pushinteger(L, self->height);
  return 1;
}



int luaopen_font(lua_State *L) {
  luaL_Reg reg[] = {
    { "new",            l_font_new            },
    { "__gc",           l_font_gc             },
    { "getWidth",       l_font_getWidth       },
    { "getHeight",      l_font_getHeight      },
    { "getWrap",        l_font_getWrap        },
    { "getAscent",      l_font_getAscent      },
    { "getDescent",     l_font_getDescent     },
    { "getBaseline",    l_font_getAscent      },
    { "getLineHeight",  l_font_getLineHeight  },
    { "setLineHeight",  l_font_setLineHeight  },
    { "hasGlyphs",      l_font_hasGlyphs      },
    { 0, 0 },
  };
  luaobj_newclass(L, CLASS_NAME, NULL, l_font_new, reg);
  return 1;
}
