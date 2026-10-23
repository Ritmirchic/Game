--!strict
--[[
	LootTables — weighted loot per location type.

	Each entry: { id = itemId, weight = n, min = n, max = n, chance = 0..1 }
	`chance` is evaluated per roll; when omitted the entry always participates in
	the weighted pick. Rolls are performed server side only.
]]

local Loot = {}

local function table_(name, rolls, entries, extra)
	Loot[name] = { name = name, rolls = rolls, entries = entries, guaranteed = (extra or {}).guaranteed or {} }
end

table_("residential", { 1, 3 }, {
	{ id = "canned_beans", weight = 22, min = 1, max = 1 },
	{ id = "crackers", weight = 18, min = 1, max = 2 },
	{ id = "energy_bar", weight = 16, min = 1, max = 2 },
	{ id = "water_bottle", weight = 20, min = 1, max = 1 },
	{ id = "dirty_water", weight = 14, min = 1, max = 1 },
	{ id = "rags", weight = 20, min = 1, max = 3 },
	{ id = "cloth", weight = 16, min = 1, max = 3 },
	{ id = "bandage", weight = 12, min = 1, max = 2 },
	{ id = "painkillers", weight = 6 },
	{ id = "battery", weight = 10 },
	{ id = "duct_tape", weight = 8 },
	{ id = "knife", weight = 5 },
	{ id = "flashlight", weight = 6 },
	{ id = "wood", weight = 10, min = 1, max = 2 },
	{ id = "scrap_metal", weight = 10, min = 1, max = 2 },
	{ id = "jacket", weight = 4 },
	{ id = "boots", weight = 4 },
	{ id = "backpack", weight = 4 },
	{ id = "map_fragment", weight = 3 },
	{ id = "document", weight = 3 },
	{ id = "radio", weight = 2 },
	{ id = "lockpick", weight = 2 },
}, { guaranteed = { { id = "rags", chance = 0.35 } } })

table_("kitchen", { 1, 2 }, {
	{ id = "canned_fish", weight = 24 },
	{ id = "canned_beans", weight = 22 },
	{ id = "water_bottle", weight = 24 },
	{ id = "crackers", weight = 16 },
	{ id = "knife", weight = 8 },
	{ id = "rags", weight = 18, min = 1, max = 2 },
	{ id = "duct_tape", weight = 6 },
	{ id = "chemicals", weight = 3 },
})

table_("medical", { 1, 3 }, {
	{ id = "bandage", weight = 34, min = 1, max = 3 },
	{ id = "painkillers", weight = 22 },
	{ id = "antibiotics", weight = 12 },
	{ id = "splint", weight = 10 },
	{ id = "medkit", weight = 5 },
	{ id = "antirade", weight = 6 },
	{ id = "stimulant", weight = 5 },
	{ id = "rags", weight = 20, min = 1, max = 2 },
	{ id = "chemicals", weight = 6 },
	{ id = "ampoule_mil", weight = 1, chance = 0.12 },
})

table_("tools", { 1, 2 }, {
	{ id = "duct_tape", weight = 22 },
	{ id = "scrap_metal", weight = 24, min = 1, max = 3 },
	{ id = "wood", weight = 18, min = 1, max = 3 },
	{ id = "wire", weight = 14 },
	{ id = "toolkit", weight = 10 },
	{ id = "multimeter", weight = 7 },
	{ id = "crowbar", weight = 9 },
	{ id = "hatchet", weight = 6 },
	{ id = "pipe_wrench", weight = 6 },
	{ id = "baseball_bat", weight = 8 },
	{ id = "battery", weight = 14 },
	{ id = "flashlight", weight = 8 },
	{ id = "headlamp", weight = 4 },
	{ id = "fuel_can", weight = 5 },
	{ id = "lockpick", weight = 4 },
	{ id = "blueprint_water", weight = 2, chance = 0.4 },
})

table_("food", { 2, 4 }, {
	{ id = "canned_beans", weight = 26 },
	{ id = "canned_fish", weight = 20 },
	{ id = "crackers", weight = 22 },
	{ id = "energy_bar", weight = 20 },
	{ id = "water_bottle", weight = 26 },
	{ id = "dirty_water", weight = 14 },
	{ id = "purified_water", weight = 8 },
	{ id = "duct_tape", weight = 6 },
	{ id = "rags", weight = 12 },
})

table_("fuel", { 1, 2 }, {
	{ id = "fuel_can", weight = 30 },
	{ id = "scrap_metal", weight = 22 },
	{ id = "duct_tape", weight = 14 },
	{ id = "chemicals", weight = 10 },
	{ id = "battery", weight = 12 },
	{ id = "crackers", weight = 10 },
	{ id = "pistol", weight = 3, chance = 0.3 },
	{ id = "ammo_9mm", weight = 8, min = 6, max = 14 },
})

table_("camp", { 2, 3 }, {
	{ id = "canned_beans", weight = 20 },
	{ id = "water_bottle", weight = 20 },
	{ id = "rags", weight = 18, min = 1, max = 3 },
	{ id = "bandage", weight = 16 },
	{ id = "wood", weight = 18, min = 2, max = 4 },
	{ id = "duct_tape", weight = 12 },
	{ id = "knife", weight = 8 },
	{ id = "hatchet", weight = 8 },
	{ id = "cooked_meat", weight = 10, chance = 0.5 },
	{ id = "raw_meat", weight = 10 },
	{ id = "tent_placeholder", weight = 0 },
	{ id = "jacket", weight = 6 },
	{ id = "map_fragment", weight = 6 },
	{ id = "document", weight = 6 },
	{ id = "blueprint_crossbow", weight = 2, chance = 0.3 },
})

table_("industrial", { 2, 3 }, {
	{ id = "scrap_metal", weight = 30, min = 1, max = 4 },
	{ id = "wire", weight = 20 },
	{ id = "duct_tape", weight = 18 },
	{ id = "toolkit", weight = 12 },
	{ id = "fuel_can", weight = 10 },
	{ id = "chemicals", weight = 12 },
	{ id = "crowbar", weight = 10 },
	{ id = "battery", weight = 16 },
	{ id = "explosive_charge", weight = 3 },
	{ id = "gas_mask", weight = 5 },
	{ id = "blueprint_crossbow", weight = 2, chance = 0.25 },
})

table_("chemical", { 2, 3 }, {
	{ id = "chemicals", weight = 34 },
	{ id = "antirade", weight = 14 },
	{ id = "hazmat", weight = 6 },
	{ id = "gas_mask", weight = 10 },
	{ id = "fuel_can", weight = 12 },
	{ id = "scrap_metal", weight = 18, min = 1, max = 3 },
	{ id = "antibiotics", weight = 8 },
	{ id = "explosive_charge", weight = 4 },
})

table_("police", { 2, 3 }, {
	{ id = "ammo_9mm", weight = 24, min = 8, max = 20 },
	{ id = "ammo_shell", weight = 10, min = 4, max = 10 },
	{ id = "pistol", weight = 10 },
	{ id = "bandage", weight = 16 },
	{ id = "medkit", weight = 6 },
	{ id = "plate_carrier", weight = 4 },
	{ id = "battery", weight = 12 },
	{ id = "flashlight", weight = 10 },
	{ id = "document", weight = 8 },
	{ id = "lockpick", weight = 8 },
	{ id = "duct_tape", weight = 10 },
	{ id = "shotgun", weight = 4, chance = 0.4 },
})

table_("weapons", { 1, 2 }, {
	{ id = "ammo_762", weight = 22, min = 10, max = 30 },
	{ id = "ammo_9mm", weight = 22, min = 10, max = 24 },
	{ id = "ammo_shell", weight = 16, min = 4, max = 12 },
	{ id = "bolt", weight = 14, min = 4, max = 10 },
	{ id = "pistol", weight = 10 },
	{ id = "rifle", weight = 5 },
	{ id = "shotgun", weight = 7 },
	{ id = "bolt_rifle", weight = 4 },
	{ id = "crossbow", weight = 5 },
	{ id = "plate_carrier", weight = 6 },
	{ id = "molotov", weight = 8 },
})

table_("military", { 2, 3 }, {
	{ id = "ammo_762", weight = 24, min = 12, max = 30 },
	{ id = "ammo_9mm", weight = 18, min = 12, max = 24 },
	{ id = "ammo_shell", weight = 14, min = 6, max = 14 },
	{ id = "plate_carrier", weight = 10 },
	{ id = "rifle", weight = 8 },
	{ id = "shotgun", weight = 8 },
	{ id = "antirade", weight = 12 },
	{ id = "medkit", weight = 10 },
	{ id = "gas_mask", weight = 12 },
	{ id = "explosive_charge", weight = 8 },
	{ id = "document", weight = 10 },
	{ id = "multimeter", weight = 8 },
	{ id = "stimulant", weight = 8 },
})

table_("military_high", { 2, 4 }, {
	{ id = "rifle", weight = 18 },
	{ id = "bolt_rifle", weight = 14 },
	{ id = "plate_carrier", weight = 16 },
	{ id = "medkit", weight = 14 },
	{ id = "antirade", weight = 16 },
	{ id = "ammo_762", weight = 24, min = 20, max = 40 },
	{ id = "explosive_charge", weight = 10 },
	{ id = "lab_keycard", weight = 1, chance = 0.25 },
	{ id = "ampoule_mil", weight = 1, chance = 0.2 },
	{ id = "document", weight = 12 },
	{ id = "hazmat", weight = 8 },
})

table_("subway", { 1, 3 }, {
	{ id = "rags", weight = 22, min = 1, max = 3 },
	{ id = "scrap_metal", weight = 18, min = 1, max = 2 },
	{ id = "bandage", weight = 14 },
	{ id = "battery", weight = 16 },
	{ id = "duct_tape", weight = 12 },
	{ id = "flashlight", weight = 10 },
	{ id = "purified_water", weight = 10 },
	{ id = "crackers", weight = 14 },
	{ id = "document", weight = 8 },
	{ id = "glowstick", weight = 12 },
	{ id = "bolt", weight = 8, min = 2, max = 6 },
	{ id = "crowbar", weight = 6 },
})

table_("electronics", { 1, 3 }, {
	{ id = "wir" .. "e", weight = 22 },
	{ id = "battery", weight = 22, min = 1, max = 3 },
	{ id = "multimeter", weight = 16 },
	{ id = "radio", weight = 10 },
	{ id = "scrap_metal", weight = 18 },
	{ id = "duct_tape", weight = 12 },
	{ id = "blueprint_crossbow", weight = 2, chance = 0.3 },
})

table_("rare", { 1, 2 }, {
	{ id = "core_sample", weight = 4, chance = 0.3 },
	{ id = "antirade", weight = 18 },
	{ id = "medkit", weight = 16 },
	{ id = "rifle", weight = 10 },
	{ id = "ammo_762", weight = 20, min = 15, max = 30 },
	{ id = "explosive_charge", weight = 12 },
	{ id = "hazmat", weight = 10 },
	{ id = "mutant_gland", weight = 22 },
	{ id = "document", weight = 14 },
	{ id = "stimulant", weight = 12 },
})

table_("shelter", { 2, 3 }, {
	{ id = "bandage", weight = 22 },
	{ id = "canned_beans", weight = 20 },
	{ id = "water_bottle", weight = 22 },
	{ id = "flashlight", weight = 14 },
	{ id = "battery", weight = 18, min = 1, max = 2 },
	{ id = "toolkit", weight = 10 },
	{ id = "rags", weight = 16 },
	{ id = "document", weight = 12 },
	{ id = "radio", weight = 8 },
	{ id = "painkillers", weight = 12 },
	{ id = "blueprint_water", weight = 6, chance = 0.4 },
	{ id = "map_fragment", weight = 8 },
})

-- Deleted placeholder keys that must never appear in loot.
Loot.residential.entries = (function()
	local out = {}
	for _, e in ipairs(Loot.residential.entries) do
		if e.weight > 0 then table.insert(out, e) end
	end
	return out
end)()

Loot.camp.entries = (function()
	local out = {}
	for _, e in ipairs(Loot.camp.entries) do
		if e.weight > 0 then table.insert(out, e) end
	end
	return out
end)()

return Loot
