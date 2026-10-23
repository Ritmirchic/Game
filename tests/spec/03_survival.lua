-- Survival simulation: resource drain, temperature, radiation, death.

local S = require("SurvivalMath")
local Config = require("Config")

local function baseCtx(overrides)
	local ctx = {
		ambientTemperature = 12,
		regionRadiation = 0,
		sprinting = false,
		crouching = false,
		moving = false,
		indoors = false,
		isNight = false,
		threat = 0,
		alone = true,
		storm = false,
		gear = { cold = 0, torso = 0 },
		fires = 0,
		activity = "idle",
	}
	for key, value in pairs(overrides or {}) do
		ctx[key] = value
	end
	return ctx
end

describe("SurvivalMath: initial state", function()
	it("creates a valid state", function()
		local state = S.newState()
		eq(state.health, Config.Survival.MaxHealth)
		eq(state.alive, true)
		expect(state.hunger > 0 and state.hunger <= 100)
		expect(state.thirst > 0 and state.thirst <= 100)
		expect(state.stamina > 0 and state.stamina <= 100)
		near(state.temperature, 36.6, 0.5)
	end)

	it("clamps hostile values", function()
		local state = S.newState()
		state.health = 500
		state.hunger = -40
		state.radiation = 300
		S.clampState(state)
		eq(state.health, 100)
		eq(state.hunger, 0)
		eq(state.radiation, 100)
	end)
end)

describe("SurvivalMath: drain", function()
	it("sprinting costs more stamina than standing", function()
		local state = S.newState()
		S.step(state, baseCtx({ activity = "sprint" }), 5)
		local afterSprint = state.stamina
		local idle = S.newState()
		S.step(idle, baseCtx({ activity = "idle" }), 5)
		expect(afterSprint < idle.stamina, "sprint did not cost stamina")
		expect(afterSprint < 100)
	end)

	it("stamina regenerates while resting and never exceeds 100", function()
		local state = S.newState()
		state.stamina = 20
		for _ = 1, 40 do
			S.step(state, baseCtx({ activity = "idle", indoors = true }), 1)
		end
		expect(state.stamina > 20)
		expect(state.stamina <= 100)
	end)

	it("hunger and thirst fall over a simulated hour", function()
		local state = S.newState()
		for _ = 1, 3600 do
			S.step(state, baseCtx({ activity = "walk" }), 1)
		end
		expect(state.hunger < 80, "hunger did not drop: " .. state.hunger)
		expect(state.thirst < 80, "thirst did not drop: " .. state.thirst)
	end)

	it("starvation eventually damages health", function()
		local state = S.newState()
		state.hunger = 0.5
		state.thirst = 50
		for _ = 1, 120 do
			S.step(state, baseCtx(), 1)
		end
		expect(state.health < 100, "no starvation damage applied")
	end)
end)

describe("SurvivalMath: environment", function()
	it("cold weather cools the body", function()
		local state = S.newState()
		for _ = 1, 90 do
			S.step(state, baseCtx({ ambientTemperature = -6, indoors = false }), 1)
		end
		expect(state.temperature < 36.6, "body did not cool: " .. state.temperature)
	end)

	it("a fire warms the body back up", function()
		local state = S.newState()
		state.temperature = 34.5
		for _ = 1, 60 do
			S.step(state, baseCtx({ ambientTemperature = 4, fires = 1 }), 1)
		end
		expect(state.temperature > 34.5)
	end)

	it("radiation accumulates in a hot zone and decays elsewhere", function()
		local state = S.newState()
		for _ = 1, 60 do
			S.step(state, baseCtx({ regionRadiation = 46 }), 1)
		end
		local peak = state.radiation
		expect(peak > 0, "no radiation gained")
		for _ = 1, 120 do
			S.step(state, baseCtx({ regionRadiation = 0 }), 1)
		end
		expect(state.radiation < peak, "radiation never decayed")
	end)

	it("is night affects thirst and stress", function()
		local day = S.newState()
		local night = S.newState()
		S.step(day, baseCtx({ isNight = false }), 30)
		S.step(night, baseCtx({ isNight = true }), 30)
		expect(night.thirst < day.thirst)
	end)
end)

describe("SurvivalMath: injury and death", function()
	it("a head hit removes more health than a leg hit", function()
		local head = S.newState()
		local leg = S.newState()
		S.applyHit(head, "Head", 20, {})
		S.applyHit(leg, "LeftLeg", 20, {})
		expect(head.health < leg.health, "head damage was not worse")
	end)

	it("armor reduces incoming damage", function()
		local bare = S.newState()
		local armored = S.newState()
		S.applyHit(bare, "Torso", 30, {})
		S.applyHit(armored, "Torso", 30, { armor = { torso = 0.5 } })
		expect(armored.health > bare.health)
	end)

	it("bleeding persists and drains health", function()
		local state = S.newState()
		state.bleeding = 3
		S.step(state, baseCtx(), 10)
		expect(state.health < 100, "bleeding did not damage")
	end)

	it("health reaching zero kills the survivor", function()
		local state = S.newState()
		state.health = 1
		state.hunger = 0
		state.thirst = 0
		S.step(state, baseCtx(), 10)
		eq(state.alive, false)
		expect(type(S.describeDeath(state)) == "string")
	end)

	it("derived conditions reflect the state", function()
		local state = S.newState()
		state.bleeding = 2
		state.infection = 60
		state.temperature = 33.5
		local conditions = S.deriveConditions(state)
		expect(conditions.bleeding == true, "bleeding not derived")
		expect(conditions.infection == true, "infection not derived")
		expect(conditions.cold == true, "hypothermia not derived")
	end)

	it("speed multiplier degrades when injured", function()
		local state = S.newState()
		local baseline = S.speedMultiplier(state, {})
		state.health = 15
		state.stamina = 2
		state.conditions = { leg_injury = true }
		local hurt = S.speedMultiplier(state, {})
		expect(hurt < baseline, "injury did not slow the survivor")
		expect(hurt > 0, "speed must stay positive")
	end)
end)

describe("SurvivalMath: consumables", function()
	it("food and water restore the right resources", function()
		local state = S.newState()
		state.hunger = 20
		state.thirst = 20
		S.applyItemEffect(state, { use = { kind = "food", hunger = 30 } })
		expect(state.hunger > 20)
		S.applyItemEffect(state, { use = { kind = "drink", thirst = 30 } })
		expect(state.thirst > 20)
	end)

	it("medical items can stop bleeding", function()
		local state = S.newState()
		state.bleeding = 4
		S.applyItemEffect(state, { use = { kind = "medical", bleeding = -100, health = 10 } })
		eq(state.bleeding, 0)
	end)
end)
