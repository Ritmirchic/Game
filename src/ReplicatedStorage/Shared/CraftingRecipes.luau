--!strict
--[[
    ASHES OF HUMANITY: LAST SIGNAL
    ReplicatedStorage.Shared.CraftingRecipes
    24 survival, medical, cooking, utility, weapon, ammunition, and electronic crafting recipes.
]]

local CraftingRecipes = {}

export type Ingredient = {
	ItemId: string,
	Count: number,
}

export type RecipeDef = {
	Id: string,
	Name: string,
	Category: "Medical" | "Survival" | "Tools" | "Weapons" | "Ammunition" | "Electronics",
	Station: "Hand" | "Campfire" | "Workbench",
	CraftSeconds: number,
	RequiresBlueprint: boolean,
	Ingredients: { Ingredient },
	OutputItemId: string,
	OutputCount: number,
}

CraftingRecipes.RECIPES = {
	-- MEDICAL
	CraftRagBandage = {
		Id = "CraftRagBandage",
		Name = "Improvised Cloth Bandage",
		Category = "Medical",
		Station = "Hand",
		CraftSeconds = 2.0,
		RequiresBlueprint = false,
		Ingredients = {
			{ ItemId = "ClothScrap", Count = 2 },
		},
		OutputItemId = "RagBandage",
		OutputCount = 1,
	},
	CraftSterileBandage = {
		Id = "CraftSterileBandage",
		Name = "Sterile Gauze Bandage",
		Category = "Medical",
		Station = "Hand",
		CraftSeconds = 3.0,
		RequiresBlueprint = false,
		Ingredients = {
			{ ItemId = "ClothScrap", Count = 2 },
			{ ItemId = "AntisepticBottle", Count = 1 },
		},
		OutputItemId = "SterileBandage",
		OutputCount = 2,
	},
	CraftSplint = {
		Id = "CraftSplint",
		Name = "Immobilizing Splint",
		Category = "Medical",
		Station = "Hand",
		CraftSeconds = 3.5,
		RequiresBlueprint = false,
		Ingredients = {
			{ ItemId = "WoodPlank", Count = 1 },
			{ ItemId = "ClothScrap", Count = 2 },
		},
		OutputItemId = "OrthopedicSplint",
		OutputCount = 1,
	},
	CraftHemostatic = {
		Id = "CraftHemostatic",
		Name = "Combat Hemostatic Dressing",
		Category = "Medical",
		Station = "Workbench",
		CraftSeconds = 4.5,
		RequiresBlueprint = false,
		Ingredients = {
			{ ItemId = "SterileBandage", Count = 2 },
			{ ItemId = "ChemicalReagent", Count = 1 },
		},
		OutputItemId = "HemostaticDressing",
		OutputCount = 2,
	},
	CraftPotassiumIodide = {
		Id = "CraftPotassiumIodide",
		Name = "Potassium Iodide Tablets",
		Category = "Medical",
		Station = "Workbench",
		CraftSeconds = 4.0,
		RequiresBlueprint = false,
		Ingredients = {
			{ ItemId = "CharcoalChunk", Count = 2 },
			{ ItemId = "ChemicalReagent", Count = 1 },
		},
		OutputItemId = "PotassiumIodide",
		OutputCount = 2,
	},
	CraftRadPurge = {
		Id = "CraftRadPurge",
		Name = "Chelation Rad-Purge Injector",
		Category = "Medical",
		Station = "Workbench",
		CraftSeconds = 6.0,
		RequiresBlueprint = true,
		Ingredients = {
			{ ItemId = "PotassiumIodide", Count = 1 },
			{ ItemId = "ChemicalReagent", Count = 2 },
			{ ItemId = "CrimsonIsotope", Count = 1 },
		},
		OutputItemId = "RadPurgeInjector",
		OutputCount = 1,
	},

	-- SURVIVAL & COOKING (Campfire)
	BoilPurifiedWater = {
		Id = "BoilPurifiedWater",
		Name = "Boil & Charcoal-Filter Water",
		Category = "Survival",
		Station = "Campfire",
		CraftSeconds = 3.5,
		RequiresBlueprint = false,
		Ingredients = {
			{ ItemId = "DirtyWaterBottle", Count = 1 },
			{ ItemId = "CharcoalChunk", Count = 1 },
		},
		OutputItemId = "PurifiedWater",
		OutputCount = 1,
	},
	BrewHerbalTea = {
		Id = "BrewHerbalTea",
		Name = "Hot Pine-Needle Infusion",
		Category = "Survival",
		Station = "Campfire",
		CraftSeconds = 3.0,
		RequiresBlueprint = false,
		Ingredients = {
			{ ItemId = "PurifiedWater", Count = 1 },
			{ ItemId = "WoodPlank", Count = 1 },
		},
		OutputItemId = "HerbalTea",
		OutputCount = 2,
	},
	CookRationStew = {
		Id = "CookRationStew",
		Name = "Hot Campfire Ration Stew",
		Category = "Survival",
		Station = "Campfire",
		CraftSeconds = 4.5,
		RequiresBlueprint = false,
		Ingredients = {
			{ ItemId = "RawVenison", Count = 1 },
			{ ItemId = "CannedBeans", Count = 1 },
		},
		OutputItemId = "CookedStew",
		OutputCount = 2,
	},
	BurnCharcoal = {
		Id = "BurnCharcoal",
		Name = "Pyrolyze Activated Charcoal",
		Category = "Survival",
		Station = "Campfire",
		CraftSeconds = 3.0,
		RequiresBlueprint = false,
		Ingredients = {
			{ ItemId = "WoodPlank", Count = 2 },
		},
		OutputItemId = "CharcoalChunk",
		OutputCount = 3,
	},

	-- TOOLS & PROTECTIVE GEAR
	CraftHammer = {
		Id = "CraftHammer",
		Name = "Carpenter's Framing Hammer",
		Category = "Tools",
		Station = "Hand",
		CraftSeconds = 3.5,
		RequiresBlueprint = false,
		Ingredients = {
			{ ItemId = "WoodPlank", Count = 1 },
			{ ItemId = "ScrapMetal", Count = 2 },
			{ ItemId = "DuctTape", Count = 1 },
		},
		OutputItemId = "BuildHammer",
		OutputCount = 1,
	},
	CraftRepairKit = {
		Id = "CraftRepairKit",
		Name = "Weapon & Gear Maintenance Kit",
		Category = "Tools",
		Station = "Workbench",
		CraftSeconds = 4.5,
		RequiresBlueprint = false,
		Ingredients = {
			{ ItemId = "ScrapMetal", Count = 2 },
			{ ItemId = "DuctTape", Count = 1 },
			{ ItemId = "ChemicalReagent", Count = 1 },
		},
		OutputItemId = "RepairKit",
		OutputCount = 1,
	},
	CraftGasMask = {
		Id = "CraftGasMask",
		Name = "CBRN Respirator Mask",
		Category = "Survival",
		Station = "Workbench",
		CraftSeconds = 6.5,
		RequiresBlueprint = false,
		Ingredients = {
			{ ItemId = "ClothScrap", Count = 3 },
			{ ItemId = "CharcoalChunk", Count = 3 },
			{ ItemId = "DuctTape", Count = 2 },
			{ ItemId = "ScrapMetal", Count = 2 },
		},
		OutputItemId = "GasMaskRespirator",
		OutputCount = 1,
	},
	CraftThermalParka = {
		Id = "CraftThermalParka",
		Name = "Insulated Field Parka",
		Category = "Survival",
		Station = "Workbench",
		CraftSeconds = 5.5,
		RequiresBlueprint = false,
		Ingredients = {
			{ ItemId = "ClothScrap", Count = 6 },
			{ ItemId = "DuctTape", Count = 2 },
		},
		OutputItemId = "ThermalParka",
		OutputCount = 1,
	},
	CraftTacticalVest = {
		Id = "CraftTacticalVest",
		Name = "Ballistic & Carapace Rig",
		Category = "Survival",
		Station = "Workbench",
		CraftSeconds = 7.5,
		RequiresBlueprint = true,
		Ingredients = {
			{ ItemId = "ScrapMetal", Count = 5 },
			{ ItemId = "ClothScrap", Count = 4 },
			{ ItemId = "DuctTape", Count = 2 },
		},
		OutputItemId = "TacticalPlateVest",
		OutputCount = 1,
	},

	-- WEAPONS
	CraftPipeClub = {
		Id = "CraftPipeClub",
		Name = "Improvised Pipe Wrench Club",
		Category = "Weapons",
		Station = "Hand",
		CraftSeconds = 4.0,
		RequiresBlueprint = false,
		Ingredients = {
			{ ItemId = "ScrapMetal", Count = 3 },
			{ ItemId = "ClothScrap", Count = 1 },
		},
		OutputItemId = "PipeClub",
		OutputCount = 1,
	},
	CraftSpikedBat = {
		Id = "CraftSpikedBat",
		Name = "Nail-Reinforced Ash Bat",
		Category = "Weapons",
		Station = "Workbench",
		CraftSeconds = 4.5,
		RequiresBlueprint = false,
		Ingredients = {
			{ ItemId = "WoodPlank", Count = 2 },
			{ ItemId = "ScrapMetal", Count = 2 },
			{ ItemId = "DuctTape", Count = 1 },
		},
		OutputItemId = "SpikedBat",
		OutputCount = 1,
	},
	CraftMakeshiftShotgun = {
		Id = "CraftMakeshiftShotgun",
		Name = "Improvised Pipe Shotgun",
		Category = "Weapons",
		Station = "Workbench",
		CraftSeconds = 7.0,
		RequiresBlueprint = false,
		Ingredients = {
			{ ItemId = "ScrapMetal", Count = 4 },
			{ ItemId = "WoodPlank", Count = 2 },
			{ ItemId = "DuctTape", Count = 2 },
		},
		OutputItemId = "MakeshiftShotgun",
		OutputCount = 1,
	},
	CraftCrossbow = {
		Id = "CraftCrossbow",
		Name = "Recurve Survival Crossbow",
		Category = "Weapons",
		Station = "Workbench",
		CraftSeconds = 7.5,
		RequiresBlueprint = false,
		Ingredients = {
			{ ItemId = "WoodPlank", Count = 3 },
			{ ItemId = "ScrapMetal", Count = 3 },
			{ ItemId = "ClothScrap", Count = 2 },
			{ ItemId = "DuctTape", Count = 2 },
		},
		OutputItemId = "TacticalCrossbow",
		OutputCount = 1,
	},

	-- AMMUNITION
	CraftBolts = {
		Id = "CraftBolts",
		Name = "Broadhead Crossbow Bolts (x6)",
		Category = "Ammunition",
		Station = "Hand",
		CraftSeconds = 3.0,
		RequiresBlueprint = false,
		Ingredients = {
			{ ItemId = "WoodPlank", Count = 1 },
			{ ItemId = "ScrapMetal", Count = 1 },
		},
		OutputItemId = "CrossbowBolt",
		OutputCount = 6,
	},
	CraftAmmo9mm = {
		Id = "CraftAmmo9mm",
		Name = "Reloaded 9x19mm Rounds (x15)",
		Category = "Ammunition",
		Station = "Workbench",
		CraftSeconds = 4.5,
		RequiresBlueprint = false,
		Ingredients = {
			{ ItemId = "ScrapMetal", Count = 2 },
			{ ItemId = "CharcoalChunk", Count = 1 },
			{ ItemId = "ChemicalReagent", Count = 1 },
		},
		OutputItemId = "Ammo9mm",
		OutputCount = 15,
	},
	CraftAmmo12g = {
		Id = "CraftAmmo12g",
		Name = "12-Gauge Buckshot Shells (x8)",
		Category = "Ammunition",
		Station = "Workbench",
		CraftSeconds = 5.0,
		RequiresBlueprint = false,
		Ingredients = {
			{ ItemId = "ScrapMetal", Count = 2 },
			{ ItemId = "CharcoalChunk", Count = 2 },
			{ ItemId = "ChemicalReagent", Count = 1 },
		},
		OutputItemId = "Ammo12Gauge",
		OutputCount = 8,
	},
	CraftAmmo556 = {
		Id = "CraftAmmo556",
		Name = "5.56x45mm NATO Rounds (x20)",
		Category = "Ammunition",
		Station = "Workbench",
		CraftSeconds = 5.5,
		RequiresBlueprint = true,
		Ingredients = {
			{ ItemId = "ScrapMetal", Count = 3 },
			{ ItemId = "CharcoalChunk", Count = 2 },
			{ ItemId = "ChemicalReagent", Count = 2 },
		},
		OutputItemId = "Ammo556",
		OutputCount = 20,
	},

	-- ELECTRONICS
	CraftBattery = {
		Id = "CraftBattery",
		Name = "Reconditioned Battery Cell",
		Category = "Electronics",
		Station = "Workbench",
		CraftSeconds = 4.0,
		RequiresBlueprint = false,
		Ingredients = {
			{ ItemId = "CircuitBoard", Count = 1 },
			{ ItemId = "ChemicalReagent", Count = 1 },
		},
		OutputItemId = "BatteryCell",
		OutputCount = 2,
	},
	CraftSignalDecoder = {
		Id = "CraftSignalDecoder",
		Name = "LAZARUS Signal Decoder",
		Category = "Electronics",
		Station = "Workbench",
		CraftSeconds = 8.0,
		RequiresBlueprint = false,
		Ingredients = {
			{ ItemId = "CircuitBoard", Count = 3 },
			{ ItemId = "BatteryCell", Count = 2 },
			{ ItemId = "CrimsonIsotope", Count = 1 },
		},
		OutputItemId = "SignalDecoder",
		OutputCount = 1,
	},
}

function CraftingRecipes.Get(recipeId: string): RecipeDef?
	return (CraftingRecipes.RECIPES :: any)[recipeId]
end

local orderedList: { RecipeDef } = {}
for _, r in pairs(CraftingRecipes.RECIPES :: { [string]: any }) do
	table.insert(orderedList, r)
end
table.sort(orderedList, function(a, b)
	if a.Category ~= b.Category then
		return a.Category < b.Category
	end
	return a.Name < b.Name
end)
CraftingRecipes.ORDERED_LIST = orderedList

return table.freeze(CraftingRecipes)
