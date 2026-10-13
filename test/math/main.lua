local t = require("testlib")
local m = love.math

function love.load()
  -- RandomGenerator
  local a, b = m.newRandomGenerator(1234), m.newRandomGenerator(1234)
  local same = true
  for i = 1, 100 do if a:random() ~= b:random() then same = false end end
  t.check("same seed, same sequence", same)
  local c = m.newRandomGenerator(1235)
  t.check("different seed, different sequence", c:random() ~= m.newRandomGenerator(1234):random())
  local inRange, ints, hit1, hit6 = true, true, false, false
  for i = 1, 2000 do
    local r = a:random()
    if r < 0 or r >= 1 then inRange = false end
    local d = a:random(6)
    if d ~= math.floor(d) or d < 1 or d > 6 then ints = false end
    hit1, hit6 = hit1 or d == 1, hit6 or d == 6
    local e = a:random(-3, 3)
    if e ~= math.floor(e) or e < -3 or e > 3 then ints = false end
  end
  t.check("random() in [0, 1)", inRange)
  t.check("random(max), random(min, max) integers in range", ints and hit1 and hit6)
  local st = a:getState()
  local x1 = a:random()
  a:setState(st)
  t.check("get/setState", type(st) == "string" and st:sub(1, 2) == "0x" and a:random() == x1)
  local lo, hi = m.newRandomGenerator(5, 7):getSeed()
  t.check("seed low/high", lo == 5 and hi == 7)
  local cl = a:clone()
  t.check("clone", cl:random() == a:random())
  local n, sum, sq = 4000, 0, 0
  for i = 1, n do local v = a:randomNormal(2, 10) sum = sum + v sq = sq + v * v end
  local mean = sum / n
  local sd = math.sqrt(sq / n - mean * mean)
  t.check("randomNormal mean/stddev (" .. string.format("%.2f %.2f", mean, sd) .. ")",
          math.abs(mean - 10) < 0.2 and math.abs(sd - 2) < 0.2)
  t.check("seed 0 allowed", m.newRandomGenerator(0):random() >= 0)
  t.check("default generator seeded", type(m.random()) == "number")
  m.setRandomSeed(42)
  local r1 = m.random()
  m.setRandomSeed(42)
  t.check("setRandomSeed", m.random() == r1 and select(1, m.getRandomSeed()) == 42)
  local s = m.getRandomState() m.random() m.setRandomState(s)
  t.check("get/setRandomState", m.getRandomState() == s)
  t.check("bad state", t.errors(m.setRandomState, "nonsense"))

  -- noise
  local ok, varied = true, false
  local first = m.noise(0.5)
  for i = 1, 200 do
    for _, v in ipairs { m.noise(i * 0.37), m.noise(i * 0.37, i * 0.11),
                         m.noise(i * 0.37, i * 0.11, i * 0.05), m.noise(i * 0.37, 1, 2, i * 0.07) } do
      if v < 0 or v > 1 then ok = false end
      if v ~= first then varied = true end
    end
  end
  t.check("noise in [0, 1] and varies", ok and varied)
  t.check("noise deterministic", m.noise(1.5, 2.5) == m.noise(1.5, 2.5))
  t.check("noise integer points", math.abs(m.noise(3) - 0.5) < 1e-9)

  -- geometry
  t.check("isConvex square", m.isConvex(0, 0, 10, 0, 10, 10, 0, 10))
  t.check("isConvex concave", not m.isConvex({ 0, 0, 10, 0, 5, 2, 10, 10, 0, 10 }))
  local function area(tris)
    local s = 0
    for _, tr in ipairs(tris) do
      s = s + math.abs((tr[3] - tr[1]) * (tr[6] - tr[2]) - (tr[5] - tr[1]) * (tr[4] - tr[2])) / 2
    end
    return s
  end
  local tris = m.triangulate(0, 0, 30, 0, 30, 30, 20, 30, 20, 10, 10, 10, 10, 30, 0, 30)
  t.check("triangulate concave U (" .. #tris .. " tris, area " .. area(tris) .. ")", #tris == 6 and area(tris) == 700)
  t.check("triangulate clockwise", area(m.triangulate({ 0, 0, 0, 10, 10, 10, 10, 0 })) == 100)

  -- BezierCurve
  local bc = m.newBezierCurve(0, 0, 10, 20, 20, 0)
  local x, y = bc:evaluate(0.5)
  t.check("bezier evaluate", x == 10 and y == 10)
  t.check("bezier degree", bc:getDegree() == 2 and bc:getControlPointCount() == 3)
  local pts = bc:render(3)
  t.check("bezier render", #pts == 18 and pts[1] == 0 and pts[17] == 20)
  local seg = bc:getSegment(0, 0.5)
  local sx, sy = seg:evaluate(1)
  t.check("bezier segment", math.abs(sx - 10) < 1e-9 and math.abs(sy - 10) < 1e-9)
  local d = bc:getDerivative()
  local dx, dy = d:evaluate(0)
  t.check("bezier derivative", dx == 20 and dy == 40)
  bc:translate(5, 5) bc:insertControlPoint(30, 30) bc:setControlPoint(1, 1, 1)
  t.check("bezier edit", select(1, bc:getControlPoint(-1)) == 30 and select(1, bc:getControlPoint(1)) == 1)

  -- Transform
  local tf = m.newTransform(10, 20, math.pi / 2, 2, 2, 1, 0)
  local px, py = tf:transformPoint(1, 0)
  t.check("transform origin maps to position", math.abs(px - 10) < 1e-9 and math.abs(py - 20) < 1e-9)
  px, py = tf:transformPoint(2, 0)
  t.check("transform rotate+scale", math.abs(px - 10) < 1e-9 and math.abs(py - 22) < 1e-9)
  local ix, iy = tf:inverseTransformPoint(10, 22)
  t.check("inverse", math.abs(ix - 2) < 1e-9 and math.abs(iy) < 1e-9)
  local tt = m.newTransform():translate(5, 0) * m.newTransform():scale(2)
  px = tt:transformPoint(1, 0)
  t.check("transform multiply", px == 7)
  t.check("getMatrix", select(4, m.newTransform(3, 4):getMatrix()) == 3)

  -- colors
  local r, g, bb, aa = m.colorToBytes(1, 0.5, 0, 1)
  t.check("colorToBytes", r == 255 and g == 128 and bb == 0 and aa == 255)
  r = m.colorFromBytes(51)
  t.check("colorFromBytes", math.abs(r - 0.2) < 1e-9)
  t.check("gamma round trip", math.abs(m.linearToGamma(m.gammaToLinear(0.5)) - 0.5) < 1e-9)
  t.finish()
end
