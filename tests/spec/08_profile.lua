-- Save data: creation, corruption recovery, migration, anti-cheat sanitation.

local Profile = require("Profile")
local Inventory = require("InventoryModel")
local Items = require("ItemDefinitions")
local Config = require("Config")

describe("Profile: creation", function()
	it("creates a schema-complete profile", function()
		local profile = Profile.new("TestSurvivor")
		eq(profile.version, Profile.SchemaVersion)
		eq(profile.name, "TestSurvivor")
		eq(profile.survival.health, Config.Survival.MaxHealth)
		expect(profile.inventory.containers.Backpack ~= nil)
		expect(profile.quests.active ~= nil)
		expect(type(profile.stats.kills) == "number")
		expect(type(profile.settings.quality) == "string")
	end)
end)

describe("Profile: normalisation", function()
	it("returns a fresh profile for garbage input", function()
		local profile, migrated = Profile.normalize("not a table", "X")
		eq(profile.name, "X")
		eq(migrated, true)
		profile = Profile.normalize(nil, "Y")
		eq(profile.name, "Y")
	end)

	it("ignores unknown fields instead of crashing", function()
		local profile = Profile.normalize({
			version = Profile.SchemaVersion,
			name = "A",
			survival = { health = 50, totallyNewStat = 999 },
			junk = { nested = true },
		}, "A")
		eq(profile.survival.health, 50)
		eq(profile.survival.totallyNewStat, nil)
		eq(profile.junk, nil)
	end)

	it("clamps impossible statistics", function()
		local profile = Profile.normalize({
			version = Profile.SchemaVersion,
			survival = { health = 99999, hunger = -500, temperature = 300 },
			stats = { kills = -12 },
		}, "B")
		expect(profile.survival.health <= 100)
		expect(profile.survival.hunger >= 0)
		expect(profile.survival.temperature <= 45)
		eq(profile.stats.kills, 0)
	end)

	it("resurrects a dead saved profile", function()
		local profile = Profile.normalize({
			version = Profile.SchemaVersion,
			survival = { health = 0, alive = false, bleeding = 5 },
		}, "C")
		expect(profile.survival.health > 0, "loaded a dead profile")
		eq(profile.survival.bleeding, 0)
	end)

	it("drops inventory entries for deleted items", function()
		local raw = {
			version = Profile.SchemaVersion,
			inventory = {
				containers = {
					Backpack = {
						cols = 6, rows = 6,
						items = {
							good = { uid = "good", itemId = "bandage", x = 1, y = 1, count = 2 },
							bad = { uid = "bad", itemId = "item_removed_in_v2", x = 3, y = 1, count = 99 },
						},
					},
				},
				equipment = {},
			},
		}
		local profile = Profile.normalize(raw, "D")
		-- deserialisation already refuses unknown item ids
		eq(Inventory.countItem(profile.inventory, "bandage"), 2)
		eq(Inventory.findUid(profile.inventory, "bad"), nil)
		-- and validate() is a second line of defence for injected entries
		profile.inventory.containers.Backpack.items.hacked = {
			uid = "hacked", itemId = "dev_weapon_that_does_not_exist", x = 5, y = 5, count = 1,
		}
		eq(Profile.validate(profile), 1)
		eq(Inventory.findUid(profile.inventory, "hacked"), nil)
	end)
end)

describe("Profile: migration", function()
	it("upgrades a version 1 save", function()
		local profile, migrated = Profile.normalize({
			version = 1,
			name = "Old",
			survival = { health = 70 },
			stats = { kills = 3 },
		}, "Old")
		eq(migrated, true)
		eq(profile.version, Profile.SchemaVersion)
		eq(profile.survival.health, 70)
		eq(profile.stats.kills, 3)
		expect(type(profile.blueprints) == "table")
		expect(type(profile.radioState) == "table")
	end)

	it("keeps current-version saves intact", function()
		local original = Profile.new("Current")
		Inventory.addItem(original.inventory, "rifle", 1)
		original.survival.hunger = 42
		original.stats.kills = 17
		original.blueprints.some_blueprint = true
		local restored = Profile.normalize(original, "Current")
		eq(restored.stats.kills, 17)
		near(restored.survival.hunger, 42, 1)
		expect(restored.blueprints.some_blueprint == true)
		eq(Inventory.countItem(restored.inventory, "rifle"), 1)
	end)

	it("the starter kit only uses real items", function()
		-- mirrors PlayerDataService.Load's first-join kit
		local STARTER = { "knife", "bandage", "water_bottle", "flashlight", "battery", "rags" }
		for _, id in ipairs(STARTER) do
			expect(Items[id] ~= nil, "starter item does not exist: " .. id)
		end
		local profile = Profile.new("Kit")
		for _, id in ipairs(STARTER) do
			expect(Inventory.addItem(profile.inventory, id, 1), "could not add " .. id)
		end
	end)

	it("the client snapshot never leaks private fields", function()
		local profile = Profile.new("Snapshot")
		local snapshot = Profile.snapshotForClient(profile)
		eq(snapshot.position, nil)
		eq(snapshot.safehouse, nil)
		expect(snapshot.survival ~= nil)
		expect(snapshot.inventory ~= nil)
		expect(snapshot.quests ~= nil)
	end)

	it("a snapshot round trip restores the visible state", function()
		local profile = Profile.new("Round")
		Inventory.addItem(profile.inventory, "bandage", 3)
		profile.survival.hunger = 55
		local snapshot = Profile.snapshotForClient(profile)
		eq(Inventory.countItem(Inventory.deserialize(snapshot.inventory), "bandage"), 3)
		near(snapshot.survival.hunger, 55, 0.01)
	end)
end)
