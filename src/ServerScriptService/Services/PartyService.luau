--!strict
--[[
    ASHES OF HUMANITY: LAST SIGNAL
    ServerScriptService.Services.PartyService
    Cooperative Multiplayer Party System, Shared Vitals Telemetry,
    Shared Base Building Permissions, and Teammate Downed/Revive Mechanics (Section 31).
]]

local Players = game:GetService("Players")
local ReplicatedStorage = game:GetService("ReplicatedStorage")

local NetworkSchema = require(ReplicatedStorage.Shared.NetworkSchema)
local UtilityModules = require(ReplicatedStorage.Shared.UtilityModules)

local PartyService = {}

export type PartyRecord = {
	PartyId: string,
	LeaderUserId: number,
	Members: { number },
	PendingInvites: { [number]: number }, -- targetUserId -> timestamp
}

local registry: any = nil
local parties: { [string]: PartyRecord } = {}
local userToParty: { [number]: string } = {}

function PartyService:Init(serviceRegistry: any)
	registry = serviceRegistry
	Players.PlayerRemoving:Connect(function(player: Player)
		self:LeaveParty(player)
	end)
end

function PartyService:GetPlayerPartyId(userId: number): string?
	return userToParty[userId]
end

function PartyService:AreInSameParty(userA: number, userB: number): boolean
	if userA == userB then
		return true
	end
	local pa = userToParty[userA]
	return pa ~= nil and pa == userToParty[userB]
end

function PartyService:HasLivingPartyMember(player: Player): boolean
	local pid = userToParty[player.UserId]
	if not pid or not parties[pid] then
		return false
	end
	for _, memberUid in ipairs(parties[pid].Members) do
		if memberUid ~= player.UserId then
			local peer = Players:GetPlayerByUserId(memberUid)
			if peer then
				local prof = registry.PlayerDataService:GetProfile(peer)
				if prof and prof.Vitals.Health > 0 and not prof.Vitals.IsDowned then
					return true
				end
			end
		end
	end
	return false
end

function PartyService:GetNearbyPartyMembers(player: Player, radius: number): { Player }
	local result: { Player } = { player }
	local pid = userToParty[player.UserId]
	if not pid or not parties[pid] then
		return result
	end
	local myRoot = registry.SecurityService:GetCharacterRoot(player)
	if not myRoot then
		return result
	end

	for _, memberUid in ipairs(parties[pid].Members) do
		if memberUid ~= player.UserId then
			local peer = Players:GetPlayerByUserId(memberUid)
			if peer then
				local peerRoot = registry.SecurityService:GetCharacterRoot(peer)
				if peerRoot and (peerRoot.Position - myRoot.Position).Magnitude <= radius then
					table.insert(result, peer)
				end
			end
		end
	end
	return result
end

function PartyService:CreateParty(player: Player): (boolean, string?)
	if userToParty[player.UserId] then
		return false, "AlreadyInParty"
	end
	local pid = UtilityModules.GenerateUID("party")
	parties[pid] = {
		PartyId = pid,
		LeaderUserId = player.UserId,
		Members = { player.UserId },
		PendingInvites = {},
	}
	userToParty[player.UserId] = pid
	self:SyncParty(pid)
	return true, pid
end

function PartyService:InvitePlayer(sender: Player, targetUserId: number): (boolean, string?)
	local pid = userToParty[sender.UserId]
	if not pid then
		local ok, newPid = self:CreateParty(sender)
		if not ok or not newPid then
			return false, "CreateFailed"
		end
		pid = newPid
	end

	local party = parties[pid]
	if not party or #party.Members >= 4 then
		return false, "PartyFull"
	end
	if userToParty[targetUserId] then
		return false, "TargetInParty"
	end

	local target = Players:GetPlayerByUserId(targetUserId)
	if not target then
		return false, "TargetOffline"
	end

	party.PendingInvites[targetUserId] = os.clock()
	registry.SendToPlayer(target, NetworkSchema.EVENTS.PartyUpdate, {
		Kind = "InviteReceived",
		PartyId = pid,
		FromName = sender.Name,
		FromUserId = sender.UserId,
	})
	return true, nil
end

function PartyService:AcceptInvite(player: Player, partyId: string): (boolean, string?)
	if userToParty[player.UserId] then
		return false, "AlreadyInParty"
	end
	local party = parties[partyId]
	if not party or not party.PendingInvites[player.UserId] then
		return false, "NoInvite"
	end
	if #party.Members >= 4 then
		return false, "PartyFull"
	end

	party.PendingInvites[player.UserId] = nil
	table.insert(party.Members, player.UserId)
	userToParty[player.UserId] = partyId
	self:SyncParty(partyId)
	return true, nil
end

function PartyService:LeaveParty(player: Player): boolean
	local pid = userToParty[player.UserId]
	if not pid then
		return false
	end
	userToParty[player.UserId] = nil
	local party = parties[pid]
	if not party then
		return true
	end

	local idx = table.find(party.Members, player.UserId)
	if idx then
		table.remove(party.Members, idx)
	end

	if #party.Members == 0 then
		parties[pid] = nil
	else
		if party.LeaderUserId == player.UserId then
			party.LeaderUserId = party.Members[1]
		end
		self:SyncParty(pid)
	end

	registry.SendToPlayer(player, NetworkSchema.EVENTS.PartyUpdate, {
		Kind = "PartyState",
		Party = nil,
	})
	return true
end

function PartyService:ReviveTeammate(reviver: Player, downedUserId: number): (boolean, string?)
	if reviver.UserId == downedUserId then
		return false, "CannotSelfRevive"
	end
	local target = Players:GetPlayerByUserId(downedUserId)
	if not target then
		return false, "TargetMissing"
	end

	local reviverProf = registry.PlayerDataService:GetProfile(reviver)
	local targetProf = registry.PlayerDataService:GetProfile(target)
	if not reviverProf or not targetProf then
		return false, "NoProfile"
	end
	if reviverProf.Vitals.IsDowned or reviverProf.Vitals.Health <= 0 then
		return false, "ReviverIncapacitated"
	end
	if not targetProf.Vitals.IsDowned then
		return false, "NotDowned"
	end

	local tRoot = registry.SecurityService:GetCharacterRoot(target)
	if not tRoot or not registry.SecurityService:IsWithinDistance(reviver, tRoot.Position, 10.0) then
		return false, "TooFarToRevive"
	end

	targetProf.Vitals.IsDowned = false
	targetProf.Vitals.DownedBleedoutTimer = 0
	targetProf.Vitals.Health = 35.0
	registry.SurvivalService:ApplyMobilityModifiers(target, targetProf)
	registry.SurvivalService:SyncPlayerVitals(target, true)

	reviverProf.Stats.TeammatesRevived += 1
	registry.SendToPlayer(target, NetworkSchema.EVENTS.Notification, {
		Kind = "Success",
		Text = reviver.Name .. " stabilized you back on your feet!",
	})
	registry.SendToPlayer(reviver, NetworkSchema.EVENTS.Notification, {
		Kind = "Success",
		Text = "Revived " .. target.Name .. "!",
	})
	return true, nil
end

function PartyService:SyncParty(partyId: string)
	local party = parties[partyId]
	if not party then
		return
	end
	local memberSummaries = {}
	for _, uid in ipairs(party.Members) do
		local p = Players:GetPlayerByUserId(uid)
		if p then
			local prof = registry.PlayerDataService:GetProfile(p)
			local r = registry.SecurityService:GetCharacterRoot(p)
			table.insert(memberSummaries, {
				UserId = uid,
				Name = p.Name,
				Health = if prof then math.floor(prof.Vitals.Health) else 100,
				MaxHealth = if prof then math.floor(prof.Vitals.MaxHealth) else 100,
				IsDowned = if prof then prof.Vitals.IsDowned else false,
				Region = if prof then prof.Vitals.CurrentRegion else "DeadSuburbs",
				Position = if r then r.Position else Vector3.zero,
			})
		end
	end

	local packet = {
		Kind = "PartyState",
		Party = {
			PartyId = party.PartyId,
			LeaderUserId = party.LeaderUserId,
			Members = memberSummaries,
		},
	}

	for _, uid in ipairs(party.Members) do
		local p = Players:GetPlayerByUserId(uid)
		if p then
			registry.SendToPlayer(p, NetworkSchema.EVENTS.PartyUpdate, packet)
		end
	end
end

function PartyService:HandleRemoteAction(player: Player, payload: any)
	if type(payload) ~= "table" or not NetworkSchema.ValidateString(payload.Action, 24) then
		return
	end
	local act = payload.Action
	if act == "Create" then
		self:CreateParty(player)
	elseif act == "Invite" and NetworkSchema.ValidateInt(payload.TargetUserId, 1, 999999999999) then
		self:InvitePlayer(player, payload.TargetUserId)
	elseif act == "Accept" and NetworkSchema.ValidateString(payload.PartyId, 64) then
		self:AcceptInvite(player, payload.PartyId)
	elseif act == "Leave" then
		self:LeaveParty(player)
	elseif act == "Revive" and NetworkSchema.ValidateInt(payload.TargetUserId, 1, 999999999999) then
		self:ReviveTeammate(player, payload.TargetUserId)
	end
end

function PartyService:Start()
	task.spawn(function()
		while true do
			task.wait(2.5)
			for pid in pairs(parties) do
				self:SyncParty(pid)
			end
		end
	end)
end

return PartyService
