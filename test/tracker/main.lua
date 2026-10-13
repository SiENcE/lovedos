local t = require("testlib")
local modgen = require("modgen")
local a = love.audio
local events, time = {}, 0
local function at(tt, f) events[#events + 1] = { t = tt, f = f } end
local song

-- A ProTracker row lasts 6 ticks of 441 frames at 22050Hz (speed 6, 125 BPM)
local ROW = 6 * 441

local function rms(sd, from, to, channel)
  local sum = 0
  for i = from, to - 1 do
    local v = sd:getSample(i, channel)
    sum = sum + v * v
  end
  return math.sqrt(sum / (to - from))
end

local function frequency(sd, from, to, channel)
  -- From the zero crossings
  local n, last = 0, sd:getSample(from, channel)
  for i = from + 1, to - 1 do
    local v = sd:getSample(i, channel)
    if (v >= 0) ~= (last >= 0) then n = n + 1 end
    last = v
  end
  return n / 2 / ((to - from) / 22050)
end

function love.load()
  -- 16 rows: a 518Hz tone (2 sine cycles in 32 samples at period 428,
  -- 8287Hz) for 8 rows, the volume set to 0 for 8 rows, then a pattern break
  modgen.write("song.mod", {
    samples = { { data = modgen.sine(32, 2, 100), loop = true } },
    patterns = { {
      [0] = { { 428, 1 } },
      [8] = { { 0, 0, 0xC, 0 } },
      [15] = { { 0, 0, 0xD, 0 } },
    } },
  })
  -- 6 channels, all playing, two patterns of 64 rows
  local rows = {}
  for c = 1, 6 do rows[c] = { 428 - c * 20, 1 } end
  modgen.write("six.mod", {
    channels = 6,
    samples = { { data = modgen.sine(32, 2, 60), loop = true } },
    patterns = { { [0] = rows }, { [0] = rows } },
    order = { 0, 1 },
  })
  love.filesystem.write("junk.mod", string.rep("not a module ", 100))
  love.filesystem.write("bad.xm", "Extended Module: " .. string.rep("\0", 100))
  love.filesystem.write("bad.s3m", string.rep("\0", 44) .. "SCRM" .. string.rep("\0", 100))
  love.filesystem.write("bad.mod", string.rep("\0", 1080) .. "M.K.")

  song = a.newSource("song.mod")
  t.check("module source", song:getType() == "stream" and song:getChannelCount() == 2)
  t.check("module duration " .. song:getDuration("samples"),
          song:getDuration("samples") == 16 * ROW and t.near(song:getDuration(), 16 * ROW / 22050, 1e-4))
  t.check("static module is streamed", a.newSource("song.mod", "static"):getType() == "stream")
  t.check("clone module", song:clone():getDuration("samples") == 16 * ROW)

  local six = a.newSource("six.mod")
  t.check("6 channel module", six:getDuration("samples") == 128 * ROW)

  local d = love.sound.newDecoder("song.mod", 4096)
  t.check("module decoder", d:getSampleRate() == 22050 and d:getBitDepth() == 16
          and d:getChannelCount() == 2 and d:getDuration() == song:getDuration())
  local total, chunks = 0, 0
  while true do
    local sd = d:decode()
    if not sd then break end
    total = total + sd:getSampleCount()
    chunks = chunks + 1
  end
  t.check("decode module (" .. chunks .. " chunks)", total == 16 * ROW and chunks == math.ceil(16 * ROW / 1024))
  d:seek(1)
  t.check("module decoder seek", t.near(d:tell(), 1, 1e-4) and d:decode():getSampleCount() == 1024)

  local sd = love.sound.newSoundData("song.mod")
  t.check("module SoundData", sd:getSampleCount() == 16 * ROW and sd:getChannelCount() == 2
          and sd:getBitDepth() == 16)
  local loud, quiet = rms(sd, 1000, 8 * ROW - 500, 1), rms(sd, 8 * ROW + 500, 16 * ROW, 1)
  t.log("rms", loud, quiet, rms(sd, 1000, 8 * ROW - 500, 2))
  t.check("note then silence", loud > 0.1 and quiet < 0.001)
  t.check("left louder (channel 1 is panned left)", rms(sd, 1000, 8 * ROW - 500, 2) < loud)
  local f = frequency(sd, 1000, 8 * ROW - 500, 1)
  t.check("note frequency " .. f, t.near(f, 517.9, 3))
  local sd2 = love.sound.newSoundData(love.sound.newDecoder("song.mod"))
  t.check("SoundData from module decoder", sd2:getSampleCount() == 16 * ROW
          and sd2:getSample(5000, 1) == sd:getSample(5000, 1))

  -- FastTracker 2: a 16bit sample, C-4 (8363Hz, 523Hz with 2 cycles in 32
  -- samples), centered; volume 0 at row 8 and a pattern break at row 15
  local wave = {}
  for i = 0, 31 do wave[#wave + 1] = math.floor(20000 * math.sin(i / 32 * 2 * 2 * math.pi)) end
  modgen.writeXM("song.xm", { channels = 2, rows = 16, sample = wave,
    notes = { [0] = { { 49, 1 } }, [8] = { { 0, 0, 0x10 } }, [15] = { { 0, 0, 0, 0xD, 0 } } } })
  local xm = a.newSource("song.xm")
  t.check("xm source", xm:getType() == "stream" and xm:getDuration("samples") == 16 * ROW)
  local xsd = love.sound.newSoundData("song.xm")
  local xl, xr = rms(xsd, 1000, 8 * ROW - 500, 1), rms(xsd, 1000, 8 * ROW - 500, 2)
  t.log("xm rms", xl, xr, rms(xsd, 8 * ROW + 500, 16 * ROW, 1))
  t.check("xm note then silence", xl > 0.05 and t.near(xl, xr, 0.01)
          and rms(xsd, 8 * ROW + 500, 16 * ROW, 1) < 0.001)
  f = frequency(xsd, 1000, 8 * ROW - 500, 1)
  t.check("xm note frequency " .. f, t.near(f, 522.7, 3))

  t.check("not a module", t.errors(a.newSource, "junk.mod"))
  t.check("invalid xm", t.errors(a.newSource, "bad.xm"))
  t.check("invalid s3m", t.errors(a.newSource, "bad.s3m"))
  t.log("empty mod loads:", pcall(a.newSource, "bad.mod"))
  t.check("wav still works", a.newSource(love.sound.newSoundData(100, 22050, 16, 1)):getType() == "static")

  song:setLooping(true)
  at(0.3, function() song:play() t.log("A module: tone 0.96s, silence 0.96s, looping") end)
  at(1.0, function() t.check("module plays", song:isPlaying() and song:tell() > 0.5 and song:tell() < 0.9) end)
  at(2.4, function() t.check("module loops " .. song:tell(), song:isPlaying() and song:tell() < 0.5) end)
  at(3.0, function() song:seek(1.2) t.log("B seek 1.2: silence") end)
  at(3.3, function() t.check("module seek " .. song:tell(), song:tell() > 1.35 and song:tell() < 1.7) end)
  at(3.5, function() song:stop() t.check("module stop " .. song:tell() .. tostring(song:isPlaying()), song:tell() == 0 and not song:isPlaying()) end)
  at(3.6, function()
    song:setLooping(false)
    song:play()
    t.log("C module once: tone 0.96s, silence 0.96s")
  end)
  at(6.0, function() t.check("module ends", not song:isPlaying()) t.finish() end)
end

function love.update(dt)
  time = time + dt
  while events[1] and time >= events[1].t do table.remove(events, 1).f() end
end
