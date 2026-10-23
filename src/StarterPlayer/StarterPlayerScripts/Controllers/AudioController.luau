--!strict
--[[
    ASHES OF HUMANITY: LAST SIGNAL
    StarterPlayerScripts.Controllers.AudioController
    Spatial Survival Horror Soundscape: Surface-Aware Footsteps, Weapon Transients,
    Geiger Radiation Ticks, Low-HP/Stress Heartbeat, Radio Static, and Horror Cues (Section 28).
    Uses verified built-in rbxasset://sounds/* primitives modulated with Equalizer & PlaybackSpeed
    so audio works immediately offline/online in Roblox Studio without broken asset IDs.
]]

local ReplicatedStorage = game:GetService("ReplicatedStorage")
local RunService = game:GetService("RunService")
local SoundService = game:GetService("SoundService")
local Workspace = game:GetService("Workspace")

local AudioController = {}

local registry: any = nil
local masterGroup: SoundGroup = nil :: any
local heartbeatSound: Sound = nil :: any
local geigerSound: Sound = nil :: any
local windLoop: Sound = nil :: any
local geigerTimer = 0.0

local SURFACE_PITCH: { [Enum.Material]: number } = {
	[Enum.Material.Concrete] = 1.02,
	[Enum.Material.Asphalt] = 0.98,
	[Enum.Material.Metal] = 1.38,
	[Enum.Material.CorrodedMetal] = 1.32,
	[Enum.Material.Wood] = 0.86,
	[Enum.Material.WoodPlanks] = 0.82,
	[Enum.Material.Grass] = 0.74,
	[Enum.Material.LeafyGrass] = 0.72,
	[Enum.Material.Mud] = 0.65,
	[Enum.Material.Ground] = 0.76,
	[Enum.Material.Sand] = 0.70,
}

function AudioController:Init(clientRegistry: any)
	registry = clientRegistry

	masterGroup = Instance.new("SoundGroup")
	masterGroup.Name = "AshesMasterAudio"
	masterGroup.Volume = 0.85
	masterGroup.Parent = SoundService

	-- Subtle atmospheric reverb in SoundService
	SoundService.AmbientReverb = Enum.ReverbType.StoneCorridor

	heartbeatSound = Instance.new("Sound")
	heartbeatSound.Name = "StressHeartbeat"
	heartbeatSound.SoundId = "rbxasset://sounds/collide.wav"
	heartbeatSound.PlaybackSpeed = 0.42
	heartbeatSound.Volume = 0
	heartbeatSound.Looped = true
	heartbeatSound.SoundGroup = masterGroup
	heartbeatSound.Parent = SoundService

	geigerSound = Instance.new("Sound")
	geigerSound.Name = "GeigerClick"
	geigerSound.SoundId = "rbxasset://sounds/snap.mp3"
	geigerSound.PlaybackSpeed = 2.35
	geigerSound.Volume = 0.28
	geigerSound.SoundGroup = masterGroup
	geigerSound.Parent = SoundService

	windLoop = Instance.new("Sound")
	windLoop.Name = "OutdoorWind"
	windLoop.SoundId = "rbxasset://sounds/impact_water.mp3"
	windLoop.PlaybackSpeed = 0.25
	windLoop.Volume = 0.08
	windLoop.Looped = true
	windLoop.SoundGroup = masterGroup
	windLoop.Parent = SoundService
end

function AudioController:PlayOneShot2D(soundId: string, volume: number, speed: number)
	local s = Instance.new("Sound")
	s.SoundId = soundId
	s.Volume = volume
	s.PlaybackSpeed = speed
	s.SoundGroup = masterGroup
	s.Parent = SoundService
	s:Play()
	task.delay(2.5, function()
		s:Destroy()
	end)
end

function AudioController:PlaySpatial3D(pos: Vector3, soundId: string, volume: number, speed: number, maxDist: number)
	local anchor = Instance.new("Part")
	anchor.Name = "AudioEmitter3D"
	anchor.Anchored = true
	anchor.CanCollide = false
	anchor.Transparency = 1
	anchor.Size = Vector3.new(0.2, 0.2, 0.2)
	anchor.Position = pos
	anchor.Parent = Workspace

	local s = Instance.new("Sound")
	s.SoundId = soundId
	s.Volume = volume
	s.PlaybackSpeed = speed
	s.RollOffMaxDistance = maxDist
	s.RollOffMinDistance = 6
	s.SoundGroup = masterGroup
	s.Parent = anchor
	s:Play()

	task.delay(3.0, function()
		anchor:Destroy()
	end)
end

function AudioController:PlayUIClick(pitch: number?)
	self:PlayOneShot2D("rbxasset://sounds/switch.wav", 0.25, pitch or 1.0)
end

function AudioController:PlayFootstep(material: Enum.Material, stance: string)
	local basePitch = SURFACE_PITCH[material] or 0.95
	local jitter = (math.random() - 0.5) * 0.10
	local vol = if stance == "Sprint"
		then 0.42
		elseif stance == "Crouch" then 0.14
		elseif stance == "Crawl" then 0.08
		else 0.26
	local soundAsset = if material == Enum.Material.Water
		then "rbxasset://sounds/impact_water.mp3"
		else "rbxasset://sounds/action_footsteps_plastic.mp3"
	self:PlayOneShot2D(soundAsset, vol, basePitch + jitter)
end

function AudioController:PlayWeaponSound(kindOrId: string, worldPos: Vector3?)
	if kindOrId == "DryFire" then
		self:PlayOneShot2D("rbxasset://sounds/switch.wav", 0.45, 1.65)
		return
	elseif kindOrId == "Melee" then
		self:PlayOneShot2D("rbxasset://sounds/snap.mp3", 0.42, 0.72)
		return
	elseif kindOrId == "TacticalCrossbow" then
		self:PlayOneShot2D("rbxasset://sounds/snap.mp3", 0.55, 1.18)
		return
	end

	local pitch = if kindOrId == "PumpShotgun" or kindOrId == "MakeshiftShotgun"
		then 0.52
		elseif kindOrId == "HuntingRifle" then 0.58
		elseif kindOrId == "AssaultRifle" then 0.76
		else 0.88

	if worldPos then
		self:PlaySpatial3D(worldPos, "rbxasset://sounds/collide.wav", 0.85, pitch, 140)
	else
		self:PlayOneShot2D("rbxasset://sounds/collide.wav", 0.80, pitch)
	end
end

function AudioController:Start()
	pcall(function()
		heartbeatSound:Play()
		windLoop:Play()
	end)

	registry.ClientController:On("RadioTransmission", function(_)
		self:PlayOneShot2D("rbxasset://sounds/switch.wav", 0.38, 0.65)
	end)

	registry.ClientController:On("HorrorCue", function(data)
		if type(data) ~= "table" then
			return
		end
		local pos = if typeof(data.Position) == "Vector3" then data.Position else nil
		if data.Kind == "ScreamerShriek" and pos then
			self:PlaySpatial3D(pos, "rbxasset://sounds/collide.wav", 1.0, 2.2, 165)
		elseif data.Kind == "CarAlarm" and pos then
			self:PlaySpatial3D(pos, "rbxasset://sounds/button.wav", 0.95, 1.45, 130)
		else
			self:PlayOneShot2D("rbxasset://sounds/collide.wav", 0.32, 0.35)
		end
	end)

	RunService.Heartbeat:Connect(function(dt: number)
		local state = registry.ClientController.State
		masterGroup.Volume = state.Settings.MasterVolume or 0.85

		local vitals = state.Vitals
		-- Stress & low-health heartbeat modulation
		local hpStress = math.clamp((45 - (vitals.Health or 100)) / 45, 0, 1)
		local psychStress = math.clamp(((vitals.MentalStress or 0) - 42) / 58, 0, 1)
		local combinedPulse = math.max(hpStress, psychStress)
		heartbeatSound.Volume = combinedPulse * 0.45
		heartbeatSound.PlaybackSpeed = 0.40 + combinedPulse * 0.38

		-- Outdoor weather wind volume
		local sheltered = vitals.IsSheltered == true
		local weather = state.Weather
		local baseWind = if weather == "Thunderstorm"
				or weather == "StrongWind"
				or weather == "RadiationStorm"
			then 0.24
			elseif weather == "Rain" or weather == "HeavyRain" then 0.16
			else 0.06
		windLoop.Volume = if sheltered then baseWind * 0.25 else baseWind

		-- Geiger counter radiation clicks
		local rad = vitals.Radiation or 0
		if rad > 4 then
			geigerTimer -= dt
			if geigerTimer <= 0 then
				local rate = math.clamp(rad / 18.0, 0.4, 9.0)
				geigerTimer = (1.0 / rate) * (0.65 + math.random() * 0.7)
				geigerSound.PlaybackSpeed = 2.1 + math.random() * 0.6
				geigerSound:Play()
			end
		end
	end)
end

return AudioController
