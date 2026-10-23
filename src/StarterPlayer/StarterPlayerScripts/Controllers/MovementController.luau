--!strict
--[[
    ASHES OF HUMANITY: LAST SIGNAL
    StarterPlayerScripts.Controllers.MovementController
    Responsive Survivor Locomotion: Walk, Sprint, Crouch, Crawl, Jump,
    Physics Obstacle Vaulting, Ladder Climbing, Swimming, Flashlight Toggle,
    and Surface-Dependent Footsteps (Section 12).
]]

local Players = game:GetService("Players")
local ReplicatedStorage = game:GetService("ReplicatedStorage")
local RunService = game:GetService("RunService")
local UserInputService = game:GetService("UserInputService")
local Workspace = game:GetService("Workspace")

local Config = require(ReplicatedStorage.Shared.Config)
local NetworkSchema = require(ReplicatedStorage.Shared.NetworkSchema)

local MovementController = {}

local LocalPlayer = Players.LocalPlayer
local registry: any = nil
local currentStance = "Walk"
local sprintHeld = false
local lastVaultAt = 0
local stepAccumulator = 0

function MovementController:Init(clientRegistry: any)
	registry = clientRegistry
end

function MovementController:GetStance(): string
	return currentStance
end

function MovementController:SetStance(newStance: string)
	if currentStance == newStance then
		return
	end
	currentStance = newStance
	registry.ClientController:FireServer(NetworkSchema.EVENTS.MovementStance, {
		Stance = newStance,
	})
	registry.ClientController:Emit("StanceChanged", newStance)

	-- Apply responsive HipHeight visual lowering on Crouch/Crawl with stand-up ceiling clearance check
	local char = LocalPlayer.Character
	local hum = if char then char:FindFirstChildOfClass("Humanoid") else nil
	if hum then
		if newStance == "Crawl" then
			hum.CameraOffset = Vector3.new(0, -2.1, 0)
		elseif newStance == "Crouch" then
			hum.CameraOffset = Vector3.new(0, -1.15, 0)
		else
			hum.CameraOffset = Vector3.zero
		end
	end
end

function MovementController:HasCeilingClearanceToStand(): boolean
	local char = LocalPlayer.Character
	local root = if char then char:FindFirstChild("HumanoidRootPart") :: BasePart? else nil
	if not root or not char then
		return true
	end
	local params = RaycastParams.new()
	params.FilterType = Enum.RaycastFilterType.Exclude
	params.FilterDescendantsInstances = { char }
	local hit = Workspace:Raycast(root.Position, Vector3.new(0, 3.8, 0), params)
	return hit == nil
end

function MovementController:TryVaultObstacle(): boolean
	local now = os.clock()
	if (now - lastVaultAt) < 0.95 then
		return false
	end
	local vitals = registry.ClientController.State.Vitals
	if vitals.Stamina < Config.MOVEMENT.VaultStaminaCost or vitals.IsDowned then
		return false
	end

	local char = LocalPlayer.Character
	local root = if char then char:FindFirstChild("HumanoidRootPart") :: BasePart? else nil
	local hum = if char then char:FindFirstChildOfClass("Humanoid") else nil
	if not root or not hum or not char then
		return false
	end

	local look = Vector3.new(root.CFrame.LookVector.X, 0, root.CFrame.LookVector.Z)
	if look.Magnitude < 0.1 then
		return false
	end
	look = look.Unit

	local params = RaycastParams.new()
	params.FilterType = Enum.RaycastFilterType.Exclude
	params.FilterDescendantsInstances = { char }

	-- Waist-height obstacle probe + chest-above-obstacle clearance probe!
	local waistHit =
		Workspace:Raycast(root.Position - Vector3.new(0, 0.5, 0), look * Config.MOVEMENT.VaultCheckDist, params)
	local upperClear = Workspace:Raycast(
		root.Position + Vector3.new(0, Config.MOVEMENT.VaultMaxHeight, 0),
		look * (Config.MOVEMENT.VaultCheckDist + 1.5),
		params
	)

	if waistHit and not upperClear then
		lastVaultAt = now
		self:SetStance("Vault")

		local att = root:FindFirstChild("VaultAttachment") :: Attachment?
		if not att then
			local a = Instance.new("Attachment")
			a.Name = "VaultAttachment"
			a.Parent = root
			att = a
		end

		local lv = Instance.new("LinearVelocity")
		lv.Attachment0 = att
		lv.MaxForce = 32000
		lv.VectorVelocity = look * 20 + Vector3.new(0, 18, 0)
		lv.Parent = root

		task.delay(0.24, function()
			if lv and lv.Parent then
				lv:Destroy()
			end
			if currentStance == "Vault" then
				self:SetStance(if sprintHeld then "Sprint" else "Walk")
			end
		end)
		return true
	end
	return false
end

function MovementController:SetSprintIntent(active: boolean)
	sprintHeld = active
	if active then
		if (currentStance == "Crouch" or currentStance == "Crawl") and not self:HasCeilingClearanceToStand() then
			return
		end
		if registry.ClientController.State.Vitals.Stamina > 6 then
			self:SetStance("Sprint")
		end
	else
		if currentStance == "Sprint" then
			self:SetStance("Walk")
		end
	end
end

function MovementController:ToggleCrouch()
	if currentStance == "Crouch" then
		if self:HasCeilingClearanceToStand() then
			self:SetStance("Walk")
		end
	else
		sprintHeld = false
		self:SetStance("Crouch")
	end
end

function MovementController:ToggleCrawl()
	if currentStance == "Crawl" then
		if self:HasCeilingClearanceToStand() then
			self:SetStance("Walk")
		else
			self:SetStance("Crouch")
		end
	else
		sprintHeld = false
		self:SetStance("Crawl")
	end
end

function MovementController:ToggleFlashlight()
	local vitals = registry.ClientController.State.Vitals
	if (vitals.FlashlightBattery or 0) > 0 then
		vitals.FlashlightOn = not vitals.FlashlightOn
		registry.ClientController:Emit("VitalsChanged", vitals)
	end
	registry.ClientController:FireServer(NetworkSchema.EVENTS.ToggleFlashlight, {})
	if registry.AudioController then
		registry.AudioController:PlayUIClick(1.25)
	end
end

function MovementController:Start()
	UserInputService.InputBegan:Connect(function(input: InputObject, gameProcessed: boolean)
		if gameProcessed then
			return
		end
		if input.KeyCode == Enum.KeyCode.LeftShift then
			self:SetSprintIntent(true)
		elseif input.KeyCode == Enum.KeyCode.C then
			self:ToggleCrouch()
		elseif input.KeyCode == Enum.KeyCode.Z then
			self:ToggleCrawl()
		elseif input.KeyCode == Enum.KeyCode.Space then
			-- Attempt low-wall / window vault first; otherwise standard jump runs
			self:TryVaultObstacle()
		elseif input.KeyCode == Enum.KeyCode.F then
			self:ToggleFlashlight()
		end
	end)

	UserInputService.InputEnded:Connect(function(input: InputObject)
		if input.KeyCode == Enum.KeyCode.LeftShift then
			self:SetSprintIntent(false)
		end
	end)

	RunService.Heartbeat:Connect(function(dt: number)
		local char = LocalPlayer.Character
		local hum = if char then char:FindFirstChildOfClass("Humanoid") else nil
		local root = if char then char:FindFirstChild("HumanoidRootPart") :: BasePart? else nil
		if not hum or not root then
			return
		end

		-- Detect swimming / climbing transitions automatically
		local hState = hum:GetState()
		if hState == Enum.HumanoidStateType.Swimming and currentStance ~= "Swim" then
			self:SetStance("Swim")
		elseif hState == Enum.HumanoidStateType.Climbing and currentStance ~= "Climb" then
			self:SetStance("Climb")
		elseif (currentStance == "Swim" or currentStance == "Climb") and hState == Enum.HumanoidStateType.Running then
			self:SetStance(if sprintHeld then "Sprint" else "Walk")
		end

		-- Auto-drop sprint when stamina bottoms out
		if currentStance == "Sprint" and registry.ClientController.State.Vitals.Stamina <= 2 then
			self:SetStance("Walk")
		end

		-- Trigger surface-aware footsteps
		local horizSpeed = Vector3.new(root.AssemblyLinearVelocity.X, 0, root.AssemblyLinearVelocity.Z).Magnitude
		if horizSpeed > 1.8 and hum.FloorMaterial ~= Enum.Material.Air then
			stepAccumulator += horizSpeed * dt
			local stride = if currentStance == "Sprint"
				then 7.2
				elseif currentStance == "Crouch" or currentStance == "Crawl" then 5.2
				else 6.0
			if stepAccumulator >= stride then
				stepAccumulator = 0
				if registry.AudioController then
					registry.AudioController:PlayFootstep(hum.FloorMaterial, currentStance)
				end
			end
		else
			stepAccumulator = 0
		end
	end)
end

return MovementController
