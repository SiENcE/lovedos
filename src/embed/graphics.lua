--
-- Copyright (c) 2017 rxi
--
-- This library is free software; you can redistribute it and/or modify it
-- under the terms of the MIT license. See LICENSE for details.
--
-- The parts of love.graphics, love.image and love.window which are
-- implemented in Lua
--

local g = love.graphics
local registry = debug.getregistry()


-- Line style and join can't change how lines are drawn in 256 colors but
-- are kept for compatibility
local lineStyle, lineJoin = "rough", "miter"
function g.setLineStyle(style) lineStyle = style end
function g.getLineStyle() return lineStyle end
function g.setLineJoin(join) lineJoin = join end
function g.getLineJoin() return lineJoin end

local defaultFilter = { "nearest", "nearest", 1 }
function g.setDefaultFilter(min, mag, anisotropy)
  defaultFilter = { min, mag or min, anisotropy or 1 }
end
function g.getDefaultFilter() return unpack(defaultFilter) end


function g.arc(mode, arctype, x, y, radius, angle1, angle2, segments)
  -- arc(mode, [arctype,] x, y, radius, angle1, angle2 [, segments])
  if type(arctype) ~= "string" then
    arctype, x, y, radius, angle1, angle2, segments =
      "pie", arctype, x, y, radius, angle1, angle2
  end
  segments = segments or math.max(math.floor(radius * math.abs(angle2 - angle1) / 6), 4)
  local v = {}
  if arctype == "pie" then v[1], v[2] = x, y end
  for i = 0, segments do
    local a = angle1 + (angle2 - angle1) * i / segments
    v[#v + 1] = x + math.cos(a) * radius
    v[#v + 1] = y + math.sin(a) * radius
  end
  if mode == "fill" or arctype ~= "open" then
    if #v >= 6 then g.polygon(mode, v) end
  else
    g.line(v)
  end
end


function g.transformPoint(x, y)
  -- The transformation is a translation only
  local ox, oy = g._getTranslation()
  return x + ox, y + oy
end

function g.inverseTransformPoint(x, y)
  local ox, oy = g._getTranslation()
  return x - ox, y - oy
end


function g.newScreenshot()
  -- LÖVE 0.10: returns a copy of the screen
  return g.newImage(g._getScreen())
end

local screenshots = {}
function g.captureScreenshot(target)
  -- Takes the screenshot when the frame is presented; `target` is a
  -- filename (the image is saved as PNG) or a function receiving the image
  screenshots[#screenshots + 1] = target
end

local present = g.present
function g.present()
  if #screenshots > 0 then
    local img = g.newImage(g._getScreen())
    for _, target in ipairs(screenshots) do
      if type(target) == "function" then
        target(img)
      elseif type(target) == "string" then
        img:encode("png", target)
      end
    end
    screenshots = {}
  end
  return present()
end


-- Information, for compatibility
function g.getRendererInfo()
  return "LoveDOS software renderer", love.getVersion(), "VGA",
         "320x200, 256 colors"
end
function g.getStats(t)
  t = t or {}
  t.drawcalls, t.canvasswitches, t.shaderswitches = 0, 0, 0
  t.drawcallsbatched, t.texturememory = 0, 0
  t.images, t.canvases, t.fonts = 0, 0, 0
  return t
end
function g.getSupported()
  return { clampzero = false, lighten = false, multicanvasformats = false,
           glsl3 = false, instancing = false, fullnpot = true,
           pixelshaderhighp = false, shaderderivatives = false }
end
function g.getSystemLimits()
  return { texturesize = 32768, pointsize = 320, multicanvas = 1,
           canvasmsaa = 1, anisotropy = 1, texturelayers = 1,
           volumetexturesize = 1, cubetexturesize = 1 }
end
function g.getCanvasFormats() return { normal = true, rgba8 = true } end
function g.getImageFormats() return { rgba8 = true } end
function g.getTextureTypes() return { ["2d"] = true } end
function g.isGammaCorrect() return false end
function g.isActive() return true end
function g.isWireframe() return false end
function g.getDPIScale() return 1 end
g.getPixelDimensions = g.getDimensions
g.getPixelWidth = g.getWidth
g.getPixelHeight = g.getHeight


-- Image: LoveDOS' Image is also LÖVE's Canvas, ImageData and Texture
local I = registry.Image.__index

function I:getFormat() return "rgba8" end
function I:getDPIScale() return 1 end
I.getPixelDimensions = I.getDimensions
I.getPixelWidth = I.getWidth
I.getPixelHeight = I.getHeight
function I:getTextureType() return "2d" end
function I:isReadable() return true end
function I:getDepth() return 1 end
function I:getLayerCount() return 1 end
function I:getMipmapCount() return 1 end
function I:isCompressed() return false end
function I:isFormatLinear() return false end
function I:getMSAA() return 0 end
function I:generateMipmaps() end
function I:getFilter() return unpack(defaultFilter) end
function I:setFilter() end
function I:getMipmapFilter() return nil end
function I:setMipmapFilter() end
function I:getWrap() return "clamp", "clamp" end
function I:setWrap() end

function I:replacePixels(data)
  self:paste(data, 0, 0)
end

function I:renderTo(f, ...)
  local old = g.getCanvas()
  g.setCanvas(self)
  f(...)
  g.setCanvas(old)
end

function I:newImageData()
  return g.newImage(self)
end

function I:mapPixel(f, x, y, w, h)
  -- Calls f(x, y, r, g, b, a) for each pixel and sets the pixel to the
  -- returned color; an alpha below half makes the pixel transparent
  x, y = x or 0, y or 0
  w, h = w or self:getWidth() - x, h or self:getHeight() - y
  local half = select(4, g.getColor()) / 2
  for py = y, y + h - 1 do
    for px = x, x + w - 1 do
      local r, gg, b, a = self:getPixel(px, py)
      if not r then r, gg, b, a = 0, 0, 0, 0 end
      r, gg, b, a = f(px, py, r, gg, b, a)
      if a and a < half then
        self:setPixel(px, py)
      else
        self:setPixel(px, py, r, gg, b)
      end
    end
  end
end

function I:encode(format, filename)
  -- Encodes the image as "png", "tga" or "bmp", returns it as FileData and
  -- writes it to `filename` if given
  local data = self:_encode(format)
  if filename then
    love.filesystem.write(filename, data)
  end
  return love.filesystem.newFileData(data, filename or ("image." .. format))
end


-- love.image
love.image = love.image or {}

function love.image.newImageData(a, b)
  -- newImageData(width, height), newImageData(filename) or
  -- newImageData(filedata)
  if type(a) == "number" then
    return g.newCanvas(a, b)
  end
  return g.newImage(a)
end

function love.image.isCompressed() return false end
function love.image.newCompressedData()
  error("compressed textures are not supported", 2)
end


-- Font
local F = registry.Font.__index
function F:getKerning() return 0 end
function F:setFallbacks() end
function F:getFilter() return unpack(defaultFilter) end
function F:setFilter() end
function F:getDPIScale() return 1 end
