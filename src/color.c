/**
 * Copyright (c) 2017 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#include "palette.h"
#include "color.h"


static double color_max = 255;


void color_setRange(double max) {
  color_max = max;
}


double color_getRange(void) {
  return color_max;
}


static int toByte(double v) {
  int n = v * 255. / color_max + 0.5;
  return n < 0 ? 0 : n > 255 ? 255 : n;
}


int color_check(lua_State *L, int idx, const int *def, int *rgb) {
  /* Reads a color given as `r, g, b [, a]` or `{r, g, b [, a]}` starting at
   * argument `idx`, or uses `def` (8 bit values) if there is no argument and
   * `def` is not NULL. The alpha component is accepted but ignored. The
   * 8 bit values are written to `rgb` and the palette index is returned */
  int c[3], i, res;
  if (lua_isnoneornil(L, idx) && def) {
    for (i = 0; i < 3; i++) c[i] = def[i];
  } else if (lua_istable(L, idx)) {
    for (i = 0; i < 3; i++) {
      lua_rawgeti(L, idx, i + 1);
      if (!lua_isnumber(L, -1)) {
        luaL_argerror(L, idx, "expected a table of three or four numbers");
      }
      c[i] = toByte(lua_tonumber(L, -1));
      lua_pop(L, 1);
    }
  } else {
    for (i = 0; i < 3; i++) c[i] = toByte(luaL_checknumber(L, idx + i));
  }
  res = palette_colorToIdx(c[0], c[1], c[2]);
  if (res < 0) {
    luaL_error(L, "color palette exhausted: use fewer unique colors");
  }
  if (rgb) {
    for (i = 0; i < 3; i++) rgb[i] = c[i];
  }
  return res;
}


int color_push(lua_State *L, const int *rgb) {
  /* Pushes the color as r, g, b, a in the current range */
  int i;
  for (i = 0; i < 3; i++) {
    if (color_max == 255) {
      lua_pushinteger(L, rgb[i]);
    } else {
      lua_pushnumber(L, rgb[i] / 255. * color_max);
    }
  }
  if (color_max == 255) {
    lua_pushinteger(L, 255);
  } else {
    lua_pushnumber(L, color_max);
  }
  return 4;
}
