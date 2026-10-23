--[=[
	Server boot smoke test.

	Loads every module in src/server against the Roblox stub, then calls Init()
	with a registry containing all seventeen services. This catches the errors
	that a syntax check cannot: missing service references, nil remotes, bad
	requires, and services that do not expose the Init/Start contract.

	Start() is deliberately *not* called: the real services spin up Heartbeat
	loops, which the deterministic stub would run forever.
]=]

local ROOT = ROOT_DIR or "."

local SERVICE_FILES = {
	"Remotes", "ConfigService", "PlayerDataService", "InventoryService",
	"SurvivalService", "LootService", "InteractionService", "CombatService",
	"CraftingService", "BuildingService", "QuestService", "PartyService",
	"EnemyService", "WeatherService", "WorldEventService", "StoryService",
	"DirectorService",
}

local function makeSharedProxy(parentName)
	local proxy
	proxy = setmetatable({ Name = parentName }, {
		__index = function(_, key)
			if key == "Parent" then
				return { Name = "ReplicatedStorage" }
			end
			if type(key) == "string" then
				return { Name = key }
			end
			return nil
		end,
	})
	return proxy
end

-- The services ask ReplicatedStorage for the Shared folder at load time. Hand
-- them a proxy whose children resolve by module name, so require() works.
local ReplicatedStorage = game:GetService("ReplicatedStorage")
local sharedProxy = makeSharedProxy("Shared")
ReplicatedStorage.WaitForChild = function(_, name)
	if name == "Shared" then return sharedProxy end
	return sharedProxy
end
-- Remotes.Init creates real RemoteEvent/RemoteFunction instances inside a
-- folder, so that one lookup gets a genuine stub instance.
local remotesFolder = Instance.new("Folder", nil)
remotesFolder.Name = "Remotes"
ReplicatedStorage.FindFirstChild = function(_, name)
	if name == "Remotes" then return remotesFolder end
	return sharedProxy
end

local loaded = {}
local failures = {}

for _, name in ipairs(SERVICE_FILES) do
	local path = ROOT .. "/src/server/" .. name .. ".lua"
	local src = TestHost.readFile(path)
	if not src then
		table.insert(failures, name .. ": source missing")
	else
		local chunk, err = TestHost.loadChunk(src, "server/" .. name)
		if not chunk then
			table.insert(failures, name .. ": compile error " .. tostring(err))
		else
			local previous = script
			script = { Name = name, Parent = { Name = "Services" } }
			local ok, result = pcall(chunk)
			script = previous
			if not ok then
				table.insert(failures, name .. ": load error " .. tostring(result))
			else
				loaded[name] = result
			end
		end
	end
end

describe("Server boot: loading", function()
	it("every service module loads", function()
		eq(#failures, 0, table.concat(failures, " | "))
		local count = 0
		for _ in pairs(loaded) do count += 1 end
		eq(count, #SERVICE_FILES)
	end)

	it("every service exposes Init and Start", function()
		for _, name in ipairs(SERVICE_FILES) do
			local service = loaded[name]
			expect(type(service) == "table", name .. " did not return a table")
			eq(type(service.Init), "function", name .. " has no Init")
			eq(type(service.Start), "function", name .. " has no Start")
		end
	end)
end)

describe("Server boot: init", function()
	it("Init accepts the registry for every service", function()
		local registry = { order = SERVICE_FILES }
		local remotesStub = {
			On = function() return true end,
			OnInvoke = function() return true end,
			Fire = function() end,
			FireAll = function() end,
			FireNear = function() end,
		}
		registry.Remotes = remotesStub
		for _, name in ipairs(SERVICE_FILES) do
			registry[name] = loaded[name]
		end
		local broken = {}
		for _, name in ipairs(SERVICE_FILES) do
			local service = loaded[name]
			local ok, err = pcall(service.Init, registry)
			if not ok then
				table.insert(broken, name .. ": " .. tostring(err))
			end
		end
		eq(#broken, 0, table.concat(broken, " | "))
	end)

	it("no service mutates the registry during Init", function()
		local registry = { order = SERVICE_FILES }
		registry.Remotes = { On = function() end, OnInvoke = function() end }
		for _, name in ipairs(SERVICE_FILES) do
			registry[name] = loaded[name]
		end
		local before = {}
		for key in pairs(registry) do before[key] = true end
		for _, name in ipairs(SERVICE_FILES) do
			pcall(loaded[name].Init, registry)
		end
		for key in pairs(registry) do
			expect(before[key], "Init added a registry key: " .. tostring(key))
		end
	end)
end)
