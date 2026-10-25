class_name SettingsPanel
extends PanelContainer
## Complete Settings Modal (Brightness/Exposure, Mouse Sensitivity, FOV,
## Graphics Quality, Master/SFX/Ambience Volumes, Fullscreen, Head-Bob).

signal closed

var _s_bright: HSlider
var _s_sens: HSlider
var _s_fov: HSlider
var _s_master: HSlider
var _s_sfx: HSlider
var _s_amb: HSlider
var _opt_qual: OptionButton
var _chk_fs: CheckBox
var _chk_bob: CheckBox


func _ready() -> void:
	add_theme_stylebox_override("panel", ThemeHelper.make_panel_style(ThemeHelper.BG_DARK, ThemeHelper.BORDER_AMBER, 6, 2))
	custom_minimum_size = Vector2(560, 490)

	var vbox := VBoxContainer.new()
	vbox.add_theme_constant_override("separation", 10)
	add_child(vbox)

	var header := Label.new()
	header.text = "SYSTEM & PERCEPTUAL CALIBRATION"
	header.add_theme_font_size_override("font_size", 20)
	header.add_theme_color_override("font_color", ThemeHelper.ACCENT_AMBER)
	vbox.add_child(header)

	var sep := HSeparator.new()
	vbox.add_child(sep)

	_s_bright = _add_slider_row(vbox, "Display Exposure / Brightness", 0.75, 1.80, 0.05, float(GameState.settings["brightness"]))
	_s_sens = _add_slider_row(vbox, "Camera Look Sensitivity", 0.0008, 0.0055, 0.0002, float(GameState.settings["mouse_sensitivity"]))
	_s_fov = _add_slider_row(vbox, "Field of View (Degrees)", 65.0, 100.0, 1.0, float(GameState.settings["fov"]))
	_s_master = _add_slider_row(vbox, "Master Audio Level", 0.0, 1.0, 0.02, float(GameState.settings["master_volume"]))
	_s_sfx = _add_slider_row(vbox, "Spatial SFX & Voices", 0.0, 1.0, 0.02, float(GameState.settings["sfx_volume"]))
	_s_amb = _add_slider_row(vbox, "Building Ambience", 0.0, 1.0, 0.02, float(GameState.settings["ambience_volume"]))

	var q_row := HBoxContainer.new()
	var q_lbl := Label.new()
	q_lbl.text = "Rendering Quality Preset"
	q_lbl.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	q_lbl.add_theme_color_override("font_color", ThemeHelper.TEXT_MAIN)
	q_row.add_child(q_lbl)
	_opt_qual = OptionButton.new()
	_opt_qual.add_item("Safe / Performance", 0)
	_opt_qual.add_item("Medium Balanced", 1)
	_opt_qual.add_item("High Cinematic", 2)
	_opt_qual.selected = int(GameState.settings.get("quality_preset", 2))
	q_row.add_child(_opt_qual)
	vbox.add_child(q_row)

	var toggles := HBoxContainer.new()
	toggles.add_theme_constant_override("separation", 24)
	_chk_fs = CheckBox.new()
	_chk_fs.text = "Fullscreen Window"
	_chk_fs.button_pressed = bool(GameState.settings.get("fullscreen", false))
	toggles.add_child(_chk_fs)

	_chk_bob = CheckBox.new()
	_chk_bob.text = "Stride Camera Sway"
	_chk_bob.button_pressed = bool(GameState.settings.get("head_bob", true))
	toggles.add_child(_chk_bob)
	vbox.add_child(toggles)

	var btn_row := HBoxContainer.new()
	btn_row.alignment = BoxContainer.ALIGNMENT_END
	btn_row.add_theme_constant_override("separation", 12)
	vbox.add_child(btn_row)

	var apply_btn := Button.new()
	apply_btn.text = "APPLY & SAVE"
	ThemeHelper.style_button(apply_btn, ThemeHelper.ACCENT_GREEN)
	apply_btn.pressed.connect(_on_apply_pressed)
	btn_row.add_child(apply_btn)

	var close_btn := Button.new()
	close_btn.text = "CLOSE"
	ThemeHelper.style_button(close_btn, ThemeHelper.BORDER_AMBER)
	close_btn.pressed.connect(func(): closed.emit())
	btn_row.add_child(close_btn)


func _add_slider_row(parent: VBoxContainer, title: String, min_v: float, max_v: float, step_v: float, cur_v: float) -> HSlider:
	var row := HBoxContainer.new()
	row.add_theme_constant_override("separation", 12)
	var lbl := Label.new()
	lbl.text = title
	lbl.custom_minimum_size = Vector2(235, 0)
	lbl.add_theme_color_override("font_color", ThemeHelper.TEXT_MAIN)
	row.add_child(lbl)

	var sl := HSlider.new()
	sl.min_value = min_v
	sl.max_value = max_v
	sl.step = step_v
	sl.value = cur_v
	sl.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	row.add_child(sl)

	var val_lbl := Label.new()
	val_lbl.custom_minimum_size = Vector2(56, 0)
	val_lbl.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	val_lbl.add_theme_color_override("font_color", ThemeHelper.ACCENT_AMBER)
	val_lbl.text = "%.2f" % cur_v if step_v < 1.0 else "%d" % int(cur_v)
	sl.value_changed.connect(func(v: float):
		val_lbl.text = ("%.4f" % v) if step_v < 0.01 else (("%.2f" % v) if step_v < 1.0 else ("%d" % int(v)))
	)
	row.add_child(val_lbl)
	parent.add_child(row)
	return sl


func _on_apply_pressed() -> void:
	GameState.settings["brightness"] = _s_bright.value
	GameState.settings["mouse_sensitivity"] = _s_sens.value
	GameState.settings["fov"] = _s_fov.value
	GameState.settings["master_volume"] = _s_master.value
	GameState.settings["sfx_volume"] = _s_sfx.value
	GameState.settings["ambience_volume"] = _s_amb.value
	GameState.settings["quality_preset"] = _opt_qual.selected
	GameState.settings["fullscreen"] = _chk_fs.button_pressed
	GameState.settings["head_bob"] = _chk_bob.button_pressed
	GameState.save_settings()
	AudioDirector.play_2d("scanner_confirm", -4.0)
	closed.emit()
