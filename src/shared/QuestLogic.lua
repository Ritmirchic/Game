--!strict
--[[
	QuestLogic — pure quest progression engine.

	questState = {
	  active = { [questId] = { objectives = { [objId] = { progress, done } }, startedAt, complete } },
	  completed = { [questId] = timestamp },
	  unlocked = { [questId] = true },
	  counters = { kills = { [enemyId] = n }, crafts = { [recipeId] = n }, builds = { [pieceId] = n } },
	}
	Events are plain tables: { type = "kill", id = "hollow" }
]]

local Quests = require(script.Parent.QuestDefinitions)

local QL = {}

function QL.newState()
	return { active = {}, completed = {}, unlocked = {}, counters = { kills = {}, crafts = {}, builds = {} } }
end

function QL.isActive(state, questId)
	return state.active[questId] ~= nil
end

function QL.isComplete(state, questId)
	return state.completed[questId] ~= nil and state.completed[questId] ~= false
end

function QL.canStart(state, quest)
	if QL.isActive(state, quest.id) or QL.isComplete(state, quest.id) then
		return false
	end
	return state.unlocked[quest.id] == true
end

function QL.start(state, quest, now)
	if not QL.canStart(state, quest) then return false end
	local objectives = {}
	for _, obj in ipairs(quest.objectives) do
		objectives[obj.id] = { progress = 0, done = (obj.count or 0) == 0 }
	end
	state.active[quest.id] = { objectives = objectives, startedAt = now or 0, complete = false }
	return true
end

-- Force a quest active regardless of unlock (used by the main line).
function QL.forceStart(state, quest, now)
	if QL.isComplete(state, quest.id) then return false end
	if QL.isActive(state, quest.id) then return false end
	local objectives = {}
	for _, obj in ipairs(quest.objectives) do
		objectives[obj.id] = { progress = 0, done = false }
	end
	state.active[quest.id] = { objectives = objectives, startedAt = now or 0, complete = false }
	state.unlocked[quest.id] = true
	return true
end

function QL.incrementCounter(state, kind, id, amount)
	local bucket = state.counters[kind]
	if not bucket then
		bucket = {}
		state.counters[kind] = bucket
	end
	bucket[id] = (bucket[id] or 0) + (amount or 1)
	return bucket[id]
end

local function objectiveSatisfied(obj, entry, state, context)
	if entry.done then return true end
	local progress = entry.progress or 0
	if obj.type == "collect" then
		local have = 0
		if context and context.countItem then
			have = context.countItem(obj.item)
		end
		entry.progress = math.min(obj.count or 1, have)
		return entry.progress >= (obj.count or 1)
	elseif obj.type == "kill" or obj.type == "reach" or obj.type == "interact"
		or obj.type == "craft" or obj.type == "build" then
		return progress >= (obj.count or 1)
	elseif obj.type == "survive" then
		return progress >= (obj.seconds or 60)
	end
	return false
end

--[[
	Feed an event (or nil to just refresh collect objectives).
	Returns { changed = bool, completedQuest = bool, quest = def }
]]
function QL.update(state, quest, event, context)
	local active = state.active[quest.id]
	if not active then return { changed = false } end
	local changed = false

	for _, obj in ipairs(quest.objectives) do
		local entry = active.objectives[obj.id]
		if entry and not entry.done then
			if event then
				if obj.type == "kill" and event.type == "kill" and event.id == obj.enemy then
					entry.progress = math.min(obj.count or 1, (entry.progress or 0) + (event.amount or 1))
				elseif obj.type == "interact" and event.type == "interact" then
					local kindOk = (obj.kind == nil) or (event.kind == obj.kind)
					local targetOk = (obj.target == nil) or (event.target == obj.target)
					if kindOk and targetOk then
						entry.progress = (obj.count or 1)
					end
				elseif obj.type == "craft" and event.type == "craft" and event.id == obj.recipe then
					entry.progress = (obj.count or 1)
				elseif obj.type == "build" and event.type == "build" and event.id == obj.piece then
					entry.progress = math.min(obj.count or 1, (entry.progress or 0) + (event.amount or 1))
				elseif obj.type == "reach" and event.type == "reach" then
					if obj.region then
						if event.region == obj.region then
							entry.progress = (obj.count or 1)
						end
					elseif obj.x and event.position then
						local dx = event.position.X - obj.x
						local dz = event.position.Z - obj.z
						if math.sqrt(dx * dx + dz * dz) <= (obj.radius or 80) then
							entry.progress = (obj.count or 1)
						end
					end
				elseif obj.type == "survive" and event.type == "survive" then
					entry.progress = (entry.progress or 0) + (event.amount or 0)
				end
			end
			if objectiveSatisfied(obj, entry, state, context) then
				entry.done = true
				changed = true
			elseif event then
				changed = true
			end
		end
	end

	local allDone = true
	for _, obj in ipairs(quest.objectives) do
		local entry = active.objectives[obj.id]
		if not entry or not entry.done then
			allDone = false
			break
		end
	end
	if allDone then
		active.complete = true
		return { changed = true, completedQuest = true, quest = quest }
	end
	return { changed = changed, completedQuest = false, quest = quest }
end

function QL.complete(state, quest, now)
	state.active[quest.id] = nil
	state.completed[quest.id] = now or true
	for _, unlock in ipairs(quest.rewards and quest.rewards.unlock or {}) do
		state.unlocked[unlock] = true
	end
	return true
end

function QL.progressSummary(state, quest)
	local active = state.active[quest.id]
	if not active then return nil end
	local total, done = 0, 0
	for _, obj in ipairs(quest.objectives) do
		total += 1
		local entry = active.objectives[obj.id]
		if entry and entry.done then done += 1 end
	end
	return done, total, active
end

function QL.currentObjective(state, quest)
	local active = state.active[quest.id]
	if not active then return nil end
	for _, obj in ipairs(quest.objectives) do
		local entry = active.objectives[obj.id]
		if entry and not entry.done then
			return obj, entry
		end
	end
	return nil
end

function QL.serialize(state)
	return {
		active = state.active,
		completed = state.completed,
		unlocked = state.unlocked,
		counters = state.counters,
	}
end

function QL.deserialize(data)
	local state = QL.newState()
	if type(data) ~= "table" then return state end
	for qid, entry in pairs(data.active or {}) do
		if Quests[qid] and type(entry) == "table" then
			state.active[qid] = entry
		end
	end
	for qid, done in pairs(data.completed or {}) do
		if Quests[qid] then
			state.completed[qid] = done
		end
	end
	for qid, unlocked in pairs(data.unlocked or {}) do
		if Quests[qid] and unlocked then
			state.unlocked[qid] = true
		end
	end
	for kind, bucket in pairs(data.counters or {}) do
		if type(bucket) == "table" then
			state.counters[kind] = bucket
		end
	end
	return state
end

return QL
