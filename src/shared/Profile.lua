--!strict
--[[
	Profile — the save/restore shape for a player, plus migration + validation.

	Kept pure so that data corruption, partial saves and schema upgrades can be
	tested offline.
]]

local Config = require(script.Parent.Config)
local Inventory = require(script.Parent.InventoryModel)
local QuestLogic = require(script.Parent.QuestLogic)
local SurvivalMath = require(script.Parent.SurvivalMath)
local Items = require(script.Parent.ItemDefinitions)

local Profile = {}

Profile.SchemaVersion = 3

function Profile.new(playerName)
	return {
		version = Profile.SchemaVersion,
		name = playerName or "Survivor",
		createdAt = os.time(),
		playtime = 0,
		inventory = Inventory.newState(),
		survival = SurvivalMath.newState(),
		quests = QuestLogic.newState(),
		blueprints = {},
		stats = { kills = 0, deaths = 0, distance = 0, itemsFound = 0, crafted = 0, built = 0, documents = 0 },
		settings = { quality = "High", sensitivity = 0.55, fov = 72, headBob = true, shake = true, invertY = false },
		position = nil,
		safehouse = nil,
		radioState = {},
		tutorialStep = 1,
		day = 1,
	}
end

-- Accepts anything from DataStore and always returns a valid profile.
function Profile.normalize(raw, playerName)
	local fresh = Profile.new(playerName)
	if type(raw) ~= "table" then
		return fresh, true
	end
	local migrated = false

	-- migrate older versions
	local version = tonumber(raw.version) or 1
	if version < Profile.SchemaVersion then
		migrated = true
		if version < 2 then
			raw.survival = raw.survival or {}
			raw.survival.stress = raw.survival.stress or 10
		end
		if version < 3 then
			raw.blueprints = raw.blueprints or {}
			raw.radioState = raw.radioState or {}
		end
		raw.version = Profile.SchemaVersion
	end

	-- inventory (validated item by item inside InventoryModel.deserialize)
	if type(raw.inventory) == "table" then
		fresh.inventory = Inventory.deserialize(raw.inventory)
	else
		migrated = true
	end

	-- survival
	if type(raw.survival) == "table" then
		for key, value in pairs(raw.survival) do
			if type(value) == "number" and fresh.survival[key] ~= nil then
				fresh.survival[key] = value
			elseif key == "bodyParts" and type(value) == "table" then
				for part, hp in pairs(value) do
					if type(hp) == "number" and fresh.survival.bodyParts[part] ~= nil then
						fresh.survival.bodyParts[part] = hp
					end
				end
			elseif key == "alive" or key == "causeOfDeath" then
				fresh.survival[key] = value
			end
		end
		if fresh.survival.health <= 0 then
			-- never load a dead profile; the player respawns in the shelter
			fresh.survival.health = Config.Survival.MaxHealth * 0.45
			fresh.survival.bleeding = 0
		end
	end

	-- quests
	fresh.quests = QuestLogic.deserialize(raw.quests)

	-- simple copied fields with type checks
	for _, key in ipairs({ "name", "playtime", "position", "safehouse", "tutorialStep", "day" }) do
		if type(raw[key]) == type(fresh[key]) or (key == "day" and type(raw[key]) == "number") then
			fresh[key] = raw[key]
		elseif raw[key] ~= nil then
			migrated = true
		end
	end
	if type(raw.blueprints) == "table" then
		for blueprint, known in pairs(raw.blueprints) do
			if type(blueprint) == "string" and known then
				fresh.blueprints[blueprint] = true
			end
		end
	end
	if type(raw.radioState) == "table" then
		for id, heard in pairs(raw.radioState) do
			if type(id) == "string" and heard then
				fresh.radioState[id] = true
			end
		end
	end
	if type(raw.stats) == "table" then
		for key, value in pairs(raw.stats) do
			if type(value) == "number" and fresh.stats[key] ~= nil then
				fresh.stats[key] = math.max(0, value)
			end
		end
	end
	if type(raw.settings) == "table" then
		for key, value in pairs(raw.settings) do
			if fresh.settings[key] ~= nil and type(value) == type(fresh.settings[key]) then
				fresh.settings[key] = value
			end
		end
	end

	-- final sanity pass
	fresh.survival = fresh.survival or SurvivalMath.newState()
	SurvivalMath.clampState(fresh.survival)
	SurvivalMath.deriveConditions(fresh.survival)
	return fresh, migrated
end

-- Remove obviously invalid entries (defensive; also used by tests)
function Profile.validate(profile)
	local removed = 0
	for _, container in pairs(profile.inventory.containers) do
		for uid, entry in pairs(container.items) do
			if type(entry) ~= "table" or not Items[entry.itemId] then
				container.items[uid] = nil
				removed += 1
			end
		end
	end
	for slot, entry in pairs(profile.inventory.equipment) do
		if type(entry) ~= "table" or not Items[entry.itemId] then
			profile.inventory.equipment[slot] = nil
			removed += 1
		end
	end
	return removed
end

function Profile.snapshotForClient(profile)
	return {
		name = profile.name,
		day = profile.day,
		playtime = profile.playtime,
		stats = profile.stats,
		survival = profile.survival,
		inventory = Inventory.serialize(profile.inventory),
		quests = QuestLogic.serialize(profile.quests),
		blueprints = profile.blueprints,
		settings = profile.settings,
		radioState = profile.radioState,
		documentsRead = profile.documentsRead,
		tutorialStep = profile.tutorialStep,
	}
end

return Profile
