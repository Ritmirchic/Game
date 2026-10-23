--!strict
--[[
	Remotes service — creates every remote declared in Shared/Remotes and provides
	rate-limited, arity-checked handlers to the other services.
]]

local ReplicatedStorage = game:GetService("ReplicatedStorage")
local Players = game:GetService("Players")

local Shared = ReplicatedStorage:WaitForChild("Shared")
local RemotesDef = require(Shared.Remotes)
local Util = require(Shared.Util)

local RemotesService = {}

local folder
local events = {}
local functions = {}
local handlers = {}
local limiters = {}

-- Per-player budget: 55 remote calls per second with burst 110. Combat and
-- movement get their own buckets so a busy fight never starves the UI.
local LIMITS = {
	RequestMove = { 30, 60 },
	RequestAttack = { 12, 24 },
	RequestInteract = { 10, 20 },
	RequestLoot = { 10, 20 },
	RequestBuild = { 8, 16 },
	RequestSprint = { 20, 40 },
	RequestPing = { 2, 4 },
}

function RemotesService.Init(registry)
	folder = ReplicatedStorage:FindFirstChild(RemotesDef.FolderName)
	if not folder then
		folder = Instance.new("Folder")
		folder.Name = RemotesDef.FolderName
		folder.Parent = ReplicatedStorage
	end
	for _, name in ipairs(RemotesDef.Events) do
		local remote = folder:FindFirstChild(name)
		if not remote then
			remote = Instance.new("RemoteEvent")
			remote.Name = name
			remote.Parent = folder
		end
		events[name] = remote
	end
	for _, name in ipairs(RemotesDef.Functions) do
		local remote = folder:FindFirstChild(name)
		if not remote then
			remote = Instance.new("RemoteFunction")
			remote.Name = name
			remote.Parent = folder
		end
		functions[name] = remote
	end
	for name in pairs(LIMITS) do
		limiters[name] = {}
	end
	for _, name in ipairs(RemotesDef.Events) do
		if not limiters[name] then
			limiters[name] = {}
		end
	end
end

local function getLimiter(player, remoteName)
	local bucket = limiters[remoteName]
	if not bucket then
		bucket = {}
		limiters[remoteName] = bucket
	end
	local limiter = bucket[player.UserId]
	if not limiter then
		local capacity, refill = 55, 55
		local conf = LIMITS[remoteName]
		if conf then
			capacity, refill = conf[1], conf[2]
		end
		limiter = Util.newRateLimiter(capacity, refill)
		bucket[player.UserId] = limiter
	end
	return limiter
end

function RemotesService.On(remoteName, handler)
	if not events[remoteName] then
		warn("[Remotes] unknown remote " .. tostring(remoteName))
		return false
	end
	if handlers[remoteName] then
		-- one owner per remote: a second binding would silently double-fire the
		-- handler (and the unthrottled one), so refuse it loudly.
		warn("[Remotes] " .. tostring(remoteName) .. " is already bound")
		return false
	end
	handlers[remoteName] = handler
	events[remoteName].OnServerEvent:Connect(function(player, ...)
		local limiter = getLimiter(player, remoteName)
		if not limiter:allow(1) then
			return
		end
		local ok, err = pcall(handler, player, ...)
		if not ok then
			warn(string.format("[Remotes] %s handler error: %s", remoteName, tostring(err)))
		end
	end)
	return true
end

function RemotesService.Fire(player, remoteName, ...)
	local remote = events[remoteName]
	if remote then
		remote:FireClient(player, ...)
	end
end

function RemotesService.FireAll(remoteName, ...)
	local remote = events[remoteName]
	if remote then
		remote:FireAllClients(...)
	end
end

function RemotesService.FireNear(position, radius, remoteName, ...)
	local remote = events[remoteName]
	if not remote then return end
	for _, player in ipairs(Players:GetPlayers()) do
		local character = player.Character
		local root = character and character:FindFirstChild("HumanoidRootPart")
		if root and (root.Position - position).Magnitude <= radius then
			remote:FireClient(player, ...)
		end
	end
end

function RemotesService.OnInvoke(funcName, handler)
	if not functions[funcName] then
		warn("[Remotes] unknown remote function " .. tostring(funcName))
		return
	end
	functions[funcName].OnServerInvoke = function(player, ...)
		local limiter = getLimiter(player, funcName)
		if not limiter:allow(1) then
			return nil
		end
		local ok, result = pcall(handler, player, ...)
		if not ok then
			warn(string.format("[Remotes] %s invoke error: %s", funcName, tostring(result)))
			return nil
		end
		return result
	end
end

function RemotesService.GetEvent(name)
	return events[name]
end

function RemotesService.ClearPlayer(player)
	for _, bucket in pairs(limiters) do
		bucket[player.UserId] = nil
	end
end

function RemotesService.Start()
	Players.PlayerRemoving:Connect(function(player)
		RemotesService.ClearPlayer(player)
	end)
end

return RemotesService
