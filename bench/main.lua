--
-- LoveDOS benchmark: measures the CPU used by audio mixing and the time taken
-- by graphics operations. Run it with `love bench`; the results are shown on
-- screen and written to `save/bench.txt`.
--

local results = {}
local getTime = love.timer.getTime
local g, a = love.graphics, love.audio

local function out(name, value)
  table.insert(results, string.format("%-30s %s", name, value))
end


local function timeit(f)
  -- Returns the average time f() takes, repeating it for at least 0.25s
  local reps = 1
  while true do
    local t0 = getTime()
    for i = 1, reps do f() end
    local dt = getTime() - t0
    if dt >= 0.25 then return dt / reps end
    reps = reps * (dt < 0.02 and 8 or 2)
  end
end

local empty

local function bench(name, f)
  local us = (timeit(f) - empty) * 1e6
  out(name, string.format("%8.1f us", us))
end


local function loopRate()
  -- Iterations of a busy loop per second; less time left for the main program
  -- (eg. due to the audio interrupt) means fewer iterations
  local n, t0 = 0, getTime()
  while getTime() - t0 < 1 do
    for i = 1, 200 do end
    n = n + 1
  end
  return n
end


local function tone(freq, rate, channels)
  local sd = love.sound.newSoundData(rate, rate, 16, channels)
  for i = 0, rate - 1 do
    local v = 0.5 * math.sin(i / rate * freq * 2 * math.pi)
    for c = 1, channels do sd:setSample(i, c, v) end
  end
  return sd
end


local function audioBenchmarks()
  local mono = tone(440, 22050, 1)
  local stereo = tone(440, 44100, 2)
  local base = loopRate()

  local function load(name, n, data, setup)
    local srcs = {}
    for i = 1, n do
      srcs[i] = a.newSource(data)
      srcs[i]:setLooping(true)
      if setup then setup(srcs[i], i) end
      srcs[i]:play()
    end
    out("audio cpu: " .. name,
        string.format("%8.1f %%", 100 * (1 - loopRate() / base)))
    a.stop()
  end

  if not a.newSource(mono):play() then
    out("audio", "no Sound Blaster 16 found")
    return
  end
  a.stop()
  load("1 src 22kHz mono", 1, mono)
  load("4 src 22kHz mono", 4, mono)
  load("8 src 22kHz mono", 8, mono)
  load("4 src 22kHz pitched", 4, mono, function(s) s:setPitch(1.1) end)
  load("4 src 44kHz stereo", 4, stereo)
  load("4 src 22kHz positioned", 4, mono,
       function(s, i) s:setPosition(i * 3, 2, 0) end)

  local s = a.newSource(mono)
  bench("Source:setVolume", function() s:setVolume(0.5) end)
  bench("Source:setPosition", function() s:setPosition(10, 5, 0) end)
  local many = {}
  for i = 1, 32 do many[i] = a.newSource(mono) end
  bench("audio.setPosition, 33 srcs", function() a.setPosition(1, 2, 0) end)
end


local function graphicsBenchmarks()
  local spr = g.newCanvas(16, 16)
  for y = 0, 15 do
    for x = 0, 15 do
      if (x + y) % 3 > 0 then spr:setPixel(x, y, 255, x * 16, y * 16) end
    end
  end
  local big = g.newCanvas(64, 64)
  local text = "The quick brown fox jumps over the lazy dog"
  local para = string.rep(text .. " ", 4)
  g.setColor(255, 255, 255)

  bench("clear", function() g.clear() end)
  bench("present", function() g.present() end)
  bench("draw 16x16", function() g.draw(spr, 100, 50) end)
  bench("draw 16x16 mirrored", function() g.draw(spr, 100, 50, 0, -1, 1) end)
  bench("draw 16x16 scaled x2", function() g.draw(spr, 100, 50, 0, 2, 2) end)
  bench("draw 16x16 rotated",
        function() g.draw(spr, 100, 50, 0.3, 1, 1, 8, 8) end)
  bench("draw 16x16 rotated, x2",
        function() g.draw(spr, 100, 50, 0.3, 2, 2, 8, 8) end)
  g.translate(3, 3)
  bench("draw 16x16 translated", function() g.draw(spr, 100, 50) end)
  g.origin()
  g.setScissor(104, 0, 8, 200)
  bench("draw 16x16 scissored", function() g.draw(spr, 100, 50) end)
  g.setScissor()
  bench("draw 64x64", function() g.draw(big, 0, 0) end)
  bench("rectangle fill 32x32",
        function() g.rectangle("fill", 10, 10, 32, 32) end)
  bench("polygon fill 32x32",
        function() g.polygon("fill", 10, 10, 42, 10, 42, 42, 10, 42) end)
  bench("polygon fill triangle",
        function() g.polygon("fill", 10, 10, 60, 30, 20, 60) end)
  bench("circle fill r16", function() g.circle("fill", 50, 50, 16) end)
  bench("ellipse fill 16x16", function() g.ellipse("fill", 50, 50, 16, 16) end)
  bench("ellipse fill 32x16", function() g.ellipse("fill", 50, 50, 32, 16) end)
  bench("circle line r16", function() g.circle("line", 50, 50, 16) end)
  bench("line 100px", function() g.line(0, 0, 100, 40) end)
  local pts = {}
  for i = 1, 50 do pts[#pts + 1] = i pts[#pts + 1] = i end
  bench("points, 50 in one call", function() g.points(pts) end)
  bench("point, 50 calls",
        function() for i = 1, 50 do g.point(i, i) end end)
  bench("print 43 chars", function() g.print(text, 0, 100) end)
  bench("printf 43 chars", function() g.printf(text, 0, 100, 320) end)
  bench("print 176 chars", function() g.print(para, 0, 100) end)
  bench("printf 176 chars, wrapped",
        function() g.printf(para, 0, 100, 150) end)
  bench("printf 176 chars, justified",
        function() g.printf(para, 0, 100, 150, "justify") end)
end


function love.load()
  empty = timeit(function() end)
  out("LoveDOS", love.getVersion())
  audioBenchmarks()
  graphicsBenchmarks()
  love.filesystem.write("bench.txt", table.concat(results, "\r\n") .. "\r\n")
end


local page = 0
local perPage = 22

function love.draw()
  g.setColor(255, 255, 255)
  for i = 1, perPage do
    local line = results[page * perPage + i]
    if line then g.print(line, 4, (i - 1) * 9) end
  end
  g.print("space: next page, escape: quit", 4, 191)
end


function love.keypressed(key)
  if key == "escape" then
    love.event.quit()
  elseif key == "space" then
    page = (page + 1) % math.ceil(#results / perPage)
  end
end
