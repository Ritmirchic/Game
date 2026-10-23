-- Base building: snapping, overlap rejection, affordability, refunds.

local BL = require("BuildLogic")
local Build = require("BuildDefinitions")
local Inventory = require("InventoryModel")
local Items = require("ItemDefinitions")

local function profileWith(items)
	local profile = { inventory = Inventory.newState(), blueprints = {} }
	for itemId, count in pairs(items or {}) do
		Inventory.addItem(profile.inventory, itemId, count)
	end
	return profile
end

local function firstPiece(predicate)
	for id, def in pairs(Build) do
		if type(def) == "table" and def.size and (not predicate or predicate(id, def)) then
			return id, def
		end
	end
	return nil
end

describe("BuildDefinitions", function()
	it("defines a usable set of pieces", function()
		local count = 0
		for _, def in pairs(Build) do
			if type(def) == "table" and def.size then count += 1 end
		end
		expect(count >= 8, "expected at least eight build pieces, got " .. count)
	end)

	it("every piece has size, cost, health and a real cost item", function()
		for id, def in pairs(Build) do
			if type(def) == "table" and def.size then
				expect(def.name and #def.name > 0, "no name on " .. id)
				expect(def.size.X > 0 and def.size.Y > 0 and def.size.Z > 0, "bad size on " .. id)
				expect(def.health and def.health > 0, "bad health on " .. id)
				local costTotal = 0
				for itemId, count in pairs(def.cost or {}) do
					expect(Items[itemId] ~= nil, "piece " .. id .. " costs unknown item " .. itemId)
					expect(count >= 1, "piece " .. id .. " has a non-positive cost")
					costTotal += count
				end
				expect(costTotal > 0, "piece " .. id .. " is free")
			end
		end
	end)
end)

describe("BuildLogic: grid", function()
	it("snaps positions onto the grid", function()
		local _, def = firstPiece()
		local snapped = BL.snapPosition(def, Vector3.new(3.7, 0.4, -9.9), 0)
		eq(snapped.X % 1, 0)
		eq(snapped.Z % 1, 0)
		near(snapped.Y, 0.4, 0.001)
	end)

	it("snaps rotation to 15 degree steps", function()
		eq(BL.snapRotation(0), 0)
		eq(BL.snapRotation(7), 0)
		eq(BL.snapRotation(8), 15)
		eq(BL.snapRotation(89), 90)
		eq(BL.snapRotation(359), 0)
	end)
end)

describe("BuildLogic: validation", function()
	it("rejects overlap with another structure", function()
		local id, def = firstPiece()
		local profile = profileWith({ scrap_metal = 40, wood = 40, cloth = 20, duct_tape = 10 })
		local position = Vector3.new(0, 0, 0)
		local others = {
			{ position = Vector3.new(0, 0, 0), rotation = 0, size = def.size },
		}
		local ok, reason = BL.validatePlacement(profile, id, position, 0, others, true)
		eq(ok, false)
		expect(string.find(reason or "", "blocked") ~= nil, "unexpected reason: " .. tostring(reason))
	end)

	it("accepts a clear placement on the ground", function()
		local id = firstPiece()
		local profile = profileWith({ scrap_metal = 60, wood = 80, cloth = 40, duct_tape = 20, nails = 40 })
		local ok, reason = BL.validatePlacement(profile, id, Vector3.new(24, 0, 24), 0, {}, true)
		expect(ok, tostring(reason))
	end)

	it("refuses a wall floating in the air", function()
		local wallId, wallDef = firstPiece(function(_, def) return def.size.Y > 6 end)
		expect(wallId ~= nil, "expected at least one tall piece")
		local profile = profileWith({ scrap_metal = 60, wood = 200, cloth = 40, duct_tape = 20, nails = 40 })
		local ok, reason = BL.validatePlacement(profile, wallId, Vector3.new(60, 40, 60), 0, {}, false)
		eq(ok, false)
		expect(string.find(reason or "", "foundation") ~= nil, "unexpected reason: " .. tostring(reason))
	end)

	it("refuses when materials are missing", function()
		local id = firstPiece()
		local profile = profileWith({})
		local ok, reason = BL.validatePlacement(profile, id, Vector3.new(0, 0, 0), 0, {}, true)
		eq(ok, false)
		expect(type(reason) == "string" and #reason > 0)
		eq(BL.canAfford(profile, id), false)
	end)
end)

describe("BuildLogic: costs", function()
	it("consumes exactly the listed cost", function()
		local id, def = firstPiece()
		local profile = profileWith({ scrap_metal = 100, wood = 200, cloth = 100, duct_tape = 100, nails = 100 })
		local before = {}
		for itemId, count in pairs(def.cost) do
			before[itemId] = Inventory.countItem(profile.inventory, itemId)
		end
		expect(BL.consumeCost(profile, id))
		for itemId, count in pairs(def.cost) do
			eq(Inventory.countItem(profile.inventory, itemId), before[itemId] - count)
		end
	end)

	it("refunds a salvage fraction, never more than the cost", function()
		local id, def = firstPiece()
		local profile = profileWith({ scrap_metal = 100, wood = 200, cloth = 100, duct_tape = 100, nails = 100 })
		local before = {}
		for itemId, count in pairs(def.cost) do
			before[itemId] = Inventory.countItem(profile.inventory, itemId)
		end
		expect(BL.consumeCost(profile, id))
		BL.refund(profile, id, 0.5)
		for itemId, count in pairs(def.cost) do
			local after = Inventory.countItem(profile.inventory, itemId)
			expect(after <= before[itemId], "refund created materials")
			expect(after >= before[itemId] - count, "refund lost everything")
		end
	end)

	it("structure health scales with quality", function()
		local id = firstPiece()
		local weak = BL.healthFor(id, 0.6)
		local perfect = BL.healthFor(id, 1)
		expect(perfect >= weak)
		expect(perfect > 0)
	end)
end)
