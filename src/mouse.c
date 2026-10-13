/**
 * Copyright (c) 2017 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#include <stdlib.h>
#include <string.h>
#include <dos.h>
#include "luaobj.h"
#include "mouse.h"
#include "event.h"

int mouse_inited;
int mouse_hasWheel;
int mouse_relative;
int mouse_x, mouse_y;
int mouse_lastX, mouse_lastY;
int mouse_buttonStates[MOUSE_BUTTON_MAX];


void mouse_init(void) {
  union REGS regs = {};
  int86(0x33, &regs, &regs);
  mouse_inited = regs.x.ax ? 1 : 0;
  if (!mouse_inited) {
    return;
  }
  /* CuteMouse compatible drivers report wheel movement (function 11h) */
  memset(&regs, 0, sizeof(regs));
  regs.x.ax = 0x11;
  int86(0x33, &regs, &regs);
  mouse_hasWheel = regs.x.ax == 0x574d && (regs.x.cx & 1);
}


void mouse_setPosition(int x, int y) {
  union REGS regs = {};
  if (!mouse_inited) {
    return;
  }
  regs.x.ax = 4;
  regs.x.cx = x << 1;
  regs.x.dx = y;
  int86(0x33, &regs, &regs);
  mouse_x = mouse_lastX = x;
  mouse_y = mouse_lastY = y;
}


void mouse_setRelativeMode(int enable) {
  /* In relative mode the pointer is kept at the center of the screen so
   * that movement isn't stopped by the screen's edges; mousemoved events
   * report the movement while the position stays the same */
  mouse_relative = enable;
  if (enable) {
    mouse_setPosition(160, 100);
  }
}


void mouse_update(void) {
  if (!mouse_inited) {
    return;
  }

  /* Update mouse position */
  union REGS regs = {};
  regs.x.ax = 3;
  int86(0x33, &regs, &regs);
  mouse_x = regs.x.cx >> 1;
  mouse_y = regs.x.dx;

  /* Wheel movement since the last call is in BH (negative is up) */
  if (mouse_hasWheel && regs.h.bh) {
    event_t e;
    e.type = EVENT_MOUSE_WHEELMOVED;
    e.mouse.dx = 0;
    e.mouse.dy = -(signed char) regs.h.bh;
    event_push(&e);
  }

  if (mouse_relative) {
    int dx = mouse_x - 160, dy = mouse_y - 100;
    if (dx || dy) {
      event_t e;
      e.type = EVENT_MOUSE_MOVED;
      e.mouse.x = mouse_x = mouse_lastX;
      e.mouse.y = mouse_y = mouse_lastY;
      e.mouse.dx = dx;
      e.mouse.dy = dy;
      event_push(&e);
      memset(&regs, 0, sizeof(regs));
      regs.x.ax = 4;
      regs.x.cx = 160 << 1;
      regs.x.dx = 100;
      int86(0x33, &regs, &regs);
    }
  } else if (mouse_x != mouse_lastX || mouse_y != mouse_lastY) {
    /* Do moved event if mouse moved */
    event_t e;
    e.type = EVENT_MOUSE_MOVED;
    e.mouse.x = mouse_x;
    e.mouse.y = mouse_y;
    e.mouse.dx = mouse_x - mouse_lastX;
    e.mouse.dy = mouse_y - mouse_lastY;
    event_push(&e);
  }

  /* Update last position */
  mouse_lastX = mouse_x;
  mouse_lastY = mouse_y;

  /* Update button states and push pressed/released events */
  int i, t;
  for (i = 0; i < MOUSE_BUTTON_MAX; i++) {
    for (t = 0; t < 2; t++) {
      memset(&regs, 0, sizeof(regs));
      regs.x.ax = 5 + t;
      regs.x.bx = i;
      int86(0x33, &regs, &regs);
      if (regs.x.bx) {
        /* Push event */
        event_t e;
        e.type = t == 0 ? EVENT_MOUSE_PRESSED : EVENT_MOUSE_RELEASED;
        e.mouse.button = i;
        e.mouse.x = mouse_x;
        e.mouse.y = mouse_y;
        event_push(&e);
        /* Set state */
        mouse_buttonStates[i] = t == 0 ? 1 : 0;
      }
    }
  }
}



int mouse_isDown(int button) {
  return mouse_buttonStates[button];
}


int mouse_getX(void) {
  return mouse_x;
}


int mouse_getY(void) {
  return mouse_y;
}
