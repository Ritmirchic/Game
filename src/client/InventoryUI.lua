--!strict
--[[
	InventoryUI — grid inventory with drag & drop, tooltips, equipment slots
	and item actions. All moves are requests; the server replies with a full
	inventory snapshot that redraws the grid.
]]

local Players = game:GetService("Players")
local UserInputService = game:GetService("UserInputService")
local ReplicatedStorage = game:GetService("ReplicatedStorage")

local Store = require(script.Parent.StateStore)
local Theme = require(script.Parent.UITheme)
local UI = require(script.Parent.UIBuilder)
local Network = require(script.Parent.Network)

local Shared = ReplicatedStorage:WaitForChild("Shared")
local Items = require(Shared.ItemDefinitions)
local Weapons = require(Shared.WeaponDefinitions)

local InventoryUI = {}

local player = Players.LocalPlayer
local gui
local root
local containerView
local tooltip
local selectedUid = nil
local dragState = nil
local cellRefs = {}
local equipmentRefs = {}
local weightLabel
local activeContainer = "Backpack"

local CELL = 46
local GAP = 3

local function itemDetails(uid)
	local inventory = Store.get("inventory")
	for containerId, container in pairs(inventory.containers or {}) do
		local entry = container.items[uid]
		if entry then
			return entry, containerId, container
		end
	end
	for slot, entry in pairs(inventory.equipment or {}) do
		if entry and entry.uid == uid then
			return entry, "equipment", slot
		end
	end
	return nil
end

local function describe(entry)
	local def = Items[entry.itemId]
	if not def then
		local weapon = Weapons[entry.itemId]
		if weapon and weapon.kind == "weapon" then
			return {
				name = weapon.name, desc = weapon.desc or "", cat = "weapon",
				w = 3, h = 2, weight = weapon.weight or 3, rarity = weapon.rarity or 2,
			}
		end
		return { name = entry.itemId, desc = "Unknown.", cat = "misc", w = 1, h = 1, weight = 0, rarity = 1 }
	end
	return def
end

local function showTooltip(entry, containerId)
	if not tooltip then return end
	local def = describe(entry)
	tooltip.Visible = true
	local title = tooltip:FindFirstChild("Title")
	local body = tooltip:FindFirstChild("Body")
	local meta = tooltip:FindFirstChild("Meta")
	title.Text = string.upper(def.name)
	title.TextColor3 = Theme.rarityColor(def.rarity)
	local lines = { def.desc or "" }
	if entry.dur then
		table.insert(lines, string.format("Condition: %d%%", math.floor(entry.dur * 100)))
	end
	if def.cat == "medical" and def.heal then
		table.insert(lines, string.format("Restores %d health", def.heal))
	end
	if def.use and def.use.kind == "food" then
		table.insert(lines, string.format("+%d food  +%d water", def.use.hunger or 0, def.use.thirst or 0))
	end
	body.Text = table.concat(lines, "\n")
	meta.Text = string.format("%s · %dx%d · %.1fkg · %s", string.upper(def.cat or "misc"),
		def.w or 1, def.h or 1, def.weight or 0, containerId or "")
end

local function hideTooltip()
	if tooltip then tooltip.Visible = false end
end

-- ------------------------------------------------------------------ drawing
local function drawItemIcon(cell, entry)
	local def = describe(entry)
	local icon = UI.frame(cell, {
		name = "Icon",
		size = UDim2.fromScale(1, 1),
		color = Theme.rarityColor(def.rarity),
		transparency = 0.86,
		strokeColor = Theme.rarityColor(def.rarity),
		strokeTransparency = 0.35,
		corner = UDim.new(0, 2),
		zIndex = 4,
	})
	UI.label(icon, {
		size = UDim2.new(1, -4, 1, -14),
		position = UDim2.fromOffset(2, 2),
		text = string.upper(string.sub(def.name or entry.itemId, 1, 3)),
		textSize = 15,
		font = Theme.Fonts.bold,
		textColor = Theme.rarityColor(def.rarity),
		alignX = Enum.TextXAlignment.Center,
		zIndex = 5,
	})
	if (entry.count or 1) > 1 then
		UI.label(icon, {
			size = UDim2.new(1, -4, 0, 12),
			position = UDim2.new(0, 2, 1, -13),
			text = "x" .. tostring(entry.count),
			textSize = 10,
			font = Theme.Fonts.mono,
			textColor = Theme.Colors.text,
			alignX = Enum.TextXAlignment.Right,
			zIndex = 6,
		})
	end
	if entry.dur and entry.dur < 0.4 then
		UI.frame(icon, {
			size = UDim2.new(1, 0, 0, 2),
			position = UDim2.new(0, 0, 1, -2),
			color = Theme.Colors.danger,
			corner = false,
			stroke = false,
			zIndex = 6,
		})
	end
	return icon
end

function InventoryUI.Redraw()
	if not containerView then return end
	UI.clear(containerView)
	cellRefs = {}
	local inventory = Store.get("inventory")
	local container = (inventory.containers or {})[activeContainer]
	if not container then return end

	local cols, rows = container.cols, container.rows
	for y = 1, rows do
		for x = 1, cols do
			local cell = UI.frame(containerView, {
				name = string.format("Cell_%d_%d", x, y),
				size = UDim2.fromOffset(CELL, CELL),
				position = UDim2.fromOffset((x - 1) * (CELL + GAP), (y - 1) * (CELL + GAP)),
				color = Theme.Colors.panelDeep,
				strokeColor = Theme.Colors.border,
				strokeTransparency = 0.6,
				corner = UDim.new(0, 2),
			})
			cellRefs[x .. "," .. y] = cell
			cell.MouseEnter:Connect(function()
				if dragState then
					cell.BackgroundColor3 = Theme.Colors.panelAlt
				end
			end)
			cell.MouseLeave:Connect(function()
				cell.BackgroundColor3 = Theme.Colors.panelDeep
			end)
		end
	end

	for uid, entry in pairs(container.items) do
		local def = describe(entry)
		local w = def.w or 1
		local h = def.h or 1
		local holder = UI.frame(containerView, {
			name = "Item_" .. uid,
			size = UDim2.fromOffset((CELL + GAP) * w - GAP, (CELL + GAP) * h - GAP),
			position = UDim2.fromOffset((entry.x - 1) * (CELL + GAP), (entry.y - 1) * (CELL + GAP)),
			transparency = 1,
			stroke = false,
			zIndex = 5,
		})
		local icon = drawItemIcon(holder, entry)
		icon.MouseEnter:Connect(function() showTooltip(entry, activeContainer) end)
		icon.MouseLeave:Connect(function() hideTooltip() end)
		icon.InputBegan:Connect(function(input)
			if input.UserInputType == Enum.UserInputType.MouseButton1 then
				selectedUid = uid
				dragState = { uid = uid, entry = entry, container = activeContainer, startPos = input.Position }
			elseif input.UserInputType == Enum.UserInputType.MouseButton2 then
				selectedUid = uid
				InventoryUI.ShowActions(entry, activeContainer)
			end
		end)
	end

	if weightLabel then
		weightLabel.Text = string.format("%.1f / %.0f kg", inventory.weight or 0, inventory.capacity or 22)
	end
end

function InventoryUI.DrawEquipment()
	if not root then return end
	local inventory = Store.get("inventory")
	for _, slotName in ipairs({ "Primary", "Secondary", "Melee", "Head", "Armor" }) do
		local ref = equipmentRefs[slotName]
		if ref then
			UI.clear(ref)
			local entry = (inventory.equipment or {})[slotName]
			if entry then
				local def = describe(entry)
				UI.label(ref, {
					size = UDim2.fromScale(1, 1),
					text = string.upper(string.sub(def.name, 1, 14)),
					textSize = 10,
					font = Theme.Fonts.bold,
					textColor = Theme.rarityColor(def.rarity),
					alignX = Enum.TextXAlignment.Center,
					wrapped = true,
				})
			else
				UI.label(ref, {
					size = UDim2.fromScale(1, 1),
					text = string.upper(slotName),
					textSize = 9,
					font = Theme.Fonts.mono,
					textColor = Theme.Colors.textFaint,
					alignX = Enum.TextXAlignment.Center,
				})
			end
		end
	end
end

function InventoryUI.ShowActions(entry, containerId)
	local def = describe(entry)
	local panel = UI.frame(gui, {
		name = "ItemActions",
		size = UDim2.fromOffset(190, 158),
		position = UDim2.fromOffset(UserInputService:GetMouseLocation().X + 6, UserInputService:GetMouseLocation().Y + 6),
		color = Theme.Colors.panel,
		zIndex = 40,
		padding = 6,
	})
	local title = UI.label(panel, {
		size = UDim2.new(1, 0, 0, 18),
		text = string.upper(def.name),
		textSize = 11,
		font = Theme.Fonts.bold,
		textColor = Theme.rarityColor(def.rarity),
	})
	local actions = {}
	if def.cat == "medical" or (def.use and (def.use.kind == "food" or def.use.kind == "drink")) then
		table.insert(actions, { "Use", function() Network.fire("RequestUseItem", entry.uid) end })
	end
	if def.cat == "weapon" or Weapons[entry.itemId] then
		table.insert(actions, { "Equip", function() Network.fire("RequestEquip", entry.uid) end })
	end
	if def.cat == "armor" then
		table.insert(actions, { "Wear", function() Network.fire("RequestEquip", entry.uid) end })
	end
	table.insert(actions, { "Drop 1", function() Network.fire("RequestDropItem", entry.uid, 1) end })
	if (entry.count or 1) > 1 then
		table.insert(actions, { "Drop All", function() Network.fire("RequestDropItem", entry.uid, entry.count) end })
	end

	local index = 0
	for _, action in ipairs(actions) do
		index += 1
		UI.button(panel, {
			size = UDim2.new(1, 0, 0, 24),
			position = UDim2.fromOffset(0, 20 + (index - 1) * 26),
			text = action[1],
			color = Theme.Colors.panelAlt,
			textSize = 12,
			zIndex = 41,
		}).MouseButton1Click:Connect(function()
			action[2]()
			panel:Destroy()
		end)
	end
	-- close on outside click
	task.delay(0.05, function()
		local connection
		connection = UserInputService.InputBegan:Connect(function(input)
			if input.UserInputType == Enum.UserInputType.MouseButton1 then
				local mouse = UserInputService:GetMouseLocation()
				local pos = panel.AbsolutePosition
				local size = panel.AbsoluteSize
				local inside = mouse.X >= pos.X and mouse.X <= pos.X + size.X
					and mouse.Y >= pos.Y and mouse.Y <= pos.Y + size.Y
				if not inside and panel.Parent then
					panel:Destroy()
				end
				if connection then connection:Disconnect() end
			end
		end)
	end)
end

local function cellUnderMouse()
	local mouse = UserInputService:GetMouseLocation()
	local container = (Store.get("inventory").containers or {})[activeContainer]
	if not container then return nil end
	for key, cell in pairs(cellRefs) do
		local pos = cell.AbsolutePosition
		local size = cell.AbsoluteSize
		if mouse.X >= pos.X and mouse.X <= pos.X + size.X and mouse.Y >= pos.Y and mouse.Y <= pos.Y + size.Y then
			local x, y = string.match(key, "(%d+),(%d+)")
			return tonumber(x), tonumber(y)
		end
	end
	-- fall back to arithmetic against the container origin
	local first = cellRefs["1,1"]
	if first then
		local origin = first.AbsolutePosition
		local x = math.floor((mouse.X - origin.X) / (CELL + GAP)) + 1
		local y = math.floor((mouse.Y - origin.Y) / (CELL + GAP)) + 1
		if x >= 1 and y >= 1 and x <= container.cols and y <= container.rows then
			return x, y
		end
	end
	return nil
end

function InventoryUI.Build(parent)
	gui = parent
	root = UI.frame(gui, {
		name = "Inventory",
		size = UDim2.fromOffset(760, 470),
		position = UDim2.fromScale(0.5, 0.5),
		anchor = Vector2.new(0.5, 0.5),
		color = Theme.Colors.bg,
		visible = false,
		zIndex = 20,
	})

	-- header
	UI.label(root, {
		size = UDim2.new(1, -24, 0, 30),
		position = UDim2.fromOffset(16, 10),
		text = "EQUIPMENT / CARGO",
		textSize = 16,
		font = Theme.Fonts.bold,
		textColor = Theme.Colors.text,
	})
	UI.label(root, {
		size = UDim2.fromOffset(400, 16),
		position = UDim2.fromOffset(16, 40),
		text = "Click to drag · right click for actions · ESC to close",
		textSize = 10,
		font = Theme.Fonts.mono,
		textColor = Theme.Colors.textFaint,
	})
	weightLabel = UI.label(root, {
		size = UDim2.fromOffset(200, 16),
		position = UDim2.new(1, -216, 0, 38),
		text = "0 / 22 kg",
		textSize = 11,
		font = Theme.Fonts.mono,
		textColor = Theme.Colors.textDim,
		alignX = Enum.TextXAlignment.Right,
	})

	-- equipment column
	local equip = UI.frame(root, {
		name = "Equipment",
		size = UDim2.fromOffset(200, 258),
		position = UDim2.fromOffset(16, 64),
		color = Theme.Colors.panel,
		padding = 8,
	})
	UI.label(equip, {
		size = UDim2.new(1, 0, 0, 16),
		text = "LOADOUT",
		textSize = 10,
		font = Theme.Fonts.mono,
		textColor = Theme.Colors.textFaint,
	})
	local slotNames = { "Primary", "Secondary", "Melee", "Head", "Armor" }
	for index, slotName in ipairs(slotNames) do
		local holder = UI.frame(equip, {
			size = UDim2.new(1, 0, 0, 40),
			position = UDim2.fromOffset(0, 20 + (index - 1) * 44),
			color = Theme.Colors.panelDeep,
			strokeColor = Theme.Colors.border,
			zIndex = 21,
		})
		holder.InputBegan:Connect(function(input)
			if input.UserInputType == Enum.UserInputType.MouseButton1 then
				Network.fire("RequestEquip", slotName)
			end
		end)
		equipmentRefs[slotName] = holder
	end

	-- container tabs
	local tabHolder = UI.frame(root, {
		size = UDim2.fromOffset(520, 26),
		position = UDim2.fromOffset(228, 64),
		transparency = 1,
		stroke = false,
	})
	local tabNames = { "Backpack", "Pockets", "Ammo" }
	for index, name in ipairs(tabNames) do
		local tab = UI.button(tabHolder, {
			size = UDim2.fromOffset(110, 24),
			position = UDim2.fromOffset((index - 1) * 116, 0),
			text = string.upper(name),
			color = Theme.Colors.panel,
			textSize = 10,
			font = Theme.Fonts.mono,
			zIndex = 21,
		})
		tab.MouseButton1Click:Connect(function()
			activeContainer = name
			InventoryUI.Redraw()
		end)
	end

	containerView = UI.frame(root, {
		name = "Container",
		size = UDim2.fromOffset(520, 340),
		position = UDim2.fromOffset(228, 96),
		transparency = 1,
		stroke = false,
		zIndex = 21,
	})

	tooltip = UI.frame(gui, {
		name = "Tooltip",
		size = UDim2.fromOffset(230, 104),
		position = UDim2.fromOffset(0, 0),
		color = Theme.Colors.panel,
		visible = false,
		zIndex = 60,
		padding = 8,
	})
	UI.label(tooltip, {
		size = UDim2.new(1, 0, 0, 18),
		text = "ITEM",
		textSize = 12,
		font = Theme.Fonts.bold,
		textColor = Theme.Colors.accent,
		name = "Title",
		zIndex = 61,
	})
	UI.label(tooltip, {
		size = UDim2.new(1, 0, 0, 56),
		position = UDim2.fromOffset(0, 20),
		text = "",
		textSize = 10,
		font = Theme.Fonts.regular,
		textColor = Theme.Colors.textDim,
		wrapped = true,
		alignY = Enum.TextYAlignment.Top,
		name = "Body",
		zIndex = 61,
	})
	UI.label(tooltip, {
		size = UDim2.new(1, 0, 0, 14),
		position = UDim2.new(0, 0, 1, -16),
		text = "",
		textSize = 9,
		font = Theme.Fonts.mono,
		textColor = Theme.Colors.textFaint,
		name = "Meta",
		zIndex = 61,
	})

	-- drag handling
	UserInputService.InputEnded:Connect(function(input)
		if input.UserInputType == Enum.UserInputType.MouseButton1 and dragState then
			local state = dragState
			dragState = nil
			if input.Position and state.startPos and (input.Position - state.startPos).Magnitude < 4 then
				return
			end
			local x, y = cellUnderMouse()
			if x and y and state.container then
				Network.fire("RequestMoveItem", state.uid, state.container, activeContainer, x, y)
			end
			InventoryUI.Redraw()
		end
	end)
	UserInputService.InputChanged:Connect(function(input)
		if dragState and tooltip then
			local mouse = UserInputService:GetMouseLocation()
			tooltip.Position = UDim2.fromOffset(mouse.X + 14, mouse.Y + 8)
		end
	end)

	Store.on("inventory", function()
		if root and root.Visible then
			InventoryUI.Redraw()
			InventoryUI.DrawEquipment()
		end
	end)
end

function InventoryUI.Open(containerId)
	if not root then return end
	root.Visible = true
	if containerId then activeContainer = containerId end
	InventoryUI.Redraw()
	InventoryUI.DrawEquipment()
end

function InventoryUI.Close()
	if root then root.Visible = false end
	hideTooltip()
end

function InventoryUI.Toggle()
	if not root then return end
	if root.Visible then
		InventoryUI.Close()
	else
		InventoryUI.Open()
	end
	return root.Visible
end

function InventoryUI.IsOpen()
	return root ~= nil and root.Visible
end

return InventoryUI
