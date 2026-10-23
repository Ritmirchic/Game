--!strict
--[[
	QuestDefinitions — main story chapters and side contracts.

	Objective types:
	  collect  { item = id, count = n }   have n of an item in the group inventory
	  kill     { enemy = id, count = n }
	  reach    { region = name } or { x = , z = , radius = }
	  interact { kind = "Radio" | "LabDoor" | "Bed" | ... , target = id }
	  craft    { recipe = id }
	  build    { piece = id }
	  talk     { npc = id }
]]

local Quests = {}

local function quest(t)
	t.objectives = t.objectives or {}
	t.rewards = t.rewards or {}
	t.side = t.side or false
	Quests[t.id] = t
	return t
end

-- ------------------------------------------------------------------- main
quest {
	id = "quest_awaken", chapter = 1, name = "Last Signal", side = false,
	brief = "Something is wrong with the radio in the shelter. Something is wrong with everything.",
	intro = "You wake on a concrete floor. The emergency lights are dying. In the corner, a radio hisses between stations.",
	objectives = {
		{ id = "o1", type = "interact", kind = "Radio", text = "Tune the shelter radio", count = 1 },
		{ id = "o2", type = "collect", item = "bandage", count = 2, text = "Find medical supplies (2 bandages)" },
		{ id = "o3", type = "reach", region = "Suburbs", text = "Climb to the surface of the Dead Suburbs" },
	},
	rewards = { xp = 120, items = { { id = "bandage", count = 2 }, { id = "water_bottle", count = 1 } }, unlock = { "quest_suburb_house" } },
	completion = "The signal repeats a single phrase, over and over: 'If anyone hears this, the relay in the city is still standing.'",
}

quest {
	id = "quest_radio", chapter = 2, name = "The Relay", side = false,
	brief = "Reach the radio relay on the rooftops of the Forgotten City and power it.",
	intro = "A transmitter strong enough to answer is on the roof of an old relay station, east of the city centre.",
	objectives = {
		{ id = "o1", type = "reach", region = "City", text = "Enter the Forgotten City" },
		{ id = "o2", type = "interact", kind = "RadioTower", text = "Inspect the relay tower" },
		{ id = "o3", type = "build", piece = "radio_relay", text = "Build and power a radio relay" },
		{ id = "o4", type = "interact", kind = "Radio", text = "Answer the signal" },
	},
	rewards = { xp = 300, items = { { id = "ammo_9mm", count = 12 }, { id = "medkit", count = 1 } }, unlock = { "quest_pharmacy" } },
	completion = "The voice on the other end is close. It says the hospital is still stocked — and that something lives in it now.",
}

quest {
	id = "quest_pharmacy", chapter = 3, name = "Ward Nine", side = false,
	brief = "Search the hospital for the sealed military ampoule and the medical records.",
	intro = "The voice gave a name: Doctor Alena Voronova. Her notes are in the hospital, with the last of the military ampoules.",
	objectives = {
		{ id = "o1", type = "reach", region = "City", text = "Return to the Forgotten City" },
		{ id = "o2", type = "collect", item = "ampoule_mil", count = 1, text = "Recover the military ampoule" },
		{ id = "o3", type = "collect", item = "ampoule_doc", count = 0, text = "Read Voronova's hospital log" },
		{ id = "o4", type = "interact", kind = "Document", target = "doc_hospital_log", text = "Read the hospital records" },
	},
	rewards = { xp = 320, items = { { id = "antibiotics", count = 2 }, { id = "gas_mask", count = 1 } }, unlock = { "quest_industrial" } },
	completion = "Voronova was not researching the catastrophe. She was documenting what it produced. The next site is the factory.",
}

quest {
	id = "quest_industrial", chapter = 4, name = "Black Water", side = false,
	brief = "Investigate the chemical plant's cooling outflow and recover the lab keycard.",
	intro = "The industrial zone has been leaking something into the ground for a decade. Someone kept records anyway.",
	objectives = {
		{ id = "o1", type = "reach", region = "Industrial", text = "Reach the Industrial Zone" },
		{ id = "o2", type = "interact", kind = "Document", target = "doc_industrial_samples", text = "Find the sampling report" },
		{ id = "o3", type = "collect", item = "chemicals", count = 4, text = "Collect chemical samples (4)" },
		{ id = "o4", type = "kill", enemy = "brute", count = 1, text = "Put down the Brute in the cooling yard" },
	},
	rewards = { xp = 400, items = { { id = "explosive_charge", count = 2 }, { id = "hazmat", count = 1 } }, unlock = { "quest_lab" } },
	completion = "The samples match the hospital cultures. Both came from the same place: a military laboratory under the exclusion zone.",
}

quest {
	id = "quest_lab", chapter = 5, name = "The Exclusion Zone", side = false,
	brief = "Enter the military underground laboratory and find Voronova's research team.",
	intro = "The military zone is still fenced. The laboratory under it is still running on emergency power.",
	objectives = {
		{ id = "o1", type = "reach", region = "Military", text = "Cross into the Exclusion Zone" },
		{ id = "o2", type = "collect", item = "lab_keycard", count = 1, text = "Recover a level 4 keycard" },
		{ id = "o3", type = "interact", kind = "LabDoor", text = "Open the laboratory blast door" },
		{ id = "o4", type = "interact", kind = "Document", target = "doc_lab_terminal", text = "Access the lab terminal" },
	},
	rewards = { xp = 520, items = { { id = "rifle", count = 1 }, { id = "ammo_762", count = 30 }, { id = "antirade", count = 2 } }, unlock = { "quest_wasteland" } },
	completion = "The team did not die in the collapse. They walked into the wasteland carrying a sample and never came back.",
}

quest {
	id = "quest_wasteland", chapter = 6, name = "Ashes of Humanity", side = false,
	brief = "Reach the reactor ruin in the Red Wasteland, recover the core sample, and decide what to do with the signal.",
	intro = "The last transmission came from the reactor. Whoever sent it is done hiding.",
	objectives = {
		{ id = "o1", type = "reach", region = "Wasteland", text = "Enter the Red Wasteland" },
		{ id = "o2", type = "collect", item = "core_sample", count = 1, text = "Recover the reactor core sample" },
		{ id = "o3", type = "interact", kind = "Document", target = "doc_reactor_final", text = "Read the final log" },
		{ id = "o4", type = "survive", seconds = 180, text = "Survive the wasteland night (3 minutes)" },
	},
	rewards = { xp = 900, items = { { id = "medkit", count = 2 }, { id = "antirade", count = 3 } } },
	completion = "The sample is a piece of the thing that ended the world. You are still holding it when the sun comes up.",
}

-- ------------------------------------------------------------------- side
quest {
	id = "quest_suburb_house", chapter = 0, name = "Somebody's Home", side = true,
	brief = "Search the abandoned houses of the suburbs.",
	intro = "Nobody has lived here for a decade, but somebody left in a hurry.",
	objectives = {
		{ id = "o1", type = "collect", item = "canned_beans", count = 3, text = "Gather food (3 cans)" },
		{ id = "o2", type = "reach", region = "Suburbs", text = "Explore the suburbs" },
	},
	rewards = { xp = 80, items = { { id = "energy_bar", count = 2 } } },
}

quest {
	id = "quest_water", chapter = 0, name = "Clean Water", side = true,
	brief = "Build a field filter and purify water.",
	intro = "The water in this region will kill you slowly. Fix that.",
	objectives = {
		{ id = "o1", type = "craft", recipe = "field_filter", text = "Craft a field filter" },
		{ id = "o2", type = "collect", item = "purified_water", count = 1, text = "Purify a bottle of water" },
	},
	rewards = { xp = 110, items = { { id = "water_bottle", count = 2 } } },
}

quest {
	id = "quest_generator", chapter = 0, name = "Lights Out", side = true,
	brief = "Restore power to a building using a generator.",
	intro = "Half the industrial zone is dark because nobody replaced a fuse.",
	objectives = {
		{ id = "o1", type = "craft", recipe = "repair_kit", text = "Craft a repair kit" },
		{ id = "o2", type = "build", piece = "generator_build", text = "Install a generator" },
		{ id = "o3", type = "build", piece = "lamp_build", text = "Install a work light" },
	},
	rewards = { xp = 160, items = { { id = "battery", count = 3 }, { id = "headlamp", count = 1 } } },
}

quest {
	id = "quest_hunter", chapter = 0, name = "The Hunt", side = true,
	brief = "Thin the mutant population around the forest.",
	intro = "The forest is louder than it used to be. Something is feeding them.",
	objectives = {
		{ id = "o1", type = "kill", enemy = "hollow", count = 5, text = "Kill 5 Hollow" },
		{ id = "o2", type = "kill", enemy = "crawler", count = 3, text = "Kill 3 Crawlers" },
		{ id = "o3", type = "collect", item = "mutant_gland", count = 2, text = "Collect 2 mutant glands" },
	},
	rewards = { xp = 220, items = { { id = "shotgun", count = 1 }, { id = "ammo_shell", count = 8 } } },
}

quest {
	id = "quest_shelter_defense", chapter = 0, name = "Hold the Line", side = true,
	brief = "Fortify and defend a shelter through the night.",
	intro = "When the sun goes down, the things outside get brave.",
	objectives = {
		{ id = "o1", type = "build", piece = "barricade", text = "Build a barricade" },
		{ id = "o2", type = "build", piece = "spikes", text = "Place a spike trap" },
		{ id = "o3", type = "kill", enemy = "stalker", count = 1, text = "Kill a Stalker" },
	},
	rewards = { xp = 260, items = { { id = "plate_carrier", count = 1 } } },
}

quest {
	id = "quest_pale", chapter = 0, name = "The Pale One", side = true,
	brief = "Something white moves at night in the wasteland. Find it.",
	intro = "Survivors on the radio refuse to say its name out loud.",
	objectives = {
		{ id = "o1", type = "kill", enemy = "pale_one", count = 1, text = "Kill the Pale One" },
	},
	rewards = { xp = 600, items = { { id = "medkit", count = 2 }, { id = "core_sample", count = 0 } } },
}

function Quests.mainLine()
	local out = {}
	for _, q in pairs(Quests) do
		if type(q) == "table" and not q.side and q.chapter > 0 then
			table.insert(out, q)
		end
	end
	table.sort(out, function(a, b) return a.chapter < b.chapter end)
	return out
end

function Quests.sideQuests()
	local out = {}
	for _, q in pairs(Quests) do
		if type(q) == "table" and q.side then
			table.insert(out, q)
		end
	end
	table.sort(out, function(a, b) return a.id < b.id end)
	return out
end

return Quests
