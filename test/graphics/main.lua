local t0 = require("testlib")
local wavgen = require("wavgen")
local out, check, errs = t0.log, t0.check, t0.errors
local t = t0
local g = love.graphics
local W = {255, 255, 255}
local function set(c, x, y) local r, gg, b = c:getPixel(x, y) return r ~= nil and (r + gg + b) > 0 end
local function color(c, x, y) local r, gg, b = c:getPixel(x, y) return r and (r .. "," .. gg .. "," .. b) end
local function count(c) local n = 0 for y = 0, c:getHeight() - 1 do for x = 0, c:getWidth() - 1 do if set(c, x, y) then n = n + 1 end end end return n end

function love.load()
  local c = g.newCanvas(64, 64)
  local function fresh() g.setCanvas(c) g.origin() g.setScissor() g.clear(0, 0, 0) g.setColor(255, 255, 255) end
  -- note: palette index 0 is transparent in canvases, so black reads as nil

  check("blend mode bad name", errs(g.setBlendMode, "nonsense"))
  check("blend mode ok", not errs(g.setBlendMode, "fast") and g.getBlendMode() == "fast")
  g.setBlendMode()

  fresh()
  g.push() g.translate(10, 5) g.rectangle("fill", 0, 0, 2, 2) g.push() g.translate(1, 1) g.point(0, 0) g.pop() g.point(0, 0) g.pop()
  g.point(0, 0)
  check("translate rect", set(c, 10, 5) and set(c, 11, 6) and not set(c, 12, 5) and not set(c, 9, 5))
  check("nested translate point", set(c, 11, 6))
  check("pop restores", set(c, 0, 0))
  check("pop underflow", errs(g.pop))
  for i = 1, 64 do g.push() end
  check("push overflow", errs(g.push))
  g.origin()
  for i = 1, 64 do g.pop() end

  fresh()
  g.setScissor(2, 3, 5, 4)
  check("getScissor", select("#", g.getScissor()) == 4 and select(3, g.getScissor()) == 5)
  g.rectangle("fill", 0, 0, 20, 20)
  check("scissor clips rect", set(c, 2, 3) and set(c, 6, 6) and not set(c, 7, 6) and not set(c, 6, 7) and not set(c, 1, 3))
  check("scissor count", count(c) == 20)
  g.setScissor(3, 4, 1, 1) g.clear(0, 0, 0)
  check("scissored clear", not set(c, 3, 4) and set(c, 4, 4))
  g.setScissor()
  check("scissor off", g.getScissor() == nil)
  fresh()
  g.setScissor(0, 0, 4, 64)
  g.print("WWWW", 0, 0)
  local ok = true for y = 0, 63 do for x = 4, 63 do if set(c, x, y) then ok = false end end end
  check("scissor clips text", ok)

  fresh()
  g.polygon("fill", 0, 0, 10, 0, 0, 10)
  local n = count(c)
  check("triangle area ~50 (" .. n .. ")", n >= 45 and n <= 55 and set(c, 1, 1) and not set(c, 9, 9))
  fresh()
  -- concave U shape: outer 0..30, notch x 10..20 from y 0..20
  g.polygon("fill", {0, 0, 10, 0, 10, 20, 20, 20, 20, 0, 30, 0, 30, 30, 0, 30})
  check("concave polygon", set(c, 5, 5) and not set(c, 15, 5) and set(c, 15, 25) and set(c, 25, 5) and count(c) == 700)
  fresh()
  g.polygon("fill", {{0, 0}, {4, 0}, {4, 4}, {0, 4}})
  check("polygon pairs", count(c) == 16)
  check("polygon too few", errs(g.polygon, "fill", 0, 0, 1, 1))
  check("polygon odd", errs(g.polygon, "fill", 0, 0, 1, 1, 2))
  fresh()
  g.polygon("line", 0, 0, 10, 0, 10, 10, 0, 10)
  check("polygon line", set(c, 5, 0) and set(c, 10, 5) and set(c, 0, 5) and not set(c, 5, 5))

  fresh()
  g.ellipse("fill", 32, 32, 20, 10)
  check("ellipse extents", set(c, 52, 32) and not set(c, 53, 32) and set(c, 12, 32) and not set(c, 11, 32) and set(c, 32, 42) and not set(c, 32, 43) and set(c, 32, 22) and not set(c, 32, 21))
  check("ellipse inside", set(c, 32, 32) and set(c, 45, 36) and not set(c, 50, 40))
  local n = count(c)
  check("ellipse area ~628 (" .. n .. ")", n > 600 and n < 680)
  fresh()
  g.ellipse("line", 32, 32, 20, 10)
  check("ellipse line hollow", set(c, 52, 32) and set(c, 32, 42) and not set(c, 32, 32))
  fresh()
  g.ellipse("fill", 32, 32, 5)
  check("ellipse one radius = circle", set(c, 37, 32) and set(c, 32, 37) and not set(c, 38, 32))

  fresh()
  g.points(1, 1, 2, 2) g.points({3, 3, 4, 4}) g.points({{5, 5}, {6, 6}})
  check("points", count(c) == 6 and set(c, 6, 6))
  fresh()
  g.line({0, 0, 10, 0, 10, 10})
  check("line table", set(c, 5, 0) and set(c, 10, 5))

  -- images: a 4x1 image with 4 colours
  local img = g.newCanvas(4, 1)
  local cols = {{255, 0, 0}, {0, 255, 0}, {0, 0, 255}, {255, 255, 0}}
  for i = 1, 4 do img:setPixel(i - 1, 0, unpack(cols[i])) end
  local function is(x, y, i) return color(c, x, y) == table.concat(cols[i], ",") end
  fresh() g.draw(img, 10, 0)
  check("draw plain", is(10, 0, 1) and is(13, 0, 4))
  fresh() g.draw(img, 10, 0, 0, -1, 1)
  check("draw sx=-1 mirrors around x", is(9, 0, 1) and is(6, 0, 4) and not set(c, 10, 0))
  fresh() g.draw(img, 10, 0, 0, -1, 1, 4, 0)
  check("draw sx=-1 with ox=w stays in place", is(13, 0, 1) and is(10, 0, 4))
  fresh() g.draw(img, 10, 0, true)
  check("legacy flip", is(10, 0, 4) and is(13, 0, 1))
  fresh() g.draw(img, 10, 2, 0, 2, 3)
  check("draw scale 2x3", is(10, 2, 1) and is(11, 4, 1) and is(16, 2, 4) and is(17, 4, 4) and not set(c, 18, 2) and not set(c, 10, 5) and count(c) == 24)
  fresh() g.draw(img, 10, 10, math.pi / 2)
  check("draw rotate 90", is(9, 10, 1) and is(9, 13, 4) and count(c) == 4)
  fresh() g.draw(img, 10, 10, math.pi, 1, 1, 2, 0)
  check("draw rotate 180 around origin", is(11, 9, 1) and is(8, 9, 4) and count(c) == 4)
  fresh() g.draw(img, 10, 10, 0, 2, 2, 2, 0)
  check("draw scale with origin", is(6, 10, 1) and is(13, 11, 4))
  fresh() g.draw(img, g.newQuad(-2, 0, 4, 1), 10, 0)
  check("quad left of image", not set(c, 10, 0) and not set(c, 11, 0) and is(12, 0, 1) and is(13, 0, 2))
  fresh() g.draw(img, g.newQuad(2, 0, 4, 1), 10, 0)
  check("quad right of image", is(10, 0, 3) and is(11, 0, 4) and count(c) == 2)
  fresh() g.translate(5, 5) g.draw(img, 0, 0) g.origin()
  check("draw translated", is(5, 5, 1))
  fresh() g.setScissor(11, 0, 2, 64) g.draw(img, 10, 0, 0, 3, 1)
  check("transformed draw scissored", count(c) == 2)

  -- text
  local f = g.getFont()
  local w = f:getWidth("aaa bbb")
  local width, lines = f:getWrap("aaa bbb ccc", w)
  check("getWrap", #lines == 2 and lines[1] == "aaa bbb" and lines[2] == "ccc" and width == w)
  local width, lines = f:getWrap("a\n\nb", 100)
  check("getWrap newlines", #lines == 3 and lines[2] == "")
  check("getWidth multiline", f:getWidth("ab\na") == f:getWidth("ab"))
  fresh() g.printf("W W", 0, 0, 64) g.print("a") g.print(1, 2)
  fresh() g.printf("W", 0, 0, 64, "right")
  local minx = 64 for y = 0, 63 do for x = 0, 63 do if set(c, x, y) and x < minx then minx = x end end end
  check("printf right", minx >= 64 - f:getWidth("W") and minx < 64)
  fresh() g.printf("W", 0, 0, 64, "center")
  local minx = 64 for y = 0, 63 do for x = 0, 63 do if set(c, x, y) and x < minx then minx = x end end end
  check("printf center", math.abs(minx - (64 - f:getWidth("W")) / 2) <= 2)
  check("printf bad align", errs(g.printf, "x", 0, 0, 10, "middle"))

  g.setCanvas()
  t0.finish()
end
