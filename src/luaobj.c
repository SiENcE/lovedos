/** 
 * Copyright (c) 2017 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "luaobj.h"


/* LÖVE's supertypes of the classes, for Object:typeOf(). LoveDOS' Image
 * doubles as LÖVE's Canvas and ImageData */
static const char *supertypes[][5] = {
  { "Image", "Texture", "Drawable", "Canvas", "ImageData" },
  { "SoundData", "Data", NULL },
  { "SpriteBatch", "Drawable", NULL },
  { "Text", "Drawable", NULL },
  { NULL },
};


static int luaobj_type(lua_State *L) {
  lua_pushvalue(L, lua_upvalueindex(1));
  return 1;
}


static int luaobj_typeOf(lua_State *L) {
  const char *class = lua_tostring(L, lua_upvalueindex(1));
  const char *name = luaL_checkstring(L, 2);
  int i, j, res = !strcmp(name, class) || !strcmp(name, "Object");
  for (i = 0; !res && supertypes[i][0]; i++) {
    if (strcmp(supertypes[i][0], class)) continue;
    for (j = 1; j < 5 && supertypes[i][j]; j++) {
      if (!strcmp(supertypes[i][j], name)) res = 1;
    }
  }
  lua_pushboolean(L, res);
  return 1;
}


static int luaobj_gc(lua_State *L) {
  /* Wraps the class' __gc so that it isn't run for released objects */
  luaobj_head_t *udata = lua_touserdata(L, 1);
  if (!udata || (udata->type & LUAOBJ_RELEASED)) return 0;
  lua_pushvalue(L, lua_upvalueindex(1));
  lua_pushvalue(L, 1);
  lua_call(L, 1, 0);
  return 0;
}


static int luaobj_release(lua_State *L) {
  /* Object:release(): frees the object's resources now instead of when it
   * is garbage collected; the object can't be used afterwards */
  luaobj_head_t *udata = lua_touserdata(L, 1);
  if (!udata) {
    luaL_argerror(L, 1, "expected an object");
  }
  if (udata->type & LUAOBJ_RELEASED) {
    lua_pushboolean(L, 0);
    return 1;
  }
  /* Objects in use as the current font or canvas are kept in the registry
   * under their address and can't be released */
  lua_pushlightuserdata(L, udata + 1);
  lua_rawget(L, LUA_REGISTRYINDEX);
  if (!lua_isnil(L, -1)) {
    lua_pushboolean(L, 0);
    return 1;
  }
  lua_pop(L, 1);
  if (!lua_isnil(L, lua_upvalueindex(1))) {
    lua_pushvalue(L, lua_upvalueindex(1));
    lua_pushvalue(L, 1);
    lua_call(L, 1, 0);
  }
  udata->type |= LUAOBJ_RELEASED;
  lua_pushboolean(L, 1);
  return 1;
}


int luaobj_newclass(lua_State *L, const char *name, const char *extends, 
                    int (*constructor)(lua_State*), luaL_Reg* reg
) {
  lua_CFunction gc = NULL;
  luaL_Reg *r;
  for (r = reg; r->name; r++) {
    if (!strcmp(r->name, "__gc")) gc = r->func;
  }
  /* Creates and pushes a new metatable which represents a class containing the
   * functions in the `reg` argument. If the `extends` argument is not NULL
   * this class will use the specified class as a super class */

  /* Build metatable */ 
  luaL_newmetatable(L, name);
  /* Add type name string */
  lua_pushstring(L, "__type");
  lua_pushstring(L, name);
  lua_rawset(L, -3);
  /* Make and add func table */
  lua_newtable(L);
  luaL_setfuncs(L, reg, 0); 
  /* Object methods */
  lua_pushstring(L, name);
  lua_pushcclosure(L, luaobj_type, 1);
  lua_setfield(L, -2, "type");
  lua_pushstring(L, name);
  lua_pushcclosure(L, luaobj_typeOf, 1);
  lua_setfield(L, -2, "typeOf");
  if (gc) lua_pushcfunction(L, gc); else lua_pushnil(L);
  lua_pushcclosure(L, luaobj_release, 1);
  lua_setfield(L, -2, "release");
  lua_pushstring(L, "__index");
  lua_pushvalue(L, -2);
  lua_rawset(L, -4);
  /* Handle extended */
  if (extends) {
    luaL_getmetatable(L, extends);
    /* Pull metatable functions from base class */
    char *mtfields[] = { "__gc", "__tostring", NULL };
    int i;
    for (i = 0; mtfields[i]; i++) {
      lua_getfield(L, -1, mtfields[i]);
      lua_setfield(L, -4, mtfields[i]);
    }
    lua_setmetatable(L, -2);
  }
  lua_pop(L, 1);              /* Pop func table */
  luaL_setfuncs(L, reg, 0);   /* Set metatable's funcs */
  if (gc) {
    lua_pushcfunction(L, gc);
    lua_pushcclosure(L, luaobj_gc, 1);
    lua_setfield(L, -2, "__gc");
  }
  lua_pop(L, 1);              /* Pop metatable */

  /* Return constructor */
  lua_pushcfunction(L, constructor);

  return 1;
}


void luaobj_setclass(lua_State *L, uint32_t type, char *name) {
  /* Sets the the metatable of the member at the top of the stack to the
   * class-metatable of the given `name` */
  luaobj_head_t *udata = lua_touserdata(L, -1);
  udata->type = type;
  luaL_getmetatable(L, name);
  if (lua_isnil(L, -1)) {
    luaL_error(
      L, "missing metatable: assure class '%s' is inited in love.c\n", name);
  }
  lua_setmetatable(L, -2);
}


void *luaobj_newudata(lua_State *L, int size) {
  /* Creates a udata of the requested size, plus space for the luaobj_head_t
   * header. Returns the pointer to the udata's body */
  int s = size + sizeof(luaobj_head_t);
  luaobj_head_t *udata = lua_newuserdata(L, s);
  udata->type = 0;
  return udata + 1;
}


void *luaobj_checkudata(lua_State *L, int index, uint32_t type) {
  /* Checks the header of a luaobj udata at the given index, taking into
   * account super-classes it may have extended. Returns the udata's body if
   * it is of the correct class, else errors out */
  index = lua_absindex(L, index);
  luaobj_head_t *udata = lua_touserdata(L, index);
  if (!udata || !(udata->type & type)) {
    luaL_argerror(L, index, "bad type");
  }
  if (udata->type & LUAOBJ_RELEASED) {
    luaL_argerror(L, index, "object has been released");
  }
  return udata + 1;
}

