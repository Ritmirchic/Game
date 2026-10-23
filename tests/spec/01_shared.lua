-- Validation of shared data definitions and pure logic.

describe("ItemDefinitions", function()
	local Items = require("ItemDefinitions")

	it("every item has required fields", function()
		local count = 0
		for id, def in pairs(Items) do
			count += 1
			eq(def.id, id, "id mismatch for " .. id)
			expect(type(def.name) == "string" and #def.name > 0, "missing name: " .. id)
			expect(type(def.desc) == "string", "missing desc: " .. id)
			expect(type(def.weight) == "number" and def.weight >= 0, "bad weight: " .. id)
			expect(def.w >= 1 and def.w <= 6, "bad width: " .. id)
			expect(def.h >= 1 and def.h <= 6, "bad height: " .. id)
			expect(def.stack >= 1, "bad stack: " .. id)
			expect(def.rarity >= 1 and def.rarity <= 5, "bad rarity: " .. id)
		end
		expect(count > 50, "expected a deep item catalogue, got " .. count)
	end)

	it("equipment slots are valid", function()
		local Config = require("Config")
		local valid = {}
		for _, slot in ipairs(Config.EquipSlots) do valid[slot] = true end
		for id, def in pairs(Items) do
			if def.equip then
				expect(valid[def.equip], "item " .. id .. " equips to unknown slot " .. tostring(def.equip))
			end
		end
	end)

	it("weapon items reference real weapon definitions", function()
		local Weapons = require("WeaponDefinitions")
		for id, def in pairs(Items) do
			if def.weapon then
				expect(Weapons[def.weapon] ~= nil, "item " .. id .. " -> unknown weapon " .. tostring(def.weapon))
			end
		end
	end)

	it("use effects only reference known effect kinds", function()
		local kinds = {
			food = true, drink = true, medical = true, craft_tool = true, radio = true,
			zoom = true, map = true, light = true, flare = true, blueprint = true,
		}
		for id, def in pairs(Items) do
			if def.use then
				expect(kinds[def.use.kind], "item " .. id .. " has unknown use kind " .. tostring(def.use.kind))
			end
		end
	end)
end)

describe("WeaponDefinitions", function()
	local Weapons = require("WeaponDefinitions")
	local Items = require("ItemDefinitions")

	it("every weapon has sane combat stats", function()
		for id, w in pairs(Weapons) do
			if type(w) ~= "table" then continue end
			expect(w.damage > 0, "damage " .. id)
			expect(w.range > 0, "range " .. id)
			expect(w.fireRate > 0, "fire rate " .. id)
			expect(w.noise >= 0, "noise " .. id)
			if w.kind == "firearm" then
				expect(Items[w.ammo] ~= nil, "weapon " .. id .. " uses unknown ammo " .. tostring(w.ammo))
				expect(w.magazine >= 1, "magazine " .. id)
				expect(w.reload > 0, "reload " .. id)
				expect(w.aimSpread <= w.spread, "aim spread should tighten " .. id)
			end
		end
	end)

	it("dps stays within a survivable band", function()
		for id, w in pairs(Weapons) do
			if type(w) ~= "table" then continue end
			local dps = (w.damage * (w.pellets or 1)) / w.fireRate
			expect(dps > 5, "weapon " .. id .. " almost harmless (" .. dps .. " dps)")
			expect(dps < 400, "weapon " .. id .. " is overtuned (" .. dps .. " dps)")
		end
	end)
end)

describe("EnemyDefinitions", function()
	local Enemies = require("EnemyDefinitions")
	local regions = require("Config").Regions

	it("defines ten species with unique ids", function()
		local n = 0
		for _, def in pairs(Enemies) do if type(def) == "table" then n += 1 end end
		eq(n, 10, "species count")
	end)

	it("habitats reference real regions", function()
		for id, def in pairs(Enemies) do
			if type(def) ~= "table" then continue end
			for _, r in ipairs(def.habitat) do
				expect(regions[r] ~= nil, "enemy " .. id .. " habitat " .. r)
			end
		end
	end)

	it("every region has at least one resident species", function()
		for name in pairs(regions) do
			local list = Enemies.byRegion(name)
			expect(#list > 0, "region " .. name .. " has no mutants")
		end
	end)

	it("stats are balanced against player weapons", function()
		for id, def in pairs(Enemies) do
			if type(def) ~= "table" then continue end
			expect(def.health > 0 and def.speed > 0, "stats " .. id)
			expect(def.speed < 40, "enemy " .. id .. " is faster than the engine can handle")
			expect(def.vision > 0 and def.vision <= 260, "vision " .. id)
			local ttk = def.health / 34 -- rifle dps approximation
			expect(ttk > 0.8, "enemy " .. id .. " dies too fast (" .. ttk .. "s)")
		end
	end)
end)

describe("LootTables", function()
	local Loot = require("LootTables")
	local Items = require("ItemDefinitions")
	local Util = require("Util")

	it("all entries reference real items", function()
		for name, tbl in pairs(Loot) do
			for _, entry in ipairs(tbl.entries) do
				expect(Items[entry.id] ~= nil, "loot table " .. name .. " references " .. tostring(entry.id))
				expect((entry.weight or 1) > 0, "zero weight entry in " .. name)
			end
		end
	end)

	it("weighted rolls always return a valid entry", function()
		local rng = Random.new(7)
		for name, tbl in pairs(Loot) do
			for _ = 1, 25 do
				local pick = Util.weightedPick(tbl.entries, rng)
				expect(pick and Items[pick.id] ~= nil, "bad pick from " .. name)
			end
		end
	end)
end)

describe("Recipes", function()
	local Recipes = require("Recipes")
	local Items = require("ItemDefinitions")

	it("references real items and produces output", function()
		for id, r in pairs(Recipes) do
			if type(r) ~= "table" then continue end
			expect(r.output and Items[r.output.id] ~= nil, "recipe " .. id .. " output")
			expect(r.output.count >= 1, "recipe " .. id .. " count")
			local total = 0
			for ing, n in pairs(r.ingredients) do
				expect(Items[ing] ~= nil, "recipe " .. id .. " ingredient " .. tostring(ing))
				expect(n > 0, "recipe " .. id .. " ingredient count")
				total += n
			end
			expect(total > 0, "recipe " .. id .. " has no ingredients")
		end
	end)

	it("blueprints referenced exist as items", function()
		for id, r in pairs(Recipes) do
			if type(r) == "table" and r.blueprint then
				expect(Items[r.blueprint] ~= nil, "recipe " .. id .. " blueprint " .. r.blueprint)
			end
		end
	end)
end)

describe("QuestDefinitions", function()
	local Quests = require("QuestDefinitions")
	local Items = require("ItemDefinitions")
	local Recipes = require("Recipes")
	local Build = require("BuildDefinitions")

	it("objective payloads are valid", function()
		for id, q in pairs(Quests) do
			if type(q) ~= "table" then continue end
			expect(#q.objectives > 0, "quest " .. id .. " has no objectives")
			for _, o in ipairs(q.objectives) do
				if o.type == "collect" then
					expect(Items[o.item] ~= nil or (o.count or 0) == 0,
						"quest " .. id .. " collect unknown item " .. tostring(o.item))
				elseif o.type == "craft" then
					expect(Recipes[o.recipe] ~= nil, "quest " .. id .. " craft " .. tostring(o.recipe))
				elseif o.type == "build" then
					expect(Build[o.piece] ~= nil, "quest " .. id .. " build " .. tostring(o.piece))
				elseif o.type == "kill" then
					expect(require("EnemyDefinitions")[o.enemy] ~= nil, "quest " .. id .. " kill " .. tostring(o.enemy))
				elseif o.type == "reach" then
					expect(o.region ~= nil or o.x ~= nil, "quest " .. id .. " reach objective has no target")
				end
			end
		end
	end)

	it("main line is ordered and reachable", function()
		local main = Quests.mainLine()
		expect(#main >= 6, "expected 6 chapters, got " .. #main)
		for i = 2, #main do
			expect(main[i].chapter > main[i - 1].chapter, "chapters must increase")
		end
	end)
end)
