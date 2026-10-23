--!strict
--[[
    ASHES OF HUMANITY: LAST SIGNAL
    StarterPlayerScripts.Controllers.EffectsController
    Atmospheric Lighting, Weather Particles, Thunderstorm Lightning,
    Damaged Flashlight Battery Flicker, Psychological Stress Grading,
    and Graphics Quality Presets (Sections 8, 26, 27, 34).
]]

local Lighting = game:GetService("Lighting")
local Players = game:GetService("Players")
local ReplicatedStorage = game:GetService("ReplicatedStorage")
local RunService = game:GetService("RunService")
local Workspace = game:GetService("Workspace")

local Config = require(ReplicatedStorage.Shared.Config)

local EffectsController = {}

local LocalPlayer = Players.LocalPlayer
local registry: any = nil

local atmosphere: Atmosphere = nil :: any
local colorCorrection: ColorCorrectionEffect = nil :: any
local bloom: BloomEffect = nil :: any
local dof: DepthOfFieldEffect = nil :: any
local sunRays: SunRaysEffect = nil :: any

local weatherCanopy: Part = nil :: any
local rainEmitter: ParticleEmitter = nil :: any
local ashEmitter: ParticleEmitter = nil :: any

local lightningFlash = 0.0
local nextLightningAt = 18.0

local function ensureLightingInstance<T>(className: string, name: string): any
	local existing = Lighting:FindFirstChild(name)
	if existing and existing.ClassName == className then
		return existing
	end
	local inst = Instance.new(className)
	inst.Name = name
	inst.Parent = Lighting
	return inst
end

function EffectsController:Init(clientRegistry: any)
	registry = clientRegistry

	atmosphere = ensureLightingInstance("Atmosphere", "AshesAtmosphere")
	colorCorrection = ensureLightingInstance("ColorCorrectionEffect", "AshesColorGrading")
	bloom = ensureLightingInstance("BloomEffect", "AshesBloom")
	dof = ensureLightingInstance("DepthOfFieldEffect", "AshesDOF")
	sunRays = ensureLightingInstance("SunRaysEffect", "AshesSunRays")

	-- Create lightweight camera-following weather canopy for Rain / Ash / Spores
	weatherCanopy = Instance.new("Part")
	weatherCanopy.Name = "WeatherCanopy"
	weatherCanopy.Size = Vector3.new(64, 1, 64)
	weatherCanopy.Anchored = true
	weatherCanopy.CanCollide = false
	weatherCanopy.CastShadow = false
	weatherCanopy.Transparency = 1
	weatherCanopy.Parent = Workspace

	rainEmitter = Instance.new("ParticleEmitter")
	rainEmitter.Name = "RainDrops"
	rainEmitter.Color = ColorSequence.new(Color3.fromRGB(168, 182, 196))
	rainEmitter.Transparency = NumberSequence.new(0.45)
	rainEmitter.Size = NumberSequence.new(0.14)
	rainEmitter.Lifetime = NumberRange.new(0.7, 1.0)
	rainEmitter.Speed = NumberRange.new(68, 82)
	rainEmitter.EmissionDirection = Enum.NormalId.Bottom
	rainEmitter.Rate = 0
	rainEmitter.Enabled = false
	rainEmitter.Parent = weatherCanopy

	ashEmitter = Instance.new("ParticleEmitter")
	ashEmitter.Name = "AshAndSpores"
	ashEmitter.Color = ColorSequence.new(Color3.fromRGB(172, 158, 146))
	ashEmitter.Transparency = NumberSequence.new({
		NumberSequenceKeypoint.new(0, 0.8),
		NumberSequenceKeypoint.new(0.5, 0.35),
		NumberSequenceKeypoint.new(1, 1.0),
	})
	ashEmitter.Size = NumberSequence.new(0.18)
	ashEmitter.Lifetime = NumberRange.new(3.5, 5.5)
	ashEmitter.Speed = NumberRange.new(3, 7)
	ashEmitter.SpreadAngle = Vector2.new(65, 65)
	ashEmitter.EmissionDirection = Enum.NormalId.Bottom
	ashEmitter.Rate = 12
	ashEmitter.Enabled = true
	ashEmitter.Parent = weatherCanopy
end

function EffectsController:ApplyGraphicsPreset(presetName: string)
	local preset = (Config.GRAPHICS_PRESETS :: any)[presetName] or Config.GRAPHICS_PRESETS.High
	Lighting.GlobalShadows = preset.EnableLocalLightShadows == true
	dof.Enabled = preset.EnableDepthOfField == true
		and (registry.ClientController.State.Settings.ScreenEffects ~= false)
	sunRays.Enabled = (preset.SunRaysIntensity or 0) > 0
	sunRays.Intensity = preset.SunRaysIntensity or 0.08
	bloom.Enabled = presetName ~= "Low"
	bloom.Intensity = preset.BloomIntensity or 0.35
end

function EffectsController:Start()
	self:ApplyGraphicsPreset(registry.ClientController.State.Settings.GraphicsPreset or "High")

	RunService.RenderStepped:Connect(function(dt: number)
		local state = registry.ClientController.State
		local vitals = state.Vitals
		local cam = Workspace.CurrentCamera
		if not cam then
			return
		end

		self:ApplyGraphicsPreset(state.Settings.GraphicsPreset or "High")
		local pScale = ((Config.GRAPHICS_PRESETS :: any)[state.Settings.GraphicsPreset] or Config.GRAPHICS_PRESETS.High).ParticleScale

		-- Position weather canopy above player camera when outdoors
		weatherCanopy.Position = cam.CFrame.Position + Vector3.new(0, 26, 0)
		local wProf = (Config.WEATHER_PROFILES :: any)[state.Weather] or Config.WEATHER_PROFILES.Clear
		local regProf = (Config.REGIONS :: any)[vitals.CurrentRegion] or Config.REGIONS.DeadSuburbs

		-- Smoothly interpolate Atmosphere density & color
		local targetDensity = (regProf.FogDensity or 0.34) + (wProf.FogDensityAdd or 0.0)
		atmosphere.Density += (targetDensity - atmosphere.Density) * math.min(1, dt * 1.5)
		if state.Weather == "RadiationStorm" or vitals.CurrentRegion == "RedWasteland" then
			atmosphere.Color = atmosphere.Color:Lerp(Color3.fromRGB(168, 92, 68), math.min(1, dt * 1.5))
			atmosphere.Decay = atmosphere.Decay:Lerp(Color3.fromRGB(96, 48, 36), math.min(1, dt * 1.5))
		elseif state.IsNight then
			atmosphere.Color = atmosphere.Color:Lerp(Color3.fromRGB(58, 66, 78), math.min(1, dt * 1.5))
			atmosphere.Decay = atmosphere.Decay:Lerp(Color3.fromRGB(32, 36, 44), math.min(1, dt * 1.5))
		else
			atmosphere.Color = atmosphere.Color:Lerp(Color3.fromRGB(146, 152, 156), math.min(1, dt * 1.5))
			atmosphere.Decay = atmosphere.Decay:Lerp(Color3.fromRGB(88, 92, 96), math.min(1, dt * 1.5))
		end

		-- Rain & Ash emitter rates (suppressed indoors!)
		local sheltered = vitals.IsSheltered == true
		local rainRate = if sheltered then 0 else math.floor((wProf.RainIntensity or 0) * 140 * pScale)
		rainEmitter.Enabled = rainRate > 0
		rainEmitter.Rate = rainRate

		ashEmitter.Enabled = not sheltered and pScale > 0.25
		ashEmitter.Rate = math.floor((if vitals.CurrentRegion == "RedWasteland" then 38 else 10) * pScale)

		-- Thunderstorm lightning flashes
		if state.Weather == "Thunderstorm" and not sheltered then
			nextLightningAt -= dt
			if nextLightningAt <= 0 then
				nextLightningAt = 8 + math.random() * 14
				lightningFlash = 0.55
				if registry.AudioController then
					registry.AudioController:PlayOneShot2D("rbxasset://sounds/collide.wav", 0.72, 0.30)
				end
			end
		end
		lightningFlash = math.max(0, lightningFlash - dt * 2.8)

		-- Grounded Post-Apocalyptic Color Grading + Health/Stress feedback
		local screenFx = state.Settings.ScreenEffects ~= false
		local lowHpRatio = if screenFx then math.clamp((40 - (vitals.Health or 100)) / 40, 0, 1) else 0
		local stressRatio = if screenFx then math.clamp(((vitals.MentalStress or 0) - 35) / 65, 0, 1) else 0

		colorCorrection.Saturation = -0.22 - stressRatio * 0.32 - lowHpRatio * 0.25
		colorCorrection.Contrast = 0.14 + stressRatio * 0.10
		colorCorrection.Brightness = -0.02 + lightningFlash
		local baseTint = if state.Weather == "RadiationStorm"
			then Color3.fromRGB(245, 206, 188)
			elseif state.Weather == "ColdWeather" then Color3.fromRGB(218, 232, 248)
			else Color3.fromRGB(236, 234, 228)
		colorCorrection.TintColor = baseTint:Lerp(Color3.fromRGB(225, 168, 162), lowHpRatio * 0.45)

		-- Damaged Flashlight low-battery flicker on local character Head
		local char = LocalPlayer.Character
		local head = if char then char:FindFirstChild("Head") else nil
		local spot = if head then head:FindFirstChild("SurvivorFlashlight") :: SpotLight? else nil
		if spot and vitals.FlashlightOn then
			local bat = vitals.FlashlightBattery or 0
			if bat > 0 and bat < 22 then
				local flicker = math.sin(os.clock() * 29.0) * math.cos(os.clock() * 13.0)
				spot.Brightness = if flicker > -0.35 then 2.6 else 0.4
			elseif bat > 0 then
				spot.Brightness = 3.2
			else
				spot.Enabled = false
			end
		end
	end)
end

return EffectsController
