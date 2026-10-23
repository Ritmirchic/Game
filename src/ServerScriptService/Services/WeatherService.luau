--!strict
--[[
    ASHES OF HUMANITY: LAST SIGNAL
    ServerScriptService.Services.WeatherService
    Complete Day/Night Cycle & 8-State Dynamic Weather System (Sections 26, 27).
]]

local Lighting = game:GetService("Lighting")
local Players = game:GetService("Players")
local ReplicatedStorage = game:GetService("ReplicatedStorage")

local Config = require(ReplicatedStorage.Shared.Config)
local NetworkSchema = require(ReplicatedStorage.Shared.NetworkSchema)
local Types = require(ReplicatedStorage.Shared.Types)
local UtilityModules = require(ReplicatedStorage.Shared.UtilityModules)

local WeatherService = {}

local registry: any = nil
local currentWeather: Types.WeatherId = "Clear"
local weatherTimer = 140.0
local clockHours = Config.START_CLOCK_TIME
local dayCount = 1
local rng = Random.new()

local WEATHER_TRANSITION_WEIGHTS: { UtilityModules.WeightedEntry<Types.WeatherId> } = {
	{ Weight = 24, Value = "Clear" },
	{ Weight = 20, Value = "Fog" },
	{ Weight = 16, Value = "Rain" },
	{ Weight = 10, Value = "HeavyRain" },
	{ Weight = 9, Value = "Thunderstorm" },
	{ Weight = 6, Value = "RadiationStorm" },
	{ Weight = 8, Value = "ColdWeather" },
	{ Weight = 7, Value = "StrongWind" },
}

function WeatherService:Init(serviceRegistry: any)
	registry = serviceRegistry
	Lighting.ClockTime = clockHours
end

function WeatherService:IsNight(): boolean
	return clockHours >= Config.NIGHT_START_HOUR or clockHours < Config.NIGHT_END_HOUR
end

function WeatherService:GetClockTime(): number
	return clockHours
end

function WeatherService:GetCurrentWeather(): Types.WeatherId
	return currentWeather
end

function WeatherService:GetProfile()
	return (Config.WEATHER_PROFILES :: any)[currentWeather] or Config.WEATHER_PROFILES.Clear
end

function WeatherService:GetTempOffsetC(): number
	return self:GetProfile().TempOffsetC
end

function WeatherService:GetOutdoorRadiationPerSec(): number
	return self:GetProfile().OutdoorRadPerSec
end

function WeatherService:GetSoundMaskFactor(): number
	return self:GetProfile().SoundMaskFactor
end

function WeatherService:SetWeather(newWeather: Types.WeatherId, customDuration: number?)
	local prof = (Config.WEATHER_PROFILES :: any)[newWeather]
	if not prof then
		return
	end
	currentWeather = newWeather
	local durRange = prof.DurationRange
	weatherTimer = customDuration or rng:NextNumber(durRange[1], durRange[2])

	Lighting:SetAttribute("CurrentWeather", newWeather)
	registry.BroadcastAll(NetworkSchema.EVENTS.WeatherUpdate, {
		Weather = currentWeather,
		ClockTime = clockHours,
		IsNight = self:IsNight(),
		DayCount = dayCount,
	})

	if newWeather == "RadiationStorm" then
		registry.BroadcastAll(NetworkSchema.EVENTS.WorldEventAlert, {
			EventId = "RadiationStorm",
			Title = "ATMOSPHERIC FALLOUT FRONT",
			Description = "A crimson Radiation Storm is sweeping across the sector — seek overhead shelter or equip CBRN protection!",
		})
	end
end

function WeatherService:Step(dt: number)
	local prevHours = clockHours
	local hoursPerSec = 24.0 / Config.DAY_LENGTH_SECONDS
	clockHours = (clockHours + hoursPerSec * dt) % 24.0
	Lighting.ClockTime = clockHours

	-- Detect sunrise rollover -> increment survived days stat
	if prevHours > 20.0 and clockHours < 4.0 then
		dayCount += 1
		for _, player in ipairs(Players:GetPlayers()) do
			local profile = registry.PlayerDataService:GetProfile(player)
			if profile and profile.Vitals.Health > 0 then
				profile.Stats.DaysSurvived += 1
			end
		end
	end

	weatherTimer -= dt
	if weatherTimer <= 0 then
		local picked = UtilityModules.PickWeighted(WEATHER_TRANSITION_WEIGHTS, rng) or "Fog"
		self:SetWeather(picked)
	end
end

function WeatherService:Start()
	Lighting:SetAttribute("CurrentWeather", currentWeather)
	task.spawn(function()
		local syncAcc = 0
		while true do
			task.wait(1.0)
			self:Step(1.0)
			syncAcc += 1
			if syncAcc >= 8 then
				syncAcc = 0
				registry.BroadcastAll(NetworkSchema.EVENTS.WeatherUpdate, {
					Weather = currentWeather,
					ClockTime = clockHours,
					IsNight = self:IsNight(),
					DayCount = dayCount,
				})
			end
		end
	end)
end

return WeatherService
