class_name SettingsModal
extends Control

signal return_to_menu_requested()
signal progress_reset()
signal closed()

var _main_panel: PanelContainer
var _master_slider: HSlider
var _sfx_slider: HSlider
var _amb_slider: HSlider
var _shake_chk: CheckButton
var _auto_chk: CheckButton
var _fs_chk: CheckButton
var _confirm_reset: bool = false
var _reset_btn: Button
var _status_lbl: Label


func _ready() -> void:
	set_anchors_preset(Control.PRESET_FULL_RECT)
	_build_modal()


func open_modal() -> void:
	_confirm_reset = false
	visible = true
	_main_panel.scale = Vector2(0.94, 0.94)
	_main_panel.modulate.a = 0.0
	var tw := create_tween().set_parallel(true)
	tw.tween_property(_main_panel, "scale", Vector2.ONE, 0.18).set_trans(Tween.TRANS_BACK)
	tw.tween_property(_main_panel, "modulate:a", 1.0, 0.15)
	refresh()


func close_modal() -> void:
	SoundEngine.play_click(0.9)
	GameState.save_game()
	visible = false
	closed.emit()


func _build_modal() -> void:
	var dim := ColorRect.new()
	dim.set_anchors_preset(Control.PRESET_FULL_RECT)
	dim.color = Color(0.03, 0.02, 0.06, 0.78)
	dim.gui_input.connect(func(ev: InputEvent) -> void:
		if ev is InputEventMouseButton and ev.pressed and ev.button_index == MOUSE_BUTTON_LEFT:
			close_modal()
	)
	add_child(dim)

	_main_panel = PanelContainer.new()
	_main_panel.position = Vector2(510, 140)
	_main_panel.size = Vector2(900, 800)
	_main_panel.pivot_offset = Vector2(450, 400)
	_main_panel.add_theme_stylebox_override("panel", AssetVault.style_box("ui_panel_ornate", 20, 28, 22))
	add_child(_main_panel)

	var vbox := VBoxContainer.new()
	vbox.add_theme_constant_override("separation", 18)
	_main_panel.add_child(vbox)

	var hdr := HBoxContainer.new()
	hdr.add_theme_constant_override("separation", 14)
	vbox.add_child(hdr)

	var ic := TextureRect.new()
	ic.texture = AssetVault.tex("res://assets/ui/icon_nav_settings.png")
	ic.custom_minimum_size = Vector2(42, 42)
	ic.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	hdr.add_child(ic)

	var title := Label.new()
	title.text = "SANCTUM SETTINGS & ARCHIVAL LEDGER"
	title.add_theme_font_override("font", AssetVault.font_display)
	title.add_theme_font_size_override("font_size", 24)
	title.add_theme_color_override("font_color", Color("#FFE082"))
	title.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	hdr.add_child(title)

	var close_btn := Button.new()
	close_btn.text = "Close [Esc]"
	close_btn.custom_minimum_size = Vector2(120, 40)
	AssetVault.style_button(close_btn, false)
	close_btn.pressed.connect(close_modal)
	hdr.add_child(close_btn)

	_master_slider = _add_slider_row(vbox, "Master Audio Volume", func(v: float) -> void:
		GameState.update_setting("master_volume", v)
	)
	_sfx_slider = _add_slider_row(vbox, "Reaction & UI Sound Effects", func(v: float) -> void:
		GameState.update_setting("sfx_volume", v)
	)
	_amb_slider = _add_slider_row(vbox, "Sanctum Ambient Atmosphere", func(v: float) -> void:
		GameState.update_setting("ambient_volume", v)
	)

	_shake_chk = CheckButton.new()
	_shake_chk.text = "Crucible Reaction Screen Shake"
	_shake_chk.toggled.connect(func(on: bool) -> void:
		GameState.update_setting("screen_shake", on)
	)
	vbox.add_child(_shake_chk)

	_auto_chk = CheckButton.new()
	_auto_chk.text = "Auto-Transmute When All Pedestals Are Filled"
	_auto_chk.toggled.connect(func(on: bool) -> void:
		GameState.update_setting("auto_transmute", on)
	)
	vbox.add_child(_auto_chk)

	_fs_chk = CheckButton.new()
	_fs_chk.text = "Fullscreen Window Mode"
	_fs_chk.toggled.connect(func(on: bool) -> void:
		GameState.update_setting("fullscreen", on)
	)
	vbox.add_child(_fs_chk)

	_status_lbl = Label.new()
	_status_lbl.text = "Save Format v%d (SHA-256 Checksum Verified)" % GameState.SAVE_VERSION
	_status_lbl.add_theme_color_override("font_color", Color("#7AF8D4"))
	vbox.add_child(_status_lbl)

	var btn_row := HBoxContainer.new()
	btn_row.add_theme_constant_override("separation", 14)
	vbox.add_child(btn_row)

	var save_btn := Button.new()
	save_btn.text = "Save Game Now"
	save_btn.custom_minimum_size = Vector2(200, 48)
	AssetVault.style_button(save_btn, true)
	save_btn.pressed.connect(func() -> void:
		SoundEngine.play_click(1.06)
		var ok := GameState.save_game()
		_status_lbl.text = "Sanctum Ledger Saved Successfully!" if ok else "Save Error!"
	)
	btn_row.add_child(save_btn)

	var menu_btn := Button.new()
	menu_btn.text = "Return to Main Menu"
	menu_btn.custom_minimum_size = Vector2(220, 48)
	AssetVault.style_button(menu_btn, false)
	menu_btn.pressed.connect(func() -> void:
		GameState.save_game()
		return_to_menu_requested.emit()
	)
	btn_row.add_child(menu_btn)

	_reset_btn = Button.new()
	_reset_btn.text = "Reset All Progress"
	_reset_btn.custom_minimum_size = Vector2(220, 48)
	AssetVault.style_button(_reset_btn, false)
	_reset_btn.pressed.connect(func() -> void:
		if not _confirm_reset:
			_confirm_reset = true
			_reset_btn.text = "Click Again to Confirm!"
		else:
			SoundEngine.play_fizzle()
			GameState.reset_to_fresh_state(true)
			_confirm_reset = false
			_reset_btn.text = "Progress Reset!"
			progress_reset.emit()
	)
	btn_row.add_child(_reset_btn)


func _add_slider_row(parent: VBoxContainer, label_text: String, cb: Callable) -> HSlider:
	var row := HBoxContainer.new()
	row.add_theme_constant_override("separation", 16)
	parent.add_child(row)

	var lbl := Label.new()
	lbl.text = label_text
	lbl.custom_minimum_size = Vector2(310, 28)
	lbl.add_theme_color_override("font_color", Color("#EDE2FA"))
	row.add_child(lbl)

	var sl := HSlider.new()
	sl.min_value = 0.0
	sl.max_value = 1.0
	sl.step = 0.02
	sl.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	sl.custom_minimum_size = Vector2(280, 28)
	sl.value_changed.connect(cb)
	row.add_child(sl)
	return sl


func refresh() -> void:
	_master_slider.set_value_no_signal(float(GameState.settings.get("master_volume", 0.85)))
	_sfx_slider.set_value_no_signal(float(GameState.settings.get("sfx_volume", 0.85)))
	_amb_slider.set_value_no_signal(float(GameState.settings.get("ambient_volume", 0.55)))
	_shake_chk.set_pressed_no_signal(bool(GameState.settings.get("screen_shake", true)))
	_auto_chk.set_pressed_no_signal(bool(GameState.settings.get("auto_transmute", false)))
	_fs_chk.set_pressed_no_signal(bool(GameState.settings.get("fullscreen", false)))
	_reset_btn.text = "Reset All Progress"
