--!strict
--[[
    ASHES OF HUMANITY: LAST SIGNAL
    ServerScriptService.Services.LootService
    Location-Based Dynamic Loot Containers, Anti-Dupe Extraction,
    Timed Respawning, and World Loose Item Drops (Section 18).
]]

local ReplicatedStorage = game:GetService("ReplicatedStorage")
local Workspace = game:GetService("Workspace")

local Config = require(ReplicatedStorage.Shared.Config)
local ItemDefinitions = require(ReplicatedStorage.Shared.ItemDefinitions)
local NetworkSchema = require(ReplicatedStorage.Shared.NetworkSchema)
local UtilityModules = require(ReplicatedStorage.Shared.UtilityModules)

local LootService = {}

export type ContainerEntry = {
	SlotId: string,
	ItemId: string,
	Count: number,
}

export type ContainerRecord = {
	Id: string,
	TableName: string,
	Instance: Instance,
	Position: Vector3,
	LastRolledAt: number,
	DepletedAt: number?,
	Items: { ContainerEntry },
	Locked: boolean,
}

local registry: any = nil
local containers: { [string]: ContainerRecord } = {}
local worldDrops: { [string]: { Id: string, Part: BasePart, ItemId: string, Count: number, Durability: number, Claimed: boolean } } =
	{}
local rng = Random.new()

local function getPartPosition(inst: Instance): Vector3
	if inst:IsA("BasePart") then
		return inst.Position
	elseif inst:IsA("Model") then
		return inst:GetPivot().Position
	end
	local bp = inst:FindFirstChildWhichIsA("BasePart", true)
	return if bp then bp.Position else Vector3.zero
end

local function rollContainerItems(tableName: string): { ContainerEntry }
	local tbl = (ItemDefinitions.LOOT_TABLES :: any)[tableName] or ItemDefinitions.LOOT_TABLES.Residential
	local numRolls = rng:NextInteger(tbl.MinRolls, tbl.MaxRolls)
	local items: { ContainerEntry } = {}
	for _ = 1, numRolls do
		local picked = UtilityModules.PickWeighted(tbl.Entries, rng)
		if picked then
			local qty = rng:NextInteger(picked.Min, picked.Max)
			table.insert(items, {
				SlotId = UtilityModules.GenerateUID("lslot"),
				ItemId = picked.ItemId,
				Count = qty,
			})
		end
	end
	return items
end

function LootService:Init(serviceRegistry: any)
	registry = serviceRegistry
end

function LootService:RegisterContainer(inst: Instance)
	local cid = inst:GetAttribute("ContainerId")
	if type(cid) ~= "string" or cid == "" then
		cid = UtilityModules.GenerateUID("cnt")
		inst:SetAttribute("ContainerId", cid)
	end
	local tableName = inst:GetAttribute("LootTable")
	if type(tableName) ~= "string" or tableName == "" then
		tableName = "Residential"
	end
	containers[cid] = {
		Id = cid,
		TableName = tableName,
		Instance = inst,
		Position = getPartPosition(inst),
		LastRolledAt = os.clock(),
		DepletedAt = nil,
		Items = rollContainerItems(tableName),
		Locked = false,
	}
end

function LootService:ScanWorldContainers()
	local interactionRoot = Workspace:FindFirstChild("InteractionObjects")
	local worldRoot = Workspace:FindFirstChild("World")
	for _, folder in ipairs({ interactionRoot, worldRoot }) do
		if folder then
			for _, desc in ipairs(folder:GetDescendants()) do
				if desc:GetAttribute("IsLootContainer") == true then
					self:RegisterContainer(desc)
				end
			end
		end
	end
end

function LootService:OpenContainer(player: Player, containerId: string): (boolean, string?)
	local rec = containers[containerId]
	if not rec then
		return false, "UnknownContainer"
	end
	if not registry.SecurityService:IsWithinDistance(player, rec.Position, Config.INVENTORY.MaxPickupDistance) then
		return false, "TooFar"
	end

	-- Check timed respawn if depleted
	local tbl = (ItemDefinitions.LOOT_TABLES :: any)[rec.TableName] or ItemDefinitions.LOOT_TABLES.Residential
	if rec.DepletedAt and (os.clock() - rec.DepletedAt) >= tbl.RespawnSeconds then
		rec.Items = rollContainerItems(rec.TableName)
		rec.DepletedAt = nil
		rec.LastRolledAt = os.clock()
	end

	registry.SendToPlayer(player, NetworkSchema.EVENTS.ContainerOpened, {
		ContainerId = rec.Id,
		TableName = rec.TableName,
		Items = rec.Items,
	})
	return true, nil
end

function LootService:TakeContainerItem(player: Player, containerId: string, slotId: string): (boolean, string?)
	local rec = containers[containerId]
	if not rec or rec.Locked then
		return false, "Unavailable"
	end
	if not registry.SecurityService:IsWithinDistance(player, rec.Position, Config.INVENTORY.MaxPickupDistance) then
		return false, "TooFar"
	end

	rec.Locked = true
	local foundIndex: number? = nil
	local entry: ContainerEntry? = nil
	for i, item in ipairs(rec.Items) do
		if item.SlotId == slotId then
			foundIndex = i
			entry = item
			break
		end
	end

	if not foundIndex or not entry then
		rec.Locked = false
		return false, "AlreadyTaken"
	end

	-- Remove from container BEFORE adding to player inventory (prevents race/dupe)
	table.remove(rec.Items, foundIndex)
	if #rec.Items == 0 then
		rec.DepletedAt = os.clock()
	end

	local ok = registry.InventoryService:AddItem(player, entry.ItemId, entry.Count)
	if not ok then
		-- Rollback if player's spatial grid is completely full
		table.insert(rec.Items, entry)
		rec.DepletedAt = nil
		rec.Locked = false
		registry.SendToPlayer(player, NetworkSchema.EVENTS.Notification, {
			Kind = "Warning",
			Text = "Backpack grid is full — free up space first.",
		})
		return false, "InventoryFull"
	end

	rec.Locked = false
	registry.SendToPlayer(player, NetworkSchema.EVENTS.ContainerOpened, {
		ContainerId = rec.Id,
		TableName = rec.TableName,
		Items = rec.Items,
	})
	return true, nil
end

function LootService:SpawnDroppedItem(itemId: string, count: number, durability: number, pos: Vector3): string?
	local def = ItemDefinitions.Get(itemId)
	if not def then
		return nil
	end
	local parentFolder = Workspace:FindFirstChild("InteractionObjects")
	if not parentFolder then
		local f = Instance.new("Folder")
		f.Name = "InteractionObjects"
		f.Parent = Workspace
		parentFolder = f
	end

	local dropId = UtilityModules.GenerateUID("drop")
	local part = Instance.new("Part")
	part.Name = "WorldDrop_" .. itemId
	part.Size = Vector3.new(1.1, 0.7, 1.1)
	part.Position = pos
	part.Anchored = true
	part.CanCollide = false
	part.Material = Enum.Material.Metal
	part.Color = Color3.fromRGB(116, 124, 112)
	part:SetAttribute("DropId", dropId)
	part:SetAttribute("ItemId", itemId)
	part:SetAttribute("Count", count)

	local prompt = Instance.new("ProximityPrompt")
	prompt.ActionText = "Pick Up"
	prompt.ObjectText = string.format("%s (x%d)", def.Name, count)
	prompt.MaxActivationDistance = 10
	prompt.HoldDuration = 0
	prompt.Parent = part

	part.Parent = parentFolder

	worldDrops[dropId] = {
		Id = dropId,
		Part = part,
		ItemId = itemId,
		Count = count,
		Durability = durability,
		Claimed = false,
	}

	prompt.Triggered:Connect(function(triggerPlayer: Player)
		self:PickupWorldItem(triggerPlayer, dropId)
	end)

	return dropId
end

function LootService:PickupWorldItem(player: Player, dropId: string): (boolean, string?)
	local rec = worldDrops[dropId]
	if not rec or rec.Claimed then
		return false, "AlreadyClaimed"
	end
	if not registry.SecurityService:IsWithinDistance(player, rec.Part.Position, Config.INVENTORY.MaxPickupDistance) then
		return false, "TooFar"
	end

	rec.Claimed = true
	local ok = registry.InventoryService:AddItem(player, rec.ItemId, rec.Count, rec.Durability)
	if not ok then
		rec.Claimed = false
		return false, "InventoryFull"
	end

	worldDrops[dropId] = nil
	rec.Part:Destroy()
	return true, nil
end

function LootService:HandleRemoteAction(player: Player, payload: any)
	if type(payload) ~= "table" or not NetworkSchema.ValidateString(payload.Action, 24) then
		return
	end
	registry.SecurityService:WithTransactionLock(player, function()
		if payload.Action == "Open" and NetworkSchema.ValidateString(payload.ContainerId, 64) then
			return self:OpenContainer(player, payload.ContainerId)
		elseif
			payload.Action == "Take"
			and NetworkSchema.ValidateString(payload.ContainerId, 64)
			and NetworkSchema.ValidateString(payload.SlotId, 64)
		then
			return self:TakeContainerItem(player, payload.ContainerId, payload.SlotId)
		elseif payload.Action == "PickupDrop" and NetworkSchema.ValidateString(payload.DropId, 64) then
			return self:PickupWorldItem(player, payload.DropId)
		end
		return false, "InvalidLootAction"
	end)
end

function LootService:GetContainerCount(): number
	local n = 0
	for _ in pairs(containers) do
		n += 1
	end
	return n
end

function LootService:Start()
	self:ScanWorldContainers()
end

return LootService
