--
-- Copyright (c) 2017 rxi
--
-- This library is free software; you can redistribute it and/or modify it
-- under the terms of the MIT license. See LICENSE for details.
--
-- The parts of love.math which are implemented in Lua
--

local lmath = love.math


local function checkVertices(name, ...)
  -- Accepts x1, y1, x2, y2, ... or a table of them
  local v = select("#", ...) == 1 and type(...) == "table" and ... or { ... }
  if #v % 2 ~= 0 then
    error(name .. ": number of vertex components must be a multiple of two", 3)
  end
  return v
end


function lmath.isConvex(...)
  local v = checkVertices("isConvex", ...)
  local n = #v / 2
  if n < 3 then return false end
  local sign
  for i = 0, n - 1 do
    local j, k = (i + 1) % n, (i + 2) % n
    local ax, ay = v[j * 2 + 1] - v[i * 2 + 1], v[j * 2 + 2] - v[i * 2 + 2]
    local bx, by = v[k * 2 + 1] - v[j * 2 + 1], v[k * 2 + 2] - v[j * 2 + 2]
    local cross = ax * by - ay * bx
    if cross ~= 0 then
      if sign == nil then
        sign = cross > 0
      elseif sign ~= (cross > 0) then
        return false
      end
    end
  end
  return true
end


function lmath.triangulate(...)
  -- Ear clipping; returns a table of triangles {x1, y1, x2, y2, x3, y3}
  local v = checkVertices("triangulate", ...)
  local n = #v / 2
  if n < 3 then
    error("triangulate: need at least 3 vertices to triangulate", 2)
  end
  if n == 3 then
    return { { v[1], v[2], v[3], v[4], v[5], v[6] } }
  end

  local idx = {}
  for i = 1, n do idx[i] = i end
  -- Make the polygon counter-clockwise (in y-up terms)
  local area = 0
  for i = 1, n do
    local j = i % n + 1
    area = area + v[i * 2 - 1] * v[j * 2] - v[j * 2 - 1] * v[i * 2]
  end
  if area < 0 then
    for i = 1, math.floor(n / 2) do idx[i], idx[n - i + 1] = idx[n - i + 1], idx[i] end
  end

  local function px(i) return v[idx[i] * 2 - 1] end
  local function py(i) return v[idx[i] * 2] end
  local function cross(ax, ay, bx, by, cx, cy)
    return (bx - ax) * (cy - ay) - (by - ay) * (cx - ax)
  end
  local function inside(x, y, ax, ay, bx, by, cx, cy)
    return cross(ax, ay, bx, by, x, y) >= 0 and cross(bx, by, cx, cy, x, y) >= 0
       and cross(cx, cy, ax, ay, x, y) >= 0
  end

  local triangles = {}
  local count = n
  local guard = 0
  local i = 1
  while count > 3 do
    local prev = (i - 2) % count + 1
    local next = i % count + 1
    local ax, ay, bx, by, cx, cy = px(prev), py(prev), px(i), py(i), px(next), py(next)
    local ear = cross(ax, ay, bx, by, cx, cy) > 0
    if ear then
      for j = 1, count do
        if j ~= prev and j ~= i and j ~= next and
           inside(px(j), py(j), ax, ay, bx, by, cx, cy) then
          ear = false
          break
        end
      end
    end
    if ear then
      triangles[#triangles + 1] = { ax, ay, bx, by, cx, cy }
      table.remove(idx, i)
      count = count - 1
      guard = 0
      if i > count then i = 1 end
    else
      i = i % count + 1
      guard = guard + 1
      if guard > count then
        error("triangulate: could not triangulate polygon", 2)
      end
    end
  end
  triangles[#triangles + 1] = { px(1), py(1), px(2), py(2), px(3), py(3) }
  return triangles
end


function lmath.colorToBytes(r, g, b, a)
  if type(r) == "table" then r, g, b, a = r[1], r[2], r[3], r[4] end
  local function c(x) return x and math.floor(math.min(math.max(x, 0), 1) * 255 + 0.5) end
  return c(r), c(g), c(b), c(a)
end


function lmath.colorFromBytes(r, g, b, a)
  if type(r) == "table" then r, g, b, a = r[1], r[2], r[3], r[4] end
  local function c(x) return x and math.min(math.max(x, 0), 255) / 255 end
  return c(r), c(g), c(b), c(a)
end


local function gammaToLinear(c)
  if c <= 0.04045 then return c / 12.92 end
  return ((c + 0.055) / 1.055) ^ 2.4
end

local function linearToGamma(c)
  if c <= 0.0031308 then return c * 12.92 end
  return 1.055 * c ^ (1 / 2.4) - 0.055
end

function lmath.gammaToLinear(r, g, b, a)
  if type(r) == "table" then r, g, b, a = r[1], r[2], r[3], r[4] end
  return gammaToLinear(r), g and gammaToLinear(g), b and gammaToLinear(b), a
end

function lmath.linearToGamma(r, g, b, a)
  if type(r) == "table" then r, g, b, a = r[1], r[2], r[3], r[4] end
  return linearToGamma(r), g and linearToGamma(g), b and linearToGamma(b), a
end


-- BezierCurve
local Bezier = { __index = {} }
local B = Bezier.__index

function lmath.newBezierCurve(...)
  local v = checkVertices("newBezierCurve", ...)
  if #v < 4 then
    error("newBezierCurve: a curve needs at least two control points", 2)
  end
  local pts = {}
  for i = 1, #v, 2 do pts[#pts + 1] = { v[i], v[i + 1] } end
  return setmetatable({ points = pts }, Bezier)
end

local function deCasteljau(pts, t)
  local x, y = {}, {}
  for i, p in ipairs(pts) do x[i], y[i] = p[1], p[2] end
  for k = #pts - 1, 1, -1 do
    for i = 1, k do
      x[i] = x[i] + (x[i + 1] - x[i]) * t
      y[i] = y[i] + (y[i + 1] - y[i]) * t
    end
  end
  return x[1], y[1]
end

function B:evaluate(t)
  if t < 0 or t > 1 then
    error("invalid evaluation parameter: must be between 0 and 1", 2)
  end
  return deCasteljau(self.points, t)
end

function B:getDegree() return #self.points - 1 end
function B:getControlPointCount() return #self.points end
function B:type() return "BezierCurve" end
function B:typeOf(name) return name == "BezierCurve" or name == "Object" end
function B:release() return true end

local function pointIndex(self, i)
  local n = #self.points
  if i < 0 then i = i + n + 1 end
  if i < 1 or i > n then error("invalid control point index", 3) end
  return i
end

function B:getControlPoint(i)
  local p = self.points[pointIndex(self, i)]
  return p[1], p[2]
end

function B:setControlPoint(i, x, y)
  self.points[pointIndex(self, i)] = { x, y }
end

function B:insertControlPoint(x, y, i)
  local n = #self.points
  i = i or -1
  if i < 0 then i = i + n + 2 end
  if i < 1 or i > n + 1 then error("invalid control point index", 2) end
  table.insert(self.points, i, { x, y })
end

function B:removeControlPoint(i)
  if #self.points <= 2 then
    error("a curve needs at least two control points", 2)
  end
  table.remove(self.points, pointIndex(self, i))
end

function B:getDerivative()
  local n = #self.points - 1
  if n < 1 then error("cannot derive a curve of degree < 1", 2) end
  local v = {}
  for i = 1, n do
    local a, b = self.points[i], self.points[i + 1]
    v[#v + 1] = (b[1] - a[1]) * n
    v[#v + 1] = (b[2] - a[2]) * n
  end
  if #v < 4 then v[3], v[4] = v[1], v[2] end
  return lmath.newBezierCurve(v)
end

function B:translate(dx, dy)
  for _, p in ipairs(self.points) do p[1], p[2] = p[1] + dx, p[2] + dy end
end

function B:scale(s, ox, oy)
  ox, oy = ox or 0, oy or 0
  for _, p in ipairs(self.points) do
    p[1], p[2] = (p[1] - ox) * s + ox, (p[2] - oy) * s + oy
  end
end

function B:rotate(angle, ox, oy)
  ox, oy = ox or 0, oy or 0
  local c, s = math.cos(angle), math.sin(angle)
  for _, p in ipairs(self.points) do
    local x, y = p[1] - ox, p[2] - oy
    p[1], p[2] = c * x - s * y + ox, s * x + c * y + oy
  end
end

function B:getSegment(t1, t2)
  if t1 < 0 or t2 > 1 or t1 >= t2 then
    error("invalid segment parameters: must be 0 <= t1 < t2 <= 1", 2)
  end
  -- Split at t2, then split the left part at t1 / t2
  local function split(pts, t)
    local left, right, cur = {}, {}, pts
    while #cur > 0 do
      left[#left + 1] = cur[1]
      table.insert(right, 1, cur[#cur])
      local nxt = {}
      for i = 1, #cur - 1 do
        nxt[i] = { cur[i][1] + (cur[i + 1][1] - cur[i][1]) * t,
                   cur[i][2] + (cur[i + 1][2] - cur[i][2]) * t }
      end
      cur = nxt
    end
    return left, right
  end
  local left = split(self.points, t2)
  local _, right = split(left, t1 / t2)
  local v = {}
  for _, p in ipairs(right) do v[#v + 1] = p[1] v[#v + 1] = p[2] end
  return lmath.newBezierCurve(v)
end

function B:render(depth)
  -- Subdivides the curve 2^depth times (default 5) and returns the points as
  -- {x1, y1, x2, y2, ...}
  depth = depth or 5
  local n = 2 ^ depth
  local v = {}
  for i = 0, n do
    local x, y = deCasteljau(self.points, i / n)
    v[#v + 1] = x
    v[#v + 1] = y
  end
  return v
end

function B:renderSegment(t1, t2, depth)
  return self:getSegment(t1, t2):render(depth)
end


-- Transform: a 2D affine transformation (the 3x3 matrix of LÖVE's 4x4
-- matrix which matters in 2D)
local Transform = { __index = {} }
local T = Transform.__index

local function newTransform(a, b, c, d, e, f)
  return setmetatable({ a or 1, b or 0, c or 0, d or 1, e or 0, f or 0 }, Transform)
end

-- Matrix [a c e; b d f; 0 0 1]
local function multiply(m, a, b, c, d, e, f)
  local a1, b1, c1, d1, e1, f1 = m[1], m[2], m[3], m[4], m[5], m[6]
  m[1] = a1 * a + c1 * b
  m[2] = b1 * a + d1 * b
  m[3] = a1 * c + c1 * d
  m[4] = b1 * c + d1 * d
  m[5] = a1 * e + c1 * f + e1
  m[6] = b1 * e + d1 * f + f1
  return m
end

function lmath.newTransform(x, y, angle, sx, sy, ox, oy, kx, ky)
  local t = newTransform()
  if x then t:setTransformation(x, y, angle, sx, sy, ox, oy, kx, ky) end
  return t
end

function T:reset() self[1], self[2], self[3], self[4], self[5], self[6] = 1, 0, 0, 1, 0, 0 return self end
function T:clone() return newTransform(unpack(self, 1, 6)) end
function T:translate(x, y) return multiply(self, 1, 0, 0, 1, x, y) end
function T:scale(sx, sy) sy = sy or sx return multiply(self, sx, 0, 0, sy, 0, 0) end
function T:shear(kx, ky) return multiply(self, 1, ky or 0, kx or 0, 1, 0, 0) end
function T:rotate(a)
  local c, s = math.cos(a), math.sin(a)
  return multiply(self, c, s, -s, c, 0, 0)
end
function T:apply(other) return multiply(self, unpack(other, 1, 6)) end
function T:type() return "Transform" end
function T:typeOf(name) return name == "Transform" or name == "Object" end
function T:release() return true end
function T:isAffine2DTransform() return true end

function T:setTransformation(x, y, angle, sx, sy, ox, oy, kx, ky)
  -- Same order as love.graphics.draw: translate, rotate, scale, shear, origin
  self:reset()
  sx = sx or 1
  self:translate(x or 0, y or 0)
  self:rotate(angle or 0)
  self:scale(sx, sy or sx)
  self:shear(kx or 0, ky or 0)
  self:translate(-(ox or 0), -(oy or 0))
  return self
end

function T:transformPoint(x, y)
  return self[1] * x + self[3] * y + self[5], self[2] * x + self[4] * y + self[6]
end

function T:inverse()
  local a, b, c, d, e, f = unpack(self, 1, 6)
  local det = a * d - b * c
  if det == 0 then error("transform is not invertible", 2) end
  return newTransform(d / det, -b / det, -c / det, a / det,
                      (c * f - d * e) / det, (b * e - a * f) / det)
end

function T:inverseTransformPoint(x, y)
  return self:inverse():transformPoint(x, y)
end

function T:getMatrix()
  -- Row-major 4x4 matrix as returned by LÖVE
  return self[1], self[3], 0, self[5],
         self[2], self[4], 0, self[6],
         0, 0, 1, 0,
         0, 0, 0, 1
end

function T:setMatrix(...)
  -- setMatrix(e1_1, e1_2, ..., e4_4) in row-major order, or with a leading
  -- layout ("row" or "column") or as a table
  local args = { ... }
  local layout = "row"
  if type(args[1]) == "string" then layout = table.remove(args, 1) end
  if type(args[1]) == "table" then
    local t = args[1]
    args = {}
    if type(t[1]) == "table" then
      for i = 1, 4 do for j = 1, 4 do args[#args + 1] = t[i][j] end end
    else
      args = t
    end
  end
  local function e(r, c)
    if layout == "column" then return args[(c - 1) * 4 + r] end
    return args[(r - 1) * 4 + c]
  end
  self[1], self[3], self[5] = e(1, 1), e(1, 2), e(1, 4)
  self[2], self[4], self[6] = e(2, 1), e(2, 2), e(2, 4)
  return self
end

Transform.__mul = function(a, b) return a:clone():apply(b) end
