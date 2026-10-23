--!strict
--[[
	BuildDefinitions — base building pieces.

	size        studs (x, y, z)
	cost        { [itemId] = count }
	health      structure hit points (attacks by mutants damage structures)
	grid        snap size in studs (0 = free placement)
	supports    requires ground/support contact
]]

local Build = {}

local function piece(t)
	t.grid = t.grid or 4
	t.health = t.health or 120
	t.size = t.size or Vector3.new(4, 4, 4)
	t.cost = t.cost or {}
	Build[t.id] = t
	return t
end

piece {
	id = "wall_wood", name = "Timber Wall", category = "Structure", size = Vector3.new(12, 12, 1),
	cost = { wood = 4 }, health = 140, grid = 4,
	desc = "Planks nailed into a barricade. Cheap, and it burns.",
}

piece {
	id = "wall_metal", name = "Scrap Wall", category = "Structure", size = Vector3.new(12, 12, 1),
	cost = { scrap_metal = 5, duct_tape = 1 }, health = 320, grid = 4,
	desc = "Welded sheet metal. Slows a Brute, barely.",
}

piece {
	id = "floor_wood", name = "Floor Panel", category = "Structure", size = Vector3.new(12, 1, 12),
	cost = { wood = 4 }, health = 120, grid = 4, horizontal = true,
}

piece {
	id = "floor_metal", name = "Steel Deck", category = "Structure", size = Vector3.new(12, 1, 12),
	cost = { scrap_metal = 5 }, health = 260, grid = 4, horizontal = true,
}

piece {
	id = "roof", name = "Roof Panel", category = "Structure", size = Vector3.new(12, 1, 12),
	cost = { wood = 3, scrap_metal = 2 }, health = 150, grid = 4, horizontal = true,
}

piece {
	id = "door_wood", name = "Door", category = "Structure", size = Vector3.new(8, 12, 1),
	cost = { wood = 4, scrap_metal = 2 }, health = 160, grid = 4, interactive = "door",
}

piece {
	id = "barricade", name = "Barricade", category = "Defense", size = Vector3.new(10, 6, 3),
	cost = { wood = 3, scrap_metal = 2 }, health = 180, grid = 2,
	desc = "Angled boards that break incoming momentum.",
}

piece {
	id = "spikes", name = "Spike Trap", category = "Defense", size = Vector3.new(8, 2, 8),
	cost = { wood = 2, scrap_metal = 3 }, health = 90, grid = 4, trap = { damage = 26, cooldown = 4 },
	desc = "Sharpened rebar. Mutants that step on it regret it.",
}

piece {
	id = "storage_box", name = "Storage Box", category = "Utility", size = Vector3.new(6, 4, 5),
	cost = { wood = 4, duct_tape = 1 }, health = 100, grid = 2, interactive = "container",
	desc = "Shared storage for the whole group.",
}

piece {
	id = "workbench_build", name = "Workbench", category = "Utility", size = Vector3.new(8, 5, 4),
	cost = { wood = 6, scrap_metal = 4 }, health = 140, grid = 4, interactive = "workbench",
	desc = "A place to build the things that keep you alive.",
}

piece {
	id = "campfire_build", name = "Campfire", category = "Utility", size = Vector3.new(6, 3, 6),
	cost = { wood = 4, scrap_metal = 1 }, health = 60, grid = 4, interactive = "campfire",
	desc = "Warmth, cooking, and a light that keeps the small things away.",
}

piece {
	id = "generator_build", name = "Generator", category = "Utility", size = Vector3.new(8, 6, 6),
	cost = { scrap_metal = 8, wire = 4, fuel_can = 1 }, health = 200, grid = 4,
	interactive = "generator", desc = "Powers lights and the radio relay. Loud.",
}

piece {
	id = "lamp_build", name = "Work Light", category = "Utility", size = Vector3.new(3, 3, 3),
	cost = { scrap_metal = 2, wire = 2, battery = 1 }, health = 40, grid = 2,
	interactive = "light", desc = "A pool of electric light in a very dark world.",
}

piece {
	id = "radio_relay", name = "Radio Relay", category = "Utility", size = Vector3.new(6, 18, 6),
	cost = { scrap_metal = 10, wire = 6, multimeter = 1, battery = 2 }, health = 220,
	grid = 4, interactive = "relay", quest = "quest_radio",
	desc = "The antenna that reaches the signal. This is what the story hangs on.",
}

return Build
