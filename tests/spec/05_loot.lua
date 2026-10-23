-- Loot tables and the roller: every table is valid, rolls are bounded.

local Loot = require("LootTables")
local Roller = require("LootRoller")
local Items = require("ItemDefinitions")
local Enemies = require("EnemyDefinitions")

local function tableCount()
	local n = 0
	for _ in pairs(Loot) do n += 1 end
	return n
end

describe("LootTables", function()
	it("defines a deep set of tables", function()
		expect(tableCount() >= 15, "expected at least 15 loot tables, got " .. tableCount())
	end)

	it("only references real items", function()
		local entriesSeen = 0
		for name, tbl in pairs(Loot) do
			expect(type(tbl) == "table", "table " .. name .. " is not a table")
			expect(type(tbl.rolls) == "table" and #tbl.rolls == 2, "bad roll range in " .. name)
			expect(tbl.rolls[2] >= tbl.rolls[1], "roll range inverted in " .. name)
			for _, entry in ipairs(tbl.entries or {}) do
				entriesSeen += 1
				expect(Items[entry.id] ~= nil,
					string.format("table %s references unknown item %s", name, tostring(entry.id)))
				expect((entry.weight or 1) > 0, "zero weight entry in " .. name)
				expect(entry.chance == nil or (entry.chance > 0 and entry.chance <= 1),
					"bad chance in " .. name)
				expect((entry.min or 1) >= 1, "bad min count in " .. name)
				expect((entry.max or 1) >= (entry.min or 1), "max below min in " .. name)
			end
			for _, guaranteed in ipairs(tbl.guaranteed or {}) do
				expect(Items[guaranteed.id] ~= nil,
					string.format("table %s guarantees unknown item %s", name, tostring(guaranteed.id)))
			end
		end
		expect(entriesSeen > 150, "expected a deep loot pool, saw " .. entriesSeen)
	end)
end)

describe("LootRoller", function()
	it("rolls a supplied table without error", function()
		for name in pairs(Loot) do
			local drops = Roller.roll(name, Random.new(11))
			expect(type(drops) == "table", "roll returned no table for " .. name)
		end
	end)

	it("returns only real items with positive counts", function()
		local rng = Random.new(3)
		for _ = 1, 40 do
			for _, drop in ipairs(Roller.roll("residential", rng)) do
				expect(Items[drop.id] ~= nil, "unknown drop " .. tostring(drop.id))
				expect(drop.count >= 1, "zero count drop")
			end
		end
	end)

	it("is deterministic for a fixed seed", function()
		local a = Roller.roll("military_low", Random.new(1234))
		local b = Roller.roll("military_low", Random.new(1234))
		eq(#a, #b)
		for index = 1, #a do
			eq(a[index].id, b[index].id)
			eq(a[index].count, b[index].count)
		end
	end)

	it("different seeds eventually produce different results", function()
		local signatures = {}
		for seed = 1, 12 do
			local drops = Roller.roll("city_building", Random.new(seed))
			local parts = {}
			for _, drop in ipairs(drops) do
				table.insert(parts, drop.id .. "x" .. drop.count)
			end
			signatures[table.concat(parts, ",")] = true
		end
		local unique = 0
		for _ in pairs(signatures) do unique += 1 end
		expect(unique > 1, "loot is not random across seeds")
	end)

	it("unknown tables fall back to the residential pool", function()
		local drops = Roller.roll("table_that_does_not_exist", Random.new(1))
		expect(#drops > 0, "fallback pool produced nothing")
		for _, drop in ipairs(drops) do
			expect(Items[drop.id] ~= nil)
		end
	end)

	it("rolls mutant drops from enemy definitions", function()
		local rng = Random.new(9)
		for id, def in pairs(Enemies) do
			if type(def) == "table" and def.drops then
				for _ = 1, 5 do
					for _, drop in ipairs(Roller.rollEnemy(def, rng)) do
						expect(Items[drop.id] ~= nil,
							string.format("enemy %s dropped unknown item %s", id, tostring(drop.id)))
					end
				end
			end
		end
	end)

	it("supply drops respect the requested tier", function()
		local drops = Roller.rollSupplyDrop(Random.new(5), 3)
		expect(#drops > 0, "high tier supply drop was empty")
	end)
end)
