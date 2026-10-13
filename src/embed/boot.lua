--
-- Copyright (c) 2017 rxi
--
-- This library is free software; you can redistribute it and/or modify it
-- under the terms of the MIT license. See LICENSE for details.
--

local function defaultConf()
  -- The same fields and defaults as LÖVE's `t` table; most of them have no
  -- effect on DOS but are accepted so that conf.lua files work unchanged
  return {
    identity = nil,
    appendidentity = false,
    version = nil,
    console = false,
    accelerometerjoystick = true,
    externalstorage = false,
    gammacorrect = false,
    audio = { mic = false, mixwithsystem = true },
    window = {
      title = "Untitled", icon = nil, width = 320, height = 200,
      borderless = false, resizable = false, minwidth = 1, minheight = 1,
      fullscreen = true, fullscreentype = "exclusive", vsync = 0, msaa = 0,
      depth = nil, stencil = nil, display = 1, highdpi = false,
      usedpiscale = true, x = nil, y = nil,
    },
    modules = {
      audio = true, data = true, event = true, font = true, graphics = true,
      image = true, joystick = true, keyboard = true, math = true,
      mouse = true, physics = true, sound = true, system = true,
      thread = true, timer = true, touch = true, video = true, window = true,
    },
  }
end


local function majorVersion(version)
  return tonumber(type(version) == "string" and version:match("^(%d+)%.")) or 0
end


function love.boot()
  -- Init package.path and add filesystem-compatible package loader
  package.path = "?.lua;?/init.lua"
  table.insert(package.loaders, 1, function(modname)
    modname = modname:gsub("%.", "/")
    for x in package.path:gmatch("[^;]+") do
      local file = x:gsub("?", modname)
      if love.filesystem.exists(file) then
        return assert(loadstring(love.filesystem.read(file), "=" .. file))
      end
    end
  end)

  -- Init event handlers table; like in LÖVE an unknown event is an error
  local function makewrapper(name)
    return function(...)
      if love[name] then return love[name](...) end
    end
  end
  love.handlers = setmetatable({ quit = function() end }, {
    __index = function(t, name) error("Unknown event: " .. tostring(name)) end,
  })
  for _, name in ipairs {
    "mousepressed", "mousereleased", "mousemoved", "wheelmoved",
    "keypressed", "keyreleased", "textinput", "textedited",
    "joystickpressed", "joystickreleased", "joystickaxis", "joystickhat",
    "joystickadded", "joystickremoved", "gamepadpressed", "gamepadreleased",
    "gamepadaxis", "focus", "mousefocus", "visible", "resize", "lowmemory",
    "filedropped", "directorydropped", "threaderror", "displayrotated",
    "touchpressed", "touchreleased", "touchmoved",
  } do
    love.handlers[name] = makewrapper(name)
  end

  -- Try to mount .exe file (a packaged game), then the first argument
  for i, v in ipairs { love.argv[1], love.argv[2] } do
    local mounted = love.filesystem.mount(v)
    if mounted then
      love.filesystem._setSource(v, i == 1)
      break
    end
  end

  -- Run conf.lua
  local conf = defaultConf()
  if love.filesystem.isFile("conf.lua") then
    require("conf")
    if love.conf then
      love.conf(conf)
    end
  end

  -- LÖVE 11 uses colors in the range 0 to 1 instead of 0 to 255, and some
  -- ids start at 1 instead of 0
  love._setCompatibility(majorVersion(conf.version) >= 11 and 11 or 10)
  love._setCompatibility = nil

  -- Remove disabled modules; the Sound Blaster is only initialized if the
  -- audio module is used
  for name, enabled in pairs(conf.modules) do
    if enabled == false then
      love[name] = nil
    end
  end
  if love.audio then
    love.audio._init()
    love.audio._init = nil
    if love.event then
      love.event._addPumpHook(love.audio._update)
    end
  end
  if love.joystick then
    love.joystick._init()
  end

  -- Init the save directory
  love.filesystem._init(conf.identity)

  -- Load main.lua or init `nogame` state
  if love.filesystem.isFile("main.lua") then
    require("main")
  else
    love.nogame()
  end

  -- LÖVE 11's love.run returns a function which is called once per frame
  -- until it returns an exit status; LoveDOS' default love.run loops itself
  local loop = love.run()
  if type(loop) == "function" then
    while true do
      local status = loop()
      if status ~= nil then
        os.exit(type(status) == "number" and status or 0)
      end
    end
  end
end


function love.run()
  -- Prepare arguments
  local args = {}
  for i = 2, #love.argv do
    args[i - 1] = love.argv[i]
  end

  -- Do load callback
  if love.load then love.load(args) end
  if love.timer then love.timer.step() end

  while true do
    -- Handle events
    love.event.pump()
    for name, a, b, c, d, e, f in love.event.poll() do
      if name == "quit" then
        if not love.quit or not love.quit() then
          os.exit(a)
        end
      end
      love.handlers[name](a, b, c, d, e, f)
    end
    -- Update
    local dt = 0
    if love.timer then
      love.timer.step()
      dt = love.timer.getDelta()
    end
    if love.update then love.update(dt) end
    -- Draw
    if love.graphics then
      love.graphics.origin()
      love.graphics.clear()
      if love.draw then love.draw() end
      love.graphics.present()
    end
  end
end


function love.errhand(msg)
  -- Init error text
  local err = { "Error\n", msg }
  local trace = debug.traceback("", 2)
  for line in string.gmatch(trace, "([^\t]-)\n") do
    table.insert(err, line)
  end
  local str = table.concat(err, "\n")
  if not love.graphics or not love.event then
    print(str)
    return
  end

  -- Init error state; the alpha returned by getColor() is the maximum of
  -- the color range (255, or 1 for LÖVE 11 games)
  love.graphics.reset()
  local m = select(4, love.graphics.getColor()) / 255
  pcall(love.graphics.setBackgroundColor, 89 * m, 157 * m, 220 * m)

  -- Do error main loop
  while true do
    love.event.pump()
    for name, a in love.event.poll() do
      if name == "keypressed" and a == "escape" then
        return
      end
    end
    love.graphics.clear()
    love.graphics.print(str, 6, 6)
    love.graphics.present()
  end
end


local function errorhandler(msg)
  -- LÖVE 11 calls love.errorhandler (which may return a function to be called
  -- each frame until it returns a value), older versions love.errhand
  local handler = love.errorhandler or love.errhand
  local loop = handler(msg)
  if type(loop) == "function" then
    while loop() == nil do end
  end
end


xpcall(love.boot, errorhandler)
