--[[
	Minimal Roblox API stub used by the offline test harness.

	It only implements what the game's shared modules touch, but it implements it
	faithfully enough that logic bugs (not API mismatches) are what the tests find.
]]

local function class(name, fields)
	local theClass = {}
	theClass.__index = theClass
	theClass.__typename = name
	function theClass.new(...)
		local self = setmetatable({}, theClass)
		self.ClassName = name
		if theClass.__init then
			theClass.__init(self, ...)
		end
		return self
	end
	return theClass
end

-- Vector3 -------------------------------------------------------------------
local Vector3 = {}
Vector3.__index = Vector3
function Vector3.new(x, y, z)
	return setmetatable({ X = x or 0, Y = y or 0, Z = z or 0 }, Vector3)
end
Vector3.zero = Vector3.new(0, 0, 0)
Vector3.one = Vector3.new(1, 1, 1)
Vector3.__add = function(a, b) return Vector3.new(a.X + b.X, a.Y + b.Y, a.Z + b.Z) end
Vector3.__sub = function(a, b) return Vector3.new(a.X - b.X, a.Y - b.Y, a.Z - b.Z) end
Vector3.__mul = function(a, b)
	if type(b) == "number" then return Vector3.new(a.X * b, a.Y * b, a.Z * b) end
	if type(a) == "number" then return Vector3.new(b.X * a, b.Y * a, b.Z * a) end
	return Vector3.new(a.X * b.X, a.Y * b.Y, a.Z * b.Z)
end
Vector3.__div = function(a, b) return Vector3.new(a.X / b, a.Y / b, a.Z / b) end
Vector3.__unm = function(a) return Vector3.new(-a.X, -a.Y, -a.Z) end
Vector3.__eq = function(a, b) return a.X == b.X and a.Y == b.Y and a.Z == b.Z end
Vector3.__tostring = function(a) return string.format("%.3f, %.3f, %.3f", a.X, a.Y, a.Z) end
Vector3.__index = function(self, key)
	if key == "Magnitude" then
		return math.sqrt(self.X * self.X + self.Y * self.Y + self.Z * self.Z)
	elseif key == "Unit" then
		local m = math.sqrt(self.X * self.X + self.Y * self.Y + self.Z * self.Z)
		if m == 0 then return Vector3.new(0, 0, 0) end
		return Vector3.new(self.X / m, self.Y / m, self.Z / m)
	end
	return rawget(Vector3, key)
end
function Vector3:Dot(other) return self.X * other.X + self.Y * other.Y + self.Z * other.Z end
function Vector3:Cross(other)
	return Vector3.new(self.Y * other.Z - self.Z * other.Y, self.Z * other.X - self.X * other.Z, self.X * other.Y - self.Y * other.X)
end
function Vector3:Lerp(other, t)
	return Vector3.new(self.X + (other.X - self.X) * t, self.Y + (other.Y - self.Y) * t, self.Z + (other.Z - self.Z) * t)
end

-- Vector2 / CFrame / Color3 -------------------------------------------------
local Vector2 = {}
Vector2.__index = Vector2
function Vector2.new(x, y) return setmetatable({ X = x or 0, Y = y or 0 }, Vector2) end

local Color3 = {}
Color3.__index = Color3
function Color3.new(r, g, b) return setmetatable({ R = r or 0, G = g or 0, B = b or 0 }, Color3) end
function Color3.fromRGB(r, g, b) return Color3.new((r or 0) / 255, (g or 0) / 255, (b or 0) / 255) end
function Color3.fromHSV(h, s, v)
	-- compact HSV->RGB
	if s == 0 then return Color3.new(v, v, v) end
	local i = math.floor(h * 6)
	local f = h * 6 - i
	local p, q, t = v * (1 - s), v * (1 - f * s), v * (1 - (1 - f) * s)
	i = i % 6
	if i == 0 then return Color3.new(v, t, p)
	elseif i == 1 then return Color3.new(q, v, p)
	elseif i == 2 then return Color3.new(p, v, t)
	elseif i == 3 then return Color3.new(p, q, v)
	elseif i == 4 then return Color3.new(t, p, v)
	else return Color3.new(v, p, q) end
end
Color3.__add = function(a, b) return Color3.new(a.R + b.R, a.G + b.G, a.B + b.B) end
Color3.__sub = function(a, b) return Color3.new(a.R - b.R, a.G - b.G, a.B - b.B) end
Color3.__mul = function(a, b)
	if type(b) == "number" then return Color3.new(a.R * b, a.G * b, a.B * b) end
	return Color3.new(a.R * b.R, a.G * b.G, a.B * b.B)
end
function Color3:Lerp(other, t)
	return Color3.new(self.R + (other.R - self.R) * t, self.G + (other.G - self.G) * t, self.B + (other.B - self.B) * t)
end

local CFrame = {}
CFrame.__index = CFrame
function CFrame.new(x, y, z)
	if type(x) == "table" then
		return setmetatable({ X = x.X, Y = x.Y, Z = x.Z, _rot = nil }, CFrame)
	end
	return setmetatable({ X = x or 0, Y = y or 0, Z = z or 0, _rot = nil }, CFrame)
end
CFrame.Angles = function(rx, ry, rz)
	return setmetatable({ X = 0, Y = 0, Z = 0, _rot = { rx, ry, rz } }, CFrame)
end
CFrame.fromEulerAnglesYXZ = CFrame.Angles
function CFrame.__mul(a, b)
	if b.X ~= nil then
		return CFrame.new(a.X + b.X, a.Y + b.Y, a.Z + b.Z)
	end
	return a
end
function CFrame:PointToWorldSpace(v) return Vector3.new(self.X + v.X, self.Y + v.Y, self.Z + v.Z) end
function CFrame:PointToObjectSpace(v) return Vector3.new(v.X - self.X, v.Y - self.Y, v.Z - self.Z) end
function CFrame:VectorToWorldSpace(v) return v end
function CFrame:ToWorldSpace(other) return self * other end
function CFrame:Lerp(other, t)
	return CFrame.new(self.X + (other.X - self.X) * t, self.Y + (other.Y - self.Y) * t, self.Z + (other.Z - self.Z) * t)
end
CFrame.__index = function(self, key)
	if key == "Position" then return Vector3.new(self.X, self.Y, self.Z) end
	if key == "LookVector" then return Vector3.new(0, 0, -1) end
	if key == "UpVector" then return Vector3.new(0, 1, 0) end
	if key == "RightVector" then return Vector3.new(1, 0, 0) end
	return rawget(CFrame, key)
end

-- Instances ----------------------------------------------------------------
local Instance = {}
Instance.__index = Instance
local instanceCount = 0
function Instance.new(className, parent)
	instanceCount += 1
	local obj = {
		ClassName = className,
		Name = className,
		_children = {},
		_parent = parent,
		_attributes = {},
		_props = {},
	}
	setmetatable(obj, Instance)
	if parent then
		table.insert(parent._children, obj)
	end
	return obj
end
function Instance:FindFirstChild(name)
	for _, c in ipairs(self._children) do
		if c.Name == name then return c end
	end
	return nil
end
function Instance:FindFirstChildOfClass(class)
	for _, c in ipairs(self._children) do
		if c.ClassName == class then return c end
	end
	return nil
end
function Instance:FindFirstAncestorOfClass(class)
	local p = self._parent
	while p do
		if p.ClassName == class then return p end
		p = p._parent
	end
	return nil
end
function Instance:FindFirstAncestor(name)
	local p = self._parent
	while p do
		if p.Name == name then return p end
		p = p._parent
	end
	return nil
end
function Instance:GetChildren() return self._children end
function Instance:GetDescendants()
	local out = {}
	local function walk(node)
		for _, c in ipairs(node._children) do
			table.insert(out, c)
			walk(c)
		end
	end
	walk(self)
	return out
end
function Instance:IsA(class)
	if self.ClassName == class then return true end
	return false
end
function Instance:Destroy()
	self._destroyed = true
	if self._parent then
		for i, c in ipairs(self._parent._children) do
			if c == self then
				table.remove(self._parent._children, i)
				break
			end
		end
	end
end
function Instance:SetAttribute(name, value) self._attributes[name] = value end
function Instance:GetAttribute(name) return self._attributes[name] end
function Instance:GetAttributeChangedSignal(name)
	return { Connect = function() return { Disconnect = function() end } end,
		Wait = function() end }
end
function Instance:GetPropertyChangedSignal(name)
	return { Connect = function() return { Disconnect = function() end } end,
		Wait = function() end }
end
function Instance:Clone()
	local copy = Instance.new(self.ClassName)
	for k, v in pairs(self._props) do copy[k] = v end
	copy.Name = self.Name
	for _, c in ipairs(self._children) do
		local cc = c:Clone()
		cc._parent = copy
		table.insert(copy._children, cc)
	end
	return copy
end
Instance.__newindex = function(self, key, value)
	rawset(self, key, value)
end

-- Services -----------------------------------------------------------------
local services = {}
local function service(name, tbl)
	services[name] = tbl
	return tbl
end

local function newSignal()
	local sig = { _handlers = {} }
	function sig:Connect(fn)
		table.insert(self._handlers, fn)
		return { Disconnect = function() end }
	end
	function sig:Once(fn) return self:Connect(fn) end
	function sig:Wait() end
	function sig:Fire(...)
		for _, fn in ipairs(self._handlers) do fn(...) end
	end
	return sig
end


-- Globals expected by Roblox code
function task_spawn(fn, ...)
	local co = coroutine.create(fn)
	coroutine.resume(co, ...)
	return co
end
function task_wait(t)
	if t then return t end
	return 0
end
function task_delay(t, fn, ...)
	-- run immediately for deterministic tests
	return fn(...)
end
function task_defer(fn, ...) return fn(...) end
function task_cancel() end
function tick() return os.clock() end
function time() return os.clock() end
function typeof(v)
	if type(v) == "table" and v.ClassName then return "Instance" end
	if type(v) == "table" and v.__typename then return v.__typename end
	return type(v)
end
function warn(...) io.stderr:write("WARN: ", ... , "\n") end
function Random_new(seed)
	local state = seed or os.time()
	local rng = {}
	local function nextInt(lo, hi)
		state = (1103515245 * state + 12345) % 2147483648
		return lo + state % (hi - lo + 1)
	end
	function rng:NextInteger(lo, hi) return nextInt(lo, hi) end
	function rng:NextNumber(lo, hi)
		local v = nextInt(0, 1000000) / 1000000
		if lo then return lo + v * ((hi or 1) - lo) end
		return v
	end
	function rng:NextUnitVector() return Vector3.new(0, 1, 0) end
	function rng:Clone() return Random_new(state) end
	return rng
end

-- NumberSequence & friends
local function NumberSequence_new(a, b, c)
	return { keypoints = { a, b, c } }
end
local function NumberSequenceKeypoint_new(t, v, e)
	return { Time = t, Value = v, Envelope = e or 0 }
end
local function ColorSequence_new(a, b, c)
	return { keypoints = { a, b, c } }
end
local function ColorSequenceKeypoint_new(t, c)
	return { Time = t, Value = c }
end
local function NumberRange_new(a, b) return { Min = a, Max = b or a } end

-- Enum
local Enum = setmetatable({}, {
	__index = function(_, enumName)
		local enumeration = setmetatable({}, {
			__index = function(_, item)
				local value = { Name = item, Value = 0, EnumType = enumName }
				return value
			end,
		})
		return enumeration
	end,
})

return {
	globals = {
		tick = tick, time = time, typeof = typeof, warn = warn,
	},
	Vector3 = Vector3, Vector2 = Vector2, CFrame = CFrame, Color3 = Color3,
	Instance = Instance, Enum = Enum, services = services,
	NumberSequence = { new = NumberSequence_new },
	NumberSequenceKeypoint = { new = NumberSequenceKeypoint_new },
	ColorSequence = { new = ColorSequence_new },
	ColorSequenceKeypoint = { new = ColorSequenceKeypoint_new },
	NumberRange = { new = NumberRange_new },
	newSignal = newSignal,
}
