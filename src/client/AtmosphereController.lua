--!strict
--[[
	AtmosphereController — client-side weather visuals and ambience.

	Rain is emitted from a box above the camera, so it follows the player without
	spawning thousands of parts. Lightning, dust and radiation motes are all
	procedural. No external assets are required to run this module; audio hooks
	are intentionally empty (see AudioController) because no licensed audio is
	bundled with this place file.
]]

local Lighting = game:GetService("Lighting")
local RunService = game:GetService("RunService")
local TweenService = game:GetService("TweenService")

local Store = require(script.Parent.StateStore)

local Atmosphere = {}

local rainEmitter
local dustEmitter
local radiationEmitter
local container
local lightningCooldown = 0
local lightningLight
local blur

local function emitter(parent, texture, rate, lifetime, speed, size)
	local particle = Instance.new("ParticleEmitter")
	particle.Texture = texture
	particle.Rate = rate
	particle.Lifetime = NumberRange.new(lifetime * 0.7, lifetime)
	particle.Speed = NumberRange.new(speed * 0.8, speed)
	particle.SpreadAngle = Vector2.new(4, 4)
	particle.Size = NumberSequence.new(size or 0.1)
	particle.Transparency = NumberSequence.new({
		NumberSequenceKeypoint.new(0, 0.35),
		NumberSequenceKeypoint.new(1, 1),
	})
	particle.LightEmission = 0.15
	particle.Parent = parent
	return particle
end

function Atmosphere.Build()
	container = Instance.new("Part")
	container.Name = "WeatherEmitter"
	container.Size = Vector3.new(90, 2, 90)
	container.Transparency = 1
	container.Anchored = true
	container.CanCollide = false
	container.CanQuery = false
	container.CanTouch = false
	container.Parent = workspace

	rainEmitter = emitter(container, "rbxasset://textures/particles/sparkles_main.dds",
		0, 0.9, 90, 0.06)
	rainEmitter.Rate = 0
	rainEmitter.Acceleration = Vector3.new(0, -120, 0)

	dustEmitter = emitter(container, "rbxasset://textures/particles/smoke_main.dds",
		18, 3.2, 3, 0.5)
	dustEmitter.Acceleration = Vector3.new(1.5, 0.4, 0)

	radiationEmitter = emitter(container, "rbxasset://textures/particles/sparkles_main.dds",
		0, 4, 2, 0.25)
	radiationEmitter.Color = ColorSequence.new(Color3.fromRGB(168, 196, 92))

	lightningLight = Instance.new("PointLight")
	lightningLight.Brightness = 0
	lightningLight.Range = 600
	lightningLight.Color = Color3.fromRGB(210, 220, 255)
	lightningLight.Parent = container

	local atmosphere = Lighting:FindFirstChildOfClass("Atmosphere")
	if atmosphere then
		-- density is driven by the server, this is only the client floor
		atmosphere.Density = math.max(atmosphere.Density, 0.3)
	end
	if not Lighting:FindFirstChild("AshesBlur") then
		blur = Instance.new("BlurEffect")
		blur.Name = "AshesBlur"
		blur.Size = 0
		blur.Parent = Lighting
	end
end

function Atmosphere.Update(dt)
	if not container then return end
	local camera = workspace.CurrentCamera
	if camera then
		container.CFrame = CFrame.new(camera.CFrame.Position + Vector3.new(0, 34, 0))
	end
	local weather = Store.get("weather") or {}
	local rain = weather.rain or 0
	rainEmitter.Rate = rain * 900
	rainEmitter.Speed = NumberRange.new(120, 160)
	dustEmitter.Rate = math.clamp(30 - rain * 30, 0, 30)
	radiationEmitter.Rate = (weather.kind == "Radiation") and 40 or 0

	lightningCooldown -= dt
	if weather.storm and lightningCooldown <= 0 then
		lightningCooldown = math.random(4, 11)
		Atmosphere.Flash()
	end
	if blur then
		local survival = Store.get("survival")
		local target = 0
		if (survival.stress or 0) > 78 then target = 10 elseif (survival.health or 100) < 20 then target = 6 end
		blur.Size = blur.Size + (target - blur.Size) * math.min(1, dt * 1.5)
	end
end

function Atmosphere.Flash()
	lightningLight.Brightness = 9
	Lighting.Brightness = math.min(6, Lighting.Brightness + 2.4)
	TweenService:Create(lightningLight, TweenInfo.new(0.28), { Brightness = 0 }):Play()
	TweenService:Create(Lighting, TweenInfo.new(0.9), { Brightness = math.max(0.2, Lighting.Brightness - 2.4) }):Play()
end

function Atmosphere.Start()
	Atmosphere.Build()
	RunService.RenderStepped:Connect(Atmosphere.Update)
end

return Atmosphere
