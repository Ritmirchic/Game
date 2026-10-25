extends Control
## Custom Cinematic Main Menu for DON'T LOOK BACK.

var _bg_tex: TextureRect
var _continue_btn: Button
var _stats_lbl: Label
var _settings_modal: SettingsPanel
var _compendium_modal: JournalPanel
var _modal_backdrop: ColorRect
var _t: float = 0.0


func _ready() -> void:
	if DisplayServer.get_name() != "headless":
		Input.set_mouse_mode(Input.MOUSE_MODE_VISIBLE)

	set_anchors_preset(Control.PRESET_FULL_RECT)
	AudioDirector.start_ambience("res://assets/audio/menu_theme.wav", -7.5)

	# Cycles Raytraced Corridor Backdrop with subtle breathing zoom
	_bg_tex = TextureRect.new()
	_bg_tex.set_anchors_preset(Control.PRESET_FULL_RECT)
	_bg_tex.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	_bg_tex.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_COVERED
	if ResourceLoader.exists("res://assets/ui/menu_hero_bg.png"):
		_bg_tex.texture = load("res://assets/ui/menu_hero_bg.png")
	add_child(_bg_tex)

	# Dark left gradient scrim so menu typography pops cleanly without darkening corridor art
	var scrim := ColorRect.new()
	scrim.set_anchors_preset(Control.PRESET_FULL_RECT)
	scrim.color = Color(0.03, 0.035, 0.048, 0.48)
	add_child(scrim)

	var left_card := PanelContainer.new()
	left_card.position = Vector2(64, 68)
	left_card.custom_minimum_size = Vector2(440, 560)
	left_card.add_theme_stylebox_override("panel", ThemeHelper.make_panel_style(Color(0.05, 0.055, 0.072, 0.90), ThemeHelper.BORDER_AMBER, 6, 2))
	add_child(left_card)

	var vb := VBoxContainer.new()
	vb.add_theme_constant_override("separation", 14)
	left_card.add_child(vb)

	var tag := Label.new()
	tag.text = "MUNICIPAL CONTAINMENT ARCHIVE // BLDG 04"
	tag.add_theme_font_size_override("font_size", 12)
	tag.add_theme_color_override("font_color", ThemeHelper.ACCENT_AMBER)
	vb.add_child(tag)

	var title := Label.new()
	title.text = "DON'T LOOK BACK"
	title.add_theme_font_size_override("font_size", 36)
	title.add_theme_color_override("font_color", ThemeHelper.TEXT_MAIN)
	vb.add_child(title)

	var subtitle := Label.new()
	subtitle.text = "Observe every corridor. Verify every distortion.\nNever trust footsteps behind your heels."
	subtitle.add_theme_font_size_override("font_size", 14)
	subtitle.add_theme_color_override("font_color", ThemeHelper.TEXT_MUTED)
	vb.add_child(subtitle)

	vb.add_child(HSeparator.new())

	var btn_new := Button.new()
	btn_new.text = "BEGIN NEW INSPECTION"
	btn_new.icon = ThemeHelper.make_icon_tex("icon_flashlight")
	btn_new.expand_icon = true
	ThemeHelper.style_button(btn_new, ThemeHelper.ACCENT_AMBER)
	btn_new.pressed.connect(func():
		AudioDirector.play_2d("scanner_confirm", -2.0)
		GameSession.load_saved_on_entry = false
		get_tree().change_scene_to_file("res://scenes/GameWorld.tscn")
	)
	vb.add_child(btn_new)

	_continue_btn = Button.new()
	_continue_btn.text = "CONTINUE SAVED SHIFT"
	_continue_btn.icon = ThemeHelper.make_icon_tex("icon_key")
	_continue_btn.expand_icon = true
	_continue_btn.disabled = not GameState.has_save_file()
	ThemeHelper.style_button(_continue_btn, ThemeHelper.ACCENT_GREEN)
	_continue_btn.pressed.connect(func():
		AudioDirector.play_2d("scanner_confirm", -2.0)
		GameSession.load_saved_on_entry = true
		get_tree().change_scene_to_file("res://scenes/GameWorld.tscn")
	)
	vb.add_child(_continue_btn)

	var btn_comp := Button.new()
	btn_comp.text = "ANOMALY COMPENDIUM & ARCHIVE"
	btn_comp.icon = ThemeHelper.make_icon_tex("icon_journal")
	btn_comp.expand_icon = true
	ThemeHelper.style_button(btn_comp)
	btn_comp.pressed.connect(func():
		_compendium_modal.refresh_contents()
		_modal_backdrop.visible = true
		_compendium_modal.visible = true
		_settings_modal.visible = false
	)
	vb.add_child(btn_comp)

	var btn_set := Button.new()
	btn_set.text = "SETTINGS & CALIBRATION"
	btn_set.icon = ThemeHelper.make_icon_tex("icon_scanner")
	btn_set.expand_icon = true
	ThemeHelper.style_button(btn_set)
	btn_set.pressed.connect(func():
		_modal_backdrop.visible = true
		_settings_modal.visible = true
		_compendium_modal.visible = false
	)
	vb.add_child(btn_set)

	var btn_quit := Button.new()
	btn_quit.text = "EXIT TO DESKTOP"
	ThemeHelper.style_button(btn_quit, ThemeHelper.ACCENT_CRIMSON)
	btn_quit.pressed.connect(func(): get_tree().quit(0))
	vb.add_child(btn_quit)

	var spacer := Control.new()
	spacer.size_flags_vertical = Control.SIZE_EXPAND_FILL
	vb.add_child(spacer)

	_stats_lbl = Label.new()
	_stats_lbl.add_theme_font_size_override("font_size", 12)
	_stats_lbl.add_theme_color_override("font_color", ThemeHelper.TEXT_MUTED)
	vb.add_child(_stats_lbl)
	_update_stats_footer()

	# Modal Layer for Settings & Compendium
	_modal_backdrop = ColorRect.new()
	_modal_backdrop.set_anchors_preset(Control.PRESET_FULL_RECT)
	_modal_backdrop.color = Color(0.02, 0.02, 0.03, 0.78)
	_modal_backdrop.visible = false
	add_child(_modal_backdrop)

	var cc := CenterContainer.new()
	cc.set_anchors_preset(Control.PRESET_FULL_RECT)
	_modal_backdrop.add_child(cc)

	_settings_modal = SettingsPanel.new()
	_settings_modal.visible = false
	_settings_modal.closed.connect(func(): _modal_backdrop.visible = false)
	cc.add_child(_settings_modal)

	_compendium_modal = JournalPanel.new()
	_compendium_modal.visible = false
	_compendium_modal.closed.connect(func(): _modal_backdrop.visible = false)
	cc.add_child(_compendium_modal)


func _update_stats_footer() -> void:
	_stats_lbl.text = "ANOMALIES CATALOGED: %d / 30   |   ENDINGS REACHED: %d / 3\nDOCUMENTS ARCHIVED: %d / 8      |   BUILD v1.0.0 WIN64" % [
		GameState.discovered_anomalies.size(),
		GameState.unlocked_endings.size(),
		GameState.collected_documents.size(),
	]


func _process(delta: float) -> void:
	_t += delta
	if _bg_tex:
		var s := 1.02 + 0.015 * sin(_t * 0.35)
		_bg_tex.pivot_offset = size * 0.5
		_bg_tex.scale = Vector2(s, s)
