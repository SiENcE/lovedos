/**
 * Copyright (c) 2017 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#include "luaobj.h"


int l_sounddata_new(lua_State *L);
int l_decoder_new(lua_State *L);

int luaopen_sound(lua_State *L) {
  luaL_Reg reg[] = {
    { "newSoundData", l_sounddata_new },
    { "newDecoder",   l_decoder_new   },
    { 0, 0 },
  };
  luaL_newlib(L, reg);
  return 1;
}
