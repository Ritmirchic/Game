--!strict
--[[
    ASHES OF HUMANITY: LAST SIGNAL
    ServerScriptService.Services.CraftingService
    Server-Authoritative Crafting System with Workstation Proximity Checks,
    Blueprint Unlocks, Timed Crafting Validation, and Atomic Resource Consumption (Section 19).
]]

local Players = game:GetService("Players")
local ReplicatedStorage = game:GetService("ReplicatedStorage")
local Workspace = game:GetService("Workspace")

local CraftingRecipes = require(ReplicatedStorage.Shared.CraftingRecipes)
local NetworkSchema = require(ReplicatedStorage.Shared.NetworkSchema)

local CraftingService = {}

local registry: any = nil
local busyUntil: { [number]: number } = {}

function CraftingService:Init(serviceRegistry: any)
	registry = serviceRegistry
	Players.PlayerRemoving:Connect(function(player: Player)
		busyUntil[player.UserId] = nil
	end)
end

function CraftingService:Start() end

function CraftingService:IsNearStation(player: Player, stationType: string): boolean
	if stationType == "Hand" then
		return true
	end
	local root = registry.SecurityService:GetCharacterRoot(player)
	if not root then
		return false
	end
	local pos = root.Position

	-- 1. Check player-built workstations via BuildingService
	if registry.BuildingService and registry.BuildingService:HasNearbyStation(pos, stationType, 15.0) then
		return true
	end

	-- 2. Check static world workstations (Shelter / Cabins / Factories / Bunkers)
	for _, folderName in ipairs({ "InteractionObjects", "World" }) do
		local folder = Workspace:FindFirstChild(folderName)
		if folder then
			for _, desc in ipairs(folder:GetDescendants()) do
				if desc:GetAttribute("StationType") == stationType then
					local sPos: Vector3? = nil
					if desc:IsA("BasePart") then
						sPos = desc.Position
					elseif desc:IsA("Model") then
						sPos = desc:GetPivot().Position
					end
					if sPos and (sPos - pos).Magnitude <= 15.0 then
						return true
					end
				end
			end
		end
	end

	return false
end

function CraftingService:UnlockBlueprint(player: Player, recipeId: string)
	local profile = registry.PlayerDataService:GetProfile(player)
	if not profile then
		return
	end
	profile.UnlockedBlueprints[recipeId] = true
	registry.SendToPlayer(player, NetworkSchema.EVENTS.Notification, {
		Kind = "Unlock",
		Text = "Blueprint Unlocked: " .. recipeId,
	})
end

function CraftingService:CraftRecipe(player: Player, recipeId: string): (boolean, string?)
	local recipe = CraftingRecipes.Get(recipeId)
	if not recipe then
		return false, "UnknownRecipe"
	end

	local profile = registry.PlayerDataService:GetProfile(player)
	if not profile or profile.Vitals.Health <= 0 or profile.Vitals.IsDowned then
		return false, "Unavailable"
	end

	local now = os.clock()
	if busyUntil[player.UserId] and now < busyUntil[player.UserId] then
		return false, "AlreadyCrafting"
	end

	if recipe.RequiresBlueprint and not profile.UnlockedBlueprints[recipe.Id] then
		return false, "BlueprintLocked"
	end

	if not self:IsNearStation(player, recipe.Station) then
		registry.SendToPlayer(player, NetworkSchema.EVENTS.Notification, {
			Kind = "Warning",
			Text = "Requires nearby " .. recipe.Station .. " to craft " .. recipe.Name .. ".",
		})
		return false, "MissingStation"
	end

	-- Verify all ingredients before consuming any
	for _, ing in ipairs(recipe.Ingredients) do
		if registry.InventoryService:CountItem(player, ing.ItemId) < ing.Count then
			return false, "InsufficientMaterials"
		end
	end

	-- Consume ingredients atomically
	for _, ing in ipairs(recipe.Ingredients) do
		if not registry.InventoryService:RemoveItemById(player, ing.ItemId, ing.Count, true) then
			return false, "ConsumeFailed"
		end
	end

	busyUntil[player.UserId] = now + math.min(recipe.CraftSeconds * 0.35, 1.5)

	-- Grant crafted output; if spatial grid is full, safely drop at player's feet so nothing is lost
	local ok = registry.InventoryService:AddItem(player, recipe.OutputItemId, recipe.OutputCount, nil, false)
	if not ok and registry.LootService then
		local root = registry.SecurityService:GetCharacterRoot(player)
		local dropPos = if root then root.Position + Vector3.new(0, 0.5, 2.0) else Vector3.zero
		registry.LootService:SpawnDroppedItem(recipe.OutputItemId, recipe.OutputCount, 100, dropPos)
		registry.InventoryService:SyncInventory(player)
	end

	profile.Stats.ItemsCrafted += 1
	if registry.QuestService then
		registry.QuestService:NotifyProgress(player, "CraftItem", recipe.OutputItemId, recipe.OutputCount)
	end

	registry.SendToPlayer(player, NetworkSchema.EVENTS.Notification, {
		Kind = "Success",
		Text = string.format("Crafted %s (x%d)", recipe.Name, recipe.OutputCount),
	})
	return true, nil
end

function CraftingService:HandleRemoteAction(player: Player, payload: any)
	if type(payload) ~= "table" or not NetworkSchema.ValidateString(payload.RecipeId, 48) then
		return
	end
	registry.SecurityService:WithTransactionLock(player, function()
		return self:CraftRecipe(player, payload.RecipeId)
	end)
end

return CraftingService
