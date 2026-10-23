--!strict
--[[
    ASHES OF HUMANITY: LAST SIGNAL
    ServerScriptService.Services.QuestService
    Main Storyline (Acts I-VI), Side Quests, Lore Document Discovery,
    Cooperative Objective Sync, and Server-Authoritative Rewards (Sections 29, 30).
]]

local ReplicatedStorage = game:GetService("ReplicatedStorage")

local NetworkSchema = require(ReplicatedStorage.Shared.NetworkSchema)
local QuestDefinitions = require(ReplicatedStorage.Shared.QuestDefinitions)
local Types = require(ReplicatedStorage.Shared.Types)

local QuestService = {}

local registry: any = nil

function QuestService:Init(serviceRegistry: any)
	registry = serviceRegistry
end

function QuestService:Start() end

function QuestService:EnsureInitialQuests(player: Player)
	local profile = registry.PlayerDataService:GetProfile(player)
	if not profile then
		return
	end
	if next(profile.Quests) == nil then
		self:ActivateQuest(player, "Main_01_EchoAwakening", true)
		self:ActivateQuest(player, "Side_01_MedicalScavenge", true)
		self:ActivateQuest(player, "Side_02_FortifyShelter", true)
	end
	self:SyncQuests(player)
end

function QuestService:ActivateQuest(player: Player, questId: string, silent: boolean?): boolean
	local qDef = (QuestDefinitions.QUESTS :: any)[questId]
	if not qDef then
		return false
	end
	local profile = registry.PlayerDataService:GetProfile(player)
	if not profile then
		return false
	end
	if profile.Quests[questId] ~= nil then
		return false
	end

	local objMap: { [string]: number } = {}
	for _, obj in ipairs(qDef.Objectives) do
		objMap[obj.Id] = 0
	end

	profile.Quests[questId] = {
		QuestId = questId,
		Status = "Active",
		Objectives = objMap,
	}

	if not silent then
		registry.SendToPlayer(player, NetworkSchema.EVENTS.Notification, {
			Kind = "Quest",
			Text = "New Objective: " .. qDef.Title,
		})
		self:SyncQuests(player)
	end
	return true
end

function QuestService:DiscoverLore(player: Player, loreId: string): boolean
	local lore = (QuestDefinitions.LORE_ENTRIES :: any)[loreId]
	if not lore then
		return false
	end
	local profile = registry.PlayerDataService:GetProfile(player)
	if not profile then
		return false
	end

	local firstTime = not profile.DiscoveredLore[loreId]
	profile.DiscoveredLore[loreId] = true

	-- Dr. Vance's Hospital Triage Log includes her Site-09 Security Keycard clipped to the clipboard
	if loreId == "Lore_09_VanceTriageTape" and registry.InventoryService then
		if registry.InventoryService:CountItem(player, "Site09Keycard") == 0 then
			registry.InventoryService:AddItem(player, "Site09Keycard", 1, nil, nil)
		end
	end

	self:NotifyProgress(player, "ReadLore", loreId, 1)

	registry.SendToPlayer(player, NetworkSchema.EVENTS.RadioTransmission, {
		Frequency = "ARCHIVE // " .. lore.Region,
		Title = lore.Title,
		Text = lore.Text,
		IsLoreDocument = true,
	})

	if firstTime then
		self:SyncQuests(player)
	end
	return true
end

function QuestService:NotifyProgress(player: Player, actionType: string, targetId: string, amount: number)
	local recipients: { Player } = { player }
	-- Share quest objective progress with nearby cooperative party members!
	if registry.PartyService then
		local partyPeers = registry.PartyService:GetNearbyPartyMembers(player, 140)
		for _, peer in ipairs(partyPeers) do
			if peer ~= player then
				table.insert(recipients, peer)
			end
		end
	end

	for _, member in ipairs(recipients) do
		self:ApplyProgressSingle(member, actionType, targetId, amount)
	end
end

function QuestService:ApplyProgressSingle(player: Player, actionType: string, targetId: string, amount: number)
	local profile = registry.PlayerDataService:GetProfile(player)
	if not profile then
		return
	end

	local anyUpdated = false
	local completedIds: { string } = {}

	for questId, qProg in pairs(profile.Quests) do
		if qProg.Status == "Active" then
			local qDef: QuestDefinitions.QuestDef? = (QuestDefinitions.QUESTS :: any)[questId]
			if qDef then
				local allDone = true
				for _, obj in ipairs(qDef.Objectives) do
					local cur = qProg.Objectives[obj.Id] or 0
					if obj.Type == actionType and obj.Target == targetId and cur < obj.RequiredCount then
						cur = math.min(obj.RequiredCount, cur + amount)
						qProg.Objectives[obj.Id] = cur
						anyUpdated = true
					end
					if cur < obj.RequiredCount then
						allDone = false
					end
				end
				if allDone then
					table.insert(completedIds, questId)
				end
			end
		end
	end

	for _, doneId in ipairs(completedIds) do
		self:CompleteQuest(player, doneId)
		anyUpdated = true
	end

	if anyUpdated then
		self:SyncQuests(player)
	end
end

function QuestService:CompleteQuest(player: Player, questId: string)
	local profile = registry.PlayerDataService:GetProfile(player)
	if not profile then
		return
	end
	local qProg: Types.QuestProgress? = profile.Quests[questId]
	local qDef: QuestDefinitions.QuestDef? = (QuestDefinitions.QUESTS :: any)[questId]
	if not qProg or not qDef or qProg.Status == "Completed" then
		return
	end

	qProg.Status = "Completed"

	-- Grant server-authoritative rewards
	if qDef.Rewards then
		if qDef.Rewards.Items then
			for _, reward in ipairs(qDef.Rewards.Items) do
				local ok = registry.InventoryService:AddItem(player, reward.ItemId, reward.Count, nil, true)
				if not ok and registry.LootService then
					local root = registry.SecurityService:GetCharacterRoot(player)
					local dropPos = if root then root.Position + Vector3.new(0, 0.5, 2) else Vector3.zero
					registry.LootService:SpawnDroppedItem(reward.ItemId, reward.Count, 100, dropPos)
				end
			end
			registry.InventoryService:SyncInventory(player)
		end
		if qDef.Rewards.UnlockBlueprints and registry.CraftingService then
			for _, bpId in ipairs(qDef.Rewards.UnlockBlueprints) do
				registry.CraftingService:UnlockBlueprint(player, bpId)
			end
		end
	end

	registry.SendToPlayer(player, NetworkSchema.EVENTS.Notification, {
		Kind = "QuestComplete",
		Text = "Completed: " .. qDef.Title,
	})

	if qDef.NextQuestId then
		self:ActivateQuest(player, qDef.NextQuestId, false)
	end

	-- Unlock mid-game side quests progressively
	if questId == "Main_02_SuburbiaRelay" then
		self:ActivateQuest(player, "Side_03_SubstationGenerator", false)
		self:ActivateQuest(player, "Side_04_SilenceTheHollows", false)
	elseif questId == "Main_03_MercyTriage" then
		self:ActivateQuest(player, "Side_05_BlackwoodBunker", false)
	elseif questId == "Main_05_LazarusBunker" then
		self:ActivateQuest(player, "Side_06_ApexNocturne", false)
	elseif questId == "Main_06_OriginOfTheSignal" then
		registry.BroadcastAll(NetworkSchema.EVENTS.WorldEventAlert, {
			EventId = "LastSignalSevered",
			Title = "CARRIER WAVE TERMINATED",
			Description = player.Name .. " overrode Tower Zero. The 142.8 MHz LAZARUS loop has fallen silent.",
		})
	end
end

function QuestService:HandleRemoteAction(player: Player, payload: any)
	if type(payload) ~= "table" or not NetworkSchema.ValidateString(payload.Action, 24) then
		return
	end
	if payload.Action == "AcceptSide" and NetworkSchema.ValidateString(payload.QuestId, 48) then
		local qDef = (QuestDefinitions.QUESTS :: any)[payload.QuestId]
		if qDef and qDef.Kind == "Side" then
			self:ActivateQuest(player, payload.QuestId, false)
		end
	elseif payload.Action == "RequestSync" then
		self:SyncQuests(player)
	end
end

function QuestService:SyncQuests(player: Player)
	local profile = registry.PlayerDataService:GetProfile(player)
	if not profile then
		return
	end
	registry.SendToPlayer(player, NetworkSchema.EVENTS.QuestUpdate, {
		Quests = profile.Quests,
		DiscoveredLore = profile.DiscoveredLore,
		UnlockedBlueprints = profile.UnlockedBlueprints,
	})
end

return QuestService
