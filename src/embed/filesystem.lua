--
-- Copyright (c) 2017 rxi
--
-- This library is free software; you can redistribute it and/or modify it
-- under the terms of the MIT license. See LICENSE for details.
--
-- The parts of love.filesystem which are implemented in Lua
--

local fs = love.filesystem


-- Save directory: "save" or, if an identity is set, "save/<identity>" with
-- the identity shortened to a valid DOS directory name. The directory is only
-- created (and mounted) once something is written to it
local identity
local saveDir = "save"
local saveDirMounted = false

local function setSaveDir(dir)
  if saveDirMounted then
    fs.unmount(saveDir)
    saveDirMounted = false
  end
  saveDir = dir
  if fs.mount(saveDir) then
    saveDirMounted = true
    fs.setWriteDir(saveDir)
  end
end

local function ensureSaveDir()
  if not saveDirMounted then
    fs.setWriteDir(saveDir)
    saveDirMounted = fs.mount(saveDir) and true or false
  end
end

function fs._init(id)
  -- Called by boot.lua with conf.lua's identity
  if id then
    fs.setIdentity(id)
  else
    setSaveDir(saveDir)
  end
  fs._init = nil
end

for _, name in ipairs { "write", "append", "remove", "createDirectory" } do
  local f = fs[name]
  fs[name] = function(...)
    ensureSaveDir()
    return f(...)
  end
end

function fs.setIdentity(name)
  if type(name) ~= "string" then
    error("bad argument #1 to 'setIdentity' (string expected)", 2)
  end
  identity = name
  local dosname = name:lower():gsub("[^%w_%-]", ""):sub(1, 8)
  setSaveDir(dosname ~= "" and ("save/" .. dosname) or "save")
end

function fs.getIdentity()
  return identity
end

function fs.getSaveDirectory()
  return fs.getWorkingDirectory() .. "/" .. saveDir
end

fs.getUserDirectory = fs.getWorkingDirectory
fs.getAppdataDirectory = fs.getWorkingDirectory


-- The game's source: set by boot.lua to the mounted path
local source, fused = nil, false

function fs._setSource(path, isFused)
  source, fused = path, isFused
  fs._setSource = nil
end

function fs.getSource()
  return source
end

function fs.getSourceBaseDirectory()
  local dir = source and source:match("^(.*)[/\\][^/\\]*$")
  return dir or fs.getWorkingDirectory()
end

function fs.isFused()
  return fused
end


-- LÖVE 0.10 functions
function fs.getSize(filename)
  local info = fs.getInfo(filename, "file")
  if not info then return nil, "file does not exist" end
  return info.size
end

function fs.getLastModified(filename)
  local info = fs.getInfo(filename)
  if not info then return nil, "file does not exist" end
  return info.modtime
end


function fs.lines(filename)
  local data = fs.read(filename)
  local pos = 1
  return function()
    if pos > #data then return nil end
    local s, e = data:find("\r?\n", pos)
    local line
    if s then
      line, pos = data:sub(pos, s - 1), e + 1
    else
      line, pos = data:sub(pos), #data + 1
    end
    return line
  end
end


function fs.load(filename)
  if not fs.isFile(filename) then
    return nil, "could not open file " .. filename
  end
  return loadstring(fs.read(filename), "=" .. filename)
end


local requirePath, cRequirePath = "?.lua;?/init.lua", "??"

function fs.getRequirePath()
  return requirePath
end

function fs.setRequirePath(path)
  requirePath = path
  package.path = path
end

function fs.getCRequirePath()
  return cRequirePath
end

function fs.setCRequirePath(path)
  -- C modules can't be loaded on DOS
  cRequirePath = path
end

function fs.areSymlinksEnabled() return false end
function fs.setSymlinksEnabled() end


-- File objects: files opened for reading are read into memory (which also
-- works for files in tar archives), files opened for writing or appending
-- are written to the save directory
local File = { __index = {} }
local F = File.__index

function fs.newFile(filename, mode)
  local self = setmetatable({ filename = filename, mode = "c" }, File)
  if mode and mode ~= "c" then
    local ok, err = self:open(mode)
    if not ok then return nil, err end
  end
  return self
end

function F:open(mode)
  if self.mode ~= "c" then
    return false, "file is already open"
  end
  if mode == "r" then
    if not fs.isFile(self.filename) then
      return false, "could not open file " .. self.filename
    end
    self.data = fs.read(self.filename)
    self.pos = 0
  elseif mode == "w" or mode == "a" then
    ensureSaveDir()
    local path = fs._getWriteDir() .. "/" .. self.filename
    self.fp = io.open(path, mode == "w" and "wb" or "ab")
    if not self.fp then
      return false, "could not open file " .. self.filename
    end
  else
    error("invalid file open mode '" .. tostring(mode) .. "'", 2)
  end
  self.mode = mode
  return true
end

function F:close()
  if self.mode == "c" then return false end
  if self.fp then self.fp:close() end
  self.fp, self.data, self.mode = nil, nil, "c"
  return true
end

function F:isOpen() return self.mode ~= "c" end
function F:getMode() return self.mode end
function F:getFilename() return self.filename end
function F:flush() if self.fp then self.fp:flush() end return true end
function F:setBuffering() return true end
function F:getBuffering() return "none" end
function F:type() return "File" end
function F:typeOf(name) return name == "File" or name == "Object" end
function F:release() self:close() return true end

function F:getSize()
  if self.data then return #self.data end
  if self.fp then
    local cur = self.fp:seek()
    local size = self.fp:seek("end")
    self.fp:seek("set", cur)
    return size
  end
  return fs.getSize(self.filename) or 0
end

function F:read(container, bytes)
  if type(container) == "number" then
    container, bytes = "string", container
  end
  if self.mode ~= "r" then
    error("file is not opened for reading", 2)
  end
  bytes = bytes or #self.data
  local s = self.data:sub(self.pos + 1, self.pos + bytes)
  self.pos = self.pos + #s
  return s, #s
end

function F:write(data, size)
  if not self.fp then
    error("file is not opened for writing", 2)
  end
  data = tostring(data)
  if size then data = data:sub(1, size) end
  return self.fp:write(data) and true or false
end

function F:lines()
  if self.mode ~= "r" then
    error("file is not opened for reading", 2)
  end
  return function()
    if self.pos >= #self.data then return nil end
    local s, e = self.data:find("\r?\n", self.pos + 1)
    local line
    if s then
      line, self.pos = self.data:sub(self.pos + 1, s - 1), e
    else
      line, self.pos = self.data:sub(self.pos + 1), #self.data
    end
    return line
  end
end

function F:seek(pos)
  if self.data then
    if pos < 0 or pos > #self.data then return false end
    self.pos = pos
    return true
  elseif self.fp then
    return self.fp:seek("set", pos) ~= nil
  end
  return false
end

function F:tell()
  if self.data then return self.pos end
  if self.fp then return self.fp:seek() end
  return nil, "file is not open"
end

function F:isEOF()
  if self.data then return self.pos >= #self.data end
  return not self.fp
end


-- FileData: file contents in memory
local FileData = { __index = {} }
local FD = FileData.__index

function fs.newFileData(contents, name)
  if name == nil then
    -- newFileData(filename)
    name = contents
    if not fs.isFile(name) then
      return nil, "could not open file " .. name
    end
    contents = fs.read(name)
  end
  return setmetatable({ data = contents, filename = name }, FileData)
end

function FD:getString() return self.data end
function FD:getSize() return #self.data end
function FD:getFilename() return self.filename end
function FD:getExtension() return self.filename:match("%.([^%./\\]*)$") or "" end
function FD:clone() return setmetatable({ data = self.data, filename = self.filename }, FileData) end
function FD:type() return "FileData" end
function FD:typeOf(name) return name == "FileData" or name == "Data" or name == "Object" end
function FD:release() return true end
