--!strict
--[[
    ASHES OF HUMANITY: LAST SIGNAL
    ReplicatedStorage.Shared.Config
    Master configuration for world geography, hardcore survival, body damage,
    movement, weather, day/night cycle, AI scheduling, base building, and security.
]]

local Config = {}

Config.GAME_TITLE = "ASHES OF HUMANITY: LAST SIGNAL"
Config.GAME_VERSION = "1.0.0"
Config.RADIO_SIGNAL_FREQ = "142.8 MHz"

-- World & Streaming
Config.WORLD_SIZE = Vector3.new(2400, 512, 2400)
Config.SPATIAL_CELL_SIZE = 80

-- Starter Underground Emergency Shelter ("Bunker Echo-07")
Config.SHELTER_SPAWN_POS = Vector3.new(0, -19.5, 620)
Config.SHELTER_EXIT_SURFACE_POS = Vector3.new(0, 3.5, 588)

-- Six Connected Open-World Regions
Config.REGIONS = {
	EmergencyShelter = {
		Id = "EmergencyShelter",
		DisplayName = "Bunker Echo-07 (Emergency Shelter)",
		Center = Vector3.new(0, -18, 620),
		Radius = 55,
		BaseTempC = 19.5,
		AmbientRadiation = 0.0,
		DangerTier = 0,
		AmbientProfile = "BunkerHum",
		FogDensity = 0.25,
	},
	DeadSuburbs = {
		Id = "DeadSuburbs",
		DisplayName = "The Dead Suburbs",
		Center = Vector3.new(0, 0, 540),
		MinBounds = Vector3.new(-460, -40, 180),
		MaxBounds = Vector3.new(460, 220, 960),
		Radius = 460,
		BaseTempC = 14.0,
		AmbientRadiation = 0.04,
		DangerTier = 1,
		AmbientProfile = "SuburbanWind",
		FogDensity = 0.36,
	},
	BlackwoodForest = {
		Id = "BlackwoodForest",
		DisplayName = "Blackwood Forest",
		Center = Vector3.new(-780, 0, 160),
		MinBounds = Vector3.new(-1150, -50, -340),
		MaxBounds = Vector3.new(-460, 240, 720),
		Radius = 460,
		BaseTempC = 9.5,
		AmbientRadiation = 0.08,
		DangerTier = 2,
		AmbientProfile = "ForestHollowWind",
		FogDensity = 0.52,
	},
	IndustrialZone = {
		Id = "IndustrialZone",
		DisplayName = "The Industrial Zone",
		Center = Vector3.new(780, 0, 180),
		MinBounds = Vector3.new(460, -50, -260),
		MaxBounds = Vector3.new(1150, 260, 720),
		Radius = 450,
		BaseTempC = 15.0,
		AmbientRadiation = 0.45,
		DangerTier = 3,
		AmbientProfile = "IndustrialGroan",
		FogDensity = 0.44,
	},
	ForgottenCity = {
		Id = "ForgottenCity",
		DisplayName = "The Forgotten City",
		Center = Vector3.new(0, 0, -340),
		MinBounds = Vector3.new(-460, -60, -860),
		MaxBounds = Vector3.new(460, 300, 180),
		Radius = 480,
		BaseTempC = 12.5,
		AmbientRadiation = 0.18,
		DangerTier = 3,
		AmbientProfile = "UrbanDesolation",
		FogDensity = 0.40,
	},
	MilitaryExclusionZone = {
		Id = "MilitaryExclusionZone",
		DisplayName = "The Military Exclusion Zone",
		Center = Vector3.new(-780, 0, -720),
		MinBounds = Vector3.new(-1150, -70, -1150),
		MaxBounds = Vector3.new(-460, 260, -340),
		Radius = 440,
		BaseTempC = 10.0,
		AmbientRadiation = 0.65,
		DangerTier = 4,
		AmbientProfile = "ExclusionSirenStatic",
		FogDensity = 0.42,
	},
	RedWasteland = {
		Id = "RedWasteland",
		DisplayName = "The Red Wasteland",
		Center = Vector3.new(780, 0, -700),
		MinBounds = Vector3.new(460, -60, -1150),
		MaxBounds = Vector3.new(1150, 280, -260),
		Radius = 460,
		BaseTempC = 18.0,
		AmbientRadiation = 1.85,
		DangerTier = 5,
		AmbientProfile = "WastelandGeigerStorm",
		FogDensity = 0.56,
	},
}

-- Day & Night Cycle
Config.DAY_LENGTH_SECONDS = 900 -- 15 real minutes per 24h game cycle
Config.START_CLOCK_TIME = 17.25 -- Late dusk awakening for immediate atmospheric tension
Config.NIGHT_START_HOUR = 19.5
Config.NIGHT_END_HOUR = 5.75
Config.NIGHT_TEMP_DROP_C = -8.5

-- Player Movement Constants
Config.MOVEMENT = {
	WalkSpeed = 12.5,
	SprintSpeed = 20.5,
	CrouchSpeed = 7.0,
	CrawlSpeed = 4.0,
	SwimSpeed = 9.5,
	ClimbSpeed = 8.0,
	JumpPower = 42.0,
	Acceleration = 9.5,
	Deceleration = 12.0,
	VaultMaxHeight = 4.6,
	VaultMinHeight = 1.8,
	VaultCheckDist = 4.2,
	SprintStaminaDrainPerSec = 11.5,
	JumpStaminaCost = 14.0,
	VaultStaminaCost = 10.0,
	BaseStaminaRegenPerSec = 8.5,
	StandHipHeightOffset = 0.0,
	CrouchCameraOffset = Vector3.new(0, -1.35, 0),
	CrawlCameraOffset = Vector3.new(0, -2.35, 0),
}

-- Hardcore Survival Vitals & Coupling Constants
Config.SURVIVAL = {
	TickInterval = 1.0,
	MaxHealth = 100.0,
	MaxHunger = 100.0,
	MaxThirst = 100.0,
	MaxStamina = 100.0,
	NormalBodyTempC = 37.0,
	HypothermiaThresholdC = 35.2,
	HyperthermiaThresholdC = 39.2,
	MaxRadiation = 1000.0,
	MaxInfection = 100.0,
	MaxFatigue = 100.0,
	MaxStress = 100.0,

	HungerDecayPerSec = 0.055, -- ~30 mins from full to 0
	ThirstDecayPerSec = 0.082, -- ~20 mins from full to 0
	FatigueGainPerSec = 0.032,
	ColdExtraFatiguePerSec = 0.045,

	StarvationDamagePerSec = 0.65,
	DehydrationDamagePerSec = 0.95,
	BleedDamagePerStackPerSec = 1.15,
	InfectionProgressPerSec = 0.14, -- worsens untreated
	SevereInfectionDamagePerSec = 0.55,
	HypothermiaDamagePerSec = 0.60,

	FlashlightMaxBattery = 100.0,
	FlashlightDrainPerSec = 0.28,
}

-- Location-Based Body Damage Regions
Config.BODY_REGIONS = {
	Head = { MaxHP = 45, DamageMult = 2.0 },
	Torso = { MaxHP = 85, DamageMult = 1.0 },
	LeftArm = { MaxHP = 55, DamageMult = 0.75 },
	RightArm = { MaxHP = 55, DamageMult = 0.75 },
	LeftLeg = { MaxHP = 65, DamageMult = 0.80 },
	RightLeg = { MaxHP = 65, DamageMult = 0.80 },
}

-- Grid Inventory Defaults
Config.INVENTORY = {
	BaseCols = 8,
	BaseRows = 5,
	BaseMaxWeightKg = 24.0,
	HotbarSlots = 5,
	MaxPickupDistance = 14.0,
}

-- Base Building Constants
Config.BUILDING = {
	GridSnapStuds = 4.0,
	RotationStepDeg = 45.0,
	MaxBuildDistance = 26.0,
	GeneratorPowerRadius = 56.0,
	MaxStructuresPerPlayer = 85,
}

-- Enemy AI Scheduling
Config.AI = {
	NearTickRate = 0.12,
	MidTickRate = 0.35,
	FarTickRate = 0.85,
	NearDist = 90.0,
	MidDist = 220.0,
	DespawnDist = 520.0,
	MaxActiveMutantsBase = 28,
	ExtraMutantsPerCoopPlayer = 6,
	PathRecomputeCooldown = 0.85,
	StuckTimeThreshold = 2.4,
}

-- Dynamic Weather Profiles
Config.WEATHER_PROFILES = {
	Clear = {
		DurationRange = { 110, 180 },
		FogDensityAdd = 0.0,
		TempOffsetC = 0.0,
		OutdoorRadPerSec = 0.0,
		SoundMaskFactor = 1.0,
		RainIntensity = 0.0,
		WindSpeed = 8.0,
		TintColor = Color3.fromRGB(214, 220, 224),
	},
	Fog = {
		DurationRange = { 90, 160 },
		FogDensityAdd = 0.24,
		TempOffsetC = -2.0,
		OutdoorRadPerSec = 0.0,
		SoundMaskFactor = 0.92,
		RainIntensity = 0.0,
		WindSpeed = 5.0,
		TintColor = Color3.fromRGB(186, 196, 198),
	},
	Rain = {
		DurationRange = { 85, 150 },
		FogDensityAdd = 0.14,
		TempOffsetC = -3.5,
		OutdoorRadPerSec = 0.02,
		SoundMaskFactor = 0.78,
		RainIntensity = 0.55,
		WindSpeed = 16.0,
		TintColor = Color3.fromRGB(172, 184, 196),
	},
	HeavyRain = {
		DurationRange = { 70, 130 },
		FogDensityAdd = 0.22,
		TempOffsetC = -5.0,
		OutdoorRadPerSec = 0.04,
		SoundMaskFactor = 0.62,
		RainIntensity = 0.90,
		WindSpeed = 26.0,
		TintColor = Color3.fromRGB(152, 166, 182),
	},
	Thunderstorm = {
		DurationRange = { 65, 115 },
		FogDensityAdd = 0.25,
		TempOffsetC = -5.5,
		OutdoorRadPerSec = 0.05,
		SoundMaskFactor = 0.48,
		RainIntensity = 1.0,
		WindSpeed = 34.0,
		TintColor = Color3.fromRGB(142, 155, 175),
	},
	RadiationStorm = {
		DurationRange = { 55, 95 },
		FogDensityAdd = 0.28,
		TempOffsetC = 2.5,
		OutdoorRadPerSec = 3.25,
		SoundMaskFactor = 0.70,
		RainIntensity = 0.25,
		WindSpeed = 30.0,
		TintColor = Color3.fromRGB(215, 158, 108),
	},
	ColdWeather = {
		DurationRange = { 80, 140 },
		FogDensityAdd = 0.12,
		TempOffsetC = -9.5,
		OutdoorRadPerSec = 0.0,
		SoundMaskFactor = 0.95,
		RainIntensity = 0.0,
		WindSpeed = 19.0,
		TintColor = Color3.fromRGB(185, 206, 225),
	},
	StrongWind = {
		DurationRange = { 75, 130 },
		FogDensityAdd = 0.10,
		TempOffsetC = -4.0,
		OutdoorRadPerSec = 0.08,
		SoundMaskFactor = 0.72,
		RainIntensity = 0.0,
		WindSpeed = 38.0,
		TintColor = Color3.fromRGB(196, 200, 196),
	},
}

-- Graphics Presets (Client-side tunable effects)
Config.GRAPHICS_PRESETS = {
	Low = {
		ParticleScale = 0.25,
		DecorDetailDist = 90,
		EnableLocalLightShadows = false,
		EnableDepthOfField = false,
		BloomIntensity = 0.2,
		SunRaysIntensity = 0.0,
		WeatherParticleRate = 18,
	},
	Medium = {
		ParticleScale = 0.60,
		DecorDetailDist = 180,
		EnableLocalLightShadows = false,
		EnableDepthOfField = false,
		BloomIntensity = 0.35,
		SunRaysIntensity = 0.06,
		WeatherParticleRate = 45,
	},
	High = {
		ParticleScale = 0.85,
		DecorDetailDist = 300,
		EnableLocalLightShadows = true,
		EnableDepthOfField = true,
		BloomIntensity = 0.45,
		SunRaysIntensity = 0.10,
		WeatherParticleRate = 75,
	},
	Ultra = {
		ParticleScale = 1.0,
		DecorDetailDist = 480,
		EnableLocalLightShadows = true,
		EnableDepthOfField = true,
		BloomIntensity = 0.55,
		SunRaysIntensity = 0.14,
		WeatherParticleRate = 110,
	},
}

-- Persistence & Security
Config.DATASTORE = {
	StoreName = "AshesOfHumanity_PlayerSave_v1",
	WorldStoreName = "AshesOfHumanity_WorldStructures_v1",
	SchemaVersion = 1,
	AutoSaveInterval = 90,
	SessionLockTTL = 300,
	MaxRetries = 3,
}

return table.freeze(Config)
