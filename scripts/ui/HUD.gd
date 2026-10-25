class_name GameHUD
extends CanvasLayer
## Complete In-Game HUD, Interaction Prompts, Progress Display, Acoustic dB Meter,
## Document Reader Modal, Anomaly Journal Modal, Settings, and Pause Menu.

signal request_save_game
signal request_return_main_menu

var _player: PlayerController
var _complex: ApartmentComplex

var _postfx_rect: ColorRect
var _postfx_mat: ShaderMaterial
var _flash_rect: ColorRect

var _top_cycle_lbl: Label
var _top_zone_lbl: Label
var _integrity_bar: ProgressBar
var _scanner_lbl: Label
var _db_bar: ProgressBar
var _torch_icon: TextureRect
var _reticle_icon: TextureRect
var _prompt_panel: PanelContainer
var _prompt_lbl: Label
var _toast_lbl: Label
var _subtitle_panel: PanelContainer
var _subtitle_lbl: RichTextLabel

var _pause_backdrop: ColorRect
var _pause_card: PanelContainer
var _journal_panel: JournalPanel
var _settings_panel: SettingsPanel
var _doc_modal: PanelContainer
var _doc_title: Label
var _doc_body: RichTextLabel

var _toast_timer: float = 0.0
var _subtitle_timer: float = 0.0


func setup(player_ref: PlayerController, complex_ref: ApartmentComplex) -> void:
	_player = player_ref
	_complex = complex_ref
	process_mode = Node.PROCESS_MODE_ALWAYS
	add_to_group("hud_listeners")

	# 1. Subtle PostFX Shader Layer
	_postfx_rect = ColorRect.new()
	_postfx_rect.set_anchors_preset(Control.PRESET_FULL_RECT)
	_postfx_rect.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_postfx_mat = ShaderMaterial.new()
	_postfx_mat.shader = load("res://shaders/horror_postfx.gdshader")
	_postfx_rect.material = _postfx_mat
	add_child(_postfx_rect)

	_flash_rect = ColorRect.new()
	_flash_rect.set_anchors_preset(Control.PRESET_FULL_RECT)
	_flash_rect.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_flash_rect.color = Color(0.95, 0.96, 1.0, 0.0)
	add_child(_flash_rect)

	# 2. Top-Left Progress & Integrity Display
	var tl_panel := PanelContainer.new()
	tl_panel.position = Vector2(20, 18)
	tl_panel.add_theme_stylebox_override("panel", ThemeHelper.make_panel_style(Color(0.05, 0.06, 0.08, 0.80), ThemeHelper.BORDER_SUBTLE, 4, 1))
	add_child(tl_panel)

	var tl_vb := VBoxContainer.new()
	tl_vb.add_theme_constant_override("separation", 4)
	tl_panel.add_child(tl_vb)

	_top_cycle_lbl = Label.new()
	_top_cycle_lbl.add_theme_font_size_override("font_size", 16)
	_top_cycle_lbl.add_theme_color_override("font_color", ThemeHelper.ACCENT_AMBER)
	tl_vb.add_child(_top_cycle_lbl)

	_top_zone_lbl = Label.new()
	_top_zone_lbl.add_theme_font_size_override("font_size", 13)
	_top_zone_lbl.add_theme_color_override("font_color", ThemeHelper.TEXT_MUTED)
	tl_vb.add_child(_top_zone_lbl)

	var int_row := HBoxContainer.new()
	var int_lbl := Label.new()
	int_lbl.text = "STABILITY"
	int_lbl.add_theme_font_size_override("font_size", 12)
	int_lbl.add_theme_color_override("font_color", ThemeHelper.TEXT_MUTED)
	int_row.add_child(int_lbl)

	_integrity_bar = ProgressBar.new()
	_integrity_bar.custom_minimum_size = Vector2(150, 14)
	_integrity_bar.show_percentage = false
	_integrity_bar.value = 100.0
	int_row.add_child(_integrity_bar)
	tl_vb.add_child(int_row)

	# 3. Top-Right Controls Quick Legend & Scanner Counter
	var tr_panel := PanelContainer.new()
	tr_panel.set_anchors_preset(Control.PRESET_TOP_RIGHT)
	tr_panel.grow_horizontal = Control.GROW_DIRECTION_BEGIN
	tr_panel.offset_right = -20
	tr_panel.offset_top = 18
	tr_panel.add_theme_stylebox_override("panel", ThemeHelper.make_panel_style(Color(0.05, 0.06, 0.08, 0.80), ThemeHelper.BORDER_SUBTLE, 4, 1))
	add_child(tr_panel)

	var tr_hb := HBoxContainer.new()
	tr_hb.add_theme_constant_override("separation", 10)
	tr_panel.add_child(tr_hb)

	var sc_ic := TextureRect.new()
	sc_ic.texture = ThemeHelper.make_icon_tex("icon_scanner")
	sc_ic.custom_minimum_size = Vector2(24, 24)
	sc_ic.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	sc_ic.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	tr_hb.add_child(sc_ic)

	_scanner_lbl = Label.new()
	_scanner_lbl.add_theme_font_size_override("font_size", 13)
	_scanner_lbl.add_theme_color_override("font_color", ThemeHelper.TEXT_MAIN)
	tr_hb.add_child(_scanner_lbl)

	# 4. Bottom-Left Flashlight & Acoustic Stealth dB Meter
	var bl_panel := PanelContainer.new()
	bl_panel.set_anchors_preset(Control.PRESET_BOTTOM_LEFT)
	bl_panel.grow_vertical = Control.GROW_DIRECTION_BEGIN
	bl_panel.offset_left = 20
	bl_panel.offset_bottom = -20
	bl_panel.add_theme_stylebox_override("panel", ThemeHelper.make_panel_style(Color(0.05, 0.06, 0.08, 0.80), ThemeHelper.BORDER_SUBTLE, 4, 1))
	add_child(bl_panel)

	var bl_hb := HBoxContainer.new()
	bl_hb.add_theme_constant_override("separation", 12)
	bl_panel.add_child(bl_hb)

	_torch_icon = TextureRect.new()
	_torch_icon.texture = ThemeHelper.make_icon_tex("icon_flashlight")
	_torch_icon.custom_minimum_size = Vector2(24, 24)
	_torch_icon.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	bl_hb.add_child(_torch_icon)

	var snd_ic := TextureRect.new()
	snd_ic.texture = ThemeHelper.make_icon_tex("icon_sound")
	snd_ic.custom_minimum_size = Vector2(22, 22)
	snd_ic.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	bl_hb.add_child(snd_ic)

	_db_bar = ProgressBar.new()
	_db_bar.min_value = 0.0
	_db_bar.max_value = 90.0
	_db_bar.custom_minimum_size = Vector2(125, 14)
	_db_bar.show_percentage = false
	bl_hb.add_child(_db_bar)

	# 5. Center Custom Reticle & Interaction Prompt
	_reticle_icon = TextureRect.new()
	_reticle_icon.texture = ThemeHelper.make_icon_tex("icon_scanner")
	_reticle_icon.set_anchors_preset(Control.PRESET_CENTER)
	_reticle_icon.grow_horizontal = Control.GROW_DIRECTION_BOTH
	_reticle_icon.grow_vertical = Control.GROW_DIRECTION_BOTH
	_reticle_icon.custom_minimum_size = Vector2(14, 14)
	_reticle_icon.offset_left = -7
	_reticle_icon.offset_right = 7
	_reticle_icon.offset_top = -7
	_reticle_icon.offset_bottom = 7
	_reticle_icon.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	_reticle_icon.modulate = Color(1, 1, 1, 0.55)
	_reticle_icon.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(_reticle_icon)

	_prompt_panel = PanelContainer.new()
	_prompt_panel.set_anchors_preset(Control.PRESET_CENTER)
	_prompt_panel.grow_horizontal = Control.GROW_DIRECTION_BOTH
	_prompt_panel.offset_top = 42
	_prompt_panel.visible = false
	_prompt_panel.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_prompt_panel.add_theme_stylebox_override("panel", ThemeHelper.make_panel_style(Color(0.06, 0.07, 0.09, 0.88), ThemeHelper.BORDER_AMBER, 4, 1))
	add_child(_prompt_panel)

	_prompt_lbl = Label.new()
	_prompt_lbl.add_theme_font_size_override("font_size", 15)
	_prompt_lbl.add_theme_color_override("font_color", ThemeHelper.TEXT_MAIN)
	_prompt_panel.add_child(_prompt_lbl)

	# 6. Top-Center Event Notification Banner & Bottom-Center Tape Subtitles
	_toast_lbl = Label.new()
	_toast_lbl.set_anchors_preset(Control.PRESET_CENTER_TOP)
	_toast_lbl.grow_horizontal = Control.GROW_DIRECTION_BOTH
	_toast_lbl.offset_top = 24
	_toast_lbl.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_toast_lbl.add_theme_font_size_override("font_size", 17)
	_toast_lbl.visible = false
	add_child(_toast_lbl)

	_subtitle_panel = PanelContainer.new()
	_subtitle_panel.set_anchors_preset(Control.PRESET_CENTER_BOTTOM)
	_subtitle_panel.grow_horizontal = Control.GROW_DIRECTION_BOTH
	_subtitle_panel.grow_vertical = Control.GROW_DIRECTION_BEGIN
	_subtitle_panel.offset_bottom = -64
	_subtitle_panel.custom_minimum_size = Vector2(720, 72)
	_subtitle_panel.visible = false
	_subtitle_panel.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_subtitle_panel.add_theme_stylebox_override("panel", ThemeHelper.make_panel_style(Color(0.04, 0.05, 0.07, 0.90), ThemeHelper.BORDER_AMBER, 4, 1))
	add_child(_subtitle_panel)

	_subtitle_lbl = RichTextLabel.new()
	_subtitle_lbl.bbcode_enabled = true
	_subtitle_lbl.fit_content = true
	_subtitle_panel.add_child(_subtitle_lbl)

	# 7. Modals: Pause Menu, Anomaly Journal, Settings, Document Reader
	_build_modals()

	# Wire signals
	_player.prompt_changed.connect(_on_prompt_changed)
	_player.scan_performed.connect(_on_scan_performed)
	GameState.state_updated.connect(_refresh_hud_numbers)
	GameState.notification_posted.connect(_show_toast)
	GameState.subtitle_triggered.connect(_show_subtitle)
	_refresh_hud_numbers()


func _build_modals() -> void:
	_pause_backdrop = ColorRect.new()
	_pause_backdrop.set_anchors_preset(Control.PRESET_FULL_RECT)
	_pause_backdrop.color = Color(0.02, 0.02, 0.03, 0.72)
	_pause_backdrop.visible = false
	add_child(_pause_backdrop)

	var center := CenterContainer.new()
	center.set_anchors_preset(Control.PRESET_FULL_RECT)
	_pause_backdrop.add_child(center)

	_pause_card = PanelContainer.new()
	_pause_card.custom_minimum_size = Vector2(380, 360)
	_pause_card.add_theme_stylebox_override("panel", ThemeHelper.make_panel_style(ThemeHelper.BG_DARK, ThemeHelper.BORDER_AMBER, 6, 2))
	center.add_child(_pause_card)

	var pvb := VBoxContainer.new()
	pvb.add_theme_constant_override("separation", 12)
	_pause_card.add_child(pvb)

	var ptitle := Label.new()
	ptitle.text = "SHIFT PAUSED"
	ptitle.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	ptitle.add_theme_font_size_override("font_size", 22)
	ptitle.add_theme_color_override("font_color", ThemeHelper.ACCENT_AMBER)
	pvb.add_child(ptitle)

	for spec in [
		["RESUME INSPECTION", func(): _close_all_modals()],
		["OPEN ANOMALY JOURNAL [J]", func(): open_journal()],
		["SETTINGS & CALIBRATION", func(): open_settings()],
		["SAVE SHIFT PROGRESS", func(): request_save_game.emit()],
		["RETURN TO MAIN MENU", Callable(self, "_on_pause_main_menu")],
		["QUIT TO WINDOWS", func(): get_tree().quit(0)],
	]:
		var b := Button.new()
		b.text = spec[0]
		ThemeHelper.style_button(b)
		b.pressed.connect(spec[1])
		pvb.add_child(b)

	_journal_panel = JournalPanel.new()
	_journal_panel.visible = false
	_journal_panel.closed.connect(_close_all_modals)
	center.add_child(_journal_panel)

	_settings_panel = SettingsPanel.new()
	_settings_panel.visible = false
	_settings_panel.closed.connect(func():
		_settings_panel.visible = false
		_pause_card.visible = true
	)
	center.add_child(_settings_panel)

	_doc_modal = PanelContainer.new()
	_doc_modal.visible = false
	_doc_modal.custom_minimum_size = Vector2(640, 440)
	_doc_modal.add_theme_stylebox_override("panel", ThemeHelper.make_panel_style(Color(0.10, 0.09, 0.08, 0.97), ThemeHelper.BORDER_AMBER, 6, 2))
	center.add_child(_doc_modal)

	var dvb := VBoxContainer.new()
	dvb.add_theme_constant_override("separation", 12)
	_doc_modal.add_child(dvb)
	_doc_title = Label.new()
	_doc_title.add_theme_font_size_override("font_size", 19)
	_doc_title.add_theme_color_override("font_color", ThemeHelper.ACCENT_AMBER)
	dvb.add_child(_doc_title)
	_doc_body = RichTextLabel.new()
	_doc_body.size_flags_vertical = Control.SIZE_EXPAND_FILL
	_doc_body.bbcode_enabled = false
	dvb.add_child(_doc_body)
	var dclose := Button.new()
	dclose.text = "CLOSE DOCUMENT [E / ESC]"
	ThemeHelper.style_button(dclose)
	dclose.pressed.connect(_close_all_modals)
	dvb.add_child(dclose)


func _unhandled_input(event: InputEvent) -> void:
	if event.is_action_pressed("pause"):
		if _pause_backdrop.visible:
			_close_all_modals()
		else:
			open_pause()
		get_viewport().set_input_as_handled()
	elif event.is_action_pressed("journal"):
		if _pause_backdrop.visible and _journal_panel.visible:
			_close_all_modals()
		else:
			open_journal()
		get_viewport().set_input_as_handled()


func open_pause() -> void:
	_pause_backdrop.visible = true
	_pause_card.visible = true
	_journal_panel.visible = false
	_settings_panel.visible = false
	_doc_modal.visible = false
	get_tree().paused = true
	_player.controls_locked = true
	if DisplayServer.get_name() != "headless":
		Input.set_mouse_mode(Input.MOUSE_MODE_VISIBLE)


func open_journal() -> void:
	_journal_panel.refresh_contents()
	_pause_backdrop.visible = true
	_pause_card.visible = false
	_journal_panel.visible = true
	_settings_panel.visible = false
	_doc_modal.visible = false
	get_tree().paused = true
	_player.controls_locked = true
	if DisplayServer.get_name() != "headless":
		Input.set_mouse_mode(Input.MOUSE_MODE_VISIBLE)


func open_settings() -> void:
	_pause_backdrop.visible = true
	_pause_card.visible = false
	_journal_panel.visible = false
	_settings_panel.visible = true
	_doc_modal.visible = false


func open_document_modal(doc_id: String) -> void:
	if not GameState.DOCUMENTS.has(doc_id):
		return
	var meta: Dictionary = GameState.DOCUMENTS[doc_id]
	_doc_title.text = meta["title"]
	_doc_body.text = meta["body"]
	_pause_backdrop.visible = true
	_pause_card.visible = false
	_journal_panel.visible = false
	_settings_panel.visible = false
	_doc_modal.visible = true
	get_tree().paused = true
	_player.controls_locked = true
	if DisplayServer.get_name() != "headless":
		Input.set_mouse_mode(Input.MOUSE_MODE_VISIBLE)


func _close_all_modals() -> void:
	_pause_backdrop.visible = false
	_pause_card.visible = false
	_journal_panel.visible = false
	_settings_panel.visible = false
	_doc_modal.visible = false
	get_tree().paused = false
	_player.controls_locked = false
	if DisplayServer.get_name() != "headless":
		Input.set_mouse_mode(Input.MOUSE_MODE_CAPTURED)


func _on_pause_main_menu() -> void:
	get_tree().paused = false
	request_return_main_menu.emit()


func _on_prompt_changed(txt: String, can_interact: bool) -> void:
	_prompt_panel.visible = can_interact and txt != ""
	_prompt_lbl.text = txt
	_reticle_icon.custom_minimum_size = Vector2(24, 24) if can_interact else Vector2(14, 14)
	_reticle_icon.modulate = ThemeHelper.ACCENT_AMBER if can_interact else Color(1, 1, 1, 0.55)


func _on_scan_performed(success: bool, _msg: String) -> void:
	_flash_rect.color = Color(0.35, 1.0, 0.65, 0.24) if success else Color(1.0, 0.25, 0.22, 0.24)
	var tw := create_tween()
	tw.tween_property(_flash_rect, "color:a", 0.0, 0.35)


func _show_toast(msg: String, col: Color) -> void:
	_toast_lbl.text = msg
	_toast_lbl.add_theme_color_override("font_color", col)
	_toast_lbl.visible = true
	_toast_timer = 4.2


func _show_subtitle(speaker: String, transcript: String, duration: float) -> void:
	_subtitle_lbl.text = "[b][color=#efb958]%s:[/color][/b] %s" % [speaker, transcript]
	_subtitle_panel.visible = true
	_subtitle_timer = duration


func _refresh_hud_numbers() -> void:
	_top_cycle_lbl.text = "CLEARANCE CYCLE: %d / %d" % [GameState.verified_cycles, GameState.TARGET_CLEARANCE_CYCLES]
	_integrity_bar.value = GameState.player_integrity
	_scanner_lbl.text = "SCANNER [R]: %d/%d   |   [F] LIGHT   |   [CTRL] CROUCH   |   [J] DOSSIER" % [
		GameState.scanner_charges, GameState.max_scanner_charges
	]


func _process(delta: float) -> void:
	if _player and _complex:
		var z := _complex.get_zone_at(_player.global_position)
		_top_zone_lbl.text = "SECTOR: " + str(z.get("name", "Complex")).to_upper()
		_torch_icon.modulate = ThemeHelper.ACCENT_AMBER if _player.flashlight_on else Color(0.4, 0.4, 0.45, 0.6)

	_db_bar.value = SoundEventBus.current_player_db
	var danger := clampf((100.0 - GameState.player_integrity) / 100.0 + GameState.loop_instability / 160.0, 0.0, 1.0)
	_postfx_mat.set_shader_parameter("danger_pulse", danger)

	if _toast_timer > 0.0:
		_toast_timer -= delta
		if _toast_timer <= 0.0:
			_toast_lbl.visible = false
	if _subtitle_timer > 0.0:
		_subtitle_timer -= delta
		if _subtitle_timer <= 0.0:
			_subtitle_panel.visible = false
