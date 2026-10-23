--!strict
--[[
	WeatherService — day/night cycle and dynamic weather.

	Owns Lighting/Atmosphere values and broadcasts a compact state to clients so
	they can drive local particles, audio and post-processing.
]]

local Lighting = game:GetService("Lighting")
local ReplicatedStorage = game:GetService("ReplicatedStorage")
local RunService = game:GetService("RunService")

local Shared = ReplicatedStorage:WaitForChild("Shared")
local Config = require(Shared.Config)
local Util = require(Shared.Util)

local WeatherService = {}

local registryRef
local remotes
local state = {
	kind = "Overcast",
	nextKind = "Fog",
	progress = 0,
	clock = 17.4,
	day = 1,
	storm = false,
	temperatureOffset = 0,
	foggy = false,
	rain = 0,
	wind = 0.2,
	intensity = 0,
}
local rng = Random.new(os.time())
local accumulator = 0
local broadcastAccumulator = 0
local WEATHERS = { "Clear", "Overcast", "Fog", "Rain", "HeavyRain", "Storm", "Radiation" }

function WeatherService.Init(registry)
	registryRef = registry
	remotes = registry.Remotes
end

function WeatherService.Start(registry)
	state.kind = "Overcast"
	state.nextKind = "Fog"
	WeatherService.ApplyLighting()

	RunService.Heartbeat:Connect(function(dt)
		accumulator += dt
		broadcastAccumulator += dt
		-- advance the clock (Config.TimeScale real seconds per in-game hour)
		state.clock += dt / Config.TimeScale
		if state.clock >= 24 then
			state.clock -= 24
			state.day += 1
			WeatherService.OnNewDay()
		end
		-- weather transition
		if state.progress < 1 then
			state.progress = math.min(1, state.progress + dt / Config.Weather.TransitionTime)
			if state.progress >= 1 then
				state.kind = state.nextKind
				state.nextKind = WeatherService.PickNext(state.kind)
				state.progress = 0
			end
		end
		if accumulator >= 0.5 then
			accumulator = 0
			WeatherService.ApplyLighting()
		end
		if broadcastAccumulator >= 1.0 then
			broadcastAccumulator = 0
			WeatherService.Broadcast()
		end
	end)
end

function WeatherService.PickNext(current)
	local options = {}
	for _, kind in ipairs(WEATHERS) do
		if kind ~= current then
			local weight = 1
			if kind == "Clear" then weight = 3 end
			if kind == "Overcast" then weight = 4 end
			if kind == "Fog" then weight = 3 end
			if kind == "Rain" then weight = 2 end
			if kind == "HeavyRain" then weight = 1 end
			if kind == "Storm" then weight = 0.8 end
			if kind == "Radiation" then weight = 0.35 end
			table.insert(options, { kind = kind, weight = weight })
		end
	end
	local pick = Util.weightedPick(options, rng)
	return pick and pick.kind or "Overcast"
end

function WeatherService.OnNewDay()
	state.day += 0
	for _, player in ipairs(game:GetService("Players"):GetPlayers()) do
		local profile = registryRef.PlayerDataService.Get(player)
		if profile then
			profile.day = state.day
		end
	end
	remotes.FireAll("Notify", string.format("Day %d. The sun is up. That is all it does.", state.day), "info", 5)
end

function WeatherService.IsNight()
	return state.clock < 5.2 or state.clock > 19.6
end

function WeatherService.GetState()
	return state
end

function WeatherService.SetClock(hours)
	state.clock = hours % 24
	WeatherService.ApplyLighting()
end

function WeatherService.ForceWeather(kind)
	if not table.find(WEATHERS, kind) then return end
	state.nextKind = kind
	state.progress = math.min(state.progress, 0.5)
	state.kind = kind
	state.progress = 0
	WeatherService.ApplyLighting()
	WeatherService.Broadcast()
end

function WeatherService.ApplyLighting()
	local kind = state.kind
	local colors = Config.AmbientColors[kind] or Config.AmbientColors.Overcast
	local atmosphere = Lighting:FindFirstChildOfClass("Atmosphere")
	local clouds = Lighting:FindFirstChildOfClass("Clouds")
	local night = WeatherService.IsNight()

	-- sun position from the clock
	local dayFraction = (state.clock - 6) / 12
	local elevation = math.sin(dayFraction * math.pi)
	Lighting.ClockTime = state.clock
	Lighting.GeographicLatitude = 51
	Lighting.Brightness = night and 0.35 or (0.9 + math.max(0, elevation) * 1.1)

	local ambient = colors.ambient
	local outdoor = colors.outdoor
	local fogColor = colors.fog
	if night then
		ambient = Color3.fromRGB(8, 9, 13)
		outdoor = Color3.fromRGB(18, 20, 28)
		fogColor = Color3.fromRGB(26, 28, 34)
	end
	Lighting.Ambient = ambient
	Lighting.OutdoorAmbient = outdoor
	Lighting.FogColor = fogColor
	Lighting.FogStart = night and 30 or 120
	Lighting.FogEnd = (Config.Weather.FogVisibility[kind] or 900) * (night and 0.45 or 1)
	Lighting.EnvironmentDiffuseScale = kind == "Clear" and 0.7 or 0.4
	Lighting.EnvironmentSpecularScale = kind == "Clear" and 0.6 or 0.35

	if atmosphere then
		atmosphere.Density = (kind == "Fog" or kind == "Storm") and 0.72 or (kind == "Radiation" and 0.62 or 0.36)
		atmosphere.Haze = (kind == "Fog" and 6.0) or 2.0
		atmosphere.Glare = night and 0.05 or 0.22
		atmosphere.Color = kind == "Radiation" and Color3.fromRGB(150, 130, 90) or Color3.fromRGB(130, 132, 140)
		atmosphere.Decay = Color3.fromRGB(58, 58, 62)
	end
	if clouds then
		clouds.Cover = (kind == "Clear" and 0.35) or (kind == "Overcast" and 0.8) or 0.95
		clouds.Density = (kind == "Storm" and 0.9) or 0.55
	end

	state.storm = kind == "Storm"
	state.foggy = kind == "Fog" or kind == "HeavyRain" or kind == "Storm"
	state.rain = (kind == "Rain" and 0.5) or (kind == "HeavyRain" and 0.85) or (kind == "Storm" and 1) or 0
	state.wind = (kind == "Storm" and 0.9) or (kind == "HeavyRain" and 0.5) or 0.25
	state.temperatureOffset = (kind == "Storm" and -5) or (kind == "HeavyRain" and -3.4)
		or (kind == "Rain" and -2.4) or (kind == "Clear" and 2) or 0
	state.intensity = state.progress
end

function WeatherService.Broadcast()
	remotes.FireAll("Atmosphere", state)
	remotes.FireAll("ServerTime", state.clock, state.day)
end

return WeatherService
