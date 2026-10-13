/**
 * Copyright (c) 2017 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <math.h>
#include <pc.h>
#include "palette.h"
#include "image.h"
#include "font.h"
#include "quad.h"
#include "vga.h"
#include "luaobj.h"

image_t  *graphics_screen;
font_t   *graphics_defaultFont;

image_t  *graphics_canvas;
font_t   *graphics_font;
pixel_t   graphics_backgroundColor;
int       graphics_backgroundColor_rgb[3];
pixel_t   graphics_color;
int       graphics_color_rgb[3];
int       graphics_blendMode;

/* Transform: only translation is supported, which keeps all drawing on the
 * pixel grid and costs nothing per pixel */
#define TRANSFORM_STACK_MAX 64
typedef struct { double x, y; } transform_t;
static transform_t graphics_transform;
static transform_t graphics_transformStack[TRANSFORM_STACK_MAX];
static int graphics_transformStackSize;

/* Scissor rectangle, in screen coordinates */
static int graphics_scissor;
static int graphics_scissorRect[4];


static inline int fastFloor(double x) {
  /* Cheaper than floor(), which is a library call */
  int i = x;
  return i - (x < i);
}


static inline int toX(double x) {
  return fastFloor(x + graphics_transform.x);
}


static inline int toY(double y) {
  return fastFloor(y + graphics_transform.y);
}


static image_rect_t getClip(void) {
  /* Returns the area of the current canvas which may be drawn to */
  return image_getClip(graphics_canvas->width, graphics_canvas->height);
}


static inline void plot(const image_rect_t *clip, int x, int y) {
  if (x >= clip->x0 && x < clip->x1 && y >= clip->y0 && y < clip->y1) {
    graphics_canvas->data[x + y * graphics_canvas->width] = graphics_color;
  }
}


static inline void hline(const image_rect_t *clip, int x0, int x1, int y) {
  /* Fills the pixels from x0 to x1 (exclusive) of row y */
  if (y < clip->y0 || y >= clip->y1) return;
  if (x0 < clip->x0) x0 = clip->x0;
  if (x1 > clip->x1) x1 = clip->x1;
  if (x0 >= x1) return;
  memset(graphics_canvas->data + x0 + y * graphics_canvas->width,
         graphics_color, x1 - x0);
}


static int checkMode(lua_State *L, int idx) {
  /* Returns 1 for "fill" and 0 for "line" */
  static const char *modes[] = { "line", "fill", NULL };
  return luaL_checkoption(L, idx, NULL, modes);
}


static int getColorFromArgs(lua_State *L, int *rgb, const int *def) {
  int r, g, b;
  if ( lua_isnoneornil(L, 1) ) {
    r = def[0];
    g = def[1];
    b = def[2];
  } else {
    r = luaL_checknumber(L, 1);
    g = luaL_checknumber(L, 2);
    b = luaL_checknumber(L, 3);
  }
  int idx = palette_colorToIdx(r, g, b);
  if (idx < 0) {
    luaL_error(L, "color palette exhausted: use fewer unique colors");
  }
  if (rgb) {
    rgb[0] = r;
    rgb[1] = g;
    rgb[2] = b;
  }
  return idx;
}


static int pushColor(lua_State *L, int *rgb) {
  lua_pushinteger(L, rgb[0]);
  lua_pushinteger(L, rgb[1]);
  lua_pushinteger(L, rgb[2]);
  return 3;
}


int l_graphics_getDimensions(lua_State *L) {
  lua_pushinteger(L, graphics_screen->width);
  lua_pushinteger(L, graphics_screen->height);
  return 2;
}


int l_graphics_getWidth(lua_State *L) {
  lua_pushinteger(L, graphics_screen->width);
  return 1;
}


int l_graphics_getHeight(lua_State *L) {
  lua_pushinteger(L, graphics_screen->height);
  return 1;
}


int l_graphics_getBackgroundColor(lua_State *L) {
  return pushColor(L, graphics_backgroundColor_rgb);
}


int l_graphics_setBackgroundColor(lua_State *L) {
  static const int def[] = { 0, 0, 0 };
  int idx = getColorFromArgs(L, graphics_backgroundColor_rgb, def);
  graphics_backgroundColor = idx;
  return 0;
}


int l_graphics_getColor(lua_State *L) {
  return pushColor(L, graphics_color_rgb);
}


int l_graphics_setColor(lua_State *L) {
  static const int def[] = { 0xff, 0xff, 0xff };
  graphics_color = getColorFromArgs(L, graphics_color_rgb, def);
  image_setColor(graphics_color);
  return 0;
}


int l_graphics_getBlendMode(lua_State *L) {
  switch (graphics_blendMode) {
    default:
    case IMAGE_NORMAL : lua_pushstring(L, "normal");  break;
    case IMAGE_FAST   : lua_pushstring(L, "fast");    break;
    case IMAGE_AND    : lua_pushstring(L, "and");     break;
    case IMAGE_OR     : lua_pushstring(L, "or");      break;
    case IMAGE_COLOR  : lua_pushstring(L, "color");   break;
  }
  return 1;
}


int l_graphics_setBlendMode(lua_State *L) {
  const char *str = lua_isnoneornil(L, 1) ? "normal" : luaL_checkstring(L, 1);
  #define SET_BLEND_MODE(name, e)\
    do {\
      if (!strcmp(str, name)) {\
        graphics_blendMode = e;\
        image_setBlendMode(graphics_blendMode);\
        return 0;\
      }\
    } while (0)

  switch (*str) {
    case 'n'  : SET_BLEND_MODE("normal",  IMAGE_NORMAL);  break;
    case 'f'  : SET_BLEND_MODE("fast",    IMAGE_FAST);    break;
    case 'a'  : SET_BLEND_MODE("and",     IMAGE_AND);     break;
    case 'o'  : SET_BLEND_MODE("or",      IMAGE_OR);      break;
    case 'c'  : SET_BLEND_MODE("color",   IMAGE_COLOR);   break;
  }
  #undef SET_BLEND_MODE
  luaL_argerror(L, 1, "bad blend mode");
  return 0;
}


int l_graphics_getFont(lua_State *L) {
  lua_pushlightuserdata(L, graphics_font);
  lua_gettable(L, LUA_REGISTRYINDEX);
  return 1;
}


int l_graphics_setFont(lua_State *L) {
  font_t *oldFont = graphics_font;
  if (lua_isnoneornil(L, 1)) {
    /* If no arguments are given we use the default embedded font, grab it
     * from the registry and set it as the first argument */
    lua_pushlightuserdata(L, &graphics_defaultFont);
    lua_gettable(L, LUA_REGISTRYINDEX);
    lua_insert(L, 1);
  }
  graphics_font = luaobj_checkudata(L, 1, LUAOBJ_TYPE_FONT);
  /* Remove old font from registry. This is done after we know the args are
   * okay so that the font remains unchanged if an error occurs */
  if (oldFont) {
    lua_pushlightuserdata(L, oldFont);
    lua_pushnil(L);
    lua_settable(L, LUA_REGISTRYINDEX);
  }
  /* Add new font to registry */
  lua_pushlightuserdata(L, graphics_font);
  lua_pushvalue(L, 1);
  lua_settable(L, LUA_REGISTRYINDEX);
  return 0;
}


int l_graphics_getCanvas(lua_State *L) {
  lua_pushlightuserdata(L, graphics_canvas);
  lua_gettable(L, LUA_REGISTRYINDEX);
  return 1;
}


int l_graphics_setCanvas(lua_State *L) {
  image_t *oldCanvas = graphics_canvas;
  if (lua_isnoneornil(L, 1)) {
    /* If no arguments are given we use the screen canvas, grab it from the
     * registry and set it as the first argument */
    lua_pushlightuserdata(L, &graphics_screen);
    lua_gettable(L, LUA_REGISTRYINDEX);
    lua_insert(L, 1);
  }
  graphics_canvas = luaobj_checkudata(L, 1, LUAOBJ_TYPE_IMAGE);
  /* Remove old canvas from registry. This is done after we know the args are
   * okay so that the canvas remains unchanged if an error occurs */
  if (oldCanvas) {
    lua_pushlightuserdata(L, oldCanvas);
    lua_pushnil(L);
    lua_settable(L, LUA_REGISTRYINDEX);
  }
  /* Add new canvas to registry */
  lua_pushlightuserdata(L, graphics_canvas);
  lua_pushvalue(L, 1);
  lua_settable(L, LUA_REGISTRYINDEX);
  return 0;
}


int l_graphics_setScissor(lua_State *L);
int l_graphics_origin(lua_State *L);

int l_graphics_reset(lua_State *L) {
  int (*funcs[])(lua_State*) = {
    l_graphics_setBackgroundColor,
    l_graphics_setColor,
    l_graphics_setBlendMode,
    l_graphics_setFont,
    l_graphics_setCanvas,
    l_graphics_setScissor,
    l_graphics_origin,
    NULL,
  };
  int i;
  for (i = 0; funcs[i]; i++) {
    lua_pushcfunction(L, funcs[i]);
    lua_call(L, 0, 0);
  }
  return 0;
}


int l_graphics_clear(lua_State *L) {
  int idx = getColorFromArgs(L, NULL, graphics_backgroundColor_rgb);
  image_rect_t clip = getClip();
  int y;
  if (!graphics_scissor) {
    memset(graphics_canvas->data, idx,
           graphics_canvas->width * graphics_canvas->height);
    return 0;
  }
  /* Like in LÖVE, clearing is limited to the scissor rectangle */
  for (y = clip.y0; y < clip.y1 && clip.x0 < clip.x1; y++) {
    memset(graphics_canvas->data + clip.x0 + y * graphics_canvas->width,
           idx, clip.x1 - clip.x0);
  }
  return 0;
}


int l_graphics_present(lua_State *L) {
  vga_update(graphics_screen->data);
  return 0;
}


int l_graphics_draw(lua_State *L) {
  /* draw(image, [quad,] x, y, r, sx, sy, ox, oy) or, as in older versions of
   * LoveDOS, draw(image, [quad,] x, y, flip) */
  image_t *img = luaobj_checkudata(L, 1, LUAOBJ_TYPE_IMAGE);
  quad_t *quad = NULL;
  int i = 2;
  int srcx, srcy, srcw, srch;
  double x, y, r = 0, sx = 1, sy = 1, ox = 0, oy = 0;
  pixel_t *buf = graphics_canvas->data;
  int bufw = graphics_canvas->width;
  int bufh = graphics_canvas->height;

  if (!lua_isnone(L, 2) && lua_type(L, 2) != LUA_TNUMBER) {
    quad = luaobj_checkudata(L, 2, LUAOBJ_TYPE_QUAD);
    i = 3;
  }
  if (quad) {
    srcx = quad->x;
    srcy = quad->y;
    srcw = quad->width;
    srch = quad->height;
  } else {
    srcx = srcy = 0;
    srcw = img->width;
    srch = img->height;
  }
  x = luaL_optnumber(L, i, 0) + graphics_transform.x;
  y = luaL_optnumber(L, i + 1, 0) + graphics_transform.y;

  if (lua_gettop(L) <= i + 1 || lua_type(L, i + 2) == LUA_TBOOLEAN) {
    /* No transformation, or the legacy horizontal flip argument */
    image_setFlip(lua_toboolean(L, i + 2));
    image_blit(img, buf, bufw, bufh, fastFloor(x), fastFloor(y),
               srcx, srcy, srcw, srch);
    return 0;
  }
  r  = luaL_optnumber(L, i + 2, 0);
  sx = luaL_optnumber(L, i + 3, 1);
  sy = luaL_optnumber(L, i + 4, sx);
  ox = luaL_optnumber(L, i + 5, 0);
  oy = luaL_optnumber(L, i + 6, 0);

  if (r == 0 && sy == 1 && (sx == 1 || sx == -1)) {
    /* Fast path: an unscaled image, optionally mirrored horizontally */
    image_setFlip(sx < 0);
    image_blit(img, buf, bufw, bufh,
               fastFloor(sx < 0 ? x + ox - srcw : x - ox), fastFloor(y - oy),
               srcx, srcy, srcw, srch);
  } else {
    image_drawTransformed(img, buf, bufw, bufh, srcx, srcy, srcw, srch,
                          x, y, r, sx, sy, ox, oy);
  }
  return 0;
}


static int getNumbers(lua_State *L, int idx, double **out) {
  /* Reads all the numbers from argument `idx` onwards -- or from the table at
   * `idx`, whose elements may also be {x, y} tables -- into a scratch buffer
   * and returns how many there are. Numbers are read in pairs */
  static double *buf;
  static int cap;
  int i, n = 0;
  int argc = lua_gettop(L);
  int istable = lua_istable(L, idx);
  int count = istable ? (int) lua_rawlen(L, idx) : argc - idx + 1;

  #define PUSH_NUMBER(v)\
    do {\
      if (n == cap) {\
        cap = cap ? cap * 2 : 64;\
        buf = realloc(buf, cap * sizeof(*buf));\
      }\
      buf[n++] = (v);\
    } while (0)

  for (i = 0; i < count; i++) {
    if (!istable) {
      PUSH_NUMBER(luaL_checknumber(L, idx + i));
      continue;
    }
    lua_rawgeti(L, idx, i + 1);
    if (lua_istable(L, -1)) {
      lua_rawgeti(L, -1, 1);
      lua_rawgeti(L, -2, 2);
      PUSH_NUMBER(luaL_checknumber(L, -2));
      PUSH_NUMBER(luaL_checknumber(L, -1));
      lua_pop(L, 2);
    } else {
      if (!lua_isnumber(L, -1)) {
        luaL_error(L, "expected numbers or {x, y} tables");
      }
      PUSH_NUMBER(lua_tonumber(L, -1));
    }
    lua_pop(L, 1);
  }
  #undef PUSH_NUMBER

  if (n & 1) {
    luaL_error(L, "number of vertex components must be a multiple of two");
  }
  *out = buf;
  return n;
}


int l_graphics_point(lua_State *L) {
  image_rect_t clip = getClip();
  int x = toX(luaL_checknumber(L, 1));
  int y = toY(luaL_checknumber(L, 2));
  plot(&clip, x, y);
  return 0;
}


int l_graphics_points(lua_State *L) {
  /* points(x, y, ...), points({x, y, ...}) or points({{x, y}, ...}) */
  image_rect_t clip = getClip();
  double *v;
  int i, n = getNumbers(L, 1, &v);
  for (i = 0; i < n; i += 2) {
    plot(&clip, toX(v[i]), toY(v[i + 1]));
  }
  return 0;
}


static void drawLine(const image_rect_t *clip, int x0, int y0, int x1, int y1) {
  #define SWAP_INT(a, b) do { int t = (a); (a) = (b); (b) = t; } while (0)
  int steep = abs(y1 - y0) > abs(x1 - x0);
  if (steep) {
    SWAP_INT(x0, y0);
    SWAP_INT(x1, y1);
  }
  if (x0 > x1) {
    SWAP_INT(x0, x1);
    SWAP_INT(y0, y1);
  }
  #undef SWAP_INT
  int deltax = x1 - x0;
  int deltay = abs(y1 - y0);
  int error = deltax / 2;
  int ystep = (y0 < y1) ? 1 : -1;
  int x, y = y0;
  for (x = x0; x < x1; x++) {
    if (steep) {
      plot(clip, y, x);
    } else {
      plot(clip, x, y);
    }
    error -= deltay;
    if (error < 0) {
      y += ystep;
      error += deltax;
    }
  }
}


static void drawPolyline(const image_rect_t *clip, const double *v, int n,
                         int closed) {
  int i;
  for (i = 0; i + 3 < n; i += 2) {
    drawLine(clip, toX(v[i]), toY(v[i + 1]), toX(v[i + 2]), toY(v[i + 3]));
  }
  if (closed && n >= 4) {
    drawLine(clip, toX(v[n - 2]), toY(v[n - 1]), toX(v[0]), toY(v[1]));
  }
}


int l_graphics_line(lua_State *L) {
  /* line(x1, y1, x2, y2, ...) or line({x1, y1, x2, y2, ...}) */
  image_rect_t clip = getClip();
  double *v;
  int n = getNumbers(L, 1, &v);
  if (n < 4) {
    luaL_error(L, "need at least two vertices to draw a line");
  }
  drawPolyline(&clip, v, n, 0);
  return 0;
}


int l_graphics_rectangle(lua_State *L) {
  int fill = checkMode(L, 1);
  double fx = luaL_checknumber(L, 2);
  double fy = luaL_checknumber(L, 3);
  int x = toX(fx);
  int y = toY(fy);
  int x2 = toX(fx + luaL_checknumber(L, 4));
  int y2 = toY(fy + luaL_checknumber(L, 5));
  image_rect_t clip = getClip();
  int i;
  if (x2 < x) { int t = x; x = x2; x2 = t; }
  if (y2 < y) { int t = y; y = y2; y2 = t; }
  if (x == x2 || y == y2) return 0;
  if (fill) {
    /* Clip once, then fill the rows */
    pixel_t *p;
    if (x < clip.x0) x = clip.x0;
    if (y < clip.y0) y = clip.y0;
    if (x2 > clip.x1) x2 = clip.x1;
    if (y2 > clip.y1) y2 = clip.y1;
    if (x >= x2) return 0;
    p = graphics_canvas->data + x + y * graphics_canvas->width;
    for (i = y; i < y2; i++, p += graphics_canvas->width) {
      memset(p, graphics_color, x2 - x);
    }
  } else {
    hline(&clip, x, x2, y);
    hline(&clip, x, x2, y2 - 1);
    for (i = y + 1; i < y2 - 1; i++) {
      plot(&clip, x, i);
      plot(&clip, x2 - 1, i);
    }
  }
  return 0;
}


int l_graphics_circle(lua_State *L) {
  int fill = checkMode(L, 1);
  int x = toX(luaL_checknumber(L, 2));
  int y = toY(luaL_checknumber(L, 3));
  int radius = luaL_checknumber(L, 4);
  image_rect_t clip = getClip();
  int dx = radius, dy = 0;
  int radiusError = 1 - dx;
  while (dx >= dy) {
    if (fill) {
      hline(&clip, x - dx, x + dx, y + dy);
      hline(&clip, x - dx, x + dx, y - dy);
      hline(&clip, x - dy, x + dy, y + dx);
      hline(&clip, x - dy, x + dy, y - dx);
    } else {
      plot(&clip,  dx + x,   dy + y);
      plot(&clip, -dx + x,   dy + y);
      plot(&clip,  dx + x,  -dy + y);
      plot(&clip, -dx + x,  -dy + y);
      plot(&clip,  dy + x,   dx + y);
      plot(&clip, -dy + x,   dx + y);
      plot(&clip,  dy + x,  -dx + y);
      plot(&clip, -dy + x,  -dx + y);
    }
    dy++;
    if (radiusError < 0) {
      radiusError += 2 * dy + 1;
    } else {
      dx--;
      radiusError += 2 * (dy - dx + 1);
    }
  }
  return 0;
}


static void ellipsePoints(const image_rect_t *clip, int cx, int cy,
                          int x, int y, int fill) {
  if (fill) {
    hline(clip, cx - x, cx + x + 1, cy + y);
    if (y != 0) hline(clip, cx - x, cx + x + 1, cy - y);
  } else {
    plot(clip, cx + x, cy + y);
    plot(clip, cx - x, cy + y);
    plot(clip, cx + x, cy - y);
    plot(clip, cx - x, cy - y);
  }
}


int l_graphics_ellipse(lua_State *L) {
  /* Midpoint ellipse algorithm; all decision variables are scaled by 4 so
   * that only integer maths is required */
  int fill = checkMode(L, 1);
  int cx = toX(luaL_checknumber(L, 2));
  int cy = toY(luaL_checknumber(L, 3));
  int rx = abs((int) luaL_checknumber(L, 4));
  int ry = abs((int) luaL_optnumber(L, 5, rx));
  image_rect_t clip = getClip();
  long long a2 = (long long) rx * rx, b2 = (long long) ry * ry;
  long long x = 0, y = ry;
  long long dx = 0, dy = 2 * a2 * y;
  long long d;

  if (rx == 0 || ry == 0) {
    int i;
    for (i = -ry; i <= ry; i++) hline(&clip, cx - rx, cx + rx + 1, cy + i);
    return 0;
  }

  /* Region 1: slope > -1 */
  d = 4 * b2 - 4 * a2 * ry + a2;
  while (dx < dy) {
    if (d < 0) {
      if (!fill) ellipsePoints(&clip, cx, cy, x, y, 0);
      x++;
      dx += 2 * b2;
      d += 4 * (dx + b2);
    } else {
      /* For filling only the widest span of each row is needed */
      ellipsePoints(&clip, cx, cy, x, y, fill);
      x++;
      y--;
      dx += 2 * b2;
      dy -= 2 * a2;
      d += 4 * (dx - dy + b2);
    }
  }

  /* Region 2: slope < -1 */
  d = b2 * (2 * x + 1) * (2 * x + 1) + 4 * a2 * (y - 1) * (y - 1) -
      4 * a2 * b2;
  while (y >= 0) {
    ellipsePoints(&clip, cx, cy, x, y, fill);
    if (d > 0) {
      y--;
      dy -= 2 * a2;
      d += 4 * (a2 - dy);
    } else {
      y--;
      x++;
      dx += 2 * b2;
      dy -= 2 * a2;
      d += 4 * (dx - dy + a2);
    }
  }
  return 0;
}


typedef struct {
  int row, endrow;      /* First and last (exclusive) row the edge covers */
  int x, dxdy;          /* 16.16 fixed point x at `row` and slope */
} edge_t;


static void fillPolygon(const image_rect_t *clip, const double *v, int n) {
  /* Scanline fill using the even-odd rule, sampling pixel centres. Edge
   * positions are stepped in fixed point so the inner loops are integer
   * only */
  static edge_t *edges;
  static int *xs;
  static int cap;
  int i, j, nedges = 0, nverts = n / 2;
  int minrow = INT_MAX, maxrow = INT_MIN;

  if (nverts > cap) {
    cap = nverts;
    edges = realloc(edges, cap * sizeof(*edges));
    xs = realloc(xs, cap * sizeof(*xs));
  }

  for (i = 0; i < nverts; i++) {
    double x0 = v[i * 2] + graphics_transform.x;
    double y0 = v[i * 2 + 1] + graphics_transform.y;
    double x1 = v[((i + 1) % nverts) * 2] + graphics_transform.x;
    double y1 = v[((i + 1) % nverts) * 2 + 1] + graphics_transform.y;
    double slope;
    edge_t *e;
    if (y0 == y1) continue;
    if (y0 > y1) {
      double t;
      t = x0; x0 = x1; x1 = t;
      t = y0; y0 = y1; y1 = t;
    }
    e = &edges[nedges];
    e->row = ceil(y0 - 0.5);
    e->endrow = ceil(y1 - 0.5);
    if (e->row >= e->endrow) continue;
    slope = (x1 - x0) / (y1 - y0);
    e->x = (x0 + (e->row + 0.5 - y0) * slope) * 65536.;
    e->dxdy = slope * 65536.;
    if (e->row < minrow) minrow = e->row;
    if (e->endrow > maxrow) maxrow = e->endrow;
    nedges++;
  }

  if (minrow < clip->y0) minrow = clip->y0;
  if (maxrow > clip->y1) maxrow = clip->y1;

  for (i = minrow; i < maxrow; i++) {
    int count = 0;
    /* Gather the crossings of this row, insertion sorted */
    for (j = 0; j < nedges; j++) {
      edge_t *e = &edges[j];
      if (i >= e->row && i < e->endrow) {
        int x = e->x + (long long) (i - e->row) * e->dxdy;
        int k = count++;
        while (k > 0 && xs[k - 1] > x) {
          xs[k] = xs[k - 1];
          k--;
        }
        xs[k] = x;
      }
    }
    /* Fill pixels whose centres lie between pairs of crossings */
    for (j = 0; j + 1 < count; j += 2) {
      int x0 = (xs[j] - 0x8000 + 0xffff) >> 16;
      int x1 = (xs[j + 1] - 0x8000 + 0xffff) >> 16;
      hline(clip, x0, x1, i);
    }
  }
}


int l_graphics_polygon(lua_State *L) {
  /* polygon(mode, x1, y1, x2, y2, ...) or polygon(mode, {x1, y1, ...}) */
  int fill = checkMode(L, 1);
  image_rect_t clip = getClip();
  double *v;
  int n = getNumbers(L, 2, &v);
  if (n < 6) {
    luaL_error(L, "need at least three vertices to draw a polygon");
  }
  if (fill) {
    fillPolygon(&clip, v, n);
  } else {
    drawPolyline(&clip, v, n, 1);
  }
  return 0;
}


int l_graphics_print(lua_State *L) {
  luaL_checkany(L, 1);
  /* Read the arguments before luaL_tolstring() pushes onto the stack */
  int x = toX(luaL_optnumber(L, 2, 0));
  int y = toY(luaL_optnumber(L, 3, 0));
  size_t len;
  const char *str = luaL_tolstring(L, 1, &len);
  font_blit(graphics_font, graphics_canvas->data, graphics_canvas->width,
            graphics_canvas->height, str, len, x, y);
  return 0;
}


enum { ALIGN_LEFT, ALIGN_CENTER, ALIGN_RIGHT, ALIGN_JUSTIFY };

typedef struct {
  int x, y, limit, align;
} printf_t;


static void printfLine(void *udata, const char *str, int len, int width,
                       int last) {
  printf_t *p = udata;
  font_t *font = graphics_font;
  pixel_t *buf = graphics_canvas->data;
  int bufw = graphics_canvas->width;
  int bufh = graphics_canvas->height;
  int x = p->x;

  if (p->align == ALIGN_JUSTIFY && !last) {
    /* Spread the remaining space over the gaps between words */
    const char *s = str, *end = str + len, *q;
    int gaps = 0, extra = p->limit - width, i = 0;
    for (q = str; q < end; q++) {
      if (*q == ' ' && q > str && q[-1] != ' ') gaps++;
    }
    while (s < end) {
      for (q = s; q < end && *q != ' '; q++);
      font_blit(font, buf, bufw, bufh, s, q - s, x, p->y);
      x += font_getWidth(font, s, q - s);
      for (s = q; q < end && *q == ' '; q++);
      x += font_getWidth(font, s, q - s);
      if (q < end && s > str && gaps > 0) {
        x += extra / gaps + (i++ < extra % gaps);
      }
      s = q;
    }
  } else {
    switch (p->align) {
      case ALIGN_CENTER : x += (p->limit - width) / 2; break;
      case ALIGN_RIGHT  : x += p->limit - width;       break;
    }
    font_blit(font, buf, bufw, bufh, str, len, x, p->y);
  }
  p->y += font->height;
}


int l_graphics_printf(lua_State *L) {
  /* printf(text, x, y, limit [, align]) */
  static const char *aligns[] = { "left", "center", "right", "justify", NULL };
  printf_t p;
  luaL_checkany(L, 1);
  /* Read the arguments before luaL_tolstring() pushes onto the stack */
  p.x = toX(luaL_checknumber(L, 2));
  p.y = toY(luaL_checknumber(L, 3));
  p.limit = luaL_checknumber(L, 4);
  p.align = luaL_checkoption(L, 5, "left", aligns);
  const char *str = luaL_tolstring(L, 1, NULL);
  font_wrap(graphics_font, str, p.limit, printfLine, &p);
  return 0;
}


int l_graphics_push(lua_State *L) {
  static const char *types[] = { "transform", NULL };
  luaL_checkoption(L, 1, "transform", types);
  if (graphics_transformStackSize == TRANSFORM_STACK_MAX) {
    luaL_error(L, "maximum stack depth reached (more pushes than pops?)");
  }
  graphics_transformStack[graphics_transformStackSize++] = graphics_transform;
  return 0;
}


int l_graphics_pop(lua_State *L) {
  if (graphics_transformStackSize == 0) {
    luaL_error(L, "minimum stack depth reached (more pops than pushes?)");
  }
  graphics_transform = graphics_transformStack[--graphics_transformStackSize];
  return 0;
}


int l_graphics_origin(lua_State *L) {
  graphics_transform.x = 0;
  graphics_transform.y = 0;
  return 0;
}


int l_graphics_translate(lua_State *L) {
  graphics_transform.x += luaL_checknumber(L, 1);
  graphics_transform.y += luaL_checknumber(L, 2);
  return 0;
}


int l_graphics_setScissor(lua_State *L) {
  /* setScissor(x, y, width, height), or setScissor() to disable it. The
   * rectangle is in screen coordinates and unaffected by translate() */
  if (lua_isnoneornil(L, 1)) {
    graphics_scissor = 0;
    image_setClip(INT_MIN, INT_MIN, INT_MAX, INT_MAX);
    return 0;
  }
  int x = luaL_checknumber(L, 1);
  int y = luaL_checknumber(L, 2);
  int w = luaL_checknumber(L, 3);
  int h = luaL_checknumber(L, 4);
  if (w < 0 || h < 0) {
    luaL_error(L, "scissor width and height must not be negative");
  }
  graphics_scissor = 1;
  graphics_scissorRect[0] = x;
  graphics_scissorRect[1] = y;
  graphics_scissorRect[2] = w;
  graphics_scissorRect[3] = h;
  image_setClip(x, y, x + w, y + h);
  return 0;
}


int l_graphics_getScissor(lua_State *L) {
  int i;
  if (!graphics_scissor) return 0;
  for (i = 0; i < 4; i++) {
    lua_pushinteger(L, graphics_scissorRect[i]);
  }
  return 4;
}



int l_image_new(lua_State *L);
int l_image_newCanvas(lua_State *L);
int l_quad_new(lua_State *L);
int l_font_new(lua_State *L);

int luaopen_graphics(lua_State *L) {
  luaL_Reg reg[] = {
    { "getDimensions",      l_graphics_getDimensions      },
    { "getWidth",           l_graphics_getWidth           },
    { "getHeight",          l_graphics_getHeight          },
    { "getBackgroundColor", l_graphics_getBackgroundColor },
    { "setBackgroundColor", l_graphics_setBackgroundColor },
    { "getColor",           l_graphics_getColor           },
    { "setColor",           l_graphics_setColor           },
    { "getBlendMode",       l_graphics_getBlendMode       },
    { "setBlendMode",       l_graphics_setBlendMode       },
    { "getFont",            l_graphics_getFont            },
    { "setFont",            l_graphics_setFont            },
    { "getCanvas",          l_graphics_getCanvas          },
    { "setCanvas",          l_graphics_setCanvas          },
    { "reset",              l_graphics_reset              },
    { "clear",              l_graphics_clear              },
    { "present",            l_graphics_present            },
    { "draw",               l_graphics_draw               },
    { "point",              l_graphics_point              },
    { "line",               l_graphics_line               },
    { "rectangle",          l_graphics_rectangle          },
    { "circle",             l_graphics_circle             },
    { "print",              l_graphics_print              },
    { "printf",             l_graphics_printf             },
    { "points",             l_graphics_points             },
    { "polygon",            l_graphics_polygon            },
    { "ellipse",            l_graphics_ellipse            },
    { "push",               l_graphics_push               },
    { "pop",                l_graphics_pop                },
    { "origin",             l_graphics_origin             },
    { "translate",          l_graphics_translate          },
    { "setScissor",         l_graphics_setScissor         },
    { "getScissor",         l_graphics_getScissor         },
    { "newImage",           l_image_new                   },
    { "newCanvas",          l_image_newCanvas             },
    { "newQuad",            l_quad_new                    },
    { "newFont",            l_font_new                    },
    { 0, 0 },
  };
  luaL_newlib(L, reg);

  /* Init screen canvas */
  lua_pushcfunction(L, l_image_newCanvas);
  lua_pushinteger(L, VGA_WIDTH);
  lua_pushinteger(L, VGA_HEIGHT);
  lua_call(L, 2, 1);
  graphics_screen = luaobj_checkudata(L, -1, LUAOBJ_TYPE_IMAGE);
  /* Add screen canvas to registry */
  lua_pushlightuserdata(L, &graphics_screen);
  lua_pushvalue(L, -2);
  lua_settable(L, LUA_REGISTRYINDEX);
  lua_pop(L, 1); /* Pop the Image object */

  /* Init default font */
  lua_pushcfunction(L, l_font_new);
  lua_call(L, 0, 1);
  graphics_defaultFont = luaobj_checkudata(L, -1, LUAOBJ_TYPE_FONT);
  /* Add default font to registry */
  lua_pushlightuserdata(L, &graphics_defaultFont);
  lua_pushvalue(L, -2);
  lua_settable(L, LUA_REGISTRYINDEX);
  lua_pop(L, 1); /* Pop the Font object */

  /* Reset all state settings to their defaults */
  lua_pushcfunction(L, l_graphics_reset);
  lua_call(L, 0, 0);

  return 1;
}
