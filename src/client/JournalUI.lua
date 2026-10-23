--!strict
--[[
	JournalUI — contracts, read documents and the story timeline.
]]

local ReplicatedStorage = game:GetService("ReplicatedStorage")

local Store = require(script.Parent.StateStore)
local Theme = require(script.Parent.UITheme)
local UI = require(script.Parent.UIBuilder)
local Network = require(script.Parent.Network)

local Shared = ReplicatedStorage:WaitForChild("Shared")
local Quests = require(Shared.QuestDefinitions)
local Story = require(Shared.StoryData)

local JournalUI = {}

local gui
local root
local listView
local detailView
local tab = "quests"
local selected = nil

local function findQuest(id)
	return Quests[id]
end

local function drawDetail()
	if not detailView then return end
	UI.clear(detailView)
	if tab == "quests" then
		local quests = Store.get("quests")
		local quest = selected and Quests[selected]
		if not quest then
			UI.label(detailView, {
				size = UDim2.fromScale(1, 1),
				text = "Select a contract.",
				textSize = 12,
				font = Theme.Fonts.mono,
				textColor = Theme.Colors.textFaint,
				alignX = Enum.TextXAlignment.Center,
			})
			return
		end
		local state = quests.active[selected]
		UI.label(detailView, {
			size = UDim2.new(1, 0, 0, 24),
			text = string.upper(quest.name),
			textSize = 16,
			font = Theme.Fonts.bold,
			textColor = Theme.Colors.accent,
		})
		UI.label(detailView, {
			size = UDim2.new(1, 0, 0, 68),
			position = UDim2.fromOffset(0, 30),
			text = quest.brief or "",
			textSize = 11,
			font = Theme.Fonts.regular,
			textColor = Theme.Colors.textDim,
			wrapped = true,
			alignY = Enum.TextYAlignment.Top,
		})
		local y = 104
		UI.label(detailView, {
			size = UDim2.new(1, 0, 0, 14),
			position = UDim2.fromOffset(0, y),
			text = "OBJECTIVES",
			textSize = 10,
			font = Theme.Fonts.mono,
			textColor = Theme.Colors.textFaint,
		})
		y += 18
		if state then
			for _, objective in ipairs(state.objectives or {}) do
				local mark = objective.done and "[x]" or "[ ]"
				UI.label(detailView, {
					size = UDim2.new(1, 0, 0, 16),
					position = UDim2.fromOffset(0, y),
					text = string.format("%s %s%s", mark, objective.text,
						objective.count and objective.count > 1
							and string.format(" (%d/%d)", objective.progress or 0, objective.count) or ""),
					textSize = 11,
					font = Theme.Fonts.mono,
					textColor = objective.done and Theme.Colors.success or Theme.Colors.text,
				})
				y += 18
			end
		else
			UI.label(detailView, {
				size = UDim2.new(1, 0, 0, 16),
				position = UDim2.fromOffset(0, y),
				text = "Not started.",
				textSize = 11,
				font = Theme.Fonts.mono,
				textColor = Theme.Colors.textFaint,
			})
			y += 18
		end
		if quest.completion then
			UI.label(detailView, {
				size = UDim2.new(1, 0, 0, 50),
				position = UDim2.new(0, 0, 1, -70),
				text = quest.completion,
				textSize = 10,
				font = Theme.Fonts.regular,
				textColor = Theme.Colors.textFaint,
				wrapped = true,
				alignY = Enum.TextYAlignment.Top,
			})
		end
	else
		local id = selected
		local doc = id and Story.Documents[id]
		if not doc then
			UI.label(detailView, {
				size = UDim2.fromScale(1, 1),
				text = "You have not read any documents yet.",
				textSize = 12,
				font = Theme.Fonts.mono,
				textColor = Theme.Colors.textFaint,
				alignX = Enum.TextXAlignment.Center,
			})
			return
		end
		UI.label(detailView, {
			size = UDim2.new(1, 0, 0, 22),
			text = string.upper(doc.title),
			textSize = 15,
			font = Theme.Fonts.bold,
			textColor = Theme.Colors.accent,
		})
		UI.label(detailView, {
			size = UDim2.new(1, 0, 0, 14),
			position = UDim2.fromOffset(0, 24),
			text = string.upper(doc.location or ""),
			textSize = 9,
			font = Theme.Fonts.mono,
			textColor = Theme.Colors.textFaint,
		})
		UI.label(detailView, {
			size = UDim2.new(1, 0, 1, -48),
			position = UDim2.fromOffset(0, 44),
			text = doc.body,
			textSize = 11,
			font = Theme.Fonts.regular,
			textColor = Theme.Colors.textDim,
			wrapped = true,
			alignY = Enum.TextYAlignment.Top,
		})
	end
end

local function drawList()
	if not listView then return end
	UI.clear(listView)
	local index = 0
	if tab == "quests" then
		local quests = Store.get("quests")
		for id, state in pairs(quests.active or {}) do
			index += 1
			local quest = Quests[id]
			if quest then
				local row = UI.frame(listView, {
					size = UDim2.new(1, -6, 0, 46),
					position = UDim2.fromOffset(0, (index - 1) * 50),
					color = (id == selected) and Theme.Colors.panelAlt or Theme.Colors.panelDeep,
					strokeColor = Theme.Colors.border,
					strokeTransparency = (id == selected) and 0 or 0.7,
					zIndex = 22,
				})
				UI.label(row, {
					size = UDim2.new(1, -10, 0, 18),
					position = UDim2.fromOffset(6, 4),
					text = string.upper(quest.name),
					textSize = 11,
					font = Theme.Fonts.medium,
					textColor = quest.side and Theme.Colors.textDim or Theme.Colors.accent,
					truncate = Enum.TextTruncate.AtEnd,
				})
				UI.label(row, {
					size = UDim2.new(1, -10, 0, 22),
					position = UDim2.fromOffset(6, 20),
					text = quest.brief or "",
					textSize = 9,
					font = Theme.Fonts.mono,
					textColor = Theme.Colors.textFaint,
					wrapped = true,
				})
				local btn = Instance.new("TextButton")
				btn.BackgroundTransparency = 1
				btn.Text = ""
				btn.Size = UDim2.fromScale(1, 1)
				btn.Parent = row
				btn.MouseButton1Click:Connect(function()
					selected = id
					drawList()
					drawDetail()
				end)
			end
		end
		if index == 0 then
			UI.label(listView, {
				size = UDim2.new(1, 0, 0, 30),
				text = "No active contracts.",
				textSize = 11,
				font = Theme.Fonts.mono,
				textColor = Theme.Colors.textFaint,
			})
		end
	else
		local profile = Store.get("profile") or {}
		local read = profile.documentsRead or {}
		for id, doc in pairs(Story.Documents) do
			if read[id] then
				index += 1
				local row = UI.frame(listView, {
					size = UDim2.new(1, -6, 0, 42),
					position = UDim2.fromOffset(0, (index - 1) * 46),
					color = (id == selected) and Theme.Colors.panelAlt or Theme.Colors.panelDeep,
					strokeColor = Theme.Colors.border,
					strokeTransparency = (id == selected) and 0 or 0.7,
					zIndex = 22,
				})
				UI.label(row, {
					size = UDim2.new(1, -10, 0, 16),
					position = UDim2.fromOffset(6, 4),
					text = string.upper(doc.title),
					textSize = 10,
					font = Theme.Fonts.medium,
					truncate = Enum.TextTruncate.AtEnd,
				})
				UI.label(row, {
					size = UDim2.new(1, -10, 0, 14),
					position = UDim2.fromOffset(6, 20),
					text = doc.location or "",
					textSize = 9,
					font = Theme.Fonts.mono,
					textColor = Theme.Colors.textFaint,
				})
				local btn = Instance.new("TextButton")
				btn.BackgroundTransparency = 1
				btn.Text = ""
				btn.Size = UDim2.fromScale(1, 1)
				btn.Parent = row
				btn.MouseButton1Click:Connect(function()
					selected = id
					drawList()
					drawDetail()
				end)
			end
		end
		if index == 0 then
			UI.label(listView, {
				size = UDim2.new(1, 0, 0, 30),
				text = "No documents recovered.",
				textSize = 11,
				font = Theme.Fonts.mono,
				textColor = Theme.Colors.textFaint,
			})
		end
	end
end

function JournalUI.Build(parent)
	gui = parent
	root = UI.frame(gui, {
		name = "Journal",
		size = UDim2.fromOffset(720, 460),
		position = UDim2.fromScale(0.5, 0.5),
		anchor = Vector2.new(0.5, 0.5),
		color = Theme.Colors.bg,
		visible = false,
		zIndex = 20,
	})
	UI.label(root, {
		size = UDim2.new(1, -24, 0, 30),
		position = UDim2.fromOffset(16, 10),
		text = "SURVIVOR JOURNAL",
		textSize = 16,
		font = Theme.Fonts.bold,
	})

	local tabs = { { "quests", "CONTRACTS" }, { "documents", "DOCUMENTS" } }
	for index, entry in ipairs(tabs) do
		local btn = UI.button(root, {
			size = UDim2.fromOffset(130, 24),
			position = UDim2.fromOffset(16 + (index - 1) * 136, 46),
			text = entry[2],
			color = Theme.Colors.panel,
			textSize = 10,
			font = Theme.Fonts.mono,
		})
		btn.MouseButton1Click:Connect(function()
			tab = entry[1]
			selected = nil
			drawList()
			drawDetail()
		end)
	end

	local listHolder = UI.frame(root, {
		size = UDim2.fromOffset(290, 360),
		position = UDim2.fromOffset(16, 80),
		color = Theme.Colors.panel,
		padding = 6,
	})
	listView = UI.frame(listHolder, {
		size = UDim2.fromScale(1, 1),
		transparency = 1,
		stroke = false,
		clip = true,
	})
	detailView = UI.frame(root, {
		size = UDim2.fromOffset(376, 360),
		position = UDim2.fromOffset(318, 80),
		color = Theme.Colors.panel,
		padding = 14,
	})

	Store.on("quests", function()
		if root and root.Visible and tab == "quests" then
			drawList()
			drawDetail()
		end
	end)
end

function JournalUI.Open()
	if not root then return end
	root.Visible = true
	drawList()
	drawDetail()
end

function JournalUI.Close() if root then root.Visible = false end end

function JournalUI.Toggle()
	if not root then return end
	if root.Visible then JournalUI.Close() else JournalUI.Open() end
	return root.Visible
end

function JournalUI.IsOpen() return root ~= nil and root.Visible end

return JournalUI
