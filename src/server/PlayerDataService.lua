--!strict
--[[
	PlayerDataService — persistence.

	* DataStore with retries and exponential backoff
	* UpdateAsync writes with a session token (prevents two servers clobbering)
	* autosave loop, save-on-leave, save on BindToClose
	* graceful degradation: if DataStore is unavailable the game still runs with
	  in-memory profiles (and says so once, instead of erroring every tick)
]]

local Players = game:GetService("Players")
local DataStoreService = game:GetService("DataStoreService")
local RunService = game:GetService("RunService")
local ReplicatedStorage = game:GetService("ReplicatedStorage")

local Shared = ReplicatedStorage:WaitForChild("Shared")
local Config = require(Shared.Config)
local Profile = require(Shared.Profile)

local PlayerDataService = {}

local profiles = {}
local store
local storeHealthy = true
local sessionToken = tostring(os.time()) .. "-" .. tostring(math.random(100000, 999999))
local dirty = {}
local lastSave = 0
local registryRef

local function logOnce(key, message)
	PlayerDataService._logged = PlayerDataService._logged or {}
	if PlayerDataService._logged[key] then return end
	PlayerDataService._logged[key] = true
	warn("[PlayerData] " .. message)
end

local function callStore(fn, ...)
	if not store then return nil, "no datastore" end
	local attempts = 0
	local delay = 1
	while attempts < 4 do
		attempts += 1
		local ok, result = pcall(fn, ...)
		if ok then
			storeHealthy = true
			return result
		end
		local message = tostring(result)
		if string.find(message, "not allowed") or string.find(message, "Studio") then
			storeHealthy = false
			logOnce("studio", "DataStore unavailable in this environment; using in-memory profiles.")
			return nil, message
		end
		if attempts >= 4 then
			storeHealthy = false
			logOnce("retry", "DataStore write failed after retries: " .. message)
			return nil, message
		end
		task.wait(delay)
		delay *= 2
	end
	return nil
end

local function keyFor(player)
	return "player_" .. tostring(player.UserId)
end

function PlayerDataService.Init(registry)
	registryRef = registry
	local ok, result = pcall(function()
		return DataStoreService:GetDataStore("AshesOfHumanity_v3")
	end)
	if ok then
		store = result
	else
		store = nil
		logOnce("nostore", "could not open DataStore: " .. tostring(result))
	end
end

function PlayerDataService.Start(registry)
	Players.PlayerAdded:Connect(function(player)
		PlayerDataService.Load(player)
	end)
	Players.PlayerRemoving:Connect(function(player)
		PlayerDataService.Save(player, true)
		profiles[player.UserId] = nil
		dirty[player.UserId] = nil
	end)
	for _, player in ipairs(Players:GetPlayers()) do
		task.spawn(function()
			PlayerDataService.Load(player)
		end)
	end

	task.spawn(function()
		while true do
			task.wait(5)
			local now = os.clock()
			if now - lastSave >= Config.SaveInterval then
				lastSave = now
				PlayerDataService.SaveAll()
			end
		end
	end)
end

function PlayerDataService.Shutdown()
	PlayerDataService.SaveAll()
end

function PlayerDataService.Load(player)
	if profiles[player.UserId] then
		return profiles[player.UserId]
	end
	local data, err
	if store then
		data = callStore(function()
			return store:GetAsync(keyFor(player))
		end)
		if err then
			logOnce("loadfail", "load failed for " .. player.Name .. ": " .. tostring(err))
		end
	end
	local profile = Profile.normalize(data, player.Name)
	if data == nil then
		-- first time on this server: a minimal kit so the first minutes are
		-- survivable. Everything else must be found in the world.
		local Inventory = require(Shared.InventoryModel)
		local STARTER = {
			{ "knife", 1 }, { "bandage", 2 }, { "water_bottle", 1 },
			{ "flashlight", 1 }, { "battery", 1 }, { "rags", 2 },
		}
		for _, entry in ipairs(STARTER) do
			Inventory.addItem(profile.inventory, entry[1], entry[2])
		end
		profile.starterKit = true
	end
	local removed = Profile.validate(profile)
	if removed > 0 then
		warn(string.format("[PlayerData] removed %d invalid items from %s's profile", removed, player.Name))
	end
	profiles[player.UserId] = profile
	profile.loaded = true
	local service = registryRef and registryRef.DirectorService
	if service and service.OnProfileReady then
		service.OnProfileReady(player, profile)
	end
	return profile
end

function PlayerDataService.Get(player)
	return profiles[player.UserId]
end

function PlayerDataService.GetByUserId(userId)
	return profiles[userId]
end

function PlayerDataService.MarkDirty(player)
	dirty[player.UserId] = true
end

function PlayerDataService.Save(player, final)
	local profile = profiles[player.UserId]
	if not profile then return false end
	if not store then
		return false
	end
	local payload = {
		version = profile.version,
		name = profile.name,
		playtime = profile.playtime,
		inventory = require(Shared.InventoryModel).serialize(profile.inventory),
		survival = profile.survival,
		quests = require(Shared.QuestLogic).serialize(profile.quests),
		blueprints = profile.blueprints,
		stats = profile.stats,
		settings = profile.settings,
		position = profile.position,
		safehouse = profile.safehouse,
		radioState = profile.radioState,
		tutorialStep = profile.tutorialStep,
		day = profile.day,
		session = sessionToken,
		savedAt = os.time(),
	}
	local ok = callStore(function()
		return store:UpdateAsync(keyFor(player), function(old)
			if type(old) == "table" and old.session and old.session ~= sessionToken
				and (os.time() - (old.savedAt or 0)) < 20 and not final then
				-- another server wrote very recently; keep their data instead
				return old
			end
			return payload
		end)
	end)
	if ok then
		dirty[player.UserId] = nil
	end
	return ok ~= nil
end

function PlayerDataService.SaveAll()
	for _, player in ipairs(Players:GetPlayers()) do
		local ok, err = pcall(PlayerDataService.Save, player, false)
		if not ok then
			warn("[PlayerData] save error: " .. tostring(err))
		end
	end
end

function PlayerDataService.ForEachProfile(fn)
	for _, profile in pairs(profiles) do
		fn(profile)
	end
end

function PlayerDataService.Count()
	local n = 0
	for _ in pairs(profiles) do n += 1 end
	return n
end

function PlayerDataService.IsStoreHealthy()
	return storeHealthy and store ~= nil
end

return PlayerDataService
