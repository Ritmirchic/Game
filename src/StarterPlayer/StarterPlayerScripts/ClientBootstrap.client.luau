--!strict
--[[
    ASHES OF HUMANITY: LAST SIGNAL
    StarterPlayerScripts.ClientBootstrap
    Master Client Entry Point — Initializes and Starts All 8 Client Controllers.
]]

local StarterGui = game:GetService("StarterGui")

local Controllers = script.Parent:WaitForChild("Controllers")
local ClientController = require(Controllers.ClientController)
local MovementController = require(Controllers.MovementController)
local CameraController = require(Controllers.CameraController)
local AnimationController = require(Controllers.AnimationController)
local WeaponController = require(Controllers.WeaponController)
local AudioController = require(Controllers.AudioController)
local EffectsController = require(Controllers.EffectsController)
local UIController = require(Controllers.UIController)

-- Hide default cartoon Roblox Backpack/Healthbar CoreGuis for grounded survival immersion
pcall(function()
	StarterGui:SetCoreGuiEnabled(Enum.CoreGuiType.Backpack, false)
	StarterGui:SetCoreGuiEnabled(Enum.CoreGuiType.Health, false)
end)

local ClientRegistry = {
	ClientController = ClientController,
	MovementController = MovementController,
	CameraController = CameraController,
	AnimationController = AnimationController,
	WeaponController = WeaponController,
	AudioController = AudioController,
	EffectsController = EffectsController,
	UIController = UIController,
}

local orderedControllers: { any } = {
	ClientController,
	AudioController,
	EffectsController,
	CameraController,
	MovementController,
	AnimationController,
	WeaponController,
	UIController,
}

for _, ctrl in ipairs(orderedControllers) do
	ctrl:Init(ClientRegistry)
end

for _, ctrl in ipairs(orderedControllers) do
	ctrl:Start()
end
