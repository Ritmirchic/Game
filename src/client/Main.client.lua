--!strict
--[[
	Main — client entry point.

	Wires the store to the remotes, boots controllers, then runs a single update
	loop. Keep this file boring: it only orchestrates.
]]

local Players = game:GetService("Players")
local ReplicatedStorage = game:GetService("ReplicatedStorage")
local RunService = game:GetService("RunService")
local UserInputService = game:GetService("UserInputService")

local Store = require(script.Parent.StateStore)
local Network = require(script.Parent.Network)
local UIManager = require(script.Parent.UIManager)
local HUD = require(script.Parent.HUD)
local CameraController = require(script.Parent.CameraController)
local MovementController = require(script.Parent.MovementController)
local CombatController = require(script.Parent.CombatController)
local InteractionController = require(script.Parent.InteractionController)
local AtmosphereController = require(script.Parent.AtmosphereController)
local AudioController = require(script.Parent.AudioController)
local MainMenu = require(script.Parent.MainMenuUI)
local MapUI = require(script.Parent.MapUI)

local player = Players.LocalPlayer

-- ------------------------------------------------------------------ boot
UIManager.Build()
CameraController.Start()
MovementController.Start()
CombatController.Start()
InteractionController.Start()
AtmosphereController.Start()
AudioController.Start()

-- ------------------------------------------------------------------ remotes
Network.bindRemoteHooks({
	onEvent = function(events)
		events.CombatFeedback.OnClientEvent:Connect(function(kind, data)
			CombatController.OnCombatFeedback(kind, data)
		end)
		events.DeathScreen.OnClientEvent:Connect(function(cause)
			UIManager.OnDeath(cause)
		end)
		events.StoryEvent.OnClientEvent:Connect(function(eventId, data)
			if eventId == "phenomenon" and data then
				Store.notify(data.text, "info", 6)
				CameraController.Shake(0.5)
			elseif eventId == "transmission" and data then
				Store.notify(data.text, "quest", 8)
			elseif eventId == "chapter_start" and data then
				Store.notify("CHAPTER: " .. string.upper(data.name or ""), "quest", 7)
			elseif eventId == "quest_complete" and data then
				Store.notify("CONTRACT COMPLETE: " .. string.upper(data.name or ""), "quest", 6)
			elseif eventId == "radio_play" and data then
				local lines = table.concat(data.lines or {}, "\n")
				Store.notify("[RADIO] " .. (data.title or "transmission"), "quest", 6)
				if data.note then Store.notify(lines, "info", 9) end
			elseif eventId == "supply_drop" and data and data.position then
				Store.notify("Supply drop marked.", "quest", 4)
			elseif eventId == "sighting" and data and data.position then
				CameraController.Shake(0.8)
			elseif eventId == "revived" and data then
				UIManager.OnRespawn()
				Store.notify("You were revived by " .. tostring(data.by), "quest", 5)
			elseif eventId == "open_crafting" and data then
				Store.notify(string.upper(tostring(data.station or "Workbench")) .. " available", "info", 3)
				UIManager.OpenModal("crafting")
			elseif eventId == "open_building" then
				UIManager.OpenModal("building")
			elseif eventId == "radiation_storm" then
				Store.notify("Geiger counter is screaming.", "danger", 6)
			end
		end)
		events.DocumentShow.OnClientEvent:Connect(function(doc)
			if type(doc) ~= "table" then return end
			Store.notify(doc.title or "Document", "quest", 4)
			Store.set("openDocument", doc)
		end)
		events.TutorialStep.OnClientEvent:Connect(function(step, text)
			HUD.ShowTutorial(string.format("%d. %s", step, text))
		end)
		events.EnemyAlert.OnClientEvent:Connect(function(data)
			if data and data.kind == "scream" then
				CameraController.Shake(0.7)
			end
		end)
		events.InteractionHint.OnClientEvent:Connect(function(text)
			if text then Store.notify(text, "info", 3) end
		end)
		events.MinimapPing.OnClientEvent:Connect(function(position, kind, label)
			MapUI.AddMarker(position, nil, label)
		end)
		events.LootResult.OnClientEvent:Connect(function(ok, items, name)
			if ok then
				Store.notify("Recovered from " .. tostring(name or "container"), "info", 3)
			end
		end)
		events.BuildResult.OnClientEvent:Connect(function(ok, pieceId, message)
			Store.notify(message or (ok and "Built." or "Cannot build there."), ok and "info" or "warn", 3)
		end)
		events.CraftResult.OnClientEvent:Connect(function(ok, recipeId, message)
			Store.notify(message, ok and "info" or "warn", 3)
		end)
		events.RevivePrompt.OnClientEvent:Connect(function(name, kind)
			Store.notify(string.format("%s wants to group up. Press 4 to accept.", tostring(name)), "info", 5)
		end)
	end,
})

-- ------------------------------------------------------------------ initial state
task.spawn(function()
	local snapshot = Network.invoke("GetInitialState")
	if snapshot then
		Store.applySnapshot(snapshot)
	end
	local info = Network.invoke("GetServerInfo")
	if info then
		Store.set("serverInfo", info)
		if info.weather then Store.merge("weather", info.weather) end
	end
	UIManager.StartGame()
	MainMenu.Hide()
	Store.notify("You are awake. Get to the surface.", "info", 6)
end)

-- ------------------------------------------------------------------ loop
local accumulator = 0
RunService.Heartbeat:Connect(function(dt)
	accumulator += dt
	if accumulator < 0.1 then return end
	accumulator = 0
	UIManager.Update(dt)
end)

-- ------------------------------------------------------------------ respawn / death
player.CharacterAdded:Connect(function(character)
	task.wait(1)
	UIManager.OnRespawn()
	local snapshot = Network.invoke("GetInitialState")
	if snapshot then
		Store.applySnapshot(snapshot)
	end
	HUD.ShowTutorial("You are alive. Keep it that way.")
end)

UserInputService.InputBegan:Connect(function(input, processed)
	if processed then return end
	if input.KeyCode == Enum.KeyCode.Four then
		Network.fire("RequestParty", "accept")
	end
end)

print("[Ashes] client ready")
