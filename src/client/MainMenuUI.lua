--!strict
--[[
	MainMenuUI — title screen over a live, still playable world.

	The menu does not stop the game; it dims it and hands control back when the
	player presses BEGIN. It also hosts the credits and the lore card.
]]

local TweenService = game:GetService("TweenService")

local Store = require(script.Parent.StateStore)
local Theme = require(script.Parent.UITheme)
local UI = require(script.Parent.UIBuilder)

local MainMenu = {}

local gui
local root
local subtitleLabel
local beginButton
local opened = true
local onBegin = nil

local SUBTITLES = {
	"The signal has not stopped.",
	"Six districts. One transmitter.",
	"Something is learning your shape.",
	"Do not answer the radio after dark.",
}

function MainMenu.Build(parent, beginCallback)
	gui = parent
	onBegin = beginCallback
	root = UI.frame(gui, {
		name = "MainMenu",
		size = UDim2.fromScale(1, 1),
		color = Color3.fromRGB(6, 6, 8),
		transparency = 0.12,
		stroke = false,
		zIndex = 80,
	})

	local center = UI.frame(root, {
		size = UDim2.fromOffset(560, 340),
		position = UDim2.fromScale(0.5, 0.5),
		anchor = Vector2.new(0.5, 0.5),
		transparency = 1,
		stroke = false,
		zIndex = 81,
	})
	UI.label(center, {
		size = UDim2.new(1, 0, 0, 30),
		text = "ASHES OF HUMANITY",
		textSize = 40,
		font = Theme.Fonts.Bold or Enum.Font.GothamBold,
		textColor = Theme.Colors.text,
		alignX = Enum.TextXAlignment.Center,
		zIndex = 82,
	})
	UI.label(center, {
		size = UDim2.new(1, 0, 0, 18),
		position = UDim2.fromOffset(0, 34),
		text = "L A S T   S I G N A L",
		textSize = 14,
		font = Theme.Fonts.mono,
		textColor = Theme.Colors.accent,
		alignX = Enum.TextXAlignment.Center,
		zIndex = 82,
	})
	UI.frame(center, {
		size = UDim2.fromOffset(320, 1),
		position = UDim2.fromOffset(120, 66),
		color = Theme.Colors.borderBright,
		corner = false,
		stroke = false,
		zIndex = 82,
	})
	subtitleLabel = UI.label(center, {
		size = UDim2.new(1, 0, 0, 20),
		position = UDim2.fromOffset(0, 78),
		text = SUBTITLES[1],
		textSize = 12,
		font = Theme.Fonts.regular,
		textColor = Theme.Colors.textDim,
		alignX = Enum.TextXAlignment.Center,
		zIndex = 82,
	})

	beginButton = UI.button(center, {
		size = UDim2.fromOffset(240, 44),
		position = UDim2.fromOffset(160, 130),
		text = "BEGIN",
		color = Theme.Colors.panelAlt,
		strokeColor = Theme.Colors.accent,
		textColor = Theme.Colors.accent,
		textSize = 15,
		font = Theme.Fonts.bold,
		zIndex = 83,
	})
	beginButton.MouseButton1Click:Connect(function()
		MainMenu.Hide()
		if onBegin then onBegin() end
	end)

	UI.label(center, {
		size = UDim2.new(1, 0, 0, 60),
		position = UDim2.fromOffset(0, 190),
		text = "Wasd to move · Left Shift to sprint · E to interact · Tab for inventory\nPress Escape at any time for the menu.",
		textSize = 10,
		font = Theme.Fonts.mono,
		textColor = Theme.Colors.textFaint,
		alignX = Enum.TextXAlignment.Center,
		wrapped = true,
		zIndex = 82,
	})
	UI.label(center, {
		size = UDim2.new(1, 0, 0, 14),
		position = UDim2.new(0, 0, 1, -8),
		text = "A fan-free original survival horror experience · version 1.0",
		textSize = 9,
		font = Theme.Fonts.mono,
		textColor = Theme.Colors.textFaint,
		alignX = Enum.TextXAlignment.Center,
		zIndex = 82,
	})

	task.spawn(function()
		local index = 1
		while true do
			task.wait(5)
			if not opened then break end
			index = (index % #SUBTITLES) + 1
			subtitleLabel.Text = SUBTITLES[index]
		end
	end)
end

function MainMenu.Show()
	if not root then return end
	opened = true
	root.Visible = true
	root.BackgroundTransparency = 1
	TweenService:Create(root, TweenInfo.new(0.4), { BackgroundTransparency = 0.12 }):Play()
end

function MainMenu.Hide()
	if not root then return end
	opened = false
	TweenService:Create(root, TweenInfo.new(0.5), { BackgroundTransparency = 1 }):Play()
	task.delay(0.5, function()
		if root then root.Visible = false end
	end)
end

function MainMenu.IsOpen()
	return opened
end

return MainMenu
