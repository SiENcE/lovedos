--
-- Copyright (c) 2017 rxi
--
-- This library is free software; you can redistribute it and/or modify it
-- under the terms of the MIT license. See LICENSE for details.
--
-- love.joystick for the PC gameport: up to two joysticks with two axes and
-- two buttons each, or one joystick with four buttons
--

local js = love.joystick
local band = bit32.band
local sticks = {}

local Joystick = { __index = {} }
local J = Joystick.__index


function js._init()
  -- Called by boot.lua after conf.lua; the joysticks should be centered
  js._init = nil
  local mask = js._detect()
  local first = band(mask, 3) == 3
  local second = band(mask, 12) == 12
  if first then
    sticks[#sticks + 1] = setmetatable({
      id = #sticks + 1, axisIdx = { 1, 2 },
      -- A 4 button joystick uses the second joystick's buttons
      buttonBits = second and { 1, 2 } or { 1, 2, 4, 8 },
      axes = { 0, 0 }, buttons = {}, connected = true,
    }, Joystick)
  end
  if second then
    sticks[#sticks + 1] = setmetatable({
      id = #sticks + 1, axisIdx = { 3, 4 }, buttonBits = { 4, 8 },
      axes = { 0, 0 }, buttons = {}, connected = true,
    }, Joystick)
  end
  if #sticks == 0 then return end

  -- Poll the gameport once per frame (it takes up to ~1.5ms)
  love.event._addPumpHook(function()
    local a1, a2, a3, a4, buttons = js._read()
    local raw = { a1, a2, a3, a4 }
    for _, j in ipairs(sticks) do
      for i, idx in ipairs(j.axisIdx) do
        local v = raw[idx] or 0
        if v ~= j.axes[i] then
          j.axes[i] = v
          love.event.push("joystickaxis", j, i, v)
        end
      end
      for i, bit in ipairs(j.buttonBits) do
        local down = band(buttons, bit) ~= 0
        if down ~= (j.buttons[i] or false) then
          j.buttons[i] = down
          love.event.push(down and "joystickpressed" or "joystickreleased", j, i)
        end
      end
    end
  end)
  for _, j in ipairs(sticks) do
    love.event.push("joystickadded", j)
  end
end


function js.getJoysticks()
  local t = {}
  for i, j in ipairs(sticks) do t[i] = j end
  return t
end

function js.getJoystickCount()
  return #sticks
end

-- Gamepad mappings don't apply to gameport joysticks
function js.setGamepadMapping() return false end
function js.loadGamepadMappings() end
function js.saveGamepadMappings() return "" end
function js.getGamepadMappingString() return nil end


function J:getName() return "Gameport joystick " .. self.id end
function J:getID() return self.id, self.id end
function J:getGUID() return "gameport" .. self.id end
function J:getDeviceInfo() return 0, 0, 0 end
function J:isConnected() return self.connected end
function J:getAxisCount() return #self.axisIdx end
function J:getButtonCount() return #self.buttonBits end
function J:getHatCount() return 0 end
function J:getHat() return "c" end
function J:isGamepad() return false end
function J:getGamepadAxis() return 0 end
function J:isGamepadDown() return false end
function J:getGamepadMapping() return nil end
function J:isVibrationSupported() return false end
function J:setVibration() return false end
function J:getVibration() return 0, 0 end
function J:type() return "Joystick" end
function J:typeOf(name) return name == "Joystick" or name == "Object" end
function J:release() return true end

function J:getAxis(i)
  return self.axes[i] or 0
end

function J:getAxes()
  return unpack(self.axes)
end

function J:isDown(...)
  for i = 1, select("#", ...) do
    if self.buttons[select(i, ...)] then return true end
  end
  return false
end
