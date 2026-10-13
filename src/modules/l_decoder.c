/**
 * Copyright (c) 2017 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#include <string.h>
#include "lib/dmt/dmt.h"
#include "decoder.h"
#include "luaobj.h"


#define CLASS_TYPE  LUAOBJ_TYPE_DECODER
#define CLASS_NAME  "Decoder"


typedef struct {
  decoder_t *decoder;
  char *filename;
  int bufferSize;
} l_decoder_t;


int l_sounddata_push(lua_State *L, sounddata_t *data);


static l_decoder_t *newDecoder(lua_State *L, const char *filename,
                               int bufferSize) {
  const char *err;
  l_decoder_t *self = luaobj_newudata(L, sizeof(*self));
  luaobj_setclass(L, CLASS_TYPE, CLASS_NAME);
  memset(self, 0, sizeof(*self));
  err = decoder_open(&self->decoder, filename, bufferSize);
  if (err) luaL_error(L, "%s", err);
  self->filename = dmt_malloc(strlen(filename) + 1);
  strcpy(self->filename, filename);
  self->bufferSize = bufferSize;
  return self;
}


int l_decoder_new(lua_State *L) {
  /* newDecoder(filename [, buffersize]) */
  const char *filename = luaL_checkstring(L, 1);
  int bufferSize = luaL_optinteger(L, 2, 16384);
  if (bufferSize < 64) bufferSize = 64;
  newDecoder(L, filename, bufferSize);
  return 1;
}


const char *l_decoder_getFilename(lua_State *L, int idx) {
  /* Returns the filename of the Decoder at `idx`, or NULL if it isn't one */
  luaobj_head_t *udata = luaL_testudata(L, idx, CLASS_NAME);
  if (!udata) return NULL;
  return ((l_decoder_t*) (udata + 1))->filename;
}


int l_decoder_gc(lua_State *L) {
  l_decoder_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  if (self->decoder) decoder_close(self->decoder);
  if (self->filename) dmt_free(self->filename);
  return 0;
}


int l_decoder_decode(lua_State *L) {
  /* Returns the next chunk as a SoundData, or nil at the end */
  l_decoder_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  sounddata_t *data;
  const char *err = decoder_decode(self->decoder, &data);
  if (err) luaL_error(L, "%s", err);
  if (!data) return 0;
  return l_sounddata_push(L, data);
}


int l_decoder_seek(lua_State *L) {
  l_decoder_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  double offset = luaL_checknumber(L, 2);
  decoder_seek(self->decoder, offset * self->decoder->samplerate);
  return 0;
}


int l_decoder_tell(lua_State *L) {
  l_decoder_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  lua_pushnumber(L, self->decoder->pos / (double) self->decoder->samplerate);
  return 1;
}


int l_decoder_getDuration(lua_State *L) {
  l_decoder_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  lua_pushnumber(L, self->decoder->length / (double) self->decoder->samplerate);
  return 1;
}


int l_decoder_getSampleRate(lua_State *L) {
  l_decoder_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  lua_pushinteger(L, self->decoder->samplerate);
  return 1;
}


int l_decoder_getBitDepth(lua_State *L) {
  l_decoder_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  lua_pushinteger(L, self->decoder->bitdepth);
  return 1;
}


int l_decoder_getChannelCount(lua_State *L) {
  l_decoder_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  lua_pushinteger(L, self->decoder->channels);
  return 1;
}


int l_decoder_clone(lua_State *L) {
  l_decoder_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  newDecoder(L, self->filename, self->bufferSize);
  return 1;
}


int luaopen_decoder(lua_State *L) {
  luaL_Reg reg[] = {
    { "new",              l_decoder_new             },
    { "__gc",             l_decoder_gc              },
    { "decode",           l_decoder_decode          },
    { "seek",             l_decoder_seek            },
    { "tell",             l_decoder_tell            },
    { "getDuration",      l_decoder_getDuration     },
    { "getSampleRate",    l_decoder_getSampleRate   },
    { "getBitDepth",      l_decoder_getBitDepth     },
    { "getChannelCount",  l_decoder_getChannelCount },
    { "getChannels",      l_decoder_getChannelCount },
    { "clone",            l_decoder_clone           },
    { 0, 0 },
  };
  luaobj_newclass(L, CLASS_NAME, NULL, l_decoder_new, reg);
  return 1;
}
