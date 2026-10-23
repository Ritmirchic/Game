--!strict
--[[
    ASHES OF HUMANITY: LAST SIGNAL
    StarterPlayerScripts.Controllers.AnimationController
    Hybrid Animator/AnimationTrack + Real-Time Procedural Motor6D Kinematics
    for Survivor Stances, Weapon Handling, Injury Limps, and Mutant Species Rigs (Section 13).
]]

local Players = game:GetService("Players")
local ReplicatedStorage = game:GetService("ReplicatedStorage")
local RunService = game:GetService("RunService")
local Workspace = game:GetService("Workspace")

local AnimationDefinitions = require(ReplicatedStorage.Shared.AnimationDefinitions)

local AnimationController = {}

local LocalPlayer = Players.LocalPlayer
local registry: any = nil
local actionState: string? = nil
local actionUntil = 0.0
local animPhase = 0.0

local mutantBaseOffsets: { [Instance]: CFrame } = {}

function AnimationController:Init(clientRegistry: any)
	registry = clientRegistry
end

function AnimationController:PlayAction(actionName: string, duration: number?)
	local prof = (AnimationDefinitions.PLAYER_STATES :: any)[actionName]
	local dur = duration or (if prof then prof.Duration else 0.6)
	actionState = actionName
	actionUntil = os.clock() + dur

	local pubId = AnimationDefinitions.PublishedIds.Player[actionName]
	local char = LocalPlayer.Character
	local hum = if char then char:FindFirstChildOfClass("Humanoid") else nil
	local animator = if hum then hum:FindFirstChildOfClass("Animator") else nil
	if animator and type(pubId) == "string" and pubId ~= "" then
		pcall(function()
			local anim = Instance.new("Animation")
			anim.AnimationId = pubId
			local track = animator:LoadAnimation(anim)
			track.Priority = Enum.AnimationPriority.Action
			track:Play(0.12)
		end)
	end
end

local function updateSurvivorProceduralJoints(dt: number)
	local char = LocalPlayer.Character
	if not char then
		return
	end
	local stance = if registry.MovementController then registry.MovementController:GetStance() else "Walk"
	local activeKey = if actionState and os.clock() < actionUntil then actionState else stance
	local prof = (AnimationDefinitions.PLAYER_STATES :: any)[activeKey] or AnimationDefinitions.PLAYER_STATES.Idle

	animPhase += dt * prof.StrideFreq * math.pi * 2

	local upperTorso = char:FindFirstChild("UpperTorso")
	local waist = if upperTorso then upperTorso:FindFirstChild("Waist") :: Motor6D? else nil
	if waist then
		local targetTilt = math.rad(prof.TorsoPitchDeg)
		local limpRoll = 0
		local br = registry.ClientController.State.BodyRegions
		if br and br.LeftLeg and br.LeftLeg.HP < 35 then
			limpRoll = math.sin(animPhase) * math.rad(5.5)
		end
		waist.Transform = waist.Transform:Lerp(CFrame.Angles(targetTilt, 0, limpRoll), math.min(1, dt * 10))
	end

	local rightUpperArm = char:FindFirstChild("RightUpperArm")
	local rShoulder = if rightUpperArm then rightUpperArm:FindFirstChild("RightShoulder") :: Motor6D? else nil
	if rShoulder then
		local isADS = registry.CameraController and registry.CameraController:IsADS()
		local armPitch = math.rad(prof.RightArmPitchDeg + (if isADS then 18 else 0))
		local reloadSwing = if activeKey == "Reload" then math.sin(animPhase * 2.0) * math.rad(22) else 0
		rShoulder.Transform =
			rShoulder.Transform:Lerp(CFrame.Angles(armPitch + reloadSwing, 0, 0), math.min(1, dt * 12))
	end
end

local function animateMutantModel(model: Model, camPos: Vector3, now: number)
	local root = model.PrimaryPart
	if not root then
		return
	end
	if (root.Position - camPos).Magnitude > 165 then
		return
	end

	local animState = tostring(model:GetAttribute("AnimState") or "Idle")
	local mProf = (AnimationDefinitions.MUTANT_STATES :: any)[animState] or AnimationDefinitions.MUTANT_STATES.Idle
	local seed = (tonumber(string.byte(model.Name, 1)) or 7) * 0.37
	local phase = now * mProf.StrideFreq * math.pi * 2 + seed

	local strideRad = math.rad(mProf.LegSwingDeg)
	local armRad = math.rad(mProf.ArmSwingDeg)
	local pitchRad = math.rad(mProf.HeadPitchDeg)

	for _, desc in ipairs(model:GetChildren()) do
		if desc:IsA("Weld") or desc:IsA("Motor6D") then
			local base = mutantBaseOffsets[desc]
			if not base then
				base = desc.C0
				mutantBaseOffsets[desc] = base
			end
			local jName = string.lower(desc.Name)
			if string.find(jName, "leg") then
				local sign = if string.find(jName, "left") then 1 else -1
				desc.C0 = base * CFrame.Angles(math.sin(phase) * strideRad * sign, 0, 0)
			elseif string.find(jName, "arm") then
				local sign = if string.find(jName, "left") then -1 else 1
				local atkBoost = if animState == "Attack" then math.abs(math.sin(phase * 1.4)) * 1.1 else 0
				desc.C0 = base * CFrame.Angles(math.sin(phase) * armRad * sign - atkBoost, 0, 0)
			elseif string.find(jName, "head") or string.find(jName, "neck") then
				desc.C0 = base
					* CFrame.Angles(
						pitchRad,
						math.sin(phase * 0.5) * math.rad(mProf.HeadYawOscDeg),
						math.cos(phase * 0.7) * 0.10
					)
			end
		end
	end

	if animState == "Death" then
		model:PivotTo(CFrame.new(root.Position) * CFrame.Angles(math.rad(82), 0, 0))
	end
end

function AnimationController:Start()
	registry.ClientController:On("CombatFeedback", function(data)
		if type(data) ~= "table" then
			return
		end
		if data.Kind == "ReloadStarted" then
			self:PlayAction("Reload", data.Duration or 2.0)
		elseif data.Kind == "MeleeHit" then
			self:PlayAction("MeleeAttack", 0.45)
		elseif data.Kind == "PlayerStruck" then
			self:PlayAction("InjuredMove", 0.35)
		end
	end)

	RunService.RenderStepped:Connect(function(dt: number)
		updateSurvivorProceduralJoints(dt)

		local cam = Workspace.CurrentCamera
		local enemiesFolder = Workspace:FindFirstChild("ActiveEnemies")
		if cam and enemiesFolder then
			local now = os.clock()
			local camPos = cam.CFrame.Position
			for _, child in ipairs(enemiesFolder:GetChildren()) do
				if child:IsA("Model") then
					animateMutantModel(child, camPos, now)
				end
			end
		end
	end)
end

return AnimationController
