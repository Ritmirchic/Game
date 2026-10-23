-- Quest state machine and crafting validation.

local QL = require("QuestLogic")
local Quests = require("QuestDefinitions")
local Craft = require("CraftLogic")
local Recipes = require("Recipes")
local Items = require("ItemDefinitions")
local Inventory = require("InventoryModel")

local function profileWith(items)
	local profile = {
		inventory = Inventory.newState(),
		blueprints = {},
	}
	for itemId, count in pairs(items or {}) do
		Inventory.addItem(profile.inventory, itemId, count)
	end
	return profile
end

describe("QuestDefinitions", function()
	it("has a main line in chapter order", function()
		local main = Quests.mainLine()
		expect(#main >= 6, "expected at least six story chapters, got " .. #main)
		for index = 2, #main do
			expect(main[index].chapter >= main[index - 1].chapter, "chapters out of order")
		end
	end)

	it("every objective is one of the implemented types", function()
		local kinds = {
			collect = true, kill = true, reach = true, interact = true,
			craft = true, build = true, survive = true, talk = true,
		}
		for id, quest in pairs(Quests) do
			if type(quest) == "table" and quest.objectives then
				expect(#quest.objectives > 0, "quest " .. id .. " has no objectives")
				for _, obj in ipairs(quest.objectives) do
					expect(kinds[obj.type], "quest " .. id .. " has unknown objective " .. tostring(obj.type))
					expect(type(obj.text) == "string" and #obj.text > 0, "objective without text in " .. id)
				end
			end
		end
	end)

	it("reward items exist", function()
		for id, quest in pairs(Quests) do
			if type(quest) == "table" and quest.rewards then
				for _, reward in ipairs(quest.rewards.items or {}) do
					expect(Items[reward.id] ~= nil,
						string.format("quest %s rewards unknown item %s", id, tostring(reward.id)))
				end
				for _, unlock in ipairs(quest.rewards.unlock or {}) do
					expect(Quests[unlock] ~= nil, "quest " .. id .. " unlocks unknown quest " .. unlock)
				end
			end
		end
	end)
end)

describe("QuestLogic", function()
	local quest = {
		id = "test_quest", name = "Test", brief = "b",
		objectives = {
			{ id = "a", type = "kill", enemy = "hollow", count = 2, text = "kill hollows" },
			{ id = "b", type = "collect", item = "bandage", count = 1, text = "find a bandage" },
		},
		rewards = { unlock = { "next_quest" } },
	}

	it("will not start a locked quest", function()
		local state = QL.newState()
		eq(QL.canStart(state, quest), false)
		eq(QL.start(state, quest, 1), false)
	end)

	it("force-started quests track progress", function()
		local state = QL.newState()
		expect(QL.forceStart(state, quest, 1))
		expect(QL.isActive(state, quest.id))
		local context = { countItem = function() return 0 end }
		QL.update(state, quest, { type = "kill", id = "hollow", amount = 1 }, context)
		eq(state.active[quest.id].objectives.a.progress, 1)
		QL.update(state, quest, { type = "kill", id = "hollow", amount = 1 }, context)
		eq(state.active[quest.id].objectives.a.done, true)
		eq(state.active[quest.id].complete, false)
	end)

	it("ignores unrelated events", function()
		local state = QL.newState()
		QL.forceStart(state, quest, 1)
		QL.update(state, quest, { type = "kill", id = "brute", amount = 5 }, { countItem = function() return 0 end })
		eq(state.active[quest.id].objectives.a.progress, 0)
	end)

	it("collect objectives use the inventory callback", function()
		local state = QL.newState()
		QL.forceStart(state, quest, 1)
		QL.update(state, quest, nil, { countItem = function() return 1 end })
		eq(state.active[quest.id].objectives.b.done, true)
	end)

	it("completes only when every objective is done", function()
		local state = QL.newState()
		QL.forceStart(state, quest, 1)
		local result = QL.update(state, quest, { type = "kill", id = "hollow", amount = 2 },
			{ countItem = function() return 0 end })
		eq(result.completedQuest, false)
		eq(state.active[quest.id].complete, false)
		result = QL.update(state, quest, nil, { countItem = function() return 1 end })
		eq(result.completedQuest, true)
		expect(QL.complete(state, quest, 99))
		expect(QL.isComplete(state, quest.id))
		eq(QL.isActive(state, quest.id), false)
		expect(state.unlocked.next_quest == true, "reward unlock missing")
	end)

	it("serialises and restores real quests", function()
		local real = Quests.mainLine()[1]
		local state = QL.newState()
		QL.forceStart(state, real, 5)
		local restored = QL.deserialize(QL.serialize(state))
		expect(QL.isActive(restored, real.id))
		eq(restored.active[real.id].startedAt, 5)
	end)
end)

describe("Recipes", function()
	it("all recipes reference real items", function()
		for id, recipe in pairs(Recipes) do
			if type(recipe) ~= "table" then
				continue
			end
			expect(type(recipe.output) == "table", "recipe " .. id .. " has no output table")
			expect(Items[recipe.output.id] ~= nil, "recipe " .. id .. " outputs unknown item")
			expect((recipe.output.count or 1) >= 1)
			expect(recipe.time > 0, "recipe " .. id .. " has no craft time")
			for itemId, count in pairs(recipe.ingredients) do
				expect(Items[itemId] ~= nil, "recipe " .. id .. " needs unknown item " .. itemId)
				expect(count >= 1)
			end
		end
	end)
end)

describe("CraftLogic", function()
	it("crafts when ingredients and station match", function()
		local profile = profileWith({ rags = 2, duct_tape = 1 })
		local ok, reason = Craft.canCraft(profile, "bandage", nil)
		expect(ok, reason)
		expect(Craft.consumeIngredients(profile, "bandage"))
		local uid = Craft.applyOutput(profile, "bandage")
		expect(type(uid) == "string", "no output item produced")
		eq(Inventory.countItem(profile.inventory, "bandage"), 1)
	end)

	it("refuses when ingredients are missing", function()
		local profile = profileWith({ rags = 1 })
		local ok, reason = Craft.canCraft(profile, "bandage", nil)
		eq(ok, false)
		expect(type(reason) == "string")
		eq(Craft.consumeIngredients(profile, "bandage"), false)
	end)

	it("refuses when the station does not match", function()
		local profile = profileWith({ rags = 2, duct_tape = 1 })
		local benchRecipe
		for id, recipe in pairs(Recipes) do
			if recipe.station then
				benchRecipe = id
				break
			end
		end
		expect(benchRecipe ~= nil, "expected at least one workstation recipe")
		local ok = Craft.canCraft(profile, benchRecipe, nil)
		eq(ok, false)
	end)

	it("blueprint recipes stay locked until known", function()
		local blueprintRecipe
		for id, recipe in pairs(Recipes) do
			if recipe.blueprint then
				blueprintRecipe = id
				break
			end
		end
		expect(blueprintRecipe ~= nil, "expected at least one blueprint recipe")
		local profile = profileWith({})
		eq(Craft.knowsRecipe(profile, blueprintRecipe), false)
		profile.blueprints[Recipes[blueprintRecipe].blueprint] = true
		expect(Craft.knowsRecipe(profile, blueprintRecipe) == true)
	end)

	it("consumes ingredients exactly once per craft", function()
		local profile = profileWith({ rags = 4, duct_tape = 2 })
		expect(Craft.consumeIngredients(profile, "bandage"))
		eq(Inventory.countItem(profile.inventory, "rags"), 2)
		eq(Inventory.countItem(profile.inventory, "duct_tape"), 1)
		expect(Craft.consumeIngredients(profile, "bandage"))
		eq(Inventory.countItem(profile.inventory, "rags"), 0)
		eq(Craft.consumeIngredients(profile, "bandage"), false)
		eq(Inventory.countItem(profile.inventory, "rags"), 0)
	end)
end)
