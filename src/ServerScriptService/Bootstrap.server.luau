--!strict
--[[
    ASHES OF HUMANITY: LAST SIGNAL
    ServerScriptService.Bootstrap
    Master Server Orchestrator, Physics Collision Groups, Remote Bus,
    Emergency Shelter Echo-07 Spawn Setup, and World Interaction Router.
]]

local PhysicsService = game:GetService("PhysicsService")
local Players = game:GetService("Players")
local ReplicatedStorage = game:GetService("ReplicatedStorage")
local ServerScriptService = game:GetService("ServerScriptService")
local Workspace = game:GetService("Workspace")

local Config = require(ReplicatedStorage.Shared.Config)
local NetworkSchema = require(ReplicatedStorage.Shared.NetworkSchema)
local UtilityModules = require(ReplicatedStorage.Shared.UtilityModules)

local ServicesFolder = ServerScriptService:WaitForChild("Services")
local SecurityService = require(ServicesFolder.SecurityService)
local PlayerDataService = require(ServicesFolder.PlayerDataService)
local SurvivalService = require(ServicesFolder.SurvivalService)
local InventoryService = require(ServicesFolder.InventoryService)
local LootService = require(ServicesFolder.LootService)
local CombatService = require(ServicesFolder.CombatService)
local EnemyAIService = require(ServicesFolder.EnemyAIService)
local CraftingService = require(ServicesFolder.CraftingService)
local BuildingService = require(ServicesFolder.BuildingService)
local QuestService = require(ServicesFolder.QuestService)
local WeatherService = require(ServicesFolder.WeatherService)
local PartyService = require(ServicesFolder.PartyService)
local WorldEventService = require(ServicesFolder.WorldEventService)

-- 1. Configure Collision Groups (Section 9)
local function setupCollisionGroups()
	pcall(function()
		PhysicsService:RegisterCollisionGroup("Survivors")
		PhysicsService:RegisterCollisionGroup("Mutants")
		PhysicsService:RegisterCollisionGroup("Debris")
		PhysicsService:RegisterCollisionGroup("InteractableLoot")

		PhysicsService:CollisionGroupSetCollidable("Debris", "Survivors", false)
		PhysicsService:CollisionGroupSetCollidable("Debris", "Mutants", false)
		PhysicsService:CollisionGroupSetCollidable("InteractableLoot", "Survivors", false)
		PhysicsService:CollisionGroupSetCollidable("InteractableLoot", "Mutants", false)
	end)
end
setupCollisionGroups()

-- 2. Create RemoteEvents & RemoteFunctions Folder
local remotesFolder = ReplicatedStorage:FindFirstChild(NetworkSchema.FOLDER_NAME)
if not remotesFolder then
	local f = Instance.new("Folder")
	f.Name = NetworkSchema.FOLDER_NAME
	f.Parent = ReplicatedStorage
	remotesFolder = f
end

local remoteEvents: { [string]: RemoteEvent } = {}
for _, evName in pairs(NetworkSchema.EVENTS) do
	local existing = remotesFolder:FindFirstChild(evName)
	if existing and existing:IsA("RemoteEvent") then
		remoteEvents[evName] = existing
	else
		local re = Instance.new("RemoteEvent")
		re.Name = evName
		re.Parent = remotesFolder
		remoteEvents[evName] = re
	end
end

local remoteFunctions: { [string]: RemoteFunction } = {}
for _, fnName in pairs(NetworkSchema.FUNCTIONS) do
	local existing = remotesFolder:FindFirstChild(fnName)
	if existing and existing:IsA("RemoteFunction") then
		remoteFunctions[fnName] = existing
	else
		local rf = Instance.new("RemoteFunction")
		rf.Name = fnName
		rf.Parent = remotesFolder
		remoteFunctions[fnName] = rf
	end
end

-- 3. Service Registry Wiring
local ServiceRegistry = {
	SecurityService = SecurityService,
	PlayerDataService = PlayerDataService,
	SurvivalService = SurvivalService,
	InventoryService = InventoryService,
	LootService = LootService,
	CombatService = CombatService,
	EnemyAIService = EnemyAIService,
	CraftingService = CraftingService,
	BuildingService = BuildingService,
	QuestService = QuestService,
	WeatherService = WeatherService,
	PartyService = PartyService,
	WorldEventService = WorldEventService,
	SendToPlayer = function(player: Player, eventName: string, payload: any)
		local re = remoteEvents[eventName]
		if re and player.Parent == Players then
			re:FireClient(player, payload)
		end
	end,
	BroadcastAll = function(eventName: string, payload: any)
		local re = remoteEvents[eventName]
		if re then
			re:FireAllClients(payload)
		end
	end,
}

local orderedServices: { any } = {
	SecurityService,
	PlayerDataService,
	WeatherService,
	SurvivalService,
	InventoryService,
	LootService,
	BuildingService,
	CraftingService,
	QuestService,
	PartyService,
	EnemyAIService,
	CombatService,
	WorldEventService,
}

for _, svc in ipairs(orderedServices) do
	svc:Init(ServiceRegistry)
end

-- 4. World Interaction Dispatcher (Doors, Terminals, Lore Notes, Rest Bunks, Loot, Objectives)
local function handleWorldInteract(player: Player, payload: any)
	if type(payload) ~= "table" or not NetworkSchema.ValidateString(payload.Kind, 32) then
		return
	end
	local profile = PlayerDataService:GetProfile(player)
	if not profile or profile.Vitals.Health <= 0 or profile.Vitals.IsDowned then
		return
	end

	local kind = payload.Kind
	if kind == "Door" and typeof(payload.Target) == "Instance" then
		local doorPart: BasePart? = nil
		if payload.Target:IsA("BasePart") then
			doorPart = payload.Target
		elseif payload.Target:IsA("Model") then
			doorPart = payload.Target.PrimaryPart or payload.Target:FindFirstChildWhichIsA("BasePart")
		end
		if doorPart and SecurityService:IsWithinDistance(player, doorPart.Position, 13.0) then
			local isOpen = doorPart:GetAttribute("DoorOpen") == true
			local nextState = not isOpen
			doorPart:SetAttribute("DoorOpen", nextState)

			-- Preferred: exact pose baked by the world builder (cannot jam or fall off its hinge).
			if not UtilityModules.ApplyDoorPose(doorPart, nextState) then
				-- Fallback for runtime-built doors: servo hinge, else a yaw nudge.
				local hinge = doorPart:FindFirstChildOfClass("HingeConstraint")
				if hinge and hinge.ActuatorType == Enum.ActuatorType.Servo then
					hinge.TargetAngle = if nextState then 88 else 0
				else
					local deltaYaw = if nextState then math.rad(85) else math.rad(-85)
					doorPart.CFrame = doorPart.CFrame * CFrame.Angles(0, deltaYaw, 0)
				end
			end
			EnemyAIService:NotifySound(doorPart.Position, 20, player)
		end
	elseif kind == "RestBunk" and typeof(payload.Target) == "Instance" then
		local part = if payload.Target:IsA("BasePart") then payload.Target else nil
		if part and SecurityService:IsWithinDistance(player, part.Position, 12.0) then
			local ok = SurvivalService:RestAtShelterBunk(player)
			if ok then
				ServiceRegistry.SendToPlayer(player, NetworkSchema.EVENTS.Notification, {
					Kind = "Info",
					Text = "Rested on emergency bunk (-Fatigue, -Stress).",
				})
			end
		end
	elseif kind == "LoreNote" and NetworkSchema.ValidateString(payload.LoreId, 64) then
		if typeof(payload.Target) == "Instance" and payload.Target:IsA("BasePart") then
			if not SecurityService:IsWithinDistance(player, payload.Target.Position, 14.0) then
				return
			end
		end
		QuestService:DiscoverLore(player, payload.LoreId)
	elseif kind == "QuestInteract" and NetworkSchema.ValidateString(payload.InteractId, 64) then
		if typeof(payload.Target) == "Instance" and payload.Target:IsA("BasePart") then
			if not SecurityService:IsWithinDistance(player, payload.Target.Position, 16.0) then
				return
			end
			EnemyAIService:NotifySound(payload.Target.Position, 55, player)
		end
		QuestService:NotifyProgress(player, "Interact", payload.InteractId, 1)
		ServiceRegistry.SendToPlayer(player, NetworkSchema.EVENTS.Notification, {
			Kind = "Success",
			Text = "Activated: " .. payload.InteractId,
		})
	elseif kind == "VehicleAlarm" and typeof(payload.Target) == "Instance" and payload.Target:IsA("BasePart") then
		if SecurityService:IsWithinDistance(player, payload.Target.Position, 14.0) then
			EnemyAIService:NotifySound(payload.Target.Position, 120, player)
			ServiceRegistry.BroadcastAll(NetworkSchema.EVENTS.HorrorCue, {
				Kind = "CarAlarm",
				Position = payload.Target.Position,
			})
		end
	end
end

-- 5. Connect Client -> Server Remotes with Token-Bucket Rate Limiting
local function connectGuarded(eventName: string, handler: (Player, any) -> ())
	local re = remoteEvents[eventName]
	if not re then
		return
	end
	re.OnServerEvent:Connect(function(player: Player, payload: any)
		if not SecurityService:CheckRateLimit(player, eventName) then
			return
		end
		handler(player, payload)
	end)
end

connectGuarded(NetworkSchema.EVENTS.MovementStance, function(player, payload)
	if type(payload) == "table" and NetworkSchema.ValidateString(payload.Stance, 16) then
		local surf = if type(payload.Surface) == "string" then payload.Surface else nil
		SurvivalService:SetStance(player, payload.Stance, surf)
	end
end)

connectGuarded(NetworkSchema.EVENTS.ToggleFlashlight, function(player, _)
	local on = SurvivalService:ToggleFlashlight(player) == true
	local char = player.Character
	local head = if char then char:FindFirstChild("Head") else nil
	local beam = if head then head:FindFirstChild("SurvivorFlashlight") else nil
	if beam and beam:IsA("SpotLight") then
		beam.Enabled = on
	end
end)

connectGuarded(NetworkSchema.EVENTS.InventoryAction, function(player, payload)
	InventoryService:HandleRemoteAction(player, payload)
end)

connectGuarded(NetworkSchema.EVENTS.LootInteract, function(player, payload)
	LootService:HandleRemoteAction(player, payload)
end)

connectGuarded(NetworkSchema.EVENTS.CraftRequest, function(player, payload)
	CraftingService:HandleRemoteAction(player, payload)
end)

connectGuarded(NetworkSchema.EVENTS.BuildAction, function(player, payload)
	BuildingService:HandleRemoteAction(player, payload)
end)

connectGuarded(NetworkSchema.EVENTS.WeaponFire, function(player, payload)
	CombatService:HandleWeaponFire(player, payload)
end)

connectGuarded(NetworkSchema.EVENTS.WeaponReload, function(player, payload)
	CombatService:HandleWeaponReload(player, payload)
end)

connectGuarded(NetworkSchema.EVENTS.MeleeSwing, function(player, payload)
	CombatService:HandleMeleeSwing(player, payload)
end)

connectGuarded(NetworkSchema.EVENTS.QuestAction, function(player, payload)
	QuestService:HandleRemoteAction(player, payload)
end)

connectGuarded(NetworkSchema.EVENTS.PartyAction, function(player, payload)
	PartyService:HandleRemoteAction(player, payload)
end)

connectGuarded(NetworkSchema.EVENTS.InteractWorldObject, handleWorldInteract)

local playerSettingsCache: { [number]: { [string]: any } } = {}
connectGuarded(NetworkSchema.EVENTS.SettingsUpdate, function(player, payload)
	if type(payload) ~= "table" then
		return
	end
	local cfg = playerSettingsCache[player.UserId]
		or {
			GraphicsPreset = "High",
			ReduceMotion = false,
			ScreenEffects = true,
			MasterVolume = 0.85,
			MouseSensitivity = 0.5,
			FOV = 78,
		}
	if type(payload.GraphicsPreset) == "string" and (Config.GRAPHICS_PRESETS :: any)[payload.GraphicsPreset] then
		cfg.GraphicsPreset = payload.GraphicsPreset
	end
	if type(payload.ReduceMotion) == "boolean" then
		cfg.ReduceMotion = payload.ReduceMotion
	end
	if type(payload.ScreenEffects) == "boolean" then
		cfg.ScreenEffects = payload.ScreenEffects
	end
	if type(payload.MasterVolume) == "number" then
		cfg.MasterVolume = math.clamp(payload.MasterVolume, 0, 1)
	end
	if type(payload.MouseSensitivity) == "number" then
		cfg.MouseSensitivity = math.clamp(payload.MouseSensitivity, 0.1, 3.0)
	end
	if type(payload.FOV) == "number" then
		cfg.FOV = math.clamp(payload.FOV, 65, 95)
	end
	playerSettingsCache[player.UserId] = cfg
end)

remoteFunctions[NetworkSchema.FUNCTIONS.PingState].OnServerInvoke = function(player: Player)
	if not SecurityService:CheckRateLimit(player, "PingState") then
		return nil
	end
	local profile = PlayerDataService:GetProfile(player)
	if not profile then
		return nil
	end
	return {
		Vitals = profile.Vitals,
		BodyRegions = profile.BodyRegions,
		Inventory = profile.Inventory,
		Quests = profile.Quests,
		DiscoveredLore = profile.DiscoveredLore,
		UnlockedBlueprints = profile.UnlockedBlueprints,
		Settings = playerSettingsCache[player.UserId],
		Stats = profile.Stats,
		Weather = WeatherService:GetCurrentWeather(),
		ClockTime = WeatherService:GetClockTime(),
	}
end

-- 6. Player Spawn & Underground Emergency Shelter Echo-07 Setup
local function setupCharacter(player: Player, char: Model)
	task.defer(function()
		for _, part in ipairs(char:GetDescendants()) do
			if part:IsA("BasePart") then
				pcall(function()
					part.CollisionGroup = "Survivors"
				end)
			end
		end

		local profile = PlayerDataService:GetProfile(player)
		local spawnTarget = Config.SHELTER_SPAWN_POS
		if profile and profile.Position then
			local savedPos = Vector3.new(profile.Position[1], profile.Position[2], profile.Position[3])
			if savedPos.Magnitude < 2500 and profile.Vitals.Health > 0 then
				spawnTarget = savedPos
			end
		end

		local root = char:WaitForChild("HumanoidRootPart", 5)
		if root and root:IsA("BasePart") then
			char:PivotTo(CFrame.new(spawnTarget))
		end

		-- Attach damaged emergency flashlight SpotLight to Survivor Head (visible to co-op peers)
		local initFlashOn = if profile then profile.Vitals.FlashlightOn == true else true
		char:SetAttribute("FlashlightOn", initFlashOn)
		local head = char:WaitForChild("Head", 5)
		if head and head:IsA("BasePart") and not head:FindFirstChild("SurvivorFlashlight") then
			local spot = Instance.new("SpotLight")
			spot.Name = "SurvivorFlashlight"
			spot.Face = Enum.NormalId.Front
			spot.Brightness = 3.6
			spot.Range = 60
			spot.Angle = 58
			spot.Color = Color3.fromRGB(255, 236, 198)
			spot.Shadows = true
			spot.Enabled = initFlashOn
			spot.Parent = head
		end

		-- Handle Roblox Humanoid death reset into Shelter Echo-07
		local hum = char:FindFirstChildOfClass("Humanoid")
		if hum then
			hum.Died:Connect(function()
				local p = PlayerDataService:GetProfile(player)
				if p then
					p.Position = { Config.SHELTER_SPAWN_POS.X, Config.SHELTER_SPAWN_POS.Y, Config.SHELTER_SPAWN_POS.Z }
					SurvivalService:ResetOnRespawn(player)
				end
			end)
		end

		if profile then
			SurvivalService:ApplyMobilityModifiers(player, profile)
			SurvivalService:SyncPlayerVitals(player, true)
			InventoryService:SyncInventory(player)
			QuestService:SyncQuests(player)
		end
	end)
end

local function onPlayerReady(player: Player)
	local profile = PlayerDataService:LoadPlayerProfile(player)
	if not profile then
		return
	end
	if next(profile.Inventory.Items) == nil then
		InventoryService:GrantStarterKit(player)
	end
	QuestService:EnsureInitialQuests(player)

	player.CharacterAdded:Connect(function(char)
		setupCharacter(player, char)
	end)
	if player.Character then
		setupCharacter(player, player.Character)
	end

	-- Atmospheric opening briefing inside Shelter Echo-07
	task.delay(2.0, function()
		if player.Parent == Players then
			ServiceRegistry.SendToPlayer(player, NetworkSchema.EVENTS.RadioTransmission, {
				Frequency = "142.8 MHz // ECHO-07 RECEIVER",
				Title = "HANDHELD FIELD RECEIVER",
				Text = "[STATIC] ...repeating on Civil Defense band: Shelter Echo-07 life support battery at 4%. Surface Relay on Maple Ridge required to triangulate Signal Origin...",
			})
		end
	end)
end

-- Start all services
for _, svc in ipairs(orderedServices) do
	svc:Start()
end

Players.PlayerAdded:Connect(onPlayerReady)
for _, existing in ipairs(Players:GetPlayers()) do
	task.spawn(onPlayerReady, existing)
end
