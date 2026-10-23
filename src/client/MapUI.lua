--!strict
--[[
	MapUI — regional map drawn from real world coordinates.

	The map is generated from the same region table that built the world, with
	markers for the player, party members, quest anchors and supply drops.
]]

local Players = game:GetService("Players")
local ReplicatedStorage = game:GetService("ReplicatedStorage")

local Store = require(script.Parent.StateStore)
local Theme = require(script.Parent.UITheme)
local UI = require(script.Parent.UIBuilder)
local Network = require(script.Parent.Network)

local Config = require(ReplicatedStorage:WaitForChild("Shared"):WaitForChild("Config"))

local MapUI = {}

local gui
local root
local canvas
local actor
local markerLayer
local labels = {}
local WORLD_SIZE = 3600
local SIZE = 420

-- Same coordinates the world generator placed the districts at.
local REGION_MAP = {
	{ name = "Suburbs", label = "DEAD SUBURBS", x = 0, z = 0, half = 520, color = Color3.fromRGB(52, 50, 46) },
	{ name = "Forest", label = "BLACKWOOD", x = -1150, z = 120, half = 470, color = Color3.fromRGB(38, 52, 40) },
	{ name = "Industrial", label = "INDUSTRIAL", x = 1050, z = -180, half = 500, color = Color3.fromRGB(58, 50, 42) },
	{ name = "City", label = "FORGOTTEN CITY", x = -120, z = -1180, half = 560, color = Color3.fromRGB(46, 46, 50) },
	{ name = "Military", label = "EXCLUSION ZONE", x = 1150, z = 1050, half = 470, color = Color3.fromRGB(52, 54, 40) },
	{ name = "Wasteland", label = "RED WASTELAND", x = -1200, z = -1150, half = 520, color = Color3.fromRGB(58, 40, 34) },
}

local function toMap(position)
	local x = (position.X + WORLD_SIZE / 2) / WORLD_SIZE
	local z = (position.Z + WORLD_SIZE / 2) / WORLD_SIZE
	return UDim2.fromScale(math.clamp(x, 0, 1), math.clamp(z, 0, 1))
end

local function drawStatic()
	if not canvas then return end
	UI.clear(canvas)
	local background = UI.frame(canvas, {
		size = UDim2.fromScale(1, 1),
		color = Color3.fromRGB(14, 15, 16),
		stroke = false,
		corner = false,
	})
	-- grid
	for i = 1, 7 do
		UI.frame(background, {
			size = UDim2.new(0, 1, 1, 0),
			position = UDim2.fromScale(i / 8, 0),
			color = Color3.fromRGB(38, 38, 36),
			corner = false,
			stroke = false,
		})
		UI.frame(background, {
			size = UDim2.new(1, 0, 0, 1),
			position = UDim2.fromScale(0, i / 8),
			color = Color3.fromRGB(38, 38, 36),
			corner = false,
			stroke = false,
		})
	end

	for index, region in ipairs(REGION_MAP) do
		local center = toMap(Vector3.new(region.x, 0, region.z))
		local box = UI.frame(background, {
			name = "Region" .. index,
			size = UDim2.fromScale((region.half * 2) / WORLD_SIZE, (region.half * 2) / WORLD_SIZE),
			position = center,
			anchor = Vector2.new(0.5, 0.5),
			color = region.color,
			transparency = 0.55,
			strokeColor = Theme.Colors.border,
			strokeTransparency = 0.5,
			corner = UDim.new(0, 1),
		})
		UI.label(box, {
			size = UDim2.new(1, 0, 0, 14),
			position = UDim2.new(0, 0, 0.5, -7),
			text = region.label,
			textSize = 9,
			font = Theme.Fonts.mono,
			textColor = Theme.Colors.textDim,
			alignX = Enum.TextXAlignment.Center,
		})
		labels[region.name] = box
	end

	markerLayer = UI.frame(background, {
		name = "Markers",
		size = UDim2.fromScale(1, 1),
		transparency = 1,
		stroke = false,
		corner = false,
	})

	actor = UI.frame(background, {
		name = "Actor",
		size = UDim2.fromOffset(9, 9),
		anchor = Vector2.new(0.5, 0.5),
		color = Theme.Colors.accent,
		corner = UDim.new(1, 0),
		strokeColor = Theme.Colors.shadow,
		zIndex = 25,
	})
end

function MapUI.AddMarker(position, color, label)
	if not markerLayer then return end
	local marker = UI.frame(markerLayer, {
		size = UDim2.fromOffset(7, 7),
		position = toMap(position),
		anchor = Vector2.new(0.5, 0.5),
		color = color or Theme.Colors.danger,
		corner = UDim.new(1, 0),
		zIndex = 24,
	})
	if label then
		UI.label(marker, {
			size = UDim2.fromOffset(80, 12),
			position = UDim2.fromOffset(6, -3),
			text = label,
			textSize = 8,
			font = Theme.Fonts.mono,
			textColor = color or Theme.Colors.danger,
			zIndex = 25,
		})
	end
	task.delay(60, function()
		if marker.Parent then marker:Destroy() end
	end)
	return marker
end

function MapUI.Update()
	local character = Players.LocalPlayer.Character
	local hrp = character and character:FindFirstChild("HumanoidRootPart")
	if hrp and actor then
		actor.Position = toMap(hrp.Position)
		local look = hrp.CFrame.LookVector
		actor.Rotation = math.deg(math.atan2(look.X, -look.Z))
	end
end

function MapUI.Build(parent)
	gui = parent
	root = UI.frame(gui, {
		name = "Map",
		size = UDim2.fromOffset(660, 520),
		position = UDim2.fromScale(0.5, 0.5),
		anchor = Vector2.new(0.5, 0.5),
		color = Theme.Colors.bg,
		visible = false,
		zIndex = 20,
	})
	UI.label(root, {
		size = UDim2.new(1, -24, 0, 30),
		position = UDim2.fromOffset(16, 10),
		text = "DISTRICT SURVEY",
		textSize = 16,
		font = Theme.Fonts.bold,
	})
	UI.label(root, {
		size = UDim2.fromOffset(420, 16),
		position = UDim2.fromOffset(16, 40),
		text = "Positions are approximate. Nothing here is safe.",
		textSize = 10,
		font = Theme.Fonts.mono,
		textColor = Theme.Colors.textFaint,
	})
	canvas = UI.frame(root, {
		name = "Canvas",
		size = UDim2.fromOffset(SIZE, SIZE),
		position = UDim2.fromOffset(16, 66),
		color = Theme.Colors.panelDeep,
		corner = UDim.new(0, 2),
		clip = true,
	})
	drawStatic()

	local side = UI.frame(root, {
		size = UDim2.fromOffset(190, 340),
		position = UDim2.fromOffset(454, 66),
		color = Theme.Colors.panel,
		padding = 10,
	})
	UI.label(side, {
		size = UDim2.new(1, 0, 0, 16),
		text = "REGIONS",
		textSize = 10,
		font = Theme.Fonts.mono,
		textColor = Theme.Colors.textFaint,
	})
	for index, region in ipairs(REGION_MAP) do
		local def = Config.Regions[region.name] or {}
		UI.label(side, {
			size = UDim2.new(1, 0, 0, 26),
			position = UDim2.fromOffset(0, 20 + (index - 1) * 28),
			text = string.format("%s\nTHREAT %d · RAD %d", region.label, def.danger or 1, def.radiation or 0),
			textSize = 10,
			font = Theme.Fonts.mono,
			textColor = Theme.Colors.textDim,
		})
	end
	local pingBtn = UI.button(side, {
		size = UDim2.new(1, 0, 0, 30),
		position = UDim2.new(0, 0, 1, -34),
		text = "PLACE PING",
		color = Theme.Colors.panelAlt,
		textSize = 10,
		font = Theme.Fonts.mono,
	})
	pingBtn.MouseButton1Click:Connect(function()
		local character = Players.LocalPlayer.Character
		local hrp = character and character:FindFirstChild("HumanoidRootPart")
		if hrp then
			Network.fire("RequestPing", hrp.Position)
			MapUI.AddMarker(hrp.Position, Theme.Colors.accent, "PING")
		end
	end)

	Store.on("weather", function()
		if root and root.Visible then MapUI.Update() end
	end)
end

function MapUI.Open()
	if not root then return end
	root.Visible = true
	MapUI.Update()
end

function MapUI.Close() if root then root.Visible = false end end

function MapUI.Toggle()
	if not root then return end
	if root.Visible then MapUI.Close() else MapUI.Open() end
	return root.Visible
end

function MapUI.IsOpen() return root ~= nil and root.Visible end

return MapUI
