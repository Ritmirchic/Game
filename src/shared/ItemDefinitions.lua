--!strict
--[[
	ItemDefinitions — every item that can exist in the world.

	Field reference
	  id          unique string key
	  name        display name
	  cat         category (Food, Water, Medicine, Weapon, Ammo, Tool, Material,
	              Clothing, Armor, Electronic, Quest, Misc)
	  desc        flavour + mechanical description shown in the inventory
	  w, h        grid footprint in slots
	  weight      kilograms
	  stack       maximum stack size (1 = unstackable)
	  dur         durability for tools/weapons (nil = no durability)
	  rarity      1 common .. 5 legendary
	  use         { kind, amount, duration, condition } consumed/used effect
	  equip       equipment slot this item can be placed in
	  weapon      weapon definition id
	  tags        array of strings used by crafting/quests
]]

local Items = {}

local function item(t)
	assert(t.id and t.name, "item requires id and name")
	t.cat = t.cat or "Misc"
	t.w = t.w or 1
	t.h = t.h or 1
	t.weight = t.weight or 0.5
	t.stack = t.stack or 1
	t.rarity = t.rarity or 1
	t.tags = t.tags or {}
	Items[t.id] = t
	return t
end

-- --------------------------------------------------------------------- food
item {
	id = "canned_beans", name = "Canned Beans", cat = "Food", w = 2, h = 2, weight = 0.45,
	rarity = 1, stack = 3,
	desc = "Dented but sealed. Protein and salt, enough to quiet the stomach for a while.",
	use = { kind = "food", hunger = 26, thirst = -3, sanity = 1 },
	tags = { "food", "canned" },
}

item {
	id = "canned_fish", name = "Tinned Fish", cat = "Food", w = 2, h = 1, weight = 0.3,
	rarity = 2, stack = 3,
	desc = "The oil has gone cloudy. Still edible. Probably.",
	use = { kind = "food", hunger = 18, thirst = -2, health = 2 },
	tags = { "food", "canned" },
}

item {
	id = "energy_bar", name = "Energy Bar", cat = "Food", w = 1, h = 2, weight = 0.12,
	rarity = 1, stack = 5,
	desc = "Compressed grain and sugar. Restores a little stamina as well as hunger.",
	use = { kind = "food", hunger = 12, stamina = 14 },
	tags = { "food", "light" },
}

item {
	id = "crackers", name = "Stale Crackers", cat = "Food", w = 2, h = 1, weight = 0.18,
	rarity = 1, stack = 4,
	desc = "Dry enough to make you thirsty.",
	use = { kind = "food", hunger = 9, thirst = -4 },
	tags = { "food" },
}

item {
	id = "cooked_meat", name = "Cooked Meat", cat = "Food", w = 2, h = 2, weight = 0.55,
	rarity = 2, stack = 2,
	desc = "Charred over a campfire. Warm, filling, and safe.",
	use = { kind = "food", hunger = 38, health = 4, sanity = 2 },
	tags = { "food", "cooked", "meat" },
}

item {
	id = "raw_meat", name = "Raw Meat", cat = "Food", w = 2, h = 2, weight = 0.6,
	rarity = 2, stack = 2,
	desc = "Eating this raw is a good way to lose a week to fever. Cook it first.",
	use = { kind = "food", hunger = 16, infection = 22, sanity = -3 },
	tags = { "food", "raw", "meat" },
}

item {
	id = "mutant_gland", name = "Mutant Gland", cat = "Material", w = 1, h = 1, weight = 0.2,
	rarity = 3, stack = 5,
	desc = "A slick organ that twitches when light hits it. Alchemists of the old world paid well for these.",
	tags = { "material", "organic", "rare" },
}

-- -------------------------------------------------------------------- water
item {
	id = "water_bottle", name = "Water Bottle", cat = "Water", w = 1, h = 3, weight = 0.7,
	rarity = 1, stack = 1, dur = 100,
	desc = "A scratched plastic bottle. Refill it at a clean source, boil it if you can.",
	use = { kind = "drink", thirst = 34, container = "water_bottle" },
	tags = { "water", "container" },
}

item {
	id = "dirty_water", name = "Tainted Water", cat = "Water", w = 1, h = 3, weight = 0.7,
	rarity = 1, stack = 1,
	desc = "Brown and gritty. Drinking it undoes the good it does.",
	use = { kind = "drink", thirst = 24, infection = 14, sanity = -1 },
	tags = { "water", "dirty" },
}

item {
	id = "purified_water", name = "Purified Water", cat = "Water", w = 1, h = 3, weight = 0.7,
	rarity = 2, stack = 1,
	desc = "Filtered and boiled. The safest thing you will drink today.",
	use = { kind = "drink", thirst = 42, health = 1 },
	tags = { "water", "clean" },
}

item {
	id = "water_filter", name = "Field Filter", cat = "Tool", w = 2, h = 2, weight = 0.9,
	rarity = 3, dur = 40,
	desc = "Charcoal, cloth and patience. Converts tainted water into something survivable.",
	use = { kind = "craft_tool", convert = { from = "dirty_water", to = "purified_water" } },
	tags = { "tool", "filter" },
}

-- ----------------------------------------------------------------- medicine
item {
	id = "bandage", name = "Bandage", cat = "Medicine", w = 1, h = 2, weight = 0.08,
	rarity = 1, stack = 5,
	desc = "Stops bleeding. Does nothing for what is already inside the wound.",
	use = { kind = "medical", bleeding = -100, health = 6 },
	tags = { "medical", "bleeding" },
}

item {
	id = "rags", name = "Cloth Rags", cat = "Material", w = 1, h = 1, weight = 0.05,
	rarity = 1, stack = 8,
	desc = "Torn strips of fabric. The raw material for bandages and torches.",
	use = { kind = "medical", bleeding = -35, health = 2 },
	tags = { "material", "cloth", "medical" },
}

item {
	id = "painkillers", name = "Painkillers", cat = "Medicine", w = 1, h = 1, weight = 0.04,
	rarity = 2, stack = 4,
	desc = "Dulls pain and steadies shaking hands for a while.",
	use = { kind = "medical", health = 10, sanity = 8, effect = "painkiller" },
	tags = { "medical" },
}

item {
	id = "antibiotics", name = "Antibiotics", cat = "Medicine", w = 1, h = 2, weight = 0.06,
	rarity = 3, stack = 3,
	desc = "Expired by a decade, still the only thing that fights the infection.",
	use = { kind = "medical", infection = -55, health = 4 },
	tags = { "medical", "infection" },
}

item {
	id = "splint", name = "Splint", cat = "Medicine", w = 2, h = 2, weight = 0.35,
	rarity = 2, stack = 1,
	desc = "Wood and cloth to hold a fracture still. Moving will still hurt.",
	use = { kind = "medical", fracture = true },
	tags = { "medical", "fracture" },
}

item {
	id = "antirade", name = "Anti-Rad Dose", cat = "Medicine", w = 1, h = 2, weight = 0.1,
	rarity = 4, stack = 2,
	desc = "Military issue. Binds what has already seeped into the tissue.",
	use = { kind = "medical", radiation = -70, health = -4 },
	tags = { "medical", "radiation" },
}

item {
	id = "medkit", name = "Field Medkit", cat = "Medicine", w = 3, h = 2, weight = 0.8,
	rarity = 4, stack = 1,
	desc = "Everything needed to pull someone back from the edge, including a teammate.",
	use = { kind = "medical", health = 45, bleeding = -100, infection = -20 },
	tags = { "medical", "revive" },
}

item {
	id = "stimulant", name = "Combat Stimulant", cat = "Medicine", w = 1, h = 1, weight = 0.03,
	rarity = 4, stack = 3,
	desc = "Your heart will hate you later. Sprint now.",
	use = { kind = "medical", stamina = 60, fatigue = 12, sanity = -6 },
	tags = { "medical", "stim" },
}

item {
	id = "ampoule_mil", name = "Military Ampoule", cat = "Quest", w = 1, h = 2, weight = 0.1,
	rarity = 5, stack = 1,
	desc = "Sealed glass, military stencil. The hospital pharmacy was emptied except for this.",
	tags = { "quest" },
}

item {
	id = "lab_keycard", name = "Lab Keycard", cat = "Quest", w = 1, h = 1, weight = 0.02,
	rarity = 5, stack = 1,
	desc = "Level 4 clearance. The photograph on it has been scratched away.",
	tags = { "quest" },
}

item {
	id = "core_sample", name = "Core Sample", cat = "Quest", w = 2, h = 2, weight = 2.4,
	rarity = 5, stack = 1,
	desc = "A lead capsule humming faintly. The glass is warm to the touch.",
	tags = { "quest" },
}

-- ------------------------------------------------------------------ weapons
item {
	id = "knife", name = "Survival Knife", cat = "Weapon", w = 1, h = 2, weight = 0.4,
	rarity = 1, dur = 100, equip = "Melee", weapon = "knife",
	desc = "Short, quiet, and always useful. Wears down on bone.",
	tags = { "weapon", "melee", "blade" },
}

item {
	id = "hatchet", name = "Fire Axe", cat = "Weapon", w = 2, h = 3, weight = 1.6,
	rarity = 2, dur = 140, equip = "Melee", weapon = "hatchet",
	desc = "Heavy head, splintered handle. Splits wood, doors and skulls.",
	tags = { "weapon", "melee", "tool" },
}

item {
	id = "crowbar", name = "Crowbar", cat = "Tool", w = 1, h = 3, weight = 1.9,
	rarity = 2, dur = 200, equip = "Melee", weapon = "crowbar",
	desc = "Prying, smashing, and opening what is not meant to be opened.",
	tags = { "weapon", "melee", "tool" },
}

item {
	id = "baseball_bat", name = "Wrapped Bat", cat = "Weapon", w = 1, h = 4, weight = 1.2,
	rarity = 1, dur = 110, equip = "Melee", weapon = "bat",
	desc = "Barbed wire and tape. Reach without elegance.",
	tags = { "weapon", "melee" },
}

item {
	id = "pipe_wrench", name = "Pipe Wrench", cat = "Weapon", w = 2, h = 3, weight = 2.2,
	rarity = 2, dur = 160, equip = "Melee", weapon = "wrench",
	desc = "Industrial. Slow to swing, brutal when it lands.",
	tags = { "weapon", "melee", "tool" },
}

item {
	id = "pistol", name = "PM-9 Pistol", cat = "Weapon", w = 2, h = 2, weight = 1.1,
	rarity = 3, dur = 240, equip = "Primary", weapon = "pistol",
	desc = "Reliable sidearm. Loud enough to call everything in the block.",
	tags = { "weapon", "gun", "sidearm" },
}

item {
	id = "shotgun", name = "Sawn-off Shotgun", cat = "Weapon", w = 3, h = 2, weight = 3.2,
	rarity = 4, dur = 200, equip = "Primary", weapon = "shotgun",
	desc = "Two barrels of close-range certainty.",
	tags = { "weapon", "gun", "shotgun" },
}

item {
	id = "rifle", name = "AK-pattern Rifle", cat = "Weapon", w = 4, h = 2, weight = 4.1,
	rarity = 5, dur = 320, equip = "Primary", weapon = "rifle",
	desc = "Military surplus. Heavy, loud, and the only answer to a Brute.",
	tags = { "weapon", "gun", "rifle" },
}

item {
	id = "bolt_rifle", name = "Hunting Rifle", cat = "Weapon", w = 5, h = 2, weight = 4.6,
	rarity = 4, dur = 260, equip = "Primary", weapon = "bolt_rifle",
	desc = "Scoped, patient, and punishing if you miss.",
	tags = { "weapon", "gun", "rifle", "scope" },
}

item {
	id = "crossbow", name = "Improvised Crossbow", cat = "Weapon", w = 4, h = 3, weight = 3.4,
	rarity = 3, dur = 180, equip = "Primary", weapon = "crossbow",
	desc = "Silent, retrievable bolts. Building one is easier than finding one.",
	tags = { "weapon", "crossbow", "silent" },
}

item {
	id = "molotov", name = "Molotov", cat = "Weapon", w = 1, h = 3, weight = 0.9,
	rarity = 2, stack = 2, equip = "Secondary", weapon = "molotov",
	desc = "Fuel in glass. Throws fire in an arc and does not care who it lands on.",
	tags = { "weapon", "throwable", "fire" },
}

-- --------------------------------------------------------------------- ammo
item {
	id = "ammo_9mm", name = "9mm Rounds", cat = "Ammo", w = 1, h = 1, weight = 0.012,
	rarity = 2, stack = 60,
	desc = "Pistol ammunition. Count every round.",
	tags = { "ammo", "9mm" },
}

item {
	id = "ammo_shell", name = "12ga Shells", cat = "Ammo", w = 1, h = 1, weight = 0.045,
	rarity = 3, stack = 24,
	desc = "Buckshot. Forgiving aim, unforgiving recoil.",
	tags = { "ammo", "shell" },
}

item {
	id = "ammo_762", name = "7.62 Rounds", cat = "Ammo", w = 1, h = 2, weight = 0.024,
	rarity = 3, stack = 40,
	desc = "Rifle rounds on a stripper clip.",
	tags = { "ammo", "762" },
}

item {
	id = "bolt", name = "Crossbow Bolt", cat = "Ammo", w = 1, h = 2, weight = 0.06,
	rarity = 2, stack = 16,
	desc = "Fletched by hand from scrap. Usually survives the trip.",
	tags = { "ammo", "bolt" },
}

item {
	id = "fuel_can", name = "Fuel Can", cat = "Material", w = 2, h = 3, weight = 4.2,
	rarity = 3, stack = 1, dur = 100,
	desc = "Generator food and molotov base. Volatile under gunfire.",
	tags = { "material", "fuel", "flammable" },
}

-- -------------------------------------------------------------------- tools
item {
	id = "flashlight", name = "Damaged Flashlight", cat = "Tool", w = 1, h = 2, weight = 0.5,
	rarity = 1, dur = 100, equip = "Flashlight",
	desc = "Cracked lens, flickering beam. The dark is bigger than this beam.",
	tags = { "tool", "light" },
}

item {
	id = "headlamp", name = "Headlamp", cat = "Tool", w = 1, h = 1, weight = 0.3,
	rarity = 3, dur = 140, equip = "Head",
	desc = "Hands free light. Worth more than most weapons.",
	tags = { "tool", "light" },
}

item {
	id = "battery", name = "Battery", cat = "Electronic", w = 1, h = 1, weight = 0.15,
	rarity = 1, stack = 6,
	desc = "Recharges lights and small electronics.",
	tags = { "electronic", "power" },
}

item {
	id = "toolkit", name = "Toolkit", cat = "Tool", w = 2, h = 2, weight = 1.4,
	rarity = 2, dur = 120,
	desc = "Wrenches, screwdrivers, wire. Unlocks advanced building repairs.",
	tags = { "tool", "repair" },
}

item {
	id = "multimeter", name = "Multimeter", cat = "Electronic", w = 1, h = 2, weight = 0.3,
	rarity = 3,
	desc = "Find the fault, restore the power. Quests love this thing.",
	tags = { "electronic", "repair", "quest" },
}

item {
	id = "radio", name = "Old Radio", cat = "Electronic", w = 2, h = 2, weight = 1.2,
	rarity = 2, dur = 80,
	desc = "Static, and sometimes a voice. Tune it at night.",
	use = { kind = "radio" },
	tags = { "electronic", "story" },
}

item {
	id = "binoculars", name = "Binoculars", cat = "Tool", w = 2, h = 1, weight = 0.6,
	rarity = 3,
	desc = "See what is watching you before it decides to move.",
	use = { kind = "zoom" },
	tags = { "tool", "scout" },
}

item {
	id = "lockpick", name = "Lockpick Set", cat = "Tool", w = 1, h = 1, weight = 0.08,
	rarity = 2, stack = 3, dur = 60,
	desc = "Opens the doors that keys forgot.",
	tags = { "tool", "lockpick" },
}

item {
	id = "map_fragment", name = "Map Fragment", cat = "Tool", w = 2, h = 2, weight = 0.05,
	rarity = 2,
	desc = "Hand-drawn streets with notes in the margins. Reveals nearby landmarks.",
	use = { kind = "map" },
	tags = { "tool", "map" },
}

-- ---------------------------------------------------------------- materials
item {
	id = "scrap_metal", name = "Scrap Metal", cat = "Material", w = 1, h = 1, weight = 0.35,
	rarity = 1, stack = 20,
	desc = "Bent, rusted, still useful.",
	tags = { "material", "metal" },
}

item {
	id = "wood", name = "Wood Scrap", cat = "Material", w = 2, h = 1, weight = 0.4,
	rarity = 1, stack = 20,
	desc = "Splinters and nails. Building material and kindling.",
	tags = { "material", "wood" },
}

item {
	id = "cloth", name = "Cloth", cat = "Material", w = 1, h = 1, weight = 0.1,
	rarity = 1, stack = 15,
	desc = "Fabric salvaged from curtains and clothes.",
	tags = { "material", "cloth" },
}

item {
	id = "wire", name = "Copper Wire", cat = "Material", w = 1, h = 1, weight = 0.2,
	rarity = 2, stack = 10,
	desc = "Conductive, and worth stealing.",
	tags = { "material", "electronic" },
}

item {
	id = "chemicals", name = "Chemicals", cat = "Material", w = 2, h = 2, weight = 0.9,
	rarity = 3, stack = 6,
	desc = "Industrial solvent. Handle away from flame.",
	tags = { "material", "chemical", "flammable" },
}

item {
	id = "explosive_charge", name = "Breaching Charge", cat = "Material", w = 2, h = 2, weight = 1.2,
	rarity = 4, stack = 2,
	desc = "Opens military doors. Or a very unlucky mutant.",
	tags = { "material", "explosive" },
}

item {
	id = "duct_tape", name = "Duct Tape", cat = "Tool", w = 1, h = 1, weight = 0.12,
	rarity = 1, stack = 6,
	desc = "Holds the world together, one wrap at a time.",
	tags = { "material", "repair" },
}

-- ---------------------------------------------------------- clothing/armour
item {
	id = "jacket", name = "Padded Jacket", cat = "Clothing", w = 3, h = 3, weight = 1.8,
	rarity = 2, dur = 100, equip = "Armor", armor = { torso = 0.18, arms = 0.08, cold = 0.4 },
	desc = "Warm and thick, stops a claw or two, does nothing against a bullet.",
	tags = { "clothing", "armor", "warm" },
}

item {
	id = "plate_carrier", name = "Plate Carrier", cat = "Armor", w = 3, h = 3, weight = 6.4,
	rarity = 4, dur = 140, equip = "Armor", armor = { torso = 0.45, cold = 0.1 },
	desc = "Ceramic plates, military issue. Heavy, and worth every kilogram.",
	tags = { "armor", "military" },
}

item {
	id = "hazmat", name = "Hazmat Suit", cat = "Armor", w = 3, h = 4, weight = 3.2,
	rarity = 4, dur = 90, equip = "Armor", armor = { torso = 0.05, radiation = 0.65 },
	desc = "Sealed against fallout. Tears easily on wire and teeth.",
	tags = { "armor", "radiation", "suit" },
}

item {
	id = "boots", name = "Reinforced Boots", cat = "Clothing", w = 2, h = 2, weight = 1.1,
	rarity = 2, dur = 120, equip = "Utility1", armor = { legs = 0.12, noise = -0.25 },
	desc = "Quieter steps, safer ankles.",
	tags = { "clothing", "boots" },
}

item {
	id = "backpack", name = "Field Backpack", cat = "Clothing", w = 3, h = 3, weight = 1.4,
	rarity = 2, equip = "Backpack",
	desc = "Canvas and buckles. Additional storage rows while equipped.",
	tags = { "clothing", "storage" },
}

item {
	id = "gas_mask", name = "Respirator", cat = "Armor", w = 2, h = 2, weight = 0.7,
	rarity = 3, dur = 100, equip = "Head", armor = { radiation = 0.35, infection = 0.3 },
	desc = "Filters the air. Muffles your breathing, not theirs.",
	tags = { "armor", "mask", "radiation" },
}

item {
	id = "glowstick", name = "Glowstick", cat = "Tool", w = 1, h = 2, weight = 0.06,
	rarity = 1, stack = 5, equip = "Utility2",
	desc = "Cold green light that mutants seem to ignore. Mark a path home.",
	use = { kind = "light", color = Color3.fromRGB(120, 230, 150) },
	tags = { "light", "marker" },
}

item {
	id = "flare", name = "Road Flare", cat = "Tool", w = 1, h = 2, weight = 0.15,
	rarity = 2, stack = 3, equip = "Utility2",
	desc = "Burns red and drives the smaller things away for a while.",
	use = { kind = "flare" },
	tags = { "light", "fire", "defensive" },
}

item {
	id = "blueprint_water", name = "Blueprint: Field Filter", cat = "Quest", w = 2, h = 2, weight = 0.05,
	rarity = 3,
	desc = "Hand-drawn plans. Unlocks the field filter recipe permanently.",
	use = { kind = "blueprint", recipe = "field_filter" },
	tags = { "blueprint" },
}

item {
	id = "blueprint_crossbow", name = "Blueprint: Crossbow", cat = "Quest", w = 2, h = 2, weight = 0.05,
	rarity = 4,
	desc = "Springs, tension, and trigonometry. Unlocks the crossbow recipe.",
	use = { kind = "blueprint", recipe = "crossbow" },
	tags = { "blueprint" },
}

item {
	id = "document", name = "Document", cat = "Quest", w = 2, h = 2, weight = 0.03,
	rarity = 2,
	desc = "Paper that survived the collapse. Someone should read it.",
	tags = { "quest", "document" },
}

return Items
