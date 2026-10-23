--!strict
--[[
	SettingsUI — graphics presets LOW/MEDIUM/HIGH/ULTRA, mouse, audio, and the
	controls reference.
]]

local Lighting = game:GetService("Lighting")
local ReplicatedStorage = game:GetService("ReplicatedStorage")
local SoundService = game:GetService("SoundService")

local Store = require(script.Parent.StateStore)
local Theme = require(script.Parent.UITheme)
local UI = require(script.Parent.UIBuilder)
local Network = require(script.Parent.Network)

local SettingsUI = {}

local gui
local root
local rows = {}

local PRESETS = {
	Low = { quality = 1, shadows = false, post = false, particles = 0.15, texture = 0.5 },
	Medium = { quality = 5, shadows = true, post = true, particles = 0.4, texture = 0.75 },
	High = { quality = 8, shadows = true, post = true, particles = 0.75, texture = 2 },
	Ultra = { quality = 10, shadows = true, post = true, particles = 1, texture = 3 },
}

function SettingsUI.ApplyPreset(name)
	local preset = PRESETS[name] or PRESETS.High
	local ok = pcall(function()
		settings().Rendering.QualityLevel = preset.quality
	end)
	if not ok then
		-- quality control can be restricted on some clients; visuals still adapt
	end
	for _, effect in ipairs(Lighting:GetChildren()) do
		if effect:IsA("PostEffect") or effect:IsA("BloomEffect") or effect:IsA("BlurEffect")
			or effect:IsA("DepthOfFieldEffect") or effect:IsA("SunRaysEffect") then
			effect.Enabled = preset.post
		end
	end
	local quality = Store.get("inventory")
	local settingsTable = Store.get("settings")
	settingsTable.quality = name
	Store.set("settings", settingsTable)
	Network.fire("RequestSettings", { quality = name })
	return preset
end

local function slider(parent, y, label, min, max, value, onChanged)
	UI.label(parent, {
		size = UDim2.new(1, -140, 0, 18),
		position = UDim2.fromOffset(0, y),
		text = label,
		textSize = 11,
		font = Theme.Fonts.mono,
		textColor = Theme.Colors.textDim,
	})
	local track = UI.frame(parent, {
		size = UDim2.new(1, -150, 0, 4),
		position = UDim2.fromOffset(0, y + 20),
		color = Theme.Colors.panelDeep,
		stroke = false,
	})
	local fill = UI.frame(track, {
		size = UDim2.fromScale((value - min) / (max - min), 1),
		color = Theme.Colors.accent,
		stroke = false,
	})
	local valueLabel = UI.label(parent, {
		size = UDim2.fromOffset(70, 18),
		position = UDim2.new(1, -70, 0, y),
		text = string.format("%.2f", value),
		textSize = 11,
		font = Theme.Fonts.mono,
		textColor = Theme.Colors.accent,
		alignX = Enum.TextXAlignment.Right,
	})
	local button = Instance.new("TextButton")
	button.BackgroundTransparency = 1
	button.Text = ""
	button.Size = UDim2.new(1, 0, 0, 26)
	button.Position = UDim2.fromOffset(0, y + 8)
	button.Parent = parent
	local dragging = false
	local function apply(x)
		local absolute = track.AbsolutePosition.X
		local width = math.max(1, track.AbsoluteSize.X)
		local fraction = math.clamp((x - absolute) / width, 0, 1)
		local newValue = min + (max - min) * fraction
		fill.Size = UDim2.fromScale(fraction, 1)
		valueLabel.Text = string.format("%.2f", newValue)
		onChanged(newValue)
	end
	button.InputBegan:Connect(function(input)
		if input.UserInputType == Enum.UserInputType.MouseButton1 then
			dragging = true
			apply(input.Position.X)
		end
	end)
	game:GetService("UserInputService").InputChanged:Connect(function(input)
		if dragging and input.UserInputType == Enum.UserInputType.MouseMovement then
			apply(input.Position.X)
		end
	end)
	game:GetService("UserInputService").InputEnded:Connect(function()
		dragging = false
	end)
	return y + 34
end

local function toggle(parent, y, label, value, onChanged)
	UI.label(parent, {
		size = UDim2.new(1, -80, 0, 22),
		position = UDim2.fromOffset(0, y),
		text = label,
		textSize = 11,
		font = Theme.Fonts.mono,
		textColor = Theme.Colors.textDim,
	})
	local btn = UI.button(parent, {
		size = UDim2.fromOffset(64, 22),
		position = UDim2.new(1, -64, 0, y),
		text = value and "ON" or "OFF",
		color = value and Theme.Colors.accentDim or Theme.Colors.panelDeep,
		textSize = 10,
		font = Theme.Fonts.mono,
	})
	local state = value
	btn.MouseButton1Click:Connect(function()
		state = not state
		btn.Text = state and "ON" or "OFF"
		btn.BackgroundColor3 = state and Theme.Colors.accentDim or Theme.Colors.panelDeep
		onChanged(state)
	end)
	return y + 28
end

function SettingsUI.Build(parent)
	gui = parent
	root = UI.frame(gui, {
		name = "Settings",
		size = UDim2.fromOffset(720, 470),
		position = UDim2.fromScale(0.5, 0.5),
		anchor = Vector2.new(0.5, 0.5),
		color = Theme.Colors.bg,
		visible = false,
		zIndex = 20,
	})
	UI.label(root, {
		size = UDim2.new(1, -24, 0, 30),
		position = UDim2.fromOffset(16, 10),
		text = "CONFIGURATION",
		textSize = 16,
		font = Theme.Fonts.bold,
	})

	local panel = UI.frame(root, {
		size = UDim2.fromOffset(340, 390),
		position = UDim2.fromOffset(16, 50),
		color = Theme.Colors.panel,
		padding = 12,
	})
	UI.label(panel, {
		size = UDim2.new(1, 0, 0, 16),
		text = "GRAPHICS",
		textSize = 10,
		font = Theme.Fonts.mono,
		textColor = Theme.Colors.textFaint,
	})
	local presets = { "Low", "Medium", "High", "Ultra" }
	for index, name in ipairs(presets) do
		local btn = UI.button(panel, {
			size = UDim2.fromOffset(74, 26),
			position = UDim2.fromOffset((index - 1) * 80, 22),
			text = string.upper(name),
			color = Theme.Colors.panelAlt,
			textSize = 9,
			font = Theme.Fonts.mono,
		})
		btn.MouseButton1Click:Connect(function()
			SettingsUI.ApplyPreset(name)
			Store.notify("Graphics preset: " .. name, "info", 2)
		end)
	end

	UI.label(panel, {
		size = UDim2.new(1, 0, 0, 16),
		position = UDim2.fromOffset(0, 60),
		text = "CONTROL",
		textSize = 10,
		font = Theme.Fonts.mono,
		textColor = Theme.Colors.textFaint,
	})
	local settings = Store.get("settings")
	local y = 82
	y = slider(panel, y, "Mouse sensitivity", 0.05, 2.0, settings.sensitivity or 0.55, function(value)
		settings.sensitivity = value
		Network.fire("RequestSettings", { sensitivity = value })
	end)
	y = slider(panel, y, "Field of view", 55, 105, settings.fov or 72, function(value)
		settings.fov = value
		Store.set("settings", settings)
		Network.fire("RequestSettings", { fov = value })
	end)
	y = toggle(panel, y, "Head bob", settings.headBob ~= false, function(value)
		settings.headBob = value
		Store.set("settings", settings)
		Network.fire("RequestSettings", { headBob = value })
	end)
	y = toggle(panel, y, "Camera shake", settings.shake ~= false, function(value)
		settings.shake = value
		Store.set("settings", settings)
		Network.fire("RequestSettings", { shake = value })
	end)
	y = toggle(panel, y, "Invert Y", settings.invertY == true, function(value)
		settings.invertY = value
		Store.set("settings", settings)
		Network.fire("RequestSettings", { invertY = value })
	end)
	y = toggle(panel, y, "Cinematic grain", true, function(value)
		Store.set("grain", value)
	end)

	local controls = UI.frame(root, {
		size = UDim2.fromOffset(340, 390),
		position = UDim2.fromOffset(364, 50),
		color = Theme.Colors.panel,
		padding = 12,
	})
	UI.label(controls, {
		size = UDim2.new(1, 0, 0, 16),
		text = "CONTROLS",
		textSize = 10,
		font = Theme.Fonts.mono,
		textColor = Theme.Colors.textFaint,
	})
	local lines = {
		{ "WASD", "Move" },
		{ "Left Shift", "Sprint" },
		{ "Left Ctrl", "Crouch" },
		{ "Space", "Jump / vault / climb" },
		{ "E", "Interact / confirm build" },
		{ "F", "Flashlight" },
		{ "Tab", "Inventory" },
		{ "C", "Crafting" },
		{ "B", "Building" },
		{ "M", "Map" },
		{ "J", "Journal" },
		{ "H", "Medical" },
		{ "V", "Camera mode" },
		{ "R", "Reload / rotate build" },
		{ "1 2 3", "Equip slots" },
		{ "Escape", "Close / menu" },
	}
	for index, line in ipairs(lines) do
		UI.label(controls, {
			size = UDim2.fromOffset(96, 18),
			position = UDim2.fromOffset(0, 22 + (index - 1) * 21),
			text = line[1],
			textSize = 10,
			font = Theme.Fonts.mono,
			textColor = Theme.Colors.accent,
		})
		UI.label(controls, {
			size = UDim2.new(1, -100, 0, 18),
			position = UDim2.fromOffset(100, 22 + (index - 1) * 21),
			text = line[2],
			textSize = 10,
			font = Theme.Fonts.regular,
			textColor = Theme.Colors.textDim,
		})
	end
end

function SettingsUI.Open()
	if not root then return end
	root.Visible = true
end

function SettingsUI.Close() if root then root.Visible = false end end

function SettingsUI.Toggle()
	if not root then return end
	if root.Visible then SettingsUI.Close() else SettingsUI.Open() end
	return root.Visible
end

function SettingsUI.IsOpen() return root ~= nil and root.Visible end

return SettingsUI
