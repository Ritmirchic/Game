extends Control

var _bar: ProgressBar
var _status_lbl: Label
var _seal: TextureRect
var _queue: Array[String] = []
var _idx: int = 0
var _done: bool = false


func _ready() -> void:
	theme = AssetVault.global_theme
	set_anchors_preset(Control.PRESET_FULL_RECT)

	var bg := TextureRect.new()
	bg.texture = AssetVault.tex("res://assets/branding/loading_bg.png")
	bg.set_anchors_preset(Control.PRESET_FULL_RECT)
	bg.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	add_child(bg)

	_seal = TextureRect.new()
	_seal.texture = AssetVault.tex("res://assets/branding/app_icon_256.png")
	_seal.size = Vector2(128, 128)
	_seal.position = Vector2(960 - 64, 155)
	_seal.pivot_offset = Vector2(64, 64)
	_seal.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	add_child(_seal)

	var logo := TextureRect.new()
	logo.texture = AssetVault.tex("res://assets/branding/logo_main.png")
	logo.size = Vector2(780, 250)
	logo.position = Vector2(960 - 390, 290)
	logo.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	logo.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	add_child(logo)

	var panel := PanelContainer.new()
	panel.position = Vector2(560, 740)
	panel.size = Vector2(800, 132)
	panel.add_theme_stylebox_override("panel", AssetVault.style_box("ui_panel_ornate", 18, 24, 18))
	add_child(panel)

	var vb := VBoxContainer.new()
	vb.alignment = BoxContainer.ALIGNMENT_CENTER
	vb.add_theme_constant_override("separation", 12)
	panel.add_child(vb)

	_status_lbl = Label.new()
	_status_lbl.text = "Igniting Aetheric Crucible & Indexing 50 Substances..."
	_status_lbl.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_status_lbl.add_theme_font_override("font", AssetVault.font_display)
	_status_lbl.add_theme_font_size_override("font_size", 18)
	_status_lbl.add_theme_color_override("font_color", Color("#FFE082"))
	vb.add_child(_status_lbl)

	_bar = ProgressBar.new()
	_bar.custom_minimum_size = Vector2(720, 26)
	_bar.max_value = 100.0
	_bar.value = 0.0
	vb.add_child(_bar)

	for ing in AlchemyDB.ingredients_ordered:
		_queue.append(ing.icon_path)
	_queue.append_array([
		"res://assets/lab/lab_bg_stone_arch.png",
		"res://assets/lab/lab_furniture_back.png",
		"res://assets/lab/lab_storage_extra.png",
		"res://assets/lab/lab_desk_foreground.png",
		"res://assets/lab/prop_cauldron_front.png",
		"res://assets/lab/prop_burner_base.png",
	])


func _process(delta: float) -> void:
	_seal.rotation += delta * 0.9
	if _done:
		return

	var batch := 6
	while batch > 0 and _idx < _queue.size():
		AssetVault.tex(_queue[_idx])
		_idx += 1
		batch -= 1

	var pct := float(_idx) / float(maxi(1, _queue.size())) * 100.0
	_bar.value = pct
	if _idx < 50:
		_status_lbl.text = "Cataloging Alchemical Reagents (%d / 50)..." % _idx
	else:
		_status_lbl.text = "Preparing Grand Sanctum Apparatus..."

	if _idx >= _queue.size():
		_done = true
		var tw := create_tween()
		tw.tween_property(self, "modulate:a", 0.0, 0.25)
		tw.tween_callback(func() -> void:
			get_tree().change_scene_to_file("res://scenes/MainMenu.tscn")
		)
