local t = require("testlib")
local wavgen = require("wavgen")
local a = love.audio
local events, time = {}, 0
local function at(tt, f) events[#events + 1] = { t = tt, f = f } end
local big, q

function love.load()
  -- 8 seconds (352KB): 440Hz for 4 seconds, then 880Hz
  wavgen.write("big.wav", 22050, 16, { function(s) return s < 4 and 440 or 880 end }, 8)
  wavgen.write("small.wav", 22050, 16, { 440 }, 0.5)

  big = a.newSource("big.wav", "stream")
  t.check("streamed source", big:getType() == "stream" and t.near(big:getDuration(), 8, 1e-3)
          and big:getDuration("samples") == 176400 and big:getChannelCount() == 1)
  t.check("small stream file in memory", a.newSource("small.wav", "stream"):getDuration("samples") == 11025)
  t.check("clone streamed", big:clone():getType() == "stream")

  local d = love.sound.newDecoder("big.wav", 8192)
  t.check("decoder info", d:getSampleRate() == 22050 and d:getBitDepth() == 16 and d:getChannelCount() == 1
          and t.near(d:getDuration(), 8, 1e-3))
  local total, chunks, first = 0, 0
  while true do
    local sd = d:decode()
    if not sd then break end
    first = first or sd
    total = total + sd:getSampleCount()
    chunks = chunks + 1
  end
  t.check("decode all (" .. chunks .. " chunks)", total == 176400 and chunks == 44 and first:getSampleCount() == 4096)
  d:seek(1)
  t.check("decoder seek/tell", t.near(d:tell(), 1, 1e-4) and d:decode():getSampleCount() == 4096)
  t.check("source from decoder", a.newSource(d):getType() == "stream")
  t.check("missing file", t.errors(love.sound.newDecoder, "nope.wav"))

  q = a.newQueueableSource(22050, 16, 1, 3)
  t.check("queueable", q:getType() == "queue" and q:getFreeBufferCount() == 3 and q:getDuration() == -1)
  local chunk = love.sound.newSoundData(11025, 22050, 16, 1)
  for i = 0, 11024 do chunk:setSample(i, 0.5 * math.sin(i / 22050 * 660 * 2 * math.pi)) end
  t.check("queue", q:queue(chunk) and q:queue(chunk) and q:queue(chunk) and q:getFreeBufferCount() == 0)
  t.check("queue full", q:queue(chunk) == false)
  t.check("queue format", t.errors(q.queue, q, love.sound.newSoundData(10, 44100, 16, 1)))
  t.check("queueable can't be cloned", t.errors(q.clone, q))
  t.check("only queueable sources queue", t.errors(big.queue, big, chunk))

  at(0.5, function() big:play() t.log("A stream 440Hz") end)
  at(1.5, function() t.check("stream plays", big:isPlaying() and big:tell() > 0.8 and big:tell() < 1.3) end)
  at(2.0, function() big:seek(6) t.log("B seek 6: 880Hz") end)
  at(2.5, function() t.check("seek while playing", big:tell() > 6.3 and big:tell() < 6.7) end)
  at(3.5, function() big:stop() t.check("stop rewinds", big:tell() == 0 and not big:isPlaying()) q:play() t.log("C queue 660Hz 1.5s") end)
  at(4.2, function() t.check("buffers consumed", q:getFreeBufferCount() >= 1) end)
  at(5.5, function() t.check("queue ran dry and stopped", not q:isPlaying() and q:getFreeBufferCount() == 3) end)
  at(5.6, function() big:seek(3.5) big:play() t.log("D stream from 3.5s: 440 then 880") end)
  at(7.0, function() big:stop() t.finish() end)
end

function love.update(dt)
  time = time + dt
  while events[1] and time >= events[1].t do table.remove(events, 1).f() end
end
