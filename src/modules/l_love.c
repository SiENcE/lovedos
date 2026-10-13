/**
 * Copyright (c) 2017 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#include <stdio.h>
#include "luaobj.h"

#define LOVE_VERSION "0.2.1"


int l_love_getVersion(lua_State *L) {
  lua_pushstring(L, LOVE_VERSION);
  return 1;
}


int l_love_isVersionCompatible(lua_State *L) {
  /* isVersionCompatible("11.3") or isVersionCompatible(11, 3, 0): LoveDOS
   * implements parts of both the LÖVE 0.10 and 11 APIs */
  int major, minor;
  if (lua_type(L, 1) == LUA_TSTRING) {
    if (sscanf(lua_tostring(L, 1), "%d.%d", &major, &minor) != 2) {
      luaL_argerror(L, 1, "expected a version such as \"11.0\"");
    }
  } else {
    major = luaL_checkinteger(L, 1);
    minor = luaL_optinteger(L, 2, 0);
  }
  lua_pushboolean(L, (major == 0 && minor == 10) || major == 11);
  return 1;
}


static int deprecationOutput = 1;

int l_love_hasDeprecationOutput(lua_State *L) {
  lua_pushboolean(L, deprecationOutput);
  return 1;
}


int l_love_setDeprecationOutput(lua_State *L) {
  deprecationOutput = lua_toboolean(L, 1);
  return 0;
}


int luaopen_image(lua_State *L);
int luaopen_quad(lua_State *L);
int luaopen_font(lua_State *L);
int luaopen_source(lua_State *L);
int luaopen_sounddata(lua_State *L);
int luaopen_system(lua_State *L);
int luaopen_event(lua_State *L);
int luaopen_filesystem(lua_State *L);
int luaopen_graphics(lua_State *L);
int luaopen_audio(lua_State *L);
int luaopen_sound(lua_State *L);
int luaopen_timer(lua_State *L);
int luaopen_keyboard(lua_State *L);
int luaopen_mouse(lua_State *L);

int luaopen_love(lua_State *L) {
  int i;
  /* Init classes -- all the built in classes are inited here as to create the
   * metatables required by their constructors. Any new classes must also be
   * registered here before they will work. */
  int (*classes[])(lua_State *L) = {
    luaopen_image,
    luaopen_quad,
    luaopen_font,
    luaopen_source,
    luaopen_sounddata,
    NULL,
  };
  for (i = 0; classes[i]; i++) {
    classes[i](L);
    lua_pop(L, 1);
  }

  /* Init love module */
  luaL_Reg reg[] = {
    { "getVersion",           l_love_getVersion           },
    { "isVersionCompatible",  l_love_isVersionCompatible  },
    { "hasDeprecationOutput", l_love_hasDeprecationOutput },
    { "setDeprecationOutput", l_love_setDeprecationOutput },
    { 0, 0 },
  };
  luaL_newlib(L, reg);

  /* Init submodules */
  struct { char *name; int (*fn)(lua_State *L); } mods[] = {
    { "system",     luaopen_system      },
    { "event",      luaopen_event       },
    { "filesystem", luaopen_filesystem  },
    { "graphics",   luaopen_graphics    },
    { "audio",      luaopen_audio       },
    { "sound",      luaopen_sound       },
    { "timer",      luaopen_timer       },
    { "keyboard",   luaopen_keyboard    },
    { "mouse",      luaopen_mouse       },
    { 0 },
  };
  for (i = 0; mods[i].name; i++) {
    mods[i].fn(L);
    lua_setfield(L, -2, mods[i].name);
  }
  return 1;
}
