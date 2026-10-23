--!strict
--[[
	StateStore — client-side replicated state with change signals.

	Everything the UI shows comes from here. The store is written only by the
	network layer (Main.client.lua) so there is a single source of truth.
]]

local StateStore = {}

local state = {
	survival = {
		health = 100, hunger = 80, thirst = 80, stamina = 100, fatigue = 10,
		temperature = 36.6, radiation = 0, infection = 0, bleeding = 0, stress = 5,
		bodyParts = {}, conditions = {}, region = "Suburbs", alive = true,
	},
	inventory = { containers = {}, equipment = {}, weight = 0, capacity = 22 },
	quests = { active = {}, completed = {}, unlocked = {} },
	weather = { kind = "Overcast", clock = 17.4, day = 1, rain = 0, storm = false, foggy = false },
	profile = nil,
	notifications = {},
	enemies = 0,
	serverInfo = nil,
	settings = { quality = "High", sensitivity = 0.55, fov = 72, headBob = true, shake = true, invertY = false },
	flashlight = false,
	aiming = false,
	sprinting = false,
	crouching = false,
	interactionTarget = nil,
	openLoot = nil,
	dead = false,
}

local listeners = {}

local function emit(key)
	local list = listeners[key]
	if list then
		for _, fn in ipairs(list) do
			local ok, err = pcall(fn, state[key], state)
			if not ok then
				warn("[StateStore] listener error for " .. key .. ": " .. tostring(err))
			end
		end
	end
	local any = listeners["*"]
	if any then
		for _, fn in ipairs(any) do
			pcall(fn, state[key], key, state)
		end
	end
end

function StateStore.on(key, fn)
	listeners[key] = listeners[key] or {}
	table.insert(listeners[key], fn)
	return function()
		local list = listeners[key]
		for index, item in ipairs(list or {}) do
			if item == fn then
				table.remove(list, index)
				break
			end
		end
	end
end

function StateStore.get(key)
	if key then return state[key] end
	return state
end

function StateStore.set(key, value)
	state[key] = value
	emit(key)
end

function StateStore.merge(key, patch)
	local target = state[key]
	if type(target) ~= "table" then
		state[key] = patch
	else
		for k, v in pairs(patch) do
			target[k] = v
		end
	end
	emit(key)
end

function StateStore.applySnapshot(snapshot)
	if type(snapshot) ~= "table" then return end
	if snapshot.survival then
		for key, value in pairs(snapshot.survival) do
			state.survival[key] = value
		end
		emit("survival")
	end
	if snapshot.inventory then
		state.inventory = snapshot.inventory
		emit("inventory")
	end
	if snapshot.quests then
		state.quests = snapshot.quests
		emit("quests")
	end
	if snapshot.settings then
		for key, value in pairs(snapshot.settings) do
			state.settings[key] = value
		end
		emit("settings")
	end
	state.profile = snapshot
	emit("profile")
end

function StateStore.notify(text, kind, duration)
	local entry = { text = text, kind = kind or "info", expiresAt = os.clock() + (duration or 3) }
	table.insert(state.notifications, entry)
	if #state.notifications > 6 then
		table.remove(state.notifications, 1)
	end
	emit("notifications")
end

function StateStore.pruneNotifications()
	local now = os.clock()
	local changed = false
	for index = #state.notifications, 1, -1 do
		if state.notifications[index].expiresAt < now then
			table.remove(state.notifications, index)
			changed = true
		end
	end
	if changed then
		emit("notifications")
	end
end

function StateStore.conditionList()
	local out = {}
	for key in pairs(state.survival.conditions or {}) do
		table.insert(out, key)
	end
	table.sort(out)
	return out
end

return StateStore
