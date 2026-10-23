--!strict
--[[
	EnemyService — spawning, AI ticking, pathfinding and combat for mutants.

	Design notes
	  * Spawn anchors are baked into the world (EnemySpawn anchors with Tier/Region).
	  * AI runs on a 4 Hz "brain" tick staggered across creatures; movement uses
	    Humanoid:MoveTo with repath only when the goal drifts.
	  * Only a limited number of creatures are alive near players at once
	    (budgeted, distance-culled) so the server stays smooth.
	  * All damage application is server side.
]]

local Players = game:GetService("Players")
local ReplicatedStorage = game:GetService("ReplicatedStorage")
local ServerStorage = game:GetService("ServerStorage")
local Workspace = game:GetService("Workspace")
local PathfindingService = game:GetService("PathfindingService")

local Shared = ReplicatedStorage:WaitForChild("Shared")
local Enemies = require(Shared.EnemyDefinitions)
local EnemyAI = require(Shared.EnemyAI)
local CombatMath = require(Shared.CombatMath)
local Config = require(Shared.Config)
local Util = require(Shared.Util)
local LootRoller = require(Shared.LootRoller)

local EnemyService = {}

local registryRef
local remotes
local anchors = {}
local alive = {}
local rng = Random.new(os.time())
local nextId = 0
local MAX_ALIVE = 42
local THREAT = {} -- [userId] = number

function EnemyService.Init(registry)
	registryRef = registry
	remotes = registry.Remotes
end

-- ---------------------------------------------------------------- modelling
local function limb(parent, name, size, position, color, material)
	local part = Instance.new("Part")
	part.Name = name
	part.Size = size
	part.Position = position
	part.Color = color
	part.Material = material or Enum.Material.Slate
	part.Anchored = false
	part.CanCollide = false
	part.TopSurface = Enum.SurfaceType.Smooth
	part.BottomSurface = Enum.SurfaceType.Smooth
	part.Parent = parent
	return part
end

local function buildModel(def, position)
	local model = Instance.new("Model")
	model.Name = def.name

	local scale = def.scale or 1
	local color = def.color or Color3.fromRGB(110, 105, 95)
	local torso = limb(model, "Torso", Vector3.new(2.2, 2.6, 1.2) * scale, position, color)
	torso.CanCollide = true
	for side = -1, 1, 2 do
		local arm = limb(model, (side < 0 and "Left" or "Right") .. "Arm",
			Vector3.new(0.7, 2.6, 0.7) * scale,
			position + Vector3.new(side * 1.5 * scale, 0.1, 0), color)
		arm.CanCollide = false
		local leg = limb(model, (side < 0 and "Left" or "Right") .. "Leg",
			Vector3.new(0.85, 2.6, 0.85) * scale,
			position + Vector3.new(side * 0.65 * scale, -2.5 * scale, 0), color)
		leg.CanCollide = false
	end
	local head = limb(model, "Head", Vector3.new(1.4, 1.5, 1.4) * scale,
		position + Vector3.new(0, 2.1 * scale, 0), Color3.fromRGB(
			math.max(0, color.R * 255 - 20) / 255,
			math.max(0, color.G * 255 - 20) / 255,
			math.max(0, color.B * 255 - 20) / 255), Enum.Material.Sand)
	head.CanCollide = false

	-- glowing eyes for visibility in the dark
	for side = -1, 1, 2 do
		local eye = Instance.new("Part")
		eye.Name = "Eye"
		eye.Size = Vector3.new(0.18, 0.18, 0.12) * scale
		eye.Position = position + Vector3.new(side * 0.32 * scale, 2.25 * scale, -0.66 * scale)
		eye.Color = (def.id == "pale_one") and Color3.fromRGB(240, 240, 255) or Color3.fromRGB(210, 90, 60)
		eye.Material = Enum.Material.Neon
		eye.CanCollide = false
		eye.Anchored = false
		eye.Parent = model
	end

	local root = Instance.new("Part")
	root.Name = "HumanoidRootPart"
	root.Size = Vector3.new(2, 2.6, 1)
	root.Transparency = 1
	root.CanCollide = false
	root.Anchored = false
	root.CFrame = CFrame.new(position)
	root.Parent = model
	model.PrimaryPart = root

	-- Every limb is welded to the root so the creature stays one assembly; without
	-- this the body parts simply fall apart under gravity.
	for _, part in ipairs(model:GetChildren()) do
		if part:IsA("BasePart") and part ~= root then
			local weld = Instance.new("WeldConstraint")
			weld.Part0 = root
			weld.Part1 = part
			weld.Parent = root
		end
	end


	-- the Humanoid is added last so it always sees a complete rig
	local humanoid = Instance.new("Humanoid")
	humanoid.MaxHealth = def.health
	humanoid.Health = def.health
	humanoid.WalkSpeed = def.speed
	humanoid.JumpPower = 40
	humanoid.Parent = model

	return model, humanoid, root
end

-- ---------------------------------------------------------------- spawning
function EnemyService.Start(registry)
	task.spawn(function()
		local world = Workspace:WaitForChild("World")
		for _, descendant in ipairs(world:GetDescendants()) do
			if descendant.Name == "EnemySpawn" then
				local tier = 1
				local region = "Suburbs"
				local tierValue = descendant:FindFirstChild("Tier")
				if tierValue then tier = tonumber(tierValue.Value) or 1 end
				local regionValue = descendant:FindFirstChild("Region")
				if regionValue then region = tostring(regionValue.Value) end
				table.insert(anchors, { position = descendant.Position, tier = tier, region = region })
			end
		end
		print(string.format("[Enemies] %d spawn anchors registered", #anchors))
	end)

	task.spawn(function()
		-- AI brain tick
		while true do
			task.wait(0.25)
			EnemyService.BrainTick(0.25)
		end
	end)

	task.spawn(function()
		-- spawn manager
		while true do
			task.wait(3)
			EnemyService.SpawnTick()
			EnemyService.CleanupTick()
		end
	end)
end

local function playersNearby(position, radius)
	local out = {}
	for _, player in ipairs(Players:GetPlayers()) do
		local character = player.Character
		local root = character and character:FindFirstChild("HumanoidRootPart")
		if root and (root.Position - position).Magnitude <= radius then
			table.insert(out, { player = player, root = root, character = character })
		end
	end
	return out
end

function EnemyService.SpawnTick()
	local count = 0
	for _ in pairs(alive) do count += 1 end
	if count >= MAX_ALIVE then return end
	-- only spawn near players, up to a per-player budget
	for _, player in ipairs(Players:GetPlayers()) do
		local character = player.Character
		local root = character and character:FindFirstChild("HumanoidRootPart")
		if root then
			local desired = 6
			local nearby = 0
			for _, enemy in pairs(alive) do
				if (enemy.root.Position - root.Position).Magnitude < 220 then
					nearby += 1
				end
			end
			if nearby < desired and #anchors > 0 then
				-- pick a spawn anchor in the same region, 60..200 studs away
				local candidates = {}
				for _, anchor in ipairs(anchors) do
					local distance = (anchor.position - root.Position).Magnitude
					if distance > 55 and distance < 210 then
						table.insert(candidates, anchor)
					end
				end
				if #candidates > 0 then
					local anchor = candidates[rng:NextInteger(1, #candidates)]
					EnemyService.Spawn(anchor, root.Position)
				end
			end
		end
	end
end

function EnemyService.Spawn(anchor, playerPosition)
	local night = registryRef.WeatherService and registryRef.WeatherService.IsNight() or false
	local candidates = EnemyAI.spawnWeights(anchor.region or "Suburbs", anchor.tier or 1, night)
	if #candidates == 0 then
		candidates = EnemyAI.spawnWeights(anchor.region or "Suburbs", 1, night)
	end
	if #candidates == 0 then return nil end
	local pick = Util.weightedPick(candidates, rng)
	local def = pick.def
	if def.nocturnal and not night then return nil end

	-- spawn slightly offset from the anchor so they don't stack
	local offset = Vector3.new(rng:NextNumber(-14, 14), 4, rng:NextNumber(-14, 14))
	local spawnPos = anchor.position + offset
	nextId += 1
	local model, humanoid, root = buildModel(def, spawnPos)
	model.Parent = Workspace
	local memory = EnemyAI.newMemory(anchor.position)
	local entry = {
		id = nextId, def = def, model = model, humanoid = humanoid, root = root,
		memory = memory, anchor = anchor, lastAttack = 0, spawnAt = os.clock(),
		home = anchor.position, target = nil,
	}
	alive[nextId] = entry
	return entry
end

function EnemyService.CleanupTick()
	local now = os.clock()
	for id, enemy in pairs(alive) do
		local dead = not enemy.model.Parent or not enemy.humanoid or enemy.humanoid.Health <= 0
		if dead then
			EnemyService.Remove(id)
		elseif now - enemy.spawnAt > 240 and #playersNearby(enemy.root.Position, 260) == 0 then
			EnemyService.Remove(id, true)
		end
	end
end

function EnemyService.Remove(id, silent)
	local enemy = alive[id]
	if not enemy then return end
	alive[id] = nil
	if not silent and enemy.model then
		local drops = LootRoller.rollEnemy(enemy.def, rng)
		for _, drop in ipairs(drops) do
			registryRef.LootService.CreateWorldDrop(enemy.root.Position + Vector3.new(0, 1, 0), drop.id, drop.count)
		end
	end
	if enemy.model and enemy.model.Parent then
		enemy.model:Destroy()
	end
end

-- ---------------------------------------------------------------- perception
local function perceptionFor(enemy, player, dt)
	local def = enemy.def
	local root = enemy.root
	local targetRoot = player.Character and player.Character:FindFirstChild("HumanoidRootPart")
	if not targetRoot then return nil end
	local toTarget = targetRoot.Position - root.Position
	local distance = toTarget.Magnitude
	local direction = root.CFrame.LookVector

	local params = RaycastParams.new()
	params.FilterType = Enum.RaycastFilterType.Exclude
	params.FilterDescendantsInstances = { enemy.model, player.Character }
	params.IgnoreWater = true
	local hit = Workspace:Raycast(root.Position + Vector3.new(0, 1.5, 0), toTarget.Unit * math.min(distance, 200), params)

	local survival = registryRef.PlayerDataService.Get(player)
	local night = registryRef.WeatherService and registryRef.WeatherService.IsNight() or false
	local weather = registryRef.WeatherService and registryRef.WeatherService.GetState() or {}

	return {
		dt = dt,
		position = root.Position,
		direction = direction,
		target = {
			id = player.UserId,
			position = targetRoot.Position,
			isSprinting = false,
			isCrouching = false,
			lightOn = survival and survival.settings and survival.settings.flashlight == true or false,
			player = player,
		},
		hasLineOfSight = hit == nil,
		distance = distance,
		owners = #playersNearby(root.Position, 60),
		health = enemy.humanoid.Health / math.max(1, enemy.def.health),
		night = night,
		foggy = weather.foggy == true,
		noiseEvents = enemy._noiseEvents or {},
		time = os.clock(),
	}
end

function EnemyService.EmitNoise(position, loudness, source)
	for _, enemy in pairs(alive) do
		if (enemy.root.Position - position).Magnitude < loudness * 1.6 then
			enemy._noiseEvents = enemy._noiseEvents or {}
			table.insert(enemy._noiseEvents, { position = position, loudness = loudness, at = os.clock() })
		end
	end
	-- propagates to nearby players as an audio cue if requested
	if source then
		remotes.FireNear(position, loudness * 0.5, "EnemyAlert", {
			kind = "heard", position = position, loudness = loudness,
		})
	end
end

function EnemyService.BrainTick(dt)
	local active = 0
	for id, enemy in pairs(alive) do
		local root = enemy.root
		if not root or not root.Parent then
			alive[id] = nil
		else
			-- stagger: only enemies near a player think every tick
			local near, nearestPlayer, nearestDistance = playersNearby(root.Position, 200), nil, math.huge
			for _, entry in ipairs(near) do
				local d = (entry.root.Position - root.Position).Magnitude
				if d < nearestDistance then
					nearestPlayer, nearestDistance = entry, d
				end
			end
			if nearestPlayer then
				active += 1
				enemy._noiseEvents = enemy._noiseEvents or {}
				-- decay noise events
				local kept = {}
				for _, evt in ipairs(enemy._noiseEvents) do
					if os.clock() - evt.at < 3 then
						table.insert(kept, evt)
					end
				end
				enemy._noiseEvents = kept

				local perception = perceptionFor(enemy, nearestPlayer.player, dt)
				if perception then
					local action = EnemyAI.update(enemy.def, enemy.memory, perception)
					EnemyService.ApplyAction(enemy, action, perception, nearestPlayer)
				end
				-- threat bookkeeping for the survival service
				if enemy.memory.state == "Chase" or enemy.memory.state == "Attack" then
					THREAT[nearestPlayer.player.UserId] = 1
				end
			end
		end
	end
end

--[[
	PathfindingService is used sparingly: only when a chasing creature's direct
	line to its target is blocked, and never more than once every two seconds per
	creature. The resulting waypoints are followed with Humanoid:MoveTo, which
	keeps the per-tick cost near zero when nothing is in the way.
]]
local function followPath(enemy, goal)
	local now = os.clock()
	local params = RaycastParams.new()
	params.FilterType = Enum.RaycastFilterType.Exclude
	params.FilterDescendantsInstances = { enemy.model }
	local toGoal = goal - enemy.root.Position
	local blocked = Workspace:Raycast(enemy.root.Position + Vector3.new(0, 2, 0), toGoal, params) ~= nil
	if not blocked then
		enemy.waypoints = nil
		return goal
	end
	if not enemy.waypoints or now - (enemy.pathAt or 0) > 2 then
		enemy.pathAt = now
		local ok, waypoints = pcall(function()
			local path = PathfindingService:CreatePath({
				AgentRadius = 2, AgentHeight = 5, AgentCanJump = true, AgentMaxSlope = 55,
			})
			path:ComputeAsync(enemy.root.Position, goal)
			if path.Status ~= Enum.PathStatus.Success then
				return nil
			end
			return path:GetWaypoints()
		end)
		enemy.waypoints = (ok and waypoints and #waypoints > 1) and waypoints or nil
		enemy.waypointIndex = 2
	end
	local waypoints = enemy.waypoints
	if not waypoints then
		return goal
	end
	local index = enemy.waypointIndex or 2
	local waypoint = waypoints[index]
	while waypoint and (waypoint.Position - enemy.root.Position).Magnitude < 4 do
		index += 1
		waypoint = waypoints[index]
	end
	enemy.waypointIndex = index
	if not waypoint then
		enemy.waypoints = nil
		return goal
	end
	if waypoint.Action == Enum.PathWaypointAction.Jump then
		enemy.humanoid.Jump = true
	end
	return waypoint.Position
end

function EnemyService.ApplyAction(enemy, action, perception, nearest)
	local humanoid = enemy.humanoid
	if not humanoid or humanoid.Health <= 0 then return end
	local def = enemy.def

	if action.attack == "melee" and perception.target then
		local now = os.clock()
		if now - enemy.lastAttack >= def.attackRate then
			local distance = (enemy.root.Position - perception.target.position).Magnitude
			if distance <= def.attackRange + 2 then
				enemy.lastAttack = now
				local targetPlayer = perception.target.player
				local damage = def.damage
				if registryRef.WeatherService and registryRef.WeatherService.IsNight() then
					damage *= 1.08
				end
				registryRef.CombatService.DamagePlayer(targetPlayer, damage, {
					part = (def.behaviour.limbTarget == "Legs") and "LeftLeg" or "Torso",
					bleeding = 0.35,
					infection = 0.12,
					cause = "Killed by " .. def.name,
				})
				remotes.Fire(targetPlayer, "CombatFeedback", "attacked", {
					enemy = def.id, damage = damage, position = enemy.root.Position,
				})
				if def.behaviour.knockback then
					local root = targetPlayer.Character and targetPlayer.Character:FindFirstChild("HumanoidRootPart")
					if root then
						root.AssemblyLinearVelocity = (root.Position - enemy.root.Position).Unit * def.behaviour.knockback
							+ Vector3.new(0, 18, 0)
					end
				end
			end
		end
	elseif action.moveTo then
		local goal = action.moveTo
		if enemy.memory.state == "Chase" then
			goal = followPath(enemy, goal)
		else
			enemy.waypoints = nil
		end
		if not enemy.lastGoal or (enemy.lastGoal - goal).Magnitude > 12 then
			enemy.lastGoal = goal
			humanoid:MoveTo(goal)
		end
		if action.sprint then
			humanoid.WalkSpeed = def.speed * 1.25
		elseif action.flee then
			humanoid.WalkSpeed = def.speed * 1.4
		else
			humanoid.WalkSpeed = def.speed
		end
	end

	if action.sound then
		remotes.FireNear(enemy.root.Position, 180, "EnemyAlert", {
			kind = "vocal", enemy = def.id, sound = action.sound, position = enemy.root.Position,
		})
		if def.behaviour.stressAura then
			for _, player in ipairs(Players:GetPlayers()) do
				local character = player.Character
				local root = character and character:FindFirstChild("HumanoidRootPart")
				if root and (root.Position - enemy.root.Position).Magnitude < 70 then
					local profile = registryRef.PlayerDataService.Get(player)
					if profile then
						profile.survival.stress = math.min(100, profile.survival.stress + def.behaviour.stressAura)
					end
				end
			end
		end
	end

	if action.callHorde then
		-- pull in nearby creatures
		for id, other in pairs(alive) do
			if other ~= enemy and (other.root.Position - enemy.root.Position).Magnitude < (def.behaviour.callRadius or 200) then
				other.memory.alert = math.min(1, (other.memory.alert or 0) + 0.75)
				other.memory.investigatePoint = enemy.root.Position
				other.memory.lastSeen = enemy.root.Position
				other.memory.lastSeenAt = os.clock()
			end
		end
		remotes.FireNear(enemy.root.Position, 260, "EnemyAlert", {
			kind = "scream", position = enemy.root.Position,
		})
	end
end

-- Damage coming from a player's weapon
function EnemyService.DamageAt(part, damage, attacker, bodyPart, def)
	local model = part and part:FindFirstAncestorOfClass("Model")
	if not model then return false end
	local entry
	for id, enemy in pairs(alive) do
		if enemy.model == model then
			entry = enemy
			break
		end
	end
	if not entry then return false end
	local enemyDef = entry.def
	local final = damage
	if bodyPart == "Head" then
		final *= 1.6
	end
	if enemyDef.behaviour and enemyDef.behaviour.armored then
		final *= (1 - enemyDef.behaviour.armored)
	end
	for _, weakness in ipairs(enemyDef.weakness or {}) do
		if def and def.tags then
			for _, tag in ipairs(def.tags) do
				if tag == weakness then
					final *= 1.35
					break
				end
			end
		end
	end
	entry.humanoid.Health = entry.humanoid.Health - final
	entry.memory.alert = 1
	entry.memory.lastSeen = attacker.Character and attacker.Character.HumanoidRootPart
		and attacker.Character.HumanoidRootPart.Position or entry.root.Position
	entry.memory.lastSeenAt = os.clock()
	entry.memory.target = attacker
	if entry.humanoid.Health <= 0 then
		registryRef.LootService.CreateWorldDrop(entry.root.Position + Vector3.new(0, 1, 0),
			"mutant_gland", 1)
		if attacker then
			local profile = registryRef.PlayerDataService.Get(attacker)
			if profile then
				profile.stats.kills = (profile.stats.kills or 0) + 1
			end
			if registryRef.QuestService then
				registryRef.QuestService.Event(attacker, { type = "kill", id = enemyDef.id })
			end
			remotes.Fire(attacker, "Notify", enemyDef.name .. " is down.", "combat", 2.4)
		end
		EnemyService.Remove(entry.id)
	end
	return final, entry.humanoid.Health <= 0
end

function EnemyService.GetThreat(player)
	return THREAT[player.UserId] or 0
end

function EnemyService.ResetThreat(player)
	THREAT[player.UserId] = 0
end

function EnemyService.Count()
	local n = 0
	for _ in pairs(alive) do n += 1 end
	return n
end

function EnemyService.Stats()
	local states = {}
	for _, enemy in pairs(alive) do
		states[enemy.memory.state] = (states[enemy.memory.state] or 0) + 1
	end
	return states
end

return EnemyService
