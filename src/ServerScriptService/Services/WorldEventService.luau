--!strict
--[[
    ASHES OF HUMANITY: LAST SIGNAL
    ServerScriptService.Services.WorldEventService
    Dynamic Random World Events: Emergency Broadcasts, Supply Drops,
    Mutant Migrations, Power Surges, Radiation Fronts, and Distress Signals (Section 32).
]]

local Players = game:GetService("Players")
local ReplicatedStorage = game:GetService("ReplicatedStorage")
local Workspace = game:GetService("Workspace")

local Config = require(ReplicatedStorage.Shared.Config)
local NetworkSchema = require(ReplicatedStorage.Shared.NetworkSchema)
local UtilityModules = require(ReplicatedStorage.Shared.UtilityModules)

local WorldEventService = {}

local registry: any = nil
local rng = Random.new()
local activeEventModels: { Instance } = {}

local EVENT_POOL: { UtilityModules.WeightedEntry<string> } = {
	{ Weight = 22, Value = "EmergencyBroadcast" },
	{ Weight = 18, Value = "SupplyDrop" },
	{ Weight = 18, Value = "MutantMigration" },
	{ Weight = 15, Value = "PowerSurge" },
	{ Weight = 12, Value = "RadiationStormEvent" },
	{ Weight = 15, Value = "DistressSignal" },
}

local DROP_REGIONS = {
	"DeadSuburbs",
	"BlackwoodForest",
	"IndustrialZone",
	"ForgottenCity",
	"MilitaryExclusionZone",
}

function WorldEventService:Init(serviceRegistry: any)
	registry = serviceRegistry
end

function WorldEventService:TriggerEvent(eventType: string)
	if eventType == "EmergencyBroadcast" then
		registry.BroadcastAll(NetworkSchema.EVENTS.RadioTransmission, {
			Frequency = "142.8 MHz // EBS",
			Title = "EMERGENCY BROADCAST INTERCEPT",
			Text = "AUTOMATED CIVIL DEFENSE ALERT: Bio-acoustic herd movement detected along Route 9. Maintain light discipline after 20:00 hours.",
		})
		registry.BroadcastAll(NetworkSchema.EVENTS.WorldEventAlert, {
			EventId = "EmergencyBroadcast",
			Title = "EMERGENCY BROADCAST",
			Description = "142.8 MHz Civil Defense repeater transmitted an automated sector advisory.",
		})
	elseif eventType == "SupplyDrop" then
		local rId = DROP_REGIONS[rng:NextInteger(1, #DROP_REGIONS)]
		local reg = (Config.REGIONS :: any)[rId]
		local dropPos = reg.Center + Vector3.new(rng:NextNumber(-55, 55), 2.2, rng:NextNumber(-55, 55))

		local crate = Instance.new("Part")
		crate.Name = "AirdropCrate_" .. UtilityModules.GenerateUID("drop")
		crate.Size = Vector3.new(4.2, 2.8, 3.2)
		crate.Position = dropPos
		crate.Anchored = true
		crate.Material = Enum.Material.Metal
		crate.Color = Color3.fromRGB(72, 86, 68)
		crate:SetAttribute("IsLootContainer", true)
		crate:SetAttribute("ContainerId", crate.Name)
		crate:SetAttribute("LootTable", "Military")

		local flareLight = Instance.new("PointLight")
		flareLight.Color = Color3.fromRGB(255, 92, 38)
		flareLight.Brightness = 3.0
		flareLight.Range = 36
		flareLight.Parent = crate

		local smoke = Instance.new("ParticleEmitter")
		smoke.Color = ColorSequence.new(Color3.fromRGB(235, 96, 42))
		smoke.Size = NumberSequence.new({ NumberSequenceKeypoint.new(0, 1.2), NumberSequenceKeypoint.new(1, 5.5) })
		smoke.Lifetime = NumberRange.new(4, 7)
		smoke.Rate = 14
		smoke.Speed = NumberRange.new(5, 9)
		smoke.Parent = crate

		local folder = Workspace:FindFirstChild("InteractionObjects") or Workspace
		crate.Parent = folder
		table.insert(activeEventModels, crate)

		if registry.LootService then
			registry.LootService:RegisterContainer(crate)
		end

		registry.BroadcastAll(NetworkSchema.EVENTS.WorldEventAlert, {
			EventId = "SupplyDrop",
			Title = "UNMANNED SUPPLY DROP",
			Description = "An automated UN-CBRN cargo pod landed in "
				.. reg.DisplayName
				.. " (marked with orange signal smoke).",
			Position = dropPos,
		})
	elseif eventType == "MutantMigration" then
		local rId = DROP_REGIONS[rng:NextInteger(1, #DROP_REGIONS)]
		local reg = (Config.REGIONS :: any)[rId]
		if registry.EnemyAIService then
			for i = 1, 3 do
				local spawnPos = reg.Center + Vector3.new(i * 8, 3.5, -i * 6)
				registry.EnemyAIService:SpawnMutant(if i == 1 then "Screamer" else "Hollow", spawnPos)
			end
		end
		registry.BroadcastAll(NetworkSchema.EVENTS.WorldEventAlert, {
			EventId = "MutantMigration",
			Title = "MUTANT PACK MIGRATION",
			Description = "A roaming pack of Hollows led by a Screamer was sighted crossing " .. reg.DisplayName .. "!",
			Position = reg.Center,
		})
	elseif eventType == "PowerSurge" then
		Workspace:SetAttribute("GridPowerSurge", true)
		registry.BroadcastAll(NetworkSchema.EVENTS.WorldEventAlert, {
			EventId = "PowerSurge",
			Title = "SUBSTATION POWER SURGE",
			Description = "Residual hydroelectric current surged through Industrial Sector transformers — auxiliary lights online for 90s.",
		})
		task.delay(90, function()
			Workspace:SetAttribute("GridPowerSurge", false)
		end)
	elseif eventType == "RadiationStormEvent" then
		if registry.WeatherService then
			registry.WeatherService:SetWeather("RadiationStorm", 110)
		end
	elseif eventType == "DistressSignal" then
		local reg = Config.REGIONS.BlackwoodForest
		local flarePos = reg.Center + Vector3.new(rng:NextNumber(-40, 40), 2.0, rng:NextNumber(-40, 40))

		local pack = Instance.new("Part")
		pack.Name = "DistressMedCache_" .. UtilityModules.GenerateUID("med")
		pack.Size = Vector3.new(2.8, 1.6, 2.2)
		pack.Position = flarePos
		pack.Anchored = true
		pack.Material = Enum.Material.Fabric
		pack.Color = Color3.fromRGB(148, 48, 42)
		pack:SetAttribute("IsLootContainer", true)
		pack:SetAttribute("ContainerId", pack.Name)
		pack:SetAttribute("LootTable", "Hospital")
		pack.Parent = Workspace:FindFirstChild("InteractionObjects") or Workspace
		table.insert(activeEventModels, pack)

		if registry.LootService then
			registry.LootService:RegisterContainer(pack)
		end
		if registry.EnemyAIService then
			registry.EnemyAIService:SpawnMutant("Mimic", flarePos + Vector3.new(12, 1.5, -10))
		end

		registry.BroadcastAll(NetworkSchema.EVENTS.WorldEventAlert, {
			EventId = "DistressSignal",
			Title = "SURVIVOR DISTRESS FLARE",
			Description = "A red distress flare ignited inside Blackwood Forest — beware of Mimic ambushes near the cache!",
			Position = flarePos,
		})
	end
end

function WorldEventService:Start()
	task.spawn(function()
		while true do
			task.wait(rng:NextNumber(165, 240))
			if #Players:GetPlayers() > 0 then
				-- Prune old event crates beyond 8 to avoid clutter
				while #activeEventModels > 8 do
					local oldest = table.remove(activeEventModels, 1)
					if oldest and oldest.Parent then
						oldest:Destroy()
					end
				end
				local picked = UtilityModules.PickWeighted(EVENT_POOL, rng) or "EmergencyBroadcast"
				self:TriggerEvent(picked)
			end
		end
	end)
end

return WorldEventService
