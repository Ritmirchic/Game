class_name GameInterface
extends CanvasLayer
const Data = preload("res://scripts/data.gd")
const State = preload("res://scripts/game_state.gd")
const World = preload("res://scripts/world.gd")
const HUD = preload("res://scripts/hud.gd")

signal launch_requested(fresh_profile: bool)
signal resume_requested
signal menu_requested
signal quit_requested
signal respawn_requested
signal travel_requested(index: int)
signal settings_changed
signal save_requested
signal sfx_requested(name: String)

const TEAL := Color("66ddc9")
const GOLD := Color("e8ba69")
const TEXT := Color("d5e6e7")
const MUTED := Color("78949b")
const RED := Color("e77870")
const BG := Color("071018")

var game_state: State
var world: World
var hud: HUD
var root: Control
var screen: Control
var station_content: Control
var ui_theme: Theme
var has_save := false
var station_tab := "trade"
var upgrade_filter := "ALL"
var return_context := "menu"
var font: Font = preload("res://assets/fonts/Rajdhani-SemiBold.ttf")
var bold: Font = preload("res://assets/fonts/Rajdhani-Bold.ttf")
var mono: Font = preload("res://assets/fonts/SpaceMono-Regular.ttf")

func _ready() -> void:
	layer = 20
	root = Control.new()
	root.name = "Interface"
	root.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	root.mouse_filter = Control.MOUSE_FILTER_IGNORE
	ui_theme = _make_theme()
	root.theme = ui_theme
	add_child(root)
	hud = HUD.new()
	root.add_child(hud)
	hud.visible = false

func configure(state: State, world_ref: World) -> void:
	game_state = state
	world = world_ref
	hud.configure(state, world_ref)

func _make_theme() -> Theme:
	var theme := Theme.new()
	theme.default_font = font
	theme.default_font_size = 18
	theme.set_font("font", "Button", bold)
	theme.set_font_size("font_size", "Button", 17)
	theme.set_color("font_color", "Button", TEXT)
	theme.set_color("font_hover_color", "Button", Color("efffff"))
	theme.set_color("font_pressed_color", "Button", TEAL)
	theme.set_color("font_disabled_color", "Button", Color("4d656a"))
	theme.set_stylebox("normal", "Button", _style(Color("0c2029"), Color("28515a"), 10))
	theme.set_stylebox("hover", "Button", _style(Color("14343d"), TEAL.darkened(0.25), 10))
	theme.set_stylebox("pressed", "Button", _style(Color("1b4548"), TEAL, 10))
	theme.set_stylebox("disabled", "Button", _style(Color("0a161d"), Color("1a3037"), 10))
	theme.set_stylebox("focus", "Button", _style(Color(0, 0, 0, 0), Color(TEAL, 0.65), 10))
	theme.set_stylebox("panel", "Panel", _style(Color(0.025, 0.06, 0.08, 0.97), Color("274b53"), 3))
	theme.set_stylebox("panel", "PanelContainer", _style(Color(0.025, 0.06, 0.08, 0.97), Color("274b53"), 3))
	theme.set_color("font_color", "Label", TEXT)
	theme.set_stylebox("slider", "HSlider", _style(Color("152c34"), Color("25434b"), 2))
	theme.set_stylebox("grabber_area", "HSlider", _style(TEAL.darkened(0.35), TEAL.darkened(0.2), 2))
	theme.set_stylebox("grabber_area_highlight", "HSlider", _style(TEAL.darkened(0.2), TEAL, 2))
	theme.set_icon("grabber", "HSlider", load("res://assets/art/particles/glow.png"))
	theme.set_icon("grabber_highlight", "HSlider", load("res://assets/art/particles/glow.png"))
	theme.set_stylebox("scroll", "VScrollBar", _style(Color("0b1b23"), Color.TRANSPARENT, 2))
	theme.set_stylebox("grabber", "VScrollBar", _style(Color("28525a"), Color.TRANSPARENT, 2))
	theme.set_stylebox("grabber_highlight", "VScrollBar", _style(TEAL.darkened(0.3), Color.TRANSPARENT, 2))
	return theme

func _style(background: Color, border: Color, margin: int = 0) -> StyleBoxFlat:
	var style := StyleBoxFlat.new()
	style.bg_color = background
	style.border_color = border
	style.set_border_width_all(1)
	style.set_corner_radius_all(3)
	style.content_margin_left = margin
	style.content_margin_right = margin
	style.content_margin_top = margin / 2
	style.content_margin_bottom = margin / 2
	return style

func _new_screen(dim: float = 0.0) -> Control:
	if is_instance_valid(screen):
		root.remove_child(screen)
		screen.queue_free()
	screen = Control.new()
	screen.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	screen.mouse_filter = Control.MOUSE_FILTER_IGNORE
	root.add_child(screen)
	if dim > 0.0:
		_color_rect(screen, Rect2(0, 0, 1600, 900), Color(0.005, 0.015, 0.025, dim))
	screen.modulate.a = 0.0
	screen.create_tween().tween_property(screen, "modulate:a", 1.0, 0.20)
	return screen

func _add_background(parent: Control, sector: int = 0) -> void:
	var texture := TextureRect.new()
	texture.texture = load(Data.SECTORS[sector].background)
	texture.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	texture.stretch_mode = TextureRect.STRETCH_SCALE
	texture.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	texture.mouse_filter = Control.MOUSE_FILTER_IGNORE
	parent.add_child(texture)
	parent.move_child(texture, 0)
	var material := ShaderMaterial.new()
	material.shader = load("res://assets/shaders/nebula.gdshader")
	material.set_shader_parameter("sector_tint", Color(Data.SECTORS[sector].color))
	texture.material = material

func _label(parent: Node, text: String, rect: Rect2, font_size: int = 20, color: Color = TEXT, font_kind: int = 0) -> Label:
	var label := Label.new()
	label.text = text
	label.position = rect.position
	label.size = rect.size
	label.add_theme_font_override("font", mono if font_kind == 2 else (bold if font_kind == 1 else font))
	label.add_theme_font_size_override("font_size", font_size)
	label.add_theme_color_override("font_color", color)
	label.mouse_filter = Control.MOUSE_FILTER_IGNORE
	parent.add_child(label)
	return label

func _wrapped_label(parent: Node, text: String, rect: Rect2, font_size: int = 16, color: Color = MUTED) -> Label:
	var label := _label(parent, text, rect, font_size, color)
	label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	label.clip_text = true
	label.max_lines_visible = maxi(1, int(rect.size.y / maxf(1.0, font_size * 1.15)))
	label.size = rect.size
	return label

func _button(parent: Node, text: String, rect: Rect2, callback: Callable, primary: bool = false, disabled: bool = false) -> Button:
	var button := Button.new()
	button.text = text
	button.position = rect.position
	button.size = rect.size
	button.disabled = disabled
	button.mouse_default_cursor_shape = Control.CURSOR_POINTING_HAND
	if primary:
		button.add_theme_stylebox_override("normal", _style(Color("17413f"), TEAL.darkened(0.15), 10))
		button.add_theme_stylebox_override("hover", _style(Color("235b53"), TEAL, 10))
		button.add_theme_color_override("font_color", Color("e4fff7"))
	parent.add_child(button)
	button.pressed.connect(func():
		sfx_requested.emit("ui")
		callback.call()
	)
	return button

func _panel(parent: Node, rect: Rect2, accent: Color = Color("2a5058")) -> Panel:
	var panel := Panel.new()
	panel.position = rect.position
	panel.size = rect.size
	panel.add_theme_stylebox_override("panel", _style(Color(0.025, 0.06, 0.08, 0.97), accent, 3))
	parent.add_child(panel)
	return panel

func _color_rect(parent: Node, rect: Rect2, color: Color) -> ColorRect:
	var control := ColorRect.new()
	control.position = rect.position
	control.size = rect.size
	control.color = color
	control.mouse_filter = Control.MOUSE_FILTER_IGNORE
	parent.add_child(control)
	return control

func _texture(parent: Node, path: String, rect: Rect2, modulate_color: Color = Color.WHITE) -> TextureRect:
	var texture := TextureRect.new()
	texture.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	texture.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	texture.texture = load(path)
	texture.position = rect.position
	texture.size = rect.size
	texture.modulate = modulate_color
	texture.mouse_filter = Control.MOUSE_FILTER_IGNORE
	parent.add_child(texture)
	return texture

func _line(parent: Node, position: Vector2, width: float, color: Color = Color("284c54")) -> void:
	_color_rect(parent, Rect2(position, Vector2(width, 1)), color)

func show_main_menu() -> void:
	hud.visible = false
	Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
	_new_screen()
	_add_background(screen, 0)
	_color_rect(screen, Rect2(0, 0, 1600, 900), Color(0.005, 0.015, 0.025, 0.32))
	_color_rect(screen, Rect2(0, 0, 790, 900), Color(0.01, 0.025, 0.04, 0.48))
	var w := 1600.0
	var h := 900.0
	_label(screen, "D E E P   B E L T   O P E R A T I O N S", Rect2(115, 158, 600, 30), 14, TEAL, 2)
	_label(screen, "SPACE MINER", Rect2(108, 200, 700, 100), 76, Color("efffff"), 1)
	_label(screen, "MINE  /  UPGRADE  /  AUTOMATE  /  SURVIVE", Rect2(115, 300, 620, 30), 15, MUTED, 2)
	_line(screen, Vector2(115, 346), 510, Color(TEAL, 0.5))
	var ship := _texture(screen, "res://assets/art/ships/ship_1.png", Rect2(970, 305, 390, 300))
	var glow := _texture(screen, "res://assets/art/particles/glow.png", Rect2(930, 360, 180, 180), Color(TEAL, 0.25))
	screen.move_child(glow, ship.get_index())
	var tween := ship.create_tween().set_loops()
	tween.tween_property(ship, "position:y", 326.0, 2.8).set_trans(Tween.TRANS_SINE)
	tween.tween_property(ship, "position:y", 305.0, 2.8).set_trans(Tween.TRANS_SINE)
	var launch_text := "CONTINUE FLIGHT" if has_save else "LAUNCH FLIGHT"
	_button(screen, launch_text, Rect2(115, 390, 330, 58), func(): launch_requested.emit(false), true)
	_button(screen, "NEW PROFILE", Rect2(460, 390, 220, 58), show_new_profile_confirm)
	_button(screen, "SETTINGS", Rect2(115, 464, 270, 50), func(): show_settings("menu"))
	_button(screen, "ACHIEVEMENTS", Rect2(400, 464, 280, 50), func(): show_achievements("menu"))
	_button(screen, "CONTROLS & FLIGHT MANUAL", Rect2(115, 530, 565, 50), func(): show_manual("menu"))
	_button(screen, "QUIT", Rect2(115, 604, 160, 42), func(): quit_requested.emit())
	if has_save and game_state != null:
		_label(screen, "PROFILE  •  LVL %d  •  %s CR  •  %s" % [int(game_state.profile.level), State.format_number(int(game_state.profile.credits)), Data.SECTORS[int(game_state.profile.current_sector)].name], Rect2(115, 682, 650, 28), 12, TEAL, 2)
	else:
		_label(screen, "YOUR FIRST CLAIM AWAITS IN THE STARTER BELT", Rect2(115, 682, 600, 28), 12, MUTED, 2)
	_label(screen, "SPACE MINER  v%s   •   GODOT 4   •   WINDOWS x64" % Data.VERSION, Rect2(50, 854, 650, 24), 11, MUTED, 2)
	_label(screen, "ORIGINAL PROCEDURAL ART & AUDIO", Rect2(1190, 854, 360, 24), 10, MUTED, 2)

func show_new_profile_confirm() -> void:
	_new_screen(0.0)
	_add_background(screen, 0)
	_color_rect(screen, Rect2(0, 0, 1600, 900), Color(0.005, 0.015, 0.025, 0.78))
	var panel := _panel(screen, Rect2(490, 280, 620, 330), RED)
	_label(panel, "START A NEW PROFILE?", Rect2(34, 28, 550, 45), 30, TEXT, 1)
	_wrapped_label(panel, "This replaces the current flight profile. Your existing save will remain as a backup until the new profile is first saved.", Rect2(34, 92, 550, 80), 18, MUTED)
	_button(panel, "CANCEL", Rect2(34, 242, 240, 50), show_main_menu)
	_button(panel, "ERASE & LAUNCH", Rect2(316, 242, 270, 50), func(): launch_requested.emit(true), true)

func show_game() -> void:
	if is_instance_valid(screen):
		root.remove_child(screen)
		screen.queue_free()
		screen = null
	hud.visible = true
	Input.mouse_mode = Input.MOUSE_MODE_HIDDEN

func show_pause() -> void:
	Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
	_new_screen(0.74)
	var panel := _panel(screen, Rect2(540, 190, 520, 530), TEAL)
	_label(panel, "FLIGHT PAUSED", Rect2(38, 30, 440, 50), 36, TEXT, 1)
	_label(panel, "SIMULATION HOLD  •  PROFILE READY", Rect2(40, 88, 440, 26), 11, MUTED, 2)
	_button(panel, "RESUME FLIGHT", Rect2(40, 140, 440, 54), func(): resume_requested.emit(), true)
	_button(panel, "SAVE FLIGHT", Rect2(40, 210, 440, 48), func(): save_requested.emit())
	_button(panel, "SETTINGS", Rect2(40, 274, 440, 48), func(): show_settings("pause"))
	_button(panel, "FLIGHT MANUAL", Rect2(40, 338, 440, 48), func(): show_manual("pause"))
	_button(panel, "RETURN TO MAIN MENU", Rect2(40, 420, 440, 48), func(): menu_requested.emit())

func show_death() -> void:
	Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
	_new_screen(0.80)
	var panel := _panel(screen, Rect2(455, 210, 690, 470), RED)
	_label(panel, "SIGNAL LOST", Rect2(42, 32, 600, 62), 46, RED, 1)
	_label(panel, "CERES INSURANCE RECOVERY COMPLETE", Rect2(44, 105, 600, 30), 13, GOLD, 2)
	_wrapped_label(panel, "Your ship was recovered to the relay. Insurance retained 75% of cargo; hull, shield and reactor systems are ready for redeployment.", Rect2(44, 160, 600, 80), 19, MUTED)
	_label(panel, "FLIGHT STATISTICS", Rect2(44, 257, 300, 28), 14, TEAL, 2)
	_label(panel, "%d ASTEROIDS   •   %d HOSTILES   •   %d CREDITS EARNED" % [int(game_state.profile.stats.asteroids), int(game_state.profile.stats.kills), int(game_state.profile.stats.earned)], Rect2(44, 292, 610, 28), 12, TEXT, 2)
	_button(panel, "REDEPLOY AT CERES RELAY", Rect2(44, 367, 390, 54), func(): respawn_requested.emit(), true)
	_button(panel, "MAIN MENU", Rect2(452, 367, 194, 54), func(): menu_requested.emit())

func show_settings(return_to: String = "menu") -> void:
	return_context = return_to
	Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
	_new_screen()
	_add_background(screen, int(game_state.profile.current_sector) if game_state != null else 0)
	_color_rect(screen, Rect2(0, 0, 1600, 900), Color(0.005, 0.015, 0.025, 0.84))
	_label(screen, "SETTINGS", Rect2(130, 80, 600, 60), 44, TEXT, 1)
	_label(screen, "AUDIO  /  FLIGHT  /  ACCESSIBILITY", Rect2(133, 143, 700, 28), 12, TEAL, 2)
	var panel := _panel(screen, Rect2(130, 200, 1340, 560))
	var row := 0
	for setting in [{"key":"master","name":"MASTER OUTPUT"}, {"key":"music","name":"AMBIENT MUSIC"}, {"key":"sfx","name":"EFFECTS & WEAPONS"}]:
		var y := 45.0 + row * 82.0
		_label(panel, setting.name, Rect2(40, y, 300, 30), 18, TEXT)
		var slider := HSlider.new()
		slider.position = Vector2(380, y + 5)
		slider.size = Vector2(650, 26)
		slider.min_value = 0.0
		slider.max_value = 1.0
		slider.step = 0.01
		slider.value = float(game_state.profile.settings[setting.key])
		panel.add_child(slider)
		var value_label := _label(panel, "%d%%" % int(slider.value * 100), Rect2(1070, y, 130, 30), 16, TEAL, 2)
		slider.value_changed.connect(func(value: float):
			game_state.profile.settings[setting.key] = value
			value_label.text = "%d%%" % int(value * 100)
			settings_changed.emit()
		)
		row += 1
	var toggles := [{"key":"screen_shake","name":"CAMERA IMPACT"}, {"key":"particles","name":"PARTICLE EFFECTS"}, {"key":"show_hints","name":"FLIGHT HINTS"}, {"key":"autosave","name":"AUTOSAVE"}]
	for i in range(toggles.size()):
		var setting: Dictionary = toggles[i]
		var x := 40.0 + (i % 2) * 640.0
		var y := 310.0 + (i / 2) * 82.0
		_label(panel, setting.name, Rect2(x, y, 330, 32), 18, TEXT)
		var key: String = setting.key
		_button(panel, "ENABLED" if bool(game_state.profile.settings[key]) else "DISABLED", Rect2(x + 365, y - 5, 200, 44), func():
			game_state.profile.settings[key] = not bool(game_state.profile.settings[key])
			settings_changed.emit()
			show_settings(return_context)
		, bool(game_state.profile.settings[key]))
	_label(panel, "THREAT LEVEL", Rect2(40, 475, 280, 30), 18, TEXT)
	var names := ["RELAXED", "STANDARD", "HARD"]
	var values := [0.7, 1.0, 1.35]
	for i in range(3):
		_button(panel, names[i], Rect2(345 + i * 190, 468, 170, 44), func():
			game_state.profile.settings.difficulty = values[i]
			settings_changed.emit()
			show_settings(return_context)
		, is_equal_approx(float(game_state.profile.settings.difficulty), float(values[i])))
	_button(panel, "TOGGLE FULLSCREEN", Rect2(940, 468, 300, 44), func():
		var full := DisplayServer.window_get_mode() == DisplayServer.WINDOW_MODE_FULLSCREEN
		DisplayServer.window_set_mode(DisplayServer.WINDOW_MODE_WINDOWED if full else DisplayServer.WINDOW_MODE_FULLSCREEN)
	)
	_button(screen, "BACK", Rect2(130, 790, 240, 48), _return_from_subscreen, true)
	_label(screen, "SETTINGS ARE STORED IN THE VERSIONED FLIGHT PROFILE", Rect2(410, 802, 850, 28), 11, MUTED, 2)

func show_achievements(return_to: String = "menu") -> void:
	return_context = return_to
	Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
	_new_screen()
	_add_background(screen, 4)
	_color_rect(screen, Rect2(0, 0, 1600, 900), Color(0.005, 0.015, 0.025, 0.86))
	_label(screen, "FLIGHT RECORDS", Rect2(100, 65, 700, 60), 44, TEXT, 1)
	_label(screen, "%d / %d ACHIEVEMENTS LOGGED" % [game_state.profile.achievements.size(), Data.ACHIEVEMENTS.size()], Rect2(103, 130, 700, 28), 13, GOLD, 2)
	for i in range(Data.ACHIEVEMENTS.size()):
		var achievement: Dictionary = Data.ACHIEVEMENTS[i]
		var unlocked: bool = achievement.id in game_state.profile.achievements
		var x := 100.0 + (i % 3) * 470.0
		var y := 190.0 + (i / 3) * 140.0
		var panel := _panel(screen, Rect2(x, y, 440, 118), GOLD if unlocked else Color("263a43"))
		_texture(panel, "res://assets/art/icons/upgrade_research.png" if unlocked else "res://assets/art/icons/upgrade_hull.png", Rect2(18, 22, 68, 68), Color.WHITE if unlocked else Color(0.3, 0.38, 0.4, 0.5))
		_label(panel, achievement.name, Rect2(102, 19, 320, 30), 19, GOLD if unlocked else MUTED, 1)
		_wrapped_label(panel, achievement.description, Rect2(102, 54, 315, 50), 15, TEXT if unlocked else MUTED)
	_button(screen, "BACK", Rect2(100, 790, 240, 48), _return_from_subscreen, true)

func show_manual(return_to: String = "menu") -> void:
	return_context = return_to
	Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
	_new_screen()
	_add_background(screen, 0)
	_color_rect(screen, Rect2(0, 0, 1600, 900), Color(0.005, 0.015, 0.025, 0.88))
	_label(screen, "FLIGHT MANUAL", Rect2(110, 65, 800, 60), 44, TEXT, 1)
	_label(screen, "CERES RELAY  •  INDEPENDENT PROSPECTOR HANDBOOK", Rect2(113, 130, 900, 28), 12, TEAL, 2)
	var left := _panel(screen, Rect2(110, 190, 655, 570))
	var right := _panel(screen, Rect2(795, 190, 695, 570))
	_label(left, "FLIGHT CONTROLS", Rect2(30, 24, 570, 36), 25, GOLD, 1)
	var controls := [["W A S D / ARROWS", "Thrust and strafe"], ["MOUSE", "Aim ship and mining emitter"], ["LEFT MOUSE (HOLD)", "Mine targeted asteroid"], ["SPACE / RIGHT MOUSE", "Fire pulse cannons"], ["SHIFT", "Afterburner; drains reactor"], ["E", "Dock while inside relay approach"], ["TAB", "Expand cargo manifest"], ["M", "Open sector chart"], ["ESC", "Pause / close current panel"]]
	for i in range(controls.size()):
		var y := 86.0 + i * 48.0
		_label(left, controls[i][0], Rect2(30, y, 285, 28), 12, TEAL, 2)
		_label(left, controls[i][1], Rect2(318, y - 2, 305, 32), 17, TEXT)
	_label(right, "OPERATIONS", Rect2(30, 24, 610, 36), 25, GOLD, 1)
	var sections := [
		["MINING", "Hold the beam on a scanned rock. Harder minerals take longer. Fly close to fragments for automatic tractor collection."],
		["ECONOMY", "Return to Ceres Relay, sell cargo, repair and invest in the upgrade grid. Every level changes a live ship statistic."],
		["AUTOMATION", "Buy a Drone Bay level, then purchase visible drones. They seek rock, mine, collect fragments and return cargo to your hold."],
		["COMBAT", "Shields recover after four seconds without damage. Pulse cannons work while moving. Hostile kills pay immediate bounties."],
		["DEEP SPACE", "Meet pilot-level and credit requirements at the relay to chart five sectors. Storm telegraphs and void wells are dangerous."]
	]
	for i in range(sections.size()):
		var y := 82.0 + i * 94.0
		_label(right, sections[i][0], Rect2(30, y, 610, 26), 14, TEAL, 2)
		_wrapped_label(right, sections[i][1], Rect2(30, y + 30, 610, 58), 16, MUTED)
	_button(screen, "BACK", Rect2(110, 790, 240, 48), _return_from_subscreen, true)

func _return_from_subscreen() -> void:
	if return_context == "pause":
		show_pause()
	elif return_context == "station":
		show_station(station_tab)
	else:
		show_main_menu()

func show_sector_map() -> void:
	Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
	_new_screen(0.78)
	var panel := _panel(screen, Rect2(75, 65, 1450, 760), TEAL)
	_label(panel, "SECTOR CHART", Rect2(35, 24, 650, 50), 36, TEXT, 1)
	_label(panel, "CERES NAVIGATION ARRAY  •  READ-ONLY FLIGHT PLOT", Rect2(38, 80, 800, 26), 12, MUTED, 2)
	var points := [Vector2(255, 275), Vector2(560, 205), Vector2(830, 360), Vector2(1110, 220), Vector2(1190, 540)]
	for i in range(points.size() - 1):
		var line := Line2D.new()
		line.points = PackedVector2Array([points[i], points[i + 1]])
		line.width = 2.0
		line.default_color = Color(0.30, 0.62, 0.62, 0.30)
		panel.add_child(line)
	for i in range(Data.SECTORS.size()):
		var sector: Dictionary = Data.SECTORS[i]
		var unlocked: bool = i in game_state.profile.unlocked_sectors
		var color := Color(sector.color) if unlocked else Color("40535c")
		var node := _panel(panel, Rect2(points[i] - Vector2(110, 65), Vector2(230, 160)), color)
		_texture(node, sector.background, Rect2(5, 5, 220, 70), Color(1, 1, 1, 0.75 if unlocked else 0.20))
		_label(node, "%02d" % (i + 1), Rect2(12, 9, 50, 25), 14, color, 2)
		_label(node, sector.name, Rect2(12, 81, 210, 26), 18, TEXT if unlocked else MUTED, 1)
		var status := "CURRENT POSITION" if i == int(game_state.profile.current_sector) else ("CHARTED" if unlocked else "LVL %d  •  %s CR" % [int(sector.level), State.format_number(int(sector.cost))])
		_label(node, status, Rect2(12, 114, 210, 22), 10, color, 2)
	_label(panel, "TRAVEL AND SECTOR UNLOCKING ARE AVAILABLE WHILE DOCKED AT CERES RELAY", Rect2(38, 690, 1050, 28), 11, GOLD, 2)
	_button(panel, "CLOSE CHART  [M]", Rect2(1160, 675, 250, 48), func(): resume_requested.emit(), true)

func show_station(tab: String = "trade") -> void:
	station_tab = tab
	Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
	_new_screen(0.82)
	var panel := _panel(screen, Rect2(50, 42, 1500, 816), GOLD)
	_label(panel, "CERES RELAY", Rect2(28, 18, 600, 46), 36, TEXT, 1)
	_label(panel, "DOCKED  •  %s  •  %s" % [Data.SECTORS[world.sector_index].name, Data.SECTORS[world.sector_index].code], Rect2(31, 69, 750, 24), 11, MUTED, 2)
	_label(panel, "%s CR" % State.format_number(int(game_state.profile.credits)), Rect2(1050, 25, 260, 42), 28, GOLD, 1)
	_button(panel, "UNDOCK  [E]", Rect2(1270, 20, 195, 46), func(): resume_requested.emit(), true)
	_line(panel, Vector2(25, 108), 1445)
	var tabs := [["trade", "TRADE & REPAIR"], ["upgrades", "UPGRADE GRID"], ["drones", "DRONE BAY"], ["ships", "SHIPYARD"], ["sectors", "SECTOR CHART"], ["research", "RESEARCH ARCHIVE"]]
	for i in range(tabs.size()):
		var id: String = tabs[i][0]
		_button(panel, tabs[i][1], Rect2(25, 135 + i * 69, 235, 53), func(): show_station(id), id == station_tab)
	_label(panel, "VESSEL", Rect2(30, 585, 200, 24), 11, MUTED, 2)
	_label(panel, Data.get_ship(game_state.profile.current_ship).name, Rect2(30, 615, 220, 52), 18, TEAL, 1)
	_label(panel, "HULL  %d / %d" % [world.player.hull, int(game_state.get_stats().max_hull)], Rect2(30, 684, 220, 24), 11, MUTED, 2)
	_label(panel, "CARGO  %d / %d" % [game_state.cargo_total(), int(game_state.get_stats().cargo)], Rect2(30, 715, 220, 24), 11, MUTED, 2)
	_label(panel, "DRONES  %d / %d" % [int(game_state.profile.drones), int(game_state.get_stats().drone_slots)], Rect2(30, 746, 220, 24), 11, MUTED, 2)
	station_content = Control.new()
	station_content.position = Vector2(285, 132)
	station_content.size = Vector2(1185, 650)
	panel.add_child(station_content)
	_render_station_content()

func _render_station_content() -> void:
	for child in station_content.get_children():
		station_content.remove_child(child)
		child.queue_free()
	match station_tab:
		"trade": _station_trade()
		"upgrades": _station_upgrades()
		"drones": _station_drones()
		"ships": _station_ships()
		"sectors": _station_sectors()
		"research": _station_research()

func _station_title(title: String, subtitle: String) -> void:
	_label(station_content, title, Rect2(0, 0, 760, 40), 28, TEXT, 1)
	_label(station_content, subtitle, Rect2(0, 42, 1050, 24), 11, MUTED, 2)

func _station_trade() -> void:
	_station_title("TRADE & REPAIR", "CERES COMMODITY EXCHANGE  •  FIXED FAIR PRICES  •  NO DOCKING FEES")
	var cargo_panel := _panel(station_content, Rect2(0, 85, 740, 505))
	_label(cargo_panel, "CARGO MANIFEST", Rect2(20, 14, 420, 30), 19, TEAL, 1)
	_label(cargo_panel, "%d / %d UNITS" % [game_state.cargo_total(), int(game_state.get_stats().cargo)], Rect2(510, 18, 200, 24), 11, GOLD, 2)
	var scroll := ScrollContainer.new()
	scroll.position = Vector2(15, 55)
	scroll.size = Vector2(710, 385)
	cargo_panel.add_child(scroll)
	var list := VBoxContainer.new()
	list.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	list.add_theme_constant_override("separation", 3)
	scroll.add_child(list)
	var total := 0
	for resource in Data.RESOURCES:
		var amount := int(game_state.profile.resources[resource.id])
		var value := amount * int(resource.value)
		total += value
		var row := HBoxContainer.new()
		row.custom_minimum_size = Vector2(680, 30)
		row.add_theme_constant_override("separation", 12)
		list.add_child(row)
		var icon := TextureRect.new()
		icon.texture = load("res://assets/art/icons/resource_%s.png" % resource.id)
		icon.custom_minimum_size = Vector2(30, 30)
		icon.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
		icon.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
		icon.modulate = Color.WHITE if amount > 0 else Color(0.4, 0.5, 0.55, 0.35)
		row.add_child(icon)
		var name_label := Label.new()
		name_label.text = resource.name.to_upper()
		name_label.custom_minimum_size = Vector2(280, 30)
		name_label.add_theme_font_size_override("font_size", 16)
		name_label.add_theme_color_override("font_color", TEXT if amount > 0 else MUTED)
		row.add_child(name_label)
		var amount_label := Label.new()
		amount_label.text = "%d u" % amount
		amount_label.custom_minimum_size = Vector2(110, 30)
		amount_label.add_theme_font_override("font", mono)
		amount_label.add_theme_font_size_override("font_size", 12)
		row.add_child(amount_label)
		var value_label := Label.new()
		value_label.text = "%s CR" % State.format_number(value)
		value_label.custom_minimum_size = Vector2(130, 30)
		value_label.add_theme_font_override("font", mono)
		value_label.add_theme_font_size_override("font_size", 12)
		value_label.add_theme_color_override("font_color", GOLD)
		row.add_child(value_label)
	_button(cargo_panel, "SELL ALL CARGO  •  %s CR" % State.format_number(total), Rect2(20, 451, 700, 40), func():
		var value := game_state.sell_cargo()
		if value > 0:
			hud.notify("CARGO SOLD  •  +%s CR" % State.format_number(value), GOLD)
			sfx_requested.emit("pickup")
			game_state.save()
			show_station("trade")
	, true, total <= 0)
	var service := _panel(station_content, Rect2(765, 85, 390, 505))
	_label(service, "SHIP SERVICES", Rect2(24, 18, 330, 32), 20, TEAL, 1)
	_texture(service, Data.get_ship(game_state.profile.current_ship).texture, Rect2(75, 62, 240, 160))
	_label(service, Data.get_ship(game_state.profile.current_ship).name, Rect2(24, 231, 340, 28), 19, TEXT, 1)
	_label(service, "HULL  %d / %d" % [world.player.hull, int(game_state.get_stats().max_hull)], Rect2(24, 274, 340, 25), 12, MUTED, 2)
	_label(service, "SHIELD  %d / %d" % [world.player.shield, int(game_state.get_stats().max_shield)], Rect2(24, 305, 340, 25), 12, MUTED, 2)
	_label(service, "REACTOR  %d / %d" % [world.player.energy, int(game_state.get_stats().max_energy)], Rect2(24, 336, 340, 25), 12, MUTED, 2)
	var cost := repair_cost()
	_button(service, "FULL SERVICE  •  %s CR" % State.format_number(cost), Rect2(24, 390, 342, 45), func():
		if game_state.profile.credits >= cost:
			game_state.profile.credits = int(game_state.profile.credits) - cost
			world.player.full_repair()
			hud.notify("SHIP SERVICED  •  ALL SYSTEMS NOMINAL", TEAL)
			sfx_requested.emit("dock")
			game_state.save()
			show_station("trade")
	, true, cost <= 0 or int(game_state.profile.credits) < cost)
	_label(service, "INCLUDES HULL, SHIELD & REACTOR", Rect2(24, 455, 342, 22), 10, MUTED, 2)

func repair_cost() -> int:
	var stats := game_state.get_stats()
	return int(ceil(maxf(0.0, float(stats.max_hull) - world.player.hull) * 1.3 + maxf(0.0, float(stats.max_shield) - world.player.shield) * 0.35))

func _station_upgrades() -> void:
	_station_title("UPGRADE GRID", "REAL SYSTEM MODIFICATIONS  •  INSTALLATION INCLUDED  •  NO RESEARCH FEES")
	var categories := ["ALL", "MINING", "SHIP", "ENGINE", "DEFENSE", "COMBAT", "UTILITY", "DRONES", "RESEARCH"]
	var filter_bar := HBoxContainer.new()
	filter_bar.position = Vector2(0, 70)
	filter_bar.size = Vector2(1160, 34)
	filter_bar.add_theme_constant_override("separation", 5)
	station_content.add_child(filter_bar)
	for category in categories:
		var button := Button.new()
		button.text = category
		button.custom_minimum_size = Vector2(118 if category != "RESEARCH" else 140, 32)
		button.add_theme_font_size_override("font_size", 13)
		if category == upgrade_filter:
			button.add_theme_stylebox_override("normal", _style(Color("19443f"), TEAL, 5))
		filter_bar.add_child(button)
		button.pressed.connect(func():
			upgrade_filter = category
			sfx_requested.emit("ui")
			_render_station_content()
		)
	var scroll := ScrollContainer.new()
	scroll.position = Vector2(0, 116)
	scroll.size = Vector2(1170, 505)
	station_content.add_child(scroll)
	var grid := GridContainer.new()
	grid.columns = 3
	grid.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	grid.add_theme_constant_override("h_separation", 12)
	grid.add_theme_constant_override("v_separation", 12)
	scroll.add_child(grid)
	for upgrade in Data.UPGRADES:
		if upgrade_filter != "ALL" and upgrade.category != upgrade_filter:
			continue
		_upgrade_card(grid, upgrade)

func _upgrade_card(parent: Node, upgrade: Dictionary) -> void:
	var level := int(game_state.profile.upgrades[upgrade.id])
	var card := Panel.new()
	card.custom_minimum_size = Vector2(370, 218)
	card.add_theme_stylebox_override("panel", _style(Color("0a1b23"), Color("284b53") if level < int(upgrade.max) else GOLD.darkened(0.3), 3))
	parent.add_child(card)
	_texture(card, "res://assets/art/icons/upgrade_%s.png" % upgrade.icon, Rect2(14, 12, 46, 46), Color.WHITE if level > 0 else Color(0.65, 0.8, 0.82, 0.8))
	_label(card, upgrade.name, Rect2(70, 12, 285, 28), 20, TEXT, 1)
	_label(card, upgrade.category, Rect2(70, 42, 200, 20), 10, TEAL, 2)
	for i in range(int(upgrade.max)):
		_color_rect(card, Rect2(15 + i * 27, 68, 20, 4), TEAL if i < level else Color("1d343c"))
	_wrapped_label(card, upgrade.description, Rect2(15, 82, 340, 48), 14, MUTED)
	_label(card, _upgrade_effect(upgrade.id), Rect2(15, 133, 340, 24), 11, TEAL if level > 0 else MUTED, 2)
	_label(card, "LVL %d / %d" % [level, int(upgrade.max)], Rect2(15, 177, 150, 24), 12, MUTED, 2)
	var cost := Data.upgrade_cost(upgrade.id, level)
	var maxed := level >= int(upgrade.max)
	_button(card, "MAXIMUM" if maxed else "%s CR" % State.format_number(cost), Rect2(210, 169, 145, 36), func():
		if game_state.buy_upgrade(upgrade.id):
			world.player.configure(game_state)
			world.sync_drones()
			hud.notify("INSTALLED  •  %s LVL %d" % [upgrade.name, level + 1], TEAL)
			sfx_requested.emit("achievement")
			game_state.save()
			show_station("upgrades")
	, level > 0 and not maxed, maxed or int(game_state.profile.credits) < cost)

func _upgrade_effect(id: String) -> String:
	var current := game_state.get_stats()
	var level := int(game_state.profile.upgrades[id])
	var maximum := int(Data.get_upgrade(id).max)
	if level >= maximum:
		return "SYSTEM FULLY OPTIMIZED"
	game_state.profile.upgrades[id] = level + 1
	var next := game_state.get_stats()
	game_state.profile.upgrades[id] = level
	var keys := {"laser_power":"laser", "mining_speed":"laser", "cargo_capacity":"cargo", "engine_speed":"speed", "acceleration":"acceleration", "hull":"max_hull", "shield":"max_shield", "energy_regen":"energy_regen", "scanner":"scanner", "drone_capacity":"drone_slots", "weapon_damage":"weapon", "auto_collection":"collection", "armor":"armor", "reactor":"max_energy", "drone_mining":"drone_power", "research":"xp_multiplier"}
	var key: String = keys.get(id, "laser")
	if id == "armor":
		return "DAMAGE RESIST  %.1f%%  >  %.1f%%" % [float(current[key]) * 100, float(next[key]) * 100]
	if id == "research":
		return "XP GAIN  %.0f%%  >  %.0f%%" % [float(current[key]) * 100, float(next[key]) * 100]
	return "%s  %.1f  >  %.1f" % [key.to_upper().replace("MAX_", "").replace("_", " "), float(current[key]), float(next[key])]

func _station_drones() -> void:
	_station_title("DRONE BAY", "VISIBLE AUTONOMOUS UNITS  •  SEARCH / MINE / COLLECT / RETURN")
	var summary := _panel(station_content, Rect2(0, 85, 1160, 130))
	_label(summary, "ACTIVE UNITS  %d / %d" % [int(game_state.profile.drones), int(game_state.get_stats().drone_slots)], Rect2(24, 20, 360, 28), 18, TEAL, 1)
	_label(summary, "POWER %.1f  •  SPEED %.0f  •  CAPACITY %d" % [float(game_state.get_stats().drone_power), float(game_state.get_stats().drone_speed), int(game_state.get_stats().drone_cargo)], Rect2(24, 65, 540, 26), 11, MUTED, 2)
	var can_buy := int(game_state.profile.drones) < int(game_state.get_stats().drone_slots) and int(game_state.profile.credits) >= game_state.drone_cost()
	_button(summary, "PURCHASE DRONE  •  %s CR" % State.format_number(game_state.drone_cost()), Rect2(610, 20, 515, 40), func():
		if game_state.buy_drone():
			world.sync_drones()
			hud.show_achievement("automation")
			sfx_requested.emit("dock")
			game_state.save()
			show_station("drones")
	, true, not can_buy)
	_button(summary, "SUSPEND AUTONOMY" if bool(game_state.profile.drones_active) else "RESUME AUTONOMY", Rect2(610, 72, 245, 38), func():
		world.set_drones_active(not bool(game_state.profile.drones_active))
		show_station("drones")
	, bool(game_state.profile.drones_active), int(game_state.profile.drones) == 0)
	_button(summary, "RECALL ALL", Rect2(875, 72, 250, 38), func():
		for drone in world.drones:
			if is_instance_valid(drone): drone.recall()
		hud.notify("ALL DRONES RETURNING TO SHIP", TEAL)
		show_station("drones")
	, false, int(game_state.profile.drones) == 0)
	if world.drones.is_empty():
		var empty := _panel(station_content, Rect2(0, 240, 1160, 315))
		_texture(empty, "res://assets/art/ships/mining_drone.png", Rect2(70, 40, 250, 210))
		_label(empty, "YOUR AUTOMATED CREW STARTS HERE", Rect2(370, 55, 710, 40), 28, TEXT, 1)
		_wrapped_label(empty, "Install a Drone Bay upgrade to open your first unit slot. Purchased drones are physical spacecraft: they navigate to asteroids, fire mining beams, collect ore fragments and ferry them back to your cargo hold.", Rect2(370, 115, 680, 100), 19, MUTED)
		_button(empty, "OPEN DRONE UPGRADES", Rect2(370, 235, 390, 45), func():
			upgrade_filter = "DRONES"
			show_station("upgrades")
		, true)
	else:
		for i in range(world.drones.size()):
			var drone := world.drones[i]
			if not is_instance_valid(drone): continue
			var x := float(i % 3) * 390.0
			var y := 240.0 + float(i / 3) * 170.0
			var card := _panel(station_content, Rect2(x, y, 370, 150))
			_texture(card, "res://assets/art/ships/mining_drone.png", Rect2(15, 22, 110, 100))
			_label(card, "D-%02d  •  %s" % [drone.drone_id, drone.state], Rect2(135, 18, 215, 28), 18, TEAL, 1)
			_label(card, "CARGO  %d / %d" % [drone.carried_total, drone.capacity], Rect2(135, 57, 215, 24), 11, TEXT, 2)
			_label(card, "MINING  %.1f /s" % drone.mining_power, Rect2(135, 87, 215, 24), 11, MUTED, 2)
			_label(card, "HULL  %d%%" % int(drone.health / 60.0 * 100.0), Rect2(135, 116, 215, 22), 10, MUTED, 2)

func _station_ships() -> void:
	_station_title("SHIPYARD", "ORIGINAL CERES HULLS  •  PURCHASE ONCE  •  SWITCH WITHOUT REFIT PENALTY")
	var scroll := ScrollContainer.new()
	scroll.position = Vector2(0, 82)
	scroll.size = Vector2(1170, 540)
	station_content.add_child(scroll)
	var grid := GridContainer.new()
	grid.columns = 2
	grid.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	grid.add_theme_constant_override("h_separation", 16)
	grid.add_theme_constant_override("v_separation", 16)
	scroll.add_child(grid)
	for ship in Data.SHIPS:
		var owned: bool = ship.id in game_state.profile.unlocked_ships
		var selected: bool = ship.id == game_state.profile.current_ship
		var card := Panel.new()
		card.custom_minimum_size = Vector2(568, 248)
		card.add_theme_stylebox_override("panel", _style(Color("0a1b23"), TEAL if selected else Color("2a4852"), 3))
		grid.add_child(card)
		_texture(card, ship.texture, Rect2(12, 22, 200, 150))
		_label(card, ship.name, Rect2(220, 16, 330, 30), 22, TEAL if selected else TEXT, 1)
		_label(card, ship.class, Rect2(220, 50, 330, 22), 10, MUTED, 2)
		_label(card, "HULL %d  •  SHIELD %d  •  CARGO %d" % [int(ship.hull), int(ship.shield), int(ship.cargo)], Rect2(220, 82, 330, 24), 11, TEXT, 2)
		_label(card, "SPEED %d  •  LASER %d  •  GUN %d" % [int(ship.speed), int(ship.laser), int(ship.weapon)], Rect2(220, 112, 330, 24), 11, TEXT, 2)
		_wrapped_label(card, ship.description, Rect2(20, 169, 520, 44), 14, MUTED)
		if selected:
			_label(card, "CURRENT VESSEL", Rect2(385, 215, 165, 24), 11, TEAL, 2)
		elif owned:
			_button(card, "SELECT VESSEL", Rect2(350, 207, 198, 34), func():
				game_state.select_ship(ship.id)
				world.player.configure(game_state)
				world.player.queue_redraw()
				game_state.save()
				show_station("ships")
			, true)
		else:
			var allowed := int(game_state.profile.level) >= int(ship.level) and int(game_state.profile.credits) >= int(ship.cost)
			_button(card, "LVL %d  •  %s CR" % [int(ship.level), State.format_number(int(ship.cost))], Rect2(330, 207, 218, 34), func():
				if game_state.buy_ship(ship.id):
					game_state.select_ship(ship.id)
					world.player.configure(game_state, true)
					hud.notify("NEW VESSEL ACQUIRED  •  " + ship.name, GOLD)
					sfx_requested.emit("achievement")
					game_state.save()
					show_station("ships")
			, false, not allowed)

func _station_sectors() -> void:
	_station_title("SECTOR CHART", "CHART NEW CLAIMS  •  TRAVEL IS INSTANT FROM THE RELAY")
	var scroll := ScrollContainer.new()
	scroll.position = Vector2(0, 82)
	scroll.size = Vector2(1170, 540)
	station_content.add_child(scroll)
	var grid := GridContainer.new()
	grid.columns = 2
	grid.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	grid.add_theme_constant_override("h_separation", 16)
	grid.add_theme_constant_override("v_separation", 16)
	scroll.add_child(grid)
	for i in range(Data.SECTORS.size()):
		var sector: Dictionary = Data.SECTORS[i]
		var unlocked: bool = i in game_state.profile.unlocked_sectors
		var status := game_state.sector_status(i)
		var color := Color(sector.color) if unlocked else Color("42535c")
		var card := Panel.new()
		card.custom_minimum_size = Vector2(568, 225)
		card.add_theme_stylebox_override("panel", _style(Color("0a1b23"), color.darkened(0.25), 3))
		grid.add_child(card)
		_texture(card, sector.background, Rect2(8, 8, 210, 135), Color(1, 1, 1, 0.85 if unlocked else 0.25))
		_label(card, "%02d  /  %s" % [i + 1, sector.code], Rect2(18, 148, 200, 22), 10, color, 2)
		_label(card, sector.name, Rect2(235, 15, 315, 30), 22, color, 1)
		_label(card, sector.subtitle, Rect2(235, 49, 315, 22), 10, MUTED, 2)
		_label(card, "%s  •  %d RESOURCE TYPES" % [sector.danger, sector.resources.size()], Rect2(235, 80, 315, 24), 11, TEXT, 2)
		_label(card, sector.hazard, Rect2(235, 111, 315, 24), 11, GOLD if unlocked else MUTED, 2)
		if i == world.sector_index:
			_label(card, "CURRENT SECTOR", Rect2(365, 179, 185, 26), 12, TEAL, 2)
		elif unlocked:
			_button(card, "TRAVEL TO SECTOR", Rect2(325, 175, 220, 38), func(): travel_requested.emit(i), true)
		else:
			_button(card, status.reason if not status.can_unlock else "CHART  •  %s CR" % State.format_number(int(sector.cost)), Rect2(250, 175, 295, 38), func():
				if game_state.unlock_sector(i):
					hud.notify("SECTOR CHARTED  •  " + sector.name, color)
					sfx_requested.emit("achievement")
					game_state.save()
					show_station("sectors")
			, status.can_unlock, not status.can_unlock)

func _station_research() -> void:
	_station_title("RESEARCH ARCHIVE", "XENO-MINERALOGY DATABASE  •  DISCOVERIES ARE RECORDED ON COLLECTION")
	var archive := _panel(station_content, Rect2(0, 85, 790, 510))
	for i in range(Data.RESOURCES.size()):
		var resource: Dictionary = Data.RESOURCES[i]
		var known: bool = resource.id in game_state.profile.discovered
		var x := 18.0 + (i % 5) * 152.0
		var y := 22.0 + (i / 5) * 156.0
		_texture(archive, "res://assets/art/icons/resource_%s.png" % resource.id, Rect2(x + 24, y, 88, 88), Color.WHITE if known else Color(0.2, 0.28, 0.32, 0.28))
		_label(archive, resource.name.to_upper() if known else "UNSURVEYED", Rect2(x, y + 93, 140, 25), 13, Color(resource.color) if known else MUTED, 1)
		_label(archive, Data.rarity_name(int(resource.rarity)) if known else "NO DATA", Rect2(x, y + 120, 140, 20), 9, MUTED, 2)
	var stats_panel := _panel(station_content, Rect2(815, 85, 345, 510))
	_label(stats_panel, "FLIGHT ANALYSIS", Rect2(22, 20, 300, 32), 20, TEAL, 1)
	var stats: Dictionary = game_state.profile.stats
	var rows := ["ASTEROIDS  %d" % int(stats.asteroids), "HOSTILES  %d" % int(stats.kills), "ORE UNITS  %d" % int(stats.mined_units), "TOTAL EARNED  %s" % State.format_number(int(stats.earned)), "TOTAL XP  %d" % int(stats.total_xp), "DISCOVERED  %d / 15" % game_state.profile.discovered.size(), "DEATHS  %d" % int(stats.deaths), "FLIGHT TIME  %dm" % (int(stats.play_seconds) / 60)]
	for i in range(rows.size()):
		_label(stats_panel, rows[i], Rect2(22, 78 + i * 43, 300, 28), 12, TEXT, 2)
	_button(stats_panel, "OPEN XENO RESEARCH UPGRADE", Rect2(22, 438, 300, 44), func():
		upgrade_filter = "RESEARCH"
		show_station("upgrades")
	, true)
