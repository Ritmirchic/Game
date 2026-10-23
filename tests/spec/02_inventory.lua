-- Grid inventory behaviour: placement, stacking, weight, equipment, dupes.

local Inventory = require("InventoryModel")
local Items = require("ItemDefinitions")
local Config = require("Config")

local function fresh()
	return Inventory.newState()
end

local function findItem(state, itemId)
	for uid, entry in pairs(state.containers.Backpack.items) do
		if entry.itemId == itemId then return entry, uid end
	end
	return nil
end

describe("Inventory: placement", function()
	it("starts with the three configured containers", function()
		local state = fresh()
		eq(state.containers.Backpack.cols, Config.Grid.BackpackCols)
		eq(state.containers.Backpack.rows, Config.Grid.BackpackRows)
		expect(state.containers.Pockets ~= nil)
		expect(state.containers.Ammo ~= nil)
	end)

	it("adds an item and reports it back", function()
		local state = fresh()
		local uid = Inventory.addItem(state, "bandage", 2)
		expect(type(uid) == "string", "expected a uid")
		eq(Inventory.countItem(state, "bandage"), 2)
	end)

	it("refuses unknown items", function()
		local state = fresh()
		local uid, reason = Inventory.addItem(state, "not_a_real_item", 1)
		eq(uid, nil)
		eq(reason, "unknown item")
	end)

	it("does not overlap two items in the same cell", function()
		local state = fresh()
		Inventory.addItem(state, "rifle", 1)
		Inventory.addItem(state, "shotgun", 1)
		local seen = {}
		for _, entry in pairs(state.containers.Backpack.items) do
			local w, h = Inventory.footprint(entry.itemId)
			for dy = 0, h - 1 do
				for dx = 0, w - 1 do
					local key = (entry.x + dx) .. ":" .. (entry.y + dy)
					expect(seen[key] == nil, "overlapping cells at " .. key)
					seen[key] = true
				end
			end
		end
	end)

	it("rejects an out of bounds placement", function()
		local state = fresh()
		local ok, reason = Inventory.canPlace(state.containers.Backpack, "rifle", 5, 5)
		eq(ok, false)
		eq(reason, "out of bounds")
	end)
end)

describe("Inventory: stacking", function()
	it("merges into an existing stack before using a new slot", function()
		local state = fresh()
		Inventory.addItem(state, "bandage", 1)
		Inventory.addItem(state, "bandage", 1)
		local count = 0
		for _ in pairs(state.containers.Backpack.items) do count += 1 end
		eq(count, 1)
		local entry = findItem(state, "bandage")
		eq(entry.count, 2)
	end)

	it("respects the per item stack limit", function()
		local state = fresh()
		local def = Items.bandage
		Inventory.addItem(state, "bandage", def.stack + 1)
		local stacks = {}
		for _, entry in pairs(state.containers.Backpack.items) do
			if entry.itemId == "bandage" then
				table.insert(stacks, entry.count)
				expect(entry.count <= def.stack, "stack exceeded limit")
			end
		end
		eq(#stacks, 2)
	end)

	it("reports failure when the grid is full", function()
		local state = fresh()
		local filled = 0
		for _ = 1, 200 do
			local uid = Inventory.addItem(state, "rifle", 1)
			if uid then filled += 1 end
		end
		local uid, reason = Inventory.addItem(state, "rifle", 1)
		eq(uid, nil)
		eq(reason, "no space")
		expect(filled > 0)
	end)
end)

describe("Inventory: consumption", function()
	it("consumes across stacks and never goes negative", function()
		local state = fresh()
		Inventory.addItem(state, "bandage", 4)
		expect(Inventory.consumeItem(state, "bandage", 3))
		eq(Inventory.countItem(state, "bandage"), 1)
		expect(Inventory.consumeItem(state, "bandage", 1))
		eq(Inventory.countItem(state, "bandage"), 0)
		eq(Inventory.consumeItem(state, "bandage", 1), false)
	end)

	it("removes by uid without duplicating the item", function()
		local state = fresh()
		local uid = Inventory.addItem(state, "medkit", 1)
		local ok = Inventory.removeByUid(state, uid)
		expect(ok)
		eq(Inventory.countItem(state, "medkit"), 0)
		eq(Inventory.removeByUid(state, uid), false)
	end)
end)

describe("Inventory: weight and equipment", function()
	it("tracks weight from item definitions", function()
		local state = fresh()
		Inventory.addItem(state, "rifle", 1)
		Inventory.addItem(state, "bandage", 2)
		local expected = Items.rifle.weight + Items.bandage.weight * 2
		near(Inventory.totalWeight(state), expected, 0.01)
	end)

	it("equips a weapon into the right slot", function()
		local state = fresh()
		local uid = Inventory.addItem(state, "rifle", 1)
		local ok = Inventory.equipFromUid(state, uid)
		expect(ok, "equip failed")
		local equipped = Inventory.getEquipped(state, "Primary")
		expect(equipped ~= nil, "no primary equipped")
		eq(equipped.itemId, "rifle")
	end)

	it("unequipping returns the item to the grid exactly once", function()
		local state = fresh()
		local uid = Inventory.addItem(state, "pistol", 1)
		local ok, slot = Inventory.equipFromUid(state, uid)
		expect(ok, "equip failed")
		eq(slot, "Primary")
		expect(Inventory.getEquipped(state, slot) ~= nil)
		expect(Inventory.unequip(state, slot))
		eq(Inventory.getEquipped(state, slot), nil)
		eq(Inventory.countItem(state, "pistol"), 1)
	end)

	it("serialisation survives a round trip", function()
		local state = fresh()
		Inventory.addItem(state, "rifle", 1)
		Inventory.addItem(state, "bandage", 3)
		local copy = Inventory.deserialize(Inventory.serialize(state))
		eq(Inventory.countItem(copy, "rifle"), 1)
		eq(Inventory.countItem(copy, "bandage"), 3)
	end)
end)
