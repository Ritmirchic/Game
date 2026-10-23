--!strict
--[[
    ASHES OF HUMANITY: LAST SIGNAL
    ServerScriptService.Services.BuildingService
    Functional Base Building, Window Fortification, Generator Power Grids,
    Campfire Warmth, Repairing, Dismantling, and Persistence (Section 20).
]]

local ReplicatedStorage = game:GetService("ReplicatedStorage")
local ServerStorage = game:GetService("ServerStorage")
local Workspace = game:GetService("Workspace")

local BuildingDefinitions = require(ReplicatedStorage.Shared.BuildingDefinitions)
local Config = require(ReplicatedStorage.Shared.Config)
local NetworkSchema = require(ReplicatedStorage.Shared.NetworkSchema)
local Types = require(ReplicatedStorage.Shared.Types)
local UtilityModules = require(ReplicatedStorage.Shared.UtilityModules)

local BuildingService = {}

export type RuntimeStructure = {
	Data: Types.PlacedStructureData,
	Def: BuildingDefinitions.BuildingDef,
	Model: Model,
	Primary: BasePart,
}

local registry: any = nil
local structures: { [string]: RuntimeStructure } = {}
local structureGrid = UtilityModules.NewSpatialHashGrid(40)
local WORLD_SAVE_KEY = "GlobalServerStructures_v1"

local function snapPosition(pos: Vector3, gridSize: number): Vector3
	local sx = math.floor(pos.X / gridSize + 0.5) * gridSize
	local sy = math.floor(pos.Y * 2 + 0.5) / 2
	local sz = math.floor(pos.Z / gridSize + 0.5) * gridSize
	return Vector3.new(sx, sy, sz)
end

local function createRuntimeStructureModel(def: BuildingDefinitions.BuildingDef, uid: string): (Model, BasePart)
	local templates = ServerStorage:FindFirstChild("BuildingTemplates")
	local tmpl = if templates then templates:FindFirstChild(def.Id) else nil
	if tmpl and tmpl:IsA("Model") then
		local cloned = tmpl:Clone()
		local prim = (cloned.PrimaryPart or cloned:FindFirstChildWhichIsA("BasePart")) :: BasePart
		cloned.PrimaryPart = prim
		for _, d in ipairs(cloned:GetDescendants()) do
			if d:IsA("BasePart") then
				d:SetAttribute("StructureUID", uid)
			end
		end
		return cloned, prim
	end

	local m = Instance.new("Model")
	m.Name = def.Id .. "_" .. uid
	local part = Instance.new("Part")
	part.Name = "Primary"
	part.Size = def.Size
	part.Anchored = true
	part.CanCollide = true
	part:SetAttribute("StructureUID", uid)

	if def.Id == "ReinforcedWall" or def.Id == "DieselGenerator" or def.Id == "PerimeterFloodlight" then
		part.Material = Enum.Material.CorrodedMetal
		part.Color = Color3.fromRGB(92, 96, 98)
	elseif def.Id == "CampfireStation" then
		part.Material = Enum.Material.Slate
		part.Color = Color3.fromRGB(88, 72, 60)
		local light = Instance.new("PointLight")
		light.Name = "FireLight"
		light.Color = Color3.fromRGB(255, 152, 68)
		light.Brightness = 2.2
		light.Range = 22
		light.Shadows = true
		light.Parent = part
	else
		part.Material = Enum.Material.WoodPlanks
		part.Color = Color3.fromRGB(108, 84, 62)
	end

	if def.Id == "PerimeterFloodlight" then
		local spot = Instance.new("SpotLight")
		spot.Name = "FloodBeam"
		spot.Brightness = 3.6
		spot.Range = 54
		spot.Angle = 72
		spot.Color = Color3.fromRGB(255, 244, 214)
		spot.Shadows = true
		spot.Enabled = false
		spot.Parent = part
	end

	if def.StationType then
		part:SetAttribute("StationType", def.StationType)
	end

	if def.Id == "SecurityDoor" then
		part:SetAttribute("InteractiveType", "Door")
		part:SetAttribute("DoorOpen", false)
	end

	part.Parent = m
	m.PrimaryPart = part
	return m, part
end

function BuildingService:Init(serviceRegistry: any)
	registry = serviceRegistry
end

function BuildingService:GetStructuresFolder(): Folder
	local world = Workspace:FindFirstChild("World")
	local parentInst: Instance = world or Workspace
	local existing = parentInst:FindFirstChild("PlayerStructures")
	if existing and existing:IsA("Folder") then
		return existing
	end
	local f = Instance.new("Folder")
	f.Name = "PlayerStructures"
	f.Parent = parentInst
	return f
end

function BuildingService:CountPlayerStructures(userId: number): number
	local c = 0
	for _, st in pairs(structures) do
		if st.Data.OwnerUserId == userId then
			c += 1
		end
	end
	return c
end

function BuildingService:CanPlayerModify(player: Player, st: RuntimeStructure): boolean
	if st.Data.OwnerUserId == player.UserId then
		return true
	end
	if registry.PartyService and registry.PartyService:AreInSameParty(player.UserId, st.Data.OwnerUserId) then
		return true
	end
	return false
end

function BuildingService:SpawnFromData(data: Types.PlacedStructureData): RuntimeStructure?
	local def = BuildingDefinitions.Get(data.BuildingId)
	if not def then
		return nil
	end

	local model, prim = createRuntimeStructureModel(def, data.StructureUID)
	local pos = Vector3.new(data.Position[1], data.Position[2], data.Position[3])
	local cf = CFrame.new(pos) * CFrame.Angles(0, math.rad(data.YawDeg), 0)
	model:PivotTo(cf)
	model:SetAttribute("StructureUID", data.StructureUID)
	model:SetAttribute("BuildingId", data.BuildingId)
	model:SetAttribute("StructureHP", data.HP)
	model:SetAttribute("MaxHP", data.MaxHP)
	model.Parent = self:GetStructuresFolder()

	local rt: RuntimeStructure = {
		Data = data,
		Def = def,
		Model = model,
		Primary = prim,
	}
	structures[data.StructureUID] = rt
	structureGrid:InsertOrUpdate(data.StructureUID, pos, rt)
	return rt
end

function BuildingService:PlaceStructure(
	player: Player,
	buildingId: string,
	rawPos: Vector3,
	yawDeg: number
): (boolean, string?)
	local def = BuildingDefinitions.Get(buildingId)
	if not def then
		return false, "UnknownStructure"
	end

	local profile = registry.PlayerDataService:GetProfile(player)
	if not profile or profile.Vitals.Health <= 0 or profile.Vitals.IsDowned then
		return false, "Unavailable"
	end

	if self:CountPlayerStructures(player.UserId) >= Config.BUILDING.MaxStructuresPerPlayer then
		return false, "StructureLimitReached"
	end

	local snappedPos = if def.SnapToWindowOrDoor then rawPos else snapPosition(rawPos, Config.BUILDING.GridSnapStuds)
	local cleanYaw = (math.floor(yawDeg / Config.BUILDING.RotationStepDeg + 0.5) * Config.BUILDING.RotationStepDeg)
		% 360

	if not registry.SecurityService:IsWithinDistance(player, snappedPos, Config.BUILDING.MaxBuildDistance) then
		return false, "TooFarToBuild"
	end

	-- Prevent building inside Echo-07 spawn room core
	if (snappedPos - Config.SHELTER_SPAWN_POS).Magnitude < 16 then
		return false, "ProtectedShelterZone"
	end

	-- Overlap validation against existing structures of same footprint
	local nearby = structureGrid:QueryRadius(snappedPos, 6.0)
	for _, entry in ipairs(nearby) do
		local other: RuntimeStructure = entry.Data
		if other.Data.BuildingId == buildingId and (entry.Pos - snappedPos).Magnitude < 1.8 then
			return false, "OverlappingStructure"
		end
	end

	-- Verify & consume material costs atomically
	for _, cost in ipairs(def.Costs) do
		if registry.InventoryService:CountItem(player, cost.ItemId) < cost.Count then
			return false, "InsufficientMaterials"
		end
	end
	for _, cost in ipairs(def.Costs) do
		registry.InventoryService:RemoveItemById(player, cost.ItemId, cost.Count, true)
	end
	registry.InventoryService:SyncInventory(player)

	local uid = UtilityModules.GenerateUID("bld")
	local partyId = if registry.PartyService then registry.PartyService:GetPlayerPartyId(player.UserId) else nil
	local data: Types.PlacedStructureData = {
		StructureUID = uid,
		BuildingId = buildingId,
		OwnerUserId = player.UserId,
		PartyId = partyId,
		Position = { snappedPos.X, snappedPos.Y, snappedPos.Z },
		YawDeg = cleanYaw,
		HP = def.MaxHP,
		MaxHP = def.MaxHP,
		Powered = def.IsGenerator,
		Fuel = if def.IsGenerator then 100.0 else nil,
		StorageItems = if def.IsStorage then {} else nil,
	}

	self:SpawnFromData(data)
	self:RefreshPowerNetwork()

	profile.Stats.StructuresBuilt += 1
	if registry.QuestService then
		registry.QuestService:NotifyProgress(player, "BuildStructure", buildingId, 1)
	end
	if registry.EnemyAIService then
		registry.EnemyAIService:NotifySound(snappedPos, 28, player)
	end

	return true, uid
end

function BuildingService:RepairStructure(player: Player, structureUid: string): (boolean, string?)
	local st = structures[structureUid]
	if not st then
		return false, "NotFound"
	end
	if not self:CanPlayerModify(player, st) then
		return false, "NoPermission"
	end
	local pos = st.Primary.Position
	if not registry.SecurityService:IsWithinDistance(player, pos, 14.0) then
		return false, "TooFar"
	end
	if st.Data.HP >= st.Data.MaxHP then
		return false, "FullDurability"
	end

	local rc = st.Def.RepairCost
	if registry.InventoryService:CountItem(player, rc.ItemId) < rc.Count then
		return false, "MissingRepairMaterial"
	end
	registry.InventoryService:RemoveItemById(player, rc.ItemId, rc.Count, false)

	st.Data.HP = math.min(st.Data.MaxHP, st.Data.HP + math.floor(st.Data.MaxHP * 0.45))
	st.Model:SetAttribute("StructureHP", st.Data.HP)
	return true, nil
end

function BuildingService:DismantleStructure(player: Player, structureUid: string): (boolean, string?)
	local st = structures[structureUid]
	if not st then
		return false, "NotFound"
	end
	if not self:CanPlayerModify(player, st) then
		return false, "NoPermission"
	end
	if not registry.SecurityService:IsWithinDistance(player, st.Primary.Position, 16.0) then
		return false, "TooFar"
	end

	-- Refund 50% of build materials (rounded up for single-cost items if >60% HP)
	local hpRatio = st.Data.HP / math.max(1, st.Data.MaxHP)
	for _, cost in ipairs(st.Def.Costs) do
		local refund = math.max(1, math.floor(cost.Count * 0.5 * hpRatio + 0.4))
		registry.InventoryService:AddItem(player, cost.ItemId, refund, nil, true)
	end
	registry.InventoryService:SyncInventory(player)

	structures[structureUid] = nil
	structureGrid:Remove(structureUid)
	st.Model:Destroy()
	self:RefreshPowerNetwork()
	return true, nil
end

function BuildingService:DamageStructure(structureUid: string, damage: number)
	local st = structures[structureUid]
	if not st then
		return
	end
	st.Data.HP = math.max(0, st.Data.HP - damage)
	st.Model:SetAttribute("StructureHP", st.Data.HP)
	if st.Data.HP <= 0 then
		structures[structureUid] = nil
		structureGrid:Remove(structureUid)
		st.Model:Destroy()
		self:RefreshPowerNetwork()
	end
end

function BuildingService:RefreshPowerNetwork()
	local activeGenerators: { Vector3 } = {}
	for _, st in pairs(structures) do
		if st.Def.IsGenerator and st.Data.Powered and (st.Data.Fuel or 0) > 0 then
			table.insert(activeGenerators, st.Primary.Position)
		end
	end

	for _, st in pairs(structures) do
		if st.Def.RequiresPower then
			local powered = false
			for _, gPos in ipairs(activeGenerators) do
				if (gPos - st.Primary.Position).Magnitude <= Config.BUILDING.GeneratorPowerRadius then
					powered = true
					break
				end
			end
			st.Data.Powered = powered
			local spot = st.Primary:FindFirstChild("FloodBeam")
			if spot and spot:IsA("SpotLight") then
				spot.Enabled = powered
			end
		end
	end
end

function BuildingService:GetWarmthBonusAt(pos: Vector3): number
	local maxWarmth = 0.0
	local nearby = structureGrid:QueryRadius(pos, 24.0)
	for _, entry in ipairs(nearby) do
		local st: RuntimeStructure = entry.Data
		if st.Def.WarmthRadius > 0 and entry.Dist <= st.Def.WarmthRadius then
			maxWarmth = math.max(maxWarmth, st.Def.WarmthBonusC)
		end
	end
	return maxWarmth
end

function BuildingService:HasNearbyStation(pos: Vector3, stationType: string, maxDist: number): boolean
	local nearby = structureGrid:QueryRadius(pos, maxDist)
	for _, entry in ipairs(nearby) do
		local st: RuntimeStructure = entry.Data
		if st.Def.StationType == stationType then
			return true
		end
	end
	return false
end

function BuildingService:CheckDefensiveSpikesAt(pos: Vector3): number
	local nearby = structureGrid:QueryRadius(pos, 4.2)
	for _, entry in ipairs(nearby) do
		local st: RuntimeStructure = entry.Data
		if st.Def.TrapDamage > 0 then
			self:DamageStructure(st.Data.StructureUID, 4.0)
			return st.Def.TrapDamage
		end
	end
	return 0
end

function BuildingService:PersistStructures()
	local serialized: { Types.PlacedStructureData } = {}
	for _, st in pairs(structures) do
		table.insert(serialized, st.Data)
	end
	registry.PlayerDataService:SaveWorldStructures(WORLD_SAVE_KEY, serialized)
end

function BuildingService:HandleRemoteAction(player: Player, payload: any)
	if type(payload) ~= "table" or not NetworkSchema.ValidateString(payload.Action, 24) then
		return
	end
	registry.SecurityService:WithTransactionLock(player, function()
		local act = payload.Action
		if
			act == "Place"
			and NetworkSchema.ValidateString(payload.BuildingId, 48)
			and NetworkSchema.ValidateVector3(payload.Position, 2000)
			and NetworkSchema.ValidateNumber(payload.YawDeg or 0, -720, 720)
		then
			return self:PlaceStructure(player, payload.BuildingId, payload.Position, payload.YawDeg or 0)
		elseif act == "Repair" and NetworkSchema.ValidateString(payload.StructureUID, 64) then
			return self:RepairStructure(player, payload.StructureUID)
		elseif act == "Dismantle" and NetworkSchema.ValidateString(payload.StructureUID, 64) then
			return self:DismantleStructure(player, payload.StructureUID)
		end
		return false, "InvalidBuildAction"
	end)
end

function BuildingService:GetStructureCount(): number
	local c = 0
	for _ in pairs(structures) do
		c += 1
	end
	return c
end

function BuildingService:Start()
	local saved = registry.PlayerDataService:LoadWorldStructures(WORLD_SAVE_KEY)
	for _, entry in ipairs(saved) do
		self:SpawnFromData(entry)
	end
	self:RefreshPowerNetwork()

	-- Generator fuel burn & mechanical hum loop
	task.spawn(function()
		while true do
			task.wait(5.0)
			local anyChanged = false
			for _, st in pairs(structures) do
				if st.Def.IsGenerator and st.Data.Powered and (st.Data.Fuel or 0) > 0 then
					st.Data.Fuel = math.max(0, (st.Data.Fuel or 0) - 0.5)
					if registry.EnemyAIService then
						registry.EnemyAIService:NotifySound(st.Primary.Position, 34, nil)
					end
					if st.Data.Fuel <= 0 then
						st.Data.Powered = false
						anyChanged = true
					end
				end
			end
			if anyChanged then
				self:RefreshPowerNetwork()
			end
		end
	end)
end

return BuildingService
