--!strict
--[[
	LootRoller — deterministic, server-side loot generation.

	The same module is used for:
	  * world containers (LootService)
	  * enemy drops (EnemyService)
	  * supply drop world events (WorldEventService)
	Because it is pure, the balancing tests can hammer it with millions of rolls.
]]

local LootTables = require(script.Parent.LootTables)
local Items = require(script.Parent.ItemDefinitions)
local Util = require(script.Parent.Util)

local Roller = {}

-- Returns array of { id = itemId, count = n }
function Roller.roll(tableName, rng, luck)
	tableName = tableName or "residential"
	local tbl = LootTables[tableName] or LootTables.residential
	luck = luck or 0
	local rolls = rng:NextInteger(tbl.rolls[1], tbl.rolls[2])
	local out = {}

	local function push(id, count)
		count = math.max(1, math.floor(count or 1))
		local def = Items[id]
		if not def then return end
		if count > def.stack then count = def.stack end
		for _, entry in ipairs(out) do
			if entry.id == id and def.stack > 1 then
				entry.count = math.min(def.stack, entry.count + count)
				return
			end
		end
		table.insert(out, { id = id, count = count })
	end

	for _ = 1, rolls do
		local pool = {}
		for _, entry in ipairs(tbl.entries) do
			-- luck biases toward rarity
			local weight = entry.weight or 1
			local def = Items[entry.id]
			if def then
				if luck > 0 and def.rarity >= 3 then
					weight = weight * (1 + luck * (def.rarity - 2) * 0.6)
				end
				table.insert(pool, { entry = entry, weight = weight })
			end
		end
		if #pool > 0 then
			local pick = Util.weightedPick(pool, rng)
			local entry = pick.entry
			if entry.chance == nil or rng:NextNumber() <= entry.chance then
				local count = rng:NextInteger(entry.min or 1, entry.max or 1)
				push(entry.id, count)
			end
		end
	end

	for _, guaranteed in ipairs(tbl.guaranteed or {}) do
		if Items[guaranteed.id] and rng:NextNumber() <= (guaranteed.chance or 1) then
			push(guaranteed.id, guaranteed.count or 1)
		end
	end

	return out
end

-- Enemy loot: uses the enemy definition's drop list.
function Roller.rollEnemy(def, rng)
	local out = {}
	if not def or not def.drops then return out end
	if rng:NextNumber() > (def.lootChance or 0.5) then
		return out
	end
	local count = rng:NextInteger(1, math.min(#def.drops, 2))
	Util.shuffle(def.drops, rng)
	for i = 1, count do
		local id = def.drops[i]
		if Items[id] then
			table.insert(out, { id = id, count = 1 })
		end
	end
	return out
end

-- Supply drop: multi-table, higher luck.
function Roller.rollSupplyDrop(rng, tier)
	local out = {}
	local tables = { "military_high", "military", "medical", "weapons" }
	if tier and tier >= 5 then
		table.insert(tables, "rare")
	end
	for _, name in ipairs(tables) do
		local rolled = Roller.roll(name, rng, 1.2)
		for _, entry in ipairs(rolled) do
			table.insert(out, entry)
			if #out >= 6 then return out end
		end
	end
	return out
end

function Roller.summary(items)
	local parts = {}
	for _, entry in ipairs(items) do
		local def = Items[entry.id]
		local name = def and def.name or entry.id
		if entry.count > 1 then
			table.insert(parts, string.format("%s x%d", name, entry.count))
		else
			table.insert(parts, name)
		end
	end
	return table.concat(parts, ", ")
end

return Roller
