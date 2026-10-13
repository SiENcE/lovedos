/**
 * Copyright (c) 2017 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <dos.h>
#include <sys/movedata.h>
#include <pc.h>

#include "vga.h"

int vga_inited = 0;


void vga_init(void) {
  if (vga_inited) return;
  vga_inited = 1;
  union REGS regs = {};
  regs.h.al = 0x13;
  int86(0x10, &regs, &regs);
}


void vga_deinit(void) {
  if (!vga_inited) return;
  vga_inited = 0;
  union REGS regs = {};
  regs.h.al = 0x3;
  int86(0x10, &regs, &regs);
}


void vga_setPalette(int idx, int r, int g, int b) {
  outp(0x03c8, idx);
  outp(0x03c9, (r >> 2) & 0x3f);
  outp(0x03c9, (g >> 2) & 0x3f);
  outp(0x03c9, (b >> 2) & 0x3f);
}


static int vga_vsync = 0;


void vga_setVSync(int enable) {
  vga_vsync = enable;
}


int vga_getVSync(void) {
  return vga_vsync;
}


void vga_update(pixel_t *buffer) {
  if (vga_vsync) {
    /* Wait for the start of the next vertical retrace so the copy isn't
     * visible as tearing */
    while (inportb(0x3da) & 8) {}
    while (!(inportb(0x3da) & 8)) {}
  }
  dosmemput(buffer, VGA_WIDTH * VGA_HEIGHT, 0xa0000);
}
