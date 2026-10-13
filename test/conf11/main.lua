local t = require("testlib")
local g = love.graphics

function love.load()
  t.check("audio module disabled", love.audio == nil)
  t.check("mouse module disabled", love.mouse == nil)
  t.check("other modules kept", love.graphics ~= nil and love.sound ~= nil)
  t.check("conf identity", love.filesystem.getIdentity() == "My Game!")

  -- colors are 0..1
  local r, gg, b, a = g.getColor()
  t.check("default color 1,1,1,1", r == 1 and gg == 1 and b == 1 and a == 1)
  g.setColor(0.5, 0.25, 1)
  r, gg, b, a = g.getColor()
  t.check("setColor 0..1", t.near(r, 128 / 255) and t.near(gg, 64 / 255) and b == 1 and a == 1)
  g.setColor({ 1, 0, 0, 0.5 })
  r, gg, b = g.getColor()
  t.check("setColor table with alpha", r == 1 and gg == 0 and b == 0)
  t.check("setColor bad table", t.errors(g.setColor, { 1, "x" }))
  g.setBackgroundColor(0, 0, 1)
  r, gg, b = g.getBackgroundColor()
  t.check("background 0..1", r == 0 and gg == 0 and b == 1)
  g.setColor(2, -1, 0.5)
  r, gg, b = g.getColor()
  t.check("out of range clamped", r == 1 and gg == 0)

  local c = g.newCanvas(4, 4)
  c:setPixel(1, 1, 0, 1, 0)
  r, gg, b, a = c:getPixel(1, 1)
  t.check("Image pixels 0..1", r == 0 and gg == 1 and b == 0 and a == 1)
  c:setPixel(2, 2, { 1, 1, 0 })
  r, gg, b = c:getPixel(2, 2)
  t.check("setPixel table", r == 1 and gg == 1 and b == 0)
  g.setCanvas(c) g.setColor(1, 0, 1) g.rectangle("fill", 0, 0, 1, 1) g.setCanvas()
  r, gg, b = c:getPixel(0, 0)
  t.check("drawing uses 0..1 colors", r == 1 and gg == 0 and b == 1)
  g.clear(0.2, 0.4, 0.6)

  -- save directory follows the identity
  love.filesystem.write("hello.txt", "hi")
  local f = io.open("save/mygame/hello.txt", "rb")
  t.check("identity save dir", f ~= nil and f:read("*a") == "hi")
  if f then f:close() end
  t.check("read back via love.filesystem", love.filesystem.read("hello.txt") == "hi")
  love.filesystem.setIdentity("other")
  love.filesystem.write("b.txt", "b")
  f = io.open("save/other/b.txt", "rb")
  t.check("setIdentity at runtime", f ~= nil and love.filesystem.getIdentity() == "other")
  if f then f:close() end

  t.check("isVersionCompatible", love.isVersionCompatible("11.3") and love.isVersionCompatible(0, 10, 2)
          and not love.isVersionCompatible("0.9.2") and not love.isVersionCompatible(12, 0))
  t.check("deprecation output", love.hasDeprecationOutput() == true)
  love.setDeprecationOutput(false)
  t.check("set deprecation output", love.hasDeprecationOutput() == false)
  t.check("internal hooks removed", love.graphics._setColorRange == nil)
  t.check("timer.step returns dt", type(love.timer.step()) == "number")
  t.finish()
end
