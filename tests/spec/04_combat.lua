-- Combat math: falloff, armour, headshots, ammo, armour aggregation.

local CM = require("CombatMath")
local Weapons = require("WeaponDefinitions")
local Items = require("ItemDefinitions")
local Config = require("Config")

local rifle = Weapons.rifle

describe("CombatMath: damage", function()
	it("damage falls off with distance", function()
		local near = CM.hitDamage(rifle, 5, "Torso", nil, 1)
		local far = CM.hitDamage(rifle, 400, "Torso", nil, 1)
		expect(near > far, "no falloff applied")
		expect(far > 0, "damage must never be negative")
	end)

	it("headshots multiply damage", function()
		local body = CM.hitDamage(rifle, 20, "Torso", nil, 1)
		local head = CM.hitDamage(rifle, 20, "Head", nil, 1)
		near(head, body * Config.Combat.HeadshotMultiplier, 0.01)
	end)

	it("armour reduces damage to the covered region only", function()
		local armor = { torso = 0.5, head = 0.2 }
		local torso = CM.hitDamage(rifle, 20, "Torso", armor, 1)
		local leg = CM.hitDamage(rifle, 20, "LeftLeg", armor, 1)
		local bare = CM.hitDamage(rifle, 20, "Torso", nil, 1)
		expect(torso < bare)
		near(leg, bare, 0.01)
	end)

	it("worn weapons deal less damage", function()
		local fresh = CM.hitDamage(rifle, 20, "Torso", nil, 1)
		local worn = CM.hitDamage(rifle, 20, "Torso", nil, 0.1)
		expect(worn < fresh)
	end)

	it("melee weapons have no distance falloff beyond their reach", function()
		local knife = Weapons.knife
		local near = CM.hitDamage(knife, 2, "Torso", nil, 1)
		local far = CM.hitDamage(knife, 400, "Torso", nil, 1)
		expect(near >= far * 0.2)
	end)
end)

describe("CombatMath: spread and recoil", function()
	it("zero spread keeps the direction", function()
		local direction = Vector3.new(0, 0, -1)
		local result = CM.spreadDirection(direction, 0, Random.new(1))
		near(result.X, 0, 1e-6)
		near(result.Z, -1, 1e-6)
	end)

	it("spread produces a unit vector within the cone", function()
		local direction = Vector3.new(0, 0, -1)
		for seed = 1, 25 do
			local result = CM.spreadDirection(direction, 0.06, Random.new(seed))
			near(result.Magnitude, 1, 1e-4)
			expect(result:Dot(direction) > 0.99, "spread travelled too far from the aim")
		end
	end)

	it("recoil impulse scales with the weapon", function()
		local pistol = CM.recoilImpulse(Weapons.pistol, 1)
		local heavy = CM.recoilImpulse(Weapons.rifle, 1)
		expect(type(pistol) == "number" and type(heavy) == "number")
		expect(heavy >= pistol, "rifle should kick at least as hard as a pistol")
	end)
end)

describe("CombatMath: ammunition", function()
	it("firing consumes exactly one round", function()
		local entry = { uid = "u1", itemId = "rifle", magazine = 3 }
		local left = CM.consumeAmmo(entry, Weapons.rifle, 1)
		eq(left, 2)
		eq(entry.magazine, 2)
	end)

	it("respects the weapon fire rate", function()
		local def = Weapons.pistol
		expect(CM.canFire(0, 0.02, def) == false, "fired inside the cooldown")
		expect(CM.canFire(0, 10, def) == true, "refused a legal shot")
	end)

	it("a full magazine lasts exactly its capacity", function()
		local entry = { uid = "u2", itemId = "pistol", magazine = Weapons.pistol.magazine }
		for _ = 1, Weapons.pistol.magazine do
			CM.consumeAmmo(entry, Weapons.pistol, 1)
		end
		eq(entry.magazine, 0)
		-- the server refuses to fire at zero; the math itself only counts down
		expect(entry.magazine >= 0)
	end)
end)

describe("CombatMath: hit resolution", function()
	it("picks a valid body part", function()
		local parts = {}
		for _, part in ipairs(Config.BodyParts) do parts[part] = true end
		local rng = Random.new(7)
		for _ = 1, 50 do
			local part = CM.pickBodyPart(nil, rng)
			expect(parts[part], "unknown body part " .. tostring(part))
		end
		eq(CM.pickBodyPart("LeftArm"), "LeftArm")
		eq(CM.pickBodyPart("Head"), "Head")
		eq(CM.pickBodyPart("HumanoidRootPart"), "Torso")
	end)

	it("aggregates armour across equipped pieces", function()
		local armor = CM.aggregateArmor({
			Head = { itemId = "gas_mask" },
			Armor = { itemId = "plate_carrier" },
		}, Items)
		expect(armor.torso > 0, "vest gave no torso protection")
		expect(armor.radiation > 0, "respirator gave no radiation protection")
		expect(armor.torso <= 0.85, "armour clamp failed")
	end)

	it("explosions fall off and never exceed the max", function()
		local center = Vector3.new(0, 0, 0)
		local close = CM.explosionDamage(center, Vector3.new(4, 0, 0), 120, 40)
		local far = CM.explosionDamage(center, Vector3.new(39, 0, 0), 120, 40)
		local outside = CM.explosionDamage(center, Vector3.new(80, 0, 0), 120, 40)
		expect(close > far)
		expect(close <= 120)
		expect(far >= 0)
		eq(outside, 0)
	end)

	it("every weapon definition is internally consistent", function()
		for id, def in pairs(Weapons) do
			if type(def) == "table" and def.damage then
				expect(def.damage > 0, "no damage on " .. id)
				expect(def.fireRate and def.fireRate > 0, "bad fire rate on " .. id)
				expect(def.magazine == nil or def.magazine >= 0, "bad magazine on " .. id)
				expect(def.range or def.falloff, "no range data on " .. id)
			end
		end
	end)
end)

describe("Weapons: item wiring", function()
	it("resolves every weapon item to a definition", function()
		for id, def in pairs(Items) do
			if def.weapon then
				local weapon = CM.weaponForItem(id, Items)
				expect(weapon ~= nil, "item " .. id .. " has no weapon definition")
			end
		end
	end)
end)
