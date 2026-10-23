--!strict
--[[
    ASHES OF HUMANITY: LAST SIGNAL
    ServerScriptService.Services.CombatService
    Server-Authoritative Weapon Ballistics, Reloading, Melee, Durability,
    Acoustic Gunshot Alerts, and Selective Environmental Destruction (Sections 11, 23).
]]

local Players = game:GetService("Players")
local ReplicatedStorage = game:GetService("ReplicatedStorage")
local Workspace = game:GetService("Workspace")

local ItemDefinitions = require(ReplicatedStorage.Shared.ItemDefinitions)
local NetworkSchema = require(ReplicatedStorage.Shared.NetworkSchema)
local Types = require(ReplicatedStorage.Shared.Types)
local UtilityModules = require(ReplicatedStorage.Shared.UtilityModules)
local WeaponDefinitions = require(ReplicatedStorage.Shared.WeaponDefinitions)

local CombatService = {}

local registry: any = nil
local lastFireClock: { [number]: number } = {}
local reloadEndClock: { [number]: number } = {}
local rng = Random.new()

function CombatService:Init(serviceRegistry: any)
	registry = serviceRegistry
	Players.PlayerRemoving:Connect(function(player: Player)
		lastFireClock[player.UserId] = nil
		reloadEndClock[player.UserId] = nil
	end)
end

function CombatService:Start() end

function CombatService:GetActiveWeaponInstance(
	profile: Types.PlayerSaveProfile,
	requestedSlotOrUid: string?
): (Types.GridItemInstance?, WeaponDefinitions.WeaponDef?)
	local inv = profile.Inventory
	local candidateUid: string? = nil

	if requestedSlotOrUid and inv.Equipped[requestedSlotOrUid] then
		candidateUid = inv.Equipped[requestedSlotOrUid]
	elseif requestedSlotOrUid and inv.Items[requestedSlotOrUid] then
		-- Must be equipped in Primary/Secondary/Melee to fire!
		for _, slot in ipairs({ "Primary", "Secondary", "Melee" }) do
			if inv.Equipped[slot] == requestedSlotOrUid then
				candidateUid = requestedSlotOrUid
				break
			end
		end
	else
		for _, slot in ipairs({ "Primary", "Secondary", "Melee" }) do
			if inv.Equipped[slot] then
				candidateUid = inv.Equipped[slot]
				break
			end
		end
	end

	if not candidateUid then
		return nil, nil
	end
	local inst = inv.Items[candidateUid]
	if not inst then
		return nil, nil
	end
	local itemDef = ItemDefinitions.Get(inst.ItemId)
	if not itemDef or not itemDef.WeaponId then
		return nil, nil
	end
	local wpnDef = WeaponDefinitions.Get(itemDef.WeaponId)
	return inst, wpnDef
end

function CombatService:DamageDestructibleObject(targetPart: BasePart, damage: number)
	local dType = targetPart:GetAttribute("DestructibleType")
	if type(dType) ~= "string" then
		return false
	end
	local hp = targetPart:GetAttribute("StructureHP")
	if type(hp) ~= "number" then
		hp = if dType == "Glass" then 18 elseif dType == "WoodenBarricade" then 85 else 110
	end
	hp -= damage
	targetPart:SetAttribute("StructureHP", hp)

	if hp <= 0 and targetPart.Transparency < 0.95 then
		local origTrans = targetPart.Transparency
		local origCollide = targetPart.CanCollide
		targetPart.Transparency = 1.0
		targetPart.CanCollide = false

		-- Emit acoustic shatter/splinter noise to nearby mutants!
		local noiseRad = if dType == "Glass" then 75 else 55
		if registry.EnemyAIService then
			registry.EnemyAIService:NotifySound(targetPart.Position, noiseRad, nil)
		end

		registry.BroadcastAll(NetworkSchema.EVENTS.CombatFeedback, {
			Kind = "Destruction",
			DestructibleType = dType,
			Position = targetPart.Position,
		})

		-- Selective respawn after 300s to keep long multiplayer servers playable
		task.delay(300, function()
			if targetPart and targetPart.Parent then
				targetPart.Transparency = origTrans
				targetPart.CanCollide = origCollide
				targetPart:SetAttribute(
					"StructureHP",
					if dType == "Glass" then 18 elseif dType == "WoodenBarricade" then 85 else 110
				)
			end
		end)
	end
	return true
end

local function applySpreadToDirection(dir: Vector3, spreadDeg: number): Vector3
	if spreadDeg <= 0.01 then
		return dir.Unit
	end
	local rad = math.rad(spreadDeg)
	local rx = (rng:NextNumber() * 2 - 1) * rad
	local ry = (rng:NextNumber() * 2 - 1) * rad
	local cf = CFrame.lookAt(Vector3.zero, dir.Unit) * CFrame.Angles(rx, ry, 0)
	return cf.LookVector
end

function CombatService:HandleWeaponFire(player: Player, payload: any): (boolean, string?)
	if type(payload) ~= "table" then
		return false, "BadPayload"
	end
	if
		not NetworkSchema.ValidateVector3(payload.Origin, 4096)
		or not NetworkSchema.ValidateVector3(payload.Direction, 10)
	then
		return false, "BadVectors"
	end

	local profile = registry.PlayerDataService:GetProfile(player)
	if not profile or profile.Vitals.Health <= 0 or profile.Vitals.IsDowned then
		return false, "Unavailable"
	end

	local root = registry.SecurityService:GetCharacterRoot(player)
	if not root then
		return false, "NoCharacter"
	end

	-- Anti-cheat: muzzle origin must be within 7.5 studs of player's head/torso!
	local origin: Vector3 = payload.Origin
	if (origin - root.Position).Magnitude > 7.5 then
		return false, "OriginOutOfBounds"
	end

	local dirRaw: Vector3 = payload.Direction
	if dirRaw.Magnitude < 0.1 then
		return false, "ZeroDirection"
	end
	local aimDir = dirRaw.Unit

	local slotArg = if type(payload.Slot) == "string" then payload.Slot else nil
	local wpnInst, wpnDef = self:GetActiveWeaponInstance(profile, slotArg)
	if not wpnInst or not wpnDef then
		return false, "NoWeapon"
	end
	if wpnDef.Type == "Melee" then
		return self:HandleMeleeSwing(player, payload)
	end
	if wpnInst.Durability <= 0 then
		registry.SendToPlayer(player, NetworkSchema.EVENTS.Notification, {
			Kind = "Warning",
			Text = wpnDef.Name .. " is broken! Repair it with a Maintenance Kit.",
		})
		return false, "WeaponBroken"
	end

	local now = os.clock()
	if reloadEndClock[player.UserId] and now < reloadEndClock[player.UserId] then
		return false, "Reloading"
	end

	-- Server fire-rate enforcement (with 8% network jitter tolerance)
	local minInterval = (60.0 / math.max(1, wpnDef.RoundsPerMinute)) * 0.92
	if (now - (lastFireClock[player.UserId] or 0)) < minInterval then
		return false, "FireRateExceeded"
	end

	local clip = wpnInst.ClipAmmo or 0
	if clip <= 0 then
		return false, "EmptyMagazine"
	end

	-- Consume 1 round & apply weapon wear
	lastFireClock[player.UserId] = now
	wpnInst.ClipAmmo = clip - 1
	wpnInst.Durability = math.max(0, wpnInst.Durability - wpnDef.DurabilityLossPerUse)

	-- Broadcast acoustic gunshot to Mutant AI (masked partially during Thunderstorms!)
	local weatherMask = if registry.WeatherService then registry.WeatherService:GetSoundMaskFactor() else 1.0
	if registry.EnemyAIService then
		registry.EnemyAIService:NotifySound(root.Position, wpnDef.AcousticRadiusStuds * weatherMask, player)
	end

	local isADS = payload.ADS == true
	local armPenalty = if registry.SurvivalService
		then registry.SurvivalService:GetArmPenaltyMultiplier(profile)
		else 1.0
	local spread = (if isADS then wpnDef.ADSSpreadDeg else wpnDef.HipSpreadDeg) * armPenalty

	local rayParams = RaycastParams.new()
	rayParams.FilterType = Enum.RaycastFilterType.Exclude
	rayParams.FilterDescendantsInstances = if player.Character then { player.Character } else {}

	local hitsSummary = {}
	for _ = 1, wpnDef.Pellets do
		local shotDir = applySpreadToDirection(aimDir, spread)
		local rayResult = Workspace:Raycast(origin, shotDir * wpnDef.MaxRange, rayParams)
		if rayResult and rayResult.Instance then
			local hitPart = rayResult.Instance
			local dist = (rayResult.Position - origin).Magnitude
			local falloff = WeaponDefinitions.ComputeFalloffMultiplier(wpnDef, dist)
			local dmg = wpnDef.BaseDamage * falloff

			-- Condition degrades damage slightly below 25% durability
			if wpnInst.Durability < wpnInst.MaxDurability * 0.25 then
				dmg *= 0.82
			end

			-- 1. Check if hit a Mutant
			local hitMutantId = self:ResolveMutantIdFromPart(hitPart)
			if hitMutantId and registry.EnemyAIService then
				local actualDmg, killed, isWeakspot = registry.EnemyAIService:ApplyDamageToMutant(
					hitMutantId,
					dmg,
					hitPart.Name,
					wpnDef.StaggerPower,
					player
				)
				table.insert(hitsSummary, {
					Pos = rayResult.Position,
					MutantId = hitMutantId,
					Damage = actualDmg,
					Weakspot = isWeakspot,
					Killed = killed,
				})
			elseif hitPart:IsA("BasePart") then
				-- 2. Check Selective Environmental Destruction
				if hitPart:GetAttribute("DestructibleType") ~= nil then
					self:DamageDestructibleObject(hitPart, dmg * wpnDef.BarricadeDamageMultiplier)
				end
				table.insert(hitsSummary, {
					Pos = rayResult.Position,
					Surface = hitPart.Material.Name,
				})
			end
		end
	end

	registry.InventoryService:SyncInventory(player)
	registry.BroadcastAll(NetworkSchema.EVENTS.CombatFeedback, {
		Kind = "ShotFired",
		ShooterId = player.UserId,
		WeaponId = wpnDef.Id,
		Origin = origin,
		Direction = aimDir,
		Hits = hitsSummary,
	})

	return true, nil
end

function CombatService:HandleWeaponReload(player: Player, payload: any): (boolean, string?)
	local profile = registry.PlayerDataService:GetProfile(player)
	if not profile or profile.Vitals.Health <= 0 or profile.Vitals.IsDowned then
		return false, "Unavailable"
	end

	local slotArg = if type(payload) == "table" and type(payload.Slot) == "string" then payload.Slot else nil
	local wpnInst, wpnDef = self:GetActiveWeaponInstance(profile, slotArg)
	if not wpnInst or not wpnDef or wpnDef.MagSize <= 0 or not wpnDef.AmmoItemId then
		return false, "CannotReload"
	end

	local currentClip = wpnInst.ClipAmmo or 0
	if currentClip >= wpnDef.MagSize then
		return false, "MagazineFull"
	end

	local availableReserve = registry.InventoryService:CountItem(player, wpnDef.AmmoItemId)
	if availableReserve <= 0 then
		return false, "NoReserveAmmo"
	end

	local needed = wpnDef.MagSize - currentClip
	local loadCount = math.min(needed, availableReserve)
	local armPenalty = if registry.SurvivalService
		then registry.SurvivalService:GetArmPenaltyMultiplier(profile)
		else 1.0
	local reloadDuration = wpnDef.ReloadSeconds * armPenalty

	local now = os.clock()
	if reloadEndClock[player.UserId] and now < reloadEndClock[player.UserId] then
		return false, "AlreadyReloading"
	end
	reloadEndClock[player.UserId] = now + reloadDuration * 0.90

	if not registry.InventoryService:RemoveItemById(player, wpnDef.AmmoItemId, loadCount, true) then
		return false, "AmmoConsumeFailed"
	end
	wpnInst.ClipAmmo = currentClip + loadCount
	registry.InventoryService:SyncInventory(player)

	registry.SendToPlayer(player, NetworkSchema.EVENTS.CombatFeedback, {
		Kind = "ReloadStarted",
		WeaponId = wpnDef.Id,
		Duration = reloadDuration,
		NewClip = wpnInst.ClipAmmo,
	})
	return true, nil
end

function CombatService:HandleMeleeSwing(player: Player, payload: any): (boolean, string?)
	if type(payload) ~= "table" or not NetworkSchema.ValidateVector3(payload.Direction, 10) then
		return false, "BadPayload"
	end
	local profile = registry.PlayerDataService:GetProfile(player)
	if not profile or profile.Vitals.Health <= 0 or profile.Vitals.IsDowned then
		return false, "Unavailable"
	end
	local root = registry.SecurityService:GetCharacterRoot(player)
	if not root then
		return false, "NoCharacter"
	end

	local slotArg = if type(payload.Slot) == "string" then payload.Slot else "Melee"
	local wpnInst, wpnDef = self:GetActiveWeaponInstance(profile, slotArg)
	if not wpnInst or not wpnDef then
		-- Unarmed desperate shove fallback
		wpnDef = WeaponDefinitions.Get("CombatKnife")
	end
	if not wpnDef then
		return false, "NoMelee"
	end

	local now = os.clock()
	local minInterval = (60.0 / math.max(1, wpnDef.RoundsPerMinute)) * 0.88
	if (now - (lastFireClock[player.UserId] or 0)) < minInterval then
		return false, "SwingCooldown"
	end
	if profile.Vitals.Stamina < wpnDef.StaminaCost * 0.5 then
		return false, "Exhausted"
	end

	lastFireClock[player.UserId] = now
	profile.Vitals.Stamina = math.max(0, profile.Vitals.Stamina - wpnDef.StaminaCost)
	if wpnInst then
		wpnInst.Durability = math.max(0, wpnInst.Durability - wpnDef.DurabilityLossPerUse)
	end

	local origin = root.Position + Vector3.new(0, 1.4, 0)
	local dir = (payload.Direction :: Vector3).Unit

	if registry.EnemyAIService then
		registry.EnemyAIService:NotifySound(origin, wpnDef.AcousticRadiusStuds, player)
	end

	local rayParams = RaycastParams.new()
	rayParams.FilterType = Enum.RaycastFilterType.Exclude
	rayParams.FilterDescendantsInstances = if player.Character then { player.Character } else {}

	local hit = Workspace:Raycast(origin, dir * wpnDef.MaxRange, rayParams)
	if hit and hit.Instance then
		local dmg = if wpnInst and wpnInst.Durability > 0 then wpnDef.BaseDamage else 10
		local mutantId = self:ResolveMutantIdFromPart(hit.Instance)
		if mutantId and registry.EnemyAIService then
			local actualDmg, killed, isWeakspot = registry.EnemyAIService:ApplyDamageToMutant(
				mutantId,
				dmg,
				hit.Instance.Name,
				wpnDef.StaggerPower,
				player
			)
			registry.SendToPlayer(player, NetworkSchema.EVENTS.CombatFeedback, {
				Kind = "MeleeHit",
				MutantId = mutantId,
				Damage = actualDmg,
				Weakspot = isWeakspot,
				Killed = killed,
				Pos = hit.Position,
			})
		elseif hit.Instance:IsA("BasePart") and hit.Instance:GetAttribute("DestructibleType") ~= nil then
			self:DamageDestructibleObject(hit.Instance, dmg * wpnDef.BarricadeDamageMultiplier)
		end
	end

	registry.InventoryService:SyncInventory(player)
	registry.SurvivalService:SyncPlayerVitals(player, false)
	return true, nil
end

function CombatService:ResolveMutantIdFromPart(inst: Instance): string?
	local cur: Instance? = inst
	while cur and cur ~= Workspace do
		local mid = cur:GetAttribute("MutantId")
		if type(mid) == "string" and mid ~= "" then
			return mid
		end
		cur = cur.Parent
	end
	return nil
end

return CombatService
