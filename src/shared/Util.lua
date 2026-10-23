--!strict
--[[
	Util — small, dependency free helpers used across the codebase.
	Everything here must be safe to call from the server and the client.
]]

local Util = {}

local sqrt = math.sqrt
local floor = math.floor

function Util.clamp(v, lo, hi)
	if v < lo then return lo end
	if v > hi then return hi end
	return v
end

function Util.lerp(a, b, t)
	return a + (b - a) * t
end

function Util.inverseLerp(a, b, v)
	if a == b then return 0 end
	return Util.clamp((v - a) / (b - a), 0, 1)
end

function Util.smoothstep(t)
	return t * t * (3 - 2 * t)
end

function Util.distance(a, b)
	local dx, dy, dz = a.X - b.X, a.Y - b.Y, a.Z - b.Z
	return sqrt(dx * dx + dy * dy + dz * dz)
end

function Util.round(v, places)
	local m = 10 ^ (places or 0)
	return floor(v * m + 0.5) / m
end

function Util.sign(v)
	if v > 0 then return 1 end
	if v < 0 then return -1 end
	return 0
end

function Util.lerpAngle(a, b, t)
	local diff = (b - a + 180) % 360 - 180
	return a + diff * t
end

function Util.choose(list, rng)
	local r = rng or Random.new()
	return list[r:NextInteger(1, #list)]
end

function Util.shuffle(list, rng)
	local r = rng or Random.new()
	for i = #list, 2, -1 do
		local j = r:NextInteger(1, i)
		list[i], list[j] = list[j], list[i]
	end
	return list
end

function Util.weightedPick(entries, rng)
	-- entries: array of { weight = number, ... }
	local total = 0
	for _, e in ipairs(entries) do
		total += math.max(e.weight or e.Weight or 1, 0)
	end
	if total <= 0 then return entries[1] end
	local r = (rng or Random.new()):NextNumber() * total
	for _, e in ipairs(entries) do
		r -= math.max(e.weight or e.Weight or 1, 0)
		if r <= 0 then return e end
	end
	return entries[#entries]
end

function Util.normalize(v)
	local m = v.Magnitude
	if m < 1e-5 then return Vector3.zero end
	return v / m
end

function Util.flatten(...)
	local out = {}
	for _, t in ipairs({ ... }) do
		for _, v in ipairs(t) do
			table.insert(out, v)
		end
	end
	return out
end

function Util.deepCopy(t, seen)
	seen = seen or {}
	if type(t) ~= "table" then return t end
	if seen[t] then return seen[t] end
	local out = {}
	seen[t] = out
	for k, v in pairs(t) do
		out[k] = Util.deepCopy(v, seen)
	end
	return out
end

function Util.countDict(dict)
	local n = 0
	for _, v in pairs(dict) do n += v end
	return n
end

function Util.keys(t)
	local out = {}
	for k in pairs(t) do table.insert(out, k) end
	return out
end

function Util.formatTime(seconds)
	seconds = math.max(0, math.floor(seconds))
	local m = floor(seconds / 60)
	local s = seconds % 60
	return string.format("%d:%02d", m, s)
end

function Util.formatClock(hours)
	local h = hours % 24
	local hh = floor(h)
	local mm = floor((h - hh) * 60)
	local suffix = "AM"
	local display = hh
	if display == 0 then
		display = 12
	elseif display >= 12 then
		suffix = "PM"
		if display > 12 then display -= 12 end
	end
	return string.format("%d:%02d %s", display, mm, suffix)
end

function Util.isFiniteNumber(v)
	return type(v) == "number" and v == v and v ~= math.huge and v ~= -math.huge
end

function Util.sanitizeString(s, maxLen)
	if type(s) ~= "string" then return "" end
	maxLen = maxLen or 256
	if #s > maxLen then s = string.sub(s, 1, maxLen) end
	return s
end

-- Simple connection bag so controllers can be torn down cleanly.
local Bag = {}
Bag.__index = Bag

function Util.newBag()
	return setmetatable({ items = {} }, Bag)
end

function Bag:add(conn)
	table.insert(self.items, conn)
	return conn
end

function Bag:clear()
	for _, c in ipairs(self.items) do
		if typeof(c) == "RBXScriptConnection" then
			c:Disconnect()
		elseif type(c) == "function" then
			c()
		end
	end
	self.items = {}
end

-- Rate limiter used by every remote handler on the server.
local Limiter = {}
Limiter.__index = Limiter

function Util.newRateLimiter(capacity, refillPerSecond)
	return setmetatable({ capacity = capacity, refill = refillPerSecond, tokens = capacity, last = os.clock() }, Limiter)
end

function Limiter:allow(cost)
	local now = os.clock()
	local elapsed = now - self.last
	self.last = now
	self.tokens = math.min(self.capacity, self.tokens + elapsed * self.refill)
	cost = cost or 1
	if self.tokens >= cost then
		self.tokens -= cost
		return true
	end
	return false
end

-- Distance between 2D points, used for map/compass math
function Util.dist2D(a, b)
	local dx, dz = a.X - b.X, a.Z - b.Z
	return sqrt(dx * dx + dz * dz)
end

function Util.bearingAngle(fromPos, toPos)
	local dx = toPos.X - fromPos.X
	local dz = toPos.Z - fromPos.Z
	return math.degrees(math.atan2(dx, -dz))
end

-- Deterministic-ish pseudo random for cosmetic variety
local seedSource = 0
function Util.hash(...)
	seedSource += 1
	local parts = { ... }
	local h = 5381 + seedSource
	for _, v in ipairs(parts) do
		local s = tostring(v)
		for i = 1, #s do
			h = (h * 33 + string.byte(s, i)) % 2147483647
		end
	end
	return h
end

function Util.sequenceFromArray(array)
	local points = {}
	for i, v in ipairs(array) do
		points[i] = NumberSequenceKeypoint.new((i - 1) / math.max(1, #array - 1), v)
	end
	return NumberSequence.new(points)
end

function Util.colorSequenceFromArray(colors)
	local points = {}
	for i, c in ipairs(colors) do
		points[i] = ColorSequenceKeypoint.new((i - 1) / math.max(1, #colors - 1), c)
	end
	return ColorSequence.new(points)
end

function Util.try(fn, ...)
	local ok, result = pcall(fn, ...)
	if ok then return result end
	return nil
end

function Util.tableContains(list, value)
	for _, v in ipairs(list) do
		if v == value then return true end
	end
	return false
end

function Util.clampVector(v, maxLen)
	local m = v.Magnitude
	if m > maxLen and m > 0 then
		return v * (maxLen / m)
	end
	return v
end

return Util
