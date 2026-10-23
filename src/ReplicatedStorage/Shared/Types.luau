--!strict
--[[
    ASHES OF HUMANITY: LAST SIGNAL
    ReplicatedStorage.Shared.Types
    Shared type definitions across server services and client controllers.
]]

export type BodyPartId = "Head" | "Torso" | "LeftArm" | "RightArm" | "LeftLeg" | "RightLeg"

export type BodyConditionMap = {
	Bleeding: boolean,
	Fracture: boolean,
	Burn: boolean,
	Infection: boolean,
	RadiationSickness: boolean,
}

export type BodyRegionState = {
	HP: number,
	MaxHP: number,
	Conditions: BodyConditionMap,
}

export type VitalsState = {
	Health: number,
	MaxHealth: number,
	Hunger: number,
	Thirst: number,
	Stamina: number,
	MaxStaminaCap: number,
	BodyTemperature: number,
	Radiation: number,
	Infection: number,
	Fatigue: number,
	BleedStacks: number,
	MentalStress: number,
	FlashlightBattery: number,
	FlashlightOn: boolean,
	IsDowned: boolean,
	DownedBleedoutTimer: number,
	IsSheltered: boolean,
	CurrentRegion: string,
}

export type ItemCategory =
	"Food"
	| "Water"
	| "Medicine"
	| "Weapons"
	| "Ammunition"
	| "Tools"
	| "Materials"
	| "Clothing"
	| "Armor"
	| "Electronics"
	| "QuestItems"

export type EquipSlot = "Primary" | "Secondary" | "Melee" | "Headgear" | "Armor" | "Backpack" | "Utility"

export type GridItemInstance = {
	UID: string,
	ItemId: string,
	Count: number,
	Durability: number,
	MaxDurability: number,
	ClipAmmo: number?,
	X: number, -- 1-indexed grid column (0 if equipped in gear slot)
	Y: number, -- 1-indexed grid row
	Rotated: boolean,
}

export type InventoryState = {
	Cols: number,
	Rows: number,
	MaxWeightKg: number,
	CurrentWeightKg: number,
	Items: { [string]: GridItemInstance },
	Equipped: { [string]: string }, -- EquipSlot -> Item UID
	Hotbar: { [number]: string }, -- 1..5 -> Item UID
}

export type AIState = "IDLE" | "PATROL" | "INVESTIGATE" | "SEARCH" | "CHASE" | "ATTACK" | "RETREAT" | "RETURN"

export type WeatherId =
	"Clear"
	| "Fog"
	| "Rain"
	| "HeavyRain"
	| "Thunderstorm"
	| "RadiationStorm"
	| "ColdWeather"
	| "StrongWind"

export type QuestProgress = {
	QuestId: string,
	Status: "Active" | "Completed",
	Objectives: { [string]: number },
}

export type PlacedStructureData = {
	StructureUID: string,
	BuildingId: string,
	OwnerUserId: number,
	PartyId: string?,
	Position: { number },
	YawDeg: number,
	HP: number,
	MaxHP: number,
	Powered: boolean,
	Fuel: number?,
	StorageItems: { GridItemInstance }?,
}

export type PlayerSaveProfile = {
	SchemaVersion: number,
	SessionLockId: string?,
	SessionLockTime: number?,
	Position: { number }?,
	Vitals: VitalsState,
	BodyRegions: { [string]: BodyRegionState },
	Inventory: InventoryState,
	UnlockedBlueprints: { [string]: boolean },
	Quests: { [string]: QuestProgress },
	DiscoveredLore: { [string]: boolean },
	Stats: {
		DaysSurvived: number,
		MutantsKilled: number,
		ItemsCrafted: number,
		StructuresBuilt: number,
		TeammatesRevived: number,
		Deaths: number,
	},
}

return {}
