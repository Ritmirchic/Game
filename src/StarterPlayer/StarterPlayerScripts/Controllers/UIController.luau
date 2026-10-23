--!strict
--[[
    ASHES OF HUMANITY: LAST SIGNAL
    StarterPlayerScripts.Controllers.UIController
    Dark Minimal Industrial Survival Horror Interface (Desktop + Mobile Touch).
    Covers Vitals HUD, Spatial Grid Inventory, Loot Extraction, 6-Region Medical Diagram,
    Workstation Crafting, Base Building Bar, Quest & Lore Log, Co-op Party, and Settings (Section 33).
]]

local Players = game:GetService("Players")
local ReplicatedStorage = game:GetService("ReplicatedStorage")
local UserInputService = game:GetService("UserInputService")

local BuildingDefinitions = require(ReplicatedStorage.Shared.BuildingDefinitions)
local CraftingRecipes = require(ReplicatedStorage.Shared.CraftingRecipes)
local ItemDefinitions = require(ReplicatedStorage.Shared.ItemDefinitions)
local NetworkSchema = require(ReplicatedStorage.Shared.NetworkSchema)
local QuestDefinitions = require(ReplicatedStorage.Shared.QuestDefinitions)
local WeaponDefinitions = require(ReplicatedStorage.Shared.WeaponDefinitions)

local UIController = {}

local LocalPlayer = Players.LocalPlayer
local registry: any = nil

-- Grounded Dark Industrial Color Palette (Zero bright neon/cartoon colors)
local COL_BG_DARK = Color3.fromRGB(16, 18, 21)
local COL_BG_PANEL = Color3.fromRGB(24, 27, 32)
local COL_BG_CELL = Color3.fromRGB(32, 36, 42)
local COL_BORDER = Color3.fromRGB(62, 68, 76)
local COL_TEXT_MAIN = Color3.fromRGB(208, 212, 216)
local COL_TEXT_DIM = Color3.fromRGB(136, 142, 148)
local COL_AMBER = Color3.fromRGB(196, 148, 76)
local COL_CRIMSON = Color3.fromRGB(178, 62, 56)
local COL_OLIVE = Color3.fromRGB(98, 146, 96)
local COL_RAD = Color3.fromRGB(172, 142, 62)

local screenGui: ScreenGui = nil :: any
local vitalsBars: { [string]: { Fill: Frame, ValueLabel: TextLabel } } = {}
local regionHeaderLabel: TextLabel = nil :: any
local weaponReadoutLabel: TextLabel = nil :: any
local promptLabel: TextLabel = nil :: any
local hitmarkerLabel: TextLabel = nil :: any
local toastLabel: TextLabel = nil :: any
local radioFrame: Frame = nil :: any
local radioTitle: TextLabel = nil :: any
local radioBody: TextLabel = nil :: any
local buildBarFrame: Frame = nil :: any
local buildInfoLabel: TextLabel = nil :: any
local modalRoot: Frame = nil :: any
local modalTitle: TextLabel = nil :: any
local modalContent: Frame = nil :: any

local selectedItemUid: string? = nil

local function styleStroke(parent: GuiObject, color: Color3?, thickness: number?)
	local s = Instance.new("UIStroke")
	s.Color = color or COL_BORDER
	s.Thickness = thickness or 1
	s.ApplyStrokeMode = Enum.ApplyStrokeMode.Border
	s.Parent = parent
end

local function makeButton(parent: Instance, text: string, size: UDim2, pos: UDim2, accent: Color3?): TextButton
	local btn = Instance.new("TextButton")
	btn.Size = size
	btn.Position = pos
	btn.BackgroundColor3 = accent or COL_BG_CELL
	btn.BorderSizePixel = 0
	btn.Font = Enum.Font.RobotoMono
	btn.TextSize = 13
	btn.TextColor3 = COL_TEXT_MAIN
	btn.Text = text
	btn.AutoButtonColor = true
	btn.Parent = parent
	styleStroke(btn, COL_BORDER, 1)
	return btn
end

local function makeLabel(
	parent: Instance,
	text: string,
	size: UDim2,
	pos: UDim2,
	fontSize: number?,
	align: Enum.TextXAlignment?
): TextLabel
	local lbl = Instance.new("TextLabel")
	lbl.Size = size
	lbl.Position = pos
	lbl.BackgroundTransparency = 1
	lbl.Font = Enum.Font.RobotoMono
	lbl.TextSize = fontSize or 13
	lbl.TextColor3 = COL_TEXT_MAIN
	lbl.TextXAlignment = align or Enum.TextXAlignment.Left
	lbl.TextWrapped = true
	lbl.Text = text
	lbl.Parent = parent
	return lbl
end

function UIController:Init(clientRegistry: any)
	registry = clientRegistry
end

function UIController:ShowToast(msg: string, isAlert: boolean?)
	if not toastLabel then
		return
	end
	toastLabel.Text = msg
	toastLabel.TextColor3 = if isAlert then COL_CRIMSON else COL_AMBER
	toastLabel.Visible = true
	local token = os.clock()
	toastLabel:SetAttribute("Token", token)
	task.delay(4.2, function()
		if toastLabel and toastLabel:GetAttribute("Token") == token then
			toastLabel.Visible = false
		end
	end)
end

function UIController:ShowRadioBanner(header: string, body: string, duration: number?)
	if not radioFrame then
		return
	end
	radioTitle.Text = header
	radioBody.Text = body
	radioFrame.Visible = true
	local token = os.clock()
	radioFrame:SetAttribute("Token", token)
	task.delay(duration or 8.5, function()
		if radioFrame and radioFrame:GetAttribute("Token") == token then
			radioFrame.Visible = false
		end
	end)
end

function UIController:BuildHUD()
	local playerGui = LocalPlayer:WaitForChild("PlayerGui")
	local existing = playerGui:FindFirstChild("MainInterface")
	if existing and existing:IsA("ScreenGui") then
		existing:Destroy()
	end

	screenGui = Instance.new("ScreenGui")
	screenGui.Name = "MainInterface"
	screenGui.ResetOnSpawn = false
	screenGui.IgnoreGuiInset = true
	screenGui.ZIndexBehavior = Enum.ZIndexBehavior.Sibling
	screenGui.Parent = playerGui

	-- 1. Top-Left Minimal Navigation Strip + Sector Telemetry
	local topBar = Instance.new("Frame")
	topBar.Name = "TopTelemetryStrip"
	topBar.Size = UDim2.new(0, 650, 0, 56)
	topBar.Position = UDim2.new(0, 16, 0, 14)
	topBar.BackgroundColor3 = COL_BG_DARK
	topBar.BackgroundTransparency = 0.18
	topBar.BorderSizePixel = 0
	topBar.Parent = screenGui
	styleStroke(topBar, COL_BORDER, 1)

	regionHeaderLabel = makeLabel(
		topBar,
		"SECTOR: SHELTER ECHO-07  |  07:30  |  CLEAR  |  SHELTERED",
		UDim2.new(1, -16, 0, 22),
		UDim2.new(0, 8, 0, 4),
		12
	)
	regionHeaderLabel.TextColor3 = COL_AMBER

	local navButtons = {
		{ Label = "INV [TAB]", Panel = "Inventory" },
		{ Label = "MED [H]", Panel = "Medical" },
		{ Label = "CRAFT [K]", Panel = "Crafting" },
		{ Label = "BUILD [B]", Panel = "__BUILD__" },
		{ Label = "LOG [J]", Panel = "Quests" },
		{ Label = "CO-OP [P]", Panel = "Party" },
		{ Label = "CFG [O]", Panel = "Settings" },
	}
	for i, nav in ipairs(navButtons) do
		local btn = makeButton(topBar, nav.Label, UDim2.new(0, 86, 0, 22), UDim2.new(0, 8 + (i - 1) * 90, 0, 28))
		btn.TextSize = 11
		btn.MouseButton1Click:Connect(function()
			if registry.AudioController then
				registry.AudioController:PlayUIClick()
			end
			if nav.Panel == "__BUILD__" then
				local st = registry.ClientController.State
				st.BuildMode = not st.BuildMode
				if st.BuildMode then
					st.ActivePanel = "None"
					self:RefreshModalPanel()
				end
				self:RefreshBuildBar()
			else
				registry.ClientController:TogglePanel(nav.Panel)
			end
		end)
	end

	-- 2. Bottom-Left Grounded Survival Vitals Cluster
	local vitalsPanel = Instance.new("Frame")
	vitalsPanel.Name = "SurvivalVitalsHUD"
	vitalsPanel.Size = UDim2.new(0, 272, 0, 174)
	vitalsPanel.Position = UDim2.new(0, 16, 1, -190)
	vitalsPanel.BackgroundColor3 = COL_BG_DARK
	vitalsPanel.BackgroundTransparency = 0.18
	vitalsPanel.BorderSizePixel = 0
	vitalsPanel.Parent = screenGui
	styleStroke(vitalsPanel, COL_BORDER, 1)

	local barRows = {
		{ Key = "Health", Label = "HP", Color = COL_CRIMSON },
		{ Key = "Stamina", Label = "STM", Color = COL_AMBER },
		{ Key = "Hunger", Label = "HNG", Color = COL_OLIVE },
		{ Key = "Thirst", Label = "THR", Color = Color3.fromRGB(78, 132, 164) },
		{ Key = "Radiation", Label = "RAD", Color = COL_RAD },
		{ Key = "Stress", Label = "STR", Color = Color3.fromRGB(138, 96, 142) },
	}
	for i, row in ipairs(barRows) do
		local y = 8 + (i - 1) * 22
		local tag = makeLabel(vitalsPanel, row.Label, UDim2.new(0, 36, 0, 16), UDim2.new(0, 8, 0, y), 11)
		tag.TextColor3 = COL_TEXT_DIM

		local track = Instance.new("Frame")
		track.Size = UDim2.new(0, 168, 0, 10)
		track.Position = UDim2.new(0, 46, 0, y + 3)
		track.BackgroundColor3 = COL_BG_CELL
		track.BorderSizePixel = 0
		track.Parent = vitalsPanel

		local fill = Instance.new("Frame")
		fill.Size = UDim2.new(1, 0, 1, 0)
		fill.BackgroundColor3 = row.Color
		fill.BorderSizePixel = 0
		fill.Parent = track

		local valLbl = makeLabel(
			vitalsPanel,
			"100",
			UDim2.new(0, 44, 0, 16),
			UDim2.new(0, 220, 0, y),
			11,
			Enum.TextXAlignment.Right
		)
		vitalsBars[row.Key] = { Fill = fill, ValueLabel = valLbl }
	end

	local statusFooter = makeLabel(
		vitalsPanel,
		"TEMP 37.0°C  |  BAT 68%  |  FATIGUE 8%",
		UDim2.new(1, -16, 0, 22),
		UDim2.new(0, 8, 0, 144),
		11
	)
	statusFooter.Name = "StatusFooter"
	statusFooter.TextColor3 = COL_TEXT_DIM

	-- 3. Bottom-Center Weapon & Quickbar Strip
	local weaponStrip = Instance.new("Frame")
	weaponStrip.Name = "WeaponTelemetryStrip"
	weaponStrip.Size = UDim2.new(0, 380, 0, 46)
	weaponStrip.Position = UDim2.new(0.5, -190, 1, -62)
	weaponStrip.BackgroundColor3 = COL_BG_DARK
	weaponStrip.BackgroundTransparency = 0.18
	weaponStrip.BorderSizePixel = 0
	weaponStrip.Parent = screenGui
	styleStroke(weaponStrip, COL_BORDER, 1)

	weaponReadoutLabel = makeLabel(
		weaponStrip,
		"[1] PRIMARY  [2] SIDEARM  [3] MELEE   |   UNARMED",
		UDim2.new(1, -16, 1, 0),
		UDim2.new(0, 8, 0, 0),
		12,
		Enum.TextXAlignment.Center
	)

	-- 4. Center Crosshair, Hitmarker & Context Prompt
	local reticle = makeLabel(
		screenGui,
		"+",
		UDim2.new(0, 24, 0, 24),
		UDim2.new(0.5, -12, 0.5, -12),
		18,
		Enum.TextXAlignment.Center
	)
	reticle.TextColor3 = Color3.fromRGB(210, 214, 218)
	reticle.TextTransparency = 0.25

	hitmarkerLabel = makeLabel(
		screenGui,
		"×",
		UDim2.new(0, 32, 0, 32),
		UDim2.new(0.5, -16, 0.5, -16),
		24,
		Enum.TextXAlignment.Center
	)
	hitmarkerLabel.TextColor3 = COL_CRIMSON
	hitmarkerLabel.Visible = false

	promptLabel = makeLabel(
		screenGui,
		"",
		UDim2.new(0, 380, 0, 28),
		UDim2.new(0.5, -190, 0.5, 28),
		14,
		Enum.TextXAlignment.Center
	)
	promptLabel.BackgroundColor3 = COL_BG_DARK
	promptLabel.BackgroundTransparency = 0.25
	promptLabel.TextColor3 = COL_AMBER
	promptLabel.Visible = false
	styleStroke(promptLabel, COL_BORDER, 1)

	-- 5. Top-Center Radio Transmission / Lore Document / World Event Feed
	radioFrame = Instance.new("Frame")
	radioFrame.Name = "RadioReceiverBanner"
	radioFrame.Size = UDim2.new(0, 520, 0, 108)
	radioFrame.Position = UDim2.new(0.5, -260, 0, 78)
	radioFrame.BackgroundColor3 = COL_BG_DARK
	radioFrame.BackgroundTransparency = 0.08
	radioFrame.BorderSizePixel = 0
	radioFrame.Visible = false
	radioFrame.Parent = screenGui
	styleStroke(radioFrame, COL_AMBER, 1)

	radioTitle = makeLabel(radioFrame, "142.8 MHz // SIGNAL", UDim2.new(1, -54, 0, 22), UDim2.new(0, 10, 0, 6), 13)
	radioTitle.TextColor3 = COL_AMBER

	local closeRadio = makeButton(radioFrame, "X", UDim2.new(0, 28, 0, 20), UDim2.new(1, -34, 0, 6))
	closeRadio.MouseButton1Click:Connect(function()
		radioFrame.Visible = false
	end)

	radioBody = makeLabel(radioFrame, "", UDim2.new(1, -20, 0, 72), UDim2.new(0, 10, 0, 30), 12)

	toastLabel =
		makeLabel(screenGui, "", UDim2.new(0, 440, 0, 26), UDim2.new(0.5, -220, 0, 44), 13, Enum.TextXAlignment.Center)
	toastLabel.Visible = false

	-- 6. Base Building Bar
	buildBarFrame = Instance.new("Frame")
	buildBarFrame.Size = UDim2.new(0, 520, 0, 54)
	buildBarFrame.Position = UDim2.new(0.5, -260, 1, -122)
	buildBarFrame.BackgroundColor3 = COL_BG_PANEL
	buildBarFrame.BorderSizePixel = 0
	buildBarFrame.Visible = false
	buildBarFrame.Parent = screenGui
	styleStroke(buildBarFrame, COL_AMBER, 1)

	buildInfoLabel =
		makeLabel(buildBarFrame, "", UDim2.new(1, -16, 1, 0), UDim2.new(0, 8, 0, 0), 12, Enum.TextXAlignment.Center)

	-- 7. Central Modal Window (Inventory, Loot, Medical, Crafting, Quests, Party, Settings)
	modalRoot = Instance.new("Frame")
	modalRoot.Name = "ModalWindow"
	modalRoot.Size = UDim2.new(0, 760, 0, 470)
	modalRoot.Position = UDim2.new(0.5, -380, 0.5, -235)
	modalRoot.BackgroundColor3 = COL_BG_PANEL
	modalRoot.BackgroundTransparency = 0.04
	modalRoot.BorderSizePixel = 0
	modalRoot.Visible = false
	modalRoot.Parent = screenGui
	styleStroke(modalRoot, COL_BORDER, 1)

	modalTitle = makeLabel(modalRoot, "INVENTORY", UDim2.new(1, -90, 0, 30), UDim2.new(0, 14, 0, 6), 15)
	modalTitle.TextColor3 = COL_AMBER

	local closeModal = makeButton(modalRoot, "CLOSE [ESC]", UDim2.new(0, 96, 0, 24), UDim2.new(1, -106, 0, 8))
	closeModal.MouseButton1Click:Connect(function()
		registry.ClientController.State.ActivePanel = "None"
		self:RefreshModalPanel()
	end)

	modalContent = Instance.new("Frame")
	modalContent.Size = UDim2.new(1, -24, 1, -48)
	modalContent.Position = UDim2.new(0, 12, 0, 38)
	modalContent.BackgroundTransparency = 1
	modalContent.Parent = modalRoot

	-- 8. Mobile Touch Controls (auto-shown on TouchEnabled devices)
	if UserInputService.TouchEnabled then
		self:BuildMobileTouchPad()
	end
end

function UIController:BuildMobileTouchPad()
	local pad = Instance.new("Frame")
	pad.Name = "MobileActionCluster"
	pad.Size = UDim2.new(0, 230, 0, 165)
	pad.Position = UDim2.new(1, -244, 1, -185)
	pad.BackgroundTransparency = 1
	pad.Parent = screenGui

	local actions = {
		{
			Text = "FIRE",
			Pos = UDim2.new(0, 145, 0, 55),
			Fn = function()
				registry.WeaponController:AttemptFireOrSwing()
			end,
		},
		{
			Text = "AIM",
			Pos = UDim2.new(0, 72, 0, 55),
			Fn = function()
				local nextADS = not registry.CameraController:IsADS()
				registry.WeaponController:SetADSIntent(nextADS)
			end,
		},
		{
			Text = "RLD",
			Pos = UDim2.new(0, 0, 0, 55),
			Fn = function()
				registry.WeaponController:RequestReload()
			end,
		},
		{
			Text = "USE [E]",
			Pos = UDim2.new(0, 145, 0, 0),
			Fn = function()
				registry.ClientController:TriggerContextInteract()
			end,
		},
		{
			Text = "LIGHT",
			Pos = UDim2.new(0, 72, 0, 0),
			Fn = function()
				registry.MovementController:ToggleFlashlight()
			end,
		},
		{
			Text = "VAULT",
			Pos = UDim2.new(0, 0, 0, 0),
			Fn = function()
				registry.MovementController:TryVaultObstacle()
			end,
		},
		{
			Text = "SPRINT",
			Pos = UDim2.new(0, 145, 0, 112),
			Fn = function()
				local s = registry.MovementController:GetStance() ~= "Sprint"
				registry.MovementController:SetSprintIntent(s)
			end,
		},
		{
			Text = "CROUCH",
			Pos = UDim2.new(0, 72, 0, 112),
			Fn = function()
				registry.MovementController:ToggleCrouch()
			end,
		},
	}
	for _, act in ipairs(actions) do
		local b = makeButton(pad, act.Text, UDim2.new(0, 68, 0, 46), act.Pos)
		b.BackgroundTransparency = 0.25
		b.MouseButton1Click:Connect(act.Fn)
	end
end

function UIController:UpdateHUDTelemetry()
	local st = registry.ClientController.State
	local v = st.Vitals
	if not v or not regionHeaderLabel then
		return
	end

	local hours = math.floor(st.ClockTime or 7)
	local mins = math.floor(((st.ClockTime or 7) - hours) * 60)
	local shelterStr = if v.IsSheltered then "SHELTERED" else "EXPOSED"
	regionHeaderLabel.Text = string.format(
		"SECTOR: %s  |  DAY %d %02d:%02d  |  %s  |  %s",
		string.upper(tostring(v.CurrentRegion or "Shelter")),
		st.DayCount or 1,
		hours,
		mins,
		string.upper(tostring(st.Weather or "Clear")),
		shelterStr
	)

	local function setBar(key: string, val: number, maxVal: number)
		local entry = vitalsBars[key]
		if entry then
			local ratio = math.clamp(val / math.max(1, maxVal), 0, 1)
			entry.Fill.Size = UDim2.new(ratio, 0, 1, 0)
			entry.ValueLabel.Text = tostring(math.floor(val + 0.5))
		end
	end

	setBar("Health", v.Health or 100, v.MaxHealth or 100)
	setBar("Stamina", v.Stamina or 100, v.MaxStaminaCap or 100)
	setBar("Hunger", v.Hunger or 100, 100)
	setBar("Thirst", v.Thirst or 100, 100)
	setBar("Radiation", v.Radiation or 0, 1000)
	setBar("Stress", v.MentalStress or 0, 100)

	local footer = screenGui:FindFirstChild("SurvivalVitalsHUD")
		and (screenGui :: any).SurvivalVitalsHUD:FindFirstChild("StatusFooter") :: TextLabel?
	if footer then
		local bleedTag = if (v.BleedStacks or 0) > 0 then " | BLEEDING!" else ""
		local downTag = if v.IsDowned then " | DOWNED!" else ""
		footer.Text = string.format(
			"%.1f°C | BAT %d%% | INF %d%%%s%s",
			v.BodyTemperature or 37.0,
			math.floor(v.FlashlightBattery or 0),
			math.floor(v.Infection or 0),
			bleedTag,
			downTag
		)
		footer.TextColor3 = if (v.BleedStacks or 0) > 0 or v.IsDowned then COL_CRIMSON else COL_TEXT_DIM
	end

	if registry.WeaponController then
		local wpnInst, wpnDef = registry.WeaponController:GetActiveWeaponInfo()
		if wpnInst and wpnDef then
			local ammoText = if wpnDef.MagSize > 0
				then string.format("AMMO %d/%d", wpnInst.ClipAmmo or 0, wpnDef.MagSize)
				else "MELEE"
			weaponReadoutLabel.Text = string.format(
				"[%s] %s  |  %s  |  COND %d%%",
				string.upper(st.ActiveSlot or "PRIMARY"),
				string.upper(wpnDef.Name),
				ammoText,
				math.floor(wpnInst.Durability or 100)
			)
		else
			weaponReadoutLabel.Text = "[1] PRIMARY   [2] SIDEARM   [3] MELEE   |   UNARMED"
		end
	end
end

function UIController:RefreshBuildBar()
	local st = registry.ClientController.State
	buildBarFrame.Visible = st.BuildMode
	if not st.BuildMode then
		return
	end
	local bId = registry.ClientController.BuildingOrder[st.SelectedBuildingIndex] or "WoodWall"
	local def = BuildingDefinitions.Get(bId)
	if def then
		local costParts = {}
		for _, c in ipairs(def.Costs) do
			table.insert(costParts, c.ItemId .. " x" .. tostring(c.Count))
		end
		buildInfoLabel.Text = string.format(
			"BUILD: %s (%d°)  |  COST: %s  |  [LMB] Place  [ / ] Cycle  [R] Rotate",
			string.upper(def.Name),
			st.BuildYawDeg,
			table.concat(costParts, ", ")
		)
	end
end

local function clearChildren(frame: Instance)
	for _, c in ipairs(frame:GetChildren()) do
		c:Destroy()
	end
end

function UIController:RenderInventoryModal()
	modalTitle.Text = "SPATIAL BACKPACK GRID & EQUIPMENT"
	local inv = registry.ClientController.State.Inventory
	local cols = inv.Cols or 8
	local rows = inv.Rows or 5
	local cellPx = 42

	-- Spatial Grid Cells (Click empty cell to move selected item there!)
	local gridFrame = Instance.new("Frame")
	gridFrame.Size = UDim2.new(0, cols * cellPx, 0, rows * cellPx)
	gridFrame.Position = UDim2.new(0, 8, 0, 8)
	gridFrame.BackgroundColor3 = COL_BG_DARK
	gridFrame.BorderSizePixel = 0
	gridFrame.Parent = modalContent
	styleStroke(gridFrame, COL_BORDER, 1)

	for gy = 1, rows do
		for gx = 1, cols do
			local cellBtn = makeButton(
				gridFrame,
				"",
				UDim2.new(0, cellPx - 2, 0, cellPx - 2),
				UDim2.new(0, (gx - 1) * cellPx + 1, 0, (gy - 1) * cellPx + 1),
				COL_BG_CELL
			)
			cellBtn.MouseButton1Click:Connect(function()
				if selectedItemUid and inv.Items[selectedItemUid] then
					local cur = inv.Items[selectedItemUid]
					registry.ClientController:FireServer(NetworkSchema.EVENTS.InventoryAction, {
						Action = "Move",
						UID = selectedItemUid,
						X = gx,
						Y = gy,
						Rotated = cur.Rotated == true,
					})
				end
			end)
		end
	end

	-- Render Unequipped Item Blocks atop Spatial Grid (X > 0, Y > 0)
	local totalWeight = 0.0
	for uid, inst in pairs(inv.Items or {}) do
		local def = ItemDefinitions.Get(inst.ItemId)
		if def then
			totalWeight += def.WeightKg * inst.Count
			if inst.X > 0 and inst.Y > 0 then
				local w = if inst.Rotated then def.Height else def.Width
				local h = if inst.Rotated then def.Width else def.Height
				local isSel = (selectedItemUid == uid)
				local itemBtn = makeButton(
					gridFrame,
					string.format("%s\nx%d", string.sub(def.Name, 1, 10), inst.Count),
					UDim2.new(0, w * cellPx - 4, 0, h * cellPx - 4),
					UDim2.new(0, (inst.X - 1) * cellPx + 2, 0, (inst.Y - 1) * cellPx + 2),
					if isSel then Color3.fromRGB(68, 58, 42) else Color3.fromRGB(46, 52, 60)
				)
				itemBtn.TextSize = 11
				itemBtn.ZIndex = 4
				styleStroke(itemBtn, if isSel then COL_AMBER else COL_BORDER, 1)
				itemBtn.MouseButton1Click:Connect(function()
					selectedItemUid = uid
					self:RefreshModalPanel()
				end)
			end
		end
	end

	-- Weight & Sort Bar
	makeLabel(
		modalContent,
		string.format("CARRY WEIGHT: %.1f / %.1f kg   (%dx%d GRID)", totalWeight, inv.MaxWeightKg or 24, cols, rows),
		UDim2.new(0, 340, 0, 22),
		UDim2.new(0, 8, 0, rows * cellPx + 16),
		12
	).TextColor3 =
		COL_AMBER

	local sortBtn =
		makeButton(modalContent, "AUTO-SORT GRID", UDim2.new(0, 130, 0, 26), UDim2.new(0, 8, 0, rows * cellPx + 42))
	sortBtn.MouseButton1Click:Connect(function()
		registry.ClientController:FireServer(NetworkSchema.EVENTS.InventoryAction, { Action = "Sort" })
	end)

	-- Selected Item Inspector & Actions
	local inspX = cols * cellPx + 24
	local selInst = if selectedItemUid and inv.Items then inv.Items[selectedItemUid] else nil
	local selDef = if selInst then ItemDefinitions.Get(selInst.ItemId) else nil

	if selInst and selDef then
		makeLabel(
			modalContent,
			string.format(
				"%s (x%d) — Cond %d%%\n%s",
				selDef.Name,
				selInst.Count,
				math.floor(selInst.Durability),
				selDef.Description
			),
			UDim2.new(0, 340, 0, 68),
			UDim2.new(0, inspX, 0, 8),
			12
		)

		local useBtn = makeButton(modalContent, "USE / CONSUME", UDim2.new(0, 108, 0, 26), UDim2.new(0, inspX, 0, 82))
		useBtn.MouseButton1Click:Connect(function()
			registry.ClientController:FireServer(
				NetworkSchema.EVENTS.InventoryAction,
				{ Action = "Use", UID = selInst.UID }
			)
		end)

		if selDef.EquipSlot then
			local eqBtn = makeButton(
				modalContent,
				"EQUIP (" .. selDef.EquipSlot .. ")",
				UDim2.new(0, 148, 0, 26),
				UDim2.new(0, inspX + 114, 0, 82)
			)
			eqBtn.MouseButton1Click:Connect(function()
				registry.ClientController:FireServer(NetworkSchema.EVENTS.InventoryAction, {
					Action = "Equip",
					UID = selInst.UID,
					Slot = selDef.EquipSlot,
				})
			end)
		end

		local rotBtn = makeButton(modalContent, "ROTATE 90°", UDim2.new(0, 108, 0, 26), UDim2.new(0, inspX, 0, 114))
		rotBtn.MouseButton1Click:Connect(function()
			if selInst.X > 0 and selInst.Y > 0 then
				registry.ClientController:FireServer(NetworkSchema.EVENTS.InventoryAction, {
					Action = "Move",
					UID = selInst.UID,
					X = selInst.X,
					Y = selInst.Y,
					Rotated = not selInst.Rotated,
				})
			end
		end)

		local dropBtn = makeButton(
			modalContent,
			"DROP ITEM",
			UDim2.new(0, 108, 0, 26),
			UDim2.new(0, inspX + 114, 0, 114),
			Color3.fromRGB(64, 34, 34)
		)
		dropBtn.MouseButton1Click:Connect(function()
			registry.ClientController:FireServer(
				NetworkSchema.EVENTS.InventoryAction,
				{ Action = "Drop", UID = selInst.UID }
			)
			selectedItemUid = nil
		end)
	else
		makeLabel(
			modalContent,
			"Select an item block in the grid to inspect, consume, rotate, equip, or move to another cell.",
			UDim2.new(0, 340, 0, 48),
			UDim2.new(0, inspX, 0, 8),
			12
		).TextColor3 =
			COL_TEXT_DIM
	end

	-- 7 Equipment Slots Readout
	makeLabel(modalContent, "EQUIPPED LOADOUT:", UDim2.new(0, 260, 0, 20), UDim2.new(0, inspX, 0, 154), 12).TextColor3 =
		COL_AMBER
	local eqSlots = { "Primary", "Secondary", "Melee", "Headgear", "Armor", "Backpack", "Utility" }
	for idx, slot in ipairs(eqSlots) do
		local eqUid = inv.Equipped and inv.Equipped[slot]
		local eqItem = if eqUid and inv.Items then inv.Items[eqUid] else nil
		local eqDef = if eqItem then ItemDefinitions.Get(eqItem.ItemId) else nil
		local labelTxt = string.format("%s: %s", slot, if eqDef then eqDef.Name else "— Empty —")
		local slotBtn =
			makeButton(modalContent, labelTxt, UDim2.new(0, 330, 0, 24), UDim2.new(0, inspX, 0, 178 + (idx - 1) * 28))
		slotBtn.MouseButton1Click:Connect(function()
			if eqUid then
				registry.ClientController:FireServer(
					NetworkSchema.EVENTS.InventoryAction,
					{ Action = "Unequip", Slot = slot }
				)
			end
		end)
	end
end

function UIController:RenderLootModal()
	local loot = registry.ClientController.State.ActiveLootContainer
	modalTitle.Text = "LOOT CONTAINER // " .. string.upper(tostring(loot and loot.TableName or "CACHE"))
	if not loot or not loot.Items or #loot.Items == 0 then
		makeLabel(modalContent, "Container is empty.", UDim2.new(0, 300, 0, 30), UDim2.new(0, 12, 0, 12), 14)
		return
	end
	for i, entry in ipairs(loot.Items) do
		local def = ItemDefinitions.Get(entry.ItemId)
		local name = if def then def.Name else entry.ItemId
		local btn = makeButton(
			modalContent,
			string.format(
				"[TAKE]  %s  (x%d)  —  Cond %d%%",
				name,
				entry.Count or 1,
				math.floor(entry.Durability or 100)
			),
			UDim2.new(0, 460, 0, 32),
			UDim2.new(0, 12, 0, 10 + (i - 1) * 38)
		)
		btn.MouseButton1Click:Connect(function()
			registry.ClientController:FireServer(NetworkSchema.EVENTS.LootInteract, {
				Action = "Take",
				ContainerId = loot.ContainerId,
				SlotId = entry.SlotId,
			})
		end)
	end
end

function UIController:RenderMedicalModal()
	modalTitle.Text = "ANATOMICAL TRAUMA & MEDICAL TREATMENT"
	local bp = registry.ClientController.State.BodyRegions or {}
	local inv = registry.ClientController.State.Inventory
	local regions = { "Head", "Torso", "LeftArm", "RightArm", "LeftLeg", "RightLeg" }

	for i, reg in ipairs(regions) do
		local state = bp[reg] or { HP = 100, MaxHP = 100, Conditions = {} }
		local condList = {}
		for cName, active in pairs(state.Conditions or {}) do
			if active then
				table.insert(condList, cName)
			end
		end
		local condStr = if #condList > 0 then table.concat(condList, ", ") else "Nominal"
		local y = 10 + (i - 1) * 58

		local card = Instance.new("Frame")
		card.Size = UDim2.new(0, 430, 0, 50)
		card.Position = UDim2.new(0, 10, 0, y)
		card.BackgroundColor3 = COL_BG_DARK
		card.BorderSizePixel = 0
		card.Parent = modalContent
		styleStroke(card, if #condList > 0 then COL_CRIMSON else COL_BORDER, 1)

		makeLabel(
			card,
			string.format("%s: %d / %d HP   [%s]", string.upper(reg), math.floor(state.HP), state.MaxHP, condStr),
			UDim2.new(1, -16, 0, 22),
			UDim2.new(0, 8, 0, 4),
			12
		)

		-- Quick apply medical item directly to this limb
		local medItems = { "SterileBandage", "OrthopedicSplint", "AntisepticBottle", "SurgicalTraumaKit" }
		for mIdx, medId in ipairs(medItems) do
			local foundUid: string? = nil
			for uid, inst in pairs(inv.Items or {}) do
				if inst.ItemId == medId then
					foundUid = uid
					break
				end
			end
			local shortName =
				string.gsub(medId, "Sterile", ""):gsub("Orthopedic", ""):gsub("Bottle", ""):gsub("Surgical", "")
			local mBtn = makeButton(
				card,
				shortName,
				UDim2.new(0, 96, 0, 20),
				UDim2.new(0, 8 + (mIdx - 1) * 102, 0, 26),
				if foundUid then COL_BG_CELL else Color3.fromRGB(22, 24, 28)
			)
			mBtn.TextSize = 10
			mBtn.TextColor3 = if foundUid then COL_TEXT_MAIN else COL_TEXT_DIM
			mBtn.MouseButton1Click:Connect(function()
				if foundUid then
					registry.ClientController:FireServer(NetworkSchema.EVENTS.InventoryAction, {
						Action = "Use",
						UID = foundUid,
						BodyPart = reg,
					})
				end
			end)
		end
	end
end

function UIController:RenderCraftingModal()
	modalTitle.Text = "CRAFTING & WORKSTATION RECIPES"
	local recipeList = CraftingRecipes.ORDERED_LIST
	local scroll = Instance.new("ScrollingFrame")
	scroll.Size = UDim2.new(1, -16, 1, -12)
	scroll.Position = UDim2.new(0, 8, 0, 6)
	scroll.BackgroundTransparency = 1
	scroll.BorderSizePixel = 0
	scroll.ScrollBarThickness = 6
	scroll.CanvasSize = UDim2.new(0, 0, 0, #recipeList * 44 + 20)
	scroll.Parent = modalContent

	local inv = registry.ClientController.State.Inventory
	local counts: { [string]: number } = {}
	for _, inst in pairs(inv.Items or {}) do
		counts[inst.ItemId] = (counts[inst.ItemId] or 0) + inst.Count
	end

	for i, rec in ipairs(recipeList) do
		local ingStrs = {}
		local canAfford = true
		for _, ing in ipairs(rec.Ingredients) do
			local have = counts[ing.ItemId] or 0
			if have < ing.Count then
				canAfford = false
			end
			table.insert(ingStrs, string.format("%s %d/%d", ing.ItemId, have, ing.Count))
		end

		local row = Instance.new("Frame")
		row.Size = UDim2.new(1, -18, 0, 38)
		row.Position = UDim2.new(0, 4, 0, (i - 1) * 42)
		row.BackgroundColor3 = COL_BG_DARK
		row.BorderSizePixel = 0
		row.Parent = scroll
		styleStroke(row, COL_BORDER, 1)

		makeLabel(
			row,
			string.format("%s (x%d) [%s] — %s", rec.Name, rec.OutputCount, rec.Station, table.concat(ingStrs, ", ")),
			UDim2.new(1, -110, 1, 0),
			UDim2.new(0, 8, 0, 0),
			11
		)

		local cBtn = makeButton(
			row,
			"CRAFT",
			UDim2.new(0, 88, 0, 26),
			UDim2.new(1, -96, 0, 6),
			if canAfford then Color3.fromRGB(48, 72, 52) else COL_BG_CELL
		)
		cBtn.MouseButton1Click:Connect(function()
			registry.ClientController:FireServer(NetworkSchema.EVENTS.CraftRequest, { RecipeId = rec.Id })
		end)
	end
end

function UIController:RenderQuestsModal()
	modalTitle.Text = "MISSION OBJECTIVES & FIELD ARCHIVES"
	local scroll = Instance.new("ScrollingFrame")
	scroll.Size = UDim2.new(0.54, -8, 1, -12)
	scroll.Position = UDim2.new(0, 6, 0, 6)
	scroll.BackgroundTransparency = 1
	scroll.BorderSizePixel = 0
	scroll.ScrollBarThickness = 5
	scroll.CanvasSize = UDim2.new(0, 0, 0, 900)
	scroll.Parent = modalContent

	local y = 0
	for qId, prog in pairs(registry.ClientController.State.Quests or {}) do
		local qDef = (QuestDefinitions.QUESTS :: any)[qId]
		if qDef then
			local lines = { string.format("[%s] %s (%s)", string.upper(qDef.Kind), qDef.Title, prog.Status) }
			for _, obj in ipairs(qDef.Objectives) do
				local cur = prog.Objectives and prog.Objectives[obj.Id] or 0
				table.insert(lines, string.format("  • %s (%d/%d)", obj.Description, cur, obj.RequiredCount))
			end
			local h = 24 + #qDef.Objectives * 18
			local lbl = makeLabel(scroll, table.concat(lines, "\n"), UDim2.new(1, -12, 0, h), UDim2.new(0, 4, 0, y), 11)
			lbl.TextColor3 = if prog.Status == "Completed" then COL_OLIVE else COL_TEXT_MAIN
			y += h + 10
		end
	end
	scroll.CanvasSize = UDim2.new(0, 0, 0, y + 20)

	-- Right Half: Discovered Lore Documents
	makeLabel(modalContent, "RECOVERED LORE DOCUMENTS:", UDim2.new(0.42, 0, 0, 20), UDim2.new(0.56, 0, 0, 6), 12).TextColor3 =
		COL_AMBER
	local lIdx = 0
	for loreId, unlocked in pairs(registry.ClientController.State.DiscoveredLore or {}) do
		if unlocked then
			local lore = (QuestDefinitions.LORE_ENTRIES :: any)[loreId]
			if lore then
				lIdx += 1
				local b = makeButton(
					modalContent,
					lore.Title,
					UDim2.new(0.42, -8, 0, 26),
					UDim2.new(0.56, 0, 0, 30 + (lIdx - 1) * 30)
				)
				b.TextSize = 11
				b.MouseButton1Click:Connect(function()
					self:ShowRadioBanner("ARCHIVE // " .. lore.Title, lore.Text, 14)
				end)
			end
		end
	end
end

function UIController:RenderPartyModal()
	modalTitle.Text = "COOPERATIVE SURVIVOR PARTY"
	local st = registry.ClientController.State

	if st.PendingInvite then
		local accBtn = makeButton(
			modalContent,
			"ACCEPT INVITE FROM " .. tostring(st.PendingInvite.FromName),
			UDim2.new(0, 320, 0, 30),
			UDim2.new(0, 12, 0, 10),
			Color3.fromRGB(48, 76, 54)
		)
		accBtn.MouseButton1Click:Connect(function()
			registry.ClientController:FireServer(NetworkSchema.EVENTS.PartyAction, {
				Action = "Accept",
				PartyId = st.PendingInvite.PartyId,
			})
			st.PendingInvite = nil
			self:RefreshModalPanel()
		end)
	end

	if not st.Party then
		local createBtn =
			makeButton(modalContent, "CREATE SURVIVOR SQUAD", UDim2.new(0, 220, 0, 30), UDim2.new(0, 12, 0, 50))
		createBtn.MouseButton1Click:Connect(function()
			registry.ClientController:FireServer(NetworkSchema.EVENTS.PartyAction, { Action = "Create" })
		end)
	else
		local leaveBtn = makeButton(
			modalContent,
			"LEAVE SQUAD",
			UDim2.new(0, 160, 0, 28),
			UDim2.new(0, 12, 0, 50),
			Color3.fromRGB(68, 36, 36)
		)
		leaveBtn.MouseButton1Click:Connect(function()
			registry.ClientController:FireServer(NetworkSchema.EVENTS.PartyAction, { Action = "Leave" })
		end)

		for idx, mem in ipairs(st.Party.Members or {}) do
			makeLabel(
				modalContent,
				string.format(
					"• %s  —  HP %d/%d  —  %s%s",
					mem.Name,
					mem.Health,
					mem.MaxHealth,
					mem.Region,
					if mem.IsDowned then " [DOWNED!]" else ""
				),
				UDim2.new(0, 460, 0, 22),
				UDim2.new(0, 12, 0, 90 + (idx - 1) * 24),
				12
			)
		end
	end

	makeLabel(
		modalContent,
		"ONLINE SURVIVORS (CLICK TO INVITE):",
		UDim2.new(0, 300, 0, 20),
		UDim2.new(0, 12, 0, 220),
		12
	).TextColor3 =
		COL_AMBER
	local row = 0
	for _, other in ipairs(Players:GetPlayers()) do
		if other ~= LocalPlayer then
			row += 1
			local invBtn = makeButton(
				modalContent,
				"INVITE " .. other.Name,
				UDim2.new(0, 240, 0, 26),
				UDim2.new(0, 12, 0, 246 + (row - 1) * 30)
			)
			invBtn.MouseButton1Click:Connect(function()
				registry.ClientController:FireServer(NetworkSchema.EVENTS.PartyAction, {
					Action = "Invite",
					TargetUserId = other.UserId,
				})
			end)
		end
	end
end

function UIController:RenderSettingsModal()
	modalTitle.Text = "GRAPHICS QUALITY & ACCESSIBILITY SETTINGS"
	local cfg = registry.ClientController.State.Settings

	makeLabel(modalContent, "GRAPHICS PRESET:", UDim2.new(0, 200, 0, 24), UDim2.new(0, 12, 0, 16), 13).TextColor3 =
		COL_AMBER
	local presets = { "Low", "Medium", "High", "Ultra" }
	for i, pName in ipairs(presets) do
		local b = makeButton(
			modalContent,
			pName,
			UDim2.new(0, 96, 0, 28),
			UDim2.new(0, 12 + (i - 1) * 104, 0, 44),
			if cfg.GraphicsPreset == pName then Color3.fromRGB(64, 58, 44) else COL_BG_CELL
		)
		b.MouseButton1Click:Connect(function()
			cfg.GraphicsPreset = pName
			registry.ClientController:FireServer(NetworkSchema.EVENTS.SettingsUpdate, cfg)
			self:RefreshModalPanel()
		end)
	end

	local rmBtn = makeButton(
		modalContent,
		"REDUCE CAMERA MOTION: " .. (if cfg.ReduceMotion then "ON" else "OFF"),
		UDim2.new(0, 280, 0, 30),
		UDim2.new(0, 12, 0, 96)
	)
	rmBtn.MouseButton1Click:Connect(function()
		cfg.ReduceMotion = not cfg.ReduceMotion
		registry.ClientController:FireServer(NetworkSchema.EVENTS.SettingsUpdate, cfg)
		self:RefreshModalPanel()
	end)

	local fxBtn = makeButton(
		modalContent,
		"STRESS / VIGNETTE SCREEN FX: " .. (if cfg.ScreenEffects then "ON" else "OFF"),
		UDim2.new(0, 280, 0, 30),
		UDim2.new(0, 12, 0, 136)
	)
	fxBtn.MouseButton1Click:Connect(function()
		cfg.ScreenEffects = not cfg.ScreenEffects
		registry.ClientController:FireServer(NetworkSchema.EVENTS.SettingsUpdate, cfg)
		self:RefreshModalPanel()
	end)

	makeLabel(modalContent, "FIELD OF VIEW:", UDim2.new(0, 200, 0, 22), UDim2.new(0, 12, 0, 184), 13).TextColor3 =
		COL_AMBER
	for i, fovVal in ipairs({ 70, 78, 86, 92 }) do
		local fb = makeButton(
			modalContent,
			tostring(fovVal) .. "°",
			UDim2.new(0, 78, 0, 28),
			UDim2.new(0, 12 + (i - 1) * 86, 0, 210),
			if cfg.FOV == fovVal then Color3.fromRGB(64, 58, 44) else COL_BG_CELL
		)
		fb.MouseButton1Click:Connect(function()
			cfg.FOV = fovVal
			registry.ClientController:FireServer(NetworkSchema.EVENTS.SettingsUpdate, cfg)
			self:RefreshModalPanel()
		end)
	end
end

function UIController:RefreshModalPanel()
	if not modalRoot then
		return
	end
	local panel = registry.ClientController.State.ActivePanel
	if panel == "None" then
		modalRoot.Visible = false
		return
	end

	clearChildren(modalContent)
	modalRoot.Visible = true

	if panel == "Inventory" then
		self:RenderInventoryModal()
	elseif panel == "Loot" then
		self:RenderLootModal()
	elseif panel == "Medical" then
		self:RenderMedicalModal()
	elseif panel == "Crafting" then
		self:RenderCraftingModal()
	elseif panel == "Quests" then
		self:RenderQuestsModal()
	elseif panel == "Party" then
		self:RenderPartyModal()
	elseif panel == "Settings" then
		self:RenderSettingsModal()
	end
end

function UIController:Start()
	self:BuildHUD()
	self:UpdateHUDTelemetry()

	registry.ClientController:On("VitalsChanged", function()
		self:UpdateHUDTelemetry()
		if registry.ClientController.State.ActivePanel == "Medical" then
			self:RefreshModalPanel()
		end
	end)

	registry.ClientController:On("InventoryChanged", function()
		self:UpdateHUDTelemetry()
		local p = registry.ClientController.State.ActivePanel
		if p == "Inventory" or p == "Crafting" or p == "Medical" then
			self:RefreshModalPanel()
		end
	end)

	registry.ClientController:On("LootChanged", function()
		if registry.ClientController.State.ActivePanel == "Loot" then
			self:RefreshModalPanel()
		end
	end)

	registry.ClientController:On("QuestsChanged", function()
		if registry.ClientController.State.ActivePanel == "Quests" then
			self:RefreshModalPanel()
		end
	end)

	registry.ClientController:On("PartyChanged", function()
		if registry.ClientController.State.ActivePanel == "Party" then
			self:RefreshModalPanel()
		end
	end)

	registry.ClientController:On("WeatherChanged", function()
		self:UpdateHUDTelemetry()
	end)

	registry.ClientController:On("PanelChanged", function()
		self:RefreshModalPanel()
	end)

	registry.ClientController:On("BuildModeChanged", function()
		self:RefreshBuildBar()
	end)

	registry.ClientController:On("PromptChanged", function(prompt)
		if prompt and prompt.Label then
			promptLabel.Text = prompt.Label
			promptLabel.Visible = true
		else
			promptLabel.Visible = false
		end
	end)

	registry.ClientController:On("Notification", function(data)
		if type(data) == "table" and data.Text then
			self:ShowToast(tostring(data.Text), data.Kind == "Warning")
		end
	end)

	registry.ClientController:On("RadioTransmission", function(data)
		if type(data) == "table" then
			local hdr = tostring(data.Title or data.Frequency or "142.8 MHz")
			self:ShowRadioBanner(hdr, tostring(data.Text or ""), if data.IsLoreDocument then 13 else 8.5)
		end
	end)

	registry.ClientController:On("WorldEventAlert", function(data)
		if type(data) == "table" then
			self:ShowRadioBanner(
				"SECTOR EVENT // " .. tostring(data.Title or "ALERT"),
				tostring(data.Description or ""),
				9.5
			)
		end
	end)

	registry.ClientController:On("CombatFeedback", function(data)
		if type(data) == "table" and (data.Kind == "ShotFired" or data.Kind == "MeleeHit") then
			local hasMutantHit = (data.Kind == "MeleeHit")
			if data.Hits then
				for _, h in ipairs(data.Hits) do
					if h.MutantId then
						hasMutantHit = true
						break
					end
				end
			end
			if hasMutantHit and hitmarkerLabel then
				hitmarkerLabel.Visible = true
				task.delay(0.11, function()
					if hitmarkerLabel then
						hitmarkerLabel.Visible = false
					end
				end)
			end
		end
		self:UpdateHUDTelemetry()
	end)
end

return UIController
