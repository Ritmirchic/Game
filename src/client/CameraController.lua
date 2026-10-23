--!strict
--[[
	CameraController — first/third person, ADS zoom, head bob, injury shake
	and death camera.

	Roblox's default camera handles collision and mouse look; this module tunes
	offset, distance and FOV, and adds a hand-made bob calculated from velocity.
]]

local Players = game:GetService("Players")
local RunService = game:GetService("RunService")
local UserInputService = game:GetService("UserInputService")

local Store = require(script.Parent.StateStore)

local Camera = {}

local player = Players.LocalPlayer
local camera = workspace.CurrentCamera
local mode = "third" -- "first" or "third"
local aimAmount = 0
local bobPhase = 0
local shake = 0
local baseFov = 72
local thirdDistance = 9.5

local function currentCharacter()
	return player.Character
end

function Camera.GetMode()
	return mode
end

function Camera.SetMode(newMode)
	mode = newMode
	Store.set("cameraMode", mode)
	if mode == "first" then
		player.CameraMode = Enum.CameraMode.LockFirstPerson
		camera.FieldOfView = baseFov
	else
		player.CameraMode = Enum.CameraMode.Classic
		player.CameraMaxZoomDistance = 24
		player.CameraMinZoomDistance = 0.5
	end
end

function Camera.ToggleMode()
	Camera.SetMode(mode == "third" and "first" or "third")
end

function Camera.SetAiming(on)
	local wants = on == true
	Store.set("aiming", wants)
	if wants then
		if mode == "first" then
			camera.FieldOfView = baseFov - 14
		else
			player.CameraMaxZoomDistance = 4.6
			player.CameraMinZoomDistance = 2.4
		end
	else
		camera.FieldOfView = baseFov
		if mode == "third" then
			player.CameraMaxZoomDistance = 24
		end
	end
end

function Camera.Shake(amount)
	shake = math.min(4, shake + (amount or 1))
end

function Camera.ApplyFovFromSettings()
	local settings = Store.get("settings")
	baseFov = settings.fov or 72
	camera.FieldOfView = baseFov
end

function Camera.Start()
	camera = workspace.CurrentCamera
	Camera.ApplyFovFromSettings()
	Camera.SetMode("third")

	Store.on("settings", function(settings)
		baseFov = settings.fov or 72
		if not Store.get("aiming") then
			camera.FieldOfView = baseFov
		end
		player.CameraMaxZoomDistance = settings.fov and 24 or 24
	end)
	Store.on("survival", function(survival)
		if survival.health and survival.health < 30 then
			shake = math.max(shake, 0.2)
		end
		if survival.bleeding and survival.bleeding > 0 then
			shake = math.max(shake, 0.12)
		end
	end)

	RunService.RenderStepped:Connect(function(dt)
		local character = currentCharacter()
		local hum = character and character:FindFirstChildOfClass("Humanoid")
		local hrp = character and character:FindFirstChild("HumanoidRootPart")
		if not hum or not hrp or hum.Health <= 0 then
			shake = math.max(0, shake - dt * 2)
			return
		end
		local settings = Store.get("settings")
		local speed = Vector3.new(hrp.AssemblyLinearVelocity.X, 0, hrp.AssemblyLinearVelocity.Z).Magnitude

		if settings.headBob ~= false then
			bobPhase += dt * (speed > 1 and (speed * 0.9) or 2.4)
			local bobY = math.sin(bobPhase * 2) * math.min(0.22, speed / 90)
			local bobX = math.cos(bobPhase) * math.min(0.14, speed / 130)
			local offset = camera.CFrame
			local desired = CFrame.new(bobX, bobY, 0)
			camera.CFrame = camera.CFrame * desired
		end

		if shake > 0.01 then
			local amt = shake * 0.012
			camera.CFrame = camera.CFrame * CFrame.Angles(
				(math.noise(os.clock() * 22) - 0.5) * amt,
				(math.noise(os.clock() * 19 + 10) - 0.5) * amt,
				(math.noise(os.clock() * 17 + 20) - 0.5) * amt * 2
			)
			shake = math.max(0, shake - dt * 1.6)
		end

		if mode == "first" then
			-- hide the character so we do not look through our own skull
			local head = character:FindFirstChild("Head")
			if head and head.LocalTransparencyModifier < 0.9 then
				head.LocalTransparencyModifier = 1
			end
		end
	end)

	UserInputService.InputBegan:Connect(function(input, processed)
		if processed then return end
		if input.KeyCode == Enum.KeyCode.V then
			Camera.ToggleMode()
		elseif input.UserInputType == Enum.UserInputType.MouseButton2 then
			Camera.SetAiming(true)
		end
	end)
	UserInputService.InputEnded:Connect(function(input)
		if input.UserInputType == Enum.UserInputType.MouseButton2 then
			Camera.SetAiming(false)
		end
	end)
end

return Camera
