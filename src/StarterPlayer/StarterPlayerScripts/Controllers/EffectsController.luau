--!strict
--[[
    ASHES OF HUMANITY: LAST SIGNAL
    StarterPlayerScripts.Controllers.EffectsController
    Camera-Aimed Dual-Cone Tactical Flashlight, Atmospheric Lighting,
    Weather Particles, Thunderstorm Lightning, Psychological Stress Grading,
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

-- Dedicated camera-tracked dual-beam tactical flashlight rig
local flashlightRig: Part = nil :: any
local centerThrowBeam: SpotLight = nil :: any
local outerSpillBeam: SpotLight = nil :: any
local nearFillGlow: PointLight = nil :: any
local currentBeamCF: CFrame? = nil

local lightningFlash = 0.0
local nextLightningAt = 18.0

local function ensureLightingInstance(className: string, name: string): any
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

	-- Camera-tracked handheld tactical flashlight rig (follows full 3D pitch + yaw in 1P & 3P!)
	flashlightRig = Instance.new("Part")
	flashlightRig.Name = "LocalFlashlightRig"
	flashlightRig.Size = Vector3.new(0.2, 0.2, 0.2)
	flashlightRig.Anchored = true
	flashlightRig.CanCollide = false
	flashlightRig.CanQuery = false
	flashlightRig.CanTouch = false
	flashlightRig.CastShadow = false
	flashlightRig.Transparency = 1
	flashlightRig.Parent = Workspace

	centerThrowBeam = Instance.new("SpotLight")
	centerThrowBeam.Name = "CenterThrowBeam"
	centerThrowBeam.Face = Enum.NormalId.Front
	centerThrowBeam.Angle = 38
	centerThrowBeam.Range = 130
	centerThrowBeam.Brightness = 6.5
	centerThrowBeam.Color = Color3.fromRGB(255, 244, 218)
	centerThrowBeam.Shadows = true
	centerThrowBeam.Enabled = true
	centerThrowBeam.Parent = flashlightRig

	outerSpillBeam = Instance.new("SpotLight")
	outerSpillBeam.Name = "OuterSpillBeam"
	outerSpillBeam.Face = Enum.NormalId.Front
	outerSpillBeam.Angle = 78
	outerSpillBeam.Range = 82
	outerSpillBeam.Brightness = 2.6
	outerSpillBeam.Color = Color3.fromRGB(255, 228, 186)
	outerSpillBeam.Shadows = false
	outerSpillBeam.Enabled = true
	outerSpillBeam.Parent = flashlightRig

	nearFillGlow = Instance.new("PointLight")
	nearFillGlow.Name = "NearFillGlow"
	nearFillGlow.Range = 16
	nearFillGlow.Brightness = 1.15
	nearFillGlow.Color = Color3.fromRGB(255, 232, 196)
	nearFillGlow.Shadows = false
	nearFillGlow.Enabled = true
	nearFillGlow.Parent = flashlightRig

	-- Lightweight camera-following weather canopy for Rain / Ash / Spores
	weatherCanopy = Instance.new("Part")
	weatherCanopy.Name = "WeatherCanopy"
	weatherCanopy.Size = Vector3.new(64, 1, 64)
	weatherCanopy.Anchored = true
	weatherCanopy.CanCollide = false
	weatherCanopy.CanQuery = false
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
	local enableShadows = preset.EnableLocalLightShadows == true
	Lighting.GlobalShadows = enableShadows
	if centerThrowBeam then
		centerThrowBeam.Shadows = enableShadows
	end
	dof.Enabled = preset.EnableDepthOfField == true
		and (registry.ClientController.State.Settings.ScreenEffects ~= false)
	sunRays.Enabled = (preset.SunRaysIntensity or 0) > 0
	sunRays.Intensity = preset.SunRaysIntensity or 0.08
	bloom.Enabled = presetName ~= "Low"
	bloom.Intensity = preset.BloomIntensity or 0.30
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
		local pScale = (
			(Config.GRAPHICS_PRESETS :: any)[state.Settings.GraphicsPreset] or Config.GRAPHICS_PRESETS.High
		).ParticleScale

		-- Position weather canopy above player camera when outdoors
		weatherCanopy.Position = cam.CFrame.Position + Vector3.new(0, 26, 0)
		local wProf = (Config.WEATHER_PROFILES :: any)[state.Weather] or Config.WEATHER_PROFILES.Clear
		local regProf = (Config.REGIONS :: any)[vitals.CurrentRegion] or Config.REGIONS.DeadSuburbs

		-- Smoothly interpolate Atmosphere density & color without crushing surface visibility
		local targetDensity = math.min(0.46, (regProf.FogDensity or 0.28) + (wProf.FogDensityAdd or 0.0))
		atmosphere.Density += (targetDensity - atmosphere.Density) * math.min(1, dt * 1.5)
		if state.Weather == "RadiationStorm" or vitals.CurrentRegion == "RedWasteland" then
			atmosphere.Color = atmosphere.Color:Lerp(Color3.fromRGB(168, 96, 72), math.min(1, dt * 1.5))
			atmosphere.Decay = atmosphere.Decay:Lerp(Color3.fromRGB(98, 52, 40), math.min(1, dt * 1.5))
		elseif state.IsNight then
			atmosphere.Color = atmosphere.Color:Lerp(Color3.fromRGB(72, 82, 96), math.min(1, dt * 1.5))
			atmosphere.Decay = atmosphere.Decay:Lerp(Color3.fromRGB(44, 50, 60), math.min(1, dt * 1.5))
		else
			atmosphere.Color = atmosphere.Color:Lerp(Color3.fromRGB(152, 158, 164), math.min(1, dt * 1.5))
			atmosphere.Decay = atmosphere.Decay:Lerp(Color3.fromRGB(98, 102, 108), math.min(1, dt * 1.5))
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

		-- Balanced Post-Apocalyptic Color Grading (preserves dark shadow detail instead of black-crushing!)
		local screenFx = state.Settings.ScreenEffects ~= false
		local lowHpRatio = if screenFx then math.clamp((40 - (vitals.Health or 100)) / 40, 0, 1) else 0
		local stressRatio = if screenFx then math.clamp(((vitals.MentalStress or 0) - 35) / 65, 0, 1) else 0

		colorCorrection.Saturation = -0.18 - stressRatio * 0.26 - lowHpRatio * 0.22
		colorCorrection.Contrast = 0.04 + stressRatio * 0.07
		colorCorrection.Brightness = 0.01 + lightningFlash
		local baseTint = if state.Weather == "RadiationStorm"
			then Color3.fromRGB(245, 208, 190)
			elseif state.Weather == "ColdWeather" then Color3.fromRGB(220, 234, 248)
			else Color3.fromRGB(240, 238, 234)
		colorCorrection.TintColor = baseTint:Lerp(Color3.fromRGB(225, 168, 162), lowHpRatio * 0.45)

		-- Suppress duplicate server SpotLight on local character Head (co-op peers still see it)
		local char = LocalPlayer.Character
		local head = if char then char:FindFirstChild("Head") else nil
		local serverSpot = if head then head:FindFirstChild("SurvivorFlashlight") :: SpotLight? else nil
		if serverSpot then
			serverSpot.Enabled = false
		end

		-- Update Camera-Aimed Dual-Beam Tactical Flashlight
		local bat = vitals.FlashlightBattery or 0
		local isFlashOn = (vitals.FlashlightOn == true) and (bat > 0) and ((vitals.Health or 100) > 0)

		local desiredCF: CFrame
		if registry.CameraController and not registry.CameraController:IsFirstPerson() and head and head:IsA("BasePart") then
			-- In 3rd-person, originate beam from survivor's shoulder/chest aimed along camera look vector
			local originPos = head.Position + cam.CFrame.RightVector * 0.85 + cam.CFrame.LookVector * 1.8
			- Vector3.new(0, 0.3, 0)
			desiredCF = CFrame.lookAt(originPos, originPos + cam.CFrame.LookVector * 40)
		else
			-- In 1st-person, offset slightly down-right like a real handheld flashlight for natural depth shadows
			desiredCF = cam.CFrame * CFrame.new(0.32, -0.26, -0.45)
		end

		if currentBeamCF == nil then
			currentBeamCF = desiredCF
		else
			currentBeamCF = (currentBeamCF :: CFrame):Lerp(desiredCF, math.min(1, dt * 26))
		end
		flashlightRig.CFrame = currentBeamCF :: CFrame

		if isFlashOn then
			local mult = 1.0
			if bat < 18 then
				local flicker = math.sin(os.clock() * 29.0) * math.cos(os.clock() * 13.0)
				mult = if flicker > -0.38 then 0.82 else 0.18
			end
			centerThrowBeam.Enabled = true
			outerSpillBeam.Enabled = true
			nearFillGlow.Enabled = true
			centerThrowBeam.Brightness = 6.5 * mult
			outerSpillBeam.Brightness = 2.6 * mult
			nearFillGlow.Brightness = 1.15 * mult
		else
			centerThrowBeam.Enabled = false
			outerSpillBeam.Enabled = false
			nearFillGlow.Enabled = false
		end
	end)
end

return EffectsController
