/**
 * Copyright (c) 2017 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "luaobj.h"


#define CLASS_TYPE  LUAOBJ_TYPE_RANDOMGENERATOR
#define CLASS_NAME  "RandomGenerator"


/*============================================================================
** RandomGenerator: the same Xorshift* generator and seeding as LÖVE, so that
** a seed gives the same numbers as in LÖVE
**============================================================================*/

typedef unsigned long long u64;

typedef struct {
  u64 seed;
  u64 state;
  double lastNormal;
  int hasLastNormal;
} rng_t;

static rng_t defaultRng;


static u64 wangHash64(u64 key) {
  key = (~key) + (key << 21);
  key = key ^ (key >> 24);
  key = (key + (key << 3)) + (key << 8);
  key = key ^ (key >> 14);
  key = (key + (key << 2)) + (key << 4);
  key = key ^ (key >> 28);
  key = key + (key << 31);
  return key;
}


static void rng_setSeed(rng_t *self, u64 seed) {
  self->seed = seed;
  /* Xorshift can't have a state of 0 and gives similar sequences for similar
   * seeds, so the seed is hashed */
  do {
    seed = wangHash64(seed);
  } while (seed == 0);
  self->state = seed;
  self->hasLastNormal = 0;
}


static u64 rng_rand(rng_t *self) {
  self->state ^= self->state >> 12;
  self->state ^= self->state << 25;
  self->state ^= self->state >> 27;
  return self->state * 2685821657736338717ULL;
}


static double rng_random(rng_t *self) {
  /* A double in [0, 1) from the top 52 bits */
  union { u64 i; double d; } u;
  u.i = (0x3ffULL << 52) | (rng_rand(self) >> 12);
  return u.d - 1.0;
}


static double rng_randomNormal(rng_t *self, double stddev) {
  double r, phi;
  if (self->hasLastNormal) {
    self->hasLastNormal = 0;
    return self->lastNormal * stddev;
  }
  r = sqrt(-2.0 * log(1. - rng_random(self)));
  phi = 2.0 * M_PI * (1. - rng_random(self));
  self->lastNormal = r * cos(phi);
  self->hasLastNormal = 1;
  return r * sin(phi) * stddev;
}


static u64 checkSeed(lua_State *L, int idx) {
  /* A seed is a number, or two numbers with the low and high 32 bits */
  if (!lua_isnoneornil(L, idx + 1)) {
    u64 low = (unsigned) luaL_checknumber(L, idx);
    u64 high = (unsigned) luaL_checknumber(L, idx + 1);
    return low | (high << 32);
  } else {
    double n = luaL_checknumber(L, idx);
    if (n != n || n * 0 != 0) {
      luaL_argerror(L, idx, "invalid seed");
    }
    return (u64) n;
  }
}


static int pushRandom(lua_State *L, rng_t *rng, int idx) {
  /* random(), random(max) or random(min, max) like math.random */
  int n = lua_gettop(L) - idx + 1;
  double r = rng_random(rng);
  if (n <= 0) {
    lua_pushnumber(L, r);
  } else {
    double lo = 1, hi;
    if (n >= 2) {
      lo = luaL_checknumber(L, idx);
      hi = luaL_checknumber(L, idx + 1);
    } else {
      hi = luaL_checknumber(L, idx);
    }
    lua_pushnumber(L, floor(r * (hi - lo + 1) + lo));
  }
  return 1;
}


static int pushNormal(lua_State *L, rng_t *rng, int idx) {
  double stddev = luaL_optnumber(L, idx, 1);
  double mean = luaL_optnumber(L, idx + 1, 0);
  lua_pushnumber(L, mean + rng_randomNormal(rng, stddev));
  return 1;
}


static int pushSeed(lua_State *L, rng_t *rng) {
  lua_pushnumber(L, (double) (rng->seed & 0xffffffffULL));
  lua_pushnumber(L, (double) (rng->seed >> 32));
  return 2;
}


static int pushState(lua_State *L, rng_t *rng) {
  char buf[32];
  sprintf(buf, "0x%016llx", rng->state);
  lua_pushstring(L, buf);
  return 1;
}


static void checkState(lua_State *L, int idx, rng_t *rng) {
  const char *s = luaL_checkstring(L, idx);
  u64 state;
  if (sscanf(s, "0x%llx", &state) != 1 || state == 0) {
    luaL_argerror(L, idx, "invalid random state");
  }
  rng->state = state;
  rng->hasLastNormal = 0;
}


int l_randomgenerator_new(lua_State *L) {
  /* newRandomGenerator([seed]) or newRandomGenerator(low, high) */
  /* Without a seed LÖVE's default seed for new generators is used */
  u64 seed = lua_isnoneornil(L, 1) ? 0x0139408dcbbf7a44ULL : checkSeed(L, 1);
  rng_t *self = luaobj_newudata(L, sizeof(*self));
  luaobj_setclass(L, CLASS_TYPE, CLASS_NAME);
  memset(self, 0, sizeof(*self));
  rng_setSeed(self, seed);
  return 1;
}

int l_randomgenerator_random(lua_State *L) {
  return pushRandom(L, luaobj_checkudata(L, 1, CLASS_TYPE), 2);
}

int l_randomgenerator_randomNormal(lua_State *L) {
  return pushNormal(L, luaobj_checkudata(L, 1, CLASS_TYPE), 2);
}

int l_randomgenerator_setSeed(lua_State *L) {
  rng_setSeed(luaobj_checkudata(L, 1, CLASS_TYPE), checkSeed(L, 2));
  return 0;
}

int l_randomgenerator_getSeed(lua_State *L) {
  return pushSeed(L, luaobj_checkudata(L, 1, CLASS_TYPE));
}

int l_randomgenerator_getState(lua_State *L) {
  return pushState(L, luaobj_checkudata(L, 1, CLASS_TYPE));
}

int l_randomgenerator_setState(lua_State *L) {
  checkState(L, 2, luaobj_checkudata(L, 1, CLASS_TYPE));
  return 0;
}

int l_randomgenerator_clone(lua_State *L) {
  rng_t *self = luaobj_checkudata(L, 1, CLASS_TYPE);
  rng_t *clone = luaobj_newudata(L, sizeof(*clone));
  luaobj_setclass(L, CLASS_TYPE, CLASS_NAME);
  *clone = *self;
  return 1;
}


int l_math_random(lua_State *L) {
  return pushRandom(L, &defaultRng, 1);
}

int l_math_randomNormal(lua_State *L) {
  return pushNormal(L, &defaultRng, 1);
}

int l_math_setRandomSeed(lua_State *L) {
  rng_setSeed(&defaultRng, checkSeed(L, 1));
  return 0;
}

int l_math_getRandomSeed(lua_State *L) {
  return pushSeed(L, &defaultRng);
}

int l_math_getRandomState(lua_State *L) {
  return pushState(L, &defaultRng);
}

int l_math_setRandomState(lua_State *L) {
  checkState(L, 1, &defaultRng);
  return 0;
}


/*============================================================================
** Noise: simplex noise for 1 and 2 dimensions and Perlin noise for 3 and 4
** dimensions, as in LÖVE, based on Stefan Gustavson's public domain
** implementations. The results are in the range 0 to 1
**============================================================================*/

static const unsigned char perm[512] = {
  151,160,137,91,90,15,131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,
  8,99,37,240,21,10,23,190,6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,
  35,11,32,57,177,33,88,237,149,56,87,174,20,125,136,171,168,68,175,74,165,71,
  134,139,48,27,166,77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,
  55,46,245,40,244,102,143,54,65,25,63,161,1,216,80,73,209,76,132,187,208,89,
  18,169,200,196,135,130,116,188,159,86,164,100,109,198,173,186,3,64,52,217,226,
  250,124,123,5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,
  189,28,42,223,183,170,213,119,248,152,2,44,154,163,70,221,153,101,155,167,43,
  172,9,129,22,39,253,19,98,108,110,79,113,224,232,178,185,112,104,218,246,97,
  228,251,34,242,193,238,210,144,12,191,179,162,241,81,51,145,235,249,14,239,
  107,49,192,214,31,181,199,106,157,184,84,204,176,115,121,50,45,127,4,150,254,
  138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180,
  151,160,137,91,90,15,131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,
  8,99,37,240,21,10,23,190,6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,
  35,11,32,57,177,33,88,237,149,56,87,174,20,125,136,171,168,68,175,74,165,71,
  134,139,48,27,166,77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,
  55,46,245,40,244,102,143,54,65,25,63,161,1,216,80,73,209,76,132,187,208,89,
  18,169,200,196,135,130,116,188,159,86,164,100,109,198,173,186,3,64,52,217,226,
  250,124,123,5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,
  189,28,42,223,183,170,213,119,248,152,2,44,154,163,70,221,153,101,155,167,43,
  172,9,129,22,39,253,19,98,108,110,79,113,224,232,178,185,112,104,218,246,97,
  228,251,34,242,193,238,210,144,12,191,179,162,241,81,51,145,235,249,14,239,
  107,49,192,214,31,181,199,106,157,184,84,204,176,115,121,50,45,127,4,150,254,
  138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180,
};

#define FASTFLOOR(x) (((x) > 0) ? ((int) (x)) : ((int) (x) - 1))
#define FADE(t) ((t) * (t) * (t) * ((t) * ((t) * 6 - 15) + 10))
#define LERP(t, a, b) ((a) + (t) * ((b) - (a)))


static double grad1(int hash, double x) {
  int h = hash & 15;
  double grad = 1.0 + (h & 7);
  if (h & 8) grad = -grad;
  return grad * x;
}


static double grad2(int hash, double x, double y) {
  int h = hash & 7;
  double u = h < 4 ? x : y;
  double v = h < 4 ? y : x;
  return ((h & 1) ? -u : u) + ((h & 2) ? -2.0 * v : 2.0 * v);
}


static double grad3(int hash, double x, double y, double z) {
  int h = hash & 15;
  double u = h < 8 ? x : y;
  double v = h < 4 ? y : (h == 12 || h == 14) ? x : z;
  return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
}


static double grad4(int hash, double x, double y, double z, double t) {
  int h = hash & 31;
  double u = h < 24 ? x : y;
  double v = h < 16 ? y : z;
  double w = h < 8 ? z : t;
  return ((h & 1) ? -u : u) + ((h & 2) ? -v : v) + ((h & 4) ? -w : w);
}


static double simplex1(double x) {
  int i0 = FASTFLOOR(x);
  int i1 = i0 + 1;
  double x0 = x - i0;
  double x1 = x0 - 1.0;
  double t0 = 1.0 - x0 * x0, t1 = 1.0 - x1 * x1, n0, n1;
  t0 *= t0;
  n0 = t0 * t0 * grad1(perm[i0 & 0xff], x0);
  t1 *= t1;
  n1 = t1 * t1 * grad1(perm[i1 & 0xff], x1);
  return 0.395 * (n0 + n1);
}


static double simplex2(double x, double y) {
  #define F2 0.366025403
  #define G2 0.211324865
  double n0, n1, n2, s = (x + y) * F2;
  int i = FASTFLOOR(x + s), j = FASTFLOOR(y + s);
  double t = (double) (i + j) * G2;
  double x0 = x - (i - t), y0 = y - (j - t);
  int i1, j1, ii, jj;
  double x1, y1, x2, y2, t0, t1, t2;
  if (x0 > y0) { i1 = 1; j1 = 0; } else { i1 = 0; j1 = 1; }
  x1 = x0 - i1 + G2;
  y1 = y0 - j1 + G2;
  x2 = x0 - 1.0 + 2.0 * G2;
  y2 = y0 - 1.0 + 2.0 * G2;
  ii = i & 0xff;
  jj = j & 0xff;
  t0 = 0.5 - x0 * x0 - y0 * y0;
  if (t0 < 0.0) n0 = 0.0;
  else { t0 *= t0; n0 = t0 * t0 * grad2(perm[ii + perm[jj]], x0, y0); }
  t1 = 0.5 - x1 * x1 - y1 * y1;
  if (t1 < 0.0) n1 = 0.0;
  else { t1 *= t1; n1 = t1 * t1 * grad2(perm[ii + i1 + perm[jj + j1]], x1, y1); }
  t2 = 0.5 - x2 * x2 - y2 * y2;
  if (t2 < 0.0) n2 = 0.0;
  else { t2 *= t2; n2 = t2 * t2 * grad2(perm[ii + 1 + perm[jj + 1]], x2, y2); }
  return 40.0 * (n0 + n1 + n2);
  #undef F2
  #undef G2
}


static double perlin3(double x, double y, double z) {
  int ix0 = FASTFLOOR(x), iy0 = FASTFLOOR(y), iz0 = FASTFLOOR(z);
  double fx0 = x - ix0, fy0 = y - iy0, fz0 = z - iz0;
  double fx1 = fx0 - 1.0, fy1 = fy0 - 1.0, fz1 = fz0 - 1.0;
  int ix1 = (ix0 + 1) & 0xff, iy1 = (iy0 + 1) & 0xff, iz1 = (iz0 + 1) & 0xff;
  double r = FADE(fz0), t = FADE(fy0), s = FADE(fx0);
  double nxy0, nxy1, nx0, nx1, n0, n1;
  ix0 &= 0xff; iy0 &= 0xff; iz0 &= 0xff;

  nxy0 = grad3(perm[ix0 + perm[iy0 + perm[iz0]]], fx0, fy0, fz0);
  nxy1 = grad3(perm[ix0 + perm[iy0 + perm[iz1]]], fx0, fy0, fz1);
  nx0 = LERP(r, nxy0, nxy1);
  nxy0 = grad3(perm[ix0 + perm[iy1 + perm[iz0]]], fx0, fy1, fz0);
  nxy1 = grad3(perm[ix0 + perm[iy1 + perm[iz1]]], fx0, fy1, fz1);
  nx1 = LERP(r, nxy0, nxy1);
  n0 = LERP(t, nx0, nx1);

  nxy0 = grad3(perm[ix1 + perm[iy0 + perm[iz0]]], fx1, fy0, fz0);
  nxy1 = grad3(perm[ix1 + perm[iy0 + perm[iz1]]], fx1, fy0, fz1);
  nx0 = LERP(r, nxy0, nxy1);
  nxy0 = grad3(perm[ix1 + perm[iy1 + perm[iz0]]], fx1, fy1, fz0);
  nxy1 = grad3(perm[ix1 + perm[iy1 + perm[iz1]]], fx1, fy1, fz1);
  nx1 = LERP(r, nxy0, nxy1);
  n1 = LERP(t, nx0, nx1);

  return 0.936 * LERP(s, n0, n1);
}


static double perlin4(double x, double y, double z, double w) {
  int ix0 = FASTFLOOR(x), iy0 = FASTFLOOR(y), iz0 = FASTFLOOR(z);
  int iw0 = FASTFLOOR(w);
  double fx0 = x - ix0, fy0 = y - iy0, fz0 = z - iz0, fw0 = w - iw0;
  double fx1 = fx0 - 1.0, fy1 = fy0 - 1.0, fz1 = fz0 - 1.0, fw1 = fw0 - 1.0;
  int ix1 = (ix0 + 1) & 0xff, iy1 = (iy0 + 1) & 0xff;
  int iz1 = (iz0 + 1) & 0xff, iw1 = (iw0 + 1) & 0xff;
  double q = FADE(fw0), r = FADE(fz0), t = FADE(fy0), s = FADE(fx0);
  double nxyz0, nxyz1, nxy0, nxy1, nx0, nx1, n0, n1;
  ix0 &= 0xff; iy0 &= 0xff; iz0 &= 0xff; iw0 &= 0xff;

  #define G(X, Y, Z, W, FX, FY, FZ, FW)\
    grad4(perm[X + perm[Y + perm[Z + perm[W]]]], FX, FY, FZ, FW)

  nxyz0 = G(ix0, iy0, iz0, iw0, fx0, fy0, fz0, fw0);
  nxyz1 = G(ix0, iy0, iz0, iw1, fx0, fy0, fz0, fw1);
  nxy0 = LERP(q, nxyz0, nxyz1);
  nxyz0 = G(ix0, iy0, iz1, iw0, fx0, fy0, fz1, fw0);
  nxyz1 = G(ix0, iy0, iz1, iw1, fx0, fy0, fz1, fw1);
  nxy1 = LERP(q, nxyz0, nxyz1);
  nx0 = LERP(r, nxy0, nxy1);
  nxyz0 = G(ix0, iy1, iz0, iw0, fx0, fy1, fz0, fw0);
  nxyz1 = G(ix0, iy1, iz0, iw1, fx0, fy1, fz0, fw1);
  nxy0 = LERP(q, nxyz0, nxyz1);
  nxyz0 = G(ix0, iy1, iz1, iw0, fx0, fy1, fz1, fw0);
  nxyz1 = G(ix0, iy1, iz1, iw1, fx0, fy1, fz1, fw1);
  nxy1 = LERP(q, nxyz0, nxyz1);
  nx1 = LERP(r, nxy0, nxy1);
  n0 = LERP(t, nx0, nx1);

  nxyz0 = G(ix1, iy0, iz0, iw0, fx1, fy0, fz0, fw0);
  nxyz1 = G(ix1, iy0, iz0, iw1, fx1, fy0, fz0, fw1);
  nxy0 = LERP(q, nxyz0, nxyz1);
  nxyz0 = G(ix1, iy0, iz1, iw0, fx1, fy0, fz1, fw0);
  nxyz1 = G(ix1, iy0, iz1, iw1, fx1, fy0, fz1, fw1);
  nxy1 = LERP(q, nxyz0, nxyz1);
  nx0 = LERP(r, nxy0, nxy1);
  nxyz0 = G(ix1, iy1, iz0, iw0, fx1, fy1, fz0, fw0);
  nxyz1 = G(ix1, iy1, iz0, iw1, fx1, fy1, fz0, fw1);
  nxy0 = LERP(q, nxyz0, nxyz1);
  nxyz0 = G(ix1, iy1, iz1, iw0, fx1, fy1, fz1, fw0);
  nxyz1 = G(ix1, iy1, iz1, iw1, fx1, fy1, fz1, fw1);
  nxy1 = LERP(q, nxyz0, nxyz1);
  nx1 = LERP(r, nxy0, nxy1);
  n1 = LERP(t, nx0, nx1);
  #undef G

  return 0.87 * LERP(s, n0, n1);
}


int l_math_noise(lua_State *L) {
  double v;
  switch (lua_gettop(L)) {
    case 1:  v = simplex1(luaL_checknumber(L, 1)); break;
    case 2:  v = simplex2(luaL_checknumber(L, 1), luaL_checknumber(L, 2));
             break;
    case 3:  v = perlin3(luaL_checknumber(L, 1), luaL_checknumber(L, 2),
                         luaL_checknumber(L, 3));
             break;
    default: v = perlin4(luaL_checknumber(L, 1), luaL_checknumber(L, 2),
                         luaL_checknumber(L, 3), luaL_checknumber(L, 4));
             break;
  }
  lua_pushnumber(L, v * 0.5 + 0.5);
  return 1;
}


int luaopen_randomgenerator(lua_State *L) {
  luaL_Reg reg[] = {
    { "new",          l_randomgenerator_new           },
    { "random",       l_randomgenerator_random        },
    { "randomNormal", l_randomgenerator_randomNormal  },
    { "setSeed",      l_randomgenerator_setSeed       },
    { "getSeed",      l_randomgenerator_getSeed       },
    { "getState",     l_randomgenerator_getState      },
    { "setState",     l_randomgenerator_setState      },
    { "clone",        l_randomgenerator_clone         },
    { 0, 0 },
  };
  luaobj_newclass(L, CLASS_NAME, NULL, l_randomgenerator_new, reg);
  return 1;
}


int luaopen_lovemath(lua_State *L) {
  luaL_Reg reg[] = {
    { "random",             l_math_random           },
    { "randomNormal",       l_math_randomNormal     },
    { "setRandomSeed",      l_math_setRandomSeed    },
    { "getRandomSeed",      l_math_getRandomSeed    },
    { "getRandomState",     l_math_getRandomState   },
    { "setRandomState",     l_math_setRandomState   },
    { "newRandomGenerator", l_randomgenerator_new   },
    { "noise",              l_math_noise            },
    { 0, 0 },
  };
  luaL_newlib(L, reg);
  /* Seed the default generator from the time, as LÖVE does */
  rng_setSeed(&defaultRng, (u64) time(NULL));
  return 1;
}
