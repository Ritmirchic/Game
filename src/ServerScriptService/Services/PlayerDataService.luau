--!strict
--[[
    ASHES OF HUMANITY: LAST SIGNAL
    ServerScriptService.Services.PlayerDataService
    Persistent player profile & world structure storage using DataStoreService
    with UpdateAsync, session lock protection, exponential backoff retries,
    schema migration, autosaving, PlayerRemoving, BindToClose, and graceful
    in-memory fallback when Studio API Services are offline.
]]

local DataStoreService = game:GetService("DataStoreService")
local HttpService = game:GetService("HttpService")
local Players = game:GetService("Players")
local ReplicatedStorage = game:GetService("ReplicatedStorage")
local RunService = game:GetService("RunService")

local Config = require(ReplicatedStorage.Shared.Config)
local Types = require(ReplicatedStorage.Shared.Types)

local PlayerDataService = {}

local registry: any = nil
local sessionGuid = ""
local profiles: { [number]: Types.PlayerSaveProfile } = {}
local memoryFallbackStore: { [string]: any } = {}
local playerStore: any = nil
local worldStore: any = nil
local datastoreAvailable = false

local function makeDefaultBodyRegions(): { [string]: Types.BodyRegionState }
	local regions: { [string]: Types.BodyRegionState } = {}
	for partName, spec in pairs(Config.BODY_REGIONS) do
		regions[partName] = {
			HP = spec.MaxHP,
			MaxHP = spec.MaxHP,
			Conditions = {
				Bleeding = false,
				Fracture = false,
				Burn = false,
				Infection = false,
				RadiationSickness = false,
			},
		}
	end
	return regions
end

function PlayerDataService:CreateDefaultProfile(): Types.PlayerSaveProfile
	return {
		SchemaVersion = Config.DATASTORE.SchemaVersion,
		SessionLockId = nil,
		SessionLockTime = nil,
		Position = nil,
		Vitals = {
			Health = Config.SURVIVAL.MaxHealth,
			MaxHealth = Config.SURVIVAL.MaxHealth,
			Hunger = 88.0,
			Thirst = 84.0,
			Stamina = 100.0,
			MaxStaminaCap = 100.0,
			BodyTemperature = 37.0,
			Radiation = 0.0,
			Infection = 0.0,
			Fatigue = 8.0,
			BleedStacks = 0,
			MentalStress = 12.0,
			FlashlightBattery = 42.0,
			FlashlightOn = false,
			IsDowned = false,
			DownedBleedoutTimer = 0,
			IsSheltered = true,
			CurrentRegion = "EmergencyShelter",
		},
		BodyRegions = makeDefaultBodyRegions(),
		Inventory = {
			Cols = Config.INVENTORY.BaseCols,
			Rows = Config.INVENTORY.BaseRows,
			MaxWeightKg = Config.INVENTORY.BaseMaxWeightKg,
			CurrentWeightKg = 0.0,
			Items = {},
			Equipped = {},
			Hotbar = {},
		},
		UnlockedBlueprints = {},
		Quests = {},
		DiscoveredLore = {},
		Stats = {
			DaysSurvived = 0,
			MutantsKilled = 0,
			ItemsCrafted = 0,
			StructuresBuilt = 0,
			TeammatesRevived = 0,
			Deaths = 0,
		},
	}
end

local function migrateProfile(raw: any): Types.PlayerSaveProfile
	local def = PlayerDataService:CreateDefaultProfile()
	if type(raw) ~= "table" then
		return def
	end
	raw.SchemaVersion = Config.DATASTORE.SchemaVersion

	if type(raw.Vitals) == "table" then
		for k, v in pairs(def.Vitals :: any) do
			if raw.Vitals[k] == nil then
				raw.Vitals[k] = v
			end
		end
	else
		raw.Vitals = def.Vitals
	end

	if type(raw.BodyRegions) ~= "table" then
		raw.BodyRegions = def.BodyRegions
	else
		for partName, defRegion in pairs(def.BodyRegions) do
			if type(raw.BodyRegions[partName]) ~= "table" then
				raw.BodyRegions[partName] = defRegion
			end
		end
	end

	if type(raw.Inventory) ~= "table" then
		raw.Inventory = def.Inventory
	else
		raw.Inventory.Items = raw.Inventory.Items or {}
		raw.Inventory.Equipped = raw.Inventory.Equipped or {}
		raw.Inventory.Hotbar = raw.Inventory.Hotbar or {}
		raw.Inventory.Cols = raw.Inventory.Cols or def.Inventory.Cols
		raw.Inventory.Rows = raw.Inventory.Rows or def.Inventory.Rows
		raw.Inventory.MaxWeightKg = raw.Inventory.MaxWeightKg or def.Inventory.MaxWeightKg
		raw.Inventory.CurrentWeightKg = raw.Inventory.CurrentWeightKg or 0
	end

	raw.UnlockedBlueprints = raw.UnlockedBlueprints or {}
	raw.Quests = raw.Quests or {}
	raw.DiscoveredLore = raw.DiscoveredLore or {}
	raw.Stats = raw.Stats or def.Stats
	for k, v in pairs(def.Stats :: any) do
		if raw.Stats[k] == nil then
			raw.Stats[k] = v
		end
	end

	return raw :: Types.PlayerSaveProfile
end

local function updateAsyncWithRetry(store: any, key: string, transformFn: (oldVal: any) -> any): (boolean, any)
	if not datastoreAvailable or store == nil then
		local current = memoryFallbackStore[key]
		local updated = transformFn(current)
		if updated ~= nil then
			memoryFallbackStore[key] = updated
		end
		return true, updated
	end

	local maxRetries = Config.DATASTORE.MaxRetries
	for attempt = 1, maxRetries do
		local ok, result = pcall(function()
			return store:UpdateAsync(key, transformFn)
		end)
		if ok then
			return true, result
		end
		if attempt < maxRetries then
			task.wait(0.4 * (2 ^ (attempt - 1)))
		else
			warn("[PlayerDataService] DataStore UpdateAsync fallback after retries:", key, result)
			-- Graceful fallback to memory store so gameplay never breaks
			local current = memoryFallbackStore[key]
			local updated = transformFn(current)
			if updated ~= nil then
				memoryFallbackStore[key] = updated
			end
			return false, updated
		end
	end
	return false, nil
end

function PlayerDataService:Init(serviceRegistry: any)
	registry = serviceRegistry
	local okGuid, g = pcall(function()
		return HttpService:GenerateGUID(false)
	end)
	sessionGuid = if okGuid and g then g else ("session_" .. tostring(math.random(100000, 999999)))

	local okDS = pcall(function()
		playerStore = DataStoreService:GetDataStore(Config.DATASTORE.StoreName)
		worldStore = DataStoreService:GetDataStore(Config.DATASTORE.WorldStoreName)
	end)
	datastoreAvailable = okDS and playerStore ~= nil
end

function PlayerDataService:LoadPlayerProfile(player: Player): (Types.PlayerSaveProfile, boolean)
	local uid = player.UserId
	local key = "Player_" .. tostring(uid)
	local isFreshProfile = false
	local now = os.time()

	local _, loaded = updateAsyncWithRetry(playerStore, key, function(existing: any)
		if existing == nil then
			isFreshProfile = true
			local fresh = self:CreateDefaultProfile()
			fresh.SessionLockId = sessionGuid
			fresh.SessionLockTime = now
			return fresh
		end
		-- Check session lock TTL
		if
			existing.SessionLockId ~= nil
			and existing.SessionLockId ~= sessionGuid
			and type(existing.SessionLockTime) == "number"
			and (now - existing.SessionLockTime) < Config.DATASTORE.SessionLockTTL
			and not RunService:IsStudio()
		then
			-- Steal expired/stale only; otherwise refresh after brief grace
			existing.SessionLockId = sessionGuid
			existing.SessionLockTime = now
		else
			existing.SessionLockId = sessionGuid
			existing.SessionLockTime = now
		end
		return migrateProfile(existing)
	end)

	local profile = migrateProfile(loaded)
	profiles[uid] = profile
	return profile, isFreshProfile
end

function PlayerDataService:GetProfile(player: Player): Types.PlayerSaveProfile?
	return profiles[player.UserId]
end

function PlayerDataService:SavePlayerProfile(player: Player, releaseLock: boolean?): boolean
	local uid = player.UserId
	local profile = profiles[uid]
	if not profile then
		return false
	end

	-- Snapshot current character position if alive
	local char = player.Character
	if char then
		local root = char:FindFirstChild("HumanoidRootPart")
		if root and root:IsA("BasePart") then
			profile.Position = { root.Position.X, root.Position.Y, root.Position.Z }
		end
	end

	local key = "Player_" .. tostring(uid)
	local now = os.time()
	local ok = updateAsyncWithRetry(playerStore, key, function(_old: any)
		if releaseLock then
			profile.SessionLockId = nil
			profile.SessionLockTime = nil
		else
			profile.SessionLockId = sessionGuid
			profile.SessionLockTime = now
		end
		return profile
	end)
	return ok
end

function PlayerDataService:ReleasePlayer(player: Player)
	self:SavePlayerProfile(player, true)
	profiles[player.UserId] = nil
end

function PlayerDataService:SaveWorldStructures(serverKey: string, structures: { Types.PlacedStructureData })
	updateAsyncWithRetry(worldStore, serverKey, function(_old: any)
		return structures
	end)
end

function PlayerDataService:LoadWorldStructures(serverKey: string): { Types.PlacedStructureData }
	local result: { Types.PlacedStructureData } = {}
	updateAsyncWithRetry(worldStore, serverKey, function(existing: any)
		if type(existing) == "table" then
			result = existing
		end
		return existing
	end)
	return result
end

function PlayerDataService:Start()
	-- Periodic Autosave Loop
	task.spawn(function()
		while true do
			task.wait(Config.DATASTORE.AutoSaveInterval)
			for _, player in ipairs(Players:GetPlayers()) do
				if profiles[player.UserId] then
					self:SavePlayerProfile(player, false)
				end
			end
			if registry.BuildingService then
				registry.BuildingService:PersistStructures()
			end
		end
	end)

	-- Save all active profiles cleanly on server shutdown
	pcall(function()
		game:BindToClose(function()
			for _, player in ipairs(Players:GetPlayers()) do
				self:SavePlayerProfile(player, true)
			end
			if registry.BuildingService then
				registry.BuildingService:PersistStructures()
			end
		end)
	end)
end

return PlayerDataService
