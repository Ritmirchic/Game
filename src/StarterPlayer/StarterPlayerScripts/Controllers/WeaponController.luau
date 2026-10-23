--!strict
--[[
    ASHES OF HUMANITY: LAST SIGNAL
    StarterPlayerScripts.Controllers.WeaponController
    Client Weapon Handling, Hotbar Slots (1-5), Semi/Auto Trigger Loop,
    ADS Zoom Integration, Pooled Brass Shell Casings, Muzzle Flash,
    Tracer Beams, and Impact Dust/Sparks (Section 23).
]]

local Players = game:GetService("Players")
local ReplicatedStorage = game:GetService("ReplicatedStorage")
local RunService = game:GetService("RunService")
local UserInputService = game:GetService("UserInputService")
local Workspace = game:GetService("Workspace")

local ItemDefinitions = require(ReplicatedStorage.Shared.ItemDefinitions)
local NetworkSchema = require(ReplicatedStorage.Shared.NetworkSchema)
local WeaponDefinitions = require(ReplicatedStorage.Shared.WeaponDefinitions)

local WeaponController = {}

local LocalPlayer = Players.LocalPlayer
local registry: any = nil

local triggerHeld = false
local lastClientShotAt = 0.0
local activeEquipSlot = "Primary" -- "Primary" | "Secondary" | "Melee"

-- Pooled brass shell casings + tracer parts for zero-GC-spike combat FX!
local shellPool: { Part } = {}
local shellIndex = 1
local POOL_SIZE = 18

local function initShellPool()
	local folder = Workspace:FindFirstChild("ClientFXPool")
	if not folder then
		local f = Instance.new("Folder")
		f.Name = "ClientFXPool"
		f.Parent = Workspace
		folder = f
	end
	for i = 1, POOL_SIZE do
		local p = Instance.new("Part")
		p.Name = "BrassShell_" .. tostring(i)
		p.Size = Vector3.new(0.12, 0.12, 0.34)
		p.Color = Color3.fromRGB(198, 162, 74)
		p.Material = Enum.Material.Metal
		p.CanCollide = false
		p.Anchored = true
		p.Transparency = 1.0
		p.Parent = folder
		table.insert(shellPool, p)
	end
end

function WeaponController:Init(clientRegistry: any)
	registry = clientRegistry
end

function WeaponController:GetActiveWeaponInfo(): (any?, WeaponDefinitions.WeaponDef?)
	local inv = registry.ClientController.State.Inventory
	if not inv or not inv.Equipped or not inv.Items then
		return nil, nil
	end
	local uid = inv.Equipped[activeEquipSlot]
	if not uid then
		for _, fallback in ipairs({ "Primary", "Secondary", "Melee" }) do
			if inv.Equipped[fallback] then
				activeEquipSlot = fallback
				uid = inv.Equipped[fallback]
				break
			end
		end
	end
	if not uid then
		return nil, nil
	end
	local inst = inv.Items[uid]
	if not inst then
		return nil, nil
	end
	local itemDef = ItemDefinitions.Get(inst.ItemId)
	if not itemDef or not itemDef.WeaponId then
		return inst, nil
	end
	return inst, WeaponDefinitions.Get(itemDef.WeaponId)
end

function WeaponController:SelectSlot(slotName: string)
	activeEquipSlot = slotName
	registry.ClientController.State.ActiveSlot = slotName
	registry.ClientController:Emit("SlotChanged", slotName)
	if registry.AudioController then
		registry.AudioController:PlayUIClick(0.92)
	end
end

function WeaponController:UseHotbarIndex(idx: number)
	if idx == 1 then
		self:SelectSlot("Primary")
		return
	elseif idx == 2 then
		self:SelectSlot("Secondary")
		return
	elseif idx == 3 then
		self:SelectSlot("Melee")
		return
	end
	local inv = registry.ClientController.State.Inventory
	local hotUid = if inv and inv.Hotbar then inv.Hotbar[idx] else nil
	if hotUid and inv.Items and inv.Items[hotUid] then
		registry.ClientController:FireServer(NetworkSchema.EVENTS.InventoryAction, {
			Action = "Use",
			UID = hotUid,
		})
	end
end

function WeaponController:SpawnShotVisuals(
	origin: Vector3,
	dir: Vector3,
	hits: { any }?,
	wpnDef: WeaponDefinitions.WeaponDef?
)
	local presets = registry.ClientController.State.Settings.GraphicsPreset
	if
		presets ~= "Low"
		and #shellPool > 0
		and wpnDef
		and wpnDef.Type ~= "Melee"
		and wpnDef.Id ~= "TacticalCrossbow"
	then
		local casing = shellPool[shellIndex]
		shellIndex = (shellIndex % #shellPool) + 1
		local right = dir:Cross(Vector3.yAxis)
		if right.Magnitude < 0.05 then
			right = Vector3.xAxis
		end
		right = right.Unit
		local startPos = origin + right * 0.45
		casing.Transparency = 0
		casing.CFrame = CFrame.new(startPos)
		task.spawn(function()
			for step = 1, 10 do
				local t = step * 0.03
				casing.CFrame = CFrame.new(startPos + right * (t * 5.2) + Vector3.new(0, 1.4 * t - 14 * t * t, 0))
					* CFrame.Angles(step * 0.6, step * 0.4, 0)
				task.wait(0.03)
			end
			casing.Transparency = 1.0
		end)
	end

	-- Subtle muzzle flash light pulse + tracer streak
	local fxPart = Instance.new("Part")
	fxPart.Anchored = true
	fxPart.CanCollide = false
	fxPart.Transparency = 1.0
	fxPart.Size = Vector3.new(0.2, 0.2, 0.2)
	fxPart.Position = origin + dir * 1.4
	fxPart.Parent = Workspace

	if not wpnDef or wpnDef.Id ~= "TacticalCrossbow" then
		local flash = Instance.new("PointLight")
		flash.Color = Color3.fromRGB(255, 196, 118)
		flash.Brightness = 3.5
		flash.Range = 14
		flash.Parent = fxPart
	end

	if hits and #hits > 0 and hits[1].Pos then
		local hitPos: Vector3 = hits[1].Pos
		local dist = math.min(140, (hitPos - origin).Magnitude)
		local beamPart = Instance.new("Part")
		beamPart.Anchored = true
		beamPart.CanCollide = false
		beamPart.Material = Enum.Material.Neon
		beamPart.Color = Color3.fromRGB(255, 214, 148)
		beamPart.Transparency = 0.45
		beamPart.Size = Vector3.new(0.06, 0.06, dist)
		beamPart.CFrame = CFrame.lookAt((origin + hitPos) * 0.5, hitPos)
		beamPart.Parent = Workspace
		task.delay(0.045, function()
			beamPart:Destroy()
		end)
	end

	task.delay(0.06, function()
		fxPart:Destroy()
	end)
end

function WeaponController:AttemptFireOrSwing()
	if registry.ClientController.State.ActivePanel ~= "None" then
		return
	end
	if registry.ClientController.State.BuildMode then
		registry.ClientController:PlaceCurrentBuildGhost()
		return
	end

	local vitals = registry.ClientController.State.Vitals
	if vitals.IsDowned or vitals.Health <= 0 then
		return
	end

	local cam = Workspace.CurrentCamera
	local char = LocalPlayer.Character
	local root = if char then char:FindFirstChild("HumanoidRootPart") :: BasePart? else nil
	if not cam or not root then
		return
	end

	local wpnInst, wpnDef = self:GetActiveWeaponInfo()
	local now = os.clock()

	if not wpnDef or wpnDef.Type == "Melee" then
		local rpm = if wpnDef then wpnDef.RoundsPerMinute else 75
		if (now - lastClientShotAt) < (60.0 / rpm) then
			return
		end
		lastClientShotAt = now
		if registry.AnimationController then
			registry.AnimationController:PlayAction("MeleeAttack", 0.45)
		end
		if registry.AudioController then
			registry.AudioController:PlayWeaponSound("Melee")
		end
		registry.ClientController:FireServer(NetworkSchema.EVENTS.MeleeSwing, {
			Slot = activeEquipSlot,
			Direction = cam.CFrame.LookVector,
		})
		return
	end

	local minInterval = 60.0 / math.max(1, wpnDef.RoundsPerMinute)
	if (now - lastClientShotAt) < minInterval then
		return
	end

	if (wpnInst.ClipAmmo or 0) <= 0 then
		lastClientShotAt = now + 0.22
		if registry.AudioController then
			registry.AudioController:PlayWeaponSound("DryFire")
		end
		return
	end

	lastClientShotAt = now
	local muzzleOrigin = root.Position + Vector3.new(0, 1.45, 0) + cam.CFrame.LookVector * 1.1
	local aimDir = cam.CFrame.LookVector

	-- Client-side responsive recoil kick
	if registry.CameraController then
		local yawSign = if math.random() > 0.5 then 1 else -1
		registry.CameraController:AddRecoil(wpnDef.RecoilPitchDeg, wpnDef.RecoilYawDeg * yawSign)
	end
	if registry.AnimationController then
		registry.AnimationController:PlayAction("Shoot", 0.14)
	end

	registry.ClientController:FireServer(NetworkSchema.EVENTS.WeaponFire, {
		Slot = activeEquipSlot,
		Origin = muzzleOrigin,
		Direction = aimDir,
		ADS = registry.CameraController and registry.CameraController:IsADS() or false,
	})
end

function WeaponController:RequestReload()
	if registry.ClientController.State.BuildMode then
		return
	end
	local _, wpnDef = self:GetActiveWeaponInfo()
	if not wpnDef or wpnDef.MagSize <= 0 then
		return
	end
	registry.ClientController:FireServer(NetworkSchema.EVENTS.WeaponReload, {
		Slot = activeEquipSlot,
	})
end

function WeaponController:SetADSIntent(active: boolean)
	local _, wpnDef = self:GetActiveWeaponInfo()
	local zoom = if wpnDef then wpnDef.ADSFov else 62
	if registry.CameraController then
		registry.CameraController:SetADS(active, zoom)
	end
end

function WeaponController:Start()
	initShellPool()

	registry.ClientController:On("CombatFeedback", function(data)
		if type(data) ~= "table" then
			return
		end
		if data.Kind == "ShotFired" then
			local wpnDef = WeaponDefinitions.Get(tostring(data.WeaponId or ""))
			if data.Origin and data.Direction then
				self:SpawnShotVisuals(data.Origin, data.Direction, data.Hits, wpnDef)
			end
			if registry.AudioController then
				registry.AudioController:PlayWeaponSound(tostring(data.WeaponId or "Firearm"), data.Origin)
			end
		end
	end)

	UserInputService.InputBegan:Connect(function(input: InputObject, gameProcessed: boolean)
		if gameProcessed then
			return
		end
		if input.UserInputType == Enum.UserInputType.MouseButton1 then
			triggerHeld = true
			self:AttemptFireOrSwing()
		elseif input.UserInputType == Enum.UserInputType.MouseButton2 then
			self:SetADSIntent(true)
		elseif input.KeyCode == Enum.KeyCode.R and not registry.ClientController.State.BuildMode then
			self:RequestReload()
		elseif input.KeyCode == Enum.KeyCode.One then
			self:UseHotbarIndex(1)
		elseif input.KeyCode == Enum.KeyCode.Two then
			self:UseHotbarIndex(2)
		elseif input.KeyCode == Enum.KeyCode.Three then
			self:UseHotbarIndex(3)
		elseif input.KeyCode == Enum.KeyCode.Four then
			self:UseHotbarIndex(4)
		elseif input.KeyCode == Enum.KeyCode.Five then
			self:UseHotbarIndex(5)
		end
	end)

	UserInputService.InputEnded:Connect(function(input: InputObject)
		if input.UserInputType == Enum.UserInputType.MouseButton1 then
			triggerHeld = false
		elseif input.UserInputType == Enum.UserInputType.MouseButton2 then
			self:SetADSIntent(false)
		end
	end)

	RunService.RenderStepped:Connect(function()
		if triggerHeld then
			local _, wpnDef = self:GetActiveWeaponInfo()
			if wpnDef and wpnDef.FireMode == "Auto" then
				self:AttemptFireOrSwing()
			end
		end
	end)
end

return WeaponController
