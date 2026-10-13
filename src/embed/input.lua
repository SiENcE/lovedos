--
-- Copyright (c) 2017 rxi
--
-- This library is free software; you can redistribute it and/or modify it
-- under the terms of the MIT license. See LICENSE for details.
--
-- The parts of love.mouse and love.keyboard which are implemented in Lua
--

local mouse, keyboard = love.mouse, love.keyboard


-- Mouse cursor: drawn by love.graphics.present() when visible. Unlike in
-- LÖVE the cursor is hidden by default, as games written for LoveDOS draw
-- their own
local visible = false
local current
local Cursor = { __index = {} }
local C = Cursor.__index

function C:getType() return self.ctype end
function C:type() return "Cursor" end
function C:typeOf(name) return name == "Cursor" or name == "Object" end
function C:release() return true end

local function updateCursor()
  if visible and current and not mouse.getRelativeMode() then
    love.graphics._setCursor(current.image, current.hotx, current.hoty)
  else
    love.graphics._setCursor()
  end
end

function mouse.newCursor(image, hotx, hoty)
  if type(image) == "string" then
    image = love.graphics.newImage(image)
  end
  return setmetatable({ image = image, hotx = hotx or 0, hoty = hoty or 0,
                        ctype = "image" }, Cursor)
end

local arrow
function mouse.getSystemCursor(ctype)
  -- All system cursors are the arrow
  if not arrow then
    local rows = {
      "X.........", "XX........", "XWX.......", "XWWX......", "XWWWX.....",
      "XWWWWX....", "XWWWWWX...", "XWWWWWWX..", "XWWWWWWWX.", "XWWWWXXXXX",
      "XWWXWWX...", "XWX.XWWX..", "XX..XWWX..", "X....XWWX.", ".....XWWX.",
      "......XX..",
    }
    local m = select(4, love.graphics.getColor()) -- 255, or 1 for LÖVE 11
    local img = love.graphics.newCanvas(10, #rows)
    for y, row in ipairs(rows) do
      for x = 1, #row do
        local c = row:sub(x, x)
        if c == "X" then img:setPixel(x - 1, y - 1, 0, 0, 0)
        elseif c == "W" then img:setPixel(x - 1, y - 1, m, m, m)
        else img:setPixel(x - 1, y - 1) end
      end
    end
    arrow = setmetatable({ image = img, hotx = 0, hoty = 0 }, Cursor)
  end
  return setmetatable({ image = arrow.image, hotx = 0, hoty = 0,
                        ctype = ctype or "arrow" }, Cursor)
end

function mouse.setCursor(cursor)
  current = cursor
  updateCursor()
end

function mouse.getCursor()
  return current
end

function mouse.setVisible(enable)
  visible = enable and true or false
  if visible and not current then
    current = mouse.getSystemCursor("arrow")
  end
  updateCursor()
end

function mouse.isVisible()
  return visible
end

local setRelativeMode = mouse.setRelativeMode
function mouse.setRelativeMode(enable)
  setRelativeMode(enable)
  updateCursor()
end

-- DOS programs always have the whole screen
local grabbed = false
function mouse.setGrabbed(enable) grabbed = enable and true or false end
function mouse.isGrabbed() return grabbed end


-- Keyboard: LoveDOS' key names are based on the keys' positions (scancodes),
-- so keys and scancodes are the same
keyboard.isScancodeDown = keyboard.isDown
function keyboard.getKeyFromScancode(scancode) return scancode end
function keyboard.getScancodeFromKey(key) return key end
function keyboard.hasScreenKeyboard() return false end

local textInput = true
function keyboard.setTextInput(enable) textInput = enable and true or false end
function keyboard.hasTextInput() return textInput end

function keyboard._filterTextInput(f)
  -- Internal: wraps the textinput handler so text input can be disabled
  return function(...)
    if textInput then return f(...) end
  end
end
