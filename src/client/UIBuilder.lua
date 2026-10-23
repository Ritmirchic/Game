--!strict
--[[
	UIBuilder — declarative helpers for building the interface in code.

	Every element is created through these helpers so padding, borders, fonts and
	animation behave consistently across panels.
]]

local Theme = require(script.Parent.UITheme)

local UI = {}

local function apply(obj, props)
	for key, value in pairs(props) do
		obj[key] = value
	end
	return obj
end

function UI.frame(parent, props)
	local frame = Instance.new("Frame")
	frame.BackgroundColor3 = props.color or Theme.Colors.panel
	frame.BackgroundTransparency = props.transparency or 0
	frame.BorderSizePixel = 0
	frame.Size = props.size or UDim2.fromScale(1, 1)
	frame.Position = props.position or UDim2.fromOffset(0, 0)
	frame.AnchorPoint = props.anchor or Vector2.new(0, 0)
	frame.Visible = props.visible ~= false
	frame.ZIndex = props.zIndex or 1
	frame.ClipsDescendants = props.clip == true
	if props.name then frame.Name = props.name end
	if props.rotation then frame.Rotation = props.rotation end
	frame.Parent = parent
	if props.corner ~= false then
		local corner = Instance.new("UICorner")
		corner.CornerRadius = props.corner or Theme.CornerRadius
		corner.Parent = frame
	end
	if props.stroke ~= false then
		local stroke = Instance.new("UIStroke")
		stroke.Color = props.strokeColor or Theme.Colors.border
		stroke.Thickness = props.strokeThickness or 1
		stroke.Transparency = props.strokeTransparency or 0
		stroke.Parent = frame
	end
	if props.gradient then
		local grad = Instance.new("UIGradient")
		grad.Color = props.gradient.color
		grad.Rotation = props.gradient.rotation or 90
		grad.Transparency = props.gradient.transparency or NumberSequence.new(0)
		grad.Parent = frame
	end
	if props.padding then
		local pad = Instance.new("UIPadding")
		pad.PaddingTop = UDim.new(0, props.padding)
		pad.PaddingBottom = UDim.new(0, props.padding)
		pad.PaddingLeft = UDim.new(0, props.padding)
		pad.PaddingRight = UDim.new(0, props.padding)
		pad.Parent = frame
	end
	return frame
end

function UI.label(parent, props)
	local label = Instance.new("TextLabel")
	label.BackgroundTransparency = props.background and 0 or 1
	label.BackgroundColor3 = props.color or Theme.Colors.panel
	label.BorderSizePixel = 0
	label.Size = props.size or UDim2.fromScale(1, 1)
	label.Position = props.position or UDim2.fromOffset(0, 0)
	label.AnchorPoint = props.anchor or Vector2.new(0, 0)
	label.Text = props.text or ""
	label.TextColor3 = props.textColor or Theme.Colors.text
	label.TextSize = props.textSize or 15
	label.Font = props.font or Theme.Fonts.regular
	label.TextXAlignment = props.alignX or Enum.TextXAlignment.Left
	label.TextYAlignment = props.alignY or Enum.TextYAlignment.Center
	label.TextWrapped = props.wrapped == true
	label.TextTruncate = props.truncate or Enum.TextTruncate.None
	label.RichText = props.rich == true
	label.Visible = props.visible ~= false
	label.ZIndex = props.zIndex or 2
	if props.name then label.Name = props.name end
	label.Parent = parent
	if props.stroke then
		local stroke = Instance.new("UIStroke")
		stroke.Color = props.strokeColor or Theme.Colors.shadow
		stroke.Thickness = props.strokeThickness or 1
		stroke.Transparency = props.strokeTransparency or 0.4
		stroke.Parent = label
	end
	return label
end

function UI.button(parent, props)
	local button = Instance.new("TextButton")
	button.BackgroundColor3 = props.color or Theme.Colors.panelAlt
	button.BackgroundTransparency = props.transparency or 0
	button.BorderSizePixel = 0
	button.AutoButtonColor = false
	button.Size = props.size or UDim2.fromOffset(120, 32)
	button.Position = props.position or UDim2.fromOffset(0, 0)
	button.AnchorPoint = props.anchor or Vector2.new(0, 0)
	button.Text = props.text or ""
	button.TextColor3 = props.textColor or Theme.Colors.text
	button.TextSize = props.textSize or 14
	button.Font = props.font or Theme.Fonts.medium
	button.ZIndex = props.zIndex or 3
	button.Parent = parent
	local corner = Instance.new("UICorner")
	corner.CornerRadius = UDim.new(0, 2)
	corner.Parent = button
	local stroke = Instance.new("UIStroke")
	stroke.Color = props.strokeColor or Theme.Colors.border
	stroke.Thickness = 1
	stroke.Parent = button
	button.MouseEnter:Connect(function()
		stroke.Color = Theme.Colors.borderBright
		button.TextColor3 = Theme.Colors.accent
	end)
	button.MouseLeave:Connect(function()
		stroke.Color = props.strokeColor or Theme.Colors.border
		button.TextColor3 = props.textColor or Theme.Colors.text
	end)
	return button
end

function UI.bar(parent, props)
	local holder = UI.frame(parent, {
		size = props.size or UDim2.fromOffset(220, 12),
		position = props.position,
		color = props.track or Theme.Colors.panelDeep,
		stroke = false,
		corner = UDim.new(0, 2),
		name = props.name,
		zIndex = props.zIndex,
	})
	local fill = UI.frame(holder, {
		size = UDim2.fromScale(props.value or 1, 1),
		color = props.color or Theme.Colors.health,
		stroke = false,
		corner = UDim.new(0, 2),
		zIndex = (props.zIndex or 1) + 1,
	})
	fill.Name = "Fill"
	local segments = props.segments
	if segments then
		for i = 1, segments - 1 do
			UI.frame(holder, {
				size = UDim2.new(0, 1, 1, 0),
				position = UDim2.fromScale(i / segments, 0),
				color = Theme.Colors.bg,
				stroke = false,
				corner = false,
				zIndex = (props.zIndex or 1) + 2,
			})
		end
	end
	return holder, fill
end

function UI.setBar(fill, fraction, tween)
	fraction = math.clamp(fraction or 0, 0, 1)
	if tween then
		local TweenService = game:GetService("TweenService")
		TweenService:Create(fill, TweenInfo.new(0.25, Enum.EasingStyle.Quad), {
			Size = UDim2.fromScale(fraction, 1),
		}):Play()
	else
		fill.Size = UDim2.fromScale(fraction, 1)
	end
end

function UI.list(parent, props)
	local scroll = Instance.new("ScrollingFrame")
	scroll.BackgroundTransparency = 1
	scroll.BorderSizePixel = 0
	scroll.Size = props.size or UDim2.fromScale(1, 1)
	scroll.Position = props.position or UDim2.fromOffset(0, 0)
	scroll.ScrollBarThickness = 4
	scroll.ScrollBarImageColor3 = Theme.Colors.border
	scroll.CanvasSize = UDim2.fromOffset(0, 0)
	scroll.AutomaticCanvasSize = Enum.AutomaticSize.Y
	scroll.ScrollingDirection = Enum.ScrollingDirection.Y
	scroll.Parent = parent
	if props.layout then
		local layout = Instance.new(props.layout)
		layout.Padding = UDim.new(0, props.gap or 6)
		layout.SortOrder = Enum.SortOrder.LayoutOrder
		layout.Parent = scroll
	end
	return scroll
end

function UI.grid(parent, props)
	local scroll = Instance.new("ScrollingFrame")
	scroll.BackgroundTransparency = 1
	scroll.BorderSizePixel = 0
	scroll.Size = props.size or UDim2.fromScale(1, 1)
	scroll.Position = props.position or UDim2.fromOffset(0, 0)
	scroll.ScrollBarThickness = 3
	scroll.ScrollBarImageColor3 = Theme.Colors.border
	scroll.CanvasSize = UDim2.fromOffset(0, 0)
	scroll.AutomaticCanvasSize = Enum.AutomaticSize.Y
	scroll.Parent = parent
	local layout = Instance.new("UIGridLayout")
	layout.CellSize = props.cellSize or UDim2.fromOffset(52, 52)
	layout.CellPadding = props.cellPadding or UDim2.fromOffset(4, 4)
	layout.SortOrder = Enum.SortOrder.LayoutOrder
	layout.Parent = scroll
	return scroll, layout
end

function UI.divider(parent, y)
	return UI.frame(parent, {
		size = UDim2.new(1, 0, 0, 1),
		position = UDim2.fromOffset(0, y),
		color = Theme.Colors.border,
		stroke = false,
		corner = false,
	})
end

function UI.fadeIn(element, duration)
	element.BackgroundTransparency = 1
	local TweenService = game:GetService("TweenService")
	TweenService:Create(element, TweenInfo.new(duration or 0.25), {
		BackgroundTransparency = 0,
	}):Play()
end

function UI.pulse(element, color, duration)
	local TweenService = game:GetService("TweenService")
	local info = TweenInfo.new(duration or 0.4, Enum.EasingStyle.Quad, Enum.EasingDirection.Out)
	TweenService:Create(element, info, { BackgroundColor3 = color }):Play()
end

function UI.clear(parent)
	for _, child in ipairs(parent:GetChildren()) do
		child:Destroy()
	end
end

return UI
