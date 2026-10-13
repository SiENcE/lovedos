/**
 * Copyright (c) 2017 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#include "sounddata.h"
#include "luaobj.h"


#define CLASS_TYPE  LUAOBJ_TYPE_SOUNDDATA
#define CLASS_NAME  "SoundData"


typedef struct {
  sounddata_t *data;
} l_sounddata_t;


static l_sounddata_t *newSoundData(lua_State *L) {
  l_sounddata_t *self = luaobj_newudata(L, sizeof(*self));
  luaobj_setclass(L, CLASS_TYPE, CLASS_NAME);
  self->data = NULL;
  return self;
}


int l_sounddata_new(lua_State *L) {
  const char *err;
  l_sounddata_t *self;
  if (lua_type(L, 1) == LUA_TSTRING) {
    const char *filename = lua_tostring(L, 1);
    self = newSoundData(L);
    err = sounddata_newFromFile(&self->data, filename);
  } else {
    int samples = luaL_checkinteger(L, 1);
    int rate = luaL_optinteger(L, 2, 44100);
    int bits = luaL_optinteger(L, 3, 16);
    int channels = luaL_optinteger(L, 4, 2);
    self = newSoundData(L);
    err = sounddata_new(&self->data, samples, rate, bits, channels);
  }
  if (err) luaL_error(L, "%s", err);
  return 1;
}


int l_sounddata_push(lua_State *L, sounddata_t *data) {
  /* Pushes a new SoundData object which takes ownership of `data` */
  l_sounddata_t *self = newSoundData(L);
  self->data = data;
  return 1;
}


sounddata_t *l_sounddata_toSoundData(lua_State *L, int idx) {
  /* Returns the sounddata of the SoundData object at the given index, or NULL
   * if the value is not a SoundData object */
  luaobj_head_t *udata = luaL_testudata(L, idx, CLASS_NAME);
  if (!udata) {
    return NULL;
  }
  return ((l_sounddata_t*) (udata + 1))->data;
}


int l_sounddata_gc(lua_State *L) {
  l_sounddata_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  if (self->data) sounddata_release(self->data);
  return 0;
}


static int checkSampleIndex(lua_State *L, sounddata_t *data, int withChannel) {
  /* Reads the sample index at argument 2 and, if `withChannel` is set, the
   * channel at argument 3. Returns the index of the interleaved sample */
  int idx = luaL_checkinteger(L, 2);
  if (withChannel) {
    int channel = luaL_checkinteger(L, 3);
    if (channel < 1 || channel > data->channels) {
      luaL_argerror(L, 3, "invalid channel");
    }
    if (idx < 0 || idx >= data->length) {
      luaL_error(L, "attempt to access out-of-range sample");
    }
    return idx * data->channels + (channel - 1);
  }
  if (idx < 0 || idx >= data->length * data->channels) {
    luaL_error(L, "attempt to access out-of-range sample");
  }
  return idx;
}


int l_sounddata_getSample(lua_State *L) {
  /* getSample(i) or getSample(i, channel) */
  l_sounddata_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  int idx = checkSampleIndex(L, self->data, !lua_isnoneornil(L, 3));
  lua_pushnumber(L, sounddata_getSample(self->data, idx));
  return 1;
}


int l_sounddata_setSample(lua_State *L) {
  /* setSample(i, sample) or setSample(i, channel, sample) */
  l_sounddata_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  int withChannel = !lua_isnoneornil(L, 4);
  int idx = checkSampleIndex(L, self->data, withChannel);
  double value = luaL_checknumber(L, withChannel ? 4 : 3);
  sounddata_setSample(self->data, idx, value);
  return 0;
}


int l_sounddata_getSampleCount(lua_State *L) {
  l_sounddata_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  lua_pushinteger(L, self->data->length);
  return 1;
}


int l_sounddata_getSampleRate(lua_State *L) {
  l_sounddata_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  lua_pushinteger(L, self->data->samplerate);
  return 1;
}


int l_sounddata_getBitDepth(lua_State *L) {
  l_sounddata_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  lua_pushinteger(L, self->data->bitdepth);
  return 1;
}


int l_sounddata_getChannelCount(lua_State *L) {
  l_sounddata_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  lua_pushinteger(L, self->data->channels);
  return 1;
}


int l_sounddata_getDuration(lua_State *L) {
  l_sounddata_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  lua_pushnumber(L, self->data->length / (double) self->data->samplerate);
  return 1;
}


int l_sounddata_getSize(lua_State *L) {
  l_sounddata_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  lua_pushinteger(L, sounddata_getSize(self->data));
  return 1;
}


int l_sounddata_getString(lua_State *L) {
  l_sounddata_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  lua_pushlstring(L, self->data->data, sounddata_getSize(self->data));
  return 1;
}


int l_sounddata_clone(lua_State *L) {
  l_sounddata_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  l_sounddata_t *clone = newSoundData(L);
  const char *err = sounddata_clone(&clone->data, self->data);
  if (err) luaL_error(L, "%s", err);
  return 1;
}


int luaopen_sounddata(lua_State *L) {
  luaL_Reg reg[] = {
    { "new",              l_sounddata_new             },
    { "__gc",             l_sounddata_gc              },
    { "getSample",        l_sounddata_getSample       },
    { "setSample",        l_sounddata_setSample       },
    { "getSampleCount",   l_sounddata_getSampleCount  },
    { "getSampleRate",    l_sounddata_getSampleRate   },
    { "getBitDepth",      l_sounddata_getBitDepth     },
    { "getChannelCount",  l_sounddata_getChannelCount },
    { "getChannels",      l_sounddata_getChannelCount },
    { "getDuration",      l_sounddata_getDuration     },
    { "getSize",          l_sounddata_getSize         },
    { "getString",        l_sounddata_getString       },
    { "clone",            l_sounddata_clone           },
    { 0, 0 },
  };
  luaobj_newclass(L, CLASS_NAME, NULL, l_sounddata_new, reg);
  return 1;
}
