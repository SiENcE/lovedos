local t = require("testlib")
function love.load()
  t.check("no joysticks", love.joystick.getJoystickCount() == 0 and #love.joystick.getJoysticks() == 0)
  t.finish()
end
