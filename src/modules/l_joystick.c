/**
 * Copyright (c) 2017 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#include <pc.h>
#include <dos.h>
#include <time.h>
#include "luaobj.h"


/* The PC gameport (port 0x201) supports two joysticks with two axes and two
 * buttons each, or one joystick with four buttons. The buttons are read
 * directly; an axis' position is the length of a pulse started by writing to
 * the port, which is measured with the PIT timer */

#define GAMEPORT      0x201
#define PIT_HZ        1193182
#define TIMEOUT_TICKS (PIT_HZ / 250)    /* 4ms; full deflection is ~1.2ms */
#define DEADZONE      0.08

typedef struct {
  int present;
  int center, min, max;
} axis_t;

static axis_t axes[4];
static int initialized = 0;


static unsigned readPIT(void) {
  /* Latches and reads channel 0's counter */
  unsigned lo, hi;
  outportb(0x43, 0x00);
  lo = inportb(0x40);
  hi = inportb(0x40);
  return lo | (hi << 8);
}


static void readAxes(int *ticks) {
  /* Measures the pulse length of the four axes in PIT ticks; -1 for axes
   * which don't respond. Interrupts are disabled while measuring so that
   * the audio interrupt can't distort the result (delaying it by <4ms) */
  unsigned start, now, elapsed;
  int i, pending = 0xf, bits, wasEnabled;
  for (i = 0; i < 4; i++) ticks[i] = -1;
  wasEnabled = disable();
  outportb(GAMEPORT, 0xff);
  start = readPIT();
  do {
    bits = inportb(GAMEPORT);
    now = readPIT();
    /* uclock() puts the PIT into mode 2, counting down by one per tick */
    elapsed = (start - now) & 0xffff;
    for (i = 0; i < 4; i++) {
      if ((pending & (1 << i)) && !(bits & (1 << i))) {
        ticks[i] = elapsed;
        pending &= ~(1 << i);
      }
    }
  } while (pending && elapsed < TIMEOUT_TICKS);
  if (wasEnabled) enable();
}


static double normalize(axis_t *a, int t) {
  /* Maps the pulse length to -1..1, calibrating from the center reading at
   * startup and the extremes seen so far */
  double v;
  if (t < a->min) a->min = t;
  if (t > a->max) a->max = t;
  if (t < a->center) {
    v = (t - a->center) / (double) (a->center - a->min);
  } else {
    v = (t - a->center) / (double) (a->max - a->center);
  }
  if (v > -DEADZONE && v < DEADZONE) return 0;
  return v < -1 ? -1 : v > 1 ? 1 : v;
}


int l_joystick_detect(lua_State *L) {
  /* Internal: returns which axes are present (a bit mask). The joysticks are
   * assumed to be centered when this is called */
  int ticks[4], i, mask = 0;
  uclock();   /* Makes sure the PIT is in mode 2 */
  readAxes(ticks);
  for (i = 0; i < 4; i++) {
    axes[i].present = ticks[i] >= 0;
    if (axes[i].present) {
      mask |= 1 << i;
      axes[i].center = ticks[i];
      /* Initial guess for the extremes, widened as they are seen */
      axes[i].min = ticks[i] / 2;
      axes[i].max = ticks[i] * 3 / 2 + 1;
    }
  }
  initialized = 1;
  lua_pushinteger(L, mask);
  return 1;
}


int l_joystick_read(lua_State *L) {
  /* Internal: returns the four axis values (nil for missing axes) and the
   * buttons as a bit mask (bit 0 is button 1) */
  int ticks[4], i, buttons;
  if (!initialized) {
    luaL_error(L, "joystick not initialized");
  }
  readAxes(ticks);
  for (i = 0; i < 4; i++) {
    if (axes[i].present && ticks[i] >= 0) {
      lua_pushnumber(L, normalize(&axes[i], ticks[i]));
    } else {
      lua_pushnil(L);
    }
  }
  buttons = (~inportb(GAMEPORT) >> 4) & 0xf;
  lua_pushinteger(L, buttons);
  return 5;
}


int luaopen_joystick(lua_State *L) {
  luaL_Reg reg[] = {
    { "_detect",  l_joystick_detect },
    { "_read",    l_joystick_read   },
    { 0, 0 },
  };
  luaL_newlib(L, reg);
  return 1;
}
