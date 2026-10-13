-- Shared helpers for the test games (copied into each test's directory)
local t = {}
local log = io.open("log.txt", "w")
local fails = 0

function t.log(...)
  local s = {}
  for i = 1, select("#", ...) do s[i] = tostring((select(i, ...))) end
  log:write(table.concat(s, " "), "\n")
  log:flush()
end

function t.check(name, cond)
  if not cond then fails = fails + 1 end
  t.log(cond and "ok  " or "FAIL", name)
end

function t.errors(f, ...)
  return not pcall(f, ...)
end

function t.near(a, b, eps)
  return math.abs(a - b) <= (eps or 1e-6)
end

function t.finish()
  t.log("fails", fails)
  log:close()
  love.event.quit()
end

-- Log errors and exit instead of waiting for a key on the error screen
function love.errorhandler(msg)
  t.log("ERROR", msg, debug.traceback("", 2))
  t.log("fails", "error")
  log:close()
  return function() return 1 end
end

return t
