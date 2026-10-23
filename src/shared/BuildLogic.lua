--!strict
--[[
	BuildLogic — placement maths for base building (pure).

	`snapPosition` quantises a point onto the build grid, `validatePlacement`
	checks footprint overlap against a caller-provided occupancy index and
	`canAfford`/`consumeCost` handle materials.
]]

local Build = require(script.Parent.BuildDefinitions)
local Inventory = require(script.Parent.InventoryModel)
local Items = require(script.Parent.ItemDefinitions)

local BL = {}

function BL.snapPosition(def, position, rotation)
	local grid = def.grid or 4
	local yaw = math.rad(rotation or 0)
	local cos, sin = math.cos(yaw), math.sin(yaw)
	-- snap in the local frame so rotated pieces line up
	local lx = position.X * cos - position.Z * sin
	local lz = position.X * sin + position.Z * cos
	local sx = math.floor(lx / grid + 0.5) * grid
	local sz = math.floor(lz / grid + 0.5) * grid
	local wx = sx * cos + sz * sin
	local wz = -sx * sin + sz * cos
	return Vector3.new(wx, position.Y, wz)
end

function BL.snapRotation(rotation)
	local step = 15
	return math.floor((rotation or 0) / step + 0.5) * step % 360
end

function BL.footprintOverlap(def, position, rotation, others)
	local size = def.size
	local cos, sin = math.cos(math.rad(rotation or 0)), math.sin(math.rad(rotation or 0))
	local halfX = (math.abs(size.X * cos) + math.abs(size.Z * sin)) / 2
	local halfZ = (math.abs(size.X * sin) + math.abs(size.Z * cos)) / 2
	for _, other in ipairs(others) do
		local osize = other.size
		local oc = math.cos(math.rad(other.rotation or 0))
		local os = math.sin(math.rad(other.rotation or 0))
		local ohalfX = (math.abs(osize.X * oc) + math.abs(osize.Z * os)) / 2
		local ohalfZ = (math.abs(osize.X * os) + math.abs(osize.Z * oc)) / 2
		local dx = math.abs(position.X - other.position.X)
		local dy = math.abs(position.Y - other.position.Y)
		local dz = math.abs(position.Z - other.position.Z)
		if dx < (halfX + ohalfX) * 0.92 and dz < (halfZ + ohalfZ) * 0.92
			and dy < (size.Y + osize.Y) * 0.45 then
			return true, other
		end
	end
	return false
end

function BL.validatePlacement(profile, pieceId, position, rotation, others, groundHit)
	local def = Build[pieceId]
	if not def then return false, "unknown piece" end
	if not groundHit and not def.horizontal then
		-- walls can be placed on top of other structures
		local supported = false
		for _, other in ipairs(others) do
			if math.abs(other.position.Y - (position.Y - def.size.Y / 2)) < 6 then
				supported = true
				break
			end
		end
		if not supported then
			return false, "needs a foundation"
		end
	end
	if position.Y < -400 then
		return false, "too far below the world"
	end
	local overlap, other = BL.footprintOverlap(def, position, rotation, others)
	if overlap then
		return false, "blocked by another structure"
	end
	local ok, reason = BL.canAfford(profile, pieceId)
	if not ok then return false, reason end
	return true
end

function BL.canAfford(profile, pieceId)
	local def = Build[pieceId]
	if not def then return false, "unknown piece" end
	for id, count in pairs(def.cost) do
		local have = Inventory.countItem(profile.inventory, id, true)
		if have < count then
			local itemDef = Items[id]
			return false, string.format("need %d %s", count, itemDef and itemDef.name or id)
		end
	end
	return true
end

function BL.consumeCost(profile, pieceId)
	local def = Build[pieceId]
	if not def then return false end
	for id, count in pairs(def.cost) do
		if not Inventory.consumeItem(profile.inventory, id, count) then
			return false
		end
	end
	return true
end

function BL.refund(profile, pieceId, fraction)
	local def = Build[pieceId]
	if not def then return end
	fraction = fraction or 0.6
	for id, count in pairs(def.cost) do
		local give = math.max(1, math.floor(count * fraction))
		Inventory.addItem(profile.inventory, id, give)
	end
end

function BL.healthFor(pieceId, quality)
	local def = Build[pieceId]
	if not def then return 100 end
	return math.floor(def.health * (quality or 1))
end

return BL
