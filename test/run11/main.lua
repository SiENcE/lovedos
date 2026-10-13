-- A LÖVE 11 style love.run, returning a function called once per frame
local t = require("testlib")
local frames = 0

function love.run()
  if love.load then love.load() end
  return function()
    love.event.pump()
    for name, a in love.event.poll do
      if name == "quit" then return a or 0 end
    end
    frames = frames + 1
    love.graphics.origin()
    love.graphics.clear(love.graphics.getBackgroundColor())
    love.graphics.present()
    if frames == 3 then
      t.check("loop function called each frame", true)
      t.finish()
    end
  end
end
