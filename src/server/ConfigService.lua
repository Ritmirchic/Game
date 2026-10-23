--!strict
--[[
	ConfigService — pushes static config to clients, stores per-player settings,
	and exposes a couple of server-side tuning endpoints for live balancing.
]]

local Players = game:GetService("Players")
local ReplicatedStorage = game:GetService("ReplicatedStorage")

local Shared = ReplicatedStorage:WaitForChild("Shared")
local Config = require(Shared.Config)
local Items = require(Shared.ItemDefinitions)
local Weapons = require(Shared.WeaponDefinitions)
local Enemies = require(Shared.EnemyDefinitions)
local Recipes = require(Shared.Recipes)
local Build = require(Shared.BuildDefinitions)
local Quests = require(Shared.QuestDefinitions)

local ConfigService = {}

local registryRef
local remotes

function ConfigService.Init(registry)
	registryRef = registry
	remotes = registry.Remotes
end

function ConfigService.Start(registry)
	remotes.On("RequestSettings", function(player, settings)
		if type(settings) ~= "table" then return end
		local profile = registryRef.PlayerDataService.Get(player)
		if not profile then return end
		local allowed = {
			quality = { "Low", "Medium", "High", "Ultra" },
			headBob = true, shake = true, invertY = true,
		}
		for key, value in pairs(settings) do
			if key == "quality" and type(value) == "string" and table.find(allowed.quality, value) then
				profile.settings.quality = value
			elseif key == "sensitivity" and type(value) == "number" then
				profile.settings.sensitivity = math.clamp(value, 0.05, 2)
			elseif key == "fov" and type(value) == "number" then
				profile.settings.fov = math.clamp(value, 55, 105)
			elseif (key == "headBob" or key == "shake" or key == "invertY") and type(value) == "boolean" then
				profile.settings[key] = value
			elseif key == "flashlight" and type(value) == "boolean" then
				profile.settings.flashlight = value
			end
		end
		registryRef.PlayerDataService.MarkDirty(player)
		remotes.Fire(player, "Notify", "Settings saved.", "info", 2)
	end)

	remotes.On("RequestPing", function(player, position)
		if typeof(position) ~= "Vector3" then return end
		remotes.FireNear(position, 400, "MinimapPing", position, "player", player.Name)
	end)
end

function ConfigService.Static()
	return {
		title = Config.Title,
		version = Config.Version,
		regions = Config.Regions,
		keys = Config.Keys,
		grid = Config.Grid,
		movement = Config.Movement,
		quality = Config.Quality,
		items = Items,
		weapons = Weapons,
		enemies = Enemies,
		recipes = Recipes,
		build = Build,
		quests = Quests,
	}
end

return ConfigService
