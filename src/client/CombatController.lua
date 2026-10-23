--!strict
--[[
	CombatController — client half of the weapon system.

	This module never decides damage. It reads the equipped weapon, builds the
	aim ray for feedback, applies recoil / sway / muzzle flash locally, sends the
	fire request, and plays reload and hit feedback.
]]

local Players = game:GetService("Players")
local UserInputService = game:GetService("UserInputService")
local RunService = game:GetService("RunService")
local ReplicatedStorage = game:GetService("ReplicatedStorage")
local TweenService = game:GetService("TweenService")

local Store = require(script.Parent.StateStore)
local Network = require(script.Parent.Network)
local CameraController = require(script.Parent.CameraController)

local Shared = ReplicatedStorage:WaitForChild("Shared")
local Weapons = require(Shared.WeaponDefinitions)
local Remotes = require(Shared.Remotes)

local Combat = {}

local player = Players.LocalPlayer
local ammoState = {}        -- weaponId -> { magazine, reserve }
local lastShot = 0
local recoil = Vector2.new(0, 0)
local reloading = false
local weapon = nil

function Combat.GetWeaponId()
	local inventory = Store.get("inventory")
	local equipment = inventory.equipment or {}
	local slot = equipment.Primary or equipment.Secondary or equipment.Melee
	return slot and slot.itemId or nil
end

function Combat.GetAmmo()
	local id = Combat.GetWeaponId()
	return id and ammoState[id] or nil
end

local function applyRecoil(dir)
	recoil += dir
end

local function raycastOrigin()
	local character = player.Character
	local hrp = character and character:FindFirstChild("HumanoidRootPart")
	local head = character and character:FindFirstChild("Head")
	if not hrp then return nil end
	local origin = (head and head.Position) or hrp.Position
	return origin + workspace.CurrentCamera.CFrame.LookVector * 1.2
end

function Combat.TryFire()
	local weaponId = Combat.GetWeaponId()
	if not weaponId then return false end
	local def = Weapons[weaponId]
	if not def or def.kind ~= "weapon" then return false end
	if reloading then return false end
	local ammo = ammoState[weaponId] or { magazine = def.magazine, reserve = 0 }
	local now = os.clock()
	if now - lastShot < def.fireRate then return false end
	if def.auto ~= true then
		-- semi auto: require a fresh click
	end
	if ammo.magazine <= 0 then
		Network.fire("RequestReload")
		Combat.Reload()
		return false
	end
	lastShot = now
	ammo.magazine -= 1
	ammoState[weaponId] = ammo

	local origin = raycastOrigin()
	local direction = workspace.CurrentCamera.CFrame.LookVector
	Network.fire("RequestAttack", direction, origin)

	-- local feedback
	local kick = def.recoil or 1
	applyRecoil(Vector2.new(math.random() * 0.6 - 0.3, kick * 0.45))
	CameraController.Shake(0.35 + kick * 0.15)
	Store.set("ammo", ammo)
	Combat.PlayMuzzle()
	return true
end

function Combat.PlayMuzzle()
	local character = player.Character
	if not character then return end
	local hrp = character:FindFirstChild("HumanoidRootPart")
	if not hrp then return end
	local flash = Instance.new("Part")
	flash.Size = Vector3.new(0.25, 0.25, 0.25)
	flash.Transparency = 0.2
	flash.Material = Enum.Material.Neon
	flash.Color = Color3.fromRGB(255, 196, 120)
	flash.CanCollide = false
	flash.Anchored = true
	flash.CFrame = workspace.CurrentCamera.CFrame * CFrame.new(1.1, -0.75, -2)
	flash.Parent = workspace
	local light = Instance.new("PointLight")
	light.Brightness = 4
	light.Range = 18
	light.Color = flash.Color
	light.Parent = flash
	task.delay(0.045, function()
		flash:Destroy()
	end)
end

function Combat.Reload()
	local weaponId = Combat.GetWeaponId()
	if not weaponId then return end
	local def = Weapons[weaponId]
	if not def or reloading then return end
	reloading = true
	task.delay(def.reload or 1.8, function()
		reloading = false
	end)
	Store.notify("Reloading...", "info", 1.4)
end

function Combat.OnCombatFeedback(kind, data)
	if kind == "hit" then
		Store.notify(data and data.kill and "Kill confirmed." or "Hit.", "combat", 1.2)
	elseif kind == "attacked" then
		CameraController.Shake(1.4)
	elseif kind == "ammo" then
		local weaponId = data and data.weapon
		if weaponId then ammoState[weaponId] = data.ammo end
		Store.set("ammo", data and data.ammo or nil)
	end
end

function Combat.SetAmmo(weaponId, magazine, reserve)
	ammoState[weaponId] = { magazine = magazine, reserve = reserve }
	Store.set("ammo", ammoState[weaponId])
end

function Combat.Start()
	UserInputService.InputBegan:Connect(function(input, processed)
		if processed then return end
		if input.UserInputType == Enum.UserInputType.MouseButton1 then
			Combat.TryFire()
		elseif input.KeyCode == Enum.KeyCode.R then
			Network.fire("RequestReload")
		elseif input.KeyCode == Enum.KeyCode.One then
			Network.fire("RequestEquip", "Primary")
		elseif input.KeyCode == Enum.KeyCode.Two then
			Network.fire("RequestEquip", "Secondary")
		elseif input.KeyCode == Enum.KeyCode.Three then
			Network.fire("RequestEquip", "Melee")
		end
	end)
	-- automatic fire while held
	local holding = false
	UserInputService.InputBegan:Connect(function(input, processed)
		if not processed and input.UserInputType == Enum.UserInputType.MouseButton1 then
			holding = true
			task.spawn(function()
				while holding do
					local weaponId = Combat.GetWeaponId()
					local def = weaponId and Weapons[weaponId]
					if not def or def.auto ~= true then break end
					Combat.TryFire()
					task.wait(def.fireRate)
				end
			end)
		end
	end)
	UserInputService.InputEnded:Connect(function(input)
		if input.UserInputType == Enum.UserInputType.MouseButton1 then
			holding = false
		end
	end)

	RunService.RenderStepped:Connect(function(dt)
		if recoil.Magnitude > 0.01 then
			local camera = workspace.CurrentCamera
			camera.CFrame = camera.CFrame * CFrame.Angles(recoil.Y * 0.01, recoil.X * 0.01, 0)
			recoil = recoil * math.max(0, 1 - dt * 9)
		end
	end)

	Store.on("inventory", function()
		local id = Combat.GetWeaponId()
		if id and not ammoState[id] then
			local def = Weapons[id]
			if def then
				ammoState[id] = { magazine = def.magazine or 0, reserve = def.reserve or 0 }
				Store.set("ammo", ammoState[id])
			end
		end
	end)
end

return Combat
