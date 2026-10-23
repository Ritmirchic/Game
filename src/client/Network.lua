--!strict
--[[
	Network — the only module allowed to talk to the server.

	Wraps RemoteEvents in a small API with local access guards, and routes
	incoming messages into StateStore. Controllers call Network.* rather than
	touching remotes directly.
]]

local ReplicatedStorage = game:GetService("ReplicatedStorage")

local Store = require(script.Parent.StateStore)
local RemoteList = require(ReplicatedStorage:WaitForChild("Shared"):WaitForChild("Remotes"))

local Client = {}

local folder = ReplicatedStorage:WaitForChild(RemoteList.FolderName)
local events = {}
local functions = {}

for _, name in ipairs(RemoteList.Events) do
	local remote = folder:WaitForChild(name)
	events[name] = remote
end
for _, name in ipairs(RemoteList.Functions) do
	functions[name] = folder:WaitForChild(name)
end

Client.Events = events
Client.Functions = functions

local lastSent = {}
local MIN_INTERVAL = {
	RequestAttack = 0.08,
	RequestInteract = 0.15,
	RequestBuild = 0.2,
	RequestPing = 1.5,
	RequestSettings = 0.5,
}

function Client.fire(name, ...)
	local remote = events[name]
	if not remote then
		warn("[Network] unknown event " .. tostring(name))
		return
	end
	local now = os.clock()
	local gate = MIN_INTERVAL[name] or 0.04
	if lastSent[name] and now - lastSent[name] < gate then
		return
	end
	lastSent[name] = now
	remote:FireServer(...)
end

function Client.invoke(name, ...)
	local remote = functions[name]
	if not remote then return nil end
	local ok, result = pcall(function(...)
		return remote:InvokeServer(...)
	end, ...)
	if ok then return result end
	return nil
end

function Client.bindRemoteHooks(context)
	events.SurvivalUpdate.OnClientEvent:Connect(function(survival)
		if type(survival) == "table" then
			Store.merge("survival", survival)
		end
	end)
	events.InventoryUpdate.OnClientEvent:Connect(function(inventory)
		if type(inventory) == "table" then
			Store.set("inventory", inventory)
		end
	end)
	events.EquipmentUpdate.OnClientEvent:Connect(function(equipment)
		if type(equipment) == "table" then
			local state = Store.get("inventory")
			state.equipment = equipment
			Store.set("inventory", state)
		end
	end)
	events.QuestUpdate.OnClientEvent:Connect(function(quests)
		if type(quests) == "table" then
			Store.set("quests", quests)
		end
	end)
	events.StateSync.OnClientEvent:Connect(function(snapshot)
		Store.applySnapshot(snapshot)
	end)
	events.Notify.OnClientEvent:Connect(function(text, kind, duration)
		Store.notify(text, kind, duration)
	end)
	events.Atmosphere.OnClientEvent:Connect(function(weather)
		if type(weather) == "table" then
			Store.merge("weather", weather)
		end
	end)
	events.ServerTime.OnClientEvent:Connect(function(clock, day)
		local weather = Store.get("weather")
		weather.clock = clock
		if day then weather.day = day end
		Store.set("weather", weather)
	end)
	if context and context.onEvent then
		context.onEvent(events)
	end
end

return Client
