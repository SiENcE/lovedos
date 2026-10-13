/**
 * Copyright (c) 2017 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

 #include "mouse.h"
 #include "luaobj.h"


int l_mouse_getPosition(lua_State *L) {
  lua_pushinteger(L, mouse_getX());
  lua_pushinteger(L, mouse_getY());
  return 2;
}


int l_mouse_getX(lua_State *L) {
  lua_pushinteger(L, mouse_getX());
  return 1;
}


int l_mouse_getY(lua_State *L) {
  lua_pushinteger(L, mouse_getY());
  return 1;
}


int l_mouse_isDown(lua_State *L) {
  int n = lua_gettop(L);
  int res = 0;
  int i;
  for (i = 1; i <= n; i++) {
    int idx = luaL_checknumber(L, i) - 1;
    if (idx >= 0 && idx < MOUSE_BUTTON_MAX) {
      res |= mouse_isDown(idx);
    }
  }
  lua_pushboolean(L, res);
  return 1;
}



int l_mouse_setPosition(lua_State *L) {
  mouse_setPosition(luaL_checknumber(L, 1), luaL_checknumber(L, 2));
  return 0;
}


int l_mouse_setX(lua_State *L) {
  mouse_setPosition(luaL_checknumber(L, 1), mouse_getY());
  return 0;
}


int l_mouse_setY(lua_State *L) {
  mouse_setPosition(mouse_getX(), luaL_checknumber(L, 1));
  return 0;
}


int l_mouse_setRelativeMode(lua_State *L) {
  mouse_setRelativeMode(lua_toboolean(L, 1));
  return 0;
}


int l_mouse_getRelativeMode(lua_State *L) {
  lua_pushboolean(L, mouse_relative);
  return 1;
}


int l_mouse_isCursorSupported(lua_State *L) {
  lua_pushboolean(L, mouse_inited);
  return 1;
}


int luaopen_mouse(lua_State *L) {
  luaL_Reg reg[] = {
    { "getPosition",  l_mouse_getPosition   },
    { "getX",         l_mouse_getX          },
    { "getY",         l_mouse_getY          },
    { "isDown",       l_mouse_isDown        },
    { "setPosition",  l_mouse_setPosition   },
    { "setX",         l_mouse_setX          },
    { "setY",         l_mouse_setY          },
    { "setRelativeMode",  l_mouse_setRelativeMode   },
    { "getRelativeMode",  l_mouse_getRelativeMode   },
    { "isCursorSupported", l_mouse_isCursorSupported },
    { 0, 0 },
  };
  luaL_newlib(L, reg);
  return 1;
}
