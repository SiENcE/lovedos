-- love.errorhandler (LÖVE 11) is used instead of love.errhand and may return
-- a function which is called until it returns a value
local t = require("testlib")
local calls = 0

function love.errorhandler(msg)
  t.check("error message passed", tostring(msg):find("boom") ~= nil)
  return function()
    calls = calls + 1
    if calls == 3 then
      t.check("error loop function called", true)
      t.finish()
      return 1
    end
  end
end

function love.load()
  error("boom")
end
