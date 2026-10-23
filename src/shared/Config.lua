--!strict
--[[
	ASHES OF HUMANITY: LAST SIGNAL
	Config — global tuning values shared by client and server.
]]

local Config = {}

Config.Version = "1.0.0"
Config.Title = "ASHES OF HUMANITY: LAST SIGNAL"
Config.SaveKey = "AshesProfile_v3"
Config.SaveInterval = 150
Config.MaxPlayers = 8

-- world --------------------------------------------------------------------
Config.WorldRadius = 3800
Config.Regions = {
	Suburbs = { name = "The Dead Suburbs", temp = 9, radiation = 0, danger = 1 },
	Forest = { name = "Blackwood Forest", temp = 5, radiation = 1, danger = 2 },
	Industrial = { name = "The Industrial Zone", temp = 8, radiation = 14, danger = 3 },
	City = { name = "The Forgotten City", temp = 7, radiation = 2, danger = 3 },
	Military = { name = "Military Exclusion Zone", temp = 6, radiation = 22, danger = 5 },
	Wasteland = { name = "The Red Wasteland", temp = 3, radiation = 46, danger = 6 },
}

-- survival -----------------------------------------------------------------
Config.Survival = {
	TickRate = 0.5, -- seconds between survival updates
	HungerDrain = 0.055, -- per second baseline
	ThirstDrain = 0.075,
	StaminaDrainSprint = 9.0,
	StaminaRegen = 7.5,
	StaminaRegenCrouch = 11.0,
	FatigueDrain = 0.02,
	BaseTemperature = 36.6,
	TemperatureDrift = 0.55, -- degrees per second toward ambient, scaled
	RadiationGainScale = 0.65,
	RadiationDecay = 0.18,
	InfectionGainScale = 0.30,
	BleedDamagePerStack = 0.35,
	StressDecay = 0.55,
	StarveDamage = 0.45,
	DehydrateDamage = 0.7,
	SleepDeprivationStress = 0.35,
	MaxHealth = 100,
	MaxPartHealth = 100,
}

Config.BodyParts = { "Head", "Torso", "LeftArm", "RightArm", "LeftLeg", "RightLeg" }
Config.BodyPartDamageMultiplier = {
	Head = 2.6, Torso = 1.0, LeftArm = 0.62, RightArm = 0.62,
	LeftLeg = 0.7, RightLeg = 0.7,
}

Config.Movement = {
	WalkSpeed = 11.5,
	SprintSpeed = 19.0,
	CrouchSpeed = 5.0,
	CrawlSpeed = 2.2,
	InjuredSpeed = 7.0,
	JumpPower = 44.0,
	Acceleration = 26.0,
	Deceleration = 34.0,
	FOV = 72,
	SprintFOV = 80,
	AimFOV = 56,
	BobAmplitude = 0.35,
	DefaultSensitivity = 0.55,
}

Config.EquipSlots = {
	"Primary", "Secondary", "Melee", "Flashlight",
	"Head", "Armor", "Backpack", "Utility1", "Utility2",
}

Config.Grid = {
	BackpackCols = 6,
	BackpackRows = 6,
	PocketCols = 3,
	PocketRows = 2,
	AmmoCols = 4,
	AmmoRows = 2,
	MaxWeightBase = 22.0,
}

Config.Interaction = {
	Range = 12,
	HoldTimeLoot = 0.35,
	HoldTimeHarvest = 1.1,
	LootRespawn = 420, -- seconds
	LootOwnerLock = 40,
}

Config.Combat = {
	MaxFireRateSlack = 0.12,
	HeadshotMultiplier = 2.6,
	MeleeStaminaCost = 6,
	GunshotNoise = 220,
	FootstepNoise = 18,
	SprintNoise = 46,
	MaxHitscanDistance = 500,
	FallDamageThreshold = 32,
	FallDamagePerUnit = 3.4,
}

Config.SurvivalEffects = {
	LowStaminaSpeedMul = 0.72,
	LowHydrationSpeedMul = 0.85,
	ColdSpeedMul = 0.9,
	BrokenLegSpeedMul = 0.55,
}

Config.Weather = {
	TransitionTime = 22,
	CycleMin = 240,
	CycleMax = 460,
	StormRadiationPerSecond = 0.32,
	RainTemperatureDrop = 2.4,
	FogVisibility = { Clear = 1400, Overcast = 900, Fog = 320, Rain = 700, HeavyRain = 420, Storm = 260, Radiation = 380 },
}

Config.TimeScale = 55 -- real seconds per in-game hour

Config.Stress = {
	GainDark = 0.06,
	GainAlone = 0.02,
	GainThreat = 0.9,
	GainAnomaly = 6.0,
	GainLowHealth = 0.25,
	MaxThreshold = 100,
	PanicThreshold = 72,
}

Config.Keys = {
	Inventory = Enum.KeyCode.Tab,
	Crafting = Enum.KeyCode.C,
	Journal = Enum.KeyCode.J,
	Map = Enum.KeyCode.M,
	Flashlight = Enum.KeyCode.F,
	Crouch = Enum.KeyCode.LeftControl,
	Sprint = Enum.KeyCode.LeftShift,
	Interact = Enum.KeyCode.E,
	Reload = Enum.KeyCode.R,
	Melee = Enum.KeyCode.V,
	Settings = Enum.KeyCode.Escape,
	Voice = Enum.KeyCode.X,
}

Config.Quality = {
	Low = { particles = 0.25, decorateRadius = 120, shadows = false, bloom = 0, dof = 0 },
	Medium = { particles = 0.6, decorateRadius = 220, shadows = true, bloom = 0.4, dof = 0.08 },
	High = { particles = 1.0, decorateRadius = 340, shadows = true, bloom = 0.55, dof = 0.18 },
	Ultra = { particles = 1.35, decorateRadius = 460, shadows = true, bloom = 0.7, dof = 0.26 },
}

Config.AmbientColors = {
	Clear = { ambient = Color3.fromRGB(15, 17, 24), outdoor = Color3.fromRGB(78, 80, 92), fog = Color3.fromRGB(96, 94, 88) },
	Overcast = { ambient = Color3.fromRGB(14, 15, 20), outdoor = Color3.fromRGB(62, 64, 70), fog = Color3.fromRGB(84, 84, 86) },
	Fog = { ambient = Color3.fromRGB(16, 17, 20), outdoor = Color3.fromRGB(58, 60, 62), fog = Color3.fromRGB(78, 80, 78) },
	HeavyRain = { ambient = Color3.fromRGB(10, 12, 16), outdoor = Color3.fromRGB(44, 48, 58), fog = Color3.fromRGB(62, 68, 76) },
	Rain = { ambient = Color3.fromRGB(12, 14, 18), outdoor = Color3.fromRGB(52, 56, 64), fog = Color3.fromRGB(70, 74, 80) },
	Storm = { ambient = Color3.fromRGB(9, 10, 14), outdoor = Color3.fromRGB(38, 40, 48), fog = Color3.fromRGB(58, 60, 66) },
	Radiation = { ambient = Color3.fromRGB(16, 18, 13), outdoor = Color3.fromRGB(78, 72, 48), fog = Color3.fromRGB(96, 82, 52) },
}

return Config
