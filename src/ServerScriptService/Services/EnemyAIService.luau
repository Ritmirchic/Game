--!strict
--[[
    ASHES OF HUMANITY: LAST SIGNAL
    ServerScriptService.Services.EnemyAIService
    Intelligent 8-State Mutant AI with Vision Cone, Flashlight Aggro,
    Acoustic Perception Bus, PathfindingService, Stuck Recovery,
    Wall-Occlusion Checks, Co-op Scaling, and All 10 Mutant Species (Sections 21, 22).
]]

local PathfindingService = game:GetService("PathfindingService")
local Players = game:GetService("Players")
local ReplicatedStorage = game:GetService("ReplicatedStorage")
local ServerStorage = game:GetService("ServerStorage")
local Workspace = game:GetService("Workspace")

local Config = require(ReplicatedStorage.Shared.Config)
local EnemyDefinitions = require(ReplicatedStorage.Shared.EnemyDefinitions)
local NetworkSchema = require(ReplicatedStorage.Shared.NetworkSchema)
local Types = require(ReplicatedStorage.Shared.Types)
local UtilityModules = require(ReplicatedStorage.Shared.UtilityModules)

local EnemyAIService = {}

export type MutantAgent = {
	Id: string,
	SpeciesId: string,
	Def: EnemyDefinitions.EnemyDef,
	Model: Model,
	Root: BasePart,
	Humanoid: Humanoid?,
	HomePos: Vector3,
	PatrolTarget: Vector3,
	InvestigatePos: Vector3?,
	TargetPlayer: Player?,
	State: Types.AIState,
	Health: number,
	MaxHealth: number,
	NextTickAt: number,
	LastPathAt: number,
	Waypoints: { Vector3 },
	WaypointIndex: number,
	LastAttackAt: number,
	LastSpecialAt: number,
	StateEnteredAt: number,
	LastProgressPos: Vector3,
	StuckTime: number,
	StaggerUntil: number,
	ScreamedOnce: boolean,
}

local registry: any = nil
local mutants: { [string]: MutantAgent } = {}
local spatialGrid = UtilityModules.NewSpatialHashGrid(Config.SPATIAL_CELL_SIZE)
local spawnPoints: { { Pos: Vector3, Region: string, PreferredSpecies: string? } } = {}
local rng = Random.new()

local BODY_REGIONS_LIST = { "Torso", "LeftArm", "RightArm", "LeftLeg", "RightLeg", "Head" }

function EnemyAIService:Init(serviceRegistry: any)
	registry = serviceRegistry
end

local function createFallbackMutantModel(def: EnemyDefinitions.EnemyDef): Model
	local m = Instance.new("Model")
	m.Name = def.DisplayName

	local isQuad = (def.Id == "Crawler" or def.Id == "Burrower")
	local scale = if def.Id == "Brute" then 1.45 elseif def.Id == "Watcher" or def.Id == "PaleOne" then 1.38 else 1.05

	local root = Instance.new("Part")
	root.Name = "HumanoidRootPart"
	root.Size = if isQuad then Vector3.new(2.4, 1.8, 3.2) else Vector3.new(2.0 * scale, 4.2 * scale, 1.4 * scale)
	root.Transparency = 1.0
	root.Anchored = true
	root.CanCollide = true
	root.Parent = m

	local torso = Instance.new("Part")
	torso.Name = "Torso"
	torso.Size = if isQuad then Vector3.new(2.0, 1.2, 2.8) else Vector3.new(1.8 * scale, 2.2 * scale, 1.1 * scale)
	torso.Color = if def.Id == "PaleOne"
		then Color3.fromRGB(218, 214, 208)
		elseif def.Id == "Contaminated" then Color3.fromRGB(98, 138, 84)
		elseif def.Id == "Brute" then Color3.fromRGB(96, 92, 88)
		else Color3.fromRGB(74, 68, 64)
	torso.Material = if def.Id == "Brute" then Enum.Material.Concrete else Enum.Material.Slate
	torso.Anchored = false
	torso.CanCollide = false
	torso.CFrame = root.CFrame
	torso.Parent = m

	local weldT = Instance.new("WeldConstraint")
	weldT.Part0 = root
	weldT.Part1 = torso
	weldT.Parent = torso

	local head = Instance.new("Part")
	head.Name = "Head"
	head.Shape = Enum.PartType.Ball
	head.Size = Vector3.new(1.15 * scale, 1.25 * scale, 1.15 * scale)
	head.Color = torso.Color
	head.Material = torso.Material
	head.Anchored = false
	head.CanCollide = false
	head.CFrame = root.CFrame * CFrame.new(0, if isQuad then 0.35 else 1.65 * scale, if isQuad then -1.4 else 0)
	head.Parent = m

	local weldH = Instance.new("WeldConstraint")
	weldH.Part0 = root
	weldH.Part1 = head
	weldH.Parent = head

	local hum = Instance.new("Humanoid")
	hum.MaxHealth = def.MaxHealth
	hum.Health = def.MaxHealth
	hum.Parent = m

	m.PrimaryPart = root
	return m
end

function EnemyAIService:ScanSpawnMarkers()
	table.clear(spawnPoints)
	local folder = Workspace:FindFirstChild("EnemySpawns")
	if folder then
		for _, child in ipairs(folder:GetChildren()) do
			if child:IsA("BasePart") then
				local pos = child.Position
				local reg = child:GetAttribute("Region")
				if type(reg) ~= "string" or reg == "" then
					reg = UtilityModules.GetRegionAtPosition(pos)
				end
				local pref = child:GetAttribute("Species")
				table.insert(spawnPoints, {
					Pos = pos,
					Region = reg,
					PreferredSpecies = if type(pref) == "string" and pref ~= "" then pref else nil,
				})
			end
		end
	end

	-- Ensure every non-shelter region has patrol spawn points even in test harnesses
	if #spawnPoints == 0 then
		for id, reg in pairs(Config.REGIONS) do
			if id ~= "EmergencyShelter" then
				for i = 1, 4 do
					local ang = (i / 4) * math.pi * 2
					table.insert(spawnPoints, {
						Pos = reg.Center + Vector3.new(math.cos(ang) * 90, 3.5, math.sin(ang) * 90),
						Region = id,
						PreferredSpecies = nil,
					})
				end
			end
		end
	end
end

function EnemyAIService:SpawnMutant(speciesId: string, spawnPos: Vector3): MutantAgent?
	local def = EnemyDefinitions.Get(speciesId)
	if not def then
		return nil
	end

	local templatesFolder = ServerStorage:FindFirstChild("EnemyTemplates")
	local template = if templatesFolder then templatesFolder:FindFirstChild(speciesId) else nil
	local model: Model
	if template and template:IsA("Model") then
		model = template:Clone()
	else
		model = createFallbackMutantModel(def)
	end

	local root = (
		model:FindFirstChild("HumanoidRootPart")
		or model.PrimaryPart
		or model:FindFirstChildWhichIsA("BasePart")
	) :: BasePart?
	if not root then
		model:Destroy()
		return nil
	end
	model.PrimaryPart = root
	root.Anchored = true

	local id = UtilityModules.GenerateUID("mut_" .. speciesId)
	model:SetAttribute("MutantId", id)
	model:SetAttribute("SpeciesId", speciesId)
	model:SetAttribute("AIState", "IDLE")
	model:SetAttribute("AnimState", "Idle")

	-- Cooperative HP scaling (+18% per extra player)
	local coopCount = math.max(1, #Players:GetPlayers())
	local hpScale = 1.0 + (coopCount - 1) * 0.18
	local scaledMaxHP = math.floor(def.MaxHealth * hpScale)

	local hum = model:FindFirstChildOfClass("Humanoid")
	if hum then
		hum.MaxHealth = scaledMaxHP
		hum.Health = scaledMaxHP
	end

	model:PivotTo(CFrame.new(spawnPos))
	local container = Workspace:FindFirstChild("ActiveEnemies")
	if not container then
		local f = Instance.new("Folder")
		f.Name = "ActiveEnemies"
		f.Parent = Workspace
		container = f
	end
	model.Parent = container

	local agent: MutantAgent = {
		Id = id,
		SpeciesId = speciesId,
		Def = def,
		Model = model,
		Root = root,
		Humanoid = hum,
		HomePos = spawnPos,
		PatrolTarget = spawnPos,
		InvestigatePos = nil,
		TargetPlayer = nil,
		State = "IDLE",
		Health = scaledMaxHP,
		MaxHealth = scaledMaxHP,
		NextTickAt = os.clock() + rng:NextNumber(0.05, 0.25),
		LastPathAt = 0,
		Waypoints = {},
		WaypointIndex = 1,
		LastAttackAt = 0,
		LastSpecialAt = 0,
		StateEnteredAt = os.clock(),
		LastProgressPos = spawnPos,
		StuckTime = 0,
		StaggerUntil = 0,
		ScreamedOnce = false,
	}

	mutants[id] = agent
	spatialGrid:InsertOrUpdate(id, spawnPos, agent)
	return agent
end

function EnemyAIService:SetState(agent: MutantAgent, newState: Types.AIState, animOverride: string?)
	if agent.State == newState and not animOverride then
		return
	end
	agent.State = newState
	agent.StateEnteredAt = os.clock()
	agent.Model:SetAttribute("AIState", newState)

	local anim = animOverride
	if not anim then
		if newState == "IDLE" then
			anim = "Idle"
		elseif newState == "PATROL" or newState == "RETURN" then
			anim = if agent.SpeciesId == "Crawler" then "Crawl" else "Patrol"
		elseif newState == "INVESTIGATE" then
			anim = "Investigate"
		elseif newState == "SEARCH" then
			anim = "Search"
		elseif newState == "CHASE" then
			anim = if agent.SpeciesId == "Crawler" then "Crawl" else "Run"
		elseif newState == "ATTACK" then
			anim = "Attack"
		elseif newState == "RETREAT" then
			anim = "Retreat"
		end
	end
	agent.Model:SetAttribute("AnimState", anim or "Idle")
end

function EnemyAIService:HasClearLineOfSight(fromPos: Vector3, toPos: Vector3, ignoreList: { Instance }): boolean
	local delta = toPos - fromPos
	if delta.Magnitude < 1.0 then
		return true
	end
	local params = RaycastParams.new()
	params.FilterType = Enum.RaycastFilterType.Exclude
	params.FilterDescendantsInstances = ignoreList
	local hit = Workspace:Raycast(fromPos, delta, params)
	return hit == nil
end

function EnemyAIService:CanDetectPlayerVisually(agent: MutantAgent, player: Player, playerRoot: BasePart): boolean
	local profile = registry.PlayerDataService:GetProfile(player)
	if not profile or profile.Vitals.Health <= 0 then
		return false
	end

	local eyePos = agent.Root.Position + Vector3.new(0, 1.6, 0)
	local targetPos = playerRoot.Position + Vector3.new(0, 1.0, 0)
	local toTarget = targetPos - eyePos
	local dist = toTarget.Magnitude

	-- Effective sight range modulated by night, stance, and player flashlight!
	local isNight = if registry.WeatherService then registry.WeatherService:IsNight() else false
	local range = agent.Def.VisionRange
	if isNight and not agent.Def.NocturnalOnly then
		range *= 0.68
	end

	local stance = if registry.SurvivalService then registry.SurvivalService:GetStance(player) else "Walk"
	if stance == "Crawl" then
		range *= 0.38
	elseif stance == "Crouch" then
		range *= 0.64
	elseif stance == "Sprint" then
		range *= 1.22
	end

	if profile.Vitals.FlashlightOn then
		range *= agent.Def.FlashlightAggroMultiplier
	end

	if dist > range then
		return false
	end

	-- FOV cone check (unless player is within immediate 5.5-stud proximity)
	if dist > 5.5 then
		local look = agent.Root.CFrame.LookVector
		local dot = look:Dot(toTarget.Unit)
		local cosHalfFov = math.cos(math.rad(agent.Def.VisionFOVDeg * 0.5))
		if dot < cosHalfFov then
			return false
		end
	end

	local ignore = { agent.Model }
	if player.Character then
		table.insert(ignore, player.Character)
	end
	return self:HasClearLineOfSight(eyePos, targetPos, ignore)
end

function EnemyAIService:NotifySound(soundPos: Vector3, baseRadius: number, sourcePlayer: Player?)
	if baseRadius <= 0 then
		return
	end
	local maxQuery = baseRadius * 2.3
	local candidates = spatialGrid:QueryRadius(soundPos, maxQuery)
	for _, entry in ipairs(candidates) do
		local agent: MutantAgent = entry.Data
		if agent and agent.Health > 0 then
			local hearingRadius = baseRadius * agent.Def.HearingMultiplier
			if entry.Dist <= hearingRadius then
				if
					agent.State == "IDLE"
					or agent.State == "PATROL"
					or agent.State == "SEARCH"
					or agent.State == "RETURN"
				then
					agent.InvestigatePos = soundPos
					if sourcePlayer and entry.Dist <= hearingRadius * 0.35 and agent.SpeciesId == "Hollow" then
						agent.TargetPlayer = sourcePlayer
						self:SetState(agent, "CHASE")
					else
						self:SetState(agent, "INVESTIGATE")
					end
				end
			end
		end
	end
end

function EnemyAIService:ComputePathWaypoints(agent: MutantAgent, goal: Vector3)
	local now = os.clock()
	if (now - agent.LastPathAt) < Config.AI.PathRecomputeCooldown then
		return
	end
	agent.LastPathAt = now

	local ok, waypoints = pcall(function()
		local path = PathfindingService:CreatePath({
			AgentRadius = 2.0,
			AgentHeight = 5.0,
			AgentCanJump = true,
		})
		path:ComputeAsync(agent.Root.Position, goal)
		if path.Status == Enum.PathStatus.Success then
			local pts = {}
			local raw = path:GetWaypoints()
			for i = 2, #raw do
				table.insert(pts, raw[i].Position)
			end
			return pts
		end
		return nil
	end)

	if ok and waypoints and #waypoints > 0 then
		agent.Waypoints = waypoints
		agent.WaypointIndex = 1
	else
		agent.Waypoints = { goal }
		agent.WaypointIndex = 1
	end
end

local function groundSnapY(pos: Vector3, ignoreModel: Model): number
	local params = RaycastParams.new()
	params.FilterType = Enum.RaycastFilterType.Exclude
	params.FilterDescendantsInstances = { ignoreModel }
	local hit = Workspace:Raycast(Vector3.new(pos.X, pos.Y + 8, pos.Z), Vector3.new(0, -32, 0), params)
	if hit then
		return hit.Position.Y + 2.3
	end
	return pos.Y
end

function EnemyAIService:MoveAgentToward(agent: MutantAgent, goal: Vector3, speed: number, dt: number)
	if os.clock() < agent.StaggerUntil then
		return
	end
	self:ComputePathWaypoints(agent, goal)
	local targetPt = agent.Waypoints[agent.WaypointIndex] or goal
	local flatDelta = Vector3.new(targetPt.X - agent.Root.Position.X, 0, targetPt.Z - agent.Root.Position.Z)
	local dist = flatDelta.Magnitude

	if dist < 2.2 and agent.WaypointIndex < #agent.Waypoints then
		agent.WaypointIndex += 1
		targetPt = agent.Waypoints[agent.WaypointIndex]
		flatDelta = Vector3.new(targetPt.X - agent.Root.Position.X, 0, targetPt.Z - agent.Root.Position.Z)
		dist = flatDelta.Magnitude
	end

	if dist > 0.15 then
		local dir = flatDelta.Unit
		-- Crawler flank weave
		if agent.SpeciesId == "Crawler" and agent.State == "CHASE" and dist > 9 then
			local right = Vector3.new(-dir.Z, 0, dir.X)
			dir = (dir + right * math.sin(os.clock() * 4.5) * 0.42).Unit
		end

		-- Forward obstacle & barricade check (Brute smashes barricades!)
		local params = RaycastParams.new()
		params.FilterType = Enum.RaycastFilterType.Exclude
		params.FilterDescendantsInstances = { agent.Model }
		local fwdHit = Workspace:Raycast(agent.Root.Position, dir * 3.0, params)
		if fwdHit and fwdHit.Instance and fwdHit.Instance:IsA("BasePart") then
			if fwdHit.Instance:GetAttribute("DestructibleType") ~= nil and registry.CombatService then
				local smashDmg = if agent.SpeciesId == "Brute" then 65 else 18
				registry.CombatService:DamageDestructibleObject(fwdHit.Instance, smashDmg * dt * 2.0)
			elseif fwdHit.Instance:GetAttribute("StructureUID") ~= nil and registry.BuildingService then
				local sUid = fwdHit.Instance:GetAttribute("StructureUID")
				local smash = if agent.SpeciesId == "Brute" then 50 else 16
				registry.BuildingService:DamageStructure(tostring(sUid), smash * dt * 2.0)
				return
			end
		end

		local step = math.min(dist, speed * dt)
		local nextXZ = agent.Root.Position + dir * step
		local nextY = groundSnapY(nextXZ, agent.Model)

		-- Burrower subterranean visual dive while closing medium range
		if agent.SpeciesId == "Burrower" and agent.State == "CHASE" and dist > 11 and dist < 38 then
			nextY -= 1.5
			agent.Model:SetAttribute("Submerged", true)
		else
			agent.Model:SetAttribute("Submerged", false)
		end

		local nextPos = Vector3.new(nextXZ.X, nextY, nextXZ.Z)
		agent.Model:PivotTo(CFrame.lookAt(nextPos, nextPos + dir))
		spatialGrid:InsertOrUpdate(agent.Id, nextPos, agent)

		-- Stuck detection
		if (nextPos - agent.LastProgressPos).Magnitude < 0.35 then
			agent.StuckTime += dt
			if agent.StuckTime >= Config.AI.StuckTimeThreshold then
				agent.StuckTime = 0
				local side = Vector3.new(-dir.Z, 0, dir.X) * (if rng:NextNumber() > 0.5 then 4.5 else -4.5)
				agent.Waypoints = { agent.Root.Position + side, goal }
				agent.WaypointIndex = 1
			end
		else
			agent.LastProgressPos = nextPos
			agent.StuckTime = 0
		end
	end
end

function EnemyAIService:PerformSpeciesSpecials(agent: MutantAgent, dt: number)
	local now = os.clock()

	-- 1. Contaminated radiation & spore aura + Watcher/general stress aura
	for _, player in ipairs(Players:GetPlayers()) do
		local pRoot = registry.SecurityService:GetCharacterRoot(player)
		if pRoot then
			local d = (pRoot.Position - agent.Root.Position).Magnitude
			local profile = registry.PlayerDataService:GetProfile(player)
			if profile and profile.Vitals.Health > 0 then
				if d <= 38 and agent.Def.StressAuraPerSec > 0 then
					profile.Vitals.MentalStress = UtilityModules.Clamp(
						profile.Vitals.MentalStress + agent.Def.StressAuraPerSec * dt * (1 - d / 42),
						0,
						100
					)
				end
				if agent.SpeciesId == "Contaminated" and d <= 11 then
					local _, _, radRes = registry.SurvivalService:GetGearModifiers(profile)
					profile.Vitals.Radiation = UtilityModules.Clamp(
						profile.Vitals.Radiation + 9.5 * (1 - radRes) * dt,
						0,
						Config.SURVIVAL.MaxRadiation
					)
				end
			end
		end
	end

	-- 2. Screamer horde alert shriek on first chase acquisition
	if agent.SpeciesId == "Screamer" and agent.State == "CHASE" and not agent.ScreamedOnce and agent.TargetPlayer then
		agent.ScreamedOnce = true
		agent.LastSpecialAt = now
		local tRoot = registry.SecurityService:GetCharacterRoot(agent.TargetPlayer)
		local alertPos = if tRoot then tRoot.Position else agent.Root.Position
		self:NotifySound(alertPos, 145, agent.TargetPlayer)
		registry.BroadcastAll(NetworkSchema.EVENTS.HorrorCue, {
			Kind = "ScreamerShriek",
			Position = agent.Root.Position,
		})
	end

	-- 3. Mimic counterfeit radio SOS lure while patrolling/investigating
	if
		agent.SpeciesId == "Mimic"
		and (agent.State == "IDLE" or agent.State == "PATROL")
		and (now - agent.LastSpecialAt) > 28
	then
		agent.LastSpecialAt = now
		for _, player in ipairs(Players:GetPlayers()) do
			local pRoot = registry.SecurityService:GetCharacterRoot(player)
			if pRoot and (pRoot.Position - agent.Root.Position).Magnitude <= 110 then
				registry.SendToPlayer(player, NetworkSchema.EVENTS.RadioTransmission, {
					Frequency = "142.8 MHz",
					Text = "[STATIC] ...please—someone near the trees—my leg is pinned, bring a bandage...",
					Distorted = true,
				})
			end
		end
	end

	-- 4. PaleOne nocturnal despawn at daybreak
	if agent.Def.NocturnalOnly and registry.WeatherService and not registry.WeatherService:IsNight() then
		self:DespawnMutant(agent.Id)
	end
end

function EnemyAIService:FindBestTarget(agent: MutantAgent): (Player?, BasePart?, number)
	local bestPlayer: Player? = nil
	local bestRoot: BasePart? = nil
	local bestScore = math.huge

	local playersList = Players:GetPlayers()
	for _, player in ipairs(playersList) do
		local pRoot = registry.SecurityService:GetCharacterRoot(player)
		local profile = registry.PlayerDataService:GetProfile(player)
		if pRoot and profile and profile.Vitals.Health > 0 and not profile.Vitals.IsDowned then
			if self:CanDetectPlayerVisually(agent, player, pRoot) then
				local dist = (pRoot.Position - agent.Root.Position).Magnitude
				local score = dist
				-- Stalker prefers isolated players (far from teammates)
				if agent.SpeciesId == "Stalker" and #playersList > 1 then
					for _, other in ipairs(playersList) do
						if other ~= player then
							local oRoot = registry.SecurityService:GetCharacterRoot(other)
							if oRoot and (oRoot.Position - pRoot.Position).Magnitude < 25 then
								score += 35 -- Penalize grouped survivors
							end
						end
					end
				end
				if score < bestScore then
					bestScore = score
					bestPlayer = player
					bestRoot = pRoot
				end
			end
		end
	end
	return bestPlayer, bestRoot, bestScore
end

function EnemyAIService:StepAgent(agent: MutantAgent, dt: number)
	if agent.Health <= 0 or not agent.Model.Parent then
		return
	end

	self:PerformSpeciesSpecials(agent, dt)
	if not mutants[agent.Id] then
		return
	end

	local now = os.clock()
	-- Check retreat threshold
	if
		agent.Def.RetreatHPFraction > 0
		and (agent.Health / agent.MaxHealth) <= agent.Def.RetreatHPFraction
		and agent.State ~= "RETREAT"
	then
		self:SetState(agent, "RETREAT")
	end

	-- Spike Barricade proximity check
	if registry.BuildingService then
		local trapDmg = registry.BuildingService:CheckDefensiveSpikesAt(agent.Root.Position)
		if trapDmg > 0 then
			self:ApplyDamageToMutant(agent.Id, trapDmg * dt, "LeftLeg", 0.1, nil)
			if agent.Health <= 0 then
				return
			end
		end
	end

	if agent.State == "RETREAT" then
		local away = (agent.HomePos - agent.Root.Position)
		local retreatGoal = if away.Magnitude > 8
			then agent.HomePos
			else (agent.Root.Position + Vector3.new(36, 0, -36))
		self:MoveAgentToward(agent, retreatGoal, agent.Def.ChaseSpeed * 0.92, dt)
		if (now - agent.StateEnteredAt) > 7.5 then
			self:SetState(agent, "RETURN")
		end
		return
	end

	-- Scan for visible players
	local seenPlayer, seenRoot = self:FindBestTarget(agent)
	if seenPlayer and seenRoot then
		agent.TargetPlayer = seenPlayer
		agent.InvestigatePos = seenRoot.Position
		local dist = (seenRoot.Position - agent.Root.Position).Magnitude

		-- Watcher behavior: observes silently from 26..65 studs unless approached!
		if agent.SpeciesId == "Watcher" and dist > 26 and agent.Health >= agent.MaxHealth * 0.98 then
			local facePos = Vector3.new(seenRoot.Position.X, agent.Root.Position.Y, seenRoot.Position.Z)
			agent.Model:PivotTo(CFrame.lookAt(agent.Root.Position, facePos))
			self:SetState(agent, "INVESTIGATE", "Idle")
			return
		end

		if dist <= agent.Def.AttackRange then
			self:SetState(agent, "ATTACK")
		else
			self:SetState(agent, "CHASE")
		end
	end

	if agent.State == "IDLE" then
		if (now - agent.StateEnteredAt) >= 3.2 then
			local ang = rng:NextNumber(0, math.pi * 2)
			local rad = rng:NextNumber(18, 46)
			agent.PatrolTarget = agent.HomePos + Vector3.new(math.cos(ang) * rad, 0, math.sin(ang) * rad)
			self:SetState(agent, "PATROL")
		end
	elseif agent.State == "PATROL" then
		self:MoveAgentToward(agent, agent.PatrolTarget, agent.Def.WalkSpeed, dt)
		if
			(Vector3.new(
					agent.PatrolTarget.X - agent.Root.Position.X,
					0,
					agent.PatrolTarget.Z - agent.Root.Position.Z
				)).Magnitude
				< 3.5
			or (now - agent.StateEnteredAt) > 14
		then
			self:SetState(agent, "IDLE")
		end
	elseif agent.State == "INVESTIGATE" then
		local goal = agent.InvestigatePos or agent.HomePos
		local invSpeed = if agent.SpeciesId == "Hollow" then agent.Def.WalkSpeed * 1.45 else agent.Def.WalkSpeed * 1.15
		self:MoveAgentToward(agent, goal, invSpeed, dt)
		if
			(Vector3.new(goal.X - agent.Root.Position.X, 0, goal.Z - agent.Root.Position.Z)).Magnitude < 4.0
			or (now - agent.StateEnteredAt) > 12
		then
			self:SetState(agent, "SEARCH")
		end
	elseif agent.State == "SEARCH" then
		local baseGoal = agent.InvestigatePos or agent.Root.Position
		local sweep = baseGoal + Vector3.new(math.cos(now * 0.9) * 10, 0, math.sin(now * 0.9) * 10)
		self:MoveAgentToward(agent, sweep, agent.Def.WalkSpeed * 0.85, dt)
		if (now - agent.StateEnteredAt) > 6.5 then
			self:SetState(agent, "RETURN")
		end
	elseif agent.State == "RETURN" then
		self:MoveAgentToward(agent, agent.HomePos, agent.Def.WalkSpeed, dt)
		if
			(Vector3.new(agent.HomePos.X - agent.Root.Position.X, 0, agent.HomePos.Z - agent.Root.Position.Z)).Magnitude
				< 4.0
			or (now - agent.StateEnteredAt) > 18
		then
			self:SetState(agent, "IDLE")
		end
	elseif agent.State == "CHASE" then
		local tPlayer = agent.TargetPlayer
		local tRoot = if tPlayer then registry.SecurityService:GetCharacterRoot(tPlayer) else nil
		if not tPlayer or not tRoot then
			self:SetState(agent, "SEARCH")
			return
		end
		local dist = (tRoot.Position - agent.Root.Position).Magnitude
		if dist > agent.Def.VisionRange * 1.75 then
			agent.InvestigatePos = tRoot.Position
			agent.TargetPlayer = nil
			self:SetState(agent, "INVESTIGATE")
			return
		end
		if dist <= agent.Def.AttackRange then
			self:SetState(agent, "ATTACK")
		else
			self:MoveAgentToward(agent, tRoot.Position, agent.Def.ChaseSpeed, dt)
		end
	elseif agent.State == "ATTACK" then
		local tPlayer = agent.TargetPlayer
		local tRoot = if tPlayer then registry.SecurityService:GetCharacterRoot(tPlayer) else nil
		if not tPlayer or not tRoot then
			self:SetState(agent, "SEARCH")
			return
		end
		local dist = (tRoot.Position - agent.Root.Position).Magnitude
		if dist > agent.Def.AttackRange * 1.22 then
			self:SetState(agent, "CHASE")
			return
		end

		local facePos = Vector3.new(tRoot.Position.X, agent.Root.Position.Y, tRoot.Position.Z)
		agent.Model:PivotTo(CFrame.lookAt(agent.Root.Position, facePos))

		if (now - agent.LastAttackAt) >= agent.Def.AttackCooldown then
			agent.LastAttackAt = now
			-- CRITICAL WALL-OCCLUSION CHECK: Never deal damage through walls!
			local ignore = { agent.Model }
			if tPlayer.Character then
				table.insert(ignore, tPlayer.Character)
			end
			if
				self:HasClearLineOfSight(
					agent.Root.Position + Vector3.new(0, 1.4, 0),
					tRoot.Position + Vector3.new(0, 1.0, 0),
					ignore
				)
			then
				agent.Model:SetAttribute("AnimState", "Attack")
				local regionHit = BODY_REGIONS_LIST[rng:NextInteger(1, #BODY_REGIONS_LIST)]
				registry.SurvivalService:InflictLocalizedDamage(tPlayer, agent.Def.AttackDamage, regionHit, {
					BleedChance = agent.Def.BleedChance,
					FractureChance = agent.Def.FractureChance,
					InfectionChance = agent.Def.InfectionChance,
					RadiationAdd = agent.Def.RadiationPulse,
				})
				registry.SendToPlayer(tPlayer, NetworkSchema.EVENTS.CombatFeedback, {
					Kind = "PlayerStruck",
					SpeciesId = agent.SpeciesId,
					BodyRegion = regionHit,
				})
			else
				-- Blocked by wall -> repath around doorway!
				self:SetState(agent, "CHASE")
			end
		end
	end
end

function EnemyAIService:ApplyDamageToMutant(
	mutantId: string,
	rawDamage: number,
	hitPartName: string?,
	staggerPower: number?,
	attacker: Player?
): (number, boolean, boolean)
	local agent = mutants[mutantId]
	if not agent or agent.Health <= 0 then
		return 0, false, false
	end

	local isWeakspot = false
	local mult = agent.Def.ArmorMultiplier
	if hitPartName and string.find(string.lower(hitPartName), string.lower(agent.Def.WeakspotPart)) then
		isWeakspot = true
		mult = agent.Def.WeakspotMultiplier
	end

	local finalDmg = math.max(1, math.floor(rawDamage * mult + 0.5))
	agent.Health = math.max(0, agent.Health - finalDmg)
	if agent.Humanoid then
		agent.Humanoid.Health = agent.Health
	end

	if staggerPower and staggerPower > 0.35 then
		agent.StaggerUntil = os.clock() + staggerPower * 0.35
		agent.Model:SetAttribute("AnimState", "DamageReact")
	end

	if agent.Health <= 0 then
		self:HandleMutantDeath(agent, attacker)
		return finalDmg, true, isWeakspot
	end

	-- Aggro onto attacker
	if attacker then
		agent.TargetPlayer = attacker
		local aRoot = registry.SecurityService:GetCharacterRoot(attacker)
		if aRoot then
			agent.InvestigatePos = aRoot.Position
		end
		if agent.State ~= "ATTACK" then
			self:SetState(agent, "CHASE")
		end
	end

	return finalDmg, false, isWeakspot
end

function EnemyAIService:HandleMutantDeath(agent: MutantAgent, killer: Player?)
	spatialGrid:Remove(agent.Id)
	mutants[agent.Id] = nil
	agent.Model:SetAttribute("AIState", "DEAD")
	agent.Model:SetAttribute("AnimState", "Death")

	-- Drop loot items at mutant's death location
	if registry.LootService then
		for _, drop in ipairs(agent.Def.Drops) do
			if rng:NextNumber() <= drop.Chance then
				local qty = rng:NextInteger(drop.Min, drop.Max)
				local offset = Vector3.new(rng:NextNumber(-1.5, 1.5), -1.0, rng:NextNumber(-1.5, 1.5))
				registry.LootService:SpawnDroppedItem(drop.ItemId, qty, 100, agent.Root.Position + offset)
			end
		end
	end

	if killer then
		local profile = registry.PlayerDataService:GetProfile(killer)
		if profile then
			profile.Stats.MutantsKilled += 1
		end
		if registry.QuestService then
			registry.QuestService:NotifyProgress(killer, "KillSpecies", agent.SpeciesId, 1)
		end
	end

	task.delay(4.0, function()
		if agent.Model and agent.Model.Parent then
			agent.Model:Destroy()
		end
	end)
end

function EnemyAIService:DespawnMutant(mutantId: string)
	local agent = mutants[mutantId]
	if not agent then
		return
	end
	spatialGrid:Remove(mutantId)
	mutants[mutantId] = nil
	if agent.Model and agent.Model.Parent then
		agent.Model:Destroy()
	end
end

function EnemyAIService:MaintainWorldPopulation()
	local playersList = Players:GetPlayers()
	if #playersList == 0 then
		return
	end

	local activeCount = 0
	for id, agent in pairs(mutants) do
		local minDist = math.huge
		for _, p in ipairs(playersList) do
			local r = registry.SecurityService:GetCharacterRoot(p)
			if r then
				local d = (r.Position - agent.Root.Position).Magnitude
				if d < minDist then
					minDist = d
				end
			end
		end
		if minDist > Config.AI.DespawnDist then
			self:DespawnMutant(id)
		else
			activeCount += 1
		end
	end

	local cap = Config.AI.MaxActiveMutantsBase + math.max(0, #playersList - 1) * Config.AI.ExtraMutantsPerCoopPlayer
	if activeCount >= cap or #spawnPoints == 0 then
		return
	end

	local isNight = if registry.WeatherService then registry.WeatherService:IsNight() else false
	for _ = 1, math.min(4, cap - activeCount) do
		local sp = spawnPoints[rng:NextInteger(1, #spawnPoints)]
		-- Spawn within 75..380 studs of at least one player, never right on top of them
		local nearAny = false
		local tooClose = false
		for _, p in ipairs(playersList) do
			local r = registry.SecurityService:GetCharacterRoot(p)
			if r then
				local d = (r.Position - sp.Pos).Magnitude
				if d < 55 then
					tooClose = true
				elseif d <= 400 then
					nearAny = true
				end
			end
		end

		if nearAny and not tooClose then
			local chosenSpecies = sp.PreferredSpecies
			if not chosenSpecies then
				local candidates: { string } = {}
				for sId, sDef in pairs(EnemyDefinitions.SPECIES) do
					if (not sDef.NocturnalOnly or isNight) and table.find(sDef.Regions, sp.Region) then
						table.insert(candidates, sId)
					end
				end
				if #candidates > 0 then
					chosenSpecies = candidates[rng:NextInteger(1, #candidates)]
				end
			end
			if chosenSpecies then
				local sDef = EnemyDefinitions.Get(chosenSpecies)
				if sDef and (not sDef.NocturnalOnly or isNight) then
					self:SpawnMutant(chosenSpecies, sp.Pos)
				end
			end
		end
	end
end

function EnemyAIService:GetActiveMutantCount(): number
	local n = 0
	for _ in pairs(mutants) do
		n += 1
	end
	return n
end

function EnemyAIService:GetAgent(mutantId: string): MutantAgent?
	return mutants[mutantId]
end

function EnemyAIService:Start()
	self:ScanSpawnMarkers()

	-- Distance-tiered AI scheduler loop
	task.spawn(function()
		local lastMaintain = 0
		while true do
			local now = os.clock()
			if (now - lastMaintain) >= 5.0 then
				lastMaintain = now
				self:MaintainWorldPopulation()
			end

			local playersList = Players:GetPlayers()
			for _, agent in pairs(mutants) do
				if now >= agent.NextTickAt then
					local minDist = math.huge
					for _, p in ipairs(playersList) do
						local r = registry.SecurityService:GetCharacterRoot(p)
						if r then
							local d = (r.Position - agent.Root.Position).Magnitude
							if d < minDist then
								minDist = d
							end
						end
					end

					local interval = if minDist <= Config.AI.NearDist
						then Config.AI.NearTickRate
						elseif minDist <= Config.AI.MidDist then Config.AI.MidTickRate
						else Config.AI.FarTickRate

					agent.NextTickAt = now + interval
					self:StepAgent(agent, interval)
				end
			end
			task.wait(0.08)
		end
	end)
end

return EnemyAIService
