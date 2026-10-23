--!strict
--[[
	Remotes — declarative list of every RemoteEvent/RemoteFunction.

	The server creates them (ServerScriptService/Remotes.lua) and the client waits
	for them, so both sides always agree on names and argument order.

	Arity checks on the server (RemoteGuard) are generated from this list.
]]

local Remotes = {}

Remotes.Events = {
	-- client -> server
	"RequestMove",         -- (state: {sprint, crouch, crawl, aiming})
	"RequestInteract",     -- (instance)
	"RequestAttack",       -- (aimDirection, origin)
	"RequestReload",       -- ()
	"RequestEquip",        -- (slot, itemUid)
	"RequestUseItem",      -- (uid)
	"RequestDropItem",     -- (uid, count)
	"RequestMoveItem",     -- (uid, fromContainer, fromX, fromY, toContainer, toX, toY)
	"RequestCraft",        -- (recipeId)
	"RequestBuild",        -- (pieceId, cframe, rotation)
	"RequestDismantle",    -- (instance)
	"RequestRepair",       -- (instance)
	"RequestQuestAction",  -- (questId, objectiveId)
	"RequestLoot",         -- (instance)
	"RequestStorage",      -- (instance)
	"RequestSprint",       -- (boolean)
	"RequestCrouch",       -- (boolean)
	"RequestAim",          -- (boolean)
	"RequestFlashlight",   -- (boolean)
	"RequestConsume",      -- (uid)
	"RequestRadio",        -- (instance)
	"RequestSleep",        -- (instance)
	"RequestSettings",     -- (settingsTable)
	"RequestMapPing",      -- ()
	"RequestRevive",       -- (player)
	"RequestDropAll",      -- ()
	"RequestSafehouse",    -- (instance)
	"RequestDocument",     -- (docId)
	"RequestParty",        -- (action, targetName)
	"RequestPing",         -- (position)

	-- server -> client
	"StateSync",           -- (stateTable) full survival/inventory/quest snapshot
	"SurvivalUpdate",      -- (survivalTable)
	"InventoryUpdate",     -- (inventoryTable)
	"EquipmentUpdate",     -- (equipmentTable)
	"QuestUpdate",         -- (questTable)
	"Notify",              -- (text, kind, duration)
	"CraftResult",         -- (ok, recipeId, message)
	"BuildResult",         -- (ok, pieceId, message)
	"LootResult",          -- (ok, items, containerName)
	"CombatFeedback",      -- (kind, data)
	"EnemyAlert",          -- (threatTable)
	"Atmosphere",          -- (weatherTable)
	"StoryEvent",          -- (eventId, data)
	"DocumentShow",        -- (doc)
	"DeathScreen",         -- (cause)
	"RevivePrompt",        -- (player, enabled)
	"InteractionHint",     -- (text, progressMax)
	"ProximityLoot",       -- (items, containerName)
	"BuildingMode",        -- (enabled, pieceId)
	"MinimapPing",         -- (position, kind, label)
	"ServerTime",          -- (clockHours, dayNumber)
	"TutorialStep",        -- (stepId, text)
}

Remotes.Functions = {
	"GetInitialState",     -- () -> stateTable
	"GetItemInfo",         -- (itemId) -> info
	"GetServerInfo",       -- () -> { players, uptime, weather }
}

Remotes.FolderName = "Remotes"

return Remotes
