--
-- Copyright (c) 2017 rxi
--
-- This library is free software; you can redistribute it and/or modify it
-- under the terms of the MIT license. See LICENSE for details.
--
-- love.data: encoding, ByteData and packing of binary data. Compression and
-- hashing are not provided as they are too slow for DOS machines
--

local data = {}
love.data = data


local ByteData = { __index = {} }
local B = ByteData.__index

function data.newByteData(source)
  -- newByteData(string) or newByteData(size)
  if type(source) == "number" then
    source = string.rep("\0", source)
  elseif type(source) == "table" and source.getString then
    source = source:getString()
  end
  return setmetatable({ data = tostring(source) }, ByteData)
end

function B:getString() return self.data end
function B:getSize() return #self.data end
function B:clone() return data.newByteData(self.data) end
function B:getPointer() return nil end
function B:getFFIPointer() return nil end
function B:type() return "ByteData" end
function B:typeOf(name) return name == "ByteData" or name == "Data" or name == "Object" end
function B:release() return true end


local function toString(s)
  if type(s) == "table" and s.getString then return s:getString() end
  return tostring(s)
end

local function result(container, s)
  if container == "data" then return data.newByteData(s) end
  return s
end


local b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"
local b64index = {}
for i = 1, 64 do b64index[b64:sub(i, i)] = i - 1 end

function data.encode(container, format, source, linelength)
  local s = toString(source)
  local out = {}
  if format == "hex" then
    for i = 1, #s do out[i] = string.format("%02x", s:byte(i)) end
    out = table.concat(out)
  elseif format == "base64" then
    for i = 1, #s, 3 do
      local a, b, c = s:byte(i, i + 2)
      local n = a * 65536 + (b or 0) * 256 + (c or 0)
      local c1 = math.floor(n / 262144) % 64
      local c2 = math.floor(n / 4096) % 64
      local c3 = math.floor(n / 64) % 64
      local c4 = n % 64
      out[#out + 1] = b64:sub(c1 + 1, c1 + 1) .. b64:sub(c2 + 1, c2 + 1) ..
        (b and b64:sub(c3 + 1, c3 + 1) or "=") .. (c and b64:sub(c4 + 1, c4 + 1) or "=")
    end
    out = table.concat(out)
    if linelength and linelength > 0 then
      local lines = {}
      for i = 1, #out, linelength do lines[#lines + 1] = out:sub(i, i + linelength - 1) end
      out = table.concat(lines, "\n")
    end
  else
    error("invalid encode format '" .. tostring(format) .. "'", 2)
  end
  return result(container, out)
end

function data.decode(container, format, source)
  local s = toString(source)
  local out = {}
  if format == "hex" then
    s = s:gsub("%s", "")
    for i = 1, #s - 1, 2 do
      out[#out + 1] = string.char(tonumber(s:sub(i, i + 1), 16) or 0)
    end
  elseif format == "base64" then
    s = s:gsub("[^%w%+/=]", "")
    for i = 1, #s, 4 do
      local n, pad = 0, 0
      for j = 0, 3 do
        local ch = s:sub(i + j, i + j)
        if ch == "=" or ch == "" then pad = pad + 1 end
        n = n * 64 + (b64index[ch] or 0)
      end
      local bytes = string.char(math.floor(n / 65536) % 256,
                                math.floor(n / 256) % 256, n % 256)
      out[#out + 1] = bytes:sub(1, 3 - pad)
    end
  else
    error("invalid decode format '" .. tostring(format) .. "'", 2)
  end
  return result(container, table.concat(out))
end


-- pack / unpack: the formats of Lua 5.3's string.pack. Supported are
-- < > = ! (endianness and alignment), b B h H l L j J T i[n] I[n] (integers,
-- up to 4 bytes exactly), f d n (floats), s[n] z x and spaces
local function parseFormat(fmt)
  local items, i, little = {}, 1, true
  while i <= #fmt do
    local c = fmt:sub(i, i)
    local n = fmt:match("^%d+", i + 1)
    i = i + 1 + (n and #n or 0)
    n = tonumber(n)
    if c == "<" or c == "=" then little = true
    elseif c == ">" then little = false
    elseif c == "!" or c == " " then -- alignment is ignored
    elseif c == "b" or c == "B" then items[#items + 1] = { "int", 1, c == "b", little }
    elseif c == "h" or c == "H" then items[#items + 1] = { "int", 2, c == "h", little }
    elseif c == "i" or c == "I" then items[#items + 1] = { "int", n or 4, c == "i", little }
    elseif c == "l" or c == "L" or c == "j" or c == "J" or c == "T" then
      items[#items + 1] = { "int", 8, c == "l" or c == "j", little }
    elseif c == "f" then items[#items + 1] = { "float", 4, false, little }
    elseif c == "d" or c == "n" then items[#items + 1] = { "float", 8, false, little }
    elseif c == "s" then items[#items + 1] = { "string", n or 8, false, little }
    elseif c == "z" then items[#items + 1] = { "zstring", 0 }
    elseif c == "x" then items[#items + 1] = { "pad", 1 }
    else error("invalid format option '" .. c .. "'", 3) end
  end
  return items
end

local function packInt(v, size, little)
  v = math.floor(v)
  if v < 0 then v = v + 2 ^ (size * 8) end
  local bytes = {}
  for i = 1, size do
    bytes[i] = v % 256
    v = math.floor(v / 256)
  end
  if not little then
    for i = 1, math.floor(size / 2) do bytes[i], bytes[size - i + 1] = bytes[size - i + 1], bytes[i] end
  end
  return string.char(unpack(bytes))
end

local function unpackInt(s, pos, size, signed, little)
  local v = 0
  for i = 0, size - 1 do
    local byte = s:byte(pos + (little and size - 1 - i or i))
    v = v * 256 + byte
  end
  if signed and v >= 2 ^ (size * 8 - 1) then v = v - 2 ^ (size * 8) end
  return v
end

local function packFloat(v, size, little)
  -- IEEE 754 single (size 4) or double (size 8) precision
  local ebits, mbits = size == 4 and 8 or 11, size == 4 and 23 or 52
  local bias = 2 ^ (ebits - 1) - 1
  local sign = (v < 0 or 1 / v < 0) and 1 or 0
  v = math.abs(v)
  local e, m
  if v ~= v then e, m = 2 ^ ebits - 1, 1
  elseif v == math.huge then e, m = 2 ^ ebits - 1, 0
  elseif v == 0 then e, m = 0, 0
  else
    local fm, fe = math.frexp(v)
    e = fe + bias - 1
    if e <= 0 then
      m = math.floor(v / 2 ^ (1 - bias) * 2 ^ mbits + 0.5)
      e = 0
    else
      m = math.floor((fm * 2 - 1) * 2 ^ mbits + 0.5)
      if m == 2 ^ mbits then m, e = 0, e + 1 end
      if e >= 2 ^ ebits - 1 then e, m = 2 ^ ebits - 1, 0 end
    end
  end
  local bytes = {}
  local hi = math.floor(e)
  -- Mantissa bytes (low first), then the top mantissa bits with the exponent
  local total = mbits + ebits + 1
  local bits = {}
  for i = 1, mbits do bits[i] = m % 2 m = math.floor(m / 2) end
  for i = 1, ebits do bits[mbits + i] = hi % 2 hi = math.floor(hi / 2) end
  bits[total] = sign
  for i = 1, size do
    local b = 0
    for j = 8, 1, -1 do b = b * 2 + bits[(i - 1) * 8 + j] end
    bytes[i] = b
  end
  if not little then
    for i = 1, size / 2 do bytes[i], bytes[size - i + 1] = bytes[size - i + 1], bytes[i] end
  end
  return string.char(unpack(bytes))
end

local function unpackFloat(s, pos, size, little)
  local ebits, mbits = size == 4 and 8 or 11, size == 4 and 23 or 52
  local bias = 2 ^ (ebits - 1) - 1
  local bits = {}
  for i = 1, size do
    local b = s:byte(pos + (little and i - 1 or size - i))
    for j = 1, 8 do bits[(i - 1) * 8 + j] = b % 2 b = math.floor(b / 2) end
  end
  local m, e = 0, 0
  for i = mbits, 1, -1 do m = m * 2 + bits[i] end
  for i = mbits + ebits, mbits + 1, -1 do e = e * 2 + bits[i] end
  local sign = bits[size * 8] == 1 and -1 or 1
  if e == 2 ^ ebits - 1 then
    return m == 0 and sign * math.huge or 0 / 0
  elseif e == 0 then
    return sign * m * 2 ^ (1 - bias - mbits)
  end
  return sign * (1 + m / 2 ^ mbits) * 2 ^ (e - bias)
end

function data.pack(container, fmt, ...)
  local items, out, arg = parseFormat(fmt), {}, 1
  local args = { ... }
  for _, it in ipairs(items) do
    local kind, size, _, little = it[1], it[2], it[3], it[4]
    if kind == "pad" then
      out[#out + 1] = "\0"
    else
      local v = args[arg]
      arg = arg + 1
      if kind == "int" then out[#out + 1] = packInt(v, size, little)
      elseif kind == "float" then out[#out + 1] = packFloat(v, size, little)
      elseif kind == "string" then out[#out + 1] = packInt(#v, size, little) .. v
      elseif kind == "zstring" then out[#out + 1] = v .. "\0" end
    end
  end
  return result(container, table.concat(out))
end

function data.unpack(fmt, source, pos)
  local s = toString(source)
  local items, values = parseFormat(fmt), {}
  pos = pos or 1
  for _, it in ipairs(items) do
    local kind, size, signed, little = it[1], it[2], it[3], it[4]
    if kind == "pad" then
      pos = pos + 1
    elseif kind == "int" then
      values[#values + 1] = unpackInt(s, pos, size, signed, little)
      pos = pos + size
    elseif kind == "float" then
      values[#values + 1] = unpackFloat(s, pos, size, little)
      pos = pos + size
    elseif kind == "string" then
      local len = unpackInt(s, pos, size, false, little)
      values[#values + 1] = s:sub(pos + size, pos + size + len - 1)
      pos = pos + size + len
    elseif kind == "zstring" then
      local e = s:find("\0", pos, true) or #s + 1
      values[#values + 1] = s:sub(pos, e - 1)
      pos = e + 1
    end
  end
  values[#values + 1] = pos
  return unpack(values)
end

function data.getPackedSize(fmt)
  local size = 0
  for _, it in ipairs(parseFormat(fmt)) do
    if it[1] == "string" or it[1] == "zstring" then
      error("variable-length format in getPackedSize", 2)
    end
    size = size + it[2]
  end
  return size
end
