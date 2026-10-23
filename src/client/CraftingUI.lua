--!strict
--[[
	CraftingUI — recipe browser with station filter, ingredient checklist and a
	progress bar. All validation happens on the server; this view only previews.
]]

local ReplicatedStorage = game:GetService("ReplicatedStorage")

local Store = require(script.Parent.StateStore)
local Theme = require(script.Parent.UITheme)
local UI = require(script.Parent.UIBuilder)
local Network = require(script.Parent.Network)

local Shared = ReplicatedStorage:WaitForChild("Shared")
local Recipes = require(Shared.Recipes)
local Items = require(Shared.ItemDefinitions)

local CraftingUI = {}

local gui
local root
local listView
local detail
local selected = nil
local stationFilter = "all"

local function countItem(itemId)
	local total = 0
	for _, container in pairs(Store.get("inventory").containers or {}) do
		for _, entry in pairs(container.items) do
			if entry.itemId == itemId then
				total += entry.count or 1
			end
		end
	end
	return total
end

local function stationLabel(station)
	if not station then return "HAND" end
	return string.upper(station)
end

local function drawDetail()
	if not detail then return end
	UI.clear(detail)
	local recipe = selected and Recipes[selected]
	if not recipe then
		UI.label(detail, {
			size = UDim2.fromScale(1, 1),
			text = "Select a blueprint.",
			textSize = 12,
			font = Theme.Fonts.mono,
			textColor = Theme.Colors.textFaint,
			alignX = Enum.TextXAlignment.Center,
		})
		return
	end
	UI.label(detail, {
		size = UDim2.new(1, 0, 0, 22),
		text = string.upper(recipe.name),
		textSize = 16,
		font = Theme.Fonts.bold,
		textColor = Theme.Colors.text,
	})
	UI.label(detail, {
		size = UDim2.new(1, 0, 0, 40),
		position = UDim2.fromOffset(0, 26),
		text = recipe.desc or "",
		textSize = 11,
		font = Theme.Fonts.regular,
		textColor = Theme.Colors.textDim,
		wrapped = true,
		alignY = Enum.TextYAlignment.Top,
	})
	UI.label(detail, {
		size = UDim2.new(1, 0, 0, 14),
		position = UDim2.fromOffset(0, 68),
		text = string.format("STATION: %s   TIME: %.1fs   OUTPUT: %dx %s",
			stationLabel(recipe.station), recipe.time or 1,
			(recipe.output and recipe.output.count) or 1,
			Items[(recipe.output and recipe.output.id) or ""] and Items[recipe.output.id].name or tostring(recipe.output and recipe.output.id)),
		textSize = 10,
		font = Theme.Fonts.mono,
		textColor = Theme.Colors.textFaint,
	})
	UI.frame(detail, {
		size = UDim2.new(1, 0, 0, 1),
		position = UDim2.fromOffset(0, 88),
		color = Theme.Colors.border,
		corner = false,
		stroke = false,
	})

	local y = 98
	UI.label(detail, {
		size = UDim2.new(1, 0, 0, 14),
		position = UDim2.fromOffset(0, y),
		text = "REQUIRES",
		textSize = 10,
		font = Theme.Fonts.mono,
		textColor = Theme.Colors.textFaint,
	})
	y += 18
	local craftable = true
	for itemId, count in pairs(recipe.ingredients or {}) do
		local have = countItem(itemId)
		local def = Items[itemId]
		local ok = have >= count
		if not ok then craftable = false end
		UI.label(detail, {
			size = UDim2.new(1, 0, 0, 16),
			position = UDim2.fromOffset(0, y),
			text = string.format("%s %s  %d/%d", ok and "+" or "x", def and def.name or itemId, have, count),
			textSize = 11,
			font = Theme.Fonts.mono,
			textColor = ok and Theme.Colors.success or Theme.Colors.danger,
		})
		y += 18
	end

	local button = UI.button(detail, {
		size = UDim2.new(1, 0, 0, 34),
		position = UDim2.new(0, 0, 1, -38),
		text = craftable and "CRAFT" or "MISSING MATERIALS",
		color = craftable and Theme.Colors.accentDim or Theme.Colors.panelAlt,
		textColor = craftable and Theme.Colors.text or Theme.Colors.textFaint,
		textSize = 12,
		font = Theme.Fonts.bold,
	})
	button.MouseButton1Click:Connect(function()
		if craftable then
			Network.fire("RequestCraft", selected)
		end
	end)
end

local function drawList()
	if not listView then return end
	UI.clear(listView)
	local known = Store.get("inventory")
	local blueprints = (Store.get("profile") or {}).blueprints or {}
	local index = 0
	for id, recipe in pairs(Recipes) do
		if type(recipe) == "table" then
			local show = stationFilter == "all" or (recipe.station or "hand") == stationFilter
			if show and (not recipe.blueprint or blueprints[recipe.blueprint] or blueprints[id]) then
				index += 1
				local row = UI.frame(listView, {
					size = UDim2.new(1, -6, 0, 30),
					position = UDim2.fromOffset(0, (index - 1) * 33),
					color = (id == selected) and Theme.Colors.panelAlt or Theme.Colors.panelDeep,
					strokeColor = (id == selected) and Theme.Colors.borderBright or Theme.Colors.border,
					strokeTransparency = (id == selected) and 0 or 0.7,
					zIndex = 22,
				})
				UI.label(row, {
					size = UDim2.new(1, -8, 0, 16),
					position = UDim2.fromOffset(6, 2),
					text = string.upper(recipe.name),
					textSize = 11,
					font = Theme.Fonts.medium,
					textColor = Theme.Colors.text,
				})
				UI.label(row, {
					size = UDim2.new(1, -8, 0, 12),
					position = UDim2.fromOffset(6, 16),
					text = stationLabel(recipe.station) .. " · " .. tostring(recipe.time or 1) .. "s",
					textSize = 9,
					font = Theme.Fonts.mono,
					textColor = Theme.Colors.textFaint,
				})
				local btn = Instance.new("TextButton")
				btn.BackgroundTransparency = 1
				btn.Text = ""
				btn.Size = UDim2.fromScale(1, 1)
				btn.Parent = row
				btn.MouseButton1Click:Connect(function()
					selected = id
					drawList()
					drawDetail()
				end)
			end
		end
	end
end

function CraftingUI.Build(parent)
	gui = parent
	root = UI.frame(gui, {
		name = "Crafting",
		size = UDim2.fromOffset(720, 440),
		position = UDim2.fromScale(0.5, 0.5),
		anchor = Vector2.new(0.5, 0.5),
		color = Theme.Colors.bg,
		visible = false,
		zIndex = 20,
	})
	UI.label(root, {
		size = UDim2.new(1, -24, 0, 30),
		position = UDim2.fromOffset(16, 10),
		text = "FABRICATION",
		textSize = 16,
		font = Theme.Fonts.bold,
	})
	UI.label(root, {
		size = UDim2.fromOffset(400, 16),
		position = UDim2.fromOffset(16, 40),
		text = "Crafting is only possible at the appropriate station.",
		textSize = 10,
		font = Theme.Fonts.mono,
		textColor = Theme.Colors.textFaint,
	})

	local filters = { "all", "hand", "campfire", "workbench", "chemstation", "forge" }
	for index, filter in ipairs(filters) do
		local btn = UI.button(root, {
			size = UDim2.fromOffset(70, 22),
			position = UDim2.fromOffset(16 + (index - 1) * 74, 62),
			text = string.upper(filter),
			color = filter == "all" and Theme.Colors.panelAlt or Theme.Colors.panel,
			textSize = 9,
			font = Theme.Fonts.mono,
		})
		btn.MouseButton1Click:Connect(function()
			stationFilter = filter
			drawList()
		end)
	end

	local listHolder = UI.frame(root, {
		name = "ListHolder",
		size = UDim2.fromOffset(300, 340),
		position = UDim2.fromOffset(16, 92),
		color = Theme.Colors.panel,
		padding = 6,
	})
	listView = UI.frame(listHolder, {
		size = UDim2.fromScale(1, 1),
		transparency = 1,
		stroke = false,
		clip = true,
	})
	detail = UI.frame(root, {
		name = "Detail",
		size = UDim2.fromOffset(370, 340),
		position = UDim2.fromOffset(332, 92),
		color = Theme.Colors.panel,
		padding = 14,
	})

	Store.on("inventory", function()
		if root and root.Visible then
			drawList()
			drawDetail()
		end
	end)
end

function CraftingUI.Open()
	if not root then return end
	root.Visible = true
	if not selected then
		for id in pairs(Recipes) do
			selected = id
			break
		end
	end
	drawList()
	drawDetail()
end

function CraftingUI.Close()
	if root then root.Visible = false end
end

function CraftingUI.Toggle()
	if not root then return end
	if root.Visible then CraftingUI.Close() else CraftingUI.Open() end
	return root.Visible
end

function CraftingUI.IsOpen()
	return root ~= nil and root.Visible
end

return CraftingUI
