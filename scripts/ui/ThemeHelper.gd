class_name ThemeHelper
extends RefCounted
## Custom Brutalist Municipal-Horror UI Theme & Styled Control Factory (Zero Emojis, Zero Generic UI).

const BG_DARK := Color(0.055, 0.060, 0.075, 0.94)
const BG_PANEL := Color(0.085, 0.092, 0.112, 0.93)
const BORDER_AMBER := Color(0.76, 0.56, 0.28, 0.72)
const BORDER_SUBTLE := Color(0.26, 0.28, 0.33, 0.85)
const TEXT_MAIN := Color(0.92, 0.90, 0.86, 1.0)
const TEXT_MUTED := Color(0.64, 0.65, 0.68, 1.0)
const ACCENT_AMBER := Color(0.94, 0.74, 0.36, 1.0)
const ACCENT_CRIMSON := Color(0.84, 0.24, 0.22, 1.0)
const ACCENT_GREEN := Color(0.32, 0.88, 0.52, 1.0)


static func make_panel_style(bg: Color = BG_PANEL, border: Color = BORDER_AMBER, radius: int = 4, border_w: int = 1) -> StyleBoxFlat:
	var sb := StyleBoxFlat.new()
	sb.bg_color = bg
	sb.border_color = border
	sb.set_border_width_all(border_w)
	sb.set_corner_radius_all(radius)
	sb.content_margin_left = 16
	sb.content_margin_right = 16
	sb.content_margin_top = 12
	sb.content_margin_bottom = 12
	return sb


static func style_button(btn: Button, accent: Color = BORDER_AMBER) -> void:
	btn.custom_minimum_size = Vector2(220, 42)
	btn.add_theme_color_override("font_color", TEXT_MAIN)
	btn.add_theme_color_override("font_hover_color", Color(1.0, 0.95, 0.84))
	btn.add_theme_font_size_override("font_size", 15)

	var normal := make_panel_style(Color(0.11, 0.12, 0.15, 0.92), BORDER_SUBTLE, 3, 1)
	var hover := make_panel_style(Color(0.18, 0.16, 0.15, 0.98), accent, 3, 2)
	var pressed := make_panel_style(Color(0.25, 0.19, 0.14, 1.0), ACCENT_AMBER, 3, 2)
	var disabled := make_panel_style(Color(0.07, 0.07, 0.09, 0.65), Color(0.18, 0.18, 0.20, 0.5), 3, 1)

	btn.add_theme_stylebox_override("normal", normal)
	btn.add_theme_stylebox_override("hover", hover)
	btn.add_theme_stylebox_override("pressed", pressed)
	btn.add_theme_stylebox_override("disabled", disabled)


static func make_icon_tex(icon_name: String) -> Texture2D:
	var p := "res://assets/icons/%s.png" % icon_name
	if ResourceLoader.exists(p):
		return load(p)
	return null
