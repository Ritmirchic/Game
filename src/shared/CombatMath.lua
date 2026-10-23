--!strict
--[[
	CombatMath — deterministic combat helpers.

	The server owns hit resolution; these helpers make it consistent and testable.
]]

local Config = require(script.Parent.Config)
local Util = require(script.Parent.Util)
local Weapons = require(script.Parent.WeaponDefinitions)

local CM = {}

function CM.falloffMultiplier(def, distance)
	local f = def.falloff
	if not f then return 1 end
	if distance <= f.start then return 1 end
	if distance >= f.finish then return f.min end
	local t = (distance - f.start) / (f.finish - f.start)
	return Util.lerp(1, f.min, t)
end

function CM.armorMultiplier(armorTable, part)
	if not armorTable then return 1 end
	local key = "torso"
	if part == "Head" then key = "head"
	elseif part == "LeftArm" or part == "RightArm" then key = "arms"
	elseif part == "LeftLeg" or part == "RightLeg" then key = "legs"
	end
	local value = armorTable[key] or 0
	return 1 - Util.clamp(value, 0, 0.85)
end

function CM.hitDamage(def, distance, part, armorTable, durabilityFactor)
	local damage = def.damage * CM.falloffMultiplier(def, distance)
	if part == "Head" then
		damage *= Config.Combat.HeadshotMultiplier
	end
	damage *= CM.armorMultiplier(armorTable, part)
	if durabilityFactor then
		-- badly worn weapons hit softer
		damage *= Util.lerp(0.55, 1, Util.clamp(durabilityFactor, 0, 1))
	end
	return damage
end

-- Random cone deviation around a direction.
function CM.spreadDirection(direction, spread, rng)
	if spread <= 0 then return direction end
	local right = direction:Cross(Vector3.new(0, 1, 0))
	if right.Magnitude < 1e-3 then
		right = Vector3.new(1, 0, 0)
	end
	right = right.Unit
	local up = right:Cross(direction).Unit
	local angle = rng:NextNumber(0, math.pi * 2)
	local radius = math.sqrt(rng:NextNumber()) * spread
	local offset = right * (math.cos(angle) * radius) + up * (math.sin(angle) * radius)
	return (direction + offset).Unit
end

-- Recoil impulse for the camera, scaled by stance.
function CM.recoilImpulse(def, crouched, aiming)
	local base = def.recoil or 0
	if crouched then base *= 0.65 end
	if aiming then base *= 0.55 end
	return base
end

-- Time-to-kill helper used by balancing tests and enemy tuning.
function CM.dps(def)
	if def.kind == "melee" then
		return def.damage / math.max(def.fireRate, 0.05)
	end
	return (def.damage * (def.pellets or 1)) / math.max(def.fireRate, 0.05)
end

function CM.canFire(lastFireTime, now, def)
	local interval = def.fireRate - Config.Combat.MaxFireRateSlack
	return (now - lastFireTime) >= math.max(interval, 0.02)
end

function CM.consumeAmmo(equipped, def, count)
	-- equipped: { uid, itemId, count, magazine }
	count = count or 1
	equipped.magazine = (equipped.magazine or def.magazine) - count
	return equipped.magazine
end

function CM.meleeHitTest(origin, direction, reach, radius, ignoreList, params)
	params = params or RaycastParams.new()
	local result = workspace:Spherecast(origin, radius or 1.4, direction * reach, params)
	return result
end

function CM.pickBodyPart(hitName, rng)
	local name = string.lower(hitName or "")
	if string.find(name, "head") then
		return "Head"
	elseif string.find(name, "arm") then
		if string.find(name, "left") then return "LeftArm" end
		return "RightArm"
	elseif string.find(name, "leg") then
		if string.find(name, "left") then return "LeftLeg" end
		return "RightLeg"
	elseif string.find(name, "torso") or string.find(name, "humanoidrootpart")
		or string.find(name, "chest") or string.find(name, "body") then
		return "Torso"
	end
	-- fall back to a weighted body distribution
	local roll = rng and rng:NextNumber() or math.random()
	if roll < 0.12 then return "Head" end
	if roll < 0.58 then return "Torso" end
	if roll < 0.72 then return "LeftArm" end
	if roll < 0.86 then return "RightArm" end
	if roll < 0.93 then return "LeftLeg" end
	return "RightLeg"
end

function CM.aggregateArmor(equipment, Items)
	local armor = { torso = 0, head = 0, arms = 0, legs = 0, radiation = 0, cold = 0, infection = 0, noise = 0 }
	if not equipment then return armor end
	for _, entry in pairs(equipment) do
		if entry and entry.itemId then
			local def = Items[entry.itemId]
			if def and def.armor then
				for key, value in pairs(def.armor) do
					armor[key] = (armor[key] or 0) + value
				end
			end
		end
	end
	for key, value in pairs(armor) do
		if key == "cold" or key == "noise" or key == "radiation" or key == "infection" then
			armor[key] = Util.clamp(value, -0.6, 0.92)
		else
			armor[key] = Util.clamp(value, 0, 0.85)
		end
	end
	return armor
end

function CM.explosionDamage(center, position, maxDamage, radius)
	local distance = (center - position).Magnitude
	if distance > radius then return 0 end
	return maxDamage * (1 - distance / radius) ^ 1.5
end

function CM.weaponForItem(itemId, Items)
	local def = Items[itemId]
	if not def or not def.weapon then return nil end
	return Weapons[def.weapon]
end

return CM
