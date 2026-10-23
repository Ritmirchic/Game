-- Enemy species definitions and the AI decision machine.

local Enemies = require("EnemyDefinitions")
local AI = require("EnemyAI")
local Config = require("Config")
local Util = require("Util")

local function species()
	local out = {}
	for id, def in pairs(Enemies) do
		if type(def) == "table" then
			out[id] = def
		end
	end
	return out
end

local function perceptionFor(def, overrides)
	local base = {
		dt = 0.25,
		position = Vector3.new(0, 0, 0),
		direction = Vector3.new(0, 0, -1),
		target = {
			id = "player", position = Vector3.new(0, 0, -40), isSprinting = false,
			isCrouching = false, lightOn = false,
		},
		hasLineOfSight = true,
		distance = 40,
		owners = 1,
		health = 1,
		night = false,
		foggy = false,
		noiseEvents = {},
		time = 1,
	}
	for key, value in pairs(overrides or {}) do
		base[key] = value
	end
	return base
end

describe("EnemyDefinitions", function()
	it("defines the ten original species", function()
		local list = species()
		local count = 0
		for _ in pairs(list) do count += 1 end
		eq(count, 10, "expected exactly ten species, got " .. count)
		for _, id in ipairs({ "hollow", "crawler", "watcher", "screamer", "brute",
			"mimic", "burrower", "stalker", "contaminated", "pale_one" }) do
			expect(list[id] ~= nil, "missing species " .. id)
		end
	end)

	it("every species has a complete stat block", function()
		for id, def in pairs(species()) do
			expect(def.name and #def.name > 0, "no name on " .. id)
			expect(def.health > 0, "bad health on " .. id)
			expect(def.speed > 0, "bad speed on " .. id)
			expect(def.damage > 0, "bad damage on " .. id)
			expect(def.attackRange > 0, "bad attack range on " .. id)
			expect(def.vision > 0, "bad vision on " .. id)
			expect(#def.habitat > 0, "no habitat on " .. id)
			expect(#def.tiers > 0, "no spawn tiers on " .. id)
			expect(def.color ~= nil, "no colour on " .. id)
			expect(def.desc and #def.desc > 20, "missing description on " .. id)
		end
	end)

	it("habitats and tiers reference real regions and valid tiers", function()
		local regions = {}
		for name in pairs(Config.Regions) do regions[name] = true end
		for id, def in pairs(species()) do
			for _, region in ipairs(def.habitat) do
				expect(regions[region], "species " .. id .. " lives in unknown region " .. region)
			end
			for _, tier in ipairs(def.tiers) do
				-- the world bakes spawn anchors at tiers 1..6
				expect(tier >= 1 and tier <= 6, "species " .. id .. " has invalid tier " .. tier)
			end
		end
	end)

	it("drops reference real items", function()
		local Items = require("ItemDefinitions")
		for id, def in pairs(species()) do
			for _, item in ipairs(def.drops or {}) do
				expect(Items[item] ~= nil, "species " .. id .. " drops unknown item " .. item)
			end
			expect((def.lootChance or 0) >= 0 and (def.lootChance or 0) <= 1)
		end
	end)

	it("spawn weights respect region, tier and night", function()
		local forestTier1 = AI.spawnWeights("Forest", 1, false)
		expect(#forestTier1 > 0, "no tier 1 forest spawns")
		for _, entry in ipairs(forestTier1) do
			expect(Util.tableContains(entry.def.habitat, "Forest"))
			expect(entry.def.nocturnal ~= true, "nocturnal species offered in daylight")
		end
		local night = AI.spawnWeights("Forest", 1, true)
		expect(#night >= #forestTier1)
	end)
end)

describe("EnemyAI: perception", function()
	it("chases what it can see", function()
		local def = Enemies.crawler
		local memory = AI.newMemory(Vector3.new(0, 0, 0))
		local action
		for step = 1, 8 do
			action = AI.update(def, memory, perceptionFor(def, { time = step * 0.25 }))
		end
		expect(action.state == "Chase" or action.state == "Attack",
			"sighted crawler did not commit to a chase, got " .. tostring(action.state))
		expect(action.moveTo ~= nil or action.attack ~= nil)
	end)

	it("attacks at melee range", function()
		local def = Enemies.hollow
		local memory = AI.newMemory(Vector3.new(0, 0, 0))
		memory.alert = 1
		memory.lastSeen = Vector3.new(0, 0, -3)
		memory.lastSeenAt = 1
		local action = AI.update(def, memory, perceptionFor(def, {
			distance = 3, target = { id = "p", position = Vector3.new(0, 0, -3) }, time = 1,
		}))
		eq(action.state, "Attack")
		eq(action.attack, "melee")
	end)

	it("blind species hunt by noise", function()
		local def = Enemies.hollow
		expect(def.hearing > 1, "hollow should have acute hearing")
		local memory = AI.newMemory(Vector3.new(0, 0, 0))
		local action
		for step = 1, 10 do
			action = AI.update(def, memory, perceptionFor(def, {
				time = step * 0.25, hasLineOfSight = false, distance = 60,
				target = { id = "p", position = Vector3.new(0, 0, -60) },
				noiseEvents = { { position = Vector3.new(0, 0, -30), loudness = 60 } },
			}))
		end
		expect(memory.alert > 0.3, "hollow ignored a loud noise")
		expect(action.moveTo ~= nil or action.state == "Investigate" or action.state == "Search"
			or action.state == "Chase", "hollow did not move toward the noise (" .. tostring(action.state) .. ")")
	end)

	it("blind species do not react to line of sight alone", function()
		local def = Enemies.hollow
		expect(def.behaviour.blind == true, "hollow should be blind")
		local memory = AI.newMemory(Vector3.new(0, 0, 0))
		for step = 1, 6 do
			AI.update(def, memory, perceptionFor(def, {
				time = step * 0.25, hasLineOfSight = true, target = nil,
			}))
		end
		expect(memory.state ~= "Attack")
	end)

	it("calm creatures stay near home", function()
		local def = Enemies.crawler
		local memory = AI.newMemory(Vector3.new(10, 0, 10))
		local action = AI.update(def, memory, perceptionFor(def, {
			distance = 500, hasLineOfSight = false, target = nil, time = 1,
		}))
		expect(action.state == "Patrol" or action.state == "Idle", "state was " .. tostring(action.state))
		if action.moveTo then
			expect((action.moveTo - memory.home).Magnitude < 120)
		end
	end)

	it("returns home when it wanders too far", function()
		local def = Enemies.crawler
		local memory = AI.newMemory(Vector3.new(0, 0, 0))
		local action = AI.update(def, memory, perceptionFor(def, {
			position = Vector3.new(400, 0, 400), distance = 500, hasLineOfSight = false,
			target = nil, time = 1,
		}))
		eq(action.state, "Return")
	end)
end)

describe("EnemyAI: special behaviours", function()
	it("nocturnal creatures idle in daylight", function()
		local nocturnal
		for _, def in pairs(species()) do
			if def.nocturnal then nocturnal = def break end
		end
		expect(nocturnal ~= nil, "no nocturnal species defined")
		local memory = AI.newMemory(Vector3.new(0, 0, 0))
		local action = AI.update(nocturnal, memory, perceptionFor(nocturnal, { night = false }))
		eq(action.state, "Idle")
	end)

	it("the watcher stalks and withdraws instead of charging", function()
		local def = Enemies.watcher
		local memory = AI.newMemory(Vector3.new(0, 0, 0))
		local action = AI.update(def, memory, perceptionFor(def, { distance = 60 }))
		eq(action.state, "Stalk")
		action = AI.update(def, memory, perceptionFor(def, { distance = 20, time = 2 }))
		expect(action.flee == true, "watcher did not withdraw when approached")
	end)

	it("the brute charges at range", function()
		local def = Enemies.brute
		local memory = AI.newMemory(Vector3.new(0, 0, 0))
		memory.alert = 1
		local action
		for step = 1, 4 do
			action = AI.update(def, memory, perceptionFor(def, { distance = 60, time = step }))
		end
		expect(action.charge == true or memory.cooldowns.charge ~= nil, "brute never charged")
	end)

	it("the screamer calls a horde", function()
		local def = Enemies.screamer
		local memory = AI.newMemory(Vector3.new(0, 0, 0))
		local action = AI.update(def, memory, perceptionFor(def, { distance = 50 }))
		eq(action.callHorde, true)
	end)

	it("the stalker retreats when badly hurt", function()
		local def = Enemies.stalker
		local memory = AI.newMemory(Vector3.new(0, 0, 0))
		memory.alert = 1
		local action = AI.update(def, memory, perceptionFor(def, { distance = 35, health = 0.1 }))
		eq(action.state, "Retreat")
		expect(action.flee == true)
	end)

	it("the mimic lures instead of chasing", function()
		local def = Enemies.mimic
		local memory = AI.newMemory(Vector3.new(0, 0, 0))
		local action = AI.update(def, memory, perceptionFor(def, { distance = 90, hasLineOfSight = false }))
		expect(action.sound == "mimic_lure" or memory.cooldowns.lure ~= nil,
			"mimic never made a luring sound")
	end)

	it("the burrower erupts from below", function()
		local def = Enemies.burrower
		local memory = AI.newMemory(Vector3.new(0, 0, 0))
		local action
		for step = 1, 3 do
			action = AI.update(def, memory, perceptionFor(def, { distance = 20, time = step * 0.5 }))
		end
		expect(action.state == "Burrow" or action.state == "Attack",
			"burrower state was " .. tostring(action.state))
	end)

	it("despawns when far away and calm", function()
		local memory = AI.newMemory(Vector3.new(0, 0, 0))
		memory.state = "Patrol"
		eq(AI.shouldDespawn({ distance = 600 }, memory), true)
		memory.state = "Chase"
		eq(AI.shouldDespawn({ distance = 600 }, memory), false)
	end)
end)
