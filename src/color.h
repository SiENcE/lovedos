/**
 * Copyright (c) 2017 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#ifndef COLOR_H
#define COLOR_H

#include "luaobj.h"

/* Colors are given to and returned from Lua either in the range 0 to 255
 * (LÖVE 0.10 and earlier, the default) or 0 to 1 (LÖVE 11), see
 * color_setRange() */

void color_setRange(double max);
double color_getRange(void);
int color_check(lua_State *L, int idx, const int *def, int *rgb);
int color_push(lua_State *L, const int *rgb);

#endif
