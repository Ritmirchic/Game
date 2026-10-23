--!strict
--[[
	PartyService — cooperative play: parties, shared objectives, revives,
	co-op enemy scaling helpers and friendly-fire arbitration.
]]

local Players = game:GetService("Players")
local ReplicatedStorage = game:GetService("ReplicatedStorage")

local Shared = ReplicatedStorage:WaitForChild("Shared")

local PartyService = {}

local registryRef
local remotes
local parties = {}   -- [partyId] = { members = { [userId] = true }, leader = userId }
local memberOf = {}  -- [userId] = partyId
local nextPartyId = 0
local downed = {}    -- [userId] = os.clock() deadline

function PartyService.Init(registry)
	registryRef = registry
	remotes = registry.Remotes
end

function PartyService.Start(registry)
	remotes.On("RequestParty", function(player, action, targetName)
		PartyService.Handle(player, action, targetName)
	end)
	remotes.On("RequestRevive", function(player, targetPlayer)
		PartyService.Revive(player, targetPlayer)
	end)
	Players.PlayerRemoving:Connect(function(player)
		PartyService.Leave(player)
		downed[player.UserId] = nil
	end)
end

function PartyService.Handle(player, action, targetName)
	if action == "invite" and type(targetName) == "string" then
		local target
		for _, other in ipairs(Players:GetPlayers()) do
			if other.Name == targetName then
				target = other
				break
			end
		end
		if not target or target == player then return end
		remotes.Fire(target, "RevivePrompt", player.Name, "invite")
		PartyService.Join(player, target)
	elseif action == "leave" then
		PartyService.Leave(player)
		remotes.Fire(player, "Notify", "You left the group.", "info", 2)
	end
end

function PartyService.Join(a, b)
	local partyA = memberOf[a.UserId]
	local partyB = memberOf[b.UserId]
	if partyA and partyB and partyA == partyB then return end
	local partyId = partyA or partyB
	if not partyId then
		nextPartyId += 1
		partyId = nextPartyId
		parties[partyId] = { members = {}, leader = a.UserId }
	end
	for _, user in ipairs({ a.UserId, b.UserId }) do
		parties[partyId].members[user] = true
		memberOf[user] = partyId
	end
	if partyA and partyB then
		-- merge
		for user in pairs(parties[partyB].members) do
			parties[partyId].members[user] = true
			memberOf[user] = partyId
		end
		parties[partyB] = nil
	end
	remotes.Fire(a, "Notify", "Group formed.", "info", 2.4)
	remotes.Fire(b, "Notify", a.Name .. " joined your group.", "info", 2.4)
end

function PartyService.Leave(player)
	local partyId = memberOf[player.UserId]
	memberOf[player.UserId] = nil
	if partyId and parties[partyId] then
		parties[partyId].members[player.UserId] = nil
		local remaining = 0
		for _ in pairs(parties[partyId].members) do remaining += 1 end
		if remaining == 0 then
			parties[partyId] = nil
		end
	end
end

function PartyService.SameParty(a, b)
	local partyId = memberOf[a.UserId]
	return partyId ~= nil and partyId == memberOf[b.UserId]
end

function PartyService.PartySize(player)
	local partyId = memberOf[player.UserId]
	if not partyId or not parties[partyId] then return 1 end
	local n = 0
	for _ in pairs(parties[partyId].members) do n += 1 end
	return n
end

function PartyService.NotifyDowned(player)
	downed[player.UserId] = os.clock() + 45
	local partyId = memberOf[player.UserId]
	if not partyId then return end
	for userId in pairs(parties[partyId].members) do
		local other = Players:GetPlayerByUserId(userId)
		if other and other ~= player then
			remotes.Fire(other, "Notify", player.Name .. " is down. Reach them in 45 seconds.", "danger", 6)
		end
	end
end

function PartyService.Revive(reviver, target)
	if type(target) ~= "instance" then return end
	local deadline = downed[target.UserId]
	if not deadline then return end
	local reviverRoot = reviver.Character and reviver.Character:FindFirstChild("HumanoidRootPart")
	local targetRoot = target.Character and target.Character:FindFirstChild("HumanoidRootPart")
	if not reviverRoot or not targetRoot then return end
	if (reviverRoot.Position - targetRoot.Position).Magnitude > 12 then return end
	local profile = registryRef.PlayerDataService.Get(target)
	if not profile then return end
	local itemCount = require(Shared.InventoryModel).countItem(profile.inventory, "medkit", true)
	if itemCount < 1 then
		remotes.Fire(reviver, "Notify", "You need a medkit to revive someone.", "warn", 3)
		return
	end
	require(Shared.InventoryModel).consumeItem(profile.inventory, "medkit", 1)
	profile.survival.health = 45
	profile.survival.bleeding = 0
	profile.survival.alive = true
	downed[target.UserId] = nil
	remotes.Fire(target, "StoryEvent", "revived", { by = reviver.Name })
	remotes.Fire(reviver, "Notify", "You brought " .. target.Name .. " back.", "quest", 4)
	if registryRef.SurvivalService then
		registryRef.SurvivalService.Push(target, profile)
	end
end

function PartyService.Cleanup()
	local now = os.clock()
	for userId, deadline in pairs(downed) do
		if now > deadline then
			downed[userId] = nil
			local player = Players:GetPlayerByUserId(userId)
			if player and registryRef.PlayerDataService then
				local profile = registryRef.PlayerDataService.Get(player)
				if profile then
					profile.survival.health = 0
					if registryRef.SurvivalService then
						registryRef.SurvivalService.OnDeath(player, profile, "Bled out alone")
					end
				end
			end
		end
	end
end

return PartyService
