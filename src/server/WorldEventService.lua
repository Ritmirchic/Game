--!strict
--[[
	WorldEventService — dynamic events + environmental props driven at runtime:
	  * supply drops (with a flare visible from far away)
	  * mutant migrations
	  * radiation storms
	  * power flickers
	  * mysterious transmissions
	  * nighttime shelter raids
	  * campfire / light registry (used by survival heat)
]]

local Players = game:GetService("Players")
local ReplicatedStorage = game:GetService("ReplicatedStorage")
local Workspace = game:GetService("Workspace")
local Lighting = game:GetService("Lighting")

local Shared = ReplicatedStorage:WaitForChild("Shared")
local Config = require(Shared.Config)
local Util = require(Shared.Util)
local LootRoller = require(Shared.LootRoller)
local Items = require(Shared.ItemDefinitions)
local Story = require(Shared.StoryData)

local WorldEventService = {}

local registryRef
local remotes
local rng = Random.new(os.time())
local fires = {}
local lightProps = {}
local nextEventAt = os.clock() + 60
local activeEvent = nil
local eventEndsAt = 0

function WorldEventService.Init(registry)
	registryRef = registry
	remotes = registry.Remotes
end

function WorldEventService.Start(registry)
	task.spawn(function()
		while true do
			task.wait(5)
			local now = os.clock()
			if activeEvent and now > eventEndsAt then
				WorldEventService.EndEvent()
			elseif not activeEvent and now > nextEventAt then
				WorldEventService.StartRandomEvent()
			end
		end
	end)
end

function WorldEventService.StartRandomEvent()
	local night = registryRef.WeatherService and registryRef.WeatherService.IsNight()
	local pool = {
		{ kind = "supply_drop", weight = 2 },
		{ kind = "migration", weight = 2 },
		{ kind = "radiation_storm", weight = 1 },
		{ kind = "flicker", weight = 2.5 },
		{ kind = "transmission", weight = 2 },
		{ kind = "sighting", weight = 2 },
	}
	if night then
		table.insert(pool, { kind = "shelter_raid", weight = 2.2 })
	end
	local pick = Util.weightedPick(pool, rng)
	WorldEventService.StartEvent(pick.kind)
end

function WorldEventService.StartEvent(kind)
	local now = os.clock()
	activeEvent = kind
	if kind == "supply_drop" then
		local drop = WorldEventService.SpawnSupplyDrop()
		eventEndsAt = now + 240
		if drop then
			remotes.FireAll("StoryEvent", "supply_drop", { position = drop.Position })
		end
	elseif kind == "migration" then
		eventEndsAt = now + 120
		WorldEventService.MigrateMutants()
	elseif kind == "radiation_storm" then
		eventEndsAt = now + 90
		if registryRef.WeatherService then
			registryRef.WeatherService.ForceWeather("Radiation")
		end
		remotes.FireAll("StoryEvent", "radiation_storm", {})
		remotes.FireAll("Notify", "Radiation storm incoming. Find cover.", "danger", 6)
	elseif kind == "flicker" then
		eventEndsAt = now + 14
		WorldEventService.Flicker()
	elseif kind == "transmission" then
		eventEndsAt = now + 20
		local line = Story.Phenomena[rng:NextInteger(1, #Story.Phenomena)]
		remotes.FireAll("StoryEvent", "transmission", { text = line.text, stress = line.stress })
	elseif kind == "sighting" then
		eventEndsAt = now + 26
		WorldEventService.Sighting()
	elseif kind == "shelter_raid" then
		eventEndsAt = now + 180
		WorldEventService.ShelterRaid()
	end
end

function WorldEventService.EndEvent()
	activeEvent = nil
	nextEventAt = os.clock() + rng:NextInteger(90, 220)
	if registryRef.WeatherService and registryRef.WeatherService.GetState().kind == "Radiation" then
		registryRef.WeatherService.ForceWeather("Overcast")
	end
end

function WorldEventService.SpawnSupplyDrop()
	local players = Players:GetPlayers()
	if #players == 0 then return nil end
	local player = players[rng:NextInteger(1, #players)]
	local character = player.Character
	local root = character and character:FindFirstChild("HumanoidRootPart")
	if not root then return nil end
	local angle = rng:NextNumber(0, math.pi * 2)
	local distance = rng:NextNumber(80, 180)
	local position = root.Position + Vector3.new(math.cos(angle) * distance, 0, math.sin(angle) * distance)

	local params = RaycastParams.new()
	params.FilterDescendantsInstances = { character }
	local hit = Workspace:Raycast(position + Vector3.new(0, 200, 0), Vector3.new(0, -400, 0), params)
	if hit then
		position = hit.Position + Vector3.new(0, 3, 0)
	end

	local container = Instance.new("Part")
	container.Name = "SupplyCrate"
	container.Size = Vector3.new(6, 5, 6)
	container.Material = Enum.Material.Metal
	container.Color = Color3.fromRGB(86, 94, 70)
	container.Anchored = true
	container.CFrame = CFrame.new(position)
	container.Parent = Workspace

	local kind = Instance.new("StringValue")
	kind.Name = "Kind"
	kind.Value = "SupplyCrate"
	kind.Parent = container
	local contents = Instance.new("StringValue")
	contents.Name = "LootTable"
	contents.Value = "military_high"
	contents.Parent = container

	local flare = Instance.new("PointLight")
	flare.Brightness = 8
	flare.Range = 220
	flare.Color = Color3.fromRGB(255, 90, 60)
	flare.Parent = container
	remotes.FireAll("Notify", "A supply drop is burning on the horizon.", "quest", 6)
	return container
end

function WorldEventService.MigrateMutants()
	for _, player in ipairs(Players:GetPlayers()) do
		local character = player.Character
		local root = character and character:FindFirstChild("HumanoidRootPart")
		if root and registryRef.EnemyService then
			registryRef.EnemyService.EmitNoise(root.Position, 400, nil)
			remotes.Fire(player, "Notify", "Something is moving through the region in numbers.", "danger", 5)
		end
	end
end

function WorldEventService.Flicker()
	local lighting = Lighting
	local original = lighting.Brightness
	task.spawn(function()
		for i = 1, 8 do
			lighting.Brightness = original * (i % 2 == 0 and 1 or 0.25)
			task.wait(0.35)
		end
		lighting.Brightness = original
	end)
	remotes.FireAll("StoryEvent", "power_flicker", {})
end

function WorldEventService.Sighting()
	for _, player in ipairs(Players:GetPlayers()) do
		local character = player.Character
		local root = character and character:FindFirstChild("HumanoidRootPart")
		if root then
			local angle = rng:NextNumber(0, math.pi * 2)
			local position = root.Position + Vector3.new(math.cos(angle) * 60, 0, math.sin(angle) * 60)
			remotes.Fire(player, "StoryEvent", "sighting", { position = position })
			local profile = registryRef.PlayerDataService.Get(player)
			if profile then
				profile.survival.stress = math.min(100, profile.survival.stress + 6)
			end
		end
	end
end

function WorldEventService.ShelterRaid()
	for _, player in ipairs(Players:GetPlayers()) do
		local profile = registryRef.PlayerDataService.Get(player)
		if profile then
			remotes.Fire(player, "Notify", "They know where you sleep tonight.", "danger", 6)
		end
	end
	if registryRef.EnemyService then
		for _, player in ipairs(Players:GetPlayers()) do
			local character = player.Character
			local root = character and character:FindFirstChild("HumanoidRootPart")
			if root then
				registryRef.EnemyService.EmitNoise(root.Position, 300, nil)
			end
		end
	end
end

function WorldEventService.RegisterFire(position)
	table.insert(fires, { position = position, addedAt = os.clock() })
end

function WorldEventService.FiresNear(position, radius)
	local count = 0
	for index = #fires, 1, -1 do
		local fire = fires[index]
		if os.clock() - fire.addedAt > 900 then
			table.remove(fires, index)
		elseif (fire.position - position).Magnitude <= radius then
			count += 1
		end
	end
	return count
end

function WorldEventService.DropLight(player, def)
	local character = player.Character
	local root = character and character:FindFirstChild("HumanoidRootPart")
	if not root then return end
	local light = Instance.new("Part")
	light.Name = "DroppedLight"
	light.Size = Vector3.new(0.6, 0.6, 2.2)
	light.Anchored = true
	light.CanCollide = false
	light.Material = Enum.Material.Neon
	light.Color = def.use.color or Color3.fromRGB(255, 140, 80)
	light.CFrame = CFrame.new(root.Position + root.CFrame.LookVector * 4 + Vector3.new(0, -2, 0))
	light.Parent = Workspace
	local pointLight = Instance.new("PointLight")
	pointLight.Brightness = 2.4
	pointLight.Range = 26
	pointLight.Color = light.Color
	pointLight.Parent = light
	table.insert(lightProps, { part = light, expiresAt = os.clock() + (def.id == "flare" and 90 or 300) })

	local kind = Instance.new("StringValue")
	kind.Name = "Kind"
	kind.Value = "Marker"
	kind.Parent = light
	table.insert(fires, { position = light.Position, addedAt = os.clock() })
	task.delay(def.id == "flare" and 90 or 300, function()
		if light.Parent then light:Destroy() end
	end)
end

function WorldEventService.ActiveEvent()
	return activeEvent
end

return WorldEventService
