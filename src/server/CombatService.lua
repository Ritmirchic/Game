--!strict
--[[
	CombatService — server authoritative combat.

	Flow for a player attack:
	  client: RequestAttack(direction, origin, targetHint?)
	  server: validate fire rate, ammo, durability, stance  ->  raycast  ->
	          resolve body part  ->  apply damage  ->  feedback to attacker and victim

	Hitscan resolution uses a single raycast from the character's head along the
	client-supplied direction, but the *origin* is server-owned, so aim spoofing
	cannot teleport hits. Both melee (short sphere cast) and firearms use the same
	path, with projectiles handled as delayed hitscan for the crossbow.
]]

local Players = game:GetService("Players")
local ReplicatedStorage = game:GetService("ReplicatedStorage")
local Workspace = game:GetService("Workspace")

local Shared = ReplicatedStorage:WaitForChild("Shared")
local Items = require(Shared.ItemDefinitions)
local Weapons = require(Shared.WeaponDefinitions)
local Config = require(Shared.Config)
local CombatMath = require(Shared.CombatMath)
local Inventory = require(Shared.InventoryModel)
local Util = require(Shared.Util)

local CombatService = {}

local registryRef
local remotes
local lastAttack = {}
local rng = Random.new(os.time())
local downed = {}

function CombatService.Init(registry)
	registryRef = registry
	remotes = registry.Remotes
end

function CombatService.Start(registry)
	remotes.On("RequestAttack", function(player, direction, origin, target)
		CombatService.Attack(player, direction, origin, target)
	end)
	remotes.On("RequestReload", function(player)
		CombatService.Reload(player)
	end)
	Players.PlayerRemoving:Connect(function(player)
		lastAttack[player.UserId] = nil
		downed[player.UserId] = nil
	end)
end

function CombatService.RefreshLoadout(player, profile)
	-- ensure magazine state exists for firearms
	local entry = Inventory.getEquipped(profile.inventory, "Primary")
	if entry then
		local def = Weapons[Items[entry.itemId] and Items[entry.itemId].weapon]
		if def and entry.magazine == nil then
			entry.magazine = def.magazine
		end
	end
end

local function equippedWeapon(profile)
	local inventory = profile.inventory
	local slotOrder = { "Primary", "Secondary", "Melee" }
	for _, slot in ipairs(slotOrder) do
		local entry = Inventory.getEquipped(inventory, slot)
		if entry then
			local item = Items[entry.itemId]
			if item and item.weapon then
				return entry, Weapons[item.weapon], slot
			end
		end
	end
	return nil
end

function CombatService.GetEquippedWeapon(player)
	local profile = registryRef.PlayerDataService.Get(player)
	if not profile then return nil end
	return equippedWeapon(profile)
end

local function characterOf(player)
	local character = player.Character
	if not character then return nil end
	local humanoid = character:FindFirstChildOfClass("Humanoid")
	local root = character:FindFirstChild("HumanoidRootPart")
	if not humanoid or not root or humanoid.Health <= 0 then
		return nil
	end
	return character, humanoid, root
end

local function raycastParamsFor(player, character)
	local params = RaycastParams.new()
	params.FilterType = Enum.RaycastFilterType.Exclude
	local ignore = { character }
	if character then
		for _, part in ipairs(character:GetDescendants()) do
			table.insert(ignore, part)
		end
	end
	params.FilterDescendantsInstances = ignore
	params.IgnoreWater = true
	params.RespectCanCollide = true
	return params
end

function CombatService.Attack(player, direction, origin, targetHint)
	local profile = registryRef.PlayerDataService.Get(player)
	if not profile or not profile.survival.alive then return false end
	local character, humanoid, root = characterOf(player)
	if not character then return false end

	local entry, def, slot = equippedWeapon(profile)
	if not entry or not def then
		return false
	end

	local now = os.clock()
	local last = lastAttack[player.UserId] or 0
	if not CombatMath.canFire(last, now, def) then
		return false
	end
	lastAttack[player.UserId] = now

	-- validate the client direction
	if typeof(direction) ~= "Vector3" then
		direction = root.CFrame.LookVector
	end
	if direction.Magnitude < 0.01 then
		return false
	end
	direction = direction.Unit

	-- stance / stamina
	local survival = profile.survival
	if def.kind == "melee" and survival.stamina < (def.stamina or 4) then
		remotes.Fire(player, "Notify", "Too exhausted to swing.", "warn", 2)
		return false
	end
	if def.kind == "melee" then
		survival.stamina = math.max(0, survival.stamina - (def.stamina or 4))
	end

	-- ammo handling
	if def.kind == "firearm" then
		if entry.magazine == nil then
			entry.magazine = def.magazine
		end
		if (entry.magazine or 0) <= 0 then
			remotes.Fire(player, "CombatFeedback", "empty", { weapon = def.id })
			return false
		end
		entry.magazine -= 1
		if (entry.magazine or 0) <= 0 then
			remotes.Fire(player, "Notify", "Empty. Reload.", "warn", 2)
		end
	end

	-- durability
	local itemDef = Items[entry.itemId]
	if itemDef and itemDef.dur then
		entry.dur = math.max(0, (entry.dur or itemDef.dur) - def.durability)
		if entry.dur <= 0 then
			profile.inventory.equipment[slot] = nil
			remotes.Fire(player, "Notify", "Your " .. itemDef.name .. " broke.", "danger", 3.4)
		end
	end

	local durabilityFactor = 1
	if itemDef and itemDef.dur and entry.dur then
		durabilityFactor = entry.dur / itemDef.dur
	end

	-- noise
	if registryRef.EnemyService then
		registryRef.EnemyService.EmitNoise(root.Position, def.noise or 30, player)
	end

	local shotOrigin = root.Position + Vector3.new(0, 1.4, 0)
	local results = {}
	local pellets = def.pellets or 1
	local params = raycastParamsFor(player, character)
	local aimDirection = CombatMath.spreadDirection(direction, def.spread or 0.01, rng)

	for i = 1, pellets do
		local dir = i == 1 and aimDirection or CombatMath.spreadDirection(direction, (def.spread or 0.01) * 1.2, rng)
		local reach = def.range or 100
		local hit = Workspace:Raycast(shotOrigin, dir * reach, params)
		if hit then
			local part = hit.Instance
			local model = part:FindFirstAncestorOfClass("Model")
			local partName = part.Name
			local bodyPart = CombatMath.pickBodyPart(partName, rng)
			local distance = (hit.Position - shotOrigin).Magnitude
			if part.Parent and part.Parent:FindFirstChildOfClass("Humanoid") then
				-- player victim
				local victimPlayer = Players:GetPlayerFromCharacter(part.Parent)
				if victimPlayer and victimPlayer ~= player then
					local damage = CombatMath.hitDamage(def, distance, bodyPart, nil, durabilityFactor)
					if registryRef.PartyService and registryRef.PartyService.SameParty(player, victimPlayer) then
						damage = damage * 0.25
					end
					registryRef.SurvivalService.ApplyDamage(victimPlayer, damage, bodyPart, {
						bleeding = def.kind == "firearm" and 0.45 or 0.2,
						infection = 0.05,
						cause = "Killed by " .. player.Name,
					})
					table.insert(results, { kind = "player", target = victimPlayer.Name, damage = damage })
					remotes.Fire(player, "CombatFeedback", "hit", { kind = "player", position = hit.Position, damage = damage })
				elseif not victimPlayer then
					-- NPC damage is resolved by the service that owns the creature
					local applied = 0
					if registryRef.EnemyService then
						applied = registryRef.EnemyService.DamageAt(
							part, def.damage * CombatMath.falloffMultiplier(def, distance),
							player, bodyPart, def) or 0
					end
					if applied and applied > 0 then
						table.insert(results, { kind = "enemy", damage = applied })
						remotes.Fire(player, "CombatFeedback", "hit", { kind = "enemy", position = hit.Position, damage = applied })
					end
				end
			else
				-- environment hit: sparks, no damage
				table.insert(results, { kind = "world", position = hit.Position })
				remotes.Fire(player, "CombatFeedback", "impact", {
					position = hit.Position, normal = hit.Normal, material = part.Material,
				})
			end
		end
	end

	remotes.Fire(player, "CombatFeedback", "shot", {
		weapon = def.id, origin = shotOrigin, direction = aimDirection,
		recoil = CombatMath.recoilImpulse(def, false, false),
		spread = def.spread, results = results,
	})
	if registryRef.QuestService and def.kind == "melee" then
		registryRef.QuestService.Event(player, { type = "melee_swing" })
	end
	return true
end

function CombatService.Reload(player)
	local profile = registryRef.PlayerDataService.Get(player)
	if not profile then return false end
	local entry, def = equippedWeapon(profile)
	if not entry or not def or def.kind ~= "firearm" then return false end
	if (entry.magazine or 0) >= def.magazine then return false end
	if (profile._reloadingUntil or 0) > os.clock() then return false end
	local available = Inventory.countItem(profile.inventory, def.ammo)
	if available <= 0 then
		remotes.Fire(player, "Notify", "No ammunition.", "warn", 2.2)
		return false
	end
	profile._reloadingUntil = os.clock() + def.reload
	remotes.Fire(player, "CombatFeedback", "reload_start", { weapon = def.id, duration = def.reload })
	task.delay(def.reload, function()
		if not player.Parent then return end
		local p = registryRef.PlayerDataService.Get(player)
		if not p then return end
		local e, d = equippedWeapon(p)
		if not e or not d or d.id ~= def.id then return end
		local need = d.magazine - (e.magazine or 0)
		local have = Inventory.countItem(p.inventory, d.ammo)
		local take = math.min(need, have)
		if take > 0 then
			Inventory.consumeItem(p.inventory, d.ammo, take)
			e.magazine = (e.magazine or 0) + take
		end
		remotes.Fire(player, "CombatFeedback", "reload_end", { weapon = d.id, magazine = e.magazine })
		remotes.Fire(player, "InventoryUpdate", Inventory.serialize(p.inventory))
	end)
	return true
end

-- Damage coming from an NPC (called by EnemyService)
function CombatService.DamagePlayer(victim, amount, opts)
	if registryRef.SurvivalService then
		return registryRef.SurvivalService.ApplyDamage(victim, amount, opts and opts.part or "Torso", {
			bleeding = opts and opts.bleeding or 0.25,
			infection = opts and opts.infection or 0.08,
			cause = opts and opts.cause or "Killed by a mutant",
		})
	end
	return 0
end

function CombatService.MarkDowned(player, downedState)
	downed[player.UserId] = downedState
end

function CombatService.IsDowned(player)
	return downed[player.UserId] == true
end

return CombatService
