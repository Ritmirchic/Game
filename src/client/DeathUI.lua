--!strict
--[[
	DeathUI — death screen with the cause, a running record and a countdown.
]]

local Store = require(script.Parent.StateStore)
local Theme = require(script.Parent.UITheme)
local UI = require(script.Parent.UIBuilder)

local DeathUI = {}

local gui
local root
local causeLabel
local statsLabel
local countdownLabel
local visible = false

function DeathUI.Build(parent)
	gui = parent
	root = UI.frame(gui, {
		name = "Death",
		size = UDim2.fromScale(1, 1),
		color = Color3.fromRGB(8, 2, 2),
		transparency = 0.05,
		stroke = false,
		visible = false,
		zIndex = 90,
	})
	local center = UI.frame(root, {
		size = UDim2.fromOffset(520, 220),
		position = UDim2.fromScale(0.5, 0.5),
		anchor = Vector2.new(0.5, 0.5),
		transparency = 1,
		stroke = false,
		zIndex = 91,
	})
	UI.label(center, {
		size = UDim2.new(1, 0, 0, 34),
		text = "YOU DIED",
		textSize = 38,
		font = Theme.Fonts.bold,
		textColor = Theme.Colors.health,
		alignX = Enum.TextXAlignment.Center,
		zIndex = 92,
	})
	causeLabel = UI.label(center, {
		size = UDim2.new(1, 0, 0, 22),
		position = UDim2.fromOffset(0, 40),
		text = "",
		textSize = 13,
		font = Theme.Fonts.mono,
		textColor = Theme.Colors.textDim,
		alignX = Enum.TextXAlignment.Center,
		zIndex = 92,
	})
	statsLabel = UI.label(center, {
		size = UDim2.new(1, 0, 0, 48),
		position = UDim2.fromOffset(0, 72),
		text = "",
		textSize = 11,
		font = Theme.Fonts.mono,
		textColor = Theme.Colors.textFaint,
		alignX = Enum.TextXAlignment.Center,
		zIndex = 92,
	})
	countdownLabel = UI.label(center, {
		size = UDim2.new(1, 0, 0, 20),
		position = UDim2.fromOffset(0, 140),
		text = "",
		textSize = 11,
		font = Theme.Fonts.mono,
		textColor = Theme.Colors.accent,
		alignX = Enum.TextXAlignment.Center,
		zIndex = 92,
	})
end

function DeathUI.Show(cause)
	if not root then return end
	visible = true
	local profile = Store.get("profile") or {}
	local stats = profile.stats or {}
	local survival = Store.get("survival")
	causeLabel.Text = string.upper(cause or "EXPOSURE")
	statsLabel.Text = string.format(
		"Day %d · %s\nKills %d · Items found %d · Documents %d\nDistance travelled %d m · Deaths %d",
		(Store.get("weather").day or 1),
		survival.region or "UNKNOWN",
		stats.kills or 0, stats.itemsFound or 0, stats.documents or 0,
		math.floor(stats.distance or 0), (stats.deaths or 0) + 1)
	root.Visible = true
	root.BackgroundTransparency = 1
	local TweenService = game:GetService("TweenService")
	TweenService:Create(root, TweenInfo.new(1.2), { BackgroundTransparency = 0.05 }):Play()
	task.spawn(function()
		for i = 9, 0, -1 do
			if not visible then break end
			countdownLabel.Text = (i > 0) and string.format("Respawn in %ds", i) or "Waking up..."
			task.wait(1)
		end
	end)
end

function DeathUI.Hide()
	if not root then return end
	visible = false
	root.Visible = false
end

function DeathUI.IsOpen()
	return visible
end

return DeathUI
