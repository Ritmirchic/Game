--!strict
--[[
	InteractionController — proximity prompt for loot, doors, workbenches,
	documents, radios, beds and the safehouse anchor.
]]

local Players = game:GetService("Players")
local UserInputService = game:GetService("UserInputService")
local RunService = game:GetService("RunService")
local ReplicatedStorage = game:GetService("ReplicatedStorage")

local Store = require(script.Parent.StateStore)
local Network = require(script.Parent.Network)

local Config = require(ReplicatedStorage:WaitForChild("Shared"):WaitForChild("Config"))

local Interaction = {}

local player = Players.LocalPlayer
local target = nil
local scanAccumulator = 0
local world = nil
local candidates = {}

local KIND_LABEL = {
	Loot = "Search",
	WorldDrop = "Pick Up",
	Door = "Open",
	Campfire = "Warm Up",
	Workbench = "Use Workbench",
	Radio = "Listen",
	RadioTower = "Transmit",
	Bed = "Sleep",
	Document = "Read",
	QuestItem = "Take",
	LabDoor = "Hack Door",
	Safehouse = "Claim Safehouse",
	SupplyCrate = "Force Open",
}

local function findKind(instance)
	local kind = instance:FindFirstChild("Kind")
	if kind and kind:IsA("StringValue") then
		return kind.Value
	end
	return nil
end

local function collect()
	candidates = {}
	if not world then return end
	local character = player.Character
	local hrp = character and character:FindFirstChild("HumanoidRootPart")
	if not hrp then return end
	local position = hrp.Position
	local range = Config.Interaction.Range
	for _, instance in ipairs(world:GetDescendants()) do
		if instance:IsA("BasePart") then
			local kind = findKind(instance)
			if kind then
				local distance = (instance.Position - position).Magnitude
				if distance <= range then
					table.insert(candidates, { instance = instance, kind = kind, distance = distance })
				end
			end
		end
	end
	table.sort(candidates, function(a, b) return a.distance < b.distance end)
end

local function promptText(entry)
	local label = KIND_LABEL[entry.kind] or "Interact"
	local name = entry.instance:FindFirstChild("Label")
	if name and name:IsA("StringValue") and name.Value ~= "" then
		return string.format("[E] %s %s", label, name.Value)
	end
	if entry.instance:FindFirstChild("LootTable") then
		return string.format("[E] %s container", label)
	end
	return string.format("[E] %s", label)
end

function Interaction.CurrentTarget()
	return target
end

function Interaction.Interact()
	local entry = target
	if not entry or not entry.instance.Parent then return end
	local kind = entry.kind
	if kind == "WorldDrop" then
		Network.fire("RequestLoot", entry.instance)
	elseif kind == "Document" then
		local docId = entry.instance:FindFirstChild("DocId")
		Network.fire("RequestDocument", docId and docId.Value or "")
		Network.fire("RequestInteract", entry.instance)
	else
		Network.fire("RequestInteract", entry.instance)
	end
end

function Interaction.Start()
	world = workspace:WaitForChild("World")
	RunService.Heartbeat:Connect(function(dt)
		scanAccumulator += dt
		if scanAccumulator < 0.25 then return end
		scanAccumulator = 0
		collect()
		local best = candidates[1]
		if best ~= target then
			target = best
			Store.set("interactionTarget", target and {
				kind = target.kind, text = promptText(target), instance = target.instance,
			} or nil)
		end
	end)

	UserInputService.InputBegan:Connect(function(input, processed)
		if processed then return end
		if input.KeyCode == Enum.KeyCode.E then
			Interaction.Interact()
		end
	end)
end

return Interaction
