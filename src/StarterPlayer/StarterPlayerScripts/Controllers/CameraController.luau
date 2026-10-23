--!strict
--[[
    ASHES OF HUMANITY: LAST SIGNAL
    StarterPlayerScripts.Controllers.CameraController
    Immersive First-Person / Over-the-Shoulder Third-Person Camera,
    ADS Zoom, Subtle Head-Bob, Breathing Sway, Weapon Recoil Recovery,
    Damage Impulse, and Accessibility ReduceMotion Support (Section 14).
]]

local Players = game:GetService("Players")
local RunService = game:GetService("RunService")
local UserInputService = game:GetService("UserInputService")
local Workspace = game:GetService("Workspace")

local CameraController = {}

local LocalPlayer = Players.LocalPlayer
local registry: any = nil

local firstPerson = true
local isADS = false
local targetFOV = 78.0
local currentFOV = 78.0
local bobPhase = 0.0
local recoilPitch = 0.0
local recoilYaw = 0.0
local damageRollKick = 0.0

function CameraController:Init(clientRegistry: any)
	registry = clientRegistry
end

function CameraController:IsFirstPerson(): boolean
	return firstPerson
end

function CameraController:IsADS(): boolean
	return isADS
end

function CameraController:SetADS(active: boolean, zoomFOV: number?)
	isADS = active
	local baseFOV = registry.ClientController.State.Settings.FOV or 78
	targetFOV = if active then (zoomFOV or 56) else baseFOV
end

function CameraController:AddRecoil(pitchDeg: number, yawDeg: number)
	local reduce = registry.ClientController.State.Settings.ReduceMotion == true
	local mult = if reduce then 0.45 else 1.0
	recoilPitch += math.rad(pitchDeg * mult)
	recoilYaw += math.rad(yawDeg * mult)
end

function CameraController:AddDamageKick(intensity: number)
	if registry.ClientController.State.Settings.ReduceMotion == true then
		return
	end
	damageRollKick = math.clamp(damageRollKick + math.rad(intensity * 3.2), -0.18, 0.18)
end

function CameraController:ApplyCameraMode()
	if firstPerson then
		LocalPlayer.CameraMode = Enum.CameraMode.LockFirstPerson
		LocalPlayer.CameraMinZoomDistance = 0.5
		LocalPlayer.CameraMaxZoomDistance = 0.5
	else
		LocalPlayer.CameraMode = Enum.CameraMode.Classic
		LocalPlayer.CameraMinZoomDistance = 6.5
		LocalPlayer.CameraMaxZoomDistance = 10.5
	end
end

function CameraController:TogglePerspective()
	firstPerson = not firstPerson
	self:ApplyCameraMode()
	registry.ClientController:Emit("Notification", {
		Kind = "Info",
		Text = if firstPerson then "Camera: Immersive First-Person" else "Camera: Over-The-Shoulder Third-Person",
	})
end

function CameraController:Start()
	self:ApplyCameraMode()

	registry.ClientController:On("CombatFeedback", function(data)
		if type(data) == "table" and data.Kind == "PlayerStruck" then
			self:AddDamageKick(1.2)
		end
	end)

	UserInputService.InputBegan:Connect(function(input: InputObject, gameProcessed: boolean)
		if gameProcessed then
			return
		end
		if input.KeyCode == Enum.KeyCode.V then
			self:TogglePerspective()
		end
	end)

	RunService:BindToRenderStep("AshesSurvivalCamera", Enum.RenderPriority.Camera.Value + 5, function(dt: number)
		local cam = Workspace.CurrentCamera
		if not cam then
			return
		end

		local settings = registry.ClientController.State.Settings
		if not isADS then
			targetFOV = settings.FOV or 78
		end

		currentFOV += (targetFOV - currentFOV) * math.min(1, dt * 11.0)
		cam.FieldOfView = currentFOV

		-- Recover recoil & damage roll smoothly
		local stepPitch = recoilPitch * math.min(1, dt * 14.0)
		local stepYaw = recoilYaw * math.min(1, dt * 14.0)
		recoilPitch -= stepPitch
		recoilYaw -= stepYaw
		damageRollKick *= math.max(0, 1 - dt * 7.5)

		local bobOffset = Vector3.zero
		local breathPitch = 0.0

		if not settings.ReduceMotion then
			local char = LocalPlayer.Character
			local root = if char then char:FindFirstChild("HumanoidRootPart") :: BasePart? else nil
			local speed = if root
				then Vector3.new(root.AssemblyLinearVelocity.X, 0, root.AssemblyLinearVelocity.Z).Magnitude
				else 0
			local stressRatio = (registry.ClientController.State.Vitals.MentalStress or 0) / 100.0

			if speed > 1.2 then
				bobPhase += dt * (4.8 + speed * 0.42)
				local amp = math.clamp(speed / 24.0, 0.02, 0.11) * (if isADS then 0.35 else 1.0)
				bobOffset = Vector3.new(math.cos(bobPhase * 0.5) * amp * 0.65, math.sin(bobPhase) * amp, 0)
			else
				bobPhase += dt * (1.6 + stressRatio * 2.4)
				breathPitch = math.sin(bobPhase) * (0.002 + stressRatio * 0.004) * (if isADS then 0.45 else 1.0)
			end
		end

		-- Third-person subtle over-the-shoulder framing offset
		local shoulderOffset = if not firstPerson then Vector3.new(1.65, 0.45, 0) else Vector3.zero

		cam.CFrame = cam.CFrame
			* CFrame.new(shoulderOffset + bobOffset)
			* CFrame.Angles(stepPitch + breathPitch, stepYaw, damageRollKick)
	end)
end

return CameraController
