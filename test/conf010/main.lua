local t = require("testlib")
local g = love.graphics
local frames = 0

function love.load()
  local r, gg, b, a = g.getColor()
  t.check("default color 255", r == 255 and gg == 255 and b == 255 and a == 255)
  g.setColor(128, 64, 255)
  r, gg, b = g.getColor()
  t.check("setColor 0..255", r == 128 and gg == 64 and b == 255)
  g.setColor({ 10, 20, 30 })
  r, gg, b = g.getColor()
  t.check("setColor table 0..255", r == 10 and gg == 20 and b == 30)
  t.check("audio module present", love.audio ~= nil and love.audio._init == nil)
  t.check("no identity", love.filesystem.getIdentity() == nil)
  love.filesystem.write("x.txt", "x")
  local f = io.open("save/x.txt", "rb")
  t.check("default save dir", f ~= nil)
  if f then f:close() end
end

function love.update()
  frames = frames + 1
  -- love.run resets the transformation every frame
  if frames == 1 then
    love.graphics.translate(10, 10)
  elseif frames == 2 then
    local c = g.newCanvas(30, 30)
    g.setColor(255, 255, 255) g.setCanvas(c) g.point(0, 0) g.setCanvas()
    t.check("origin reset each frame", c:getPixel(0, 0) == 255)
    t.finish()
  end
end
