--[[
	Offline test harness for the game's Luau modules.

	Usage:  luau_rt --run tests/harness.lua <repoRoot>

	It installs a Roblox-compatible stub, builds a module resolver over src/shared,
	then runs every tests/spec/*.lua file. Exits non-zero if any assertion fails.
]]

local ROOT = args[1] or "."
local TestHost = TestHost

------------------------------------------------------------------------------
-- tiny bdd framework
------------------------------------------------------------------------------
local T = { passed = 0, failed = 0, failures = {}, suite = "(root)" }
local current = T
local FW = {}

FW.describe = function(name, fn)
	local prev = current
	current = { passed = 0, failed = 0, failures = T.failures, suite = prev.suite .. " > " .. name, parent = prev }
	local ok, err = pcall(fn)
	current = prev
	if not ok then
		T.failed += 1
		table.insert(T.failures, string.format("%s: suite error: %s", prev.suite, tostring(err)))
	end
end

FW.it = function(name, fn)
	local ok, err = pcall(fn)
	if ok then
		T.passed += 1
	else
		T.failed += 1
		table.insert(T.failures, string.format("%s > %s: %s", current.suite, name, tostring(err)))
	end
end

FW.fail = function(msg)
	error(msg, 2)
end

FW.expect = function(value, msg)
	if not value then
		fail(msg or "expected truthy value")
	end
	return value
end

FW.eq = function(a, b, msg)
	if a ~= b then
		fail(string.format("%s (got %s, expected %s)", msg or "values differ", tostring(a), tostring(b)))
	end
end

FW.near = function(a, b, tol, msg)
	tol = tol or 0.001
	if type(a) ~= "number" or type(b) ~= "number" then
		fail(string.format("%s (non numeric: %s, %s)", msg or "not a number", tostring(a), tostring(b)))
	end
	if math.abs(a - b) > tol then
		fail(string.format("%s (got %.6f, expected %.6f ± %.6f)", msg or "numbers differ", a, b, tol))
	end
end

FW.ok = function(fn, msg)
	local success, err = pcall(fn)
	if not success then
		fail(string.format("%s (%s)", msg or "call failed", tostring(err)))
	end
end

FW.throws = function(fn, msg)
	local success = pcall(fn)
	if success then
		fail(msg or "expected call to throw")
	end
end

-- expose the framework as globals so spec chunks (separate Lua chunks) see it
describe, it, fail, expect, eq, near, ok, throws = FW.describe, FW.it, FW.fail,
	FW.expect, FW.eq, FW.near, FW.ok, FW.throws

------------------------------------------------------------------------------
-- roblox stub installation
------------------------------------------------------------------------------
local mockSrc = assert(TestHost.readFile(ROOT .. "/tests/mock_roblox.lua"))
local mockChunk = assert(TestHost.loadChunk(mockSrc, "mock_roblox"))
local mock = mockChunk()

Vector3 = mock.Vector3
Vector2 = mock.Vector2
CFrame = mock.CFrame
Color3 = mock.Color3
Instance = mock.Instance
Enum = mock.Enum
NumberSequence = mock.NumberSequence
NumberSequenceKeypoint = mock.NumberSequenceKeypoint
ColorSequence = mock.ColorSequence
ColorSequenceKeypoint = mock.ColorSequenceKeypoint
NumberRange = mock.NumberRange
Random = { new = function(seed) return mock_rng(seed) end }
tick = mock.globals.tick
time = mock.globals.time
typeof = mock.globals.typeof
warn = mock.globals.warn
UDim = { new = function(s, o) return { Scale = s, Offset = o } end }
UDim2 = { new = function(a, b, c, d) return { a, b, c, d } end }
Vector3int16 = { new = function() return {} end }
BrickColor = { new = function() return {} end }
PhysicalProperties = { new = function(...) return { ... } end }
Ray = { new = function(o, d) return { Origin = o, Direction = d } end }
Region3 = { new = function(...) return { ... } end }
Rect = { new = function() return {} end }
Font = {}
TweenInfo = { new = function(...) return { ... } end }
DateTime = { now = function() return { UnixTimestamp = 0 } end }
os.time = os.time

-- Random stub (round-trip with the mock's generator)
function mock_rng(seed)
	local state = seed or 12345
	local rng = {}
	local function nxt(lo, hi)
		state = (1103515245 * state + 12345) % 2147483648
		return lo + state % (hi - lo + 1)
	end
	function rng:NextInteger(lo, hi) return nxt(lo, hi) end
	function rng:NextNumber(lo, hi)
		local v = nxt(0, 1000000) / 1000000
		if lo then return lo + v * ((hi or 1) - lo) end
		return v
	end
	function rng:Clone() return mock_rng(state) end
	function rng:NextUnitVector() return Vector3.new(0, 1, 0) end
	return rng
end

-- services ------------------------------------------------------------------
local function instanceNode(className, name)
	local node = Instance.new(className, nil)
	node.Name = name
	return node
end

local replicatedStorage = instanceNode("ReplicatedStorage", "ReplicatedStorage")
local sharedFolder = instanceNode("Folder", "Shared")
replicatedStorage:FindFirstChild("Shared")
table.insert(replicatedStorage._children, sharedFolder)
local remotesFolder = instanceNode("Folder", "Remotes")
table.insert(replicatedStorage._children, remotesFolder)
local serverStorage = instanceNode("ServerStorage", "ServerStorage")
local serverScriptService = instanceNode("ServerScriptService", "ServerScriptService")
local players = instanceNode("Players", "Players")
local workspaceNode = instanceNode("Workspace", "Workspace")
local lighting = instanceNode("Lighting", "Lighting")
local starterGui = instanceNode("StarterGui", "StarterGui")
local starterPlayer = instanceNode("StarterPlayer", "StarterPlayer")
local httpService = instanceNode("HttpService", "HttpService")
httpService.JSONEncode = function(_, t) return tostring(t) end
httpService.JSONDecode = function(_, t) return {} end
httpService.GenerateGUID = function() return "guid" end
local runService = instanceNode("RunService", "RunService")
runService.IsStudio = function() return false end
runService.IsServer = function() return true end
runService.IsClient = function() return false end
local debris = instanceNode("Debris", "Debris")
debris.AddItem = function() end
local collectionService = instanceNode("CollectionService", "CollectionService")
collectionService.AddTag = function() end
collectionService.RemoveTag = function() end
collectionService.GetTagged = function() return {} end
collectionService.HasTag = function() return false end
local dataStoreService = instanceNode("DataStoreService", "DataStoreService")
dataStoreService.GetDataStore = function()
	return {
		GetAsync = function() return nil end,
		SetAsync = function() return true end,
		UpdateAsync = function(_, key, fn) return fn(nil) end,
		RemoveAsync = function() return true end,
	}
end
dataStoreService.GetGlobalDataStore = dataStoreService.GetDataStore
local teleportService = instanceNode("TeleportService", "TeleportService")
local marketplaceService = instanceNode("MarketplaceService", "MarketplaceService")
local soundService = instanceNode("SoundService", "SoundService")
local pathfindingService = instanceNode("PathfindingService", "PathfindingService")
pathfindingService.CreatePath = function()
	return {
		ComputeAsync = function() end,
		Status = Enum.PathStatus and Enum.PathStatus.Success or 0,
		GetWaypoints = function() return {} end,
	}
end
local userInputService = instanceNode("UserInputService", "UserInputService")
userInputService.InputBegan = mock.newSignal()
userInputService.InputEnded = mock.newSignal()
userInputService.InputChanged = mock.newSignal()
local contextActionService = instanceNode("ContextActionService", "ContextActionService")
contextActionService.BindAction = function() end
contextActionService.UnbindAction = function() end
local tweenService = instanceNode("TweenService", "TweenService")
tweenService.Create = function()
	return { Play = function() end, Completed = { Wait = function() end, Connect = function() end } }
end
local guiService = instanceNode("GuiService", "GuiService")
local vrService = instanceNode("VRService", "VRService")
local physicsService = instanceNode("PhysicsService", "PhysicsService")
physicsService.RegisterCollisionGroup = function() end
physicsService.CollisionGroupSetCollidable = function() end
local testService = instanceNode("TestService", "TestService")

local serviceMap = {
	ReplicatedStorage = replicatedStorage,
	ServerStorage = serverStorage,
	ServerScriptService = serverScriptService,
	Players = players,
	Workspace = workspaceNode,
	Lighting = lighting,
	StarterGui = starterGui,
	StarterPlayer = starterPlayer,
	HttpService = httpService,
	RunService = runService,
	Debris = debris,
	CollectionService = collectionService,
	DataStoreService = dataStoreService,
	TeleportService = teleportService,
	MarketplaceService = marketplaceService,
	SoundService = soundService,
	PathfindingService = pathfindingService,
	UserInputService = userInputService,
	ContextActionService = contextActionService,
	TweenService = tweenService,
	GuiService = guiService,
	VRService = vrService,
	PhysicsService = physicsService,
	TestService = testService,
}

game = instanceNode("DataModel", "Game")
game.GetService = function(_, name)
	local svc = serviceMap[name]
	if not svc then
		svc = instanceNode(name, name)
		serviceMap[name] = svc
	end
	return svc
end
game.FindService = function(_, name) return game:GetService(name) end
game.PlaceId = 0
game.JobId = "test"
game.CreatorId = 0
game.BindToClose = function() end
game.Players = players
workspace = workspaceNode

shared = setmetatable({}, { __newindex = function(t, k, v) rawset(t, k, v) end })

------------------------------------------------------------------------------
-- module resolver
------------------------------------------------------------------------------
local moduleCache = {}
local moduleRoot = ROOT .. "/src/shared"
local loadedNames = {}

local function makeNode(name)
	local node = { Name = name }
	local parentProxy
	parentProxy = setmetatable({ Name = "Shared" }, {
		__index = function(_, key)
			if key == "Parent" then
				return setmetatable({ Name = "ReplicatedStorage" }, { __index = function(_, k) return nil end })
			end
			if type(key) == "string" then
				return makeNode(key)
			end
			return nil
		end,
	})
	node.Parent = parentProxy
	return node
end

local function resolveModule(name)
	if moduleCache[name] then
		return moduleCache[name]
	end
	local path = moduleRoot .. "/" .. name .. ".lua"
	local src = TestHost.readFile(path)
	if not src then
		error("module not found: " .. name, 3)
	end
	local chunk, err = TestHost.loadChunk(src, name)
	if not chunk then
		error(string.format("compile error in %s: %s", name, tostring(err)), 3)
	end
	local prevScript = script
	script = makeNode(name)
	moduleCache[name] = true -- recursion guard
	local result = chunk()
	script = prevScript
	moduleCache[name] = result
	table.insert(loadedNames, name)
	return result
end

require = function(target)
	if type(target) == "table" and target.Name then
		return resolveModule(target.Name)
	elseif type(target) == "string" then
		return resolveModule(target)
	end
	error("require() expects a module node or name, got " .. typeof(target))
end

script = makeNode("harness")
-- expose the repo root to spec files that want to lint source on disk
ROOT_DIR = ROOT

------------------------------------------------------------------------------
-- run spec files
------------------------------------------------------------------------------
local specDir = ROOT .. "/tests/spec"
local specs = TestHost.listDir(specDir) or {}
table.sort(specs)

local specCount = 0
for _, fileName in ipairs(specs) do
	if string.sub(fileName, -4) == ".lua" then
		specCount += 1
		local src = assert(TestHost.readFile(specDir .. "/" .. fileName))
		local chunk, err = TestHost.loadChunk(src, "spec/" .. fileName)
		if not chunk then
			table.insert(T.failures, string.format("spec %s failed to compile: %s", fileName, tostring(err)))
			T.failed += 1
		else
			local success, runErr = pcall(chunk)
			if not success then
				table.insert(T.failures, string.format("spec %s crashed: %s", fileName, tostring(runErr)))
				T.failed += 1
			end
		end
	end
end

print(string.format("\n%d assertions passed, %d failed across %d spec files (%d modules loaded)",
	T.passed, T.failed, specCount, #loadedNames))
if T.failed > 0 then
	for _, f in ipairs(T.failures) do
		print("  FAIL " .. f)
	end
	TestHost.exit(1)
else
	print("  all green")
	TestHost.exit(0)
end
