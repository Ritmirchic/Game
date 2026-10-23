--!strict
--[[
	BuildingUI — piece palette and placement ghost.

	The ghost is drawn from the shared BuildDefinitions, snapped to the same grid
	the server uses, and colour-coded for affordability.
]]

local Players = game:GetService("Players")
local UserInputService = game:GetService("UserInputService")
local RunService = game:GetService("RunService")
local ReplicatedStorage = game:GetService("ReplicatedStorage")

local Store = require(script.Parent.StateStore)
local Theme = require(script.Parent.UITheme)
local UI = require(script.Parent.UIBuilder)
local Network = require(script.Parent.Network)

local Shared = ReplicatedStorage:WaitForChild("Shared")
local Build = require(Shared.BuildDefinitions)
local Items = require(Shared.ItemDefinitions)

local BuildingUI = {}

local player = Players.LocalPlayer
local gui
local root
local ghost
local selectedPiece = nil
local rotation = 0
local ghostConnection

local function countItem(itemId)
	local total = 0
	for _, container in pairs(Store.get("inventory").containers or {}) do
		for _, entry in pairs(container.items) do
			if entry.itemId == itemId then total += entry.count or 1 end
		end
	end
	return total
end

local function canAfford(def)
	for itemId, count in pairs(def.cost or {}) do
		if countItem(itemId) < count then return false end
	end
	return true
end

local function updateGhost()
	if not ghost then return end
	local character = player.Character
	local hrp = character and character:FindFirstChild("HumanoidRootPart")
	if not hrp then
		ghost.Visible = false
		return
	end
	local mouse = UserInputService:GetMouseLocation()
	local ray = workspace.CurrentCamera:ViewportPointToRay(mouse.X, mouse.Y)
	local params = RaycastParams.new()
	params.FilterType = Enum.RaycastFilterType.Exclude
	params.FilterDescendantsInstances = { character }
	local result = workspace:Raycast(ray.Origin, ray.Direction * 200, params)
	if not result then
		ghost.Visible = false
		return
	end
	local def = Build[selectedPiece]
	if not def then return end
	local grid = def.grid or 4
	local pos = result.Position + result.Normal * (def.size.Y / 2)
	pos = Vector3.new(
		math.floor(pos.X / grid + 0.5) * grid,
		math.floor(pos.Y / grid + 0.5) * grid,
		math.floor(pos.Z / grid + 0.5) * grid
	)
	ghost.Visible = true
	ghost.Size = def.size
	ghost.CFrame = CFrame.new(pos) * CFrame.Angles(0, math.rad(rotation), 0)
	local affordable = canAfford(def)
	ghost.Color = affordable and Color3.fromRGB(90, 190, 110) or Color3.fromRGB(190, 70, 60)
	ghost.Transparency = 0.62
	return pos
end

local function enable(pieceId)
	selectedPiece = pieceId
	local def = Build[pieceId]
	if not def then return end
	if not ghost then
		ghost = Instance.new("Part")
		ghost.Name = "BuildGhost"
		ghost.Anchored = true
		ghost.CanCollide = false
		ghost.CanQuery = false
		ghost.CanTouch = false
		ghost.Material = Enum.Material.ForceField
		ghost.Parent = workspace
		ghostConnection = RunService.RenderStepped:Connect(updateGhost)
	end
	Store.notify("Placing " .. def.name .. " · R to rotate · E to confirm · ESC to cancel", "info", 5)
end

local function disable()
	selectedPiece = nil
	if ghost then ghost:Destroy() ghost = nil end
	if ghostConnection then ghostConnection:Disconnect() ghostConnection = nil end
end

function BuildingUI.Build(parent)
	gui = parent
	root = UI.frame(gui, {
		name = "Building",
		size = UDim2.fromOffset(230, 420),
		position = UDim2.new(0, 22, 0.5, -210),
		color = Theme.Colors.panel,
		transparency = 0.15,
		visible = false,
		zIndex = 20,
		padding = 10,
	})
	UI.label(root, {
		size = UDim2.new(1, 0, 0, 20),
		text = "CONSTRUCTION",
		textSize = 13,
		font = Theme.Fonts.bold,
	})
	local index = 0
	for id, def in pairs(Build) do
		if type(def) == "table" and def.size then
			index += 1
			local affordable = canAfford(def)
			local costText = {}
			for itemId, count in pairs(def.cost or {}) do
				table.insert(costText, string.format("%s x%d", (Items[itemId] and Items[itemId].name) or itemId, count))
			end
			local btn = UI.button(root, {
				size = UDim2.new(1, 0, 0, 40),
				position = UDim2.fromOffset(0, 24 + (index - 1) * 44),
				text = string.upper(def.name) .. "\n" .. table.concat(costText, ", "),
				color = affordable and Theme.Colors.panelAlt or Theme.Colors.panelDeep,
				textColor = affordable and Theme.Colors.text or Theme.Colors.textFaint,
				textSize = 9,
				font = Theme.Fonts.mono,
				zIndex = 21,
			})
			btn.MouseButton1Click:Connect(function()
				enable(id)
			end)
		end
	end
	UI.label(root, {
		size = UDim2.new(1, 0, 0, 30),
		position = UDim2.new(0, 0, 1, -32),
		text = "Click a piece, then E to place.\nR rotates. ESC exits build mode.",
		textSize = 9,
		font = Theme.Fonts.mono,
		textColor = Theme.Colors.textFaint,
		wrapped = true,
	})

	UserInputService.InputBegan:Connect(function(input, processed)
		if processed or not selectedPiece then return end
		if input.KeyCode == Enum.KeyCode.R then
			rotation = (rotation + 90) % 360
		elseif input.KeyCode == Enum.KeyCode.E then
			local pos = updateGhost()
			if pos then
				Network.fire("RequestBuild", selectedPiece, pos, rotation)
			end
		elseif input.KeyCode == Enum.KeyCode.Escape then
			BuildingUI.Close()
		end
	end)
end

function BuildingUI.Open()
	if not root then return end
	root.Visible = true
end

function BuildingUI.Close()
	if root then root.Visible = false end
	disable()
end

function BuildingUI.Toggle()
	if not root then return end
	if root.Visible then BuildingUI.Close() else BuildingUI.Open() end
	return root.Visible
end

function BuildingUI.IsOpen() return root ~= nil and root.Visible end

return BuildingUI
