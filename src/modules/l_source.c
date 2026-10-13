/**
 * Copyright (c) 2017 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#include <string.h>
#include "lib/cmixer/cmixer.h"
#include "lib/dmt/dmt.h"
#include "audio.h"
#include "sounddata.h"
#include "decoder.h"
#include "filesystem.h"
#include "spatial.h"
#include "luaobj.h"


#define CLASS_TYPE  LUAOBJ_TYPE_SOURCE
#define CLASS_NAME  "Source"

/* Registry table of sources which are playing or paused: it keeps them from
 * being garbage collected while the mixer still uses them */
#define ACTIVE_SOURCES "lovedos.activeSources"

#define CLAMP(x, a, b) ((x) < (a) ? (a) : (x) > (b) ? (b) : (x))


enum { SOURCE_STATIC, SOURCE_STREAM, SOURCE_QUEUE };

static const char *sourceTypes[] = { "static", "stream", "queue", NULL };
static const char *newSourceTypes[] = { "static", "stream", NULL };

/* "stream" sources from files at least this large are streamed from disk */
#define STREAM_MIN_SIZE   (256 * 1024)
#define STREAM_CHUNK_SIZE 16384
#define STREAM_BUFFERS    4
#define QUEUE_SLOTS       16
#define QUEUE_DEFAULT     8


/* A ring of sounddata buffers for queueable sources and sources streamed from
 * disk. The mixer (interrupt) reads at `head`, the main program adds buffers
 * at `tail`; with only one reader and one writer no locking is needed */
typedef struct {
  sounddata_t *slots[QUEUE_SLOTS];
  volatile int head, tail;
  volatile int readPos;     /* Frame within slots[head] */
  volatile int starved;     /* Set by the mixer when it ran out of data */
  int size;                 /* Number of usable buffers */
  int bitdepth;
  decoder_t *decoder;       /* Set for sources streamed from disk */
} queue_t;
static const char *timeUnits[] = { "seconds", "samples", NULL };


typedef struct source_t source_t;

struct source_t {
  cm_Source *source;
  sounddata_t *data;  /* For sources in memory */
  queue_t *queue;     /* For queueable and streamed sources */
  char *filename;     /* For streamed sources, so they can be cloned */
  int samplerate, channels, length;
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


static void queue_handler(cm_Event *e) {
  /* Feeds the mixer from a source's buffer queue; called from inside the
   * soundblaster's interrupt handler. Silence is output when the queue is
   * empty. Rewinding and seeking are done by the main program */
  source_t *self = e->udata;
  queue_t *q = self->queue;
  cm_Int16 *dst;
  int len, n;

  if (e->type != CM_EVENT_SAMPLES) return;
  dst = e->buffer;
  len = e->length / 2;
  while (len > 0) {
    sounddata_t *data;
    if (q->head == q->tail) {
      memset(dst, 0, len * 2 * sizeof(*dst));
      q->starved = 1;
      break;
    }
    data = q->slots[q->head];
    n = data->length - q->readPos;
    n = n < len ? n : len;
    sounddata_readStereo(data, q->readPos, n, dst);
    dst += n * 2;
    len -= n;
    q->readPos += n;
    if (q->readPos >= data->length) {
      q->readPos = 0;
      q->head = (q->head + 1) % QUEUE_SLOTS;
    }
  }
}


static int queue_used(queue_t *q) {
  return (q->tail - q->head + QUEUE_SLOTS) % QUEUE_SLOTS;
}


static int queue_push(queue_t *q, sounddata_t *data) {
  /* Takes ownership of `data`; returns 0 if the queue is full */
  if (queue_used(q) >= q->size) return 0;
  /* The slot was used before and has been consumed by the mixer */
  if (q->slots[q->tail]) sounddata_release(q->slots[q->tail]);
  q->slots[q->tail] = data;
  q->tail = (q->tail + 1) % QUEUE_SLOTS;
  return 1;
}


static void queue_clear(queue_t *q) {
  int i, lock = audio_lockMixer();
  q->head = q->tail = 0;
  q->readPos = 0;
  q->starved = 0;
  for (i = 0; i < QUEUE_SLOTS; i++) {
    if (q->slots[i]) sounddata_release(q->slots[i]);
    q->slots[i] = NULL;
  }
  audio_unlockMixer(lock);
}


static void queue_free(queue_t *q) {
  queue_clear(q);
  if (q->decoder) decoder_close(q->decoder);
  dmt_free(q);
}


static void refill(source_t *self) {
  /* Decodes chunks of a streamed source into its free buffers. Like other
   * mixer streams it loops, the mixer decides when the source ends */
  queue_t *q = self->queue;
  while (q->decoder && queue_used(q) < q->size) {
    sounddata_t *data;
    if (decoder_decode(q->decoder, &data) || !data) {
      decoder_seek(q->decoder, 0);
      if (decoder_decode(q->decoder, &data) || !data) return;
    }
    queue_push(q, data);
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
  if (self->channels == 1) {
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
  /* Creates the mixer source for the source's (already set) sounddata or
   * queue */
  cm_SourceInfo info;
  if (self->data) {
    self->samplerate = self->data->samplerate;
    self->channels = self->data->channels;
    self->length = self->data->length;
  }
  info.handler = self->queue ? queue_handler : stream_handler;
  info.udata = self;
  info.samplerate = self->samplerate;
  info.length = self->length;
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


static void openStream(lua_State *L, source_t *self, const char *filename) {
  /* Sets the source up to be streamed from the file */
  queue_t *q;
  decoder_t *dec;
  const char *err = decoder_open(&dec, filename, STREAM_CHUNK_SIZE);
  if (err) luaL_error(L, "%s", err);
  q = dmt_calloc(1, sizeof(*q));
  q->size = STREAM_BUFFERS;
  q->decoder = dec;
  q->bitdepth = dec->bitdepth;
  self->queue = q;
  self->filename = dmt_malloc(strlen(filename) + 1);
  strcpy(self->filename, filename);
  self->samplerate = dec->samplerate;
  self->channels = dec->channels;
  self->length = dec->length;
  refill(self);
}


sounddata_t *l_sounddata_toSoundData(lua_State *L, int idx);
const char *l_decoder_getFilename(lua_State *L, int idx);

int l_source_new(lua_State *L) {
  /* newSource(filename [, type]), newSource(soundData) or
   * newSource(decoder [, type]) */
  sounddata_t *data = l_sounddata_toSoundData(L, 1);
  const char *decoderFile = l_decoder_getFilename(L, 1);
  const char *filename = data || decoderFile ? NULL : luaL_checkstring(L, 1);
  int type = luaL_checkoption(L, 2, "stream", newSourceTypes);
  const char *err = NULL;
  filesystem_info_t info;
  source_t *self = newSource(L);
  if (data) {
    /* Like LÖVE, the source gets its own copy of the SoundData */
    self->type = SOURCE_STATIC;
    err = sounddata_clone(&self->data, data);
  } else if (decoderFile) {
    self->type = SOURCE_STREAM;
    openStream(L, self, decoderFile);
  } else {
    self->type = type;
    if (type == SOURCE_STREAM && filesystem_getInfo(filename, &info) >= 0 &&
        info.size >= STREAM_MIN_SIZE) {
      openStream(L, self, filename);
    } else {
      err = sounddata_newFromFile(&self->data, filename);
    }
  }
  if (err) luaL_error(L, "%s", err);
  initSource(L, self);
  return 1;
}


int l_source_newQueueable(lua_State *L) {
  /* newQueueableSource(samplerate, bitdepth, channels [, buffercount]) */
  int rate = luaL_checkinteger(L, 1);
  int bits = luaL_checkinteger(L, 2);
  int channels = luaL_checkinteger(L, 3);
  int count = luaL_optinteger(L, 4, 0);
  source_t *self;
  queue_t *q;
  if (rate <= 0) luaL_argerror(L, 1, "invalid sample rate");
  if (bits != 8 && bits != 16) luaL_argerror(L, 2, "bit depth must be 8 or 16");
  if (channels != 1 && channels != 2) {
    luaL_argerror(L, 3, "channel count must be 1 or 2");
  }
  if (count <= 0) count = QUEUE_DEFAULT;
  if (count > QUEUE_SLOTS - 1) count = QUEUE_SLOTS - 1;
  self = newSource(L);
  self->type = SOURCE_QUEUE;
  q = dmt_calloc(1, sizeof(*q));
  q->size = count;
  q->bitdepth = bits;
  self->queue = q;
  self->samplerate = rate;
  self->channels = channels;
  /* The mixer needs a length; queueable sources stop when they run out of
   * buffers instead */
  self->length = 0x7fffffff;
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
  if (self->queue) queue_free(self->queue);
  if (self->filename) dmt_free(self->filename);
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
  source_t *clone;
  if (self->type == SOURCE_QUEUE) {
    luaL_error(L, "queueable sources can't be cloned");
  }
  clone = newSource(L);
  clone->type = self->type;
  clone->looping = self->looping;
  clone->volume = self->volume;
  clone->minVolume = self->minVolume;
  clone->maxVolume = self->maxVolume;
  clone->pitch = self->pitch;
  clone->spatial = self->spatial;
  if (self->data) {
    clone->data = self->data;
    sounddata_retain(clone->data);
  } else {
    openStream(L, clone, self->filename);
  }
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


static void seekStream(source_t *self, int frame) {
  /* Restarts decoding a streamed source at the mixer block containing
   * `frame` (see cm_seek()) */
  int block = frame - frame % 128;
  queue_clear(self->queue);
  decoder_seek(self->queue->decoder, block);
  refill(self);
}


int l_source_stop(lua_State *L) {
  source_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  cm_stop(self->source);
  setActive(L, 1, 0);
  if (self->queue) {
    /* Stopping clears a queueable source's queue and rewinds a stream */
    queue_clear(self->queue);
    if (self->queue->decoder) seekStream(self, 0);
  }
  return 0;
}


static void seek(source_t *self, int frame) {
  if (self->type == SOURCE_QUEUE) return;
  if (self->queue) {
    /* Pause while the stream is refilled so the mixer neither reads stale
     * buffers nor runs ahead of the data */
    int playing = cm_get_state(self->source) == CM_STATE_PLAYING;
    if (playing) cm_pause(self->source);
    seekStream(self, frame);
    cm_seek(self->source, frame);
    if (playing) cm_play(self->source);
  } else {
    cm_seek(self->source, frame);
  }
}


int l_source_rewind(lua_State *L) {
  source_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  seek(self, 0);
  return 0;
}


int l_source_seek(lua_State *L) {
  source_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  double offset = luaL_checknumber(L, 2);
  int unit = luaL_checkoption(L, 3, "seconds", timeUnits);
  if (unit == 0) {
    offset *= self->samplerate;
  }
  if (offset < 0) {
    luaL_argerror(L, 2, "offset must not be negative");
  }
  seek(self, offset < self->length ? offset : self->length);
  return 0;
}


int l_source_tell(lua_State *L) {
  source_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  int unit = luaL_checkoption(L, 2, "seconds", timeUnits);
  int frame = cm_tell(self->source);
  if (unit == 0) {
    lua_pushnumber(L, frame / (double) self->samplerate);
  } else {
    lua_pushinteger(L, frame);
  }
  return 1;
}


int l_source_getDuration(lua_State *L) {
  source_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  int unit = luaL_checkoption(L, 2, "seconds", timeUnits);
  if (unit == 0) {
    if (self->type == SOURCE_QUEUE) {
      lua_pushnumber(L, -1);
      return 1;
    }
    lua_pushnumber(L, self->length / (double) self->samplerate);
  } else {
    lua_pushinteger(L, self->type == SOURCE_QUEUE ? -1 : self->length);
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
  lua_pushinteger(L, self->channels);
  return 1;
}


int l_source_queue(lua_State *L) {
  /* queue(sounddata): adds a copy of the sound data to a queueable source's
   * queue. Returns false if there is no free buffer */
  source_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  sounddata_t *data = l_sounddata_toSoundData(L, 2), *copy;
  const char *err;
  if (self->type != SOURCE_QUEUE) {
    luaL_error(L, "only queueable sources can be queued");
  }
  if (!data) luaL_argerror(L, 2, "expected SoundData");
  if (data->samplerate != self->samplerate ||
      data->bitdepth != self->queue->bitdepth ||
      data->channels != self->channels) {
    luaL_error(L, "the sound data's format doesn't match the source's");
  }
  if (queue_used(self->queue) >= self->queue->size) {
    lua_pushboolean(L, 0);
    return 1;
  }
  err = sounddata_clone(&copy, data);
  if (err) luaL_error(L, "%s", err);
  queue_push(self->queue, copy);
  self->queue->starved = 0;
  lua_pushboolean(L, 1);
  return 1;
}


int l_source_getFreeBufferCount(lua_State *L) {
  source_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  if (self->type != SOURCE_QUEUE) {
    lua_pushinteger(L, 0);
  } else {
    lua_pushinteger(L, self->queue->size - queue_used(self->queue));
  }
  return 1;
}


void l_source_updateStreams(void) {
  /* Called once per frame: refills sources streamed from disk and stops
   * queueable sources which ran out of buffers, as LÖVE does */
  source_t *s;
  for (s = sources; s; s = s->next) {
    if (!s->queue) continue;
    if (s->queue->decoder) {
      refill(s);
    } else if (s->queue->starved && queue_used(s->queue) == 0 &&
               cm_get_state(s->source) == CM_STATE_PLAYING) {
      cm_stop(s->source);
    }
  }
}


static source_t *checkMono(lua_State *L) {
  source_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  if (self->channels != 1) {
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
    { "queue",            l_source_queue            },
    { "getFreeBufferCount", l_source_getFreeBufferCount },
    { 0, 0 },
  };
  luaobj_newclass(L, CLASS_NAME, NULL, l_source_new, reg);
  return 1;
}
