--!strict
--[[
	InteractionService — resolves what the player is looking at and routes the
	interaction to the right service (loot, doors, campfires, workbenches,
	documents, radio, beds, quest items, world drops).

	Clients only send the instance they *think* they are interacting with; the
	server re-validates distance and type.
]]

local Players = game:GetService("Players")
local ReplicatedStorage = game:GetService("ReplicatedStorage")
local Workspace = game:GetService("Workspace")

local Shared = ReplicatedStorage:WaitForChild("Shared")
local Config = require(Shared.Config)
local Items = require(Shared.ItemDefinitions)

local InteractionService = {}

local registryRef
local remotes
local doorState = {}   -- [instance] = { open = bool, locked = bool }
local stationState = {} -- [instance] = { kind = "Campfire"|"Workbench", burning = bool }

function InteractionService.Init(registry)
	registryRef = registry
	remotes = registry.Remotes
end

local function kindOf(instance)
	local kind = instance:FindFirstChild("Kind")
	if kind and type(kind.Value) == "string" then
		return kind.Value
	end
	-- containers/scenery that are tagged by name
	if instance.Name == "DroppedItem" then
		return "WorldDrop"
	end
	if instance.Name == "LootAnchor" then
		return "Loot"
	end
	return nil
end

local function distanceCheck(player, instance)
	local character = player.Character
	local root = character and character:FindFirstChild("HumanoidRootPart")
	if not root then return false end
	local anchorPos = instance.Position
	local d = (root.Position - anchorPos).Magnitude
	return d <= Config.Interaction.Range + 4, d
end

function InteractionService.Start(registry)
	remotes.On("RequestInteract", function(player, instance)
		InteractionService.Interact(player, instance)
	end)
	remotes.On("RequestLoot", function(player, instance, index)
		if type(instance) ~= "instance" then return end
		local ok = distanceCheck(player, instance)
		if not ok then return end
		local kind = kindOf(instance)
		if kind == "Loot" then
			if index == "all" then
				registryRef.LootService.TakeAll(player, instance)
			elseif index == nil then
				registryRef.LootService.Open(player, instance)
			else
				registryRef.LootService.Take(player, instance, index)
			end
		elseif kind == "WorldDrop" then
			registryRef.LootService.PickupWorldDrop(player, instance)
		end
	end)
	-- NOTE: RequestDocument is owned by StoryService (documents are its domain).
	remotes.On("RequestRadio", function(player, instance)
		if registryRef.StoryService then
			registryRef.StoryService.PlayRadio(player, registryRef.PlayerDataService.Get(player), "manual")
		end
	end)
	remotes.On("RequestSleep", function(player, instance)
		InteractionService.Sleep(player)
	end)
	remotes.On("RequestSafehouse", function(player, instance)
		InteractionService.SetSafehouse(player, instance)
	end)
end

function InteractionService.Interact(player, instance)
	if type(instance) ~= "instance" then return false end
	local ok, distance = distanceCheck(player, instance)
	if not ok then
		return false
	end
	local kind = kindOf(instance)
	if not kind then
		return false
	end

	if kind == "Loot" then
		return registryRef.LootService.Open(player, instance)
	elseif kind == "WorldDrop" then
		return registryRef.LootService.PickupWorldDrop(player, instance)
	elseif kind == "Door" then
		return InteractionService.ToggleDoor(player, instance)
	elseif kind == "Campfire" then
		return InteractionService.ToggleCampfire(player, instance)
	elseif kind == "Workbench" then
		if registryRef.CraftingService then
			registryRef.CraftingService.OpenStation(player, instance, "Workbench")
		end
		return true
	elseif kind == "Radio" then
		if registryRef.StoryService then
			registryRef.StoryService.PlayRadio(player, registryRef.PlayerDataService.Get(player), "manual")
		end
		if registryRef.QuestService then
			registryRef.QuestService.Event(player, { type = "interact", kind = "Radio" })
		end
		return true
	elseif kind == "RadioTower" then
		if registryRef.QuestService then
			registryRef.QuestService.Event(player, { type = "interact", kind = "RadioTower" })
		end
		remotes.Fire(player, "Notify", "The relay is dead. It needs a working transmitter.", "info", 4)
		return true
	elseif kind == "Bed" then
		return InteractionService.Sleep(player)
	elseif kind == "Document" then
		local docId = instance:FindFirstChild("DocId")
		if docId and registryRef.StoryService then
			registryRef.StoryService.ShowDocument(player, docId.Value)
			if registryRef.QuestService then
				registryRef.QuestService.Event(player, { type = "interact", kind = "Document", target = docId.Value })
			end
			return true
		end
	elseif kind == "QuestItem" then
		local itemId = instance:FindFirstChild("ItemId")
		if itemId then
			local uid = registryRef.InventoryService.AddItem(player, itemId.Value, 1)
			if uid then
				instance:Destroy()
				remotes.Fire(player, "Notify", "Recovered " .. tostring(itemId.Value), "quest", 4)
				return true
			end
		end
	elseif kind == "LabDoor" then
		local profile = registryRef.PlayerDataService.Get(player)
		if profile then
			if profile.inventory and require(Shared.InventoryModel).countItem(profile.inventory, "lab_keycard") > 0 then
				remotes.Fire(player, "StoryEvent", "lab_door_open", {})
				remotes.Fire(player, "Notify", "The blast door grinds open.", "quest", 4)
				if registryRef.QuestService then
					registryRef.QuestService.Event(player, { type = "interact", kind = "LabDoor" })
				end
				return true
			end
			remotes.Fire(player, "Notify", "A level 4 keycard is required.", "warn", 3.4)
		end
		return false
	elseif kind == "PlayerSpawn" then
		return false
	elseif kind == "EnemySpawn" then
		return false
	elseif kind == "Safehouse" then
		return InteractionService.SetSafehouse(player, instance)
	end

	remotes.Fire(player, "Notify", "Nothing useful here.", "info", 2)
	return false
end

function InteractionService.ToggleDoor(player, instance)
	local state = doorState[instance] or { open = false }
	state.open = not state.open
	doorState[instance] = state
	remotes.FireNear(instance.Position, 60, "StoryEvent", "door", { open = state.open, position = instance.Position })
	return true
end

function InteractionService.ToggleCampfire(player, instance)
	local state = stationState[instance] or { burning = false }
	state.burning = not state.burning
	stationState[instance] = state
	if state.burning and registryRef.WorldEventService then
		registryRef.WorldEventService.RegisterFire(instance.Position)
	end
	remotes.Fire(player, "Notify", state.burning and "The fire catches." or "You put the fire out.", "info", 2)
	return true
end

function InteractionService.Sleep(player)
	local profile = registryRef.PlayerDataService.Get(player)
	if not profile then return false end
	local survival = profile.survival
	-- sleeping only works when relatively safe
	local threat = registryRef.EnemyService and registryRef.EnemyService.GetThreat(player) or 0
	if threat > 0.25 then
		remotes.Fire(player, "Notify", "Not with something nearby. Not now.", "warn", 3)
		return false
	end
	survival.fatigue = math.max(0, survival.fatigue - 45)
	survival.stress = math.max(0, survival.stress - 18)
	survival.health = math.min(Config.Survival.MaxHealth, survival.health + 8)
	survival.stamina = 100
	if registryRef.SurvivalService then
		registryRef.SurvivalService.Push(player, profile)
	end
	remotes.Fire(player, "Notify", "You sleep badly, but you sleep.", "info", 4)
	if registryRef.PlayerDataService then
		registryRef.PlayerDataService.Save(player, false)
	end
	return true
end

function InteractionService.SetSafehouse(player, instance)
	local profile = registryRef.PlayerDataService.Get(player)
	if not profile then return false end
	profile.safehouse = { x = instance.Position.X, y = instance.Position.Y, z = instance.Position.Z }
	registryRef.PlayerDataService.MarkDirty(player)
	remotes.Fire(player, "Notify", "Safehouse set.", "info", 2.4)
	return true
end

function InteractionService.DoorStates()
	return doorState
end

return InteractionService
