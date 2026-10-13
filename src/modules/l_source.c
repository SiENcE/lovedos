/**
 * Copyright (c) 2017 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#include <string.h>
#include "lib/cmixer/cmixer.h"
#include "audio.h"
#include "sounddata.h"
#include "spatial.h"
#include "luaobj.h"


#define CLASS_TYPE  LUAOBJ_TYPE_SOURCE
#define CLASS_NAME  "Source"

/* Registry table of sources which are playing or paused: it keeps them from
 * being garbage collected while the mixer still uses them */
#define ACTIVE_SOURCES "lovedos.activeSources"

#define CLAMP(x, a, b) ((x) < (a) ? (a) : (x) > (b) ? (b) : (x))


enum { SOURCE_STATIC, SOURCE_STREAM };

static const char *sourceTypes[] = { "static", "stream", NULL };
static const char *timeUnits[] = { "seconds", "samples", NULL };


typedef struct source_t source_t;

struct source_t {
  cm_Source *source;
  sounddata_t *data;
  int idx;            /* Read position of the stream; used by the mixer */
  int type;
  int looping;
  double volume, minVolume, maxVolume;
  double pitch;
  spatial_source_t spatial;
  double spatialGain, spatialPan, spatialPitch;
  source_t *prev, *next;
};

/* All sources with a mixer source, so they can be updated when the listener
 * changes */
static source_t *sources;


static void stream_handler(cm_Event *e) {
  /* Feeds the mixer from the source's sounddata. This is called from inside
   * the soundblaster's interrupt handler, so it must stay simple and free of
   * floating point maths */
  source_t *self = e->udata;
  sounddata_t *data = self->data;
  cm_Int16 *dst;
  int len, n;

  switch (e->type) {
    case CM_EVENT_SAMPLES:
      /* The mixer expects streams to continuously loop */
      dst = e->buffer;
      len = e->length / 2;
      while (len > 0) {
        n = data->length - self->idx;
        n = n < len ? n : len;
        sounddata_readStereo(data, self->idx, n, dst);
        dst += n * 2;
        len -= n;
        self->idx += n;
        if (self->idx >= data->length) {
          self->idx = 0;
        }
      }
      break;

    case CM_EVENT_REWIND:
      self->idx = 0;
      break;

    case CM_EVENT_SEEK:
      self->idx = e->position % data->length;
      break;
  }
}


static void updateParams(source_t *self) {
  /* Applies volume, pitch and the (cached) positional audio results to the
   * mixer source */
  double gain = self->volume * self->spatialGain;
  cm_set_gain(self->source, CLAMP(gain, self->minVolume, self->maxVolume));
  cm_set_pan(self->source, self->spatialPan);
  cm_set_pitch(self->source, self->pitch * self->spatialPitch);
}


static void updateSpatial(source_t *self) {
  /* Recalculates the positional audio of mono sources; only needed when the
   * source's or the listener's positional settings change */
  if (self->data->channels == 1) {
    spatial_compute(&self->spatial, &self->spatialGain, &self->spatialPan,
                    &self->spatialPitch);
  }
  updateParams(self);
}


void l_source_updateAll(void) {
  /* Called when the listener changes */
  source_t *s;
  for (s = sources; s; s = s->next) {
    updateSpatial(s);
  }
}


static source_t *newSource(lua_State *L) {
  source_t *self = luaobj_newudata(L, sizeof(*self));
  luaobj_setclass(L, CLASS_TYPE, CLASS_NAME);
  memset(self, 0, sizeof(*self));
  self->volume = 1;
  self->minVolume = 0;
  self->maxVolume = 1;
  self->pitch = 1;
  spatial_initSource(&self->spatial);
  self->spatialGain = 1;
  self->spatialPan = 0;
  self->spatialPitch = 1;
  return self;
}


static void initSource(lua_State *L, source_t *self) {
  /* Creates the mixer source for the source's (already set) sounddata */
  cm_SourceInfo info;
  info.handler = stream_handler;
  info.udata = self;
  info.samplerate = self->data->samplerate;
  info.length = self->data->length;
  self->source = cm_new_source(&info);
  if (!self->source) {
    luaL_error(L, "%s", cm_get_error());
  }
  updateSpatial(self);
  cm_set_loop(self->source, self->looping);
  /* Add to sources list */
  self->prev = NULL;
  self->next = sources;
  if (sources) sources->prev = self;
  sources = self;
}


sounddata_t *l_sounddata_toSoundData(lua_State *L, int idx);

int l_source_new(lua_State *L) {
  sounddata_t *data = l_sounddata_toSoundData(L, 1);
  const char *filename = data ? NULL : luaL_checkstring(L, 1);
  int type = luaL_checkoption(L, 2, "stream", sourceTypes);
  const char *err;
  source_t *self = newSource(L);
  if (data) {
    /* Like LÖVE, the source gets its own copy of the SoundData */
    self->type = SOURCE_STATIC;
    err = sounddata_clone(&self->data, data);
  } else {
    self->type = type;
    err = sounddata_newFromFile(&self->data, filename);
  }
  if (err) luaL_error(L, "%s", err);
  initSource(L, self);
  return 1;
}


int l_source_gc(lua_State *L) {
  source_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  if (self->source) {
    /* Remove from sources list */
    if (self->prev) self->prev->next = self->next;
    else sources = self->next;
    if (self->next) self->next->prev = self->prev;
    cm_destroy_source(self->source);
  }
  if (self->data) sounddata_release(self->data);
  return 0;
}


static void setActive(lua_State *L, int idx, int active) {
  idx = lua_absindex(L, idx);
  luaL_getsubtable(L, LUA_REGISTRYINDEX, ACTIVE_SOURCES);
  lua_pushvalue(L, idx);
  if (active) {
    lua_pushboolean(L, 1);
  } else {
    lua_pushnil(L);
  }
  lua_rawset(L, -3);
  lua_pop(L, 1);
}


void l_source_pushActive(lua_State *L) {
  /* Pushes an array of all the sources which are currently playing or paused.
   * Sources which have stopped by reaching their end are removed from the
   * active sources table */
  int n = 0;
  luaL_getsubtable(L, LUA_REGISTRYINDEX, ACTIVE_SOURCES);
  lua_newtable(L);
  lua_pushnil(L);
  while (lua_next(L, -3)) {
    source_t *self = luaobj_checkudata(L, -2, CLASS_TYPE);
    lua_pop(L, 1);
    if (cm_get_state(self->source) == CM_STATE_STOPPED) {
      /* Clearing an existing field during traversal is allowed */
      lua_pushvalue(L, -1);
      lua_pushnil(L);
      lua_rawset(L, -5);
    } else {
      lua_pushvalue(L, -1);
      lua_rawseti(L, -3, ++n);
    }
  }
  lua_remove(L, -2);
}


int l_source_clone(lua_State *L) {
  source_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  source_t *clone = newSource(L);
  clone->type = self->type;
  clone->looping = self->looping;
  clone->volume = self->volume;
  clone->minVolume = self->minVolume;
  clone->maxVolume = self->maxVolume;
  clone->pitch = self->pitch;
  clone->spatial = self->spatial;
  clone->data = self->data;
  sounddata_retain(clone->data);
  initSource(L, clone);
  return 1;
}


int l_source_play(lua_State *L) {
  source_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  if (!audio_isInitialized()) {
    /* Without a soundblaster nothing would ever advance the source */
    lua_pushboolean(L, 0);
    return 1;
  }
  /* Drop references to sources which have finished playing */
  l_source_pushActive(L);
  lua_pop(L, 1);
  cm_play(self->source);
  setActive(L, 1, 1);
  lua_pushboolean(L, 1);
  return 1;
}


int l_source_pause(lua_State *L) {
  source_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  if (cm_get_state(self->source) == CM_STATE_PLAYING) {
    cm_pause(self->source);
  }
  return 0;
}


int l_source_resume(lua_State *L) {
  source_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  if (cm_get_state(self->source) == CM_STATE_PAUSED) {
    cm_play(self->source);
  }
  return 0;
}


int l_source_stop(lua_State *L) {
  source_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  cm_stop(self->source);
  setActive(L, 1, 0);
  return 0;
}


int l_source_rewind(lua_State *L) {
  source_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  cm_seek(self->source, 0);
  return 0;
}


int l_source_seek(lua_State *L) {
  source_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  double offset = luaL_checknumber(L, 2);
  int unit = luaL_checkoption(L, 3, "seconds", timeUnits);
  if (unit == 0) {
    offset *= self->data->samplerate;
  }
  if (offset < 0) {
    luaL_argerror(L, 2, "offset must not be negative");
  }
  cm_seek(self->source, offset < self->data->length ? offset
                                                     : self->data->length);
  return 0;
}


int l_source_tell(lua_State *L) {
  source_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  int unit = luaL_checkoption(L, 2, "seconds", timeUnits);
  int frame = cm_tell(self->source);
  if (unit == 0) {
    lua_pushnumber(L, frame / (double) self->data->samplerate);
  } else {
    lua_pushinteger(L, frame);
  }
  return 1;
}


int l_source_getDuration(lua_State *L) {
  source_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  int unit = luaL_checkoption(L, 2, "seconds", timeUnits);
  if (unit == 0) {
    lua_pushnumber(L, self->data->length / (double) self->data->samplerate);
  } else {
    lua_pushinteger(L, self->data->length);
  }
  return 1;
}


int l_source_setVolume(lua_State *L) {
  source_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  double n = luaL_checknumber(L, 2);
  self->volume = n < 0 ? 0 : n;
  updateParams(self);
  return 0;
}


int l_source_getVolume(lua_State *L) {
  source_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  lua_pushnumber(L, self->volume);
  return 1;
}


int l_source_setVolumeLimits(lua_State *L) {
  source_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  double min = luaL_checknumber(L, 2);
  double max = luaL_checknumber(L, 3);
  if (min < 0 || max > 1 || min > max) {
    luaL_error(L, "invalid volume limits: [%f, %f]; must be [0, 1]", min, max);
  }
  self->minVolume = min;
  self->maxVolume = max;
  updateParams(self);
  return 0;
}


int l_source_getVolumeLimits(lua_State *L) {
  source_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  lua_pushnumber(L, self->minVolume);
  lua_pushnumber(L, self->maxVolume);
  return 2;
}


int l_source_setPitch(lua_State *L) {
  source_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  double n = luaL_checknumber(L, 2);
  /* `n * 0 == 0` is false for infinity and NaN */
  if (!(n > 0) || n * 0 != 0) {
    luaL_argerror(L, 2, "pitch must be a positive, finite number");
  }
  self->pitch = n;
  updateParams(self);
  return 0;
}


int l_source_getPitch(lua_State *L) {
  source_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  lua_pushnumber(L, self->pitch);
  return 1;
}


int l_source_setLooping(lua_State *L) {
  source_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  self->looping = lua_toboolean(L, 2);
  cm_set_loop(self->source, self->looping);
  return 0;
}


int l_source_isLooping(lua_State *L) {
  source_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  lua_pushboolean(L, self->looping);
  return 1;
}


int l_source_isPlaying(lua_State *L) {
  source_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  lua_pushboolean(L, cm_get_state(self->source) == CM_STATE_PLAYING);
  return 1;
}


int l_source_isPaused(lua_State *L) {
  source_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  lua_pushboolean(L, cm_get_state(self->source) == CM_STATE_PAUSED);
  return 1;
}


int l_source_isStopped(lua_State *L) {
  source_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  lua_pushboolean(L, cm_get_state(self->source) == CM_STATE_STOPPED);
  return 1;
}


int l_source_getType(lua_State *L) {
  source_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  lua_pushstring(L, sourceTypes[self->type]);
  return 1;
}


int l_source_getChannelCount(lua_State *L) {
  source_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  lua_pushinteger(L, self->data->channels);
  return 1;
}


static source_t *checkMono(lua_State *L) {
  source_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  if (self->data->channels != 1) {
    luaL_error(L, "positional audio is only available for mono sources");
  }
  return self;
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


int l_source_setPosition(lua_State *L) {
  source_t *self = checkMono(L);
  checkVec3(L, 2, &self->spatial.position);
  updateSpatial(self);
  return 0;
}


int l_source_getPosition(lua_State *L) {
  return pushVec3(L, &checkMono(L)->spatial.position);
}


int l_source_setVelocity(lua_State *L) {
  source_t *self = checkMono(L);
  checkVec3(L, 2, &self->spatial.velocity);
  updateSpatial(self);
  return 0;
}


int l_source_getVelocity(lua_State *L) {
  return pushVec3(L, &checkMono(L)->spatial.velocity);
}


int l_source_setDirection(lua_State *L) {
  source_t *self = checkMono(L);
  checkVec3(L, 2, &self->spatial.direction);
  updateSpatial(self);
  return 0;
}


int l_source_getDirection(lua_State *L) {
  return pushVec3(L, &checkMono(L)->spatial.direction);
}


int l_source_setCone(lua_State *L) {
  source_t *self = checkMono(L);
  double inner = luaL_checknumber(L, 2);
  double outer = luaL_checknumber(L, 3);
  double volume = luaL_optnumber(L, 4, 0);
  if (inner < 0 || outer < inner || volume < 0 || volume > 1) {
    luaL_error(L, "invalid cone: angles must be 0 <= inner <= outer and the "
                  "outer volume between 0 and 1");
  }
  self->spatial.innerCone = inner;
  self->spatial.outerCone = outer;
  self->spatial.outerConeVolume = volume;
  updateSpatial(self);
  return 0;
}


int l_source_getCone(lua_State *L) {
  source_t *self = checkMono(L);
  lua_pushnumber(L, self->spatial.innerCone);
  lua_pushnumber(L, self->spatial.outerCone);
  lua_pushnumber(L, self->spatial.outerConeVolume);
  return 3;
}


int l_source_setRelative(lua_State *L) {
  source_t *self = checkMono(L);
  self->spatial.relative = lua_toboolean(L, 2);
  updateSpatial(self);
  return 0;
}


int l_source_isRelative(lua_State *L) {
  lua_pushboolean(L, checkMono(L)->spatial.relative);
  return 1;
}


int l_source_setAttenuationDistances(lua_State *L) {
  source_t *self = checkMono(L);
  double ref = luaL_checknumber(L, 2);
  double max = luaL_checknumber(L, 3);
  if (ref < 0 || max < 0) {
    luaL_error(L, "attenuation distances must not be negative");
  }
  self->spatial.refDistance = ref;
  self->spatial.maxDistance = max;
  updateSpatial(self);
  return 0;
}


int l_source_getAttenuationDistances(lua_State *L) {
  source_t *self = checkMono(L);
  lua_pushnumber(L, self->spatial.refDistance);
  lua_pushnumber(L, self->spatial.maxDistance);
  return 2;
}


int l_source_setRolloff(lua_State *L) {
  source_t *self = checkMono(L);
  double n = luaL_checknumber(L, 2);
  if (n < 0) {
    luaL_argerror(L, 2, "rolloff must not be negative");
  }
  self->spatial.rolloff = n;
  updateSpatial(self);
  return 0;
}


int l_source_getRolloff(lua_State *L) {
  lua_pushnumber(L, checkMono(L)->spatial.rolloff);
  return 1;
}


int luaopen_source(lua_State *L) {
  luaL_Reg reg[] = {
    { "new",              l_source_new              },
    { "__gc",             l_source_gc               },
    { "clone",            l_source_clone            },
    { "play",             l_source_play             },
    { "pause",            l_source_pause            },
    { "resume",           l_source_resume           },
    { "stop",             l_source_stop             },
    { "rewind",           l_source_rewind           },
    { "seek",             l_source_seek             },
    { "tell",             l_source_tell             },
    { "getDuration",      l_source_getDuration      },
    { "setVolume",        l_source_setVolume        },
    { "getVolume",        l_source_getVolume        },
    { "setVolumeLimits",  l_source_setVolumeLimits  },
    { "getVolumeLimits",  l_source_getVolumeLimits  },
    { "setPitch",         l_source_setPitch         },
    { "getPitch",         l_source_getPitch         },
    { "setLooping",       l_source_setLooping       },
    { "isLooping",        l_source_isLooping        },
    { "isPlaying",        l_source_isPlaying        },
    { "isPaused",         l_source_isPaused         },
    { "isStopped",        l_source_isStopped        },
    { "getType",          l_source_getType          },
    { "getChannelCount",  l_source_getChannelCount  },
    { "getChannels",      l_source_getChannelCount  },
    { "setPosition",      l_source_setPosition      },
    { "getPosition",      l_source_getPosition      },
    { "setVelocity",      l_source_setVelocity      },
    { "getVelocity",      l_source_getVelocity      },
    { "setDirection",     l_source_setDirection     },
    { "getDirection",     l_source_getDirection     },
    { "setCone",          l_source_setCone          },
    { "getCone",          l_source_getCone          },
    { "setRelative",      l_source_setRelative      },
    { "isRelative",       l_source_isRelative       },
    { "setAttenuationDistances", l_source_setAttenuationDistances },
    { "getAttenuationDistances", l_source_getAttenuationDistances },
    { "setRolloff",       l_source_setRolloff       },
    { "getRolloff",       l_source_getRolloff       },
    { 0, 0 },
  };
  luaobj_newclass(L, CLASS_NAME, NULL, l_source_new, reg);
  return 1;
}
