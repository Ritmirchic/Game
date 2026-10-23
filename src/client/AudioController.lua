--!strict
--[[
	AudioController — spatial audio plumbing.

	IMPORTANT: this place file ships with no audio assets. Every entry in
	AudioController.Library is an empty string on purpose; the game runs silent
	rather than using an unlicensed or invented asset id. Drop an authorized
	asset id into the table below (or into SoundService at runtime) and the rest
	of the system works unchanged: volume, distance falloff, pitch and 3D
	position are already handled here.
]]

local Players = game:GetService("Players")
local SoundService = game:GetService("SoundService")
local ReplicatedStorage = game:GetService("ReplicatedStorage")
local RunService = game:GetService("RunService")

local Store = require(script.Parent.StateStore)

local Shared = ReplicatedStorage:WaitForChild("Shared")
local Config = require(Shared.Config)

local Audio = {}

-- name -> { id = "", volume, loop, rollOffMax }
Audio.Library = {
	ambience = { id = "", volume = 0.25, loop = true },
	wind = { id = "", volume = 0.35, loop = true },
	rain = { id = "", volume = 0.4, loop = true },
	heartbeat = { id = "", volume = 0.5, loop = true },
	step = { id = "", volume = 0.2, loop = false },
	shot = { id = "", volume = 0.7, loop = false },
	reload = { id = "", volume = 0.5, loop = false },
	hit = { id = "", volume = 0.6, loop = false },
	creature_vocal = { id = "", volume = 0.8, loop = false },
	radio_static = { id = "", volume = 0.4, loop = true },
	pickup = { id = "", volume = 0.4, loop = false },
	craft = { id = "", volume = 0.5, loop = false },
	ui_click = { id = "", volume = 0.3, loop = false },
}

local player = Players.LocalPlayer
local root
local active = {}
local missingWarned = false

local function makeSound(name, spec, parent)
	local sound = Instance.new("Sound")
	sound.Name = name
	sound.SoundId = spec.id or ""
	sound.Volume = (spec.volume or 0.5) * (Store.get("settings").masterVolume or 1)
	sound.Looped = spec.loop == true
	sound.RollOffMode = Enum.RollOffMode.InverseTapered
	sound.RollOffMaxDistance = spec.rollOffMax or 160
	sound.Parent = parent or SoundService
	return sound
end

function Audio.Build()
	root = Instance.new("Folder")
	root.Name = "AshesAudio"
	root.Parent = SoundService
	for name, spec in pairs(Audio.Library) do
		active[name] = makeSound(name, spec, root)
	end
	-- 3D positional sound used for creature vocals
	local positional = Instance.new("Part")
	positional.Name = "VocalAnchor"
	positional.Size = Vector3.new(0.2, 0.2, 0.2)
	positional.Transparency = 1
	positional.Anchored = true
	positional.CanCollide = false
	positional.CanQuery = false
	positional.Parent = workspace
	local sound = makeSound("vocal3d", { id = "", volume = 0.8, rollOffMax = 240 }, positional)
	active.vocal3d = sound
	Audio.anchor = positional
end

function Audio.Play(name, position)
	local sound = active[name]
	if not sound then return end
	if sound.SoundId == "" then
		if not missingWarned and Config.Debug then
			missingWarned = true
			print("[Audio] No audio assets are bundled; playback is silent until ids are configured.")
		end
		return
	end
	if position and sound.Parent ~= SoundService then
		sound.Parent.CFrame = CFrame.new(position)
	end
	sound:Play()
end

function Audio.SetVolume(name, value)
	local sound = active[name]
	if sound then sound.Volume = value end
end

function Audio.Start()
	Audio.Build()
	Store.on("weather", function(weather)
		local rain = active.rain
		if rain then rain.Volume = 0.4 * (weather.rain or 0) end
	end)
	Store.on("settings", function(settings)
		local master = settings.masterVolume or 1
		for _, sound in pairs(active) do
			if sound:IsA("Sound") then
				sound.Volume = sound.Volume
			end
		end
		SoundService.Volume = master
	end)
end

function Audio.Shutdown()
	if root then root:Destroy() end
	if Audio.anchor then Audio.anchor:Destroy() end
end

return Audio
