--!strict
--[[
	MovementController — sprint, crouch, crawl, vault, climb, stamina feedback.

	The server owns the authoritative stamina pool; this module only predicts the
	local character response (speed, height, camera offset) and asks the server to
	confirm the stance. Injury states come down through SurvivalUpdate.
]]

local Players = game:GetService("Players")
local UserInputService = game:GetService("UserInputService")
local ReplicatedStorage = game:GetService("ReplicatedStorage")
local RunService = game:GetService("RunService")

local Store = require(script.Parent.StateStore)
local Network = require(script.Parent.Network)
local Theme = require(script.Parent.UITheme)

local Config = require(ReplicatedStorage:WaitForChild("Shared"):WaitForChild("Config"))

local Movement = {}

local player = Players.LocalPlayer
local sprinting = false
local crouching = false
local climbing = false
local staminaDraw = 0
local vaultDebounce = 0
local lastSprintToggle = 0
local baseWalkSpeed = 12

local SPRINT_SPEED = 22
local CROUCH_SPEED = 5.5
local CRAWL_SPEED = 2.6
local CLIMB_SPEED = 7

local function humanoid()
	local character = player.Character
	return character and character:FindFirstChildOfClass("Humanoid")
end

local function root()
	local character = player.Character
	return character and character:FindFirstChild("HumanoidRootPart")
end

function Movement.SetStance(newSprinting, newCrouching)
	local hum = humanoid()
	local survival = Store.get("survival")
	local injured = (survival.conditions and (survival.conditions.leg_injury or survival.conditions.sprained)) == true
	local exhausted = (survival.stamina or 100) <= 2
	sprinting = newSprinting and not crouching and not exhausted
	crouching = newCrouching
	Store.set("sprinting", sprinting)
	Store.set("crouching", crouching)
	if hum then
		local speed = baseWalkSpeed
		if injured then speed *= 0.72 end
		if sprinting then
			speed = SPRINT_SPEED
		elseif crouching then
			speed = CROUCH_SPEED
		elseif (survival.fatigue or 0) > 75 then
			speed = baseWalkSpeed * 0.82
		end
		if (survival.health or 100) < 25 then speed *= 0.85 end
		hum.WalkSpeed = speed
	end
end

function Movement.ToggleSprint()
	Movement.SetStance(not sprinting, crouching)
	Network.fire("RequestMove", { sprint = sprinting, crouch = crouching })
end

function Movement.ToggleCrouch()
	Movement.SetStance(sprinting, not crouching)
	Network.fire("RequestMove", { sprint = sprinting, crouch = crouching })
end

-- Vault: if a low obstacle is directly ahead and there is space beyond it,
-- give the character a short upward + forward impulse. Server validates speed.
function Movement.TryVault()
	local character = player.Character
	local hum = humanoid()
	local hrp = root()
	if not character or not hum or not hrp then return false end
	if os.clock() - vaultDebounce < 0.7 then return false end
	local survival = Store.get("survival")
	if (survival.stamina or 0) < 14 then return false end
	local look = hrp.CFrame.LookVector
	local params = RaycastParams.new()
	params.FilterType = Enum.RaycastFilterType.Exclude
	params.FilterDescendantsInstances = { character }
	local origin = hrp.Position + Vector3.new(0, -1.2, 0)
	local hits = workspace:Raycast(origin, look * 4.2, params)
	if not hits then return false end
	local top = hits.Position.Y + 0.2
	local headroom = workspace:Raycast(hrp.Position + Vector3.new(0, 3.4, 0), look * 4.5, params)
	if headroom then return false end
	local clearance = hits.Instance
	if not clearance or not clearance:IsA("BasePart") then return false end
	local height = top - hrp.Position.Y
	if height < 0.6 or height > 4.2 then return false end
	vaultDebounce = os.clock()
	hum:ChangeState(Enum.HumanoidStateType.Jumping)
	hrp.AssemblyLinearVelocity = look * 24 + Vector3.new(0, 26, 0)
	Network.fire("RequestMove", { vault = true })
	return true
end

-- Climb: while pressing space against a tall wall, ascend.
function Movement.TryClimb()
	local character = player.Character
	local hum = humanoid()
	local hrp = root()
	if not character or not hum or not hrp then return false end
	local survival = Store.get("survival")
	if (survival.stamina or 0) < 6 then return false end
	local look = hrp.CFrame.LookVector
	local params = RaycastParams.new()
	params.FilterType = Enum.RaycastFilterType.Exclude
	params.FilterDescendantsInstances = { character }
	local wall = workspace:Raycast(hrp.Position + Vector3.new(0, 1.4, 0), look * 2.4, params)
	if not wall then return false end
	local surfaceNormal = wall.Normal
	if math.abs(surfaceNormal.Y) > 0.5 then return false end
	climbing = true
	hrp.AssemblyLinearVelocity = Vector3.new(0, CLIMB_SPEED, 0) + look * 3
	Network.fire("RequestMove", { climb = true })
	task.delay(0.2, function() climbing = false end)
	return true
end

function Movement.IsSprinting() return sprinting end
function Movement.IsCrouching() return crouching end

function Movement.OnCharacter(character)
	local hum = character:WaitForChild("Humanoid")
	baseWalkSpeed = hum.WalkSpeed
	Movement.SetStance(false, false)
	hum.Died:Connect(function()
		Movement.SetStance(false, false)
	end)
end

function Movement.Start()
	Store.on("survival", function(survival)
		if sprinting and (survival.stamina or 0) <= 2 then
			Movement.SetStance(false, crouching)
			Store.notify("You are winded.", "warn", 2)
		end
	end)

	-- tell the server what the body is doing, twice a second, so stamina and
	-- temperature react to real movement rather than guesses
	task.spawn(function()
		while true do
			task.wait(0.5)
			local hrp = root()
			local moving = false
			if hrp then
				local velocity = hrp.AssemblyLinearVelocity
				moving = Vector3.new(velocity.X, 0, velocity.Z).Magnitude > 1.5
			end
			Network.fire("RequestMove", {
				sprint = sprinting, crouch = crouching, moving = moving,
			})
		end
	end)

	UserInputService.InputBegan:Connect(function(input, processed)
		if processed then return end
		local key = input.KeyCode
		if key == Enum.KeyCode.LeftShift then
			Movement.ToggleSprint()
		elseif key == Enum.KeyCode.LeftControl then
			Movement.ToggleCrouch()
		elseif key == Enum.KeyCode.Space then
			if not Movement.TryVault() then
				Movement.TryClimb()
			end
		end
	end)

	if player.Character then
		Movement.OnCharacter(player.Character)
	end
	player.CharacterAdded:Connect(function(character)
		task.wait(0.1)
		Movement.OnCharacter(character)
	end)
end

return Movement
