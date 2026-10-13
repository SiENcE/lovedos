--
-- Copyright (c) 2017 rxi
--
-- This library is free software; you can redistribute it and/or modify it
-- under the terms of the MIT license. See LICENSE for details.
--
-- The event queue: events from the keyboard and mouse (queued in C) are moved
-- into a Lua queue, which can also hold events pushed with love.event.push()
-- and events from polled devices such as joysticks
--

local ev = love.event
local cpump, cpoll = ev.pump, ev.poll
local queue = {}
local hooks = {}

local function pack(...)
  return { n = select("#", ...), ... }
end

local function pop()
  local e = table.remove(queue, 1)
  if e then
    return unpack(e, 1, e.n)
  end
end


function ev._addPumpHook(f)
  -- Internal: `f` is called on each pump, eg. to poll a device
  hooks[#hooks + 1] = f
end


function ev.pump()
  cpump()
  while true do
    local e = pack(cpoll())
    if e.n == 0 then break end
    queue[#queue + 1] = e
  end
  for i = 1, #hooks do hooks[i]() end
end


function ev.push(name, ...)
  if type(name) ~= "string" then
    error("bad argument #1 to 'push' (string expected)", 2)
  end
  queue[#queue + 1] = pack(name, ...)
  return true
end


function ev.poll()
  -- Returns an iterator over the queued events, as in LÖVE:
  -- for name, a, b, c, d, e, f in love.event.poll() do ... end
  return pop
end


function ev.wait()
  while #queue == 0 do
    ev.pump()
  end
  return pop()
end


function ev.clear()
  cpump()
  while cpoll() do end
  queue = {}
end


function ev.quit(status)
  return ev.push("quit", status or 0)
end
