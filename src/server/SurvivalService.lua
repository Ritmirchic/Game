--!strict
--[[
	SurvivalService — ticks survival state for every player, applies weather and
	environment context, handles bleeding, temperature, radiation, stress, death
	and respawn.
]]

local Players = game:GetService("Players")
local RunService = game:GetService("RunService")
local ReplicatedStorage = game:GetService("ReplicatedStorage")
local Workspace = game:GetService("Workspace")

local Shared = ReplicatedStorage:WaitForChild("Shared")
local Config = require(Shared.Config)
local SurvivalMath = require(Shared.SurvivalMath)
local Util = require(Shared.Util)
local CombatMath = require(Shared.CombatMath)
local Items = require(Shared.ItemDefinitions)

local SurvivalService = {}

local registryRef
local remotes
local accumulator = 0
local activity = {}   -- [userId] = { sprint, crouch, moving, indoors, aim }
local shelters = {}   -- array of { position, radius }

function SurvivalService.Init(registry)
	registryRef = registry
	remotes = registry.Remotes
end

function SurvivalService.Start(registry)
	-- collect shelter volumes from anchors tagged Safehouse
	task.spawn(function()
		local world = Workspace:FindFirstChild("World")
		if not world then return end
		for _, descendant in ipairs(world:GetDescendants()) do
			if descendant.Name == "SafehouseAnchor" then
				table.insert(shelters, { position = descendant.Position, radius = 48 })
			end
		end
	end)

	remotes.On("RequestMove", function(player, state)
		if type(state) ~= "table" then return end
		local entry = activity[player.UserId] or {}
		entry.sprint = state.sprint == true
		entry.crouch = state.crouch == true
		entry.crawl = state.crawl == true
		entry.aim = state.aim == true
		entry.moving = state.moving == true
		entry.climb = state.climb == true
		activity[player.UserId] = entry
	end)

	Players.PlayerRemoving:Connect(function(player)
		activity[player.UserId] = nil
	end)

	RunService.Heartbeat:Connect(function(dt)
		accumulator += dt
		if accumulator < Config.Survival.TickRate then return end
		local step = accumulator
		accumulator = 0
		SurvivalService.Tick(step)
	end)
end

-- Region centers mirror the baked world (tools/worldgen.py REGIONS).
local REGION_CENTERS = {
	Suburbs = { x = 0, z = 0 },
	Forest = { x = -1150, z = 120 },
	Industrial = { x = 1050, z = -180 },
	City = { x = -120, z = -1180 },
	Military = { x = 1150, z = 1050 },
	Wasteland = { x = -1200, z = -1150 },
}

function SurvivalService.GetRegion(position)
	local best, bestDist = "Suburbs", math.huge
	for name, center in pairs(REGION_CENTERS) do
		local dx = position.X - center.x
		local dz = position.Z - center.z
		local d = dx * dx + dz * dz
		if d < bestDist then
			best, bestDist = name, d
		end
	end
	return best, math.sqrt(bestDist)
end

function SurvivalService.IsIndoors(position)
	for _, shelter in ipairs(shelters) do
		local d = (shelter.position - position).Magnitude
		if d <= shelter.radius then
			return true
		end
	end
	return false
end

function SurvivalService.Tick(dt)
	local players = Players:GetPlayers()
	local partySize = #players
	for _, player in ipairs(players) do
		local profile = registryRef.PlayerDataService.Get(player)
		if profile then
			SurvivalService.StepPlayer(player, profile, dt, partySize)
		end
	end
end

function SurvivalService.StepPlayer(player, profile, dt, partySize)
	local character = player.Character
	local root = character and character:FindFirstChild("HumanoidRootPart")
	local humanoid = character and character:FindFirstChildOfClass("Humanoid")
	local survival = profile.survival
	if not survival.alive then
		return
	end

	local position = root and root.Position or Vector3.new(0, 0, 0)
	local region = SurvivalService.GetRegion(position)
	local regionInfo = Config.Regions[region] or Config.Regions.Suburbs
	local act = activity[player.UserId] or {}
	local weather = registryRef.WeatherService and registryRef.WeatherService.GetState() or { temperature = 10, foggy = false, storm = false }
	local armor = CombatMath.aggregateArmor(profile.inventory.equipment, Items)

	local ctx = {
		ambientTemperature = (regionInfo.temp or 10) + (weather.temperatureOffset or 0),
		regionRadiation = (regionInfo.radiation or 0) + (humanoid and 0 or 0),
		sprinting = act.sprint == true,
		crouching = act.crouch == true,
		moving = act.moving == true,
		indoors = SurvivalService.IsIndoors(position),
		isNight = registryRef.WeatherService and registryRef.WeatherService.IsNight() or false,
		threat = registryRef.EnemyService and registryRef.EnemyService.GetThreat(player) or 0,
		alone = partySize <= 1,
		storm = weather.storm == true,
		gear = armor,
		fires = registryRef.WorldEventService and registryRef.WorldEventService.FiresNear(position, 30) or 0,
		activity = act.sprint and "sprint" or (act.climb and "climb") or (act.crouch and "crouch")
			or (act.moving and "walk") or "idle",
	}

	local events = SurvivalMath.step(survival, ctx, dt)

	-- keep the Roblox humanoid in sync
	if humanoid then
		local speedMul = SurvivalMath.speedMultiplier(survival, profile._effects)
		local base = act.sprint and Config.Movement.SprintSpeed or Config.Movement.WalkSpeed
		if act.crouch then base = Config.Movement.CrouchSpeed end
		local targetSpeed = base * speedMul
		if math.abs(humanoid.WalkSpeed - targetSpeed) > 0.05 then
			humanoid.WalkSpeed = targetSpeed
		end
		if humanoid.Health > survival.health + 0.05 or humanoid.Health < survival.health - 0.05 then
			humanoid.Health = math.max(survival.health, 0)
		end
		if humanoid.Health <= 0 and survival.alive then
			survival.alive = false
			survival.causeOfDeath = SurvivalMath.describeDeath(survival)
			SurvivalService.OnDeath(player, profile, survival.causeOfDeath)
		end
	end

	-- passive effects from consumables expire
	if profile._effects then
		for key, remaining in pairs(profile._effects) do
			profile._effects[key] = remaining - dt
			if profile._effects[key] <= 0 then
				profile._effects[key] = nil
			end
		end
	end

	for _, event in ipairs(events) do
		if event == "death" then
			SurvivalService.OnDeath(player, profile, survival.causeOfDeath)
		elseif event == "bloodDrop" then
			remotes.Fire(player, "CombatFeedback", "blood", { amount = survival.bleeding })
		elseif event == "panicBreath" then
			remotes.Fire(player, "CombatFeedback", "breath", { stress = survival.stress })
		elseif event == "radNausea" then
			remotes.Fire(player, "Notify", "Your gums are bleeding. Radiation sickness.", "danger", 4)
		elseif event == "stomachGrowl" then
			remotes.Fire(player, "CombatFeedback", "stomach", {})
		elseif event == "dryMouth" then
			remotes.Fire(player, "CombatFeedback", "thirst", {})
		end
	end

	SurvivalService.Push(player, profile, region)
	if survival.health <= 0 then
		SurvivalService.OnDeath(player, profile, survival.causeOfDeath)
	end
end

function SurvivalService.Push(player, profile, region)
	if not remotes then return end
	local survival = profile.survival
	remotes.Fire(player, "SurvivalUpdate", {
		health = survival.health,
		hunger = survival.hunger,
		thirst = survival.thirst,
		stamina = survival.stamina,
		fatigue = survival.fatigue,
		temperature = survival.temperature,
		radiation = survival.radiation,
		infection = survival.infection,
		bleeding = survival.bleeding,
		stress = survival.stress,
		bodyParts = survival.bodyParts,
		conditions = survival.conditions,
		region = region or "Suburbs",
		alive = survival.alive,
	})
end

function SurvivalService.ApplyDamage(player, amount, part, opts)
	local profile = registryRef.PlayerDataService.Get(player)
	if not profile or not profile.survival.alive then return 0 end
	local survival = profile.survival
	local armor = CombatMath.aggregateArmor(profile.inventory.equipment, Items)
	opts = opts or {}
	opts.armor = opts.armor or armor
	local damage = SurvivalMath.applyHit(survival, part, amount, opts)
	if survival.health <= 0 and survival.alive then
		survival.alive = false
		survival.causeOfDeath = SurvivalMath.describeDeath(survival)
		SurvivalService.OnDeath(player, profile, survival.causeOfDeath)
	end
	SurvivalService.Push(player, profile)
	remotes.Fire(player, "CombatFeedback", "hurt", {
		amount = damage, part = part, health = survival.health,
	})
	return damage
end

function SurvivalService.Heal(player, amount)
	local profile = registryRef.PlayerDataService.Get(player)
	if not profile then return end
	profile.survival.health = math.min(Config.Survival.MaxHealth, profile.survival.health + amount)
	SurvivalService.Push(player, profile)
end

function SurvivalService.OnDeath(player, profile, cause)
	local survival = profile.survival
	survival.alive = false
	profile.stats.deaths = (profile.stats.deaths or 0) + 1
	remotes.Fire(player, "DeathScreen", cause or "Exposure")
	if registryRef.PartyService then
		registryRef.PartyService.NotifyDowned(player)
	end
	if registryRef.PlayerDataService then
		registryRef.PlayerDataService.MarkDirty(player)
	end
	-- schedule respawn
	task.delay(9, function()
		if player.Parent then
			SurvivalService.Respawn(player)
		end
	end)
end

function SurvivalService.Respawn(player)
	local profile = registryRef.PlayerDataService.Get(player)
	if not profile then return end
	local survival = profile.survival
	local fresh = SurvivalMath.newState()
	fresh.hunger = math.max(survival.hunger, 35)
	fresh.thirst = math.max(survival.thirst, 35)
	fresh.stress = math.min(survival.stress, 40)
	fresh.health = Config.Survival.MaxHealth * 0.5
	fresh.radiation = math.min(survival.radiation, 25)
	profile.survival = fresh
	profile._effects = {}
	survival.alive = true
	local spawn = Workspace:FindFirstChildOfClass("SpawnLocation")
	if spawn then
		player:LoadCharacter()
	end
	if registryRef.DirectorService then
		registryRef.DirectorService.OnRespawn(player, profile)
	end
	remotes.Fire(player, "Notify", "You wake up in the shelter. Everything hurts.", "info", 5)
end

function SurvivalService.SetClimbing(player, value)
	local act = activity[player.UserId] or {}
	act.climb = value == true
	activity[player.UserId] = act
end

function SurvivalService.GetContext(player, profile)
	local character = player.Character
	local root = character and character:FindFirstChild("HumanoidRootPart")
	local position = root and root.Position or Vector3.new(0, 0, 0)
	local region = SurvivalService.GetRegion(position)
	return {
		region = region,
		regionInfo = Config.Regions[region],
		position = position,
		indoors = SurvivalService.IsIndoors(position),
	}
end

return SurvivalService
