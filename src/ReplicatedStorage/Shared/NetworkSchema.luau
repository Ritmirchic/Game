--!strict
--[[
    ASHES OF HUMANITY: LAST SIGNAL
    ReplicatedStorage.Shared.NetworkSchema
    Centralized RemoteEvent & RemoteFunction registry + rate-limit budgets + payload validators.
]]

local UtilityModules = require(script.Parent.UtilityModules)

local NetworkSchema = {}

NetworkSchema.FOLDER_NAME = "Remotes"

-- Client -> Server / Server -> Client RemoteEvents
NetworkSchema.EVENTS = {
	-- State Sync (Server -> Client)
	StateSync = "StateSync",
	VitalsUpdate = "VitalsUpdate",
	InventoryUpdate = "InventoryUpdate",
	WeatherUpdate = "WeatherUpdate",
	WorldEventAlert = "WorldEventAlert",
	HorrorCue = "HorrorCue",
	RadioTransmission = "RadioTransmission",
	CombatFeedback = "CombatFeedback",
	QuestUpdate = "QuestUpdate",
	PartyUpdate = "PartyUpdate",
	ContainerOpened = "ContainerOpened",
	Notification = "Notification",

	-- Player Actions (Client -> Server, strictly validated & rate-limited)
	MovementStance = "MovementStance", -- Walk/Sprint/Crouch/Crawl/Vault/FootstepNoise
	ToggleFlashlight = "ToggleFlashlight",
	WeaponFire = "WeaponFire",
	WeaponReload = "WeaponReload",
	MeleeSwing = "MeleeSwing",
	InventoryAction = "InventoryAction", -- Move/Rotate/Equip/Unequip/Drop/Use/Split/Sort/BindHotbar
	MedicalTreat = "MedicalTreat", -- Use medical item on specific BodyPartId
	LootInteract = "LootInteract", -- OpenContainer / TakeLootItem / PickupWorldItem
	CraftRequest = "CraftRequest",
	BuildAction = "BuildAction", -- Place / Repair / Dismantle / TogglePower / Refuel
	InteractWorldObject = "InteractWorldObject", -- Doors, Lore Documents, Radio Towers, Generators, Campfires, SleepBunk
	QuestAction = "QuestAction", -- Track / AcceptSideQuest / Claim
	PartyAction = "PartyAction", -- Create / Invite / Accept / Leave / ReviveTeammate
	SettingsUpdate = "SettingsUpdate",
	RespawnRequest = "RespawnRequest",
}

NetworkSchema.FUNCTIONS = {
	PingState = "PingState",
}

-- Per-remote token-bucket rate limits (MaxBurst, RefillPerSecond)
NetworkSchema.RATE_LIMITS = {
	MovementStance = { Burst = 18, Rate = 10 },
	ToggleFlashlight = { Burst = 6, Rate = 3 },
	WeaponFire = { Burst = 20, Rate = 14 },
	WeaponReload = { Burst = 5, Rate = 2 },
	MeleeSwing = { Burst = 8, Rate = 4 },
	InventoryAction = { Burst = 16, Rate = 8 },
	MedicalTreat = { Burst = 6, Rate = 3 },
	LootInteract = { Burst = 12, Rate = 6 },
	CraftRequest = { Burst = 6, Rate = 2 },
	BuildAction = { Burst = 8, Rate = 4 },
	InteractWorldObject = { Burst = 10, Rate = 5 },
	QuestAction = { Burst = 6, Rate = 2 },
	PartyAction = { Burst = 6, Rate = 2 },
	SettingsUpdate = { Burst = 6, Rate = 2 },
	RespawnRequest = { Burst = 3, Rate = 0.5 },
	PingState = { Burst = 4, Rate = 1 },
}

function NetworkSchema.ValidateString(val: any, maxLen: number?): boolean
	if type(val) ~= "string" then
		return false
	end
	local limit = maxLen or 64
	return #val > 0 and #val <= limit
end

function NetworkSchema.ValidateInt(val: any, minVal: number, maxVal: number): boolean
	if not UtilityModules.IsFiniteNumber(val) then
		return false
	end
	if math.floor(val) ~= val then
		return false
	end
	return val >= minVal and val <= maxVal
end

function NetworkSchema.ValidateNumber(val: any, minVal: number, maxVal: number): boolean
	if not UtilityModules.IsFiniteNumber(val) then
		return false
	end
	return val >= minVal and val <= maxVal
end

function NetworkSchema.ValidateVector3(val: any, maxAbsCoord: number?): boolean
	if not UtilityModules.IsFiniteVector3(val) then
		return false
	end
	local bound = maxAbsCoord or 4096
	return math.abs(val.X) <= bound and math.abs(val.Y) <= bound and math.abs(val.Z) <= bound
end

return table.freeze(NetworkSchema)
