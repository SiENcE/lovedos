/**
 * Copyright (c) 2017 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#include <string.h>
#include "lib/cmixer/cmixer.h"
#include "spatial.h"
#include "luaobj.h"


static double masterVolume = 1;


int l_source_new(lua_State *L);
int l_source_play(lua_State *L);
int l_source_pause(lua_State *L);
int l_source_resume(lua_State *L);
int l_source_stop(lua_State *L);
int l_source_rewind(lua_State *L);
int l_source_isPlaying(lua_State *L);
void l_source_pushActive(lua_State *L);
void l_source_updateAll(void);

static const char *distanceModels[] = {
  "none", "inverse", "inverseclamped", "linear", "linearclamped", "exponent",
  "exponentclamped", NULL
};


static int callSourceFn(lua_State *L, lua_CFunction fn, int idx) {
  /* Calls `fn` with the value at `idx` as its argument and returns the
   * truthiness of its (first) result */
  int res;
  idx = lua_absindex(L, idx);
  lua_pushcfunction(L, fn);
  lua_pushvalue(L, idx);
  lua_call(L, 1, 1);
  res = lua_isnil(L, -1) || lua_toboolean(L, -1);
  lua_pop(L, 1);
  return res;
}


static int applyToSources(lua_State *L, lua_CFunction fn) {
  /* Calls `fn` for each argument, which can be either a Source or an array of
   * Sources. With no arguments `fn` is called for all the active sources.
   * Returns true if every call succeeded */
  int i, j, n = lua_gettop(L);
  int ok = 1;
  if (n == 0) {
    l_source_pushActive(L);
    n = 1;
  }
  for (i = 1; i <= n; i++) {
    if (lua_istable(L, i)) {
      for (j = 1; lua_rawgeti(L, i, j), !lua_isnil(L, -1); j++) {
        ok = callSourceFn(L, fn, -1) && ok;
        lua_pop(L, 1);
      }
      lua_pop(L, 1);
    } else {
      ok = callSourceFn(L, fn, i) && ok;
    }
  }
  return ok;
}


int l_audio_play(lua_State *L) {
  luaL_checkany(L, 1);
  lua_pushboolean(L, applyToSources(L, l_source_play));
  return 1;
}


int l_audio_pause(lua_State *L) {
  /* With no arguments all playing sources are paused and returned */
  int i, n = 0;
  if (lua_gettop(L) > 0) {
    applyToSources(L, l_source_pause);
    return 0;
  }
  l_source_pushActive(L);
  lua_newtable(L);
  for (i = 1; lua_rawgeti(L, -2, i), !lua_isnil(L, -1); i++) {
    if (callSourceFn(L, l_source_isPlaying, -1)) {
      callSourceFn(L, l_source_pause, -1);
      lua_rawseti(L, -2, ++n);
    } else {
      lua_pop(L, 1);
    }
  }
  lua_pop(L, 1);
  return 1;
}


int l_audio_resume(lua_State *L) {
  applyToSources(L, l_source_resume);
  return 0;
}


int l_audio_stop(lua_State *L) {
  applyToSources(L, l_source_stop);
  return 0;
}


int l_audio_rewind(lua_State *L) {
  applyToSources(L, l_source_rewind);
  return 0;
}


int l_audio_getActiveSourceCount(lua_State *L) {
  l_source_pushActive(L);
  lua_pushinteger(L, lua_rawlen(L, -1));
  return 1;
}


int l_audio_getActiveSources(lua_State *L) {
  l_source_pushActive(L);
  return 1;
}


int l_audio_setVolume(lua_State *L) {
  double n = luaL_checknumber(L, 1);
  masterVolume = n < 0 ? 0 : n;
  cm_set_master_gain(masterVolume);
  return 0;
}


int l_audio_getVolume(lua_State *L) {
  lua_pushnumber(L, masterVolume);
  return 1;
}


static void checkVec3(lua_State *L, int idx, spatial_vec3_t *v) {
  v->x = luaL_checknumber(L, idx);
  v->y = luaL_checknumber(L, idx + 1);
  v->z = luaL_optnumber(L, idx + 2, 0);
}


static int pushVec3(lua_State *L, spatial_vec3_t *v) {
  lua_pushnumber(L, v->x);
  lua_pushnumber(L, v->y);
  lua_pushnumber(L, v->z);
  return 3;
}


int l_audio_setPosition(lua_State *L) {
  checkVec3(L, 1, &spatial_listener.position);
  l_source_updateAll();
  return 0;
}


int l_audio_getPosition(lua_State *L) {
  return pushVec3(L, &spatial_listener.position);
}


int l_audio_setVelocity(lua_State *L) {
  checkVec3(L, 1, &spatial_listener.velocity);
  l_source_updateAll();
  return 0;
}


int l_audio_getVelocity(lua_State *L) {
  return pushVec3(L, &spatial_listener.velocity);
}


int l_audio_setOrientation(lua_State *L) {
  int i;
  for (i = 1; i <= 6; i++) luaL_checknumber(L, i);
  spatial_listener.forward.x = lua_tonumber(L, 1);
  spatial_listener.forward.y = lua_tonumber(L, 2);
  spatial_listener.forward.z = lua_tonumber(L, 3);
  spatial_listener.up.x = lua_tonumber(L, 4);
  spatial_listener.up.y = lua_tonumber(L, 5);
  spatial_listener.up.z = lua_tonumber(L, 6);
  l_source_updateAll();
  return 0;
}


int l_audio_getOrientation(lua_State *L) {
  pushVec3(L, &spatial_listener.forward);
  return pushVec3(L, &spatial_listener.up) + 3;
}


int l_audio_setDistanceModel(lua_State *L) {
  /* Also accept LÖVE 11 names which use spaces ("inverse clamped") */
  char buf[32];
  const char *name = luaL_checkstring(L, 1);
  int i, j;
  for (i = j = 0; name[i] && j < (int) sizeof(buf) - 1; i++) {
    if (name[i] != ' ') buf[j++] = name[i];
  }
  buf[j] = '\0';
  for (i = 0; distanceModels[i]; i++) {
    if (!strcmp(buf, distanceModels[i])) {
      spatial_listener.distanceModel = i;
      l_source_updateAll();
      return 0;
    }
  }
  return luaL_argerror(L, 1, "invalid distance model");
}


int l_audio_getDistanceModel(lua_State *L) {
  lua_pushstring(L, distanceModels[spatial_listener.distanceModel]);
  return 1;
}


int l_audio_setDopplerScale(lua_State *L) {
  double n = luaL_checknumber(L, 1);
  if (n < 0) {
    luaL_argerror(L, 1, "doppler scale must not be negative");
  }
  spatial_listener.dopplerScale = n;
  l_source_updateAll();
  return 0;
}


int l_audio_getDopplerScale(lua_State *L) {
  lua_pushnumber(L, spatial_listener.dopplerScale);
  return 1;
}


int luaopen_audio(lua_State *L) {
  luaL_Reg reg[] = {
    { "newSource",            l_source_new                  },
    { "play",                 l_audio_play                  },
    { "pause",                l_audio_pause                 },
    { "resume",               l_audio_resume                },
    { "stop",                 l_audio_stop                  },
    { "rewind",               l_audio_rewind                },
    { "getActiveSourceCount", l_audio_getActiveSourceCount  },
    { "getSourceCount",       l_audio_getActiveSourceCount  },
    { "getActiveSources",     l_audio_getActiveSources      },
    { "setVolume",            l_audio_setVolume             },
    { "getVolume",            l_audio_getVolume             },
    { "setPosition",          l_audio_setPosition           },
    { "getPosition",          l_audio_getPosition           },
    { "setVelocity",          l_audio_setVelocity           },
    { "getVelocity",          l_audio_getVelocity           },
    { "setOrientation",       l_audio_setOrientation        },
    { "getOrientation",       l_audio_getOrientation        },
    { "setDistanceModel",     l_audio_setDistanceModel      },
    { "getDistanceModel",     l_audio_getDistanceModel      },
    { "setDopplerScale",      l_audio_setDopplerScale       },
    { "getDopplerScale",      l_audio_getDopplerScale       },
    { 0, 0 },
  };
  luaL_newlib(L, reg);
  return 1;
}
