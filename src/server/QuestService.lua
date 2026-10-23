--!strict
--[[
	QuestService — story + side quest progression.

	Owns: objective events, completion, rewards, quest state sync, and the
	"main line" bootstrap that starts chapter 1 for new survivors.
]]

local Players = game:GetService("Players")
local ReplicatedStorage = game:GetService("ReplicatedStorage")

local Shared = ReplicatedStorage:WaitForChild("Shared")
local Quests = require(Shared.QuestDefinitions)
local QuestLogic = require(Shared.QuestLogic)
local Inventory = require(Shared.InventoryModel)
local Items = require(Shared.ItemDefinitions)
local Config = require(Shared.Config)

local QuestService = {}

local registryRef
local remotes

function QuestService.Init(registry)
	registryRef = registry
	remotes = registry.Remotes
end

function QuestService.Start(registry)
	remotes.On("RequestQuestAction", function(player, questId, action)
		QuestService.HandleAction(player, questId, action)
	end)
end

function QuestService.HandleAction(player, questId, action)
	local profile = registryRef.PlayerDataService.Get(player)
	if not profile or type(questId) ~= "string" then return end
	local quest = Quests[questId]
	if not quest then return end
	local state = profile.quests
	if action == "abandon" then
		if state.active[questId] then
			state.active[questId] = nil
			QuestService.SyncQuestState(player, profile)
		end
		return
	end
	-- "track" simply re-sends the state; tracking is client-side
	QuestService.SyncQuestState(player, profile)
end

function QuestService.Bootstrap(player, profile)
	local main = Quests.mainLine()
	local state = profile.quests
	-- start the first chapter
	local first = main[1]
	if first and not QuestLogic.isComplete(state, first.id) then
		QuestLogic.forceStart(state, first, os.time())
		state.unlocked[first.id] = true
		remotes.Fire(player, "StoryEvent", "chapter_start", {
			questId = first.id, name = first.name, brief = first.brief, intro = first.intro,
		})
	end
	-- refresh collect objectives from the inventory
	QuestService.RefreshCollectObjectives(player, profile)
	QuestService.SyncQuestState(player, profile)
end

function QuestService.RefreshCollectObjectives(player, profile)
	local state = profile.quests
	for questId in pairs(state.active) do
		local quest = Quests[questId]
		if quest then
			QuestLogic.update(state, quest, nil, {
				countItem = function(itemId)
					return Inventory.countItem(profile.inventory, itemId, true)
				end,
			})
		end
	end
end

function QuestService.OnItemChanged(player, profile)
	QuestService.RefreshCollectObjectives(player, profile)
	QuestService.CheckCompletion(player, profile)
	QuestService.SyncQuestState(player, profile)
end

function QuestService.Event(player, event)
	local profile = registryRef.PlayerDataService.Get(player)
	if not profile or type(event) ~= "table" then return end
	local state = profile.quests
	for questId in pairs(state.active) do
		local quest = Quests[questId]
		if quest then
			QuestLogic.update(state, quest, event, {
				countItem = function(itemId)
					return Inventory.countItem(profile.inventory, itemId, true)
				end,
			})
		end
	end
	QuestService.CheckCompletion(player, profile)
	QuestService.SyncQuestState(player, profile)
end

function QuestService.CheckCompletion(player, profile)
	local state = profile.quests
	local completed = {}
	for questId, active in pairs(state.active) do
		if active.complete then
			table.insert(completed, questId)
		end
	end
	for _, questId in ipairs(completed) do
		local quest = Quests[questId]
		QuestService.Complete(player, profile, quest)
	end
end

function QuestService.Complete(player, profile, quest)
	if not quest then return end
	local state = profile.quests
	QuestLogic.complete(state, quest, os.time())
	-- rewards
	local rewards = quest.rewards or {}
	if rewards.xp then
		profile.xp = (profile.xp or 0) + rewards.xp
	end
	for _, item in ipairs(rewards.items or {}) do
		if Items[item.id] then
			Inventory.addItem(profile.inventory, item.id, item.count or 1)
		end
	end
	for _, unlock in ipairs(rewards.unlock or {}) do
		state.unlocked[unlock] = true
		if Quests[unlock] and not QuestLogic.isComplete(state, unlock) then
			QuestLogic.forceStart(state, Quests[unlock], os.time())
		end
	end
	if registryRef.PlayerDataService then
		registryRef.PlayerDataService.MarkDirty(player)
	end
	remotes.Fire(player, "StoryEvent", "quest_complete", {
		questId = quest.id, name = quest.name, text = quest.completion, rewards = rewards,
	})
	remotes.Fire(player, "Notify", "Quest complete: " .. quest.name, "quest", 5)
	remotes.Fire(player, "InventoryUpdate", Inventory.serialize(profile.inventory))
	QuestService.SyncQuestState(player, profile)
end

function QuestService.SyncQuestState(player, profile)
	local state = profile.quests
	local payload = { active = {}, completed = {}, unlocked = {} }
	for questId, active in pairs(state.active) do
		local quest = Quests[questId]
		if quest then
			local objectives = {}
			for _, obj in ipairs(quest.objectives) do
				local entry = active.objectives[obj.id] or { progress = 0, done = false }
				table.insert(objectives, {
					id = obj.id, text = obj.text, type = obj.type,
					count = obj.count, progress = entry.progress or 0, done = entry.done == true,
				})
			end
			payload.active[questId] = {
				id = questId, name = quest.name, brief = quest.brief, side = quest.side,
				chapter = quest.chapter, objectives = objectives, complete = active.complete == true,
			}
		end
	end
	for questId in pairs(state.completed) do
		payload.completed[questId] = true
	end
	for questId, unlocked in pairs(state.unlocked) do
		payload.unlocked[questId] = unlocked
	end
	remotes.Fire(player, "QuestUpdate", payload)
end

function QuestService.TryUnlockSideQuests(player, profile)
	-- side quests unlock through story progress + simple conditions
	local state = profile.quests
	local side = Quests.sideQuests()
	for _, quest in ipairs(side) do
		if not state.unlocked[quest.id] and not state.active[quest.id] and not state.completed[quest.id] then
			local shouldUnlock = false
			if quest.id == "quest_suburb_house" and state.completed["quest_awaken"] then
				shouldUnlock = true
			elseif quest.id == "quest_hunter" and state.completed["quest_awaken"] then
				shouldUnlock = true
			elseif quest.id == "quest_water" and state.completed["quest_radio"] then
				shouldUnlock = true
			elseif quest.id == "quest_generator" and state.completed["quest_industrial"] then
				shouldUnlock = true
			elseif quest.id == "quest_shelter_defense" and state.completed["quest_pharmacy"] then
				shouldUnlock = true
			elseif quest.id == "quest_pale" and state.completed["quest_lab"] then
				shouldUnlock = true
			end
			if shouldUnlock then
				state.unlocked[quest.id] = true
				QuestLogic.forceStart(state, quest, os.time())
				remotes.Fire(player, "Notify", "New contract: " .. quest.name, "quest", 4)
			end
		end
	end
end

function QuestService.Tick(player, profile)
	QuestService.TryUnlockSideQuests(player, profile)
	QuestService.RefreshCollectObjectives(player, profile)
	QuestService.CheckCompletion(player, profile)
end

return QuestService
