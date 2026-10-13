local t = require("testlib")
local g = love.graphics

local function count(c)
  local n = 0
  for y = 0, c:getHeight() - 1 do for x = 0, c:getWidth() - 1 do
    local r, gg, b = c:getPixel(x, y)
    if r and r + gg + b > 0 then n = n + 1 end
  end end
  return n
end
local function set(c, x, y) local r, gg, b = c:getPixel(x, y) return r ~= nil and r + gg + b > 0 end
local function fresh(c) g.setCanvas(c) g.origin() g.setScissor() g.clear(0, 0, 0) g.setColor(255, 255, 255) end

local shot
function love.load()
  -- mouse
  love.mouse.setPosition(100, 50)
  t.check("mouse setPosition", love.mouse.getX() == 100 and love.mouse.getY() == 50)
  love.mouse.setX(10) love.mouse.setY(20)
  t.check("mouse setX/setY", love.mouse.getX() == 10 and love.mouse.getY() == 20)
  love.mouse.setRelativeMode(true)
  t.check("relative mode", love.mouse.getRelativeMode())
  love.mouse.setRelativeMode(false)
  t.check("cursor hidden by default", not love.mouse.isVisible())
  love.mouse.setVisible(true)
  t.check("setVisible", love.mouse.isVisible() and love.mouse.getCursor():getType() == "arrow")
  local cimg = g.newCanvas(4, 4)
  local cur = love.mouse.newCursor(cimg, 1, 1)
  love.mouse.setCursor(cur)
  t.check("custom cursor", love.mouse.getCursor() == cur and cur:getType() == "image" and love.mouse.isCursorSupported())
  love.mouse.setGrabbed(true)
  t.check("grabbed", love.mouse.isGrabbed())

  -- keyboard
  love.keyboard.setKeyRepeat(true)
  t.check("hasKeyRepeat", love.keyboard.hasKeyRepeat())
  love.keyboard.setKeyRepeat(false)
  t.check("scancodes", love.keyboard.getKeyFromScancode("a") == "a" and love.keyboard.getScancodeFromKey("space") == "space"
          and love.keyboard.isScancodeDown("a") == false and not love.keyboard.hasScreenKeyboard())
  love.keyboard.setTextInput(false)
  t.check("text input", not love.keyboard.hasTextInput())
  love.keyboard.setTextInput(true)

  -- graphics
  local c = g.newCanvas(40, 40)
  g.setLineWidth(5)
  t.check("line width", g.getLineWidth() == 5)
  fresh(c) g.line(5, 20, 35, 20) g.setCanvas()
  t.check("thick line", set(c, 20, 18) and set(c, 20, 21) and not set(c, 20, 23) and not set(c, 20, 16))
  fresh(c) g.setLineWidth(3) g.rectangle("line", 10, 10, 20, 20) g.setCanvas()
  t.check("thick rectangle", set(c, 9, 15) and set(c, 11, 15) and not set(c, 12, 15) and not set(c, 8, 15) and not set(c, 20, 20))
  fresh(c) g.setLineWidth(4) g.circle("line", 20, 20, 10) g.setCanvas()
  t.check("thick circle", set(c, 30, 20) and set(c, 31, 20) and not set(c, 20, 20) and not set(c, 34, 20))
  fresh(c) g.ellipse("line", 20, 20, 12, 6) g.setCanvas()
  t.check("thick ellipse", set(c, 32, 20) and not set(c, 20, 20))
  g.setLineWidth(1)
  fresh(c) g.setPointSize(3) g.points(10, 10) g.setPointSize(1) g.setCanvas()
  t.check("point size", count(c) == 9 and g.getPointSize() == 1)
  fresh(c) g.arc("fill", 20, 20, 10, 0, math.pi / 2) g.setCanvas()
  local n = count(c)
  t.check("arc pie (" .. n .. ")", n > 60 and n < 100 and set(c, 23, 23) and not set(c, 17, 23))
  fresh(c) g.setLineWidth(1) g.arc("line", "open", 20, 20, 10, 0, math.pi) g.setCanvas()
  t.check("arc open line", (set(c, 20, 29) or set(c, 20, 30)) and not set(c, 20, 20))
  g.setScissor(0, 0, 20, 20) g.intersectScissor(10, 10, 20, 20)
  local sx, sy, sw, sh = g.getScissor()
  t.check("intersectScissor", sx == 10 and sy == 10 and sw == 10 and sh == 10)
  g.setScissor()
  g.push() g.push() g.translate(5, 6)
  local px, py = g.transformPoint(1, 1)
  local ix, iy = g.inverseTransformPoint(6, 7)
  t.check("stack depth / transformPoint", g.getStackDepth() == 2 and px == 6 and py == 7 and ix == 1 and iy == 1)
  g.pop() g.pop()
  g.setLineStyle("smooth") g.setLineJoin("bevel") g.setDefaultFilter("linear")
  t.check("line style/join, default filter", g.getLineStyle() == "smooth" and g.getLineJoin() == "bevel"
          and g.getDefaultFilter() == "linear")

  -- images
  local img = g.newCanvas(3, 2)
  img:setPixel(0, 0, 255, 0, 0) img:setPixel(2, 1, 0, 0, 255)
  local copy = g.newImage(img)
  img:setPixel(0, 0, 0, 255, 0)
  t.check("newImage copies", select(1, copy:getPixel(0, 0)) == 255)
  local png = copy:encode("png")
  t.check("encode png", png:getString():sub(2, 4) == "PNG" and png:getExtension() == "png")
  local decoded = g.newImage(png)
  t.check("png round trip", decoded:getWidth() == 3 and select(1, decoded:getPixel(0, 0)) == 255
          and select(3, decoded:getPixel(2, 1)) == 255 and decoded:getPixel(1, 0) == nil)
  t.check("encode bmp/tga", copy:encode("bmp"):getString():sub(1, 2) == "BM" and #copy:encode("tga"):getString() > 18)
  copy:encode("png", "out.png")
  t.check("encode to file", love.filesystem.getInfo("out.png") ~= nil)
  local dst = love.image.newImageData(4, 4)
  dst:paste(copy, 1, 1)
  t.check("paste", select(1, dst:getPixel(1, 1)) == 255 and dst:getPixel(0, 0) == nil and select(3, dst:getPixel(3, 2)) == 255)
  dst:mapPixel(function(x, y, r, gg, b, a) return 0, 255, 0, 255 end, 0, 0, 2, 1)
  t.check("mapPixel", select(2, dst:getPixel(0, 0)) == 255 and select(2, dst:getPixel(1, 0)) == 255 and dst:getPixel(2, 0) == nil)
  local target = g.newCanvas(5, 5)
  target:renderTo(function() g.setColor(255, 255, 255) g.points(2, 2) end)
  t.check("renderTo", set(target, 2, 2) and g.getCanvas() ~= target)
  t.check("texture stubs", img:getFormat() == "rgba8" and img:getFilter() == "linear" and img:getDPIScale() == 1
          and img:getPixelWidth() == 3 and img:getTextureType() == "2d")

  -- fonts
  local f = g.getFont()
  t.check("font metrics", f:getAscent() > 0 and f:getDescent() <= 0 and f:getBaseline() == f:getAscent())
  local h = f:getHeight()
  fresh(c) f:setLineHeight(2) g.print("I\nI", 0, 0) g.setCanvas()
  local lowest = 0
  for y = 0, 39 do for x = 0, 10 do if set(c, x, y) then lowest = y end end end
  f:setLineHeight(1)
  t.check("line height", lowest >= 2 * h and f:getLineHeight() == 1)
  t.check("hasGlyphs", f:hasGlyphs("abc", 65) and not f:hasGlyphs("é") and f:getKerning("a", "b") == 0)

  -- objects
  t.check("type/typeOf", img:type() == "Image" and img:typeOf("Texture") and img:typeOf("Drawable")
          and img:typeOf("Object") and not img:typeOf("Font") and love.sound.newSoundData(4):typeOf("Data"))
  local sd = love.sound.newSoundData(4)
  t.check("release", sd:release() and t.errors(sd.getSampleCount, sd) and sd:release() == false)
  t.check("current font can't be released", f:release() == false and f:getHeight() == h)
  t.check("released sounddata can't make a source", t.errors(love.audio.newSource, sd))

  -- window / system / renderer info
  local w, hh, fl = love.window.getMode()
  t.check("window mode", w == 320 and hh == 200 and fl.fullscreen and love.window.getDesktopDimensions() == 320)
  love.window.setTitle("x")
  t.check("window title", love.window.getTitle() == "x" and love.window.hasFocus() and love.window.getDPIScale() == 1)
  love.window.setVSync(1)
  t.check("vsync", love.window.getVSync() == 1)
  love.window.setVSync(0)
  love.system.setClipboardText("clip")
  t.check("system stubs", love.system.getProcessorCount() == 1 and love.system.getClipboardText() == "clip"
          and love.system.getPowerInfo() == "unknown" and love.system.openURL("x") == false)
  t.check("renderer info", select(3, g.getRendererInfo()) == "VGA" and g.getStats().drawcalls == 0
          and g.getPixelWidth() == 320 and g.getSystemLimits().texturesize > 0)

  -- love.data inside LoveDOS
  local d = love.data
  local packed = d.pack("string", "<i4 d f s1", -5, 0.1, 0.5, "hi")
  local a1, a2, a3, a4 = d.unpack("<i4 d f s1", packed)
  t.check("data pack/unpack", a1 == -5 and a2 == 0.1 and a3 == 0.5 and a4 == "hi")
  t.check("data base64", d.decode("string", "base64", d.encode("string", "base64", "LoveDOS!")) == "LoveDOS!")

  g.captureScreenshot(function(img) shot = img end)
end

local frames = 0
function love.update()
  frames = frames + 1
  if frames == 3 then
    t.check("captureScreenshot", shot and shot:getWidth() == 320 and g.newScreenshot():getHeight() == 200)
    t.finish()
  end
end
