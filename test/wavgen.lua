-- Writes sine wave .wav files to the save directory for the audio tests
local wavgen = {}

local function u16(n) return string.char(n % 256, math.floor(n / 256) % 256) end
local function u32(n) return u16(n % 65536) .. u16(math.floor(n / 65536)) end

function wavgen.write(filename, rate, bits, freqs, duration)
  -- `freqs` has one frequency per channel; a frequency can also be a
  -- function of the time in seconds
  local channels = #freqs
  local frames = math.floor(rate * duration)
  local parts = {}
  for i = 0, frames - 1 do
    for c = 1, channels do
      local f = freqs[c]
      if type(f) == "function" then f = f(i / rate) end
      local v = 0.5 * math.sin(i / rate * f * 2 * math.pi)
      if bits == 16 then
        local n = math.floor(v * 32767 + 0.5)
        if n < 0 then n = n + 65536 end
        parts[#parts + 1] = u16(n)
      else
        parts[#parts + 1] = string.char(math.floor(v * 127 + 128.5))
      end
    end
  end
  local data = table.concat(parts)
  local header = "RIFF" .. u32(36 + #data) .. "WAVEfmt " .. u32(16) .. u16(1) ..
    u16(channels) .. u32(rate) .. u32(rate * channels * bits / 8) ..
    u16(channels * bits / 8) .. u16(bits) .. "data" .. u32(#data)
  love.filesystem.write(filename, header .. data)
end

return wavgen
