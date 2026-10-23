--!strict
--[[
    ASHES OF HUMANITY: LAST SIGNAL
    StarterPlayerScripts.Controllers.ClientController
    Central Client State Store, Remote Event Bus, World Raycast Interaction
    Scanner, and Base-Building Ghost Placement Preview.
]]

local Players = game:GetService("Players")
local ReplicatedStorage = game:GetService("ReplicatedStorage")
local RunService = game:GetService("RunService")
local UserInputService = game:GetService("UserInputService")
local Workspace = game:GetService("Workspace")

local BuildingDefinitions = require(ReplicatedStorage.Shared.BuildingDefinitions)
local Config = require(ReplicatedStorage.Shared.Config)
local NetworkSchema = require(ReplicatedStorage.Shared.NetworkSchema)

local ClientController = {}

local LocalPlayer = Players.LocalPlayer
local RemotesFolder: Folder = nil :: any

export type SignalConnection = { Disconnect: (self: SignalConnection) -> () }

local listeners: { [string]: { (any) -> () } } = {}

ClientController.State = {
	Vitals = {
		Health = 100,
		MaxHealth = 100,
		Hunger = 88,
		Thirst = 86,
		Stamina = 100,
		MaxStaminaCap = 100,
		BodyTemperature = 37.0,
		Radiation = 0,
		Infection = 0,
		Fatigue = 8,
		BleedStacks = 0,
		MentalStress = 10,
		FlashlightBattery = 68,
		FlashlightOn = false,
		IsSheltered = true,
		IsDowned = false,
		DownedBleedoutTimer = 0,
		CurrentRegion = "EmergencyShelter",
	},
	BodyRegions = {} :: any,
	Inventory = {
		Cols = 8,
		Rows = 5,
		MaxWeightKg = 24.0,
		CurrentWeightKg = 0.0,
		Items = {},
		Equipped = {},
		Hotbar = {},
	} :: any,
	Quests = {} :: any,
	DiscoveredLore = {} :: { [string]: boolean },
	UnlockedBlueprints = {} :: { [string]: boolean },
	Party = nil :: any,
	PendingInvite = nil :: any,
	Settings = {
		GraphicsPreset = "High",
		ReduceMotion = false,
		ScreenEffects = true,
		MasterVolume = 0.85,
		MouseSensitivity = 0.5,
		FOV = 78,
	},
	Weather = "Clear",
	ClockTime = Config.START_CLOCK_TIME,
	IsNight = false,
	DayCount = 1,

	-- Active UI & Gameplay Modes
	ActiveSlot = "Primary",
	ActivePanel = "None", -- "None" | "Inventory" | "Medical" | "Crafting" | "Quests" | "Party" | "Settings" | "Loot"
	ActiveLootContainer = nil :: any,
	BuildMode = false,
	SelectedBuildingIndex = 1,
	BuildYawDeg = 0,
	CurrentPrompt = nil :: { Label: string, Payload: any, TargetPos: Vector3 }?,
}

local BUILDING_ORDER = {
	"WoodFloor",
	"WoodWall",
	"ReinforcedWall",
	"WoodRoof",
	"SecurityDoor",
	"WindowBarricade",
	"StorageChest",
	"WorkbenchStation",
	"CampfireStation",
	"DieselGenerator",
	"PerimeterFloodlight",
	"SpikeBarricade",
}
ClientController.BuildingOrder = BUILDING_ORDER

local ghostPart: Part? = nil

function ClientController:On(topic: string, fn: (any) -> ())
	if not listeners[topic] then
		listeners[topic] = {}
	end
	table.insert(listeners[topic], fn)
end

function ClientController:Emit(topic: string, data: any)
	local list = listeners[topic]
	if list then
		for _, fn in ipairs(list) do
			task.spawn(fn, data)
		end
	end
end

function ClientController:FireServer(eventName: string, payload: any)
	if not RemotesFolder then
		return
	end
	local re = RemotesFolder:FindFirstChild(eventName)
	if re and re:IsA("RemoteEvent") then
		re:FireServer(payload)
	end
end

function ClientController:Init(_registry: any)
	RemotesFolder = ReplicatedStorage:WaitForChild(NetworkSchema.FOLDER_NAME, 15) :: Folder
	if not RemotesFolder then
		return
	end

	local function bindRemote(evName: string, handler: (any) -> ())
		local re = RemotesFolder:WaitForChild(evName, 10)
		if re and re:IsA("RemoteEvent") then
			re.OnClientEvent:Connect(handler)
		end
	end

	bindRemote(NetworkSchema.EVENTS.VitalsUpdate, function(packet)
		if type(packet) ~= "table" then
			return
		end
		if packet.Vitals then
			self.State.Vitals = packet.Vitals
		end
		if packet.BodyRegions then
			self.State.BodyRegions = packet.BodyRegions
		end
		self:Emit("VitalsChanged", self.State.Vitals)
	end)

	bindRemote(NetworkSchema.EVENTS.InventoryUpdate, function(inv)
		if type(inv) == "table" then
			self.State.Inventory = inv
			self:Emit("InventoryChanged", inv)
		end
	end)

	bindRemote(NetworkSchema.EVENTS.ContainerOpened, function(data)
		if type(data) == "table" then
			self.State.ActiveLootContainer = data
			self.State.ActivePanel = "Loot"
			self:Emit("LootChanged", data)
			self:Emit("PanelChanged", "Loot")
		end
	end)

	bindRemote(NetworkSchema.EVENTS.QuestUpdate, function(data)
		if type(data) == "table" then
			self.State.Quests = data.Quests or {}
			self.State.DiscoveredLore = data.DiscoveredLore or {}
			self.State.UnlockedBlueprints = data.UnlockedBlueprints or {}
			self:Emit("QuestsChanged", data)
		end
	end)

	bindRemote(NetworkSchema.EVENTS.WeatherUpdate, function(data)
		if type(data) == "table" then
			self.State.Weather = data.Weather or "Clear"
			self.State.ClockTime = data.ClockTime or 12
			self.State.IsNight = data.IsNight == true
			self.State.DayCount = data.DayCount or 1
			self:Emit("WeatherChanged", data)
		end
	end)

	bindRemote(NetworkSchema.EVENTS.PartyUpdate, function(data)
		if type(data) == "table" then
			if data.Kind == "InviteReceived" then
				self.State.PendingInvite = data
				self:Emit("Notification", {
					Kind = "Info",
					Text = "Co-op Invite from " .. tostring(data.FromName) .. " (Open [P] Party menu)",
				})
			elseif data.Kind == "PartyState" then
				self.State.Party = data.Party
			end
			self:Emit("PartyChanged", data)
		end
	end)

	bindRemote(NetworkSchema.EVENTS.WorldEventAlert, function(data)
		self:Emit("WorldEventAlert", data)
	end)

	bindRemote(NetworkSchema.EVENTS.RadioTransmission, function(data)
		self:Emit("RadioTransmission", data)
	end)

	bindRemote(NetworkSchema.EVENTS.CombatFeedback, function(data)
		self:Emit("CombatFeedback", data)
	end)

	bindRemote(NetworkSchema.EVENTS.Notification, function(data)
		self:Emit("Notification", data)
	end)

	bindRemote(NetworkSchema.EVENTS.HorrorCue, function(data)
		self:Emit("HorrorCue", data)
	end)
end

function ClientController:ScanInteractable(): { Label: string, Payload: any, TargetPos: Vector3 }?
	local cam = Workspace.CurrentCamera
	local char = LocalPlayer.Character
	local root = if char then char:FindFirstChild("HumanoidRootPart") :: BasePart? else nil
	if not cam or not root then
		return nil
	end

	-- 1. Check nearby downed cooperative teammate first
	for _, other in ipairs(Players:GetPlayers()) do
		if other ~= LocalPlayer and other.Character then
			local oRoot = other.Character:FindFirstChild("HumanoidRootPart") :: BasePart?
			if oRoot and (oRoot.Position - root.Position).Magnitude <= 8.5 then
				if self.State.Party then
					for _, mem in ipairs(self.State.Party.Members or {}) do
						if mem.UserId == other.UserId and mem.IsDowned then
							return {
								Label = "[E] Stabilize & Revive " .. other.Name,
								Payload = { Kind = "RevivePeer", TargetUserId = other.UserId },
								TargetPos = oRoot.Position,
							}
						end
					end
				end
			end
		end
	end

	-- 2. Camera center raycast for world objects
	local params = RaycastParams.new()
	params.FilterType = Enum.RaycastFilterType.Exclude
	local exclude: { Instance } = { char :: Instance }
	if ghostPart then
		table.insert(exclude, ghostPart)
	end
	params.FilterDescendantsInstances = exclude

	local hit = Workspace:Raycast(cam.CFrame.Position, cam.CFrame.LookVector * 13.5, params)
	if not hit or not hit.Instance then
		return nil
	end

	local inst: Instance? = hit.Instance
	while inst and inst ~= Workspace do
		if inst:GetAttribute("IsLootContainer") == true then
			local cid = tostring(inst:GetAttribute("ContainerId") or inst.Name)
			local label = tostring(inst:GetAttribute("PromptLabel") or ("Search " .. inst.Name))
			return {
				Label = "[E] " .. label,
				Payload = { Kind = "OpenLoot", ContainerId = cid },
				TargetPos = hit.Position,
			}
		end
		if type(inst:GetAttribute("DropId")) == "string" then
			local pid = tostring(inst:GetAttribute("DropId"))
			local itemName = tostring(inst:GetAttribute("ItemId") or "Supply")
			return {
				Label = "[E] Pick Up " .. itemName,
				Payload = { Kind = "PickupWorldItem", DropId = pid },
				TargetPos = hit.Position,
			}
		end
		if type(inst:GetAttribute("LoreId")) == "string" then
			local lid = tostring(inst:GetAttribute("LoreId"))
			local title = tostring(inst:GetAttribute("PromptLabel") or "Read Field Document")
			return {
				Label = "[E] " .. title,
				Payload = { Kind = "LoreNote", LoreId = lid, Target = hit.Instance },
				TargetPos = hit.Position,
			}
		end
		if type(inst:GetAttribute("QuestInteractId")) == "string" then
			local qid = tostring(inst:GetAttribute("QuestInteractId"))
			local label = tostring(inst:GetAttribute("PromptLabel") or ("Operate " .. qid))
			return {
				Label = "[E] " .. label,
				Payload = { Kind = "QuestInteract", InteractId = qid, Target = hit.Instance },
				TargetPos = hit.Position,
			}
		end
		local iType = inst:GetAttribute("InteractiveType")
		if iType == "Door" then
			local isOpen = inst:GetAttribute("DoorOpen") == true
			return {
				Label = if isOpen then "[E] Close Door" else "[E] Open Door",
				Payload = { Kind = "Door", Target = inst },
				TargetPos = hit.Position,
			}
		elseif iType == "RestBunk" then
			return {
				Label = "[E] Rest on Emergency Bunk",
				Payload = { Kind = "RestBunk", Target = hit.Instance },
				TargetPos = hit.Position,
			}
		elseif iType == "VehicleAlarm" then
			return {
				Label = "[E] Trigger Car Horn Distraction",
				Payload = { Kind = "VehicleAlarm", Target = hit.Instance },
				TargetPos = hit.Position,
			}
		end
		inst = inst.Parent
	end

	return nil
end

function ClientController:TriggerContextInteract()
	local prompt = self.State.CurrentPrompt
	if not prompt then
		return
	end
	local p = prompt.Payload
	if p.Kind == "OpenLoot" then
		self:FireServer(NetworkSchema.EVENTS.LootInteract, { Action = "Open", ContainerId = p.ContainerId })
	elseif p.Kind == "PickupWorldItem" then
		self:FireServer(NetworkSchema.EVENTS.LootInteract, { Action = "PickupDrop", DropId = p.DropId })
	elseif p.Kind == "RevivePeer" then
		self:FireServer(NetworkSchema.EVENTS.PartyAction, { Action = "Revive", TargetUserId = p.TargetUserId })
	else
		self:FireServer(NetworkSchema.EVENTS.InteractWorldObject, p)
	end
end

function ClientController:UpdateBuildGhost()
	if not self.State.BuildMode then
		if ghostPart then
			ghostPart.Parent = nil
		end
		return
	end

	local bId = BUILDING_ORDER[self.State.SelectedBuildingIndex] or "WoodWall"
	local def = BuildingDefinitions.Get(bId)
	local cam = Workspace.CurrentCamera
	local char = LocalPlayer.Character
	if not def or not cam or not char then
		return
	end

	if not ghostPart then
		local gp = Instance.new("Part")
		gp.Name = "BuildPlacementGhost"
		gp.Anchored = true
		gp.CanCollide = false
		gp.CastShadow = false
		gp.Material = Enum.Material.ForceField
		gp.Transparency = 0.45
		ghostPart = gp
	end

	local g = ghostPart :: Part
	g.Size = def.Size
	local params = RaycastParams.new()
	params.FilterType = Enum.RaycastFilterType.Exclude
	params.FilterDescendantsInstances = { char, g }

	local hit = Workspace:Raycast(cam.CFrame.Position, cam.CFrame.LookVector * Config.BUILDING.MaxBuildDistance, params)
	local rawPos = if hit
		then hit.Position + Vector3.new(0, def.Size.Y * 0.5, 0)
		else (cam.CFrame.Position + cam.CFrame.LookVector * 12)
	local grid = Config.BUILDING.GridSnapStuds
	local snapped = if def.SnapToWindowOrDoor
		then rawPos
		else Vector3.new(
			math.floor(rawPos.X / grid + 0.5) * grid,
			math.floor(rawPos.Y * 2 + 0.5) / 2,
			math.floor(rawPos.Z / grid + 0.5) * grid
		)

	g.CFrame = CFrame.new(snapped) * CFrame.Angles(0, math.rad(self.State.BuildYawDeg), 0)
	g.Color = if hit then Color3.fromRGB(110, 195, 130) else Color3.fromRGB(205, 90, 75)
	g.Parent = Workspace
end

function ClientController:PlaceCurrentBuildGhost()
	if not self.State.BuildMode or not ghostPart or not ghostPart.Parent then
		return
	end
	local bId = BUILDING_ORDER[self.State.SelectedBuildingIndex] or "WoodWall"
	self:FireServer(NetworkSchema.EVENTS.BuildAction, {
		Action = "Place",
		BuildingId = bId,
		Position = ghostPart.Position,
		YawDeg = self.State.BuildYawDeg,
	})
end

function ClientController:TogglePanel(panelName: string)
	if self.State.ActivePanel == panelName then
		self.State.ActivePanel = "None"
	else
		self.State.ActivePanel = panelName
		self.State.BuildMode = false
	end
	self:Emit("PanelChanged", self.State.ActivePanel)
end

function ClientController:Start()
	-- Pull initial snapshot from server
	task.spawn(function()
		if not RemotesFolder then
			return
		end
		local rf = RemotesFolder:WaitForChild(NetworkSchema.FUNCTIONS.PingState, 10)
		if rf and rf:IsA("RemoteFunction") then
			local ok, snap = pcall(function()
				return rf:InvokeServer()
			end)
			if ok and type(snap) == "table" then
				if snap.Vitals then
					self.State.Vitals = snap.Vitals
				end
				if snap.BodyRegions then
					self.State.BodyRegions = snap.BodyRegions
				end
				if snap.Inventory then
					self.State.Inventory = snap.Inventory
				end
				if snap.Quests then
					self.State.Quests = snap.Quests
				end
				if snap.DiscoveredLore then
					self.State.DiscoveredLore = snap.DiscoveredLore
				end
				if snap.UnlockedBlueprints then
					self.State.UnlockedBlueprints = snap.UnlockedBlueprints
				end
				if snap.Settings then
					self.State.Settings = snap.Settings
				end
				self:Emit("VitalsChanged", self.State.Vitals)
				self:Emit("InventoryChanged", self.State.Inventory)
				self:Emit("QuestsChanged", snap)
			end
		end
	end)

	UserInputService.InputBegan:Connect(function(input: InputObject, gameProcessed: boolean)
		if gameProcessed then
			return
		end
		if input.KeyCode == Enum.KeyCode.E then
			self:TriggerContextInteract()
		elseif input.KeyCode == Enum.KeyCode.Tab then
			self:TogglePanel("Inventory")
		elseif input.KeyCode == Enum.KeyCode.H then
			self:TogglePanel("Medical")
		elseif input.KeyCode == Enum.KeyCode.K then
			self:TogglePanel("Crafting")
		elseif input.KeyCode == Enum.KeyCode.J then
			self:TogglePanel("Quests")
		elseif input.KeyCode == Enum.KeyCode.P then
			self:TogglePanel("Party")
		elseif input.KeyCode == Enum.KeyCode.O then
			self:TogglePanel("Settings")
		elseif input.KeyCode == Enum.KeyCode.B then
			self.State.BuildMode = not self.State.BuildMode
			if self.State.BuildMode then
				self.State.ActivePanel = "None"
				self:Emit("PanelChanged", "None")
			end
			self:Emit("BuildModeChanged", self.State.BuildMode)
		elseif input.KeyCode == Enum.KeyCode.LeftBracket and self.State.BuildMode then
			self.State.SelectedBuildingIndex = ((self.State.SelectedBuildingIndex - 2) % #BUILDING_ORDER) + 1
			self:Emit("BuildModeChanged", self.State.BuildMode)
		elseif input.KeyCode == Enum.KeyCode.RightBracket and self.State.BuildMode then
			self.State.SelectedBuildingIndex = (self.State.SelectedBuildingIndex % #BUILDING_ORDER) + 1
			self:Emit("BuildModeChanged", self.State.BuildMode)
		elseif input.KeyCode == Enum.KeyCode.R and self.State.BuildMode then
			self.State.BuildYawDeg = (self.State.BuildYawDeg + Config.BUILDING.RotationStepDeg) % 360
		end
	end)

	RunService.RenderStepped:Connect(function()
		local nextPrompt = self:ScanInteractable()
		if nextPrompt ~= self.State.CurrentPrompt then
			self.State.CurrentPrompt = nextPrompt
			self:Emit("PromptChanged", nextPrompt)
		end
		self:UpdateBuildGhost()
	end)
end

return ClientController
