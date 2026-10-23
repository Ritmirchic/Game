--!strict
--[[
	Recipes — crafting definitions.

	station    nil (anywhere) | "Workbench" | "Campfire" | "Generator"
	ingredients { [itemId] = count }
	output     { id = itemId, count = n }
	time       seconds of crafting
	blueprint  requires the blueprint item to be known
	unlock     quest id that unlocks this recipe
]]

local Recipes = {}

local function recipe(t)
	t.time = t.time or 2
	t.ingredients = t.ingredients or {}
	Recipes[t.id] = t
	return t
end

recipe {
	id = "bandage", name = "Bandage", station = nil, time = 2.5,
	ingredients = { rags = 2, duct_tape = 1 }, output = { id = "bandage", count = 1 },
	desc = "Clean cloth pressed into a wound.",
}

recipe {
	id = "rags", name = "Tear Cloth", station = nil, time = 1.2,
	ingredients = { cloth = 1 }, output = { id = "rags", count = 3 },
	desc = "Strip fabric into usable strips.",
}

recipe {
	id = "splint", name = "Splint", station = nil, time = 4,
	ingredients = { wood = 2, rags = 2 }, output = { id = "splint", count = 1 },
	desc = "Straighten a break well enough to walk.",
}

recipe {
	id = "torch", name = "Fire Torch", station = "Campfire", time = 3,
	ingredients = { wood = 1, rags = 1, fuel_can = 1 }, output = { id = "glowstick", count = 1 },
	desc = "A steady flame that mutants dislike.",
}

recipe {
	id = "purified_water", name = "Boil Water", station = "Campfire", time = 5,
	ingredients = { dirty_water = 1 }, output = { id = "purified_water", count = 1 },
	desc = "Kill whatever is swimming in it.",
}

recipe {
	id = "cooked_meat", name = "Cook Meat", station = "Campfire", time = 6,
	ingredients = { raw_meat = 1, wood = 1 }, output = { id = "cooked_meat", count = 1 },
	desc = "Heat until the colour changes. Then a little longer.",
}

recipe {
	id = "field_filter", name = "Field Filter", station = "Workbench", time = 12,
	ingredients = { scrap_metal = 4, cloth = 3, duct_tape = 2 }, output = { id = "water_filter", count = 1 },
	blueprint = "blueprint_water",
	desc = "Charcoal and cloth. Makes the water survivable.",
}

recipe {
	id = "ammo_9mm", name = "Press 9mm Rounds", station = "Workbench", time = 7,
	ingredients = { scrap_metal = 2, chemicals = 1 }, output = { id = "ammo_9mm", count = 6 },
	desc = "Reloading brass by hand, badly, but it fires.",
}

recipe {
	id = "ammo_shell", name = "Load Shells", station = "Workbench", time = 9,
	ingredients = { scrap_metal = 3, chemicals = 1, cloth = 1 }, output = { id = "ammo_shell", count = 4 },
}

recipe {
	id = "ammo_762", name = "Load 7.62", station = "Workbench", time = 11,
	ingredients = { scrap_metal = 3, chemicals = 2 }, output = { id = "ammo_762", count = 8 },
}

recipe {
	id = "bolt", name = "Fletch Bolts", station = "Workbench", time = 4,
	ingredients = { wood = 2, scrap_metal = 1 }, output = { id = "bolt", count = 3 },
}

recipe {
	id = "crossbow", name = "Crossbow", station = "Workbench", time = 22,
	ingredients = { wood = 6, scrap_metal = 4, wire = 2, duct_tape = 2 },
	output = { id = "crossbow", count = 1 },
	blueprint = "blueprint_crossbow",
	desc = "Silent, retrievable, and slow to make.",
}

recipe {
	id = "knife", name = "Scrap Knife", station = "Workbench", time = 8,
	ingredients = { scrap_metal = 3, wood = 1, duct_tape = 1 }, output = { id = "knife", count = 1 },
}

recipe {
	id = "baseball_bat", name = "Wrapped Bat", station = "Workbench", time = 10,
	ingredients = { wood = 4, wire = 2, duct_tape = 1 }, output = { id = "baseball_bat", count = 1 },
}

recipe {
	id = "molotov", name = "Molotov", station = "Workbench", time = 6,
	ingredients = { fuel_can = 1, rags = 2, water_bottle = 1 }, output = { id = "molotov", count = 1 },
}

recipe {
	id = "repair_kit", name = "Repair Kit", station = "Workbench", time = 8,
	ingredients = { scrap_metal = 3, duct_tape = 2, tools = 0 }, output = { id = "toolkit", count = 1 },
}

recipe {
	id = "antirade", name = "Anti-Rad Dose", station = "Workbench", time = 14,
	ingredients = { chemicals = 3, bandage = 1, medic_placeholder = 0 }, output = { id = "antirade", count = 1 },
	unlock = "quest_pharmacy",
}

recipe {
	id = "splint_cloth", name = "Field Bandage Roll", station = "Workbench", time = 6,
	ingredients = { rags = 4, cloth = 2 }, output = { id = "bandage", count = 3 },
}

recipe {
	id = "flare", name = "Improvised Flare", station = "Campfire", time = 5,
	ingredients = { chemicals = 1, cloth = 1, wood = 1 }, output = { id = "flare", count = 1 },
}

recipe {
	id = "glowstick", name = "Chem Light", station = "Workbench", time = 3,
	ingredients = { chemicals = 1, battery = 1 }, output = { id = "glowstick", count = 2 },
}

recipe {
	id = "gas_mask", name = "Respirator", station = "Workbench", time = 16,
	ingredients = { cloth = 4, scrap_metal = 2, duct_tape = 3 }, output = { id = "gas_mask", count = 1 },
	unlock = "quest_pharmacy",
}

recipe {
	id = "headlamp", name = "Headlamp", station = "Workbench", time = 12,
	ingredients = { flashlight = 1, battery = 2, duct_tape = 1 }, output = { id = "headlamp", count = 1 },
}

recipe {
	id = "lockpick", name = "Lockpicks", station = "Workbench", time = 5,
	ingredients = { scrap_metal = 2, wire = 1 }, output = { id = "lockpick", count = 1 },
}

recipe {
	id = "explosive_charge", name = "Breaching Charge", station = "Workbench", time = 20,
	ingredients = { chemicals = 2, scrap_metal = 3, wire = 2, fuel_can = 1 },
	output = { id = "explosive_charge", count = 1 },
	unlock = "quest_industrial",
}

-- remove the accidental placeholder ingredients (kept explicit and validated)
for id, r in pairs(Recipes) do
	local clean = {}
	for k, v in pairs(r.ingredients) do
		if k ~= "tools" and k ~= "medic_placeholder" then
			clean[k] = v
		end
	end
	if id == "repair_kit" then
		clean = { scrap_metal = 3, duct_tape = 2 }
	end
	if id == "antirade" then
		clean = { chemicals = 3, bandage = 1 }
	end
	r.ingredients = clean
end

function Recipes.byStation(station)
	local out = {}
	for _, r in pairs(Recipes) do
		if type(r) == "table" and (r.station or "") == (station or "") then
			table.insert(out, r)
		end
	end
	table.sort(out, function(a, b) return a.name < b.name end)
	return out
end

return Recipes
