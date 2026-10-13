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


-- Save directory: "save" or, if an identity is set, "save/<identity>" with
-- the identity shortened to a valid DOS directory name
local identity
local saveDir = "save"
local saveDirMounted = false

local function setSaveDir(dir)
  if saveDirMounted then
    love.filesystem.unmount(saveDir)
    saveDirMounted = false
  end
  saveDir = dir
  -- If the directory doesn't exist yet it is only created (and mounted) when
  -- something is written to it
  if love.filesystem.mount(saveDir) then
    saveDirMounted = true
    love.filesystem.setWriteDir(saveDir)
  end
end

local function ensureSaveDir()
  if not saveDirMounted then
    love.filesystem.setWriteDir(saveDir)
    saveDirMounted = love.filesystem.mount(saveDir) and true or false
  end
end


function love.filesystem.setIdentity(name)
  if type(name) ~= "string" then
    error("bad argument #1 to 'setIdentity' (string expected)", 2)
  end
  identity = name
  local dosname = name:lower():gsub("[^%w_%-]", ""):sub(1, 8)
  setSaveDir(dosname ~= "" and ("save/" .. dosname) or "save")
end


function love.filesystem.getIdentity()
  return identity
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

  -- Init event handlers table
  local function makewrapper(name)
    return function(...)
      if love[name] then return love[name](...) end
    end
  end
  love.handlers = {
    ["quit"] = function() end,
    ["mousepressed"] = makewrapper("mousepressed"),
    ["mousereleased"] = makewrapper("mousereleased"),
    ["mousemoved"] = makewrapper("mousemoved"),
    ["keypressed"] = makewrapper("keypressed"),
    ["keyreleased"] = makewrapper("keyreleased"),
    ["textinput"] = makewrapper("textinput"),
   }

  -- Try to mount .exe file, then the first argument
  for i, v in ipairs { love.argv[1], love.argv[2] } do
    local mounted = love.filesystem.mount(v)
    if mounted then
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

  -- LÖVE 11 uses colors in the range 0 to 1 instead of 0 to 255
  if majorVersion(conf.version) >= 11 then
    love.graphics._setColorRange(1)
  end
  love.graphics._setColorRange = nil

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
  end

  -- Init the save directory. love.filesystem.write() is wrapped so that the
  -- directory is only created when something is written to it
  if conf.identity then
    love.filesystem.setIdentity(conf.identity)
  else
    setSaveDir(saveDir)
  end
  local write = love.filesystem.write
  love.filesystem.write = function(...)
    ensureSaveDir()
    return write(...)
  end

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
    while 1 do
      local name, a,b,c,d = love.event.poll()
      if not name then
        break
      end
      if name == "quit" then
        if not love.quit or not love.quit() then
          os.exit(a)
        end
      end
      love.handlers[name](a, b, c, d)
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
    repeat
      local name, a = love.event.poll()
      if name == "keypressed" and a == "escape" then
        return
      end
    until not name
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
