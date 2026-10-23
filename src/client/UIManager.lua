--!strict
--[[
	UIManager — owns the ScreenGui, the panel stack and all key bindings.

	Only one modal panel is open at a time. When a panel is open the character
	is frozen by the server (the client sends RequestMove with ui = true) and the
	mouse is unlocked.
]]

local Players = game:GetService("Players")
local UserInputService = game:GetService("UserInputService")
local GuiService = game:GetService("GuiService")

local Store = require(script.Parent.StateStore)
local Theme = require(script.Parent.UITheme)
local UI = require(script.Parent.UIBuilder)
local Network = require(script.Parent.Network)
local HUD = require(script.Parent.HUD)
local InventoryUI = require(script.Parent.InventoryUI)
local CraftingUI = require(script.Parent.CraftingUI)
local MedicalUI = require(script.Parent.MedicalUI)
local MapUI = require(script.Parent.MapUI)
local JournalUI = require(script.Parent.JournalUI)
local BuildingUI = require(script.Parent.BuildingUI)
local SettingsUI = require(script.Parent.SettingsUI)
local MainMenu = require(script.Parent.MainMenuUI)
local DeathUI = require(script.Parent.DeathUI)

local UIManager = {}

local player = Players.LocalPlayer
local gui
local modal = nil
local started = false

local PANELS = {
	inventory = InventoryUI,
	crafting = CraftingUI,
	medical = MedicalUI,
	map = MapUI,
	journal = JournalUI,
	building = BuildingUI,
	settings = SettingsUI,
}

local function closeModal()
	if modal and PANELS[modal] then
		PANELS[modal].Close()
	end
	modal = nil
	GuiService.SelectedObject = nil
end

local function openModal(name)
	if modal == name then
		closeModal()
		return
	end
	closeModal()
	modal = name
	local panel = PANELS[name]
	if panel then
		panel.Open()
	end
end

function UIManager.OpenModal(name)
	if not started then return end
	if PANELS[name] then
		openModal(name)
	end
end

function UIManager.CloseModal()
	closeModal()
end

function UIManager.IsModalOpen()
	return modal ~= nil
end

function UIManager.StartGame()
	started = true
	HUD.SetVisible(true)
	UserInputService.MouseIconEnabled = false
	if Store.get("settings").mouseLock ~= false then
		UserInputService.MouseBehavior = Enum.MouseBehavior.LockCenter
	end
end

function UIManager.Build()
	gui = Instance.new("ScreenGui")
	gui.Name = "AshesUI"
	gui.ResetOnSpawn = false
	gui.IgnoreGuiInset = true
	gui.ZIndexBehavior = Enum.ZIndexBehavior.Sibling
	gui.DisplayOrder = 10
	gui.Parent = player:WaitForChild("PlayerGui")

	local flash = Instance.new("Frame")
	flash.Name = "Grain"
	flash.Size = UDim2.fromScale(1, 1)
	flash.BackgroundColor3 = Color3.fromRGB(0, 0, 0)
	flash.BackgroundTransparency = 1
	flash.BorderSizePixel = 0
	flash.ZIndex = 0
	flash.Parent = gui

	HUD.Build(gui)
	for _, panel in pairs(PANELS) do
		if panel.Build then
			panel.Build(gui)
		end
	end
	DeathUI.Build(gui)
	MainMenu.Build(gui, function()
		UIManager.StartGame()
	end)

	HUD.RefreshVitals(Store.get("survival"))
	HUD.RefreshWeather(Store.get("weather"))

	Store.on("survival", function(survival) HUD.RefreshVitals(survival) end)
	Store.on("weather", function(weather) HUD.RefreshWeather(weather) end)
	Store.on("quests", function(quests) HUD.RefreshQuests(quests) end)
	Store.on("inventory", function(inventory) HUD.RefreshInventory(inventory) end)
	Store.on("notifications", function() HUD.RefreshNotifications() end)
	Store.on("interactionTarget", function(target)
		HUD.SetPrompt(target and target.text or nil)
	end)

	UserInputService.InputBegan:Connect(function(input, processed)
		if processed then return end
		local key = input.KeyCode
		if key == Enum.KeyCode.Tab then
			if started then openModal("inventory") end
		elseif key == Enum.KeyCode.C then
			if started then openModal("crafting") end
		elseif key == Enum.KeyCode.H then
			if started then openModal("medical") end
		elseif key == Enum.KeyCode.M then
			if started then openModal("map") end
		elseif key == Enum.KeyCode.J then
			if started then openModal("journal") end
		elseif key == Enum.KeyCode.B then
			if started then openModal("building") end
		elseif key == Enum.KeyCode.F then
			UIManager.ToggleFlashlight()
		elseif key == Enum.KeyCode.Escape then
			if modal then
				closeModal()
			elseif started then
				openModal("settings")
			end
		end
	end)

	Store.on("dead", function(dead)
		if dead then
			closeModal()
		end
	end)

end

function UIManager.ToggleFlashlight()
	local on = not Store.get("flashlight")
	Store.set("flashlight", on)
	Network.fire("RequestFlashlight", on)
	local character = player.Character
	if character then
		local light = character:FindFirstChild("Flashlight")
		if not light then
			local root = character:FindFirstChild("HumanoidRootPart")
			if root then
				light = Instance.new("PointLight")
				light.Name = "Flashlight"
				light.Brightness = 0
				light.Range = 42
				light.Color = Color3.fromRGB(255, 236, 200)
				light.Parent = root
			end
		end
		if light and light:IsA("PointLight") then
			light.Brightness = on and 2.6 or 0
		end
	end
end

function UIManager.OnDeath(cause)
	closeModal()
	Store.set("dead", true)
	DeathUI.Show(cause)
end

function UIManager.OnRespawn()
	Store.set("dead", false)
	DeathUI.Hide()
end

function UIManager.Update(dt)
	HUD.Update()
	if modal == "map" then
		MapUI.Update()
	end
	if modal then
		UserInputService.MouseBehavior = Enum.MouseBehavior.Default
		UserInputService.MouseIconEnabled = true
	elseif started then
		UserInputService.MouseBehavior = Enum.MouseBehavior.LockCenter
		UserInputService.MouseIconEnabled = false
	end
end

return UIManager
