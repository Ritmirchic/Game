--!strict
--[[
	EnemyAI — perception + decision state machine (pure logic).

	The server's EnemyService owns the Roblox side (models, pathfinding, damage);
	this module owns *decisions*, so the behaviour is deterministic and testable.

	memory = {
	  state      = "Idle"|"Patrol"|"Investigate"|"Search"|"Chase"|"Attack"|"Retreat"|"Return"|"Stalk"|"Burrow"
	  target     = id/nil, lastSeen (Vector3|nil), lastSeenAt, alert (0..1), patrolIndex,
	  home       = Vector3, stateTime, cooldowns = {...}, noise = 0
	}
	perception = {
	  dt, position, direction, target = { id, position, isSprinting, isCrouching, lightOn, player },
	  hasLineOfSight, distance, owners (number of nearby survivors), health (0..1),
	  night (bool), noiseEvents = { {position, loudness} }, time
	}
]]

local Enemies = require(script.Parent.EnemyDefinitions)
local Util = require(script.Parent.Util)

local AI = {}

AI.States = {
	IDLE = "Idle", PATROL = "Patrol", INVESTIGATE = "Investigate", SEARCH = "Search",
	CHASE = "Chase", ATTACK = "Attack", RETREAT = "Retreat", RETURN = "Return",
	STALK = "Stalk", BURROW = "Burrow",
}

function AI.newMemory(home)
	return {
		state = AI.States.IDLE,
		target = nil,
		lastSeen = nil,
		lastSeenAt = 0,
		alert = 0,
		patrolIndex = 1,
		home = home or Vector3.new(0, 0, 0),
		stateTime = 0,
		noise = 0,
		lastNoise = nil,
		cooldowns = {},
		searchPoint = nil,
		investigatePoint = nil,
	}
end

local function canSee(perception, def)
	if perception.hasLineOfSight and perception.distance <= def.vision then
		return true
	end
	return false
end

local function effectiveVision(perception, def)
	local vision = def.vision
	if perception.night then
		vision *= 0.72
	end
	if perception.target and perception.target.isCrouching then
		vision *= 0.75
	end
	if perception.target and perception.target.lightOn then
		vision *= 1.45
	end
	if perception.foggy then
		vision *= 0.65
	end
	return vision
end

local function hearingScore(perception, def)
	local best = 0
	for _, evt in ipairs(perception.noiseEvents or {}) do
		local d = (evt.position - perception.position).Magnitude
		local reach = (evt.loudness or 20) * def.hearing
		if d < reach then
			local score = 1 - d / math.max(reach, 1)
			if score > best then
				best = score
				perception._heard = evt.position
			end
		end
	end
	return best
end

local function setState(memory, state, time)
	if memory.state ~= state then
		memory.state = state
		memory.stateTime = time or 0
		memory.changed = true
	end
end

--[[
	Advance the AI one tick.
	Returns an action table:
	  { state, moveTo = Vector3|nil, sprint = bool, attack = "melee"|"burst"|nil,
	    face = Vector3|nil, sound = string|nil, flee = bool }
]]
function AI.update(def, memory, perception)
	local now = perception.time or 0
	local action = { state = memory.state, moveTo = nil, sprint = false, attack = nil, sound = nil }
	memory.changed = false
	memory.stateTime = memory.stateTime or now

	local vision = effectiveVision(perception, def)
	local sees = perception.hasLineOfSight and perception.distance <= vision
	local heard = hearingScore(perception, def)
	local blinded = def.behaviour and def.behaviour.blind
	if blinded then
		sees = false
	end

	-- stimuli ---------------------------------------------------------------
	if sees then
		memory.lastSeen = perception.target and perception.target.position or nil
		memory.lastSeenAt = now
		memory.alert = math.min(1, (memory.alert or 0) + perception.dt * 2.4)
	elseif heard > 0 then
		memory.investigatePoint = perception._heard or nil
		memory.alert = math.min(1, (memory.alert or 0) + perception.dt * (0.7 + heard))
	else
		memory.alert = math.max(0, (memory.alert or 0) - perception.dt * 0.22)
	end

	local targetPos = memory.lastSeen
	local distanceToTarget = perception.distance

	-- night-only creatures sleep in daylight
	if def.nocturnal and not perception.night then
		setState(memory, AI.States.IDLE, now)
		action.state = memory.state
		if perception.distance < 40 then
			memory.alert = math.min(1, (memory.alert or 0) + perception.dt * 0.5)
		end
		return action
	end

	-- watchers never commit to a fight; they stare and withdraw
	local beh = def.behaviour or {}
	if beh.observer then
		if sees and distanceToTarget < def.vision then
			setState(memory, AI.States.STALK, now)
			action.state = memory.state
			action.face = perception.target and perception.target.position or nil
			if (memory.cooldowns.stare or 0) <= now then
				memory.cooldowns.stare = now + 6
				action.sound = "watcher_stare"
			end
			if distanceToTarget < (beh.fleeOnApproach or 40) then
				local away = (perception.position - targetPos)
				if away.Magnitude < 0.1 then away = Vector3.new(1, 0, 0) end
				action.moveTo = perception.position + away.Unit * 30
				action.sprint = true
				action.flee = true
			end
			return action
		else
			setState(memory, AI.States.IDLE, now)
			action.state = memory.state
			return action
		end
	end

	-- low health retreat (Stalker)
	local healthFrac = perception.health or 1
	if beh.fleeBelowHealth and healthFrac < beh.fleeBelowHealth and memory.state ~= AI.States.RETREAT then
		setState(memory, AI.States.RETREAT, now)
		memory.cooldowns.retreatUntil = now + (beh.retreatTime or 6)
	end

	-- burrowers hide below ground until prey is close
	if beh.burrow and memory.state == AI.States.IDLE then
		if distanceToTarget < (beh.burrowTrigger or 34) and perception.target then
			setState(memory, AI.States.BURROW, now)
		end
	end

	local state = memory.state

	if state == AI.States.RETREAT then
		if now >= (memory.cooldowns.retreatUntil or 0) then
			setState(memory, AI.States.RETURN, now)
		else
			local away = perception.position - (targetPos or perception.position)
			if away.Magnitude < 0.1 then away = Vector3.new(1, 0, 0) end
			action.moveTo = perception.position + away.Unit * 26
			action.sprint = true
			action.flee = true
		end
	elseif state == AI.States.BURROW then
		action.state = state
		if distanceToTarget < def.attackRange + 4 and perception.target then
			setState(memory, AI.States.ATTACK, now)
			action.attack = "erupt"
		elseif distanceToTarget < (beh.burrowTrigger or 34) then
			action.moveTo = perception.target and perception.target.position or nil
			action.sprint = true
		elseif (now - memory.stateTime) > 9 then
			setState(memory, AI.States.SEARCH, now)
		end
	elseif memory.alert >= 0.85 and (sees or (memory.lastSeen and (now - memory.lastSeenAt) < 8)) then
		-- CHASE / ATTACK
		if perception.target and distanceToTarget <= def.attackRange then
			setState(memory, AI.States.ATTACK, now)
			action.attack = "melee"
			action.face = perception.target.position
		else
			setState(memory, AI.States.CHASE, now)
			action.moveTo = targetPos or (perception.target and perception.target.position)
			action.sprint = true
			-- special behaviours during a chase
			if beh.charge and distanceToTarget > 30 and (memory.cooldowns.charge or 0) <= now then
				memory.cooldowns.charge = now + 12
				action.sound = "brute_charge"
				action.charge = true
			end
		end
	elseif memory.alert >= 0.4 then
		if memory.investigatePoint then
			setState(memory, AI.States.INVESTIGATE, now)
			action.moveTo = memory.investigatePoint
			if (perception.position - memory.investigatePoint).Magnitude < 8 then
				memory.investigatePoint = nil
				setState(memory, AI.States.SEARCH, now)
			end
		else
			setState(memory, AI.States.SEARCH, now)
			if (now - memory.stateTime) > 6 then
				local angle = (now * 2.3) % (math.pi * 2)
				local radius = 22
				action.moveTo = perception.position + Vector3.new(math.cos(angle) * radius, 0, math.sin(angle) * radius)
			end
		end
	elseif memory.alert > 0.08 then
		setState(memory, AI.States.INVESTIGATE, now)
		action.moveTo = memory.investigatePoint or memory.lastSeen
		action.sprint = false
	else
		-- calm: patrol around home
		if (perception.position - memory.home).Magnitude > 90 then
			setState(memory, AI.States.RETURN, now)
			action.moveTo = memory.home
		else
			setState(memory, AI.States.PATROL, now)
			if (now - memory.stateTime) > 7 then
				memory.stateTime = now
				memory.patrolIndex = (memory.patrolIndex or 1) + 1
				memory.patrolTarget = memory.home + Vector3.new(
					math.sin(memory.patrolIndex * 1.7) * 40, 0, math.cos(memory.patrolIndex * 2.3) * 40)
			end
			action.moveTo = memory.patrolTarget or memory.home
		end
	end

	-- mimic behaviour: lure players instead of charging when far away
	if beh.mimic and memory.state ~= AI.States.ATTACK and memory.state ~= AI.States.CHASE then
		if (memory.cooldowns.lure or 0) <= now and perception.target then
			memory.cooldowns.lure = now + (beh.lureCooldown or 18)
			action.sound = "mimic_lure"
		end
	end

	-- screamers call the horde when they find a target
	if beh.caller and sees and (memory.cooldowns.call or 0) <= now then
		memory.cooldowns.call = now + (beh.callCooldown or 22)
		action.sound = "screamer_call"
		action.callHorde = true
	end

	-- solitude hunters hesitate when targets are grouped
	if beh.solitudeHunter and perception.target then
		local owners = perception.owners or 1
		if owners > 1 then
			memory.alert = math.max(0, memory.alert - perception.dt * 0.5)
			action.hesitate = true
		end
	end

	action.state = memory.state
	if action.moveTo and not memory.lastSeen and memory.state ~= AI.States.INVESTIGATE then
		-- keep remembered target position fresh while chasing
		memory.moveGoal = action.moveTo
	end
	return action
end

function AI.describe(memory)
	return string.format("%s (alert %.2f)", memory.state, memory.alert or 0)
end

function AI.shouldDespawn(perception, memory)
	if perception.distance > 520 and (memory.state == AI.States.PATROL or memory.state == AI.States.IDLE) then
		return true
	end
	return false
end

function AI.spawnWeights(regionName, tier, night)
	local list = {}
	for _, def in pairs(Enemies) do
		if type(def) == "table" then
			local inRegion = false
			for _, r in ipairs(def.habitat) do
				if r == regionName then
					inRegion = true
					break
				end
			end
			local inTier = false
			for _, t in ipairs(def.tiers) do
				if t == tier then
					inTier = true
					break
				end
			end
			if inRegion and inTier and (not def.nocturnal or night) then
				local weight = 10 - def.tiers[1] * 1.5
				table.insert(list, { def = def, weight = math.max(1, weight) })
			end
		end
	end
	return list
end

return AI
