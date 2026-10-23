--!strict
--[[
    ASHES OF HUMANITY: LAST SIGNAL
    ServerScriptService.Services.SecurityService
    Token-bucket RemoteEvent rate limiting, anti-duplication mutexes,
    player proximity validation, and payload sanitization.
]]

local Players = game:GetService("Players")
local ReplicatedStorage = game:GetService("ReplicatedStorage")

local NetworkSchema = require(ReplicatedStorage.Shared.NetworkSchema)
local UtilityModules = require(ReplicatedStorage.Shared.UtilityModules)

local SecurityService = {}

type Bucket = {
	Tokens: number,
	LastRefill: number,
}

local playerBuckets: { [number]: { [string]: Bucket } } = {}
local transactionLocks: { [number]: boolean } = {}
local violationCounts: { [number]: number } = {}

function SecurityService:Init(_registry: any)
	Players.PlayerRemoving:Connect(function(player: Player)
		playerBuckets[player.UserId] = nil
		transactionLocks[player.UserId] = nil
		violationCounts[player.UserId] = nil
	end)
end

function SecurityService:Start() end

function SecurityService:CheckRateLimit(player: Player, remoteName: string): boolean
	local uid = player.UserId
	local spec = (NetworkSchema.RATE_LIMITS :: any)[remoteName] or { Burst = 12, Rate = 6 }
	local now = os.clock()

	local pMap = playerBuckets[uid]
	if not pMap then
		pMap = {}
		playerBuckets[uid] = pMap
	end

	local bucket = pMap[remoteName]
	if not bucket then
		bucket = {
			Tokens = spec.Burst - 1,
			LastRefill = now,
		}
		pMap[remoteName] = bucket
		return true
	end

	local elapsed = math.max(0, now - bucket.LastRefill)
	bucket.Tokens = math.min(spec.Burst, bucket.Tokens + elapsed * spec.Rate)
	bucket.LastRefill = now

	if bucket.Tokens >= 1.0 then
		bucket.Tokens -= 1.0
		return true
	end

	violationCounts[uid] = (violationCounts[uid] or 0) + 1
	return false
end

function SecurityService:WithTransactionLock(player: Player, callback: () -> (boolean, string?)): (boolean, string?)
	local uid = player.UserId
	if transactionLocks[uid] then
		return false, "Busy"
	end
	transactionLocks[uid] = true
	local ok, res1, res2 = pcall(callback)
	transactionLocks[uid] = nil
	if not ok then
		warn("[SecurityService] Transaction error for", player.Name, res1)
		return false, "InternalError"
	end
	return res1, res2
end

function SecurityService:GetCharacterRoot(player: Player): BasePart?
	local char = player.Character
	if not char then
		return nil
	end
	local hum = char:FindFirstChildOfClass("Humanoid")
	if not hum or hum.Health <= 0 then
		return nil
	end
	local root = char:FindFirstChild("HumanoidRootPart")
	if root and root:IsA("BasePart") then
		return root
	end
	return nil
end

function SecurityService:IsWithinDistance(player: Player, targetPos: Vector3, maxDist: number): boolean
	if not UtilityModules.IsFiniteVector3(targetPos) then
		return false
	end
	local root = self:GetCharacterRoot(player)
	if not root then
		return false
	end
	return (root.Position - targetPos).Magnitude <= maxDist
end

function SecurityService:GetViolationCount(userId: number): number
	return violationCounts[userId] or 0
end

return SecurityService
