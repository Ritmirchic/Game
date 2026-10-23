--!strict
--[[
	HUD — the always-on interface: vitals, compass, crosshair, interaction
	prompt, notifications, quest tracker, damage vignette and tutorial tip.
]]

local Players = game:GetService("Players")
local RunService = game:GetService("RunService")
local TweenService = game:GetService("TweenService")

local Store = require(script.Parent.StateStore)
local Theme = require(script.Parent.UITheme)
local UI = require(script.Parent.UIBuilder)

local HUD = {}

local player = Players.LocalPlayer
local gui
local refs = {}
local vignette
local tutorialBox
local compassStrip

local function barRow(parent, y, label, color, name)
	local row = UI.frame(parent, {
		size = UDim2.new(1, 0, 0, 15),
		position = UDim2.fromOffset(0, y),
		transparency = 1,
		stroke = false,
		name = name,
	})
	UI.label(row, {
		size = UDim2.fromOffset(34, 15),
		text = label,
		textSize = 10,
		font = Theme.Fonts.mono,
		textColor = Theme.Colors.textDim,
	})
	local track, fill = UI.bar(row, {
		size = UDim2.new(1, -38, 0, 6),
		position = UDim2.fromOffset(38, 5),
		color = color,
		track = Theme.Colors.panelDeep,
		corner = UDim.new(0, 1),
		name = name .. "Bar",
	})
	local valueLabel = UI.label(row, {
		size = UDim2.fromOffset(60, 15),
		position = UDim2.new(1, -60, 0, 0),
		text = "100",
		textSize = 10,
		font = Theme.Fonts.mono,
		textColor = Theme.Colors.textDim,
		alignX = Enum.TextXAlignment.Right,
	})
	return fill, valueLabel, track
end

function HUD.Build(parent)
	gui = parent

	-- vitals, bottom left
	local vitals = UI.frame(gui, {
		name = "Vitals",
		size = UDim2.fromOffset(250, 168),
		position = UDim2.new(0, 22, 1, -190),
		color = Theme.Colors.panel,
		transparency = 0.25,
		strokeColor = Theme.Colors.border,
		padding = 10,
	})
	UI.label(vitals, {
		size = UDim2.new(1, 0, 0, 14),
		text = "VITALS",
		textSize = 10,
		font = Theme.Fonts.mono,
		textColor = Theme.Colors.textFaint,
	})
	local y = 20
	refs.health, refs.healthText = barRow(vitals, y, "HP", Theme.Colors.health, "Health")
	y += 18
	refs.hunger, refs.hungerText = barRow(vitals, y, "FD", Theme.Colors.hunger, "Hunger")
	y += 18
	refs.thirst, refs.thirstText = barRow(vitals, y, "WA", Theme.Colors.thirst, "Thirst")
	y += 18
	refs.stamina, refs.staminaText = barRow(vitals, y, "ST", Theme.Colors.stamina, "Stamina")
	y += 18
	refs.temperature, refs.temperatureText = barRow(vitals, y, "TMP", Theme.Colors.temperature, "Temperature")
	y += 18
	refs.radiation, refs.radiationText = barRow(vitals, y, "RAD", Theme.Colors.radiation, "Radiation")
	y += 18
	refs.infection, refs.infectionText = barRow(vitals, y, "INF", Theme.Colors.warn, "Infection")
	y += 18
	refs.stress, refs.stressText = barRow(vitals, y, "PSY", Theme.Colors.stress, "Stress")
	y += 20
	refs.conditions = UI.label(vitals, {
		size = UDim2.new(1, 0, 0, 16),
		position = UDim2.fromOffset(0, y),
		text = "",
		textSize = 10,
		font = Theme.Fonts.mono,
		textColor = Theme.Colors.warn,
	})

	-- region + clock, top left
	local regionPanel = UI.frame(gui, {
		name = "RegionPanel",
		size = UDim2.fromOffset(230, 54),
		position = UDim2.fromOffset(22, 22),
		color = Theme.Colors.panel,
		transparency = 0.35,
		padding = 8,
	})
	refs.region = UI.label(regionPanel, {
		size = UDim2.new(1, 0, 0, 18),
		text = "DEAD SUBURBS",
		textSize = 13,
		font = Theme.Fonts.bold,
		textColor = Theme.Colors.text,
	})
	refs.clock = UI.label(regionPanel, {
		size = UDim2.new(1, 0, 0, 14),
		position = UDim2.fromOffset(0, 18),
		text = "DAY 1 · 17:24 · OVERCAST",
		textSize = 10,
		font = Theme.Fonts.mono,
		textColor = Theme.Colors.textDim,
	})

	-- weapon/ammo, bottom right
	local weaponPanel = UI.frame(gui, {
		name = "WeaponPanel",
		size = UDim2.fromOffset(220, 58),
		position = UDim2.new(1, -242, 1, -80),
		color = Theme.Colors.panel,
		transparency = 0.35,
		padding = 8,
	})
	refs.weaponName = UI.label(weaponPanel, {
		size = UDim2.new(1, 0, 0, 18),
		text = "UNARMED",
		textSize = 13,
		font = Theme.Fonts.bold,
	})
	refs.ammo = UI.label(weaponPanel, {
		size = UDim2.new(1, 0, 0, 20),
		position = UDim2.fromOffset(0, 20),
		text = "-- / --",
		textSize = 18,
		font = Theme.Fonts.mono,
		textColor = Theme.Colors.accent,
		alignX = Enum.TextXAlignment.Right,
	})

	-- quest tracker, top right
	local tracker = UI.frame(gui, {
		name = "Tracker",
		size = UDim2.fromOffset(270, 96),
		position = UDim2.new(1, -292, 0, 22),
		color = Theme.Colors.panel,
		transparency = 0.45,
		padding = 8,
	})
	refs.questTitle = UI.label(tracker, {
		size = UDim2.new(1, 0, 0, 16),
		text = "NO ACTIVE CONTRACT",
		textSize = 11,
		font = Theme.Fonts.bold,
		textColor = Theme.Colors.accent,
		truncate = Enum.TextTruncate.AtEnd,
	})
	refs.questBody = UI.label(tracker, {
		size = UDim2.new(1, 0, 1, -18),
		position = UDim2.fromOffset(0, 18),
		text = "",
		textSize = 10,
		font = Theme.Fonts.mono,
		textColor = Theme.Colors.textDim,
		wrapped = true,
		alignY = Enum.TextYAlignment.Top,
	})

	-- crosshair
	refs.crosshair = UI.frame(gui, {
		name = "Crosshair",
		size = UDim2.fromOffset(18, 18),
		position = UDim2.fromScale(0.5, 0.5),
		anchor = Vector2.new(0.5, 0.5),
		transparency = 1,
		stroke = false,
	})
	for _, offset in ipairs({ Vector2.new(-14, -0.5), Vector2.new(12, -0.5), Vector2.new(-0.5, -14), Vector2.new(-0.5, 12) }) do
		UI.frame(refs.crosshair, {
			size = UDim2.fromOffset(3, 1),
			position = UDim2.fromOffset(offset.X, offset.Y),
			color = Theme.Colors.text,
			transparency = 0.25,
			corner = false,
			stroke = false,
		})
	end

	-- interaction prompt
	refs.prompt = UI.frame(gui, {
		name = "Prompt",
		size = UDim2.fromOffset(320, 30),
		position = UDim2.fromScale(0.5, 0.66),
		anchor = Vector2.new(0.5, 0.5),
		color = Theme.Colors.panelDeep,
		transparency = 0.25,
		visible = false,
	})
	UI.label(refs.prompt, {
		size = UDim2.fromScale(1, 1),
		text = "",
		textSize = 13,
		font = Theme.Fonts.medium,
		textColor = Theme.Colors.text,
		alignX = Enum.TextXAlignment.Center,
		name = "Text",
	})
	refs.promptText = refs.prompt:FindFirstChild("Text")

	-- notifications, right middle
	refs.notify = UI.frame(gui, {
		name = "Notify",
		size = UDim2.fromOffset(300, 220),
		position = UDim2.new(1, -320, 1, -300),
		transparency = 1,
		stroke = false,
	})
	local list = Instance.new("UIListLayout")
	list.Padding = UDim.new(0, 4)
	list.VerticalAlignment = Enum.VerticalAlignment.Bottom
	list.HorizontalAlignment = Enum.HorizontalAlignment.Right
	list.SortOrder = Enum.SortOrder.LayoutOrder
	list.Parent = refs.notify

	-- tutorial
	tutorialBox = UI.frame(gui, {
		name = "Tutorial",
		size = UDim2.fromOffset(420, 40),
		position = UDim2.new(0.5, -210, 1, -80),
		color = Theme.Colors.panelAlt,
		transparency = 0.2,
		visible = false,
	})
	UI.label(tutorialBox, {
		size = UDim2.fromScale(1, 1),
		text = "",
		textSize = 12,
		font = Theme.Fonts.mono,
		textColor = Theme.Colors.accent,
		alignX = Enum.TextXAlignment.Center,
		wrapped = true,
		name = "Text",
	})
	refs.tutorialText = tutorialBox:FindFirstChild("Text")

	-- damage vignette
	vignette = UI.frame(gui, {
		name = "Vignette",
		size = UDim2.fromScale(1, 1),
		color = Theme.Colors.health,
		transparency = 1,
		stroke = false,
		zIndex = 1,
	})

	-- compass at top centre
	compassStrip = UI.frame(gui, {
		name = "Compass",
		size = UDim2.fromOffset(360, 22),
		position = UDim2.new(0.5, -180, 0, 84),
		color = Theme.Colors.panelDeep,
		transparency = 0.55,
		clip = true,
		stroke = false,
	})
	refs.compassMarks = {}
	for i = 0, 11 do
		local mark = UI.label(compassStrip, {
			size = UDim2.fromOffset(30, 22),
			position = UDim2.fromOffset(i * 30, 0),
			text = ({ "N", "NE", "E", "SE", "S", "SW", "W", "NW" })[(i % 8) + 1],
			textSize = 10,
			font = Theme.Fonts.mono,
			textColor = Theme.Colors.textDim,
			alignX = Enum.TextXAlignment.Center,
			name = "Mark" .. i,
		})
		table.insert(refs.compassMarks, mark)
	end

	HUD.RefreshVitals(Store.get("survival"))
end

local function formatCondition(name)
	local pretty = {
		bleeding = "BLEEDING",
		infected = "INFECTED",
		broken_arm = "ARM FRACTURE",
		leg_injury = "LEG INJURY",
		sprained = "SPRAINED",
		hypothermia = "HYPOTHERMIA",
		heatstroke = "HEATSTROKE",
		radiation_sickness = "RADIATION SICKNESS",
		fatigue = "EXHAUSTED",
		panic = "PANIC",
	}
	return pretty[name] or string.upper(name)
end

function HUD.RefreshVitals(survival)
	if not refs.health then return end
	local function set(fill, label, value, max, display)
		local fraction = math.clamp((value or 0) / (max or 100), 0, 1)
		UI.setBar(fill, fraction, true)
		if label then
			if display == "temp" then
				label.Text = string.format("%.1f", value or 36.6)
			else
				label.Text = tostring(math.floor(value or 0))
			end
		end
	end
	set(refs.health, refs.healthText, survival.health, 100)
	set(refs.hunger, refs.hungerText, survival.hunger, 100)
	set(refs.thirst, refs.thirstText, survival.thirst, 100)
	set(refs.stamina, refs.staminaText, survival.stamina, 100)
	UI.setBar(refs.temperature, math.clamp(((survival.temperature or 36.6) - 33) / 6, 0, 1), true)
	if refs.temperatureText then
		refs.temperatureText.Text = string.format("%.1f", survival.temperature or 36.6)
	end
	set(refs.radiation, refs.radiationText, survival.radiation, 100)
	set(refs.infection, refs.infectionText, survival.infection, 100)
	set(refs.stress, refs.stressText, survival.stress, 100)

	local names = {}
	for key, active in pairs(survival.conditions or {}) do
		if active then table.insert(names, formatCondition(key)) end
	end
	refs.conditions.Text = table.concat(names, " · ")

	if refs.region and survival.region then
		refs.region.Text = string.upper(survival.region .. " ZONE")
	end
	-- damage vignette
	local hp = survival.health or 100
	vignette.BackgroundTransparency = 1 - math.clamp((45 - hp) / 100, 0, 0.5)
end

function HUD.RefreshWeather(weather)
	if not refs.clock then return end
	local hours = math.floor(weather.clock or 0)
	local minutes = math.floor(((weather.clock or 0) % 1) * 60)
	refs.clock.Text = string.format("DAY %d · %02d:%02d · %s", weather.day or 1, hours, minutes,
		string.upper(weather.kind or "OVERCAST"))
end

function HUD.RefreshInventory(inventory)
	if not refs.weaponName then return end
	local equipment = inventory.equipment or {}
	local slot = equipment.Primary or equipment.Secondary or equipment.Melee
	local ammo = Store.get("ammo")
	if slot and slot.itemId then
		local item = slot.itemId:gsub("_", " ")
		refs.weaponName.Text = string.upper(item)
		if ammo then
			refs.ammo.Text = string.format("%d / %d", ammo.magazine or 0, ammo.reserve or 0)
		else
			refs.ammo.Text = "MELEE"
		end
	else
		refs.weaponName.Text = "UNARMED"
		refs.ammo.Text = "--"
	end
end

function HUD.RefreshQuests(quests)
	if not refs.questTitle then return end
	local best, bestState
	for id, quest in pairs(quests.active or {}) do
		if not best or (quest.chapter or 99) < (best.chapter or 99) then
			best, bestState = quest, quests.active[id]
		end
	end
	if not best then
		refs.questTitle.Text = "NO ACTIVE CONTRACT"
		refs.questBody.Text = "Read documents. Listen to the radio."
		return
	end
	refs.questTitle.Text = string.upper(best.name)
	local lines = {}
	for _, objective in ipairs(best.objectives or {}) do
		local mark = objective.done and "[x]" or "[ ]"
		local count = objective.count and objective.count > 1
			and string.format(" (%d/%d)", objective.progress or 0, objective.count) or ""
		table.insert(lines, string.format("%s %s%s", mark, objective.text, count))
	end
	refs.questBody.Text = table.concat(lines, "\n")
end

function HUD.RefreshNotifications()
	if not refs.notify then return end
	UI.clear(refs.notify)
	local notifications = Store.get("notifications")
	for index, entry in ipairs(notifications) do
		local color = Theme.Colors.text
		if entry.kind == "danger" then color = Theme.Colors.danger
		elseif entry.kind == "warn" then color = Theme.Colors.warn
		elseif entry.kind == "quest" then color = Theme.Colors.accent
		elseif entry.kind == "combat" then color = Theme.Colors.health
		elseif entry.kind == "title" then color = Theme.Colors.accent
		elseif entry.kind == "info" then color = Theme.Colors.textDim
		end
		local box = UI.frame(refs.notify, {
			size = UDim2.new(1, 0, 0, 26),
			color = Theme.Colors.panelDeep,
			transparency = 0.15,
			strokeColor = color,
			strokeTransparency = 0.55,
			layoutOrder = index,
		})
		UI.label(box, {
			size = UDim2.new(1, -12, 1, 0),
			position = UDim2.fromOffset(8, 0),
			text = entry.text,
			textSize = 12,
			font = entry.kind == "title" and Theme.Fonts.bold or Theme.Fonts.medium,
			textColor = color,
			truncate = Enum.TextTruncate.AtEnd,
		})
	end
end

function HUD.SetPrompt(text)
	if not refs.prompt then return end
	refs.prompt.Visible = text ~= nil
	if text then
		refs.promptText.Text = text
	end
end

function HUD.ShowTutorial(text)
	if not tutorialBox then return end
	if not text then
		tutorialBox.Visible = false
		return
	end
	tutorialBox.Visible = true
	refs.tutorialText.Text = text
	task.delay(9, function()
		if tutorialBox and refs.tutorialText.Text == text then
			tutorialBox.Visible = false
		end
	end)
end

function HUD.Update()
	local survival = Store.get("survival")
	HUD.RefreshWeather(Store.get("weather"))
	HUD.RefreshQuests(Store.get("quests"))
	HUD.RefreshInventory(Store.get("inventory"))

	local camera = workspace.CurrentCamera
	if camera then
		local look = camera.CFrame.LookVector
		local heading = (math.deg(math.atan2(look.X, -look.Z)) + 360) % 360
		local offset = (heading / 360) * 240
		for index, mark in ipairs(refs.compassMarks or {}) do
			local x = ((index - 1) * 30 - offset) % 360
			mark.Position = UDim2.fromOffset(x, 0)
			mark.TextTransparency = (x > 0 and x < 300) and 0 or 1
		end
	end
	refs.crosshair.Visible = Store.get("aiming") == true
	HUD.RefreshVitals(survival)
	Store.pruneNotifications()
end

function HUD.SetVisible(visible)
	if not gui then return end
	for _, child in ipairs(gui:GetChildren()) do
		if child:IsA("GuiObject") and child.Name ~= "Vignette" then
			child.Visible = visible
		end
	end
end

return HUD
