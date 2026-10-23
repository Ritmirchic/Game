--!strict
--[[
	LootService — world containers, interaction anchors, respawn and world drops.

	Containers live on the server in a registry keyed by the anchor instance.
	Each container rolls its table once, keeps the contents until emptied, then
	refills after Config.Interaction.LootRespawn seconds.
]]

local Players = game:GetService("Players")
local ReplicatedStorage = game:GetService("ReplicatedStorage")
local Workspace = game:GetService("Workspace")

local Shared = ReplicatedStorage:WaitForChild("Shared")
local Config = require(Shared.Config)
local Items = require(Shared.ItemDefinitions)
local LootRoller = require(Shared.LootRoller)
local LootTables = require(Shared.LootTables)
local Util = require(Shared.Util)

local LootService = {}

local registryRef
local remotes
local containers = {}
local worldDrops = {}
local rng = Random.new(math.floor(os.time()))
local anchorCount = 0

function LootService.Init(registry)
	registryRef = registry
	remotes = registry.Remotes
end

function LootService.Start(registry)
	task.spawn(function()
		local world = Workspace:WaitForChild("World")
		local anchors = {}
		for _, descendant in ipairs(world:GetDescendants()) do
			if descendant.Name == "LootAnchor" then
				table.insert(anchors, descendant)
			end
		end
		anchorCount = #anchors
		for index, anchor in ipairs(anchors) do
			local tableName = "residential"
			local tag = anchor:FindFirstChild("LootTable")
			if tag and type(tag.Value) == "string" then
				tableName = tag.Value
			end
			if LootTables[tableName] then
				containers[anchor] = {
					anchor = anchor,
					tableName = tableName,
					items = LootRoller.roll(tableName, rng),
					refillAt = 0,
					openedBy = {},
					lockedUntil = {},
					lastOpen = 0,
				}
				-- visual marker so the container can be found in the dark
				local marker = Instance.new("Part")
				marker.Name = "LootMarker"
				marker.Size = Vector3.new(0.7, 0.7, 0.7)
				marker.Shape = Enum.PartType.Ball
				marker.Material = Enum.Material.Neon
				marker.Color = Color3.fromRGB(196, 168, 92)
				marker.Anchored = true
				marker.CanCollide = false
				marker.CanQuery = false
				marker.CanTouch = false
				marker.Transparency = 0.35
				marker.CFrame = anchor.CFrame
				marker.Parent = anchor
			end
			if index % 200 == 0 then
				task.wait()
			end
		end
		print(string.format("[Loot] %d containers ready", anchorCount))
	end)

	task.spawn(function()
		while true do
			task.wait(20)
			local now = os.clock()
			for anchor, container in pairs(containers) do
				if now >= container.refillAt and #container.items == 0 then
					container.items = LootRoller.roll(container.tableName, rng)
					container.openedBy = {}
				end
				-- clean up drops that were not picked up
				for dropId, drop in pairs(worldDrops) do
					if now - drop.createdAt > 240 then
						if drop.part and drop.part.Parent then
							drop.part:Destroy()
						end
						worldDrops[dropId] = nil
					end
				end
			end
		end
	end)
end

function LootService.GetContainer(anchor)
	if not anchor then return nil end
	if not containers[anchor] then
		local tableName = "residential"
		local tag = anchor:FindFirstChild("LootTable")
		if tag and type(tag.Value) == "string" then
			tableName = tag.Value
		end
		containers[anchor] = {
			anchor = anchor, tableName = tableName,
			items = LootRoller.roll(tableName, rng), refillAt = 0, openedBy = {}, lockedUntil = {},
			lastOpen = 0,
		}
	end
	return containers[anchor]
end

local nextDropId = 0

function LootService.CreateWorldDrop(position, itemId, count)
	local def = Items[itemId]
	if not def then return nil end
	nextDropId += 1
	local part = Instance.new("Part")
	part.Name = "DroppedItem"
	part.Size = Vector3.new(1.4, 1.4, 1.4)
	part.Material = Enum.Material.SmoothPlastic
	part.Color = Color3.fromRGB(150, 140, 110)
	part.Anchored = true
	part.CanCollide = false
	part.CFrame = CFrame.new(position)
	part.Parent = Workspace
	local kind = Instance.new("StringValue")
	kind.Name = "Kind"
	kind.Value = "WorldDrop"
	kind.Parent = part
	local id = Instance.new("StringValue")
	id.Name = "DropId"
	id.Value = tostring(nextDropId)
	id.Parent = part
	local uid = Instance.new("StringValue")
	uid.Name = "ItemId"
	uid.Value = itemId
	uid.Parent = part
	local amount = Instance.new("IntValue")
	amount.Name = "Count"
	amount.Value = count or 1
	amount.Parent = part
	worldDrops[tostring(nextDropId)] = { part = part, itemId = itemId, count = count or 1, createdAt = os.clock() }
	return part
end

function LootService.Open(player, anchor)
	local profile = registryRef.PlayerDataService.Get(player)
	if not profile then return false end
	local container = LootService.GetContainer(anchor)
	if not container then return false end
	local now = os.time()
	if container.lockedUntil[player.UserId] and container.lockedUntil[player.UserId] > now then
		return false
	end
	if #container.items == 0 then
		remotes.Fire(player, "Notify", "Empty. Someone was here first.", "info", 2)
		return false
	end
	container.lastOpen = now
	container.lockedUntil[player.UserId] = now + 2
	remotes.Fire(player, "ProximityLoot", container.items, container.tableName)
	return true
end

function LootService.Take(player, anchor, index)
	local profile = registryRef.PlayerDataService.Get(player)
	if not profile then return false end
	local container = LootService.GetContainer(anchor)
	if not container then return false end
	index = tonumber(index)
	if not index then
		index = 1
	end
	local entry = container.items[index]
	if not entry then return false end
	local uid = registryRef.InventoryService.AddItem(player, entry.id, entry.count, { silent = false })
	if not uid then
		return false
	end
	table.remove(container.items, index)
	if #container.items == 0 then
		container.refillAt = os.clock() + Config.Interaction.LootRespawn
	end
	remotes.Fire(player, "ProximityLoot", container.items, container.tableName)
	return true, entry
end

function LootService.TakeAll(player, anchor)
	local profile = registryRef.PlayerDataService.Get(player)
	if not profile then return false end
	local container = LootService.GetContainer(anchor)
	if not container then return false end
	local taken = {}
	for index = #container.items, 1, -1 do
		local entry = container.items[index]
		local uid = registryRef.InventoryService.AddItem(player, entry.id, entry.count, { silent = true })
		if uid then
			table.insert(taken, entry)
			table.remove(container.items, index)
		end
	end
	if #container.items == 0 then
		container.refillAt = os.clock() + Config.Interaction.LootRespawn
	end
	if #taken > 0 then
		remotes.Fire(player, "LootResult", true, taken, container.tableName)
		remotes.Fire(player, "Notify", "Took: " .. LootRoller.summary(taken), "loot", 3.2)
	else
		remotes.Fire(player, "Notify", "You can't carry any more.", "warn", 2.4)
	end
	remotes.Fire(player, "ProximityLoot", container.items, container.tableName)
	return #taken > 0, taken
end

function LootService.PickupWorldDrop(player, part)
	local dropId = part:FindFirstChild("DropId")
	if not dropId then return false end
	local drop = worldDrops[dropId.Value]
	if not drop then return false end
	local uid = registryRef.InventoryService.AddItem(player, drop.itemId, drop.count)
	if not uid then return false end
	worldDrops[dropId.Value] = nil
	part:Destroy()
	return true
end

function LootService.Stats()
	local total = 0
	local filled = 0
	for _, container in pairs(containers) do
		total += 1
		if #container.items > 0 then filled += 1 end
	end
	return total, filled
end

return LootService
