extends CanvasLayer
## All screens built in code with a custom texture-based theme (no default
## unstyled controls, no emoji). Main menu, HUD, shop, settings, death/victory.
class_name UIManager

signal action_requested(action: String, payload: String)

var assets: AssetLib
var session
var player: Player
var theme_custom: Theme

static func _clear_children(n: Node) -> void:
	for c in n.get_children():
		c.queue_free()

var main_menu: Control
var hud: Control
var shop_win: Control
var pause_win: Control
var death_win: Control
var victory_win: Control
var loading_rect: Control

# HUD element refs
var health_bar: ProgressBar
var stamina_bar: ProgressBar
var ammo_label: Label
var weapon_label: Label
var phase_label: Label
var timer_label: Label
var resource_label: Label
var prompt_label: Label
var toast_label: Label
var banner_label: Label
var vignette: ColorRect
var downed_rect: ColorRect
var slots_box: HBoxContainer
var minimap: Control
var grid: GridContainer
var map_t: float = 0.0
var toast_t: float = 0.0
var banner_t: float = 0.0
var shop_open: bool = false
var join_edit: LineEdit

func _init(a: AssetLib) -> void:
	assets = a
	name = "UIManager"
	layer = 5

func _ready() -> void:
	_build_theme()
	_build_loading()
	_build_main_menu()
	_build_hud()
	_build_shop()
	_build_pause()
	_build_death()
	_build_victory()
	show_main_menu()

# ------------------------------------------------------------------ theme
func _build_theme() -> void:
	theme_custom = Theme.new()
	var font := ThemeDB.fallback_font
	theme_custom.default_font = font
	theme_custom.default_font_size = 18
	var states := {"normal": "button_normal", "hover": "button_hover", "pressed": "button_pressed"}
	for state in states:
		var sb := StyleBoxTexture.new()
		sb.texture = assets.tex(states[state])
		sb.set_texture_margin_all(8.0)
		sb.content_margin_left = 14
		sb.content_margin_right = 14
		sb.content_margin_top = 8
		sb.content_margin_bottom = 8
		theme_custom.set_stylebox(state, "Button", sb)
	theme_custom.set_color("font_color", "Button", Color(0.92, 0.9, 0.84))
	theme_custom.set_color("font_hover_color", "Button", Color(1, 0.85, 0.7))
	var panel_sb := StyleBoxTexture.new()
	panel_sb.texture = assets.tex("ui_panel")
	panel_sb.set_texture_margin_all(8.0)
	panel_sb.set_content_margin_all(16)
	theme_custom.set_stylebox("panel", "PanelContainer", panel_sb)
	theme_custom.set_stylebox("panel", "Window", panel_sb)
	for prog in ["health", "stamina"]:
		var bg := StyleBoxFlat.new()
		bg.bg_color = Color(0.08, 0.08, 0.1, 0.85)
		bg.set_corner_radius_all(3)
		var fill := StyleBoxFlat.new()
		fill.bg_color = Color(0.75, 0.15, 0.13) if prog == "health" else Color(0.7, 0.65, 0.25)
		fill.set_corner_radius_all(3)
		theme_custom.set_stylebox("background", "ProgressBar", bg)
		theme_custom.set_stylebox("fill", "ProgressBar", fill)
	theme_custom.set_color("font_color", "Label", Color(0.92, 0.92, 0.88))
	theme_custom.set_color("font_color", "LineEdit", Color(0.95, 0.95, 0.9))
	var edit_sb := StyleBoxFlat.new()
	edit_sb.bg_color = Color(0.08, 0.09, 0.12, 0.95)
	edit_sb.border_color = Color(0.5, 0.2, 0.18)
	edit_sb.set_border_width_all(1)
	edit_sb.content_margin_left = 8
	edit_sb.content_margin_right = 8
	theme_custom.set_stylebox("normal", "LineEdit", edit_sb)
	theme_custom.set_stylebox("focus", "LineEdit", edit_sb)

func _styled(p: Control) -> Control:
	p.theme = theme_custom
	return p

# --------------------------------------------------------------- loading
func _build_loading() -> void:
	loading_rect = Control.new()
	loading_rect.set_anchors_preset(Control.PRESET_FULL_RECT)
	var tr := TextureRect.new()
	tr.texture = load("res://assets/textures/loading.jpg")
	tr.set_anchors_preset(Control.PRESET_FULL_RECT)
	tr.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	tr.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_COVERED
	loading_rect.add_child(tr)
	var bar := ProgressBar.new()
	bar.custom_minimum_size = Vector2(420, 22)
	bar.position = Vector2(750, 960)
	bar.min_value = 0
	bar.max_value = 100
	bar.value = 0
	bar.name = "LoadBar"
	loading_rect.add_child(bar)
	add_child(loading_rect)

func set_load(pct: float) -> void:
	var b := loading_rect.get_node_or_null("LoadBar") as ProgressBar
	if b:
		b.value = pct

# ------------------------------------------------------------- main menu
func _build_main_menu() -> void:
	main_menu = Control.new()
	main_menu.theme = theme_custom
	main_menu.set_anchors_preset(Control.PRESET_FULL_RECT)
	var bg := ColorRect.new()
	bg.color = Color(0.04, 0.05, 0.07, 0.96)
	bg.set_anchors_preset(Control.PRESET_FULL_RECT)
	main_menu.add_child(bg)
	var logo := TextureRect.new()
	logo.texture = assets.tex("logo")
	logo.custom_minimum_size = Vector2(560, 280)
	logo.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	logo.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	logo.position = Vector2(680, 120)
	main_menu.add_child(logo)
	var box := VBoxContainer.new()
	box.position = Vector2(810, 440)
	box.add_theme_constant_override("separation", 14)
	main_menu.add_child(box)
	for spec in [
		["SINGLE PLAYER", "single"], ["HOST CO-OP (LAN)", "host"],
		["JOIN CO-OP (IP)", "join"], ["SETTINGS", "settings"], ["QUIT", "quit"],
	]:
		var btn := Button.new()
		btn.text = spec[0]
		btn.custom_minimum_size = Vector2(300, 46)
		btn.pressed.connect(_on_menu.bind(spec[1]))
		box.add_child(_styled(btn))
	join_edit = LineEdit.new()
	join_edit.placeholder_text = "Server IP address (e.g. 192.168.1.10)"
	join_edit.custom_minimum_size = Vector2(300, 38)
	join_edit.visible = false
	box.add_child(join_edit)
	var ver := Label.new()
	ver.text = "THE LAST NIGHT v1.0  —  survive ten nights"
	ver.position = Vector2(760, 1000)
	main_menu.add_child(ver)
	add_child(main_menu)

func _on_menu(action: String) -> void:
	if action == "settings":
		_open_pause(true)
		return
	if action == "quit":
		get_tree().quit()
		return
	if action == "join":
		if not join_edit.visible:
			join_edit.visible = true
			return
		action_requested.emit("join", join_edit.text)
		return
	action_requested.emit(action, "")

func show_main_menu() -> void:
	main_menu.visible = true
	hud.visible = false
	shop_win.visible = false
	pause_win.visible = false
	death_win.visible = false
	victory_win.visible = false
	loading_rect.visible = false
	Input.mouse_mode = Input.MOUSE_MODE_VISIBLE

# -------------------------------------------------------------------- HUD
func _build_hud() -> void:
	hud = Control.new()
	hud.theme = theme_custom
	hud.set_anchors_preset(Control.PRESET_FULL_RECT)
	hud.mouse_filter = Control.MOUSE_FILTER_IGNORE
	# crosshair
	var ch := TextureRect.new()
	ch.texture = assets.tex("crosshair")
	ch.custom_minimum_size = Vector2(48, 48)
	ch.set_anchors_preset(Control.PRESET_CENTER)
	ch.position = Vector2(-24, -24)
	hud.add_child(ch)
	# damage vignette
	vignette = ColorRect.new()
	vignette.set_anchors_preset(Control.PRESET_FULL_RECT)
	vignette.color = Color(0.6, 0.02, 0.02, 0.0)
	vignette.mouse_filter = Control.MOUSE_FILTER_IGNORE
	hud.add_child(vignette)
	# top center: phase + timer
	phase_label = Label.new()
	phase_label.position = Vector2(820, 18)
	phase_label.add_theme_font_size_override("font_size", 26)
	hud.add_child(phase_label)
	timer_label = Label.new()
	timer_label.position = Vector2(880, 52)
	timer_label.add_theme_font_size_override("font_size", 22)
	hud.add_child(timer_label)
	# resources top-right
	resource_label = Label.new()
	resource_label.position = Vector2(1560, 18)
	resource_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	resource_label.size = Vector2(330, 140)
	hud.add_child(resource_label)
	# minimap
	minimap = Minimap.new()
	minimap.position = Vector2(1660, 170)
	minimap.size = Vector2(230, 170)
	hud.add_child(minimap)
	# bottom-left bars
	var bars := VBoxContainer.new()
	bars.position = Vector2(30, 900)
	bars.add_theme_constant_override("separation", 8)
	hud.add_child(bars)
	health_bar = ProgressBar.new()
	health_bar.custom_minimum_size = Vector2(300, 24)
	health_bar.min_value = 0
	health_bar.max_value = 100
	bars.add_child(health_bar)
	stamina_bar = ProgressBar.new()
	stamina_bar.custom_minimum_size = Vector2(300, 16)
	stamina_bar.min_value = 0
	stamina_bar.max_value = 100
	bars.add_child(stamina_bar)
	# weapon slots
	slots_box = HBoxContainer.new()
	slots_box.position = Vector2(30, 970)
	slots_box.add_theme_constant_override("separation", 6)
	hud.add_child(slots_box)
	# bottom-right ammo
	weapon_label = Label.new()
	weapon_label.position = Vector2(1500, 940)
	weapon_label.size = Vector2(390, 40)
	weapon_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	weapon_label.add_theme_font_size_override("font_size", 24)
	hud.add_child(weapon_label)
	ammo_label = Label.new()
	ammo_label.position = Vector2(1500, 980)
	ammo_label.size = Vector2(390, 44)
	ammo_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	ammo_label.add_theme_font_size_override("font_size", 30)
	hud.add_child(ammo_label)
	# interaction prompt
	prompt_label = Label.new()
	prompt_label.set_anchors_preset(Control.PRESET_CENTER_BOTTOM)
	prompt_label.position = Vector2(-220, -150)
	prompt_label.size = Vector2(440, 40)
	prompt_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	hud.add_child(prompt_label)
	# toast + banner
	toast_label = Label.new()
	toast_label.position = Vector2(610, 120)
	toast_label.size = Vector2(700, 34)
	toast_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	hud.add_child(toast_label)
	banner_label = Label.new()
	banner_label.position = Vector2(560, 200)
	banner_label.size = Vector2(800, 60)
	banner_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	banner_label.add_theme_font_size_override("font_size", 40)
	hud.add_child(banner_label)
	# downed overlay
	downed_rect = ColorRect.new()
	downed_rect.set_anchors_preset(Control.PRESET_FULL_RECT)
	downed_rect.color = Color(0.25, 0.05, 0.05, 0.35)
	downed_rect.visible = false
	downed_rect.mouse_filter = Control.MOUSE_FILTER_IGNORE
	hud.add_child(downed_rect)
	add_child(hud)

func bind_player(p: Player) -> void:
	player = p
	_clear_children(slots_box)
	for wid in p.owned:
		var tr := TextureRect.new()
		tr.texture = assets.tex(p.session.data.WEAPONS[wid]["icon"])
		tr.custom_minimum_size = Vector2(54, 54)
		tr.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
		slots_box.add_child(tr)

func _process(_delta: float) -> void:
	if session == null or player == null or not hud.visible:
		return
	health_bar.max_value = player.max_health
	health_bar.value = player.health
	stamina_bar.max_value = player.max_stamina
	stamina_bar.value = player.stamina
	downed_rect.visible = player.downed
	# ammo / weapon
	var w := player.weapon()
	weapon_label.text = w.get("name", "")
	if w.get("kind", "") == "gun":
		var at: String = w["ammo_type"]
		ammo_label.text = "%d  /  %d" % [int(player.mags.get(player.weapon_id(), 0)), int(session.resources.get("ammo", {}).get(at, 0))]
	else:
		ammo_label.text = "MELEE"
	# phase
	if session.phase == "prep":
		phase_label.text = "NIGHT %d — PREPARATION" % session.night
		timer_label.text = "Night falls in %s   (start early at the supply terminal)" % _fmt(session.prep_t)
	elif session.phase == "night":
		phase_label.text = "NIGHT %d — DEFEND" % session.night
		timer_label.text = session.director.progress_text()
	# resources
	var r: Dictionary = session.resources
	resource_label.text = "Cash: $%d\nMaterials: %d\nMedkits: %d (H to use)\nKills: %d" % [
		int(r.get("cash", 0)), int(r.get("materials", 0)), int(r.get("medkits", 0)), int(session.stats.get("kills", 0))]
	# prompt
	if is_instance_valid(player.focused):
		if player.focused.has_method("get_prompt"):
			prompt_label.text = player.focused.get_prompt()
		elif player.focused.is_in_group("shop_terminal"):
			prompt_label.text = "Open supply terminal  [E]"
		else:
			prompt_label.text = ""
	else:
		prompt_label.text = ""
	# slot highlight
	for i in slots_box.get_child_count():
		var t: TextureRect = slots_box.get_child(i)
		t.modulate = Color.WHITE if i == player.current else Color(0.55, 0.55, 0.55)
	# timers
	if toast_t > 0:
		toast_t -= _delta
		if toast_t <= 0:
			toast_label.text = ""
	if banner_t > 0:
		banner_t -= _delta
		if banner_t <= 0:
			banner_label.text = ""
	vignette.color.a = max(0.0, vignette.color.a - _delta * 1.8)
	map_t -= _delta
	if map_t <= 0:
		map_t = 0.25
		minimap.queue_redraw()

func _fmt(t: float) -> String:
	return "%02d:%02d" % [int(t) / 60, int(t) % 60]

func damage_flash() -> void:
	vignette.color.a = 0.45

func toast(text: String) -> void:
	toast_label.text = text
	toast_t = 3.5

func banner(text: String) -> void:
	banner_label.text = text
	banner_t = 3.5

func hud_flash(text: String) -> void:
	toast(text)

# ------------------------------------------------------------------- shop
func _build_shop() -> void:
	shop_win = Control.new()
	shop_win.theme = theme_custom
	shop_win.set_anchors_preset(Control.PRESET_FULL_RECT)
	shop_win.visible = false
	var dim := ColorRect.new()
	dim.color = Color(0, 0, 0, 0.55)
	dim.set_anchors_preset(Control.PRESET_FULL_RECT)
	shop_win.add_child(dim)
	var panel := PanelContainer.new()
	panel.position = Vector2(410, 120)
	panel.custom_minimum_size = Vector2(1100, 820)
	shop_win.add_child(panel)
	var vbox := VBoxContainer.new()
	vbox.add_theme_constant_override("separation", 8)
	panel.add_child(vbox)
	var title := Label.new()
	title.text = "SUPPLY TERMINAL"
	title.add_theme_font_size_override("font_size", 32)
	vbox.add_child(title)
	grid = GridContainer.new()
	grid.columns = 3
	grid.name = "ShopGrid"
	grid.add_theme_constant_override("h_separation", 10)
	grid.add_theme_constant_override("v_separation", 10)
	vbox.add_child(grid)
	var close := Button.new()
	close.text = "CLOSE (E)"
	close.custom_minimum_size = Vector2(220, 44)
	close.pressed.connect(func(): close_shop())
	vbox.add_child(close)
	add_child(shop_win)

func open_shop(_p: Player) -> void:
	if session.phase != "prep":
		toast("The terminal is offline during the night.")
		return
	_clear_children(grid)
	for item in session.data.SHOP["items"]:
		var btn := Button.new()
		btn.text = "%s\n$%d" % [item["name"], int(item["price"])]
		btn.custom_minimum_size = Vector2(330, 64)
		btn.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
		btn.pressed.connect(_buy.bind(item))
		grid.add_child(_styled(btn))
	shop_open = true
	shop_win.visible = true
	session.paused = true
	Input.mouse_mode = Input.MOUSE_MODE_VISIBLE

func close_shop() -> void:
	shop_open = false
	shop_win.visible = false
	session.paused = false
	Input.mouse_mode = Input.MOUSE_MODE_CAPTURED

func _buy(item: Dictionary) -> void:
	session.buy_shop_item(item, player)

# ------------------------------------------------------------------ pause
func _build_pause() -> void:
	pause_win = Control.new()
	pause_win.theme = theme_custom
	pause_win.set_anchors_preset(Control.PRESET_FULL_RECT)
	pause_win.visible = false
	var dim := ColorRect.new()
	dim.color = Color(0.02, 0.02, 0.03, 0.9)
	dim.set_anchors_preset(Control.PRESET_FULL_RECT)
	pause_win.add_child(dim)
	var box := VBoxContainer.new()
	box.position = Vector2(760, 260)
	box.add_theme_constant_override("separation", 12)
	pause_win.add_child(box)
	var title := Label.new()
	title.text = "SETTINGS"
	title.add_theme_font_size_override("font_size", 34)
	box.add_child(title)
	for bus in ["Master", "SFX", "Ambient", "UI"]:
		var h := HBoxContainer.new()
		var l := Label.new()
		l.text = bus + " volume"
		l.custom_minimum_size = Vector2(200, 30)
		h.add_child(l)
		var slider := HSlider.new()
		slider.min_value = -40
		slider.max_value = 6
		slider.value = AudioServer.get_bus_volume_db(AudioServer.get_bus_index(bus))
		slider.custom_minimum_size = Vector2(280, 30)
		slider.value_changed.connect(func(v): AudioServer.set_bus_volume_db(AudioServer.get_bus_index(bus), v))
		h.add_child(slider)
		box.add_child(h)
	var sens := HSlider.new()
	sens.min_value = 0.5
	sens.max_value = 3.0
	sens.step = 0.1
	sens.value = 1.0
	sens.custom_minimum_size = Vector2(280, 30)
	sens.value_changed.connect(func(v): if player: player.mouse_sens = 0.0022 * v)
	var sens_row := HBoxContainer.new()
	var sl := Label.new()
	sl.text = "Mouse sensitivity"
	sl.custom_minimum_size = Vector2(200, 30)
	sens_row.add_child(sl)
	sens_row.add_child(sens)
	box.add_child(sens_row)
	var resume := Button.new()
	resume.text = "RESUME"
	resume.custom_minimum_size = Vector2(240, 44)
	resume.pressed.connect(func(): _open_pause(false))
	box.add_child(_styled(resume))
	var menu := Button.new()
	menu.text = "SAVE AND QUIT TO MENU"
	menu.custom_minimum_size = Vector2(240, 44)
	menu.pressed.connect(_quit_to_menu)
	box.add_child(_styled(menu))
	add_child(pause_win)

func _open_pause(open: bool) -> void:
	if session == null:
		pause_win.visible = open
		Input.mouse_mode = Input.MOUSE_MODE_VISIBLE if open else Input.MOUSE_MODE_CAPTURED
		return
	pause_win.visible = open
	session.paused = open
	Input.mouse_mode = Input.MOUSE_MODE_VISIBLE if open else Input.MOUSE_MODE_CAPTURED

func _quit_to_menu() -> void:
	if session:
		session.save.write_campaign(session)
	action_requested.emit("quit_to_menu", "")

# ------------------------------------------------------------- death / win
func _build_death() -> void:
	death_win = Control.new()
	death_win.theme = theme_custom
	death_win.set_anchors_preset(Control.PRESET_FULL_RECT)
	death_win.visible = false
	var dim := ColorRect.new()
	dim.color = Color(0.08, 0.01, 0.01, 0.92)
	dim.set_anchors_preset(Control.PRESET_FULL_RECT)
	death_win.add_child(dim)
	var box := VBoxContainer.new()
	box.position = Vector2(700, 360)
	box.add_theme_constant_override("separation", 16)
	death_win.add_child(box)
	var t := Label.new()
	t.text = "YOU DID NOT SURVIVE THE NIGHT"
	t.add_theme_font_size_override("font_size", 40)
	box.add_child(t)
	var s := Label.new()
	s.text = "The store falls silent. The horde moves on."
	box.add_child(s)
	var btn := Button.new()
	btn.text = "RETURN TO MENU"
	btn.custom_minimum_size = Vector2(280, 46)
	btn.pressed.connect(_quit_to_menu)
	box.add_child(_styled(btn))
	add_child(death_win)

func show_death() -> void:
	death_win.visible = true
	Input.mouse_mode = Input.MOUSE_MODE_VISIBLE

func _build_victory() -> void:
	victory_win = Control.new()
	victory_win.theme = theme_custom
	victory_win.set_anchors_preset(Control.PRESET_FULL_RECT)
	victory_win.visible = false
	var dim := ColorRect.new()
	dim.color = Color(0.02, 0.05, 0.04, 0.94)
	dim.set_anchors_preset(Control.PRESET_FULL_RECT)
	victory_win.add_child(dim)
	var box := VBoxContainer.new()
	box.position = Vector2(640, 320)
	box.add_theme_constant_override("separation", 16)
	victory_win.add_child(box)
	var t := Label.new()
	t.text = "DAWN — TEN NIGHTS SURVIVED"
	t.add_theme_font_size_override("font_size", 44)
	box.add_child(t)
	var s := Label.new()
	s.text = "The evacuation convoy rolls through the parking lot.\nYou walk out of the supermarket alive."
	box.add_child(s)
	var stats := Label.new()
	stats.name = "VictoryStats"
	box.add_child(stats)
	var btn := Button.new()
	btn.text = "RETURN TO MENU"
	btn.custom_minimum_size = Vector2(280, 46)
	btn.pressed.connect(_quit_to_menu)
	box.add_child(_styled(btn))
	add_child(victory_win)

func show_victory() -> void:
	victory_win.visible = true
	Input.mouse_mode = Input.MOUSE_MODE_VISIBLE

func _input(event: InputEvent) -> void:
	if event.is_action_pressed("pause") and session != null and session.phase in ["prep", "night"]:
		if shop_win.visible:
			close_shop()
		else:
			_open_pause(not pause_win.visible)
	if event.is_action_pressed("medkit") and player and not player.downed:
		session.use_medkit(player)
	if event.is_action_pressed("interact") and shop_win.visible:
		close_shop()

# --------------------------------------------------------------- minimap
class Minimap extends Control:
	var ui: UIManager
	func _ready() -> void:
		ui = get_parent().get_parent() as UIManager
	func _draw() -> void:
		if ui == null or ui.session == null:
			return
		var s: Session = ui.session
		var scale_m: float = 5.2
		var ox: float = 8
		var oz: float = 8
		# walls
		for c in s.data.wall_cells():
			draw_rect(Rect2(ox + c.x * scale_m, oz + c.y * scale_m, scale_m, scale_m), Color(0.35, 0.34, 0.32))
		# doors
		for d in s.data.LAYOUT["doors"]:
			for c in d["cells"]:
				draw_rect(Rect2(ox + c[0] * scale_m, oz + c[1] * scale_m, scale_m, scale_m), Color(0.85, 0.7, 0.25))
		# loot
		for n in s.level.interactables.get_children():
			if n is LootCrate and not (n as LootCrate).searched:
				var cell: Vector2i = s.data.world_to_cell(n.global_position)
				draw_circle(Vector2(ox + cell.x * scale_m + 2, oz + cell.y * scale_m + 2), 1.6, Color(0.4, 0.8, 1.0))
		# zombies
		for z in s.zombies:
			if z.state == Zombie.State.DEAD:
				continue
			var cell: Vector2i = s.data.world_to_cell(z.global_position)
			draw_circle(Vector2(ox + cell.x * scale_m + 2, oz + cell.y * scale_m + 2), 2.2, Color(0.9, 0.2, 0.15))
		# players
		for p in s.players:
			var cell: Vector2i = s.data.world_to_cell(p.global_position)
			draw_circle(Vector2(ox + cell.x * scale_m + 2, oz + cell.y * scale_m + 2), 2.6, Color(0.3, 1.0, 0.45))
