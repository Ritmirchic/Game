--!strict
--[[
	CraftingService — validates and executes crafting requests.

	The client shows a preview using the same CraftLogic module, but the server
	re-checks ingredients, station proximity, blueprints and capacity before
	consuming anything.
]]

local ReplicatedStorage = game:GetService("ReplicatedStorage")

local Shared = ReplicatedStorage:WaitForChild("Shared")
local CraftLogic = require(Shared.CraftLogic)
local Recipes = require(Shared.Recipes)
local Items = require(Shared.ItemDefinitions)
local Config = require(Shared.Config)

local CraftingService = {}

local registryRef
local remotes
local crafting = {} -- [userId] = { recipeId, finishAt }
local stationAnchors = {}

function CraftingService.Init(registry)
	registryRef = registry
	remotes = registry.Remotes
end

function CraftingService.Start(registry)
	remotes.On("RequestCraft", function(player, recipeId)
		CraftingService.Craft(player, recipeId)
	end)
end

-- Called when the player interacts with a workstation in the world. The
-- crafting UI opens on the client; the server keeps the authority for the
-- actual craft request (see CraftingService.Craft).
function CraftingService.OpenStation(player, instance, kind)
	if typeof(instance) ~= "instance" then return false end
	local character = player.Character
	local root = character and character:FindFirstChild("HumanoidRootPart")
	if not root then return false end
	if (instance.Position - root.Position).Magnitude > Config.Interaction.Range + 6 then
		return false
	end
	remotes.Fire(player, "StoryEvent", "open_crafting", { station = kind, name = instance.Name })
	if registryRef.DirectorService then
		registryRef.DirectorService.CheckTutorial(player, registryRef.PlayerDataService.Get(player), "craft_station")
	end
	return true
end

function CraftingService.NearestStation(player, kind)
	local character = player.Character
	local root = character and character:FindFirstChild("HumanoidRootPart")
	if not root then return nil, math.huge end
	local world = workspace:FindFirstChild("World")
	if not world then return nil, math.huge end
	local best, bestDist = nil, math.huge
	for _, descendant in ipairs(world:GetDescendants()) do
		local tag = descendant:FindFirstChild("Kind")
		if tag and tag.Value == kind then
			local d = (descendant.Position - root.Position).Magnitude
			if d < bestDist and d <= Config.Interaction.Range + 6 then
				best, bestDist = descendant, d
			end
		end
	end
	return best, bestDist
end

function CraftingService.Craft(player, recipeId)
	local profile = registryRef.PlayerDataService.Get(player)
	if not profile or type(recipeId) ~= "string" then return false end
	local recipe = Recipes[recipeId]
	if not recipe then return false end

	if crafting[player.UserId] then
		remotes.Fire(player, "CraftResult", false, recipeId, "Already crafting something.")
		return false
	end

	-- station check
	local station = recipe.station
	if station then
		local anchor = CraftingService.NearestStation(player, station)
		if not anchor then
			remotes.Fire(player, "CraftResult", false, recipeId, "Requires a " .. station .. " nearby.")
			return false
		end
	end

	local ok, reason = CraftLogic.canCraft(profile, recipeId, station)
	if not ok then
		remotes.Fire(player, "CraftResult", false, recipeId, reason or "Cannot craft that.")
		return false
	end

	if not CraftLogic.consumeIngredients(profile, recipeId) then
		remotes.Fire(player, "CraftResult", false, recipeId, "Missing ingredients.")
		return false
	end

	crafting[player.UserId] = { recipeId = recipeId, finishAt = os.clock() + recipe.time }
	remotes.Fire(player, "CraftResult", true, recipeId, "Crafting " .. recipe.name .. "...")
	registryRef.PlayerDataService.MarkDirty(player)

	task.delay(recipe.time, function()
		crafting[player.UserId] = nil
		if not player.Parent then return end
		local p = registryRef.PlayerDataService.Get(player)
		if not p then return end
		local uid, err = CraftLogic.applyOutput(p, recipeId)
		if uid then
			p.stats.crafted = (p.stats.crafted or 0) + 1
			remotes.Fire(player, "CraftResult", true, recipeId, "Crafted " .. recipe.name)
			remotes.Fire(player, "InventoryUpdate", require(Shared.InventoryModel).serialize(p.inventory))
			if registryRef.QuestService then
				registryRef.QuestService.Event(player, { type = "craft", id = recipeId })
				registryRef.QuestService.OnItemChanged(player, p)
			end
			if registryRef.DirectorService then
				registryRef.DirectorService.CheckTutorial(player, p, "craft")
			end
		else
			remotes.Fire(player, "CraftResult", false, recipeId, err or "No space for the result.")
			-- refund the ingredients so nothing is lost to a full bag
			for id, count in pairs(recipe.ingredients) do
				require(Shared.InventoryModel).addItem(p.inventory, id, count)
			end
		end
	end)
	return true
end

function CraftingService.Cancel(player)
	crafting[player.UserId] = nil
end

function CraftingService.RecipeList()
	local out = {}
	for id, recipe in pairs(Recipes) do
		if type(recipe) == "table" then
			table.insert(out, {
				id = id, name = recipe.name, station = recipe.station,
				time = recipe.time, ingredients = recipe.ingredients,
				output = recipe.output, desc = recipe.desc, blueprint = recipe.blueprint,
			})
		end
	end
	return out
end

return CraftingService
