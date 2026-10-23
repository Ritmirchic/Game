--!strict
--[[
	StoryService — documents, radio transmissions and environmental horror beats.

	The horror director lives here: it schedules subtle phenomena (screams,
	footsteps, whispers) based on time of day, stress and region.
]]

local Players = game:GetService("Players")
local ReplicatedStorage = game:GetService("ReplicatedStorage")

local Shared = ReplicatedStorage:WaitForChild("Shared")
local Story = require(Shared.StoryData)
local Config = require(Shared.Config)
local Util = require(Shared.Util)

local StoryService = {}

local registryRef
local remotes
local rng = Random.new(os.time())
local lastPhenomenon = 0
local radioIndex = {}

function StoryService.Init(registry)
	registryRef = registry
	remotes = registry.Remotes
end

function StoryService.Start(registry)
	remotes.On("RequestDocument", function(player, docId)
		StoryService.ShowDocument(player, docId)
	end)
	task.spawn(function()
		while true do
			task.wait(14)
			StoryService.HorrorTick()
		end
	end)
end

function StoryService.ShowDocument(player, docId)
	local doc = Story.Documents[docId]
	if not doc then
		remotes.Fire(player, "Notify", "The page has crumbled to nothing.", "info", 2.4)
		return false
	end
	local profile = registryRef.PlayerDataService.Get(player)
	if profile then
		profile.documentsRead = profile.documentsRead or {}
		if not profile.documentsRead[docId] then
			profile.documentsRead[docId] = true
			profile.stats.documents = (profile.stats.documents or 0) + 1
			profile.survival.stress = math.min(100, profile.survival.stress + 2)
			registryRef.PlayerDataService.MarkDirty(player)
		end
	end
	remotes.Fire(player, "DocumentShow", {
		id = docId, title = doc.title, location = doc.location, body = doc.body,
	})
	return true
end

function StoryService.PlayRadio(player, profile, mode)
	if not profile then return end
	local state = profile.radioState or {}
	profile.radioState = state
	local candidates = {}
	for _, transmission in ipairs(Story.Radio) do
		if not state[transmission.id] then
			table.insert(candidates, transmission)
		end
	end
	local pick
	if #candidates > 0 then
		pick = candidates[1]
	else
		pick = Story.Radio[rng:NextInteger(1, #Story.Radio)]
	end
	state[pick.id] = true
	remotes.Fire(player, "StoryEvent", "radio_play", {
		title = pick.title, lines = pick.lines, note = pick.note,
	})
	if registryRef.QuestService then
		registryRef.QuestService.Event(player, { type = "interact", kind = "Radio" })
	end
	registryRef.PlayerDataService.MarkDirty(player)
end

function StoryService.HorrorTick()
	for _, player in ipairs(Players:GetPlayers()) do
		local profile = registryRef.PlayerDataService.Get(player)
		local character = player.Character
		local root = character and character:FindFirstChild("HumanoidRootPart")
		if profile and root and profile.survival.alive then
			local night = registryRef.WeatherService and registryRef.WeatherService.IsNight() or false
			local indoors = registryRef.SurvivalService and registryRef.SurvivalService.IsIndoors(root.Position) or false
			local threat = registryRef.EnemyService and registryRef.EnemyService.GetThreat(player) or 0
			local stress = profile.survival.stress
			local chance = 0.16 + (night and 0.24 or 0) + stress / 400
			if indoors then chance += 0.06 end
			if threat > 0.5 then chance *= 0.4 end
			if rng:NextNumber() < chance then
				local phenomenon = Story.Phenomena[rng:NextInteger(1, #Story.Phenomena)]
				remotes.Fire(player, "StoryEvent", "phenomenon", {
					text = phenomenon.text, id = phenomenon.id,
				})
				profile.survival.stress = math.min(100, stress + phenomenon.stress)
			end
			registryRef.SurvivalService.Push(player, profile)
		end
	end
end

function StoryService.Count()
	local n = 0
	for _ in pairs(Story.Documents) do n += 1 end
	return n
end

return StoryService
