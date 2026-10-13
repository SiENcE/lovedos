local t = require("testlib")
local added = {}
function love.joystickadded(j) added[#added + 1] = j end

local frames = 0
function love.update()
  frames = frames + 1
  if frames < 3 then return end
  local js = love.joystick.getJoysticks()
  t.log("count", #js)
  t.check("joysticks detected", love.joystick.getJoystickCount() >= 1 and #js == love.joystick.getJoystickCount())
  t.check("joystickadded callbacks", #added == #js)
  local j = js[1]
  if j then
    t.check("joystick info", j:getName() == "Gameport joystick 1" and j:getAxisCount() == 2 and j:getButtonCount() >= 2
            and j:isConnected() and not j:isGamepad() and j:getHatCount() == 0)
    local x, y = j:getAxes()
    t.log("axes", x, y)
    t.check("centered axes", x == 0 and y == 0 and j:getAxis(1) == 0)
    t.check("buttons up", not j:isDown(1, 2))
    t.check("type", j:type() == "Joystick" and j:typeOf("Object"))
  end
  t.finish()
end
