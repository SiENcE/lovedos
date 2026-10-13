local t = require("testlib")
local fs = love.filesystem

local function sorted(tbl) table.sort(tbl) return table.concat(tbl, ",") end

function love.load()
  -- clean up from previous runs
  for _, f in ipairs { "out.txt", "app.txt", "made/deep", "made", "w.txt" } do fs.remove(f) end

  local info = fs.getInfo("data/lines.txt")
  t.check("getInfo file", info and info.type == "file" and info.size == 29 and type(info.modtime) == "number")
  info = fs.getInfo("data")
  t.check("getInfo directory", info and info.type == "directory")
  t.check("getInfo missing", fs.getInfo("nope.txt") == nil)
  t.check("getInfo filter", fs.getInfo("data", "file") == nil and fs.getInfo("data", "directory") ~= nil)
  local reuse = {}
  t.check("getInfo reuses table", fs.getInfo("data/lines.txt", reuse) == reuse and reuse.size == 29)
  t.check("getSize / getLastModified", fs.getSize("data/lines.txt") == 29 and type(fs.getLastModified("data/lines.txt")) == "number")

  t.check("getDirectoryItems", sorted(fs.getDirectoryItems("data")) == "chunk.lua,lines.txt,sub")
  t.check("getDirectoryItems sub", sorted(fs.getDirectoryItems("data/sub")) == "inner.txt")
  t.check("getDirectoryItems missing", #fs.getDirectoryItems("nope") == 0)

  local lines = {}
  for l in fs.lines("data/lines.txt") do lines[#lines + 1] = l end
  t.check("lines", #lines == 3 and lines[1] == "line one" and lines[3] == "line three")
  local chunk = fs.load("data/chunk.lua")
  t.check("load", chunk and chunk() == 42)
  t.check("load missing", fs.load("nope.lua") == nil)

  -- writing goes to save/fstest
  t.check("write", fs.write("out.txt", "hello"))
  t.check("append", fs.append("app.txt", "ab") and fs.append("app.txt", "cd") and fs.read("app.txt") == "abcd")
  t.check("createDirectory", fs.createDirectory("made/deep") and fs.getInfo("made/deep").type == "directory")
  t.check("save dir listed", sorted(fs.getDirectoryItems("")):find("out.txt", 1, true) ~= nil)
  t.check("getRealDirectory", fs.getRealDirectory("out.txt") == "save/fstest" and fs.getRealDirectory("data/lines.txt") == ".")
  t.check("remove", fs.remove("made/deep") and fs.remove("made") and fs.getInfo("made") == nil)
  t.check("remove missing", not fs.remove("nope.txt"))
  t.check("getSaveDirectory", fs.getSaveDirectory():lower():find("save/fstest", 1, true) ~= nil)
  t.check("getWorkingDirectory", type(fs.getWorkingDirectory()) == "string")
  t.check("getSource", fs.getSource() == "." and not fs.isFused())
  t.check("require path", fs.getRequirePath() == "?.lua;?/init.lua")

  -- File objects
  local f = fs.newFile("data/lines.txt")
  t.check("newFile closed", not f:isOpen() and f:getMode() == "c" and f:getFilename() == "data/lines.txt")
  t.check("File open r", f:open("r") and f:isOpen() and f:getSize() == 29)
  t.check("File read n", f:read(4) == "line" and f:tell() == 4)
  t.check("File seek", f:seek(0) and f:read() == fs.read("data/lines.txt") and f:isEOF())
  f:seek(0)
  local n = 0 for l in f:lines() do n = n + 1 end
  t.check("File lines", n == 3)
  t.check("File close", f:close() and not f:isOpen())
  local w = fs.newFile("w.txt", "w")
  t.check("File write", w and w:write("abc") and w:write("defgh", 2) and w:close())
  t.check("File written", fs.read("w.txt") == "abcde")
  local a = fs.newFile("w.txt") a:open("a") a:write("!") a:close()
  t.check("File append", fs.read("w.txt") == "abcde!")
  t.check("File missing", fs.newFile("nope.txt", "r") == nil)
  t.check("File type", f:type() == "File" and f:typeOf("Object"))

  local fd = fs.newFileData("data/chunk.lua")
  t.check("FileData", fd:getString() == "return 6 * 7\n" and fd:getSize() == 13 and fd:getExtension() == "lua")
  local fd2 = fs.newFileData("contents", "name.txt")
  t.check("FileData from string", fd2:getString() == "contents" and fd2:getFilename() == "name.txt")

  -- tar archives: pack a directory and list it through a mount
  t.check("tar mount", fs.mount("pack.tar"))
  t.check("tar getDirectoryItems", sorted(fs.getDirectoryItems("tdir")) == "a.txt,b")
  local ti = fs.getInfo("tdir/a.txt")
  t.check("tar getInfo", ti and ti.type == "file" and ti.size == 3)
  fs.unmount("pack.tar")
  t.finish()
end
