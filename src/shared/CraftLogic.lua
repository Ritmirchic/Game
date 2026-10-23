--!strict
--[[
	CraftLogic — pure crafting rules shared by client preview and server authority.
]]

local Recipes = require(script.Parent.Recipes)
local Items = require(script.Parent.ItemDefinitions)
local Inventory = require(script.Parent.InventoryModel)

local Craft = {}

function Craft.knowsRecipe(profile, recipeId)
	local recipe = Recipes[recipeId]
	if not recipe then return false, "unknown recipe" end
	if not recipe.blueprint then
		return true
	end
	-- blueprints are consumed on first use and recorded in profile.blueprints
	if profile and profile.blueprints and profile.blueprints[recipe.blueprint] then
		return true
	end
	if profile and profile.inventory then
		local count = Inventory.countItem(profile.inventory, recipe.blueprint, true)
		if count > 0 then return true end
	end
	return false, "blueprint required"
end

function Craft.stationMatches(recipe, station)
	if not recipe.station then return true end
	return recipe.station == station
end

function Craft.canCraft(profile, recipeId, station)
	local recipe = Recipes[recipeId]
	if not recipe then return false, "unknown recipe" end
	local knows, why = Craft.knowsRecipe(profile, recipeId)
	if not knows then return false, why end
	if not Craft.stationMatches(recipe, station) then
		return false, "requires " .. tostring(recipe.station)
	end
	local inventory = profile.inventory
	for id, count in pairs(recipe.ingredients) do
		local have = Inventory.countItem(inventory, id, true)
		if have < count then
			local def = Items[id]
			return false, string.format("need %d %s (have %d)", count, def and def.name or id, have)
		end
	end
	-- weight check on the output
	local outDef = Items[recipe.output.id]
	if outDef then
		local newWeight = Inventory.totalWeight(inventory) + outDef.weight * (recipe.output.count or 1)
		if newWeight > Inventory.capacity(inventory) + 0.001 then
			return false, "not enough carrying capacity"
		end
	end
	return true
end

function Craft.consumeIngredients(profile, recipeId)
	local recipe = Recipes[recipeId]
	if not recipe then return false end
	for id, count in pairs(recipe.ingredients) do
		if not Inventory.consumeItem(profile.inventory, id, count) then
			return false
		end
	end
	return true
end

function Craft.applyOutput(profile, recipeId)
	local recipe = Recipes[recipeId]
	if not recipe then return nil end
	if recipe.blueprint then
		profile.blueprints = profile.blueprints or {}
		profile.blueprints[recipe.blueprint] = true
	end
	local uid, err = Inventory.addItem(profile.inventory, recipe.output.id, recipe.output.count)
	return uid, err
end

function Craft.listAvailable(profile, station)
	local out = {}
	for id, recipe in pairs(Recipes) do
		if type(recipe) == "table" then
			local ok, reason = Craft.canCraft(profile, id, station)
			local knows = Craft.knowsRecipe(profile, id)
			if knows then
				table.insert(out, {
					id = id, recipe = recipe, craftable = ok, reason = reason,
					station = recipe.station,
				})
			end
		end
	end
	table.sort(out, function(a, b)
		if a.craftable ~= b.craftable then
			return a.craftable
		end
		return a.recipe.name < b.recipe.name
	end)
	return out
end

function Craft.missingIngredients(profile, recipeId)
	local recipe = Recipes[recipeId]
	local missing = {}
	if not recipe then return missing end
	for id, count in pairs(recipe.ingredients) do
		local have = Inventory.countItem(profile.inventory, id, true)
		if have < count then
			table.insert(missing, { id = id, need = count, have = have })
		end
	end
	return missing
end

return Craft
