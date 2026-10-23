--!strict
--[[
    ASHES OF HUMANITY: LAST SIGNAL
    ServerScriptService.Services.InventoryService
    Server-Authoritative 2D Spatial Grid Inventory, Equipment, Hotbar,
    Weight, Durability, Sorting, Consumables, and World Item Drops (Section 17).
]]

local ReplicatedStorage = game:GetService("ReplicatedStorage")
local Workspace = game:GetService("Workspace")

local Config = require(ReplicatedStorage.Shared.Config)
local ItemDefinitions = require(ReplicatedStorage.Shared.ItemDefinitions)
local NetworkSchema = require(ReplicatedStorage.Shared.NetworkSchema)
local Types = require(ReplicatedStorage.Shared.Types)
local UtilityModules = require(ReplicatedStorage.Shared.UtilityModules)
local WeaponDefinitions = require(ReplicatedStorage.Shared.WeaponDefinitions)

local InventoryService = {}

local registry: any = nil

local function getItemDims(def: ItemDefinitions.ItemDef, rotated: boolean): (number, number)
	if rotated then
		return def.Height, def.Width
	end
	return def.Width, def.Height
end

local function isEquippedUID(inv: Types.InventoryState, uid: string): boolean
	for _, eqUid in pairs(inv.Equipped) do
		if eqUid == uid then
			return true
		end
	end
	return false
end

local function buildOccupancyGrid(inv: Types.InventoryState, ignoreUid: string?): { [number]: { [number]: string } }
	local grid: { [number]: { [number]: string } } = {}
	for y = 1, inv.Rows do
		grid[y] = {}
	end
	for uid, item in pairs(inv.Items) do
		if uid ~= ignoreUid and item.X > 0 and item.Y > 0 and not isEquippedUID(inv, uid) then
			local def = ItemDefinitions.Get(item.ItemId)
			if def then
				local w, h = getItemDims(def, item.Rotated)
				for dy = 0, h - 1 do
					local row = grid[item.Y + dy]
					if row then
						for dx = 0, w - 1 do
							row[item.X + dx] = uid
						end
					end
				end
			end
		end
	end
	return grid
end

local function canFitAt(
	inv: Types.InventoryState,
	occ: { [number]: { [number]: string } },
	x: number,
	y: number,
	w: number,
	h: number
): boolean
	if x < 1 or y < 1 or (x + w - 1) > inv.Cols or (y + h - 1) > inv.Rows then
		return false
	end
	for dy = 0, h - 1 do
		local row = occ[y + dy]
		if not row then
			return false
		end
		for dx = 0, w - 1 do
			if row[x + dx] ~= nil then
				return false
			end
		end
	end
	return true
end

local function findFirstSlot(
	inv: Types.InventoryState,
	def: ItemDefinitions.ItemDef,
	ignoreUid: string?
): (number?, number?, boolean)
	local occ = buildOccupancyGrid(inv, ignoreUid)
	for _, rot in ipairs({ false, true }) do
		local w, h = getItemDims(def, rot)
		for y = 1, inv.Rows - h + 1 do
			for x = 1, inv.Cols - w + 1 do
				if canFitAt(inv, occ, x, y, w, h) then
					return x, y, rot
				end
			end
		end
		if def.Width == def.Height then
			break
		end
	end
	return nil, nil, false
end

function InventoryService:Init(serviceRegistry: any)
	registry = serviceRegistry
end

function InventoryService:Start() end

function InventoryService:RecalculateCapacityAndWeight(profile: Types.PlayerSaveProfile)
	local inv = profile.Inventory
	local extraCols = 0
	local extraRows = 0
	local extraWeight = 0.0

	local packUid = inv.Equipped.Backpack
	if packUid and inv.Items[packUid] then
		local packDef = ItemDefinitions.Get(inv.Items[packUid].ItemId)
		if packDef then
			extraCols = packDef.ExtraCols or 0
			extraRows = packDef.ExtraRows or 0
			extraWeight = packDef.ExtraWeightKg or 0.0
		end
	end

	inv.Cols = Config.INVENTORY.BaseCols + extraCols
	inv.Rows = Config.INVENTORY.BaseRows + extraRows
	inv.MaxWeightKg = Config.INVENTORY.BaseMaxWeightKg + extraWeight

	local totalW = 0.0
	for _, item in pairs(inv.Items) do
		local def = ItemDefinitions.Get(item.ItemId)
		if def then
			totalW += def.WeightKg * math.max(1, item.Count)
		end
	end
	inv.CurrentWeightKg = math.floor(totalW * 100 + 0.5) / 100
end

function InventoryService:GrantStarterKit(player: Player)
	local profile = registry.PlayerDataService:GetProfile(player)
	if not profile then
		return
	end
	local inv = profile.Inventory

	-- Equip starter gear first so backpack expands grid rows immediately
	for slot, spec in pairs(ItemDefinitions.STARTER_LOADOUT.Equipped) do
		local def = ItemDefinitions.Get(spec.ItemId)
		if def then
			local uid = UtilityModules.GenerateUID("itm")
			local wpnDef = if def.WeaponId then WeaponDefinitions.Get(def.WeaponId) else nil
			inv.Items[uid] = {
				UID = uid,
				ItemId = def.Id,
				Count = 1,
				Durability = spec.Durability or def.MaxDurability,
				MaxDurability = def.MaxDurability,
				ClipAmmo = if wpnDef and wpnDef.MagSize > 0 then wpnDef.MagSize else nil,
				X = 0,
				Y = 0,
				Rotated = false,
			}
			inv.Equipped[slot] = uid
			if slot == "Melee" then
				inv.Hotbar[1] = uid
			end
		end
	end
	self:RecalculateCapacityAndWeight(profile)

	-- Add starter supplies into spatial grid
	local nextHotbar = 2
	for _, entry in ipairs(ItemDefinitions.STARTER_LOADOUT.Inventory) do
		local ok, addedUids = self:AddItem(player, entry.ItemId, entry.Count, nil, true)
		if ok and addedUids and #addedUids > 0 and nextHotbar <= 5 then
			local def = ItemDefinitions.Get(entry.ItemId)
			if
				def
				and (
					def.Category == "Medicine"
					or def.Category == "Food"
					or def.Category == "Water"
					or def.Id == "OldFieldRadio"
				)
			then
				inv.Hotbar[nextHotbar] = addedUids[1]
				nextHotbar += 1
			end
		end
	end
	self:SyncInventory(player)
end

function InventoryService:CountItem(player: Player, itemId: string): number
	local profile = registry.PlayerDataService:GetProfile(player)
	if not profile then
		return 0
	end
	local total = 0
	for _, inst in pairs(profile.Inventory.Items) do
		if inst.ItemId == itemId then
			total += inst.Count
		end
	end
	return total
end

function InventoryService:RemoveItemById(player: Player, itemId: string, count: number, skipSync: boolean?): boolean
	if count <= 0 then
		return true
	end
	local profile = registry.PlayerDataService:GetProfile(player)
	if not profile then
		return false
	end
	if self:CountItem(player, itemId) < count then
		return false
	end

	local remaining = count
	local inv = profile.Inventory
	-- Prefer removing unequipped stacks first
	for uid, inst in pairs(inv.Items) do
		if inst.ItemId == itemId and not isEquippedUID(inv, uid) then
			local take = math.min(remaining, inst.Count)
			inst.Count -= take
			remaining -= take
			if inst.Count <= 0 then
				inv.Items[uid] = nil
				for slotIdx, hUid in pairs(inv.Hotbar) do
					if hUid == uid then
						inv.Hotbar[slotIdx] = nil
					end
				end
			end
			if remaining <= 0 then
				break
			end
		end
	end

	-- Fallback to equipped if still needed
	if remaining > 0 then
		for uid, inst in pairs(inv.Items) do
			if inst.ItemId == itemId then
				local take = math.min(remaining, inst.Count)
				inst.Count -= take
				remaining -= take
				if inst.Count <= 0 then
					inv.Items[uid] = nil
					for slotName, eqUid in pairs(inv.Equipped) do
						if eqUid == uid then
							inv.Equipped[slotName] = nil
						end
					end
				end
				if remaining <= 0 then
					break
				end
			end
		end
	end

	self:RecalculateCapacityAndWeight(profile)
	if not skipSync then
		self:SyncInventory(player)
	end
	return true
end

function InventoryService:AddItem(
	player: Player,
	itemId: string,
	count: number,
	customDurability: number?,
	skipSync: boolean?
): (boolean, { string }?)
	local def = ItemDefinitions.Get(itemId)
	if not def or count <= 0 then
		return false, nil
	end
	local profile = registry.PlayerDataService:GetProfile(player)
	if not profile then
		return false, nil
	end

	local inv = profile.Inventory
	local remaining = math.floor(count)
	local touchedUids: { string } = {}

	-- 1. Stack into existing non-full stacks first
	if def.MaxStack > 1 then
		for uid, inst in pairs(inv.Items) do
			if inst.ItemId == itemId and inst.Count < def.MaxStack then
				local space = def.MaxStack - inst.Count
				local add = math.min(space, remaining)
				inst.Count += add
				remaining -= add
				table.insert(touchedUids, uid)
				if remaining <= 0 then
					break
				end
			end
		end
	end

	-- 2. Allocate new grid cells for remainder
	while remaining > 0 do
		local stackAmt = math.min(def.MaxStack, remaining)
		local gx, gy, rot = findFirstSlot(inv, def, nil)
		if gx == nil or gy == nil then
			-- Grid full: stop here
			break
		end
		local uid = UtilityModules.GenerateUID("itm")
		local wpnDef = if def.WeaponId then WeaponDefinitions.Get(def.WeaponId) else nil
		inv.Items[uid] = {
			UID = uid,
			ItemId = itemId,
			Count = stackAmt,
			Durability = customDurability or def.MaxDurability,
			MaxDurability = def.MaxDurability,
			ClipAmmo = if wpnDef and wpnDef.MagSize > 0 then wpnDef.MagSize else nil,
			X = gx,
			Y = gy,
			Rotated = rot,
		}
		table.insert(touchedUids, uid)
		remaining -= stackAmt
	end

	self:RecalculateCapacityAndWeight(profile)
	if registry.SurvivalService then
		registry.SurvivalService:ApplyMobilityModifiers(player, profile)
	end
	if registry.QuestService and #touchedUids > 0 then
		registry.QuestService:NotifyProgress(player, "CollectItem", itemId, count - remaining)
	end
	if not skipSync then
		self:SyncInventory(player)
	end

	return remaining == 0, touchedUids
end

function InventoryService:MoveOrRotateItem(
	player: Player,
	uid: string,
	targetX: number,
	targetY: number,
	rotated: boolean
): (boolean, string?)
	local profile = registry.PlayerDataService:GetProfile(player)
	if not profile then
		return false, "NoProfile"
	end
	local inv = profile.Inventory
	local inst = inv.Items[uid]
	if not inst then
		return false, "MissingItem"
	end
	local def = ItemDefinitions.Get(inst.ItemId)
	if not def then
		return false, "InvalidDef"
	end

	local w, h = getItemDims(def, rotated)
	local occ = buildOccupancyGrid(inv, uid)

	-- Check if dropping onto a compatible stackable item
	if occ[targetY] and occ[targetY][targetX] then
		local otherUid = occ[targetY][targetX]
		local other = inv.Items[otherUid]
		if
			other
			and other.UID ~= uid
			and other.ItemId == inst.ItemId
			and def.MaxStack > 1
			and other.Count < def.MaxStack
		then
			local transfer = math.min(def.MaxStack - other.Count, inst.Count)
			other.Count += transfer
			inst.Count -= transfer
			if inst.Count <= 0 then
				inv.Items[uid] = nil
			end
			self:RecalculateCapacityAndWeight(profile)
			self:SyncInventory(player)
			return true, nil
		end
	end

	if not canFitAt(inv, occ, targetX, targetY, w, h) then
		return false, "Blocked"
	end

	-- If item was equipped, unequip it first
	for slot, eqUid in pairs(inv.Equipped) do
		if eqUid == uid then
			if slot == "Backpack" then
				-- Ensure items still fit without extra rows
				local baseRows = Config.INVENTORY.BaseRows
				for otherId, otherItem in pairs(inv.Items) do
					if otherId ~= uid and otherItem.Y > 0 then
						local oDef = ItemDefinitions.Get(otherItem.ItemId)
						if oDef then
							local _, oh = getItemDims(oDef, otherItem.Rotated)
							if otherItem.Y + oh - 1 > baseRows or targetY + h - 1 > baseRows then
								return false, "BackpackRowsInUse"
							end
						end
					end
				end
			end
			inv.Equipped[slot] = nil
		end
	end

	inst.X = targetX
	inst.Y = targetY
	inst.Rotated = rotated
	self:RecalculateCapacityAndWeight(profile)
	self:SyncInventory(player)
	return true, nil
end

function InventoryService:EquipItem(player: Player, uid: string, requestedSlot: string?): (boolean, string?)
	local profile = registry.PlayerDataService:GetProfile(player)
	if not profile then
		return false, "NoProfile"
	end
	local inv = profile.Inventory
	local inst = inv.Items[uid]
	if not inst then
		return false, "MissingItem"
	end
	local def = ItemDefinitions.Get(inst.ItemId)
	if not def or not def.EquipSlot then
		return false, "NotEquippable"
	end

	local slot = def.EquipSlot
	if requestedSlot and requestedSlot ~= slot then
		return false, "SlotMismatch"
	end

	local prevUid = inv.Equipped[slot]
	if prevUid == uid then
		return true, nil
	end

	-- Remove new item from grid temporarily so previous item can take space
	local oldX, oldY, oldRot = inst.X, inst.Y, inst.Rotated
	inst.X = 0
	inst.Y = 0
	inv.Equipped[slot] = uid
	self:RecalculateCapacityAndWeight(profile)

	if prevUid and inv.Items[prevUid] then
		local prevItem = inv.Items[prevUid]
		local prevDef = ItemDefinitions.Get(prevItem.ItemId)
		if prevDef then
			local gx, gy, grot = findFirstSlot(inv, prevDef, prevUid)
			if gx == nil or gy == nil then
				-- Rollback
				inv.Equipped[slot] = prevUid
				inst.X, inst.Y, inst.Rotated = oldX, oldY, oldRot
				self:RecalculateCapacityAndWeight(profile)
				return false, "NoSpaceToSwap"
			end
			prevItem.X = gx
			prevItem.Y = gy
			prevItem.Rotated = grot
		end
	end

	if registry.SurvivalService then
		registry.SurvivalService:ApplyMobilityModifiers(player, profile)
	end
	self:SyncCharEquippedAttribute(player, profile)
	self:SyncInventory(player)
	return true, nil
end

function InventoryService:UnequipSlot(player: Player, slot: string): (boolean, string?)
	local profile = registry.PlayerDataService:GetProfile(player)
	if not profile then
		return false, "NoProfile"
	end
	local inv = profile.Inventory
	local uid = inv.Equipped[slot]
	if not uid or not inv.Items[uid] then
		return false, "EmptySlot"
	end
	local inst = inv.Items[uid]
	local def = ItemDefinitions.Get(inst.ItemId)
	if not def then
		return false, "InvalidDef"
	end

	if slot == "Backpack" then
		local baseRows = Config.INVENTORY.BaseRows
		for otherId, otherItem in pairs(inv.Items) do
			if otherId ~= uid and otherItem.Y > 0 then
				local oDef = ItemDefinitions.Get(otherItem.ItemId)
				if oDef then
					local _, oh = getItemDims(oDef, otherItem.Rotated)
					if otherItem.Y + oh - 1 > baseRows then
						return false, "EmptyBottomRowsFirst"
					end
				end
			end
		end
	end

	inv.Equipped[slot] = nil
	self:RecalculateCapacityAndWeight(profile)
	local gx, gy, grot = findFirstSlot(inv, def, uid)
	if gx == nil or gy == nil then
		inv.Equipped[slot] = uid
		self:RecalculateCapacityAndWeight(profile)
		return false, "InventoryFull"
	end

	inst.X = gx
	inst.Y = gy
	inst.Rotated = grot
	if slot == "Utility" then
		profile.Vitals.FlashlightOn = false
	end
	self:SyncCharEquippedAttribute(player, profile)
	self:SyncInventory(player)
	return true, nil
end

function InventoryService:UseItem(player: Player, uid: string, targetBodyPart: string?): (boolean, string?)
	local profile = registry.PlayerDataService:GetProfile(player)
	if not profile then
		return false, "NoProfile"
	end
	local inv = profile.Inventory
	local inst = inv.Items[uid]
	if not inst then
		return false, "MissingItem"
	end
	local def = ItemDefinitions.Get(inst.ItemId)
	if not def then
		return false, "InvalidDef"
	end

	-- Equippable shortcut if double-clicked
	if def.EquipSlot and not def.Effects then
		return self:EquipItem(player, uid, nil)
	end

	if not def.Effects then
		return false, "NotUsable"
	end

	-- Repair Kit handling
	if def.Effects.RepairDurability then
		local repaired = false
		for _, slotName in ipairs({ "Primary", "Secondary", "Melee", "Armor", "Headgear" }) do
			local eqUid = inv.Equipped[slotName]
			if eqUid and inv.Items[eqUid] then
				local eqItem = inv.Items[eqUid]
				if eqItem.Durability < eqItem.MaxDurability then
					eqItem.Durability = math.min(eqItem.MaxDurability, eqItem.Durability + def.Effects.RepairDurability)
					repaired = true
					break
				end
			end
		end
		if not repaired then
			return false, "NothingNeedsRepair"
		end
	else
		registry.SurvivalService:ApplyConsumableEffects(player, def.Effects, targetBodyPart)
	end

	inst.Count -= 1
	if inst.Count <= 0 then
		inv.Items[uid] = nil
		for hIdx, hUid in pairs(inv.Hotbar) do
			if hUid == uid then
				inv.Hotbar[hIdx] = nil
			end
		end
	end

	self:RecalculateCapacityAndWeight(profile)
	self:SyncInventory(player)
	return true, nil
end

function InventoryService:SortInventory(player: Player): boolean
	local profile = registry.PlayerDataService:GetProfile(player)
	if not profile then
		return false
	end
	local inv = profile.Inventory

	-- Collect unequipped items and merge partial stacks
	local unequipped: { Types.GridItemInstance } = {}
	for uid, inst in pairs(inv.Items) do
		if not isEquippedUID(inv, uid) then
			inst.X = 0
			inst.Y = 0
			table.insert(unequipped, inst)
		end
	end

	-- Sort largest area first so 2D bin-packing is tight
	table.sort(unequipped, function(a, b)
		local da = ItemDefinitions.Get(a.ItemId)
		local db = ItemDefinitions.Get(b.ItemId)
		local areaA = if da then da.Width * da.Height else 1
		local areaB = if db then db.Width * db.Height else 1
		if areaA ~= areaB then
			return areaA > areaB
		end
		return a.ItemId < b.ItemId
	end)

	for _, inst in ipairs(unequipped) do
		local def = ItemDefinitions.Get(inst.ItemId)
		if def then
			local gx, gy, grot = findFirstSlot(inv, def, inst.UID)
			if gx and gy then
				inst.X = gx
				inst.Y = gy
				inst.Rotated = grot
			end
		end
	end

	self:SyncInventory(player)
	return true
end

function InventoryService:DropItemToWorld(player: Player, uid: string): (boolean, string?)
	local profile = registry.PlayerDataService:GetProfile(player)
	if not profile then
		return false, "NoProfile"
	end
	local inv = profile.Inventory
	local inst = inv.Items[uid]
	if not inst then
		return false, "MissingItem"
	end
	local def = ItemDefinitions.Get(inst.ItemId)
	if not def or def.Category == "QuestItems" then
		return false, "CannotDropQuestItem"
	end

	-- Remove from inventory BEFORE spawning world pickup (prevents duplication!)
	inv.Items[uid] = nil
	for slot, eqUid in pairs(inv.Equipped) do
		if eqUid == uid then
			inv.Equipped[slot] = nil
		end
	end
	for hIdx, hUid in pairs(inv.Hotbar) do
		if hUid == uid then
			inv.Hotbar[hIdx] = nil
		end
	end
	self:RecalculateCapacityAndWeight(profile)
	self:SyncCharEquippedAttribute(player, profile)
	self:SyncInventory(player)

	local root = registry.SecurityService:GetCharacterRoot(player)
	local dropPos = if root
		then (root.Position + root.CFrame.LookVector * 3.0 + Vector3.new(0, -1.2, 0))
		else Config.SHELTER_SPAWN_POS
	if registry.LootService then
		registry.LootService:SpawnDroppedItem(inst.ItemId, inst.Count, inst.Durability, dropPos)
	end
	return true, nil
end

function InventoryService:BindHotbar(player: Player, slotIndex: number, uid: string?): boolean
	if slotIndex < 1 or slotIndex > Config.INVENTORY.HotbarSlots then
		return false
	end
	local profile = registry.PlayerDataService:GetProfile(player)
	if not profile then
		return false
	end
	local inv = profile.Inventory
	if uid == nil or uid == "" then
		inv.Hotbar[slotIndex] = nil
	elseif inv.Items[uid] then
		inv.Hotbar[slotIndex] = uid
	else
		return false
	end
	self:SyncInventory(player)
	return true
end

function InventoryService:SyncCharEquippedAttribute(player: Player, profile: Types.PlayerSaveProfile)
	local char = player.Character
	if not char then
		return
	end
	local activeWeaponId = ""
	for _, slotName in ipairs({ "Primary", "Secondary", "Melee" }) do
		local u = profile.Inventory.Equipped[slotName]
		if u and profile.Inventory.Items[u] then
			local def = ItemDefinitions.Get(profile.Inventory.Items[u].ItemId)
			if def and def.WeaponId then
				activeWeaponId = def.WeaponId
				break
			end
		end
	end
	char:SetAttribute("ActiveWeaponId", activeWeaponId)
end

function InventoryService:HandleRemoteAction(player: Player, payload: any)
	if type(payload) ~= "table" or not NetworkSchema.ValidateString(payload.Action, 32) then
		return
	end
	registry.SecurityService:WithTransactionLock(player, function()
		local act = payload.Action
		if act == "Move" then
			if
				NetworkSchema.ValidateString(payload.UID, 64)
				and NetworkSchema.ValidateInt(payload.X, 1, 16)
				and NetworkSchema.ValidateInt(payload.Y, 1, 16)
			then
				return self:MoveOrRotateItem(player, payload.UID, payload.X, payload.Y, payload.Rotated == true)
			end
		elseif act == "Equip" then
			if NetworkSchema.ValidateString(payload.UID, 64) then
				local slot = if type(payload.Slot) == "string" then payload.Slot else nil
				return self:EquipItem(player, payload.UID, slot)
			end
		elseif act == "Unequip" then
			if NetworkSchema.ValidateString(payload.Slot, 24) then
				return self:UnequipSlot(player, payload.Slot)
			end
		elseif act == "Use" then
			if NetworkSchema.ValidateString(payload.UID, 64) then
				local part = if type(payload.BodyPart) == "string" then payload.BodyPart else nil
				return self:UseItem(player, payload.UID, part)
			end
		elseif act == "Drop" then
			if NetworkSchema.ValidateString(payload.UID, 64) then
				return self:DropItemToWorld(player, payload.UID)
			end
		elseif act == "Sort" then
			return self:SortInventory(player), nil
		elseif act == "BindHotbar" then
			if NetworkSchema.ValidateInt(payload.SlotIndex, 1, 5) then
				local u = if type(payload.UID) == "string" then payload.UID else nil
				return self:BindHotbar(player, payload.SlotIndex, u), nil
			end
		end
		return false, "InvalidAction"
	end)
end

function InventoryService:SyncInventory(player: Player)
	local profile = registry.PlayerDataService:GetProfile(player)
	if not profile then
		return
	end
	registry.SendToPlayer(player, NetworkSchema.EVENTS.InventoryUpdate, profile.Inventory)
end

return InventoryService
