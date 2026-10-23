--!strict
--[[
	MedicalUI — body-region damage map, condition list and treatment actions.
]]

local ReplicatedStorage = game:GetService("ReplicatedStorage")

local Store = require(script.Parent.StateStore)
local Theme = require(script.Parent.UITheme)
local UI = require(script.Parent.UIBuilder)
local Network = require(script.Parent.Network)

local Items = require(ReplicatedStorage:WaitForChild("Shared"):WaitForChild("ItemDefinitions"))

local MedicalUI = {}

local gui
local root
local bodyView
local conditionView
local detailLabel

local REGIONS = {
	{ id = "Head", label = "HEAD", position = UDim2.fromOffset(88, 6), size = UDim2.fromOffset(44, 40) },
	{ id = "Torso", label = "TORSO", position = UDim2.fromOffset(82, 50), size = UDim2.fromOffset(56, 72) },
	{ id = "LeftArm", label = "L.ARM", position = UDim2.fromOffset(40, 50), size = UDim2.fromOffset(36, 66) },
	{ id = "RightArm", label = "R.ARM", position = UDim2.fromOffset(144, 50), size = UDim2.fromOffset(36, 66) },
	{ id = "LeftLeg", label = "L.LEG", position = UDim2.fromOffset(82, 126), size = UDim2.fromOffset(26, 76) },
	{ id = "RightLeg", label = "R.LEG", position = UDim2.fromOffset(112, 126), size = UDim2.fromOffset(26, 76) },
}

local injuries = {}

local function countItem(itemId)
	local total = 0
	for _, container in pairs(Store.get("inventory").containers or {}) do
		for _, entry in pairs(container.items) do
			if entry.itemId == itemId then total += entry.count or 1 end
		end
	end
	return total
end

local function drawBody()
	if not bodyView then return end
	UI.clear(bodyView)
	local survival = Store.get("survival")
	local parts = survival.bodyParts or {}
	for _, region in ipairs(REGIONS) do
		local health = parts[region.id] or 100
		local color = Theme.Colors.health
		if health > 75 then color = Theme.Colors.success
		elseif health > 40 then color = Theme.Colors.warn end
		local cell = UI.frame(bodyView, {
			size = region.size,
			position = region.position,
			color = color,
			transparency = 0.75,
			strokeColor = color,
			strokeTransparency = 0.4,
		})
		UI.label(cell, {
			size = UDim2.fromScale(1, 1),
			text = region.label .. "\n" .. math.floor(health) .. "%",
			textSize = 9,
			font = Theme.Fonts.mono,
			textColor = Theme.Colors.text,
			alignX = Enum.TextXAlignment.Center,
			wrapped = true,
		})
	end
end

local function drawConditions()
	if not conditionView then return end
	UI.clear(conditionView)
	local survival = Store.get("survival")
	local entries = {}
	for key, active in pairs(survival.conditions or {}) do
		if active then table.insert(entries, key) end
	end
	table.sort(entries)
	if #entries == 0 then
		UI.label(conditionView, {
			size = UDim2.new(1, 0, 0, 20),
			text = "No active conditions.",
			textSize = 11,
			font = Theme.Fonts.mono,
			textColor = Theme.Colors.textFaint,
		})
	else
		for index, key in ipairs(entries) do
			UI.label(conditionView, {
				size = UDim2.new(1, 0, 0, 18),
				position = UDim2.fromOffset(0, (index - 1) * 20),
				text = "- " .. string.upper((key:gsub("_", " "))),
				textSize = 11,
				font = Theme.Fonts.mono,
				textColor = Theme.Colors.warn,
			})
		end
	end
	if detailLabel then
		detailLabel.Text = string.format(
			"Blood loss: %d%%\nInfection: %d%%\nBody temperature: %.1f C\nStress: %d%%",
			math.floor(survival.bleeding or 0), math.floor(survival.infection or 0),
			survival.temperature or 36.6, math.floor(survival.stress or 0))
	end
end

function MedicalUI.Build(parent)
	gui = parent
	root = UI.frame(gui, {
		name = "Medical",
		size = UDim2.fromOffset(660, 400),
		position = UDim2.fromScale(0.5, 0.5),
		anchor = Vector2.new(0.5, 0.5),
		color = Theme.Colors.bg,
		visible = false,
		zIndex = 20,
	})
	UI.label(root, {
		size = UDim2.new(1, -24, 0, 30),
		position = UDim2.fromOffset(16, 10),
		text = "FIELD MEDICINE",
		textSize = 16,
		font = Theme.Fonts.bold,
	})
	UI.label(root, {
		size = UDim2.fromOffset(420, 16),
		position = UDim2.fromOffset(16, 40),
		text = "Untreated wounds bleed. Infection spreads. Treat early.",
		textSize = 10,
		font = Theme.Fonts.mono,
		textColor = Theme.Colors.textFaint,
	})

	bodyView = UI.frame(root, {
		name = "Body",
		size = UDim2.fromOffset(200, 220),
		position = UDim2.fromOffset(16, 66),
		color = Theme.Colors.panel,
		transparency = 0.4,
		stroke = false,
	})
	conditionView = UI.frame(root, {
		name = "Conditions",
		size = UDim2.fromOffset(220, 130),
		position = UDim2.fromOffset(232, 66),
		color = Theme.Colors.panel,
		padding = 10,
	})
	detailLabel = UI.label(root, {
		size = UDim2.fromOffset(220, 80),
		position = UDim2.fromOffset(232, 204),
		text = "",
		textSize = 10,
		font = Theme.Fonts.mono,
		textColor = Theme.Colors.textDim,
		wrapped = true,
		alignY = Enum.TextYAlignment.Top,
	})

	local treatments = {
		{ item = "bandage", label = "BANDAGE" },
		{ item = "medkit", label = "MEDKIT" },
		{ item = "antibiotics", label = "ANTIBIOTICS" },
		{ item = "painkillers", label = "PAINKILLERS" },
		{ item = "splint", label = "SPLINT" },
	}
	local panel = UI.frame(root, {
		size = UDim2.fromOffset(170, 290),
		position = UDim2.fromOffset(470, 66),
		color = Theme.Colors.panel,
		padding = 8,
	})
	UI.label(panel, {
		size = UDim2.new(1, 0, 0, 16),
		text = "TREATMENT",
		textSize = 10,
		font = Theme.Fonts.mono,
		textColor = Theme.Colors.textFaint,
	})
	for index, treatment in ipairs(treatments) do
		local btn = UI.button(panel, {
			size = UDim2.new(1, 0, 0, 30),
			position = UDim2.fromOffset(0, 22 + (index - 1) * 34),
			text = treatment.label .. " (0)",
			color = Theme.Colors.panelAlt,
			textSize = 10,
			font = Theme.Fonts.mono,
		})
		local itemId = treatment.item
		btn.MouseButton1Click:Connect(function()
			local uid
			for _, container in pairs(Store.get("inventory").containers or {}) do
				for id, entry in pairs(container.items) do
					if entry.itemId == itemId then
						uid = id
						break
					end
				end
				if uid then break end
			end
			if uid then
				Network.fire("RequestUseItem", uid)
			else
				Store.notify("No " .. string.lower(string.gsub(treatment.label, " ", "")) .. " available.", "warn", 2.4)
			end
		end)
		treatment.button = btn
	end

	Store.on("survival", function()
		drawBody()
		drawConditions()
		if root and root.Visible then
			for _, treatment in ipairs(treatments) do
				treatment.button.Text = string.format("%s (%d)", treatment.label, countItem(treatment.item))
			end
		end
	end)
	Store.on("inventory", function()
		if root and root.Visible then
			for _, treatment in ipairs(treatments) do
				treatment.button.Text = string.format("%s (%d)", treatment.label, countItem(treatment.item))
			end
		end
	end)
end

function MedicalUI.Open()
	if not root then return end
	root.Visible = true
	drawBody()
	drawConditions()
end

function MedicalUI.Close() if root then root.Visible = false end end

function MedicalUI.Toggle()
	if not root then return end
	if root.Visible then MedicalUI.Close() else MedicalUI.Open() end
	return root.Visible
end

function MedicalUI.IsOpen() return root ~= nil and root.Visible end

return MedicalUI
