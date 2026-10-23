--!strict
--[[
    ASHES OF HUMANITY: LAST SIGNAL
    ReplicatedStorage.Shared.UtilityModules
    High-performance reusable utilities:
    - SpatialHashGrid (2D spatial partitioning for O(1) AI/loot/building proximity queries)
    - Spring1D / Spring3D (critically/under-damped physics springs for camera, sway, recoil & procedural animations)
    - ObjectPool (zero-allocation cosmetic debris/shell-casing pooling)
    - Signal (lightweight event dispatcher)
    - WeightedRandom & UID generation
]]

local Config = require(script.Parent.Config)

local UtilityModules = {}

-- ============================================================
-- UID Generator
-- ============================================================
local uidCounter = 0
function UtilityModules.GenerateUID(prefix: string?): string
	uidCounter += 1
	local t = math.floor(os.clock() * 1000)
	local r = math.random(1000, 9999)
	return string.format("%s_%d_%d_%d", prefix or "id", t, uidCounter, r)
end

-- ============================================================
-- Math Helpers
-- ============================================================
function UtilityModules.Clamp(x: number, minVal: number, maxVal: number): number
	if x < minVal then
		return minVal
	elseif x > maxVal then
		return maxVal
	end
	return x
end

function UtilityModules.Lerp(a: number, b: number, t: number): number
	return a + (b - a) * UtilityModules.Clamp(t, 0, 1)
end

function UtilityModules.IsFiniteNumber(v: any): boolean
	return type(v) == "number" and v == v and v > -math.huge and v < math.huge
end

function UtilityModules.IsFiniteVector3(v: any): boolean
	if typeof(v) ~= "Vector3" then
		return false
	end
	return UtilityModules.IsFiniteNumber(v.X)
		and UtilityModules.IsFiniteNumber(v.Y)
		and UtilityModules.IsFiniteNumber(v.Z)
end

-- ============================================================
-- Region Resolver
-- ============================================================
function UtilityModules.GetRegionAtPosition(pos: Vector3): string
	-- Check underground shelter first
	local shelter = Config.REGIONS.EmergencyShelter
	if pos.Y < -5 and (pos - shelter.Center).Magnitude <= shelter.Radius then
		return "EmergencyShelter"
	end

	for id, reg in pairs(Config.REGIONS) do
		if id ~= "EmergencyShelter" and reg.MinBounds and reg.MaxBounds then
			if
				pos.X >= reg.MinBounds.X
				and pos.X <= reg.MaxBounds.X
				and pos.Z >= reg.MinBounds.Z
				and pos.Z <= reg.MaxBounds.Z
			then
				return id
			end
		end
	end
	return "DeadSuburbs"
end

-- ============================================================
-- Weighted Loot / Event Picker
-- ============================================================
export type WeightedEntry<T> = {
	Weight: number,
	Value: T,
}

function UtilityModules.PickWeighted<T>(entries: { WeightedEntry<T> }, rng: Random?): T?
	local total = 0
	for _, entry in ipairs(entries) do
		if entry.Weight > 0 then
			total += entry.Weight
		end
	end
	if total <= 0 then
		return nil
	end
	local roll = if rng then rng:NextNumber(0, total) else math.random() * total
	local acc = 0
	for _, entry in ipairs(entries) do
		if entry.Weight > 0 then
			acc += entry.Weight
			if roll <= acc then
				return entry.Value
			end
		end
	end
	return entries[#entries].Value
end

-- ============================================================
-- 1D & 3D Damped Harmonic Oscillator Springs
-- ============================================================
export type Spring1D = {
	Position: number,
	Velocity: number,
	Target: number,
	Frequency: number,
	Damping: number,
	Impulse: (self: Spring1D, dv: number) -> (),
	Update: (self: Spring1D, dt: number) -> number,
}

function UtilityModules.NewSpring1D(initial: number?, freq: number?, damping: number?): Spring1D
	local p0 = initial or 0
	local spring = {
		Position = p0,
		Velocity = 0,
		Target = p0,
		Frequency = freq or 14.0,
		Damping = damping or 0.82,
	}
	function spring:Impulse(dv: number)
		self.Velocity += dv
	end
	function spring:Update(dt: number): number
		local step = math.min(dt, 0.05)
		local omega = self.Frequency * 2.0 * math.pi
		local disp = self.Position - self.Target
		local accel = -omega * omega * disp - 2.0 * self.Damping * omega * self.Velocity
		self.Velocity += accel * step
		self.Position += self.Velocity * step
		return self.Position
	end
	return spring
end

export type Spring3D = {
	Position: Vector3,
	Velocity: Vector3,
	Target: Vector3,
	Frequency: number,
	Damping: number,
	Impulse: (self: Spring3D, dv: Vector3) -> (),
	Update: (self: Spring3D, dt: number) -> Vector3,
}

function UtilityModules.NewSpring3D(initial: Vector3?, freq: number?, damping: number?): Spring3D
	local p0 = initial or Vector3.zero
	local spring = {
		Position = p0,
		Velocity = Vector3.zero,
		Target = p0,
		Frequency = freq or 14.0,
		Damping = damping or 0.82,
	}
	function spring:Impulse(dv: Vector3)
		self.Velocity += dv
	end
	function spring:Update(dt: number): Vector3
		local step = math.min(dt, 0.05)
		local omega = self.Frequency * 2.0 * math.pi
		local disp = self.Position - self.Target
		local accel = disp * (-omega * omega) - self.Velocity * (2.0 * self.Damping * omega)
		self.Velocity += accel * step
		self.Position += self.Velocity * step
		return self.Position
	end
	return spring
end

-- ============================================================
-- 2D Spatial Hash Grid (XZ Plane Partitioning)
-- ============================================================
export type SpatialHashGrid = {
	CellSize: number,
	Cells: { [string]: { [string]: { Id: string, Pos: Vector3, Data: any } } },
	ItemCell: { [string]: string },
	InsertOrUpdate: (self: SpatialHashGrid, id: string, pos: Vector3, data: any) -> (),
	Remove: (self: SpatialHashGrid, id: string) -> (),
	QueryRadius: (
		self: SpatialHashGrid,
		center: Vector3,
		radius: number
	) -> { { Id: string, Pos: Vector3, Data: any, Dist: number } },
}

local function cellKey(cx: number, cz: number): string
	return string.format("%d:%d", cx, cz)
end

function UtilityModules.NewSpatialHashGrid(cellSize: number?): SpatialHashGrid
	local cs = cellSize or Config.SPATIAL_CELL_SIZE
	local grid = {
		CellSize = cs,
		Cells = {} :: { [string]: { [string]: { Id: string, Pos: Vector3, Data: any } } },
		ItemCell = {} :: { [string]: string },
	}

	function grid:InsertOrUpdate(id: string, pos: Vector3, data: any)
		local cx = math.floor(pos.X / self.CellSize)
		local cz = math.floor(pos.Z / self.CellSize)
		local newKey = cellKey(cx, cz)
		local prevKey = self.ItemCell[id]
		if prevKey and prevKey ~= newKey then
			local prevBucket = self.Cells[prevKey]
			if prevBucket then
				prevBucket[id] = nil
				if next(prevBucket) == nil then
					self.Cells[prevKey] = nil
				end
			end
		end
		local bucket = self.Cells[newKey]
		if not bucket then
			bucket = {}
			self.Cells[newKey] = bucket
		end
		bucket[id] = { Id = id, Pos = pos, Data = data }
		self.ItemCell[id] = newKey
	end

	function grid:Remove(id: string)
		local prevKey = self.ItemCell[id]
		if prevKey then
			local bucket = self.Cells[prevKey]
			if bucket then
				bucket[id] = nil
				if next(bucket) == nil then
					self.Cells[prevKey] = nil
				end
			end
			self.ItemCell[id] = nil
		end
	end

	function grid:QueryRadius(center: Vector3, radius: number)
		local results = {}
		local rSq = radius * radius
		local minCX = math.floor((center.X - radius) / self.CellSize)
		local maxCX = math.floor((center.X + radius) / self.CellSize)
		local minCZ = math.floor((center.Z - radius) / self.CellSize)
		local maxCZ = math.floor((center.Z + radius) / self.CellSize)

		for cx = minCX, maxCX do
			for cz = minCZ, maxCZ do
				local bucket = self.Cells[cellKey(cx, cz)]
				if bucket then
					for _, entry in pairs(bucket) do
						local d = entry.Pos - center
						local dSq = d.X * d.X + d.Y * d.Y + d.Z * d.Z
						if dSq <= rSq then
							table.insert(results, {
								Id = entry.Id,
								Pos = entry.Pos,
								Data = entry.Data,
								Dist = math.sqrt(dSq),
							})
						end
					end
				end
			end
		end
		return results
	end

	return grid
end

-- ============================================================
-- Lightweight Signal
-- ============================================================
export type SignalConnection = {
	Connected: boolean,
	Disconnect: (self: SignalConnection) -> (),
}

export type Signal<T...> = {
	Connect: (self: Signal<T...>, fn: (T...) -> ()) -> SignalConnection,
	Fire: (self: Signal<T...>, T...) -> (),
	Destroy: (self: Signal<T...>) -> (),
}

function UtilityModules.NewSignal<T...>(): Signal<T...>
	local listeners: { (T...) -> () } = {}
	local sig = {}

	function sig:Connect(fn: (T...) -> ()): SignalConnection
		table.insert(listeners, fn)
		local conn = { Connected = true }
		function conn:Disconnect()
			if not self.Connected then
				return
			end
			self.Connected = false
			local idx = table.find(listeners, fn)
			if idx then
				table.remove(listeners, idx)
			end
		end
		return conn
	end

	function sig:Fire(...: T...)
		local snapshot = table.clone(listeners)
		for _, fn in ipairs(snapshot) do
			task.spawn(fn, ...)
		end
	end

	function sig:Destroy()
		table.clear(listeners)
	end

	return sig
end

-- ============================================================
-- Object Pool (Cosmetic Debris / Shell Casings / Shards)
-- ============================================================
function UtilityModules.NewObjectPool<T>(factory: () -> T, resetter: (item: T) -> (), maxSize: number?)
	local cap = maxSize or 64
	local freeList: { T } = {}
	local pool = {}

	function pool:Acquire(): T
		local n = #freeList
		if n > 0 then
			local item = freeList[n]
			freeList[n] = nil
			return item
		end
		return factory()
	end

	function pool:Release(item: T)
		resetter(item)
		if #freeList < cap then
			table.insert(freeList, item)
		end
	end

	function pool:Size(): number
		return #freeList
	end

	return pool
end

return UtilityModules
