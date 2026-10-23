--!strict
--[[
    ASHES OF HUMANITY: LAST SIGNAL
    ServerScriptService.Services.SurvivalService
    Hardcore Interconnected Survival & Location-Based Anatomical Damage Service (Sections 15, 16, 28).
]]

local Players = game:GetService("Players")
local ReplicatedStorage = game:GetService("ReplicatedStorage")
local Workspace = game:GetService("Workspace")

local Config = require(ReplicatedStorage.Shared.Config)
local ItemDefinitions = require(ReplicatedStorage.Shared.ItemDefinitions)
local NetworkSchema = require(ReplicatedStorage.Shared.NetworkSchema)
local Types = require(ReplicatedStorage.Shared.Types)
local UtilityModules = require(ReplicatedStorage.Shared.UtilityModules)

local SurvivalService = {}

local registry: any = nil
local playerStances: { [number]: string } = {}
local lastSyncClock: { [number]: number } = {}

local VALID_BODY_PARTS = {
	Head = true,
	Torso = true,
	LeftArm = true,
	RightArm = true,
	LeftLeg = true,
	RightLeg = true,
}

function SurvivalService:Init(serviceRegistry: any)
	registry = serviceRegistry
	Players.PlayerRemoving:Connect(function(player: Player)
		playerStances[player.UserId] = nil
		lastSyncClock[player.UserId] = nil
	end)
end

function SurvivalService:GetStance(player: Player): string
	return playerStances[player.UserId] or "Walk"
end

function SurvivalService:SetStance(player: Player, stance: string, footstepSurface: string?)
	if
		stance ~= "Idle"
		and stance ~= "Walk"
		and stance ~= "Sprint"
		and stance ~= "Crouch"
		and stance ~= "Crawl"
		and stance ~= "Climb"
		and stance ~= "Vault"
	then
		return
	end
	local profile = registry.PlayerDataService:GetProfile(player)
	if not profile then
		return
	end

	-- Prevent sprinting with fractured leg or zero stamina or while downed
	if stance == "Sprint" then
		if profile.Vitals.IsDowned or profile.Vitals.Stamina < 6 or self:HasLegInjury(profile) then
			stance = "Walk"
		end
	end

	if stance == "Vault" then
		if profile.Vitals.Stamina >= Config.MOVEMENT.VaultStaminaCost then
			profile.Vitals.Stamina = math.max(0, profile.Vitals.Stamina - Config.MOVEMENT.VaultStaminaCost)
		end
		playerStances[player.UserId] = "Walk"
	else
		playerStances[player.UserId] = stance
	end

	self:ApplyMobilityModifiers(player, profile)

	-- Broadcast footstep noise to Mutant AI when moving
	local root = registry.SecurityService:GetCharacterRoot(player)
	if root and registry.EnemyAIService then
		local radius = 0
		if stance == "Sprint" then
			radius = 48
		elseif stance == "Walk" then
			radius = 22
		elseif stance == "Crouch" then
			radius = 8
		elseif stance == "Crawl" then
			radius = 4
		elseif stance == "Vault" then
			radius = 30
		end
		if footstepSurface == "Metal" or footstepSurface == "Glass" then
			radius *= 1.35
		elseif footstepSurface == "Grass" or footstepSurface == "Mud" then
			radius *= 0.85
		end
		if radius > 0 then
			registry.EnemyAIService:NotifySound(root.Position, radius, player)
		end
	end
end

function SurvivalService:HasLegInjury(profile: Types.PlayerSaveProfile): boolean
	local ll = profile.BodyRegions.LeftLeg
	local rl = profile.BodyRegions.RightLeg
	return (ll and (ll.Conditions.Fracture or ll.HP <= 12)) or (rl and (rl.Conditions.Fracture or rl.HP <= 12))
end

function SurvivalService:GetArmPenaltyMultiplier(profile: Types.PlayerSaveProfile): number
	local la = profile.BodyRegions.LeftArm
	local ra = profile.BodyRegions.RightArm
	local mult = 1.0
	if la and (la.Conditions.Fracture or la.HP <= 15) then
		mult += 0.35
	end
	if ra and (ra.Conditions.Fracture or ra.HP <= 15) then
		mult += 0.45
	end
	return mult
end

function SurvivalService:GetGearModifiers(profile: Types.PlayerSaveProfile): (number, number, number)
	local warmth = 0.0
	local armor = 0.0
	local radRes = 0.0
	for _, itemUid in pairs(profile.Inventory.Equipped) do
		local inst = profile.Inventory.Items[itemUid]
		if inst and inst.Durability > 0 then
			local def = ItemDefinitions.Get(inst.ItemId)
			if def then
				warmth += (def.WarmthBonusC or 0)
				armor += (def.ArmorReduction or 0)
				radRes += (def.RadResistance or 0)
			end
		end
	end
	return warmth, math.min(0.65, armor), math.min(0.85, radRes)
end

function SurvivalService:CheckIsSheltered(pos: Vector3, char: Model?): boolean
	if pos.Y < -5 then
		return true
	end
	local params = RaycastParams.new()
	params.FilterType = Enum.RaycastFilterType.Exclude
	if char then
		params.FilterDescendantsInstances = { char }
	end
	local hit = Workspace:Raycast(pos + Vector3.new(0, 1.5, 0), Vector3.new(0, 28, 0), params)
	return hit ~= nil
end

function SurvivalService:ApplyMobilityModifiers(player: Player, profile: Types.PlayerSaveProfile)
	local char = player.Character
	if not char then
		return
	end
	local hum = char:FindFirstChildOfClass("Humanoid")
	if not hum then
		return
	end

	if profile.Vitals.IsDowned then
		hum.WalkSpeed = 2.2
		hum.JumpPower = 0
		return
	end

	local stance = playerStances[player.UserId] or "Walk"
	local speed = Config.MOVEMENT.WalkSpeed
	if stance == "Sprint" and profile.Vitals.Stamina > 2 then
		speed = Config.MOVEMENT.SprintSpeed
	elseif stance == "Crouch" then
		speed = Config.MOVEMENT.CrouchSpeed
	elseif stance == "Crawl" then
		speed = Config.MOVEMENT.CrawlSpeed
	elseif stance == "Climb" then
		speed = Config.MOVEMENT.ClimbSpeed
	end

	-- Leg fracture / injury penalty
	if self:HasLegInjury(profile) then
		speed *= 0.62
	end
	-- Dehydration & Fatigue penalties
	if profile.Vitals.Thirst < 20 then
		speed *= 0.86
	end
	if profile.Vitals.Fatigue > 80 then
		speed *= 0.88
	end
	-- Overweight penalty
	if profile.Inventory.CurrentWeightKg > profile.Inventory.MaxWeightKg then
		speed *= 0.72
	end

	hum.WalkSpeed = math.max(2.5, speed)
	hum.UseJumpPower = true
	hum.JumpPower = if self:HasLegInjury(profile) or stance == "Crawl" then 18 else Config.MOVEMENT.JumpPower
end

function SurvivalService:InflictLocalizedDamage(
	player: Player,
	rawDamage: number,
	targetRegion: string?,
	causes: {
		BleedChance: number?,
		FractureChance: number?,
		BurnChance: number?,
		InfectionChance: number?,
		RadiationAdd: number?,
	}?
): number
	local profile = registry.PlayerDataService:GetProfile(player)
	if not profile or profile.Vitals.Health <= 0 then
		return 0
	end

	local regionName = if targetRegion and VALID_BODY_PARTS[targetRegion] then targetRegion else "Torso"
	local region = profile.BodyRegions[regionName]
	local _, armorRed, radRes = self:GetGearModifiers(profile)

	local finalDamage = math.max(1.0, rawDamage * (1.0 - armorRed))
	if region then
		region.HP = math.max(0, region.HP - finalDamage * 0.85)
		if causes then
			if causes.BleedChance and math.random() < causes.BleedChance then
				if not region.Conditions.Bleeding then
					region.Conditions.Bleeding = true
					profile.Vitals.BleedStacks += 1
				end
			end
			if causes.FractureChance and (regionName ~= "Torso") and math.random() < causes.FractureChance then
				region.Conditions.Fracture = true
			end
			if causes.BurnChance and math.random() < causes.BurnChance then
				region.Conditions.Burn = true
			end
			if causes.InfectionChance and math.random() < causes.InfectionChance then
				region.Conditions.Infection = true
				profile.Vitals.Infection = UtilityModules.Clamp(profile.Vitals.Infection + 12, 0, 100)
			end
			if causes.RadiationAdd and causes.RadiationAdd > 0 then
				profile.Vitals.Radiation = UtilityModules.Clamp(
					profile.Vitals.Radiation + causes.RadiationAdd * (1.0 - radRes),
					0,
					Config.SURVIVAL.MaxRadiation
				)
			end
		end
	end

	-- Degrade equipped armor slightly on hit
	local armorUid = profile.Inventory.Equipped.Armor
	if armorUid and profile.Inventory.Items[armorUid] then
		local aItem = profile.Inventory.Items[armorUid]
		aItem.Durability = math.max(0, aItem.Durability - finalDamage * 0.12)
	end

	-- Stress spike on trauma
	profile.Vitals.MentalStress = UtilityModules.Clamp(profile.Vitals.MentalStress + finalDamage * 0.45, 0, 100)
	self:ModifyHealth(player, -finalDamage, "CombatTrauma")
	self:ApplyMobilityModifiers(player, profile)
	self:SyncPlayerVitals(player, true)
	return finalDamage
end

function SurvivalService:ModifyHealth(player: Player, delta: number, _reason: string?)
	local profile = registry.PlayerDataService:GetProfile(player)
	if not profile then
		return
	end
	local v = profile.Vitals
	-- Radiation reduces effective MaxHealth ceiling
	local radPenalty = math.floor((v.Radiation / Config.SURVIVAL.MaxRadiation) * 45.0)
	v.MaxHealth = math.max(35, Config.SURVIVAL.MaxHealth - radPenalty)
	v.Health = UtilityModules.Clamp(v.Health + delta, 0, v.MaxHealth)

	local char = player.Character
	local hum = if char then char:FindFirstChildOfClass("Humanoid") else nil

	if v.Health <= 0 then
		-- Cooperative Party Downed / Bleedout check!
		local canGoDowned = registry.PartyService and registry.PartyService:HasLivingPartyMember(player)
		if canGoDowned and not v.IsDowned then
			v.IsDowned = true
			v.DownedBleedoutTimer = 35.0
			v.Health = 15.0
			self:ApplyMobilityModifiers(player, profile)
			registry.SendToPlayer(player, NetworkSchema.EVENTS.Notification, {
				Kind = "Danger",
				Text = "CRITICAL TRAUMA — Downed! Awaiting teammate revival (35s).",
			})
			return
		end

		-- Lethal death
		v.IsDowned = false
		v.Health = 0
		profile.Stats.Deaths += 1
		if hum and hum.Health > 0 then
			hum.Health = 0
		end
	else
		if hum and hum.Health > 0 then
			hum.MaxHealth = v.MaxHealth
			hum.Health = math.max(1, v.Health)
		end
	end
end

function SurvivalService:ApplyConsumableEffects(
	player: Player,
	effects: { [string]: any },
	targetPart: string?
): boolean
	local profile = registry.PlayerDataService:GetProfile(player)
	if not profile then
		return false
	end
	local v = profile.Vitals

	if effects.BatteryRestore then
		v.FlashlightBattery =
			UtilityModules.Clamp(v.FlashlightBattery + effects.BatteryRestore, 0, Config.SURVIVAL.FlashlightMaxBattery)
	end
	if effects.Hunger then
		v.Hunger = UtilityModules.Clamp(v.Hunger + effects.Hunger, 0, Config.SURVIVAL.MaxHunger)
	end
	if effects.Thirst then
		v.Thirst = UtilityModules.Clamp(v.Thirst + effects.Thirst, 0, Config.SURVIVAL.MaxThirst)
	end
	if effects.Stamina then
		v.Stamina = UtilityModules.Clamp(v.Stamina + effects.Stamina, 0, v.MaxStaminaCap)
	end
	if effects.TempDelta then
		v.BodyTemperature = UtilityModules.Clamp(v.BodyTemperature + effects.TempDelta, 34.0, 40.5)
	end
	if effects.Fatigue then
		v.Fatigue = UtilityModules.Clamp(v.Fatigue + effects.Fatigue, 0, 100)
	end
	if effects.Stress then
		v.MentalStress = UtilityModules.Clamp(v.MentalStress + effects.Stress, 0, 100)
	end
	if effects.Radiation then
		v.Radiation = UtilityModules.Clamp(v.Radiation + effects.Radiation, 0, Config.SURVIVAL.MaxRadiation)
	end
	if effects.Infection then
		v.Infection = UtilityModules.Clamp(v.Infection + effects.Infection, 0, 100)
	end

	-- Target region or worst injured region
	local chosenRegion: Types.BodyRegionState? = nil
	if targetPart and VALID_BODY_PARTS[targetPart] then
		chosenRegion = profile.BodyRegions[targetPart]
	else
		-- Auto-pick bleeding/fractured/lowest HP region
		local worstScore = -1
		for _, reg in pairs(profile.BodyRegions) do
			local score = (reg.MaxHP - reg.HP)
				+ (if reg.Conditions.Bleeding then 100 else 0)
				+ (if reg.Conditions.Fracture then 80 else 0)
			if score > worstScore then
				worstScore = score
				chosenRegion = reg
			end
		end
	end

	if chosenRegion then
		if effects.LimbHeal then
			chosenRegion.HP = UtilityModules.Clamp(chosenRegion.HP + effects.LimbHeal, 0, chosenRegion.MaxHP)
		end
		if effects.CureBleed and chosenRegion.Conditions.Bleeding then
			chosenRegion.Conditions.Bleeding = false
			v.BleedStacks = math.max(0, v.BleedStacks - 1)
		end
		if effects.CureFracture then
			chosenRegion.Conditions.Fracture = false
		end
		if effects.CureBurn then
			chosenRegion.Conditions.Burn = false
		end
		if effects.CureLimbInfection then
			chosenRegion.Conditions.Infection = false
		end
		if effects.CureRadSickness then
			chosenRegion.Conditions.RadiationSickness = false
		end
	end

	if effects.ClearAllBleed then
		for _, reg in pairs(profile.BodyRegions) do
			reg.Conditions.Bleeding = false
		end
		v.BleedStacks = 0
	end

	local healTotal = (effects.HealHP or 0) + (effects.Health or 0)
	if healTotal ~= 0 then
		self:ModifyHealth(player, healTotal, "MedicalItem")
	end

	self:ApplyMobilityModifiers(player, profile)
	self:SyncPlayerVitals(player, true)
	return true
end

function SurvivalService:ToggleFlashlight(player: Player)
	local profile = registry.PlayerDataService:GetProfile(player)
	if not profile then
		return
	end
	local utilUid = profile.Inventory.Equipped.Utility
	if not utilUid or not profile.Inventory.Items[utilUid] then
		profile.Vitals.FlashlightOn = false
	elseif profile.Vitals.FlashlightBattery <= 0 then
		profile.Vitals.FlashlightOn = false
	else
		profile.Vitals.FlashlightOn = not profile.Vitals.FlashlightOn
	end

	-- Sync attribute onto character so other co-op clients & AI see flashlight beam
	if player.Character then
		player.Character:SetAttribute("FlashlightOn", profile.Vitals.FlashlightOn)
	end
	self:SyncPlayerVitals(player, true)
end

function SurvivalService:RestAtShelterBunk(player: Player)
	local profile = registry.PlayerDataService:GetProfile(player)
	if not profile then
		return
	end
	profile.Vitals.Fatigue = math.max(0, profile.Vitals.Fatigue - 55)
	profile.Vitals.MentalStress = math.max(0, profile.Vitals.MentalStress - 35)
	self:ModifyHealth(player, 15, "Rest")
	self:SyncPlayerVitals(player, true)
	registry.SendToPlayer(player, NetworkSchema.EVENTS.Notification, {
		Kind = "Info",
		Text = "Rested on shelter bunk: Fatigue and Mental Stress reduced.",
	})
end

function SurvivalService:ResetOnRespawn(player: Player)
	local profile = registry.PlayerDataService:GetProfile(player)
	if not profile then
		return
	end
	local v = profile.Vitals
	v.IsDowned = false
	v.DownedBleedoutTimer = 0
	v.MaxHealth = Config.SURVIVAL.MaxHealth
	v.Health = 85.0
	v.Hunger = math.max(55.0, v.Hunger)
	v.Thirst = math.max(55.0, v.Thirst)
	v.Stamina = 100.0
	v.MaxStaminaCap = 100.0
	v.BodyTemperature = 37.0
	v.Radiation = math.min(v.Radiation * 0.25, 80.0)
	v.Infection = 0.0
	v.Fatigue = 15.0
	v.BleedStacks = 0
	v.MentalStress = 20.0
	v.FlashlightBattery = math.max(35.0, v.FlashlightBattery)

	for partName, spec in pairs(Config.BODY_REGIONS) do
		profile.BodyRegions[partName] = {
			HP = spec.MaxHP,
			MaxHP = spec.MaxHP,
			Conditions = {
				Bleeding = false,
				Fracture = false,
				Burn = false,
				Infection = false,
				RadiationSickness = false,
			},
		}
	end
	self:ApplyMobilityModifiers(player, profile)
	self:SyncPlayerVitals(player, true)
end

function SurvivalService:StepPlayer(player: Player, dt: number)
	local profile = registry.PlayerDataService:GetProfile(player)
	if not profile then
		return
	end
	local v = profile.Vitals
	if v.Health <= 0 and not v.IsDowned then
		return
	end

	local root = registry.SecurityService:GetCharacterRoot(player)
	local pos = if root then root.Position else Config.SHELTER_SPAWN_POS

	-- Resolve current region & shelter status
	local prevRegion = v.CurrentRegion
	local curRegion = UtilityModules.GetRegionAtPosition(pos)
	v.CurrentRegion = curRegion
	v.IsSheltered = self:CheckIsSheltered(pos, player.Character)

	if curRegion ~= prevRegion and registry.QuestService then
		registry.QuestService:NotifyProgress(player, "VisitRegion", curRegion, 1)
	end

	-- Downed bleedout tick
	if v.IsDowned then
		v.DownedBleedoutTimer -= dt
		if v.DownedBleedoutTimer <= 0 then
			v.IsDowned = false
			self:ModifyHealth(player, -999, "Bleedout")
		end
		self:SyncPlayerVitals(player, false)
		return
	end

	-- 1. Hunger & Thirst decay
	local stance = playerStances[player.UserId] or "Walk"
	local activityMult = if stance == "Sprint" then 1.55 elseif stance == "Walk" then 1.08 else 0.85
	v.Hunger = math.max(0, v.Hunger - Config.SURVIVAL.HungerDecayPerSec * activityMult * dt)
	v.Thirst = math.max(0, v.Thirst - Config.SURVIVAL.ThirstDecayPerSec * activityMult * dt)

	-- 2. Weather & Ambient Temperature -> Core Body Temperature
	local regInfo = (Config.REGIONS :: any)[curRegion] or Config.REGIONS.DeadSuburbs
	local weatherTempOffset = if registry.WeatherService then registry.WeatherService:GetTempOffsetC() else 0.0
	local isNight = if registry.WeatherService then registry.WeatherService:IsNight() else false
	local warmthGear, _, radRes = self:GetGearModifiers(profile)
	local campfireBonus = if registry.BuildingService then registry.BuildingService:GetWarmthBonusAt(pos) else 0.0

	local envTempC = regInfo.BaseTempC
		+ weatherTempOffset
		+ (if isNight and not v.IsSheltered then Config.NIGHT_TEMP_DROP_C else 0)
	if v.IsSheltered then
		envTempC = math.max(envTempC + 6.5, 17.5)
	end
	local effectiveFeltTemp = envTempC + warmthGear + campfireBonus
	local targetBodyTemp = 37.0 + UtilityModules.Clamp((effectiveFeltTemp - 16.0) * 0.09, -2.8, 2.2)
	v.BodyTemperature = UtilityModules.Lerp(v.BodyTemperature, targetBodyTemp, dt * 0.08)

	-- 3. Fatigue & Stamina Cap Coupling
	local extraColdFatigue = if v.BodyTemperature < 35.8 then Config.SURVIVAL.ColdExtraFatiguePerSec else 0.0
	v.Fatigue = UtilityModules.Clamp(v.Fatigue + (Config.SURVIVAL.FatigueGainPerSec + extraColdFatigue) * dt, 0, 100)

	local staminaCap = 100.0
	if v.Hunger < 25 then
		staminaCap -= (25 - v.Hunger) * 0.9
	end
	if v.Thirst < 25 then
		staminaCap -= (25 - v.Thirst) * 1.1
	end
	if v.Fatigue > 60 then
		staminaCap -= (v.Fatigue - 60) * 0.65
	end
	if v.Infection > 30 then
		staminaCap -= (v.Infection - 30) * 0.35
	end
	v.MaxStaminaCap = UtilityModules.Clamp(staminaCap, 25, 100)

	if stance == "Sprint" and root and root.AssemblyLinearVelocity.Magnitude > 4 then
		v.Stamina = math.max(0, v.Stamina - Config.MOVEMENT.SprintStaminaDrainPerSec * dt)
		if v.Stamina <= 0.5 then
			playerStances[player.UserId] = "Walk"
			self:ApplyMobilityModifiers(player, profile)
		end
	else
		local regenMult = 1.0
		if v.Hunger < 35 then
			regenMult *= 0.58
		end
		if v.Thirst < 35 then
			regenMult *= 0.52
		end
		v.Stamina = UtilityModules.Clamp(
			v.Stamina + Config.MOVEMENT.BaseStaminaRegenPerSec * regenMult * dt,
			0,
			v.MaxStaminaCap
		)
	end

	-- 4. Environmental & Storm Radiation
	local outdoorStormRad = if (not v.IsSheltered and registry.WeatherService)
		then registry.WeatherService:GetOutdoorRadiationPerSec()
		else 0.0
	local rawRad = (if v.IsSheltered and curRegion == "EmergencyShelter" then 0 else regInfo.AmbientRadiation)
		+ outdoorStormRad
	if rawRad > 0 then
		v.Radiation = UtilityModules.Clamp(v.Radiation + rawRad * (1.0 - radRes) * dt, 0, Config.SURVIVAL.MaxRadiation)
	end
	if v.Radiation >= 350 then
		profile.BodyRegions.Torso.Conditions.RadiationSickness = true
	end

	-- 5. Bleeding & Untreated Infection Progression
	local bleedCount = 0
	local hasLimbInfection = false
	for _, reg in pairs(profile.BodyRegions) do
		if reg.Conditions.Bleeding then
			bleedCount += 1
		end
		if reg.Conditions.Infection then
			hasLimbInfection = true
		end
	end
	v.BleedStacks = bleedCount

	if hasLimbInfection or v.Infection > 5 then
		v.Infection = UtilityModules.Clamp(v.Infection + Config.SURVIVAL.InfectionProgressPerSec * dt, 0, 100)
	end

	-- 6. Psychological Stress (Darkness, Isolation, Fatigue, Bleed, Campfire relief)
	local stressDelta = -0.35 -- Slow natural recovery when safe
	if isNight and not v.IsSheltered and not v.FlashlightOn and campfireBonus <= 0 then
		stressDelta += 1.05
	end
	if v.Fatigue > 75 then
		stressDelta += 0.35
	end
	if v.BleedStacks > 0 then
		stressDelta += 0.55 * v.BleedStacks
	end
	if campfireBonus > 0 or curRegion == "EmergencyShelter" then
		stressDelta -= 1.25
	end
	v.MentalStress = UtilityModules.Clamp(v.MentalStress + stressDelta * dt, 0, 100)

	-- 7. Flashlight Battery Drain
	if v.FlashlightOn then
		v.FlashlightBattery = math.max(0, v.FlashlightBattery - Config.SURVIVAL.FlashlightDrainPerSec * dt)
		if v.FlashlightBattery <= 0 then
			v.FlashlightOn = false
			if player.Character then
				player.Character:SetAttribute("FlashlightOn", false)
			end
		end
	end

	-- 8. Progressive Health Drain / Slow Well-Fed Recovery
	local hpDelta = 0.0
	if v.Hunger <= 0 then
		hpDelta -= Config.SURVIVAL.StarvationDamagePerSec * dt
	end
	if v.Thirst <= 0 then
		hpDelta -= Config.SURVIVAL.DehydrationDamagePerSec * dt
	end
	if v.BleedStacks > 0 then
		hpDelta -= Config.SURVIVAL.BleedDamagePerStackPerSec * v.BleedStacks * dt
	end
	if v.Infection > 65 then
		hpDelta -= Config.SURVIVAL.SevereInfectionDamagePerSec * dt
	end
	if v.BodyTemperature < Config.SURVIVAL.HypothermiaThresholdC then
		hpDelta -= Config.SURVIVAL.HypothermiaDamagePerSec * dt
	end
	if v.Hunger > 65 and v.Thirst > 65 and v.BleedStacks == 0 and v.Infection < 15 and v.Radiation < 250 then
		hpDelta += 0.28 * dt
	end

	if hpDelta ~= 0 then
		self:ModifyHealth(player, hpDelta, "SurvivalTick")
	end

	self:SyncPlayerVitals(player, false)
end

function SurvivalService:SyncPlayerVitals(player: Player, force: boolean)
	local now = os.clock()
	if not force and (now - (lastSyncClock[player.UserId] or 0)) < 0.85 then
		return
	end
	lastSyncClock[player.UserId] = now
	local profile = registry.PlayerDataService:GetProfile(player)
	if not profile then
		return
	end
	registry.SendToPlayer(player, NetworkSchema.EVENTS.VitalsUpdate, {
		Vitals = profile.Vitals,
		BodyRegions = profile.BodyRegions,
	})
end

function SurvivalService:Start()
	task.spawn(function()
		while true do
			task.wait(Config.SURVIVAL.TickInterval)
			for _, player in ipairs(Players:GetPlayers()) do
				self:StepPlayer(player, Config.SURVIVAL.TickInterval)
			end
		end
	end)
end

return SurvivalService
