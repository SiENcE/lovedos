/**
 * Copyright (c) 2017 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <dos.h>
#include <crt0.h>

#include "lib/dmt/dmt.h"

#include "luaobj.h"
#include "vga.h"
#include "audio.h"
#include "keyboard.h"
#include "filesystem.h"
#include "mouse.h"
#include "image.h"
#include "palette.h"
#include "package.h"


/* Lock all memory as it is allocated: the audio mixer runs inside the
 * soundblaster's interrupt handler and touches code, sample data and Lua
 * objects all over the heap, none of which may be paged out by a DPMI host
 * using virtual memory (eg. CWSDPMI with a swap file or a Windows DOS box) */
int _crt0_startup_flags = _CRT0_FLAG_LOCK_MEMORY;

static lua_State *L;

static void deinit(void) {
  /* Deinit and clear up everything. Called at exit */
  audio_deinit();
  vga_deinit();
  keyboard_deinit();
  lua_close(L);
  filesystem_deinit();
  if ( dmt_usage() > 0 ) {
    dmt_dump(stdout);
  }
}


static int onLuaPanic(lua_State *L) {
  vga_deinit();
  const char *err = lua_tostring(L, -1);
  printf("lua panic: %s\n", err);
  return 0;
}


int luaopen_love(lua_State *L);

int main(int argc, char **argv) {

  /* Handle package command */
  if ( package_run(argc, argv) == PACKAGE_ESUCCESS ) {
    exit(EXIT_SUCCESS);
  }

  /* Init everything */
  atexit(deinit);
  /* The audio is initialized by boot.lua (love.audio._init) once conf.lua
   * has been run, as games may disable it */
  vga_init();
  palette_init();
  keyboard_init();
  mouse_init();

  /* Init lua */
  L = luaL_newstate();
  lua_atpanic(L, onLuaPanic);
  luaL_openlibs(L);
  luaL_requiref(L, "love", luaopen_love, 1);

  /* Create `love.argv` and fill with arguments */
  lua_getglobal(L, "love");
  if (!lua_isnil(L, -1)) {
    lua_newtable(L);
    int i;
    for (i = 0; i < argc; i++) {
      lua_pushstring(L, argv[i]);
      lua_rawseti(L, -2, i + 1);
    }
    lua_setfield(L, -2, "argv");
  }
  lua_pop(L, 1);

  /* Init embedded scripts */
  #include "nogame_lua.h"
  #include "filesystem_lua.h"
  #include "math_lua.h"
  #include "event_lua.h"
  #include "joystick_lua.h"
  #include "boot_lua.h"
  struct {
    const char *name, *data; int size;
  } items[] = {
    { "nogame.lua",     nogame_lua,     sizeof(nogame_lua)      },
    { "filesystem.lua", filesystem_lua, sizeof(filesystem_lua)  },
    { "math.lua",       math_lua,       sizeof(math_lua)        },
    { "event.lua",      event_lua,      sizeof(event_lua)       },
    { "joystick.lua",   joystick_lua,   sizeof(joystick_lua)    },
    { "boot.lua",     boot_lua,     sizeof(boot_lua)    },
    { NULL, NULL, 0 }
  };
  int i;
  for (i = 0; items[i].name; i++) {
    int err = luaL_loadbuffer(L, items[i].data, items[i].size, items[i].name);
    if (err || lua_pcall(L, 0, 0, 0) != 0) {
      const char *str = lua_tostring(L, -1);
      fprintf(stderr, "Error: %s\n", str);
      abort();
    }
  }

  return 0;
}
