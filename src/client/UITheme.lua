--!strict
--[[
	UITheme — the visual language of the interface.

	Dark, industrial, high contrast, minimal colour. Nothing in this UI should
	look like a simulator; it should look like salvaged equipment.
]]

local UITheme = {}

UITheme.Colors = {
	bg = Color3.fromRGB(10, 11, 12),
	panel = Color3.fromRGB(16, 17, 19),
	panelAlt = Color3.fromRGB(22, 23, 26),
	panelDeep = Color3.fromRGB(8, 9, 10),
	border = Color3.fromRGB(54, 52, 46),
	borderBright = Color3.fromRGB(112, 104, 84),
	text = Color3.fromRGB(220, 216, 204),
	textDim = Color3.fromRGB(136, 132, 122),
	textFaint = Color3.fromRGB(88, 86, 80),
	accent = Color3.fromRGB(196, 158, 84),
	accentDim = Color3.fromRGB(110, 88, 48),
	health = Color3.fromRGB(176, 48, 44),
	hunger = Color3.fromRGB(168, 122, 62),
	thirst = Color3.fromRGB(74, 118, 150),
	stamina = Color3.fromRGB(112, 142, 88),
	radiation = Color3.fromRGB(146, 166, 62),
	temperature = Color3.fromRGB(140, 164, 186),
	stress = Color3.fromRGB(128, 84, 148),
	danger = Color3.fromRGB(196, 62, 48),
	success = Color3.fromRGB(110, 158, 100),
	warn = Color3.fromRGB(198, 150, 66),
	shadow = Color3.fromRGB(0, 0, 0),
}

UITheme.Fonts = {
	regular = Enum.Font.Gotham,
	mono = Enum.Font.Code,
	bold = Enum.Font.GothamBold,
	medium = Enum.Font.GothamMedium,
	condensed = Enum.Font.GothamCondensed,
}

UITheme.RarityColors = {
	Color3.fromRGB(150, 148, 140),
	Color3.fromRGB(170, 172, 166),
	Color3.fromRGB(110, 150, 170),
	Color3.fromRGB(160, 138, 88),
	Color3.fromRGB(178, 116, 64),
}

function UITheme.rarityColor(rarity)
	return UITheme.RarityColors[math.clamp(rarity or 1, 1, 5)]
end

UITheme.CornerRadius = UDim.new(0, 3)
UITheme.Padding = 10

return UITheme
