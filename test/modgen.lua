-- Writes small ProTracker MOD files for the tests (copied into each test's
-- directory). A song is a table:
--   channels  number of channels (4 is written as "M.K.", others as "xCHN")
--   samples   list of { data = <8bit signed string>, volume = 0-64,
--                       loop = true }
--   patterns  list of patterns; a pattern is indexed by row (0-63) and then
--             by channel (1-n): { period, sample, effect, param }
--   order     list of 0-based pattern numbers (default { 0 })
local modgen = {}

local function u16be(n)
  return string.char(math.floor(n / 256) % 256, n % 256)
end

local function u16le(n)
  return string.char(n % 256, math.floor(n / 256) % 256)
end

local function u32le(n)
  return u16le(n % 65536) .. u16le(math.floor(n / 65536))
end

local function pad(s, n)
  s = s:sub(1, n)
  return s .. string.rep("\0", n - #s)
end

function modgen.sine(length, cycles, amplitude)
  -- A signed 8bit sine wave sample
  local t = {}
  for i = 0, length - 1 do
    local v = math.floor(amplitude * math.sin(i / length * cycles * 2 * math.pi) + 0.5)
    t[#t + 1] = string.char(v % 256)
  end
  return table.concat(t)
end

function modgen.write(filename, song)
  local channels = song.channels or 4
  local order = song.order or { 0 }
  local out = { pad(song.name or "test", 20) }
  for i = 1, 31 do
    local s = song.samples[i]
    if s then
      local words = #s.data / 2
      out[#out + 1] = pad("sample" .. i, 22) .. u16be(words) .. "\0" ..
        string.char(s.volume or 64) .. u16be(0) .. u16be(s.loop and words or 1)
    else
      out[#out + 1] = string.rep("\0", 30)
    end
  end
  local numPatterns = 0
  for _, p in ipairs(order) do numPatterns = math.max(numPatterns, p + 1) end
  local orderTable = {}
  for i = 1, 128 do orderTable[i] = string.char(order[i] or 0) end
  out[#out + 1] = string.char(#order, 127) .. table.concat(orderTable)
  out[#out + 1] = channels == 4 and "M.K." or (channels .. "CHN")
  for p = 1, numPatterns do
    local pattern = song.patterns[p] or {}
    for row = 0, 63 do
      for c = 1, channels do
        local n = pattern[row] and pattern[row][c]
        if n then
          local period, sample, effect, param = n[1] or 0, n[2] or 0, n[3] or 0, n[4] or 0
          out[#out + 1] = string.char(
            math.floor(sample / 16) * 16 + math.floor(period / 256),
            period % 256, (sample % 16) * 16 + effect, param)
        else
          out[#out + 1] = "\0\0\0\0"
        end
      end
    end
  end
  for i = 1, 31 do
    if song.samples[i] then out[#out + 1] = song.samples[i].data end
  end
  love.filesystem.write(filename, table.concat(out))
end

function modgen.writeXM(filename, song)
  -- A FastTracker 2 module with one pattern of `song.rows` rows and one
  -- instrument with a 16bit sample (`song.sample`, a list of values from
  -- -32768 to 32767, looped). `song.notes` is indexed by row (0-based) and
  -- channel (1-n): { key, instrument, volume, effect, param }
  local channels, rows = song.channels or 2, song.rows or 64
  local out = { "Extended Module: ", pad("test", 20), "\26", pad("modgen", 20),
                u16le(0x0104), u32le(276), u16le(1), u16le(0), u16le(channels),
                u16le(1), u16le(1), u16le(1), u16le(6), u16le(125),
                string.rep("\0", 256) }
  local notes = {}
  for row = 0, rows - 1 do
    for c = 1, channels do
      local n = song.notes[row] and song.notes[row][c] or {}
      notes[#notes + 1] = string.char(n[1] or 0, n[2] or 0, n[3] or 0, n[4] or 0, n[5] or 0)
    end
  end
  notes = table.concat(notes)
  out[#out + 1] = u32le(9) .. "\0" .. u16le(rows) .. u16le(#notes) .. notes
  -- Instrument header (263 bytes), envelopes disabled
  out[#out + 1] = u32le(263) .. pad("instrument", 22) .. "\0" .. u16le(1) ..
    u32le(40) .. string.rep("\0", 96 + 48 + 48 + 8 + 2 + 4) .. u16le(0) ..
    string.rep("\0", 22)
  -- Sample header and delta encoded data
  local data, last = {}, 0
  for i, v in ipairs(song.sample) do
    local d = (v - last) % 65536
    data[i] = u16le(d)
    last = v
  end
  local bytes = #song.sample * 2
  out[#out + 1] = u32le(bytes) .. u32le(0) .. u32le(bytes) .. "\64\0" ..
    string.char(0x11, 128, 0, 0) .. pad("sample", 22)
  out[#out + 1] = table.concat(data)
  love.filesystem.write(filename, table.concat(out))
end

return modgen
