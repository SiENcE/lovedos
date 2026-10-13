/**
 * Copyright (c) 2017 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#include <unistd.h>
#include <string.h>
#include "filesystem.h"
#include "luaobj.h"


static int pushResult(lua_State *L, int err) {
  if (err) {
    lua_pushboolean(L, 0);
    lua_pushstring(L, filesystem_strerror(err));
    return 2;
  }
  lua_pushboolean(L, 1);
  return 1;
}


int l_filesystem_mount(lua_State *L) {
  const char *path = luaL_checkstring(L, 1);
  int err = filesystem_mount(path);
  if (err) {
    lua_pushnil(L);
    lua_pushstring(L, filesystem_strerror(err));
    return 2;
  }
  lua_pushboolean(L, 1);
  return 1;
}


int l_filesystem_unmount(lua_State *L) {
  const char *path = luaL_checkstring(L, 1);
  int err = filesystem_unmount(path);
  if (err) {
    lua_pushnil(L);
    lua_pushstring(L, filesystem_strerror(err));
    return 2;
  }
  lua_pushboolean(L, 1);
  return 1;
}


int l_filesystem_exists(lua_State *L) {
  const char *filename = luaL_checkstring(L, 1);
  lua_pushboolean( L, filesystem_exists(filename) );
  return 1;
}


int l_filesystem_isFile(lua_State *L) {
  const char *filename = luaL_checkstring(L, 1);
  lua_pushboolean( L, filesystem_isFile(filename) );
  return 1;
}


int l_filesystem_isDirectory(lua_State *L) {
  const char *filename = luaL_checkstring(L, 1);
  lua_pushboolean( L, filesystem_isDirectory(filename) );
  return 1;
}


int l_filesystem_read(lua_State *L) {
  const char *filename = luaL_checkstring(L, 1);
  int size;
  void *data = filesystem_read(filename, &size);
  if (!data) {
    luaL_error(L, "could not read file");
  }
  lua_pushlstring(L, data, size);
  filesystem_free(data);
  return 1;
}


int l_filesystem_setWriteDir(lua_State *L) {
  const char *path = luaL_checkstring(L, 1);
  int err = filesystem_setWriteDir(path);
  if (err) {
    lua_pushnil(L);
    lua_pushstring(L, filesystem_strerror(err));
    return 2;
  }
  lua_pushboolean(L, 1);
  return 1;
}


int l_filesystem_write(lua_State *L) {
  size_t sz;
  const char *filename = luaL_checkstring(L, 1);
  const char *data = luaL_tolstring(L, 2, &sz);
  int err = filesystem_write(filename, data, sz);
  if (err) {
    luaL_error(L, "%s", filesystem_strerror(err));
  }
  lua_pushboolean(L, 1);
  return 1;
}


int l_filesystem_getInfo(lua_State *L) {
  /* getInfo(path [, filtertype] [, info]): returns a table with the fields
   * type ("file" or "directory"), size and modtime, or nil */
  static const char *types[] = { "file", "directory", NULL };
  const char *filename = luaL_checkstring(L, 1);
  filesystem_info_t info;
  int filter = -1, tableIdx = 0;
  if (lua_type(L, 2) == LUA_TSTRING) {
    filter = luaL_checkoption(L, 2, NULL, types);
    if (lua_istable(L, 3)) tableIdx = 3;
  } else if (lua_istable(L, 2)) {
    tableIdx = 2;
  }
  if (filesystem_getInfo(filename, &info) < 0) {
    lua_pushnil(L);
    return 1;
  }
  if (filter >= 0 && (filter == 0) != (info.type == FILESYSTEM_TREG)) {
    lua_pushnil(L);
    return 1;
  }
  if (tableIdx) {
    lua_pushvalue(L, tableIdx);
  } else {
    lua_newtable(L);
  }
  lua_pushstring(L, info.type == FILESYSTEM_TREG ? "file" : "directory");
  lua_setfield(L, -2, "type");
  lua_pushnumber(L, info.size);
  lua_setfield(L, -2, "size");
  if (info.modtime >= 0) {
    lua_pushnumber(L, info.modtime);
  } else {
    lua_pushnil(L);
  }
  lua_setfield(L, -2, "modtime");
  return 1;
}


static void addItem(void *udata, const char *name) {
  /* Adds the name to the set at -2 and, if it is new, to the array at -1 */
  lua_State *L = udata;
  lua_getfield(L, -2, name);
  if (lua_isnil(L, -1)) {
    lua_pushboolean(L, 1);
    lua_setfield(L, -4, name);
    lua_pushstring(L, name);
    lua_rawseti(L, -3, lua_rawlen(L, -3) + 1);
  }
  lua_pop(L, 1);
}


int l_filesystem_getDirectoryItems(lua_State *L) {
  const char *dir = luaL_checkstring(L, 1);
  lua_newtable(L);  /* set of names */
  lua_newtable(L);  /* result */
  filesystem_list(dir, addItem, L);
  return 1;
}


int l_filesystem_append(lua_State *L) {
  size_t sz;
  const char *filename = luaL_checkstring(L, 1);
  const char *data = luaL_checklstring(L, 2, &sz);
  int size = luaL_optinteger(L, 3, sz);
  if (size < 0 || size > (int) sz) size = sz;
  return pushResult(L, filesystem_append(filename, data, size));
}


int l_filesystem_remove(lua_State *L) {
  const char *filename = luaL_checkstring(L, 1);
  lua_pushboolean(L, filesystem_remove(filename) == FILESYSTEM_ESUCCESS);
  return 1;
}


int l_filesystem_createDirectory(lua_State *L) {
  const char *path = luaL_checkstring(L, 1);
  lua_pushboolean(L, filesystem_createDirectory(path) == FILESYSTEM_ESUCCESS);
  return 1;
}


int l_filesystem_getRealDirectory(lua_State *L) {
  const char *path = filesystem_getRealDirectory(luaL_checkstring(L, 1));
  if (!path) {
    lua_pushnil(L);
    lua_pushstring(L, "file does not exist");
    return 2;
  }
  lua_pushstring(L, path);
  return 1;
}


int l_filesystem_getWorkingDirectory(lua_State *L) {
  char buf[512];
  if (!getcwd(buf, sizeof(buf))) {
    lua_pushnil(L);
    return 1;
  }
  lua_pushstring(L, buf);
  return 1;
}


int l_filesystem_getWriteDir(lua_State *L) {
  /* Internal: used by File objects and getSaveDirectory() */
  lua_pushstring(L, filesystem_getWriteDir());
  return 1;
}


int luaopen_filesystem(lua_State *L) {
  luaL_Reg reg[] = {
    { "mount",        l_filesystem_mount        },
    { "unmount",      l_filesystem_unmount      },
    { "exists",       l_filesystem_exists       },
    { "isFile",       l_filesystem_isFile       },
    { "isDirectory",  l_filesystem_isDirectory  },
    { "read",         l_filesystem_read         },
    { "setWriteDir",  l_filesystem_setWriteDir  },
    { "write",        l_filesystem_write        },
    { "append",       l_filesystem_append       },
    { "remove",       l_filesystem_remove       },
    { "createDirectory",    l_filesystem_createDirectory    },
    { "getInfo",            l_filesystem_getInfo            },
    { "getDirectoryItems",  l_filesystem_getDirectoryItems  },
    { "getRealDirectory",   l_filesystem_getRealDirectory   },
    { "getWorkingDirectory", l_filesystem_getWorkingDirectory },
    { "_getWriteDir",       l_filesystem_getWriteDir        },
    { 0, 0 },
  };
  luaL_newlib(L, reg);
  return 1;
}
