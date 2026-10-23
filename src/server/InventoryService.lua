--!strict
--[[
	InventoryService — server authority for items, equipment and item use.

	Every mutation is validated here; clients only send *requests*. The service
	also owns the "world drop" model (items dropped on the ground become
	interactable containers handled by InteractionService).
]]

local Players = game:GetService("Players")
local ReplicatedStorage = game:GetService("ReplicatedStorage")
local ServerStorage = game:GetService("ServerStorage")

local Shared = ReplicatedStorage:WaitForChild("Shared")
local Items = require(Shared.ItemDefinitions)
local Inventory = require(Shared.InventoryModel)
local Config = require(Shared.Config)
local SurvivalMath = require(Shared.SurvivalMath)
local CombatMath = require(Shared.CombatMath)

local InventoryService = {}
local registryRef
local remotes

local function profileOf(player)
	local data = registryRef and registryRef.PlayerDataService
	if not data then return nil end
	local profile = data.Get(player)
	if profile then
		data.MarkDirty(player)
	end
	return profile
end

local function pushState(player, profile)
	if not remotes then return end
	remotes.Fire(player, "InventoryUpdate", Inventory.serialize(profile.inventory))
	remotes.Fire(player, "EquipmentUpdate", Inventory.serialize(profile.inventory).equipment)
end

function InventoryService.Init(registry)
	registryRef = registry
	remotes = registry.Remotes
end

function InventoryService.Start(registry)
	remotes.On("RequestMoveItem", function(player, uid, fromContainer, toContainer, toX, toY)
		InventoryService.MoveItem(player, uid, fromContainer, toContainer, toX, toY)
	end)
	remotes.On("RequestEquip", function(player, slotOrUid)
		InventoryService.Equip(player, slotOrUid)
	end)
	remotes.On("RequestDropItem", function(player, uid, count)
		InventoryService.DropItem(player, uid, count)
	end)
	remotes.On("RequestUseItem", function(player, uid)
		InventoryService.UseItem(player, uid)
	end)
	remotes.On("RequestConsume", function(player, uid)
		InventoryService.UseItem(player, uid)
	end)
end

function InventoryService.MoveItem(player, uid, fromContainer, toContainer, toX, toY)
	local profile = profileOf(player)
	if not profile or type(uid) ~= "string" then return false end
	if type(toX) ~= "number" or type(toY) ~= "number" then return false end
	toX, toY = math.floor(toX), math.floor(toY)
	local source, sourceContainer = Inventory.findUid(profile.inventory, uid)
	if not source then return false end
	local target = profile.inventory.containers[toContainer]
	if not target then return false end
	if sourceContainer == target then
		local ok = Inventory.canPlace(target, source.itemId, toX, toY, uid)
		if not ok then return false end
		source.x, source.y = toX, toY
	else
		local ok = Inventory.canPlace(target, source.itemId, toX, toY)
		if not ok then return false end
		if sourceContainer then
			sourceContainer.items[uid] = nil
		else
			-- was equipped
			for slot, entry in pairs(profile.inventory.equipment) do
				if entry.uid == uid then
					profile.inventory.equipment[slot] = nil
					break
				end
			end
		end
		source.x, source.y = toX, toY
		target.items[uid] = source
	end
	local weight = Inventory.totalWeight(profile.inventory)
	if weight > Inventory.capacity(profile.inventory) + 0.01 then
		-- revert is complex; instead just report the overweight state
		remotes.Fire(player, "Notify", "You are over capacity. Drop something.", "warn", 3)
	end
	pushState(player, profile)
	return true
end

function InventoryService.Equip(player, slotOrUid)
	local profile = profileOf(player)
	if not profile or type(slotOrUid) ~= "string" then return false end
	local ok, result = Inventory.equipFromUid(profile.inventory, slotOrUid)
	if not ok then
		-- maybe they passed a slot name to unequip
		if Config.EquipSlots and profile.inventory.equipment[slotOrUid] then
			Inventory.unequip(profile.inventory, slotOrUid)
			ok = true
		end
	end
	if ok then
		pushState(player, profile)
		local entry = profile.inventory.equipment[type(result) == "string" and result or ""]
		if entry then
			local def = Items[entry.itemId]
			if def then
				remotes.Fire(player, "Notify", "Equipped " .. def.name, "info", 2)
			end
		end
		if registryRef.CombatService then
			registryRef.CombatService.RefreshLoadout(player, profile)
		end
	end
	return ok
end

function InventoryService.AddItem(player, itemId, count, opts)
	local profile = profileOf(player)
	if not profile then return nil end
	local def = Items[itemId]
	if not def then return nil end
	count = math.max(1, math.floor(count or 1))
	-- weight gate: refuse to go far beyond capacity
	local current = Inventory.totalWeight(profile.inventory)
	local capacity = Inventory.capacity(profile.inventory)
	if current + def.weight * count > capacity * 1.35 then
		remotes.Fire(player, "Notify", "Too heavy to carry that.", "warn", 3)
		return nil
	end
	local uid, err = Inventory.addItem(profile.inventory, itemId, count)
	if uid then
		profile.stats.itemsFound = (profile.stats.itemsFound or 0) + count
		pushState(player, profile)
		if opts and opts.silent ~= true then
			remotes.Fire(player, "Notify", string.format("+%d %s", count, def.name), "loot", 2.4)
		end
		if registryRef.QuestService then
			registryRef.QuestService.OnItemChanged(player, profile)
		end
		if registryRef.DirectorService then
			registryRef.DirectorService.CheckTutorial(player, profile, "loot")
		end
	end
	return uid, err
end

function InventoryService.Count(player, itemId)
	local profile = profileOf(player)
	if not profile then return 0 end
	return Inventory.countItem(profile.inventory, itemId, true)
end

function InventoryService.Consume(player, itemId, count)
	local profile = profileOf(player)
	if not profile then return false end
	local ok = Inventory.consumeItem(profile.inventory, itemId, count or 1)
	if ok then
		pushState(player, profile)
	end
	return ok
end

function InventoryService.DropItem(player, uid, count)
	local profile = profileOf(player)
	if not profile or type(uid) ~= "string" then return false end
	local entry = Inventory.findUid(profile.inventory, uid)
	if not entry then return false end
	local character = player.Character
	local root = character and character:FindFirstChild("HumanoidRootPart")
	if not root then return false end
	local dropCount = math.min(count or entry.count or 1, entry.count or 1)
	if registryRef.LootService then
		registryRef.LootService.CreateWorldDrop(root.Position + Vector3.new(0, -1, 0), entry.itemId, dropCount)
	end
	Inventory.removeByUid(profile.inventory, uid, dropCount)
	pushState(player, profile)
	if registryRef.QuestService then
		registryRef.QuestService.OnItemChanged(player, profile)
	end
	return true
end

function InventoryService.UseItem(player, uid)
	local profile = profileOf(player)
	if not profile or type(uid) ~= "string" then return false end
	local entry = Inventory.findUid(profile.inventory, uid)
	if not entry then return false end
	local def = Items[entry.itemId]
	if not def then return false end
	if not def.use then
		-- equipment and weapons get equipped instead
		if def.equip then
			return InventoryService.Equip(player, uid)
		end
		remotes.Fire(player, "Notify", "Nothing happens.", "info", 2)
		return false
	end

	local survival = profile.survival
	local effects = profile._effects or {}
	profile._effects = effects

	if def.use.kind == "medical" then
		if survival.health >= Config.Survival.MaxHealth and (not def.use.bleeding or survival.bleeding < 1)
			and (not def.use.infection or survival.infection <= 0) then
			remotes.Fire(player, "Notify", "You don't need that right now.", "info", 2)
			return false
		end
	elseif def.use.kind == "food" and survival.hunger >= 99 then
		remotes.Fire(player, "Notify", "You are not hungry.", "info", 2)
		return false
	elseif def.use.kind == "drink" and survival.thirst >= 99 then
		remotes.Fire(player, "Notify", "You are not thirsty.", "info", 2)
		return false
	elseif def.use.kind == "blueprint" then
		profile.blueprints = profile.blueprints or {}
		profile.blueprints[def.use.recipe] = true
		Inventory.removeByUid(profile.inventory, uid, 1)
		pushState(player, profile)
		remotes.Fire(player, "Notify", "Blueprint studied. Recipe unlocked.", "quest", 4)
		if registryRef.QuestService then
			registryRef.QuestService.SyncQuestState(player, profile)
		end
		return true
	elseif def.use.kind == "craft_tool" then
		-- water filter: convert a dirty bottle in the inventory
		local convert = def.use.convert
		if convert and Inventory.countItem(profile.inventory, convert.from) > 0 then
			Inventory.consumeItem(profile.inventory, convert.from, 1)
			Inventory.addItem(profile.inventory, convert.to, 1)
			entry.dur = (entry.dur or def.dur or 40) - 1
			if entry.dur <= 0 then
				Inventory.removeByUid(profile.inventory, uid, 1)
			end
			pushState(player, profile)
			remotes.Fire(player, "Notify", "Water filtered.", "loot", 2.4)
			return true
		end
		remotes.Fire(player, "Notify", "No tainted water to filter.", "warn", 2.4)
		return false
	elseif def.use.kind == "radio" then
		if registryRef.StoryService then
			registryRef.StoryService.PlayRadio(player, profile, "auto")
		end
		return true
	elseif def.use.kind == "map" then
		remotes.Fire(player, "StoryEvent", "map_reveal", {})
		return true
	elseif def.use.kind == "light" or def.use.kind == "flare" then
		if registryRef.WorldEventService then
			registryRef.WorldEventService.DropLight(player, def)
		end
		Inventory.removeByUid(profile.inventory, uid, 1)
		pushState(player, profile)
		return true
	end

	-- consume one
	local stack = entry.count or 1
	if stack > 1 then
		entry.count = stack - 1
	else
		Inventory.removeByUid(profile.inventory, uid, 1)
	end
	SurvivalMath.applyItemEffect(survival, def, effects)
	if def.use.kind == "drink" and def.use.container then
		Inventory.addItem(profile.inventory, "water_bottle", 1)
	end
	pushState(player, profile)
	if registryRef.SurvivalService then
		registryRef.SurvivalService.Push(player, profile)
	end
	remotes.Fire(player, "CombatFeedback", "consume", { item = def.id })
	remotes.Fire(player, "Notify", string.format("Used %s", def.name), "info", 2)
	if registryRef.QuestService then
		registryRef.QuestService.OnItemChanged(player, profile)
	end
	return true
end

function InventoryService.Serialize(profile)
	return Inventory.serialize(profile.inventory)
end

function InventoryService.Armor(profile)
	return CombatMath.aggregateArmor(profile.inventory.equipment, Items)
end

return InventoryService
