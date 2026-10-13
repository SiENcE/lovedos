local t = require("testlib")
local got = {}

function love.handlers.custom(a, b, c, d)
  got[#got + 1] = { a, b, c, d }
end

function love.load()
  love.event.push("custom", 1, nil, "three")
  love.event.push("custom", "x")
  local n = 0
  for name, a, b, c in love.event.poll() do
    n = n + 1
    if n == 1 then t.check("poll order and nil args", name == "custom" and a == 1 and b == nil and c == "three") end
  end
  t.check("poll iterator drains queue", n == 2 and love.event.poll()() == nil)
  love.event.push("custom", "cleared")
  love.event.clear()
  t.check("clear", love.event.poll()() == nil)
  love.event.push("custom", "w")
  local name, a = love.event.wait()
  t.check("wait", name == "custom" and a == "w")
  love.event.push("custom", "via run", 2)
  t.check("push requires a name", t.errors(love.event.push, 42))
  t.check("unknown event errors", t.errors(function() return love.handlers.nonexistent end))
end

local frames = 0
function love.update()
  frames = frames + 1
  if frames == 2 then
    t.check("custom event dispatched by love.run", #got == 1 and got[1][1] == "via run" and got[1][2] == 2)
    t.finish()
  end
end
