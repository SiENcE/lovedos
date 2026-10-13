--
-- Copyright (c) 2017 rxi
--
-- This library is free software; you can redistribute it and/or modify it
-- under the terms of the MIT license. See LICENSE for details.
--
-- love.window and the parts of love.system which are implemented in Lua.
-- Most of these have nothing to do on DOS and exist so that games written
-- for LÖVE run unchanged
--

local sys = love.system

function sys.getProcessorCount() return 1 end
function sys.getPowerInfo() return "unknown", nil, nil end
function sys.openURL() return false end
function sys.vibrate() end
function sys.hasBackgroundMusic() return false end

-- The clipboard only exists within the program
local clipboard = ""
function sys.getClipboardText() return clipboard end
function sys.setClipboardText(text) clipboard = tostring(text) end


-- love.window
local win = {}
love.window = win

local W, H = 320, 200
local title = "Untitled"
local icon

function win._init(conf)
  -- Called by boot.lua with conf.lua's window table
  win._init = nil
  if type(conf) ~= "table" then return end
  title = conf.title or title
  if conf.vsync and conf.vsync ~= 0 then win.setVSync(conf.vsync) end
end

function win.setVSync(vsync)
  -- Waits for the VGA's vertical retrace before presenting a frame, which
  -- avoids tearing but limits the frame rate to 70
  love.graphics._setVSync(vsync and vsync ~= 0)
end

function win.getVSync()
  return love.graphics._getVSync() and 1 or 0
end

local function flags()
  return {
    fullscreen = true, fullscreentype = "exclusive",
    vsync = win.getVSync(), msaa = 0, stencil = false, depth = 0,
    resizable = false, borderless = true, centered = true, display = 1,
    minwidth = W, minheight = H, highdpi = false, refreshrate = 70,
    x = 0, y = 0, usedpiscale = true,
  }
end

function win.getMode() return W, H, flags() end
function win.setMode(w, h, f)
  if f and f.vsync ~= nil then win.setVSync(f.vsync) end
  return w == W and h == H
end
function win.updateMode(w, h, f) return win.setMode(w or W, h or H, f) end
function win.getTitle() return title end
function win.setTitle(t) title = tostring(t) end
function win.getFullscreen() return true, "exclusive" end
function win.setFullscreen() return true end
function win.getFullscreenModes() return { { width = W, height = H } } end
function win.getDesktopDimensions() return W, H end
function win.getDisplayCount() return 1 end
function win.getDisplayName() return "VGA" end
function win.getDisplayOrientation() return "landscape" end
function win.getDPIScale() return 1 end
function win.toPixels(x, y) return x, y end
function win.fromPixels(x, y) return x, y end
function win.getSafeArea() return 0, 0, W, H end
function win.hasFocus() return true end
function win.hasMouseFocus() return true end
function win.isOpen() return true end
function win.isVisible() return true end
function win.isMinimized() return false end
function win.isMaximized() return false end
function win.minimize() end
function win.maximize() end
function win.restore() end
function win.requestAttention() end
function win.getPosition() return 0, 0, 1 end
function win.setPosition() end
function win.getIcon() return icon end
function win.setIcon(data) icon = data return true end
function win.isDisplaySleepEnabled() return false end
function win.setDisplaySleepEnabled() end
function win.close() end

function win.showMessageBox(t, message, buttons)
  -- Shows the message until a key is pressed; returns the first button
  if love.graphics and love.event then
    local text = tostring(t) .. "\n\n" .. tostring(message) ..
                 "\n\nPress a key to continue"
    love.event.clear()
    love.graphics.push()
    love.graphics.origin()
    while true do
      love.graphics.clear()
      love.graphics.printf(text, 10, 10, W - 20)
      love.graphics.present()
      local name = love.event.wait()
      if name == "keypressed" or name == "mousepressed" then break end
    end
    love.graphics.pop()
  end
  return type(buttons) == "table" and 1 or true
end
