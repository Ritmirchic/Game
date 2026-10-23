--!strict
--[[
	BuildingService — base building with grid snapping, validation and persistence.

	Structures are real parts parented to Workspace/Structures so they replicate.
	Player-built pieces are saved in the profile (list of {pieceId, position,
	rotation, health}) and restored on join.
]]

local Players = game:GetService("Players")
local ReplicatedStorage = game:GetService("ReplicatedStorage")
local Workspace = game:GetService("Workspace")
local ServerStorage = game:GetService("ServerStorage")

local Shared = ReplicatedStorage:WaitForChild("Shared")
local Build = require(Shared.BuildDefinitions)
local BuildLogic = require(Shared.BuildLogic)
local Config = require(Shared.Config)
local Items = require(Shared.ItemDefinitions)

local BuildingService = {}

local registryRef
local remotes
local structuresFolder
local occupancy = {} -- [userId] = array of {position, rotation, size, pieceId, part}
local pieces = {}    -- [part] = { pieceId, owner, health, data }
local globalIndex = {} -- all structures, used for overlap checks

function BuildingService.Init(registry)
	registryRef = registry
	remotes = registry.Remotes
end

function BuildingService.Start(registry)
	structuresFolder = Workspace:FindFirstChild("Structures")
	if not structuresFolder then
		structuresFolder = Instance.new("Folder")
		structuresFolder.Name = "Structures"
		structuresFolder.Parent = Workspace
	end
	remotes.On("RequestBuild", function(player, pieceId, position, rotation)
		BuildingService.Place(player, pieceId, position, rotation)
	end)
	remotes.On("RequestDismantle", function(player, instance)
		BuildingService.Dismantle(player, instance)
	end)
	remotes.On("RequestRepair", function(player, instance)
		BuildingService.Repair(player, instance)
	end)
	Players.PlayerAdded:Connect(function(player)
		task.spawn(function()
			task.wait(3)
			BuildingService.Restore(player)
		end)
	end)
end

local function makePart(def, position, rotation, owner)
	local part = Instance.new("Part")
	part.Name = def.name
	part.Size = def.size
	part.Position = position
	part.CFrame = CFrame.new(position) * CFrame.Angles(0, math.rad(rotation or 0), 0)
	if def.horizontal then
		part.Material = Enum.Material.WoodPlanks
		part.Color = Color3.fromRGB(92, 76, 60)
	elseif string.find(def.id, "metal") then
		part.Material = Enum.Material.CorrugatedMetal
		part.Color = Color3.fromRGB(104, 100, 92)
	else
		part.Material = Enum.Material.WoodPlanks
		part.Color = Color3.fromRGB(86, 70, 56)
	end
	part.Anchored = true
	part.TopSurface = Enum.SurfaceType.Smooth
	part.BottomSurface = Enum.SurfaceType.Smooth
	local tag = Instance.new("StringValue")
	tag.Name = "PieceId"
	tag.Value = def.id
	tag.Parent = part
	local ownerTag = Instance.new("StringValue")
	ownerTag.Name = "Owner"
	ownerTag.Value = tostring(owner and owner.UserId or 0)
	ownerTag.Parent = part
	part.Parent = structuresFolder
	return part
end

function BuildingService.Place(player, pieceId, position, rotation)
	local profile = registryRef.PlayerDataService.Get(player)
	if not profile or type(pieceId) ~= "string" then return false end
	local def = Build[pieceId]
	if not def then return false end
	if typeof(position) ~= "Vector3" then return false end

	local character = player.Character
	local root = character and character:FindFirstChild("HumanoidRootPart")
	if not root then return false end
	if (root.Position - position).Magnitude > 32 then
		remotes.Fire(player, "BuildResult", false, pieceId, "Too far away.")
		return false
	end

	local snapped = BuildLogic.snapPosition(def, position, rotation)
	local snappedRotation = BuildLogic.snapRotation(rotation)

	-- ground check: build must be near the ground or supported by a structure
	local params = RaycastParams.new()
	params.FilterType = Enum.RaycastFilterType.Exclude
	params.FilterDescendantsInstances = { character }
	local down = Workspace:Raycast(snapped + Vector3.new(0, 8, 0), Vector3.new(0, -20, 0), params)
	local occupied = {}
	for part, info in pairs(pieces) do
		if part.Parent then
			table.insert(occupied, {
				position = part.Position, rotation = info.rotation,
				size = Build[info.pieceId] and Build[info.pieceId].size or Vector3.new(4, 4, 4),
			})
		end
	end

	local ok, reason = BuildLogic.validatePlacement(profile, pieceId, snapped, snappedRotation, occupied,
		down ~= nil and math.abs(down.Position.Y - snapped.Y) < 8)
	if not ok then
		remotes.Fire(player, "BuildResult", false, pieceId, reason or "Cannot build here.")
		return false
	end

	if not BuildLogic.consumeCost(profile, pieceId) then
		remotes.Fire(player, "BuildResult", false, pieceId, "Not enough materials.")
		return false
	end

	local part = makePart(def, snapped, snappedRotation, player)
	pieces[part] = {
		pieceId = pieceId, owner = player.UserId, rotation = snappedRotation,
		health = BuildLogic.healthFor(pieceId, 1),
	}
	profile.structures = profile.structures or {}
	table.insert(profile.structures, {
		pieceId = pieceId, x = snapped.X, y = snapped.Y, z = snapped.Z, rotation = snappedRotation,
	})
	profile.stats.built = (profile.stats.built or 0) + 1
	registryRef.PlayerDataService.MarkDirty(player)

	remotes.Fire(player, "BuildResult", true, pieceId, "Built " .. def.name)
	remotes.Fire(player, "InventoryUpdate", require(Shared.InventoryModel).serialize(profile.inventory))
	if registryRef.QuestService then
		registryRef.QuestService.Event(player, { type = "build", id = pieceId })
	end
	if registryRef.DirectorService then
		registryRef.DirectorService.CheckTutorial(player, profile, "build")
	end

	-- interactive pieces hook into the world
	if def.interactive == "campfire" and registryRef.WorldEventService then
		registryRef.WorldEventService.RegisterFire(snapped)
	elseif def.interactive == "light" then
		local light = Instance.new("PointLight")
		light.Brightness = 2.2
		light.Range = 28
		light.Color = Color3.fromRGB(255, 214, 160)
		light.Parent = part
	elseif def.interactive == "relay" then
		if registryRef.QuestService then
			registryRef.QuestService.Event(player, { type = "interact", kind = "RadioRelayBuilt" })
		end
	end
	return true
end

function BuildingService.Dismantle(player, instance)
	if typeof(instance) ~= "instance" then return false end
	local info = pieces[instance]
	if not info then return false end
	local character = player.Character
	local root = character and character:FindFirstChild("HumanoidRootPart")
	if not root or (root.Position - instance.Position).Magnitude > 20 then return false end
	if info.owner ~= player.UserId then
		remotes.Fire(player, "Notify", "This belongs to another survivor.", "warn", 2.4)
		return false
	end
	local profile = registryRef.PlayerDataService.Get(player)
	if profile then
		BuildLogic.refund(profile, info.pieceId, 0.6)
		profile.structures = profile.structures or {}
		for index, record in ipairs(profile.structures) do
			if math.abs(record.x - instance.Position.X) < 0.5
				and math.abs(record.y - instance.Position.Y) < 0.5
				and math.abs(record.z - instance.Position.Z) < 0.5 then
				table.remove(profile.structures, index)
				break
			end
		end
		remotes.Fire(player, "InventoryUpdate", require(Shared.InventoryModel).serialize(profile.inventory))
	end
	pieces[instance] = nil
	instance:Destroy()
	remotes.Fire(player, "BuildResult", true, info.pieceId, "Dismantled")
	return true
end

function BuildingService.Repair(player, instance)
	if typeof(instance) ~= "instance" then return false end
	local info = pieces[instance]
	if not info then return false end
	local profile = registryRef.PlayerDataService.Get(player)
	if not profile then return false end
	local def = Build[info.pieceId]
	if not def then return false end
	local maxHealth = BuildLogic.healthFor(info.pieceId, 1)
	if info.health >= maxHealth then
		remotes.Fire(player, "Notify", "Already in good condition.", "info", 2)
		return false
	end
	local have = require(Shared.InventoryModel).countItem(profile.inventory, "scrap_metal", true)
	if have < 2 then
		remotes.Fire(player, "Notify", "Needs 2 scrap metal.", "warn", 2.4)
		return false
	end
	require(Shared.InventoryModel).consumeItem(profile.inventory, "scrap_metal", 2)
	info.health = math.min(maxHealth, info.health + maxHealth * 0.4)
	remotes.Fire(player, "Notify", "Repaired.", "info", 2)
	return true
end

function BuildingService.Restore(player)
	local profile = registryRef.PlayerDataService.Get(player)
	if not profile or type(profile.structures) ~= "table" then return end
	for _, record in ipairs(profile.structures) do
		local def = Build[record.pieceId]
		if def then
			local position = Vector3.new(record.x, record.y, record.z)
			local part = makePart(def, position, record.rotation, player)
			pieces[part] = {
				pieceId = record.pieceId, owner = player.UserId, rotation = record.rotation,
				health = BuildLogic.healthFor(record.pieceId, 1),
			}
		end
	end
end

function BuildingService.DamageStructure(part, amount)
	local info = pieces[part]
	if not info then return end
	info.health -= amount
	if info.health <= 0 then
		pieces[part] = nil
		part:Destroy()
	end
end

function BuildingService.Count()
	local n = 0
	for _ in pairs(pieces) do n += 1 end
	return n
end

return BuildingService
