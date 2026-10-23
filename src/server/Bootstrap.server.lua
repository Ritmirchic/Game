--!strict
--[[
	ASHES OF HUMANITY: LAST SIGNAL — server bootstrap.

	Loads every service module in a deterministic order, wires up remotes and
	starts the world. Individual services expose :Init() and :Start().
]]

local ServerScriptService = game:GetService("ServerScriptService")
local ReplicatedStorage = game:GetService("ReplicatedStorage")
local Players = game:GetService("Players")

local Services = ServerScriptService:WaitForChild("Services")

local ORDER = {
	"Remotes",
	"ConfigService",
	"PlayerDataService",
	"InventoryService",
	"SurvivalService",
	"LootService",
	"InteractionService",
	"CombatService",
	"CraftingService",
	"BuildingService",
	"QuestService",
	"PartyService",
	"EnemyService",
	"WeatherService",
	"WorldEventService",
	"StoryService",
	"DirectorService",
}

local loaded = {}
local loadOrder = {}

for _, name in ipairs(ORDER) do
	local moduleScript = Services:FindFirstChild(name)
	if moduleScript then
		local ok, result = pcall(require, moduleScript)
		if ok then
			loaded[name] = result
			table.insert(loadOrder, name)
		else
			warn(string.format("[Bootstrap] failed to load service %s: %s", name, tostring(result)))
		end
	else
		warn("[Bootstrap] missing service module " .. name)
	end
end

-- give every service a reference to the others so they can talk to each other
local registry = { order = loadOrder }
for name, service in pairs(loaded) do
	registry[name] = service
end
for _, name in ipairs(loadOrder) do
	local service = loaded[name]
	if type(service.Init) == "function" then
		local ok, err = pcall(service.Init, registry)
		if not ok then
			warn(string.format("[Bootstrap] %s:Init failed: %s", name, tostring(err)))
		end
	end
end
for _, name in ipairs(loadOrder) do
	local service = loaded[name]
	if type(service.Start) == "function" then
		local ok, err = pcall(service.Start, registry)
		if not ok then
			warn(string.format("[Bootstrap] %s:Start failed: %s", name, tostring(err)))
		end
	end
end

print(string.format("[Ashes] server ready — %d services online", #loadOrder))

-- Keep the server alive even if a service throws later on.
game:BindToClose(function()
	for _, name in ipairs(loadOrder) do
		local service = loaded[name]
		if type(service.Shutdown) == "function" then
			pcall(service.Shutdown, registry)
		end
	end
end)
