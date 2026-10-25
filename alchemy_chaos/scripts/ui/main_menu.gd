extends Control

var _lab_stage: LaboratoryStage
var _settings_modal: SettingsModal
var _achievements_modal: AchievementsModal
var _summary_lbl: Label
var _continue_btn: Button
var _confirm_new: bool = false
var _new_btn: Button


func _ready() -> void:
	theme = AssetVault.global_theme
	set_anchors_preset(Control.PRESET_FULL_RECT)
	SoundEngine.start_ambient()

	_lab_stage = LaboratoryStage.new()
	add_child(_lab_stage)

	var shade := ColorRect.new()
	shade.set_anchors_preset(Control.PRESET_FULL_RECT)
	shade.color = Color(0.04, 0.02, 0.08, 0.56)
	add_child(shade)

	var center_box := VBoxContainer.new()
	center_box.position = Vector2(540, 88)
	center_box.size = Vector2(840, 904)
	center_box.alignment = BoxContainer.ALIGNMENT_CENTER
	center_box.add_theme_constant_override("separation", 18)
	add_child(center_box)

	var logo := TextureRect.new()
	logo.texture = AssetVault.tex("res://assets/branding/logo_main.png")
	logo.custom_minimum_size = Vector2(760, 240)
	logo.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	logo.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	center_box.add_child(logo)

	var tagline := Label.new()
	tagline.text = "A Grand Hermetic Sandbox of Elemental Transmutation, Discovery & Forbidden Synthesis"
	tagline.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	tagline.add_theme_font_override("font", AssetVault.font_italic)
	tagline.add_theme_font_size_override("font_size", 23)
	tagline.add_theme_color_override("font_color", Color("#F5DFB0"))
	center_box.add_child(tagline)

	var menu_panel := PanelContainer.new()
	menu_panel.custom_minimum_size = Vector2(540, 470)
	menu_panel.size_flags_horizontal = Control.SIZE_SHRINK_CENTER
	menu_panel.add_theme_stylebox_override("panel", AssetVault.style_box("ui_panel_ornate", 20, 32, 26))
	center_box.add_child(menu_panel)

	var btn_v := VBoxContainer.new()
	btn_v.alignment = BoxContainer.ALIGNMENT_CENTER
	btn_v.add_theme_constant_override("separation", 14)
	menu_panel.add_child(btn_v)

	_summary_lbl = Label.new()
	_summary_lbl.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_summary_lbl.add_theme_color_override("font_color", Color("#7AF8D4"))
	_summary_lbl.add_theme_font_size_override("font_size", 15)
	btn_v.add_child(_summary_lbl)

	var has_save := GameState.has_existing_save() or GameState.total_experiments > 0

	_continue_btn = Button.new()
	_continue_btn.text = "ENTER SANCTUM (CONTINUE)" if has_save else "BEGIN SANCTUM RESEARCH"
	_continue_btn.custom_minimum_size = Vector2(440, 58)
	AssetVault.style_button(_continue_btn, true)
	_continue_btn.pressed.connect(func() -> void:
		SoundEngine.play_click(1.08)
		get_tree().change_scene_to_file("res://scenes/MainGame.tscn")
	)
	btn_v.add_child(_continue_btn)

	_new_btn = Button.new()
	_new_btn.text = "NEW ALCHEMICAL LEDGER"
	_new_btn.custom_minimum_size = Vector2(440, 52)
	AssetVault.style_button(_new_btn, false)
	_new_btn.pressed.connect(func() -> void:
		if has_save and not _confirm_new:
			_confirm_new = true
			_new_btn.text = "CLICK AGAIN TO OVERWRITE LEDGER"
			SoundEngine.play_click(0.95)
		else:
			SoundEngine.play_click(1.1)
			GameState.reset_to_fresh_state(true)
			get_tree().change_scene_to_file("res://scenes/MainGame.tscn")
	)
	btn_v.add_child(_new_btn)

	var ach_btn := Button.new()
	ach_btn.text = "HALL OF ACHIEVEMENTS"
	ach_btn.custom_minimum_size = Vector2(440, 52)
	AssetVault.style_button(ach_btn, false)
	ach_btn.pressed.connect(func() -> void:
		SoundEngine.play_click(1.04)
		_achievements_modal.open_modal()
	)
	btn_v.add_child(ach_btn)

	var set_btn := Button.new()
	set_btn.text = "SANCTUM SETTINGS"
	set_btn.custom_minimum_size = Vector2(440, 52)
	AssetVault.style_button(set_btn, false)
	set_btn.pressed.connect(func() -> void:
		SoundEngine.play_click(1.04)
		_settings_modal.open_modal()
	)
	btn_v.add_child(set_btn)

	var quit_btn := Button.new()
	quit_btn.text = "EXIT TO DESKTOP"
	quit_btn.custom_minimum_size = Vector2(440, 50)
	AssetVault.style_button(quit_btn, false)
	quit_btn.pressed.connect(func() -> void:
		GameState.save_game()
		get_tree().quit()
	)
	btn_v.add_child(quit_btn)

	var ver_lbl := Label.new()
	ver_lbl.text = "ALCHEMY CHAOS v1.0.0  |  50 Substances  |  120 Transmutations  |  Grand Sanctum Edition"
	ver_lbl.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	ver_lbl.add_theme_color_override("font_color", Color("#A799C0"))
	ver_lbl.add_theme_font_size_override("font_size", 13)
	center_box.add_child(ver_lbl)

	_achievements_modal = AchievementsModal.new()
	_achievements_modal.visible = false
	add_child(_achievements_modal)

	_settings_modal = SettingsModal.new()
	_settings_modal.visible = false
	_settings_modal.progress_reset.connect(_refresh_summary)
	add_child(_settings_modal)

	_refresh_summary()


func _refresh_summary() -> void:
	_summary_lbl.text = "%s  |  %d/50 Substances  |  %d/120 Formulas  |  %d RP" % [
		GameState.get_current_rank_title(),
		GameState.discovered_ingredients.size(),
		GameState.discovered_recipes.size(),
		GameState.research_points
	]
