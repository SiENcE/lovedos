local t = require("testlib")
local g = love.graphics

local function snapshot(c)
  local s = {}
  for y = 0, c:getHeight() - 1 do
    for x = 0, c:getWidth() - 1 do
      local r, gg, b = c:getPixel(x, y)
      s[#s + 1] = r and (r * 65536 + gg * 256 + b) or -1
    end
  end
  return table.concat(s, ",")
end

local function render(c, f)
  g.setCanvas(c) g.origin() g.setScissor() g.clear(0, 0, 0) g.setColor(255, 255, 255)
  f()
  g.setCanvas()
  return snapshot(c)
end

function love.load()
  local c = g.newCanvas(80, 60)
  local img = g.newCanvas(4, 2)
  local cols = { {255, 0, 0}, {0, 255, 0}, {0, 0, 255}, {255, 255, 0} }
  for i = 1, 4 do img:setPixel(i - 1, 0, unpack(cols[i])) img:setPixel(i - 1, 1, unpack(cols[5 - i])) end
  local quad = g.newQuad(1, 0, 2, 2)

  -- SpriteBatch draws exactly like individual draw calls
  local sprites = {
    { 0, 0 }, { 10, 3, 0, -1, 1 }, { 20, 5, 0, 2, 3 }, { 40, 20, 0.7, 1, 1, 2, 1 },
    { quad, 50, 2 }, { quad, 60, 10, 0, -1, 1, 1, 0 }, { -2, 50 }, { 75, 58, 0, 3, 3 },
  }
  local batch = g.newSpriteBatch(img, 2)
  local ids = {}
  for i, s in ipairs(sprites) do ids[i] = batch:add(unpack(s)) end
  t.check("ids start at 0 (LÖVE 0.10)", ids[1] == 0 and ids[8] == 7)
  t.check("count and growth", batch:getCount() == 8 and batch:getBufferSize() >= 8)
  local direct = render(c, function()
    g.translate(3, 2)
    for _, s in ipairs(sprites) do g.draw(img, unpack(s)) end
  end)
  local batched = render(c, function() g.translate(3, 2) g.draw(batch) end)
  t.check("batch == individual draws", direct == batched)
  t.check("batch draws something", direct ~= render(c, function() end))
  local offset = render(c, function() g.translate(13, 7) for _, s in ipairs(sprites) do g.draw(img, unpack(s)) end end)
  t.check("batch drawn at x, y", offset == render(c, function() g.draw(batch, 13, 7) end))
  batch:set(ids[1], 30, 30)
  sprites[1] = { 30, 30 }
  t.check("set", render(c, function() for _, s in ipairs(sprites) do g.draw(img, unpack(s)) end end)
                 == render(c, function() g.draw(batch) end))
  batch:setDrawRange(2, 1)
  t.check("draw range", render(c, function() g.draw(img, unpack(sprites[2])) end)
                        == render(c, function() g.draw(batch) end))
  batch:setDrawRange()
  t.check("getTexture", batch:getTexture() == img)
  t.check("rotated batch draw errors", t.errors(g.draw, batch, 0, 0, 1))
  t.check("bad id errors", t.errors(batch.set, batch, 99, 0, 0))
  g.setScissor(0, 0, 20, 20)
  local sc = render(c, function() g.setScissor(0, 0, 20, 20) g.draw(batch) end)
  t.check("batch respects scissor", sc == render(c, function() g.setScissor(0, 0, 20, 20) for _, s in ipairs(sprites) do g.draw(img, unpack(s)) end end))
  batch:clear()
  t.check("clear", batch:getCount() == 0 and render(c, function() g.draw(batch) end) == render(c, function() end))

  -- Text draws exactly like print / printf
  local font = g.getFont()
  local text = g.newText(font, "Hello DOS")
  t.check("Text == print", render(c, function() g.print("Hello DOS", 3, 4) end)
                           == render(c, function() g.draw(text, 3, 4) end))
  t.check("Text dimensions", text:getWidth() == font:getWidth("Hello DOS") and text:getHeight() == font:getHeight())
  local para = "The quick brown fox jumps over the lazy dog"
  for _, align in ipairs { "left", "center", "right", "justify" } do
    text:setf(para, 70, align)
    t.check("Text:setf == printf " .. align,
      render(c, function() g.printf(para, 5, 2, 70, align) end) == render(c, function() g.draw(text, 5, 2) end))
  end
  text:clear()
  text:add("one", 0, 0) local i2 = text:add("two\nlines", 20, 30)
  t.check("add returns index", i2 == 2)
  t.check("Text:add", render(c, function() g.print("one", 0, 0) g.print("two\nlines", 20, 30) end)
                      == render(c, function() g.draw(text) end))
  text:set("Color")
  local red = render(c, function() g.setColor(255, 0, 0) g.draw(text, 1, 1) end)
  t.check("Text uses current color", red == render(c, function() g.setColor(255, 0, 0) g.print("Color", 1, 1) end))
  g.setScissor(0, 0, 2, 2) text:set("Scissor") g.setScissor()
  t.check("Text unaffected by scissor when built",
    render(c, function() g.draw(text, 0, 20) end) == render(c, function() g.print("Scissor", 0, 20) end))
  t.check("Text translated", render(c, function() g.translate(4, 4) g.draw(text) end)
                             == render(c, function() g.print("Scissor", 4, 4) end))
  local colored = g.newText(font, { {1, 0, 0}, "ab", {0, 1, 0}, "cd" })
  t.check("colored text table", colored:getWidth() == font:getWidth("abcd"))
  t.check("empty text draws nothing", render(c, function() g.draw(g.newText(font)) end) == render(c, function() end))

  -- ImageFont: glyphs separated by columns whose top pixel has the color of
  -- the top left pixel; glyphs keep their colors
  local fimg = g.newCanvas(9, 3)
  for y = 0, 2 do
    fimg:setPixel(0, y, 255, 0, 255) fimg:setPixel(3, y, 255, 0, 255) fimg:setPixel(7, y, 255, 0, 255)
    for x = 1, 2 do fimg:setPixel(x, y, 0, 255, 255) end
    for x = 4, 6 do fimg:setPixel(x, y, 255, 128, 0) end
  end
  fimg:setPixel(5, 1, 255, 0, 255) -- separator color inside a glyph: transparent
  local ifont = g.newImageFont(fimg, "AB", 1)
  t.check("ImageFont widths", ifont:getWidth("A") == 3 and ifont:getWidth("B") == 4 and ifont:getWidth("AB") == 7)
  t.check("ImageFont height", ifont:getHeight() == 3)
  t.check("ImageFont unknown chars", ifont:getWidth("AxB") == 7)
  local out = render(c, function() g.setFont(ifont) g.setColor(255, 0, 0) g.print("BA", 0, 0) g.setFont() end)
  g.setCanvas(c) g.setCanvas()
  local r, gg, b = c:getPixel(0, 0)
  t.check("ImageFont keeps glyph colors", r == 255 and gg == 128 and b == 0)
  r, gg, b = c:getPixel(4, 0)
  t.check("ImageFont second glyph", r == 0 and gg == 255 and b == 255)
  t.check("ImageFont separator pixel transparent", (c:getPixel(1, 1)) == 0)
  local ftext = g.newText(ifont, "BA")
  t.check("Text with ImageFont == print",
    render(c, function() g.draw(ftext, 0, 0) end) == out)
  t.check("UTF-8 glyph skipped", g.newImageFont(fimg, "éB"):getWidth("B") == 3)

  local nf = g.setNewFont(12)
  t.check("setNewFont", g.getFont() == nf and nf:getHeight() > 8)
  g.setFont()
  t.finish()
end
