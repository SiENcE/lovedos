/**
 * Copyright (c) 2017 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#ifndef MOUSE_H
#define MOUSE_H

enum {
  MOUSE_BUTTON_LEFT,
  MOUSE_BUTTON_RIGHT,
  MOUSE_BUTTON_MIDDLE,
  MOUSE_BUTTON_MAX
};


void mouse_init(void);
void mouse_update(void);
int mouse_isDown(int button);
int mouse_getX(void);
int mouse_getY(void);
void mouse_setPosition(int x, int y);
void mouse_setRelativeMode(int enable);

extern int mouse_inited;
extern int mouse_relative;

#endif
