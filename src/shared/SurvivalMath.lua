--!strict
--[[
	SurvivalMath — pure survival simulation.

	No Roblox services are touched here, which keeps the rules testable and makes
	the client able to predict the same numbers the server produces.
]]

local Config = require(script.Parent.Config)
local Util = require(script.Parent.Util)

local S = {}
local C = Config.Survival

S.EFFECTS = {
	bleeding = "Bleeding",
	fracture = "Fracture",
	infection = "Infection",
	radiation = "Radiation Sickness",
	cold = "Hypothermia",
	starving = "Starving",
	dehydrated = "Dehydrated",
	exhausted = "Exhausted",
	panic = "Panic Attack",
}

function S.newState()
	return {
		health = C.MaxHealth,
		hunger = 82,
		thirst = 78,
		stamina = 100,
		fatigue = 12,
		temperature = C.BaseTemperature,
		radiation = 0,
		infection = 0,
		bleeding = 0,
		stress = 10,
		bodyParts = { Head = 100, Torso = 100, LeftArm = 100, RightArm = 100, LeftLeg = 100, RightLeg = 100 },
		conditions = {},
		alive = true,
		causeOfDeath = nil,
		lastDamageAt = 0,
	}
end

function S.clampState(state)
	state.health = Util.clamp(state.health, 0, C.MaxHealth)
	state.hunger = Util.clamp(state.hunger, 0, 100)
	state.thirst = Util.clamp(state.thirst, 0, 100)
	state.stamina = Util.clamp(state.stamina, 0, 100)
	state.fatigue = Util.clamp(state.fatigue, 0, 100)
	state.radiation = Util.clamp(state.radiation, 0, 100)
	state.infection = Util.clamp(state.infection, 0, 100)
	state.bleeding = Util.clamp(state.bleeding, 0, 5)
	state.stress = Util.clamp(state.stress, 0, 100)
	state.temperature = Util.clamp(state.temperature, 28, 42.5)
	for part, hp in pairs(state.bodyParts) do
		state.bodyParts[part] = Util.clamp(hp, 0, 100)
	end
end

function S.deriveConditions(state)
	local cond = {}
	if state.bleeding >= 1 then
		cond.bleeding = true
	end
	if (state.bodyParts.LeftLeg < 35 and state.bodyParts.LeftLeg > 0)
		or (state.bodyParts.RightLeg < 35 and state.bodyParts.RightLeg > 0) then
		cond.fracture = true
	end
	if state.infection > 40 then
		cond.infection = true
	end
	if state.radiation > 45 then
		cond.radiation = true
	end
	if state.temperature < 35.2 then
		cond.cold = true
	end
	if state.hunger <= 0 then
		cond.starving = true
	end
	if state.thirst <= 0 then
		cond.dehydrated = true
	end
	if state.fatigue > 85 then
		cond.exhausted = true
	end
	if state.stress > Config.Stress.PanicThreshold then
		cond.panic = true
	end
	state.conditions = cond
	return cond
end

-- Speed multiplier from injuries and stats
function S.speedMultiplier(state, effects)
	local mul = 1.0
	local legs = (state.bodyParts.LeftLeg + state.bodyParts.RightLeg) / 2
	if legs < 100 then
		mul = mul * Util.lerp(Config.SurvivalEffects.BrokenLegSpeedMul, 1, legs / 100)
	end
	if state.stamina < 12 then
		mul = mul * Config.SurvivalEffects.LowStaminaSpeedMul
	end
	if state.thirst < 20 then
		mul = mul * Config.SurvivalEffects.LowHydrationSpeedMul
	end
	if state.temperature < 35.2 then
		mul = mul * Config.SurvivalEffects.ColdSpeedMul
	end
	if state.fatigue > 85 then
		mul = mul * 0.86
	end
	if state.bleeding >= 2 then
		mul = mul * 0.93
	end
	if effects and effects.painkiller then
		mul = mul * 1.04
	end
	return Util.clamp(mul, 0.32, 1.15)
end

function S.staminaDrainRate(state, activity)
	local drain = 0
	if activity == "sprint" then
		drain = C.StaminaDrainSprint
	elseif activity == "swim" then
		drain = C.StaminaDrainSprint * 0.7
	elseif activity == "climb" then
		drain = C.StaminaDrainSprint * 0.55
	elseif activity == "aim" then
		drain = 1.4
	end
	-- weak and injured survivors burn stamina faster
	drain = drain * (1 + (100 - state.health) / 260 + state.fatigue / 240)
	return drain
end

function S.staminaRegenRate(state, activity)
	if activity == "sprint" or activity == "climb" or activity == "swim" then
		return 0
	end
	local regen = C.StaminaRegen
	if activity == "crouch" or activity == "idle" then
		regen = C.StaminaRegenCrouch
	end
	if state.hunger < 25 then
		regen = regen * 0.55
	end
	if state.thirst < 25 then
		regen = regen * 0.6
	end
	if state.fatigue > 70 then
		regen = regen * 0.7
	end
	if state.temperature < 35.0 then
		regen = regen * 0.65
	end
	return regen
end

--[[
	Advance one tick.

	ctx = {
	  dt, ambientTemperature, regionRadiation, sprinting, crouching, moving,
	  indoors, isNight, threat (0..1), storm (bool), gear = { radiation = 0..1 },
	  activity = "sprint"|"walk"|"idle"|"crouch"|"climb"|"swim",
	}
	Returns a list of event strings for VFX/SFX/notifications.
]]
function S.step(state, ctx, dt)
	local events = {}
	local activity = ctx.activity or "idle"

	-- hunger / thirst -------------------------------------------------------
	local exertion = (activity == "sprint" and 1.9) or (activity == "climb" and 1.5)
		or (activity == "walk" and 1.0) or 0.55
	state.hunger -= C.HungerDrain * dt * exertion
	state.thirst -= C.ThirstDrain * dt * exertion
	if ctx.isNight then
		state.thirst -= C.ThirstDrain * 0.2 * dt
	end

	-- stamina ---------------------------------------------------------------
	if activity == "sprint" or activity == "climb" or activity == "swim" or activity == "aim" then
		state.stamina -= S.staminaDrainRate(state, activity) * dt
	else
		state.stamina += S.staminaRegenRate(state, activity) * dt
	end

	-- fatigue ---------------------------------------------------------------
	if activity == "idle" and ctx.indoors then
		state.fatigue -= 0.55 * dt
	else
		state.fatigue += C.FatigueDrain * dt * (ctx.isNight and 1.4 or 1)
	end

	-- temperature -----------------------------------------------------------
	local ambient = ctx.ambientTemperature or 10
	local insulation = 0
	if ctx.gear then
		insulation = (ctx.gear.cold or 0)
	end
	if ctx.indoors then
		ambient = ambient + 4
	end
	if ctx.fires and ctx.fires > 0 then
		ambient = ambient + 9 * math.min(1, ctx.fires)
	end
	local target = C.BaseTemperature + (ambient - 20) * 0.055
	target += insulation * 1.6
	local drift = (target - state.temperature) * C.TemperatureDrift * dt
	state.temperature += Util.clamp(drift, -1.6 * dt, 1.6 * dt)

	-- radiation -------------------------------------------------------------
	local exposure = (ctx.regionRadiation or 0) * 0.01
	exposure += ctx.storm and Config.Weather.StormRadiationPerSecond or 0
	local protection = 0
	if ctx.gear then
		protection = ctx.gear.radiation or 0
	end
	exposure = exposure * (1 - Util.clamp(protection, 0, 0.92))
	if exposure > 0.004 then
		state.radiation += exposure * C.RadiationGainScale * dt * 3.0
	else
		state.radiation -= C.RadiationDecay * dt
	end

	-- bleeding --------------------------------------------------------------
	if state.bleeding >= 1 then
		local bleedDamage = C.BleedDamagePerStack * state.bleeding * dt
		state.health -= bleedDamage
		if state.bleeding >= 3 and math.random() < dt * 0.4 then
			table.insert(events, "bloodDrop")
		end
	end

	-- infection -------------------------------------------------------------
	if state.infection > 0 then
		if state.infection > 60 then
			state.health -= (state.infection - 60) * 0.004 * dt
		end
		if state.infection > 30 then
			state.stamina -= 0.5 * dt
		end
	end

	-- starvation / dehydration ---------------------------------------------
	if state.hunger <= 0 then
		state.health -= C.StarveDamage * dt
		if math.random() < dt * 0.35 then table.insert(events, "stomachGrowl") end
	end
	if state.thirst <= 0 then
		state.health -= C.DehydrateDamage * dt
		if math.random() < dt * 0.35 then table.insert(events, "dryMouth") end
	end

	-- radiation sickness ----------------------------------------------------
	if state.radiation > 45 then
		local sev = (state.radiation - 45) / 55
		state.health -= sev * 0.22 * dt
		state.stamina -= sev * 1.2 * dt
		state.stress += sev * 0.6 * dt
		if state.radiation > 80 and math.random() < dt * 0.25 then
			table.insert(events, "radNausea")
		end
	end

	-- stress ----------------------------------------------------------------
	local stressDelta = -C.StressDecay * dt
	if ctx.isNight then stressDelta += Config.Stress.GainDark * dt end
	if ctx.alone then stressDelta += Config.Stress.GainAlone * dt end
	if (ctx.threat or 0) > 0 then
		stressDelta += Config.Stress.GainThreat * (ctx.threat or 0) * dt
	end
	if state.health < 35 then
		stressDelta += Config.Stress.GainLowHealth * dt
	end
	if state.fatigue > 85 then
		stressDelta += C.SleepDeprivationStress * dt
	end
	if not ctx.indoors and ctx.isNight and (ctx.threat or 0) <= 0 then
		stressDelta += 0.10 * dt
	end
	state.stress += stressDelta
	if state.stress > Config.Stress.PanicThreshold and math.random() < dt * 0.12 then
		table.insert(events, "panicBreath")
	end

	-- health regeneration ----------------------------------------------------
	if state.hunger > 55 and state.thirst > 55 and state.bleeding < 1
		and state.radiation < 25 and state.health < C.MaxHealth and (ctx.threat or 0) <= 0 then
		state.health += 0.9 * dt
	end

	-- body part recovery -----------------------------------------------------
	for part, hp in pairs(state.bodyParts) do
		if hp > 0 and hp < 100 and state.hunger > 40 and state.thirst > 40 then
			state.bodyParts[part] = math.min(100, hp + 0.18 * dt)
		end
	end

	S.clampState(state)
	S.deriveConditions(state)

	if state.health <= 0 and state.alive then
		state.alive = false
		state.causeOfDeath = S.describeDeath(state)
		table.insert(events, "death")
	end
	return events
end

function S.describeDeath(state)
	if state.radiation > 60 then return "Acute radiation syndrome" end
	if state.thirst <= 0 then return "Dehydration" end
	if state.hunger <= 0 then return "Starvation" end
	if state.bleeding >= 1 then return "Blood loss" end
	if state.infection > 50 then return "Sepsis" end
	if state.temperature < 34.5 then return "Hypothermia" end
	if state.lastDamageCause then return state.lastDamageCause end
	return "Exposure"
end

-- Apply a single hit to a body part, returns actual damage dealt.
function S.applyHit(state, part, rawDamage, opts)
	opts = opts or {}
	part = part or "Torso"
	local mult = Config.BodyPartDamageMultiplier[part] or 1
	local armor = 0
	if opts.armor then
		local key = "torso"
		if part == "Head" then key = "head"
		elseif part == "LeftArm" or part == "RightArm" then key = "arms"
		elseif part == "LeftLeg" or part == "RightLeg" then key = "legs"
		end
		armor = opts.armor[key] or 0
	end
	local damage = rawDamage * mult * (1 - Util.clamp(armor, 0, 0.85))
	state.health -= damage
	state.bodyParts[part] = math.max(0, (state.bodyParts[part] or 100) - damage * 1.5)
	if opts.bleeding and math.random() < (opts.bleeding or 0) then
		state.bleeding = math.min(5, state.bleeding + 1)
	end
	if opts.infection and math.random() < opts.infection then
		state.infection = math.min(100, state.infection + 12)
	end
	state.lastDamageAt = os.clock()
	state.lastDamageCause = opts.cause
	S.clampState(state)
	return damage
end

-- Consume / medical effects (shared by server and client preview)
function S.applyItemEffect(state, def, effects)
	effects = effects or {}
	if def.use then
		state.hunger += def.use.hunger or 0
		state.thirst += def.use.thirst or 0
		state.stamina += def.use.stamina or 0
		state.health += def.use.health or 0
		state.radiation += def.use.radiation or 0
		state.infection += def.use.infection or 0
		state.stress += def.use.sanity or 0
		if def.use.bleeding then
			-- a treatment worth -100% stops all bleeding; partial values remove
			-- proportionally (a bandage slows it, a medkit ends it)
			if def.use.bleeding <= -100 then
				state.bleeding = 0
			else
				state.bleeding = math.max(0, state.bleeding + def.use.bleeding / 100 * 3)
			end
		end
		if def.use.fracture then
			effects.splint = true
		end
		if def.use.fatigue then
			state.fatigue = state.fatigue + def.use.fatigue
		end
		if def.use.effect then
			effects[def.use.effect] = (effects[def.use.effect] or 0) + 12
		end
	end
	S.clampState(state)
	S.deriveConditions(state)
	return state
end

function S.statusLine(state)
	local cond = state.conditions or S.deriveConditions(state)
	local out = {}
	for key in pairs(cond) do
		table.insert(out, S.EFFECTS[key] or key)
	end
	table.sort(out)
	return out
end

function S.temperatureLabel(state)
	local t = state.temperature
	if t < 34.5 then return "Hypothermic", Color3.fromRGB(120, 170, 220) end
	if t < 35.8 then return "Cold", Color3.fromRGB(150, 190, 220) end
	if t < 37.6 then return "Normal", Color3.fromRGB(180, 210, 190) end
	if t < 38.6 then return "Feverish", Color3.fromRGB(225, 180, 110) end
	return "Fever", Color3.fromRGB(230, 120, 100)
end

return S
