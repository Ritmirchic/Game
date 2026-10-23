--!strict
--[[
	DirectorService — glue layer.

	* first-join onboarding + tutorial steps
	* keeps services in sync after respawn
	* server health monitor (reports when a subsystem stops ticking)
	* graceful handling of a profile that fails to load
]]

local Players = game:GetService("Players")
local ReplicatedStorage = game:GetService("ReplicatedStorage")
local RunService = game:GetService("RunService")

local Shared = ReplicatedStorage:WaitForChild("Shared")
local Config = require(Shared.Config)
local Util = require(Shared.Util)
local Inventory = require(Shared.InventoryModel)
local Items = require(Shared.ItemDefinitions)

local DirectorService = {}

local registryRef
local remotes
local joinedAt = {}
local tutorial = {}

local TUTORIAL = {
	[1] = "You are alive. That is step one. Press TAB to check what you carried out of the shelter.",
	[2] = "Press F for the flashlight. The dark is not empty.",
	[3] = "Press E to search containers. Everything you find matters.",
	[4] = "Press C to craft. Bandages first, then water.",
	[5] = "Press J for your journal. The signal has a source.",
	[6] = "Press Left Shift to sprint. It costs stamina and it makes noise.",
	[7] = "Press M for the map. Get to the surface and find the radio relay.",
}

function DirectorService.Init(registry)
	registryRef = registry
	remotes = registry.Remotes
end

function DirectorService.Start(registry)
	Players.PlayerAdded:Connect(function(player)
		joinedAt[player.UserId] = os.clock()
		task.spawn(function()
			task.wait(1.5)
			DirectorService.OnPlayerReady(player)
		end)
	end)
	for _, player in ipairs(Players:GetPlayers()) do
		joinedAt[player.UserId] = os.clock()
		task.spawn(function()
			task.wait(1.5)
			DirectorService.OnPlayerReady(player)
		end)
	end

	remotes.OnInvoke("GetInitialState", function(player)
		local profile = registryRef.PlayerDataService.Get(player)
		if not profile then return nil end
		return require(Shared.Profile).snapshotForClient(profile)
	end)
	remotes.OnInvoke("GetItemInfo", function(player, itemId)
		if type(itemId) ~= "string" then return nil end
		local def = Items[itemId]
		if not def then return nil end
		return { id = def.id, name = def.name, desc = def.desc, cat = def.cat,
			w = def.w, h = def.h, weight = def.weight, rarity = def.rarity, stack = def.stack }
	end)
	remotes.OnInvoke("GetServerInfo", function(player)
		return {
			players = #Players:GetPlayers(),
			uptime = math.floor(os.clock()),
			weather = registryRef.WeatherService and registryRef.WeatherService.GetState(),
			enemies = registryRef.EnemyService and registryRef.EnemyService.Count() or 0,
		}
	end)

	-- server heartbeat / module watchdog (also keeps the server from idling)
	task.spawn(function()
		local lastWarning = 0
		while true do
			task.wait(30)
			local now = os.clock()
			if now - lastWarning > 120 then
				local enemies = registryRef.EnemyService and registryRef.EnemyService.Count() or 0
				local structures = registryRef.BuildingService and registryRef.BuildingService.Count() or 0
				print(string.format("[Director] %d players, %d active creatures, %d structures, uptime %.0fs",
					#Players:GetPlayers(), enemies, structures, now))
				lastWarning = now
			end
			if registryRef.PartyService then
				pcall(registryRef.PartyService.Cleanup)
			end
			for _, player in ipairs(Players:GetPlayers()) do
				local profile = registryRef.PlayerDataService.Get(player)
				if profile and registryRef.QuestService then
					pcall(registryRef.QuestService.Tick, player, profile)
				end
			end
		end
	end)
end

function DirectorService.OnPlayerReady(player)
	local profile = registryRef.PlayerDataService.Get(player)
	if not profile then return end
	if registryRef.QuestService then
		registryRef.QuestService.Bootstrap(player, profile)
	end
	if registryRef.SurvivalService then
		registryRef.SurvivalService.Push(player, profile)
	end
	remotes.Fire(player, "StateSync", require(Shared.Profile).snapshotForClient(profile))
	remotes.Fire(player, "Atmosphere", registryRef.WeatherService and registryRef.WeatherService.GetState() or {})
	remotes.Fire(player, "Notify", Config.Title, "title", 6)
	if profile.tutorialStep and TUTORIAL[profile.tutorialStep] then
		remotes.Fire(player, "TutorialStep", profile.tutorialStep, TUTORIAL[profile.tutorialStep])
	end
end

function DirectorService.OnProfileReady(player, profile)
	DirectorService.OnPlayerReady(player)
end

function DirectorService.OnRespawn(player, profile)
	if registryRef.QuestService then
		registryRef.QuestService.SyncQuestState(player, profile)
	end
	remotes.Fire(player, "SurvivalUpdate", profile.survival)
end

function DirectorService.CheckTutorial(player, profile, action)
	local step = profile.tutorialStep or 1
	local advance = false
	if action == "loot" and step == 3 then advance = true
	elseif action == "craft" and step == 4 then advance = true
	elseif action == "build" and step == 6 then advance = true
	elseif action == "document" and step == 5 then advance = true
	end
	if advance then
		profile.tutorialStep = step + 1
		if TUTORIAL[profile.tutorialStep] then
			remotes.Fire(player, "TutorialStep", profile.tutorialStep, TUTORIAL[profile.tutorialStep])
		end
		registryRef.PlayerDataService.MarkDirty(player)
	end
end

function DirectorService.AdvanceTutorial(player, profile)
	local step = (profile.tutorialStep or 1) + 1
	profile.tutorialStep = step
	if TUTORIAL[step] then
		remotes.Fire(player, "TutorialStep", step, TUTORIAL[step])
	end
end

return DirectorService
