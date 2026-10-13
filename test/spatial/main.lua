local t0 = require("testlib")
local wavgen = require("wavgen")
local out, check, errs = t0.log, t0.check, t0.errors

local errs = t0.errors
local a = love.audio
local src, st
local events, t = {}, 0
local function at(tt, f) table.insert(events, { t = tt, f = f }) end
function love.load()
  wavgen.write("tone440.wav", 22050, 16, { 440 }, 1)
  wavgen.write("tone660.wav", 22050, 16, { 660 }, 1)
  wavgen.write("stereo.wav", 44100, 16, { 440, 660 }, 1)
  wavgen.write("tone8.wav", 11025, 8, { 300 }, 0.5)
  src = a.newSource("tone440.wav") src:setLooping(true)
  st = a.newSource("stereo.wav")
  check("defaults", a.getDistanceModel() == "inverseclamped" and a.getDopplerScale() == 1)
  local fx, fy, fz, ux, uy, uz = a.getOrientation()
  check("orientation default", fz == -1 and uy == 1)
  check("source defaults", select(1, src:getAttenuationDistances()) == 1 and src:getRolloff() == 1 and not src:isRelative())
  check("stereo rejects position", errs(st.setPosition, st, 1, 0, 0))
  check("bad model", errs(a.setDistanceModel, "loud"))
  a.setDistanceModel("linear clamped") check("love11 name", a.getDistanceModel() == "linearclamped")
  a.setDistanceModel("inverseclamped")
  check("bad cone", errs(src.setCone, src, 2, 1))
  local c = src:clone() src:setPosition(1, 2, 3) local c2 = src:clone()
  check("clone copies position", select(3, c2:getPosition()) == 3 and select(3, c:getPosition()) == 0)
  src:setPosition(0, 0, 0)
  at(1, function() src:play() out("A center full") end)
  at(2, function() src:setPosition(4, 0, 0) out("B right 0.25") end)
  at(3, function() a.setPosition(4, 0, 0) out("C listener follows: center full") end)
  at(4, function() a.setDistanceModel("none") src:setPosition(-6, 0, 0) out("D model none, left full") end)
  at(5, function() src:setVelocity(34.33, 0, 0) out("E doppler approaching 488.9Hz") end)
  at(6, function() src:setVelocity(0, 0, 0) src:setRelative(true) src:setPosition(0, 0, -3) a.setDistanceModel("inverse") out("F relative ahead, 1/3") end)
  at(7, function() a.setOrientation(1, 0, 0, 0, 1, 0) src:setRelative(false) a.setPosition(0, 0, 0) src:setPosition(0, 0, 2) out("G listener faces +x, source at +z: right 0.5") end)
  at(8, function() src:stop() t0.finish() end)
end
function love.update(dt)
  t = t + dt
  while events[1] and t >= events[1].t do table.remove(events, 1).f() end
end
