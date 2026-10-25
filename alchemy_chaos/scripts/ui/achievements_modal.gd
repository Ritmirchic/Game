class_name AchievementsModal
extends Control

signal closed()

var _main_panel: PanelContainer
var _prog_lbl: Label
var _grid: GridContainer


func _ready() -> void:
	set_anchors_preset(Control.PRESET_FULL_RECT)
	_build_modal()
	GameState.achievement_unlocked.connect(func(_id: String) -> void: if visible: refresh())


func open_modal() -> void:
	visible = true
	_main_panel.scale = Vector2(0.94, 0.94)
	_main_panel.modulate.a = 0.0
	var tw := create_tween().set_parallel(true)
	tw.tween_property(_main_panel, "scale", Vector2.ONE, 0.18).set_trans(Tween.TRANS_BACK)
	tw.tween_property(_main_panel, "modulate:a", 1.0, 0.15)
	refresh()


func close_modal() -> void:
	SoundEngine.play_click(0.9)
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
	_main_panel.position = Vector2(220, 84)
	_main_panel.size = Vector2(1480, 912)
	_main_panel.pivot_offset = Vector2(740, 456)
	_main_panel.add_theme_stylebox_override("panel", AssetVault.style_box("ui_panel_ornate", 20, 24, 18))
	add_child(_main_panel)

	var vbox := VBoxContainer.new()
	vbox.add_theme_constant_override("separation", 14)
	_main_panel.add_child(vbox)

	var hdr := HBoxContainer.new()
	hdr.add_theme_constant_override("separation", 14)
	vbox.add_child(hdr)

	var ic := TextureRect.new()
	ic.texture = AssetVault.tex("res://assets/ui/icon_nav_achievements.png")
	ic.custom_minimum_size = Vector2(44, 44)
	ic.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	hdr.add_child(ic)

	var title := Label.new()
	title.text = "HALL OF HERMETIC HONORS & ACHIEVEMENTS"
	title.add_theme_font_override("font", AssetVault.font_display)
	title.add_theme_font_size_override("font_size", 26)
	title.add_theme_color_override("font_color", Color("#FFE082"))
	title.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	hdr.add_child(title)

	_prog_lbl = Label.new()
	_prog_lbl.add_theme_color_override("font_color", Color("#FFE082"))
	hdr.add_child(_prog_lbl)

	var close_btn := Button.new()
	close_btn.text = "Close [Esc]"
	close_btn.custom_minimum_size = Vector2(124, 42)
	AssetVault.style_button(close_btn, false)
	close_btn.pressed.connect(close_modal)
	hdr.add_child(close_btn)

	var scroll := ScrollContainer.new()
	scroll.size_flags_vertical = Control.SIZE_EXPAND_FILL
	scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	vbox.add_child(scroll)

	_grid = GridContainer.new()
	_grid.columns = 2
	_grid.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_grid.add_theme_constant_override("h_separation", 14)
	_grid.add_theme_constant_override("v_separation", 12)
	scroll.add_child(_grid)


func refresh() -> void:
	_prog_lbl.text = "Unlocked: %d / %d   " % [GameState.unlocked_achievements.size(), AlchemyDB.achievements.size()]
	for ch in _grid.get_children():
		ch.queue_free()

	for a in AlchemyDB.achievements:
		var aid := String(a["id"])
		var unlocked := GameState.unlocked_achievements.has(aid)
		var card := PanelContainer.new()
		card.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		card.add_theme_stylebox_override("panel", AssetVault.style_box("ui_card_selected" if unlocked else "ui_card_locked", 12, 14, 12))
		_grid.add_child(card)

		var hb := HBoxContainer.new()
		hb.add_theme_constant_override("separation", 14)
		card.add_child(hb)

		var ic := TextureRect.new()
		ic.texture = AssetVault.tex("res://assets/ui/icon_nav_achievements.png" if unlocked else "res://assets/ui/icon_lock_mystery.png")
		ic.custom_minimum_size = Vector2(54, 54)
		ic.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
		hb.add_child(ic)

		var vb := VBoxContainer.new()
		vb.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		hb.add_child(vb)

		var t := Label.new()
		t.text = String(a["title"]) + ("  [UNLOCKED]" if unlocked else "")
		t.add_theme_font_override("font", AssetVault.font_display)
		t.add_theme_font_size_override("font_size", 17)
		t.add_theme_color_override("font_color", Color("#FFE494") if unlocked else Color("#9E92B2"))
		vb.add_child(t)

		var d := Label.new()
		d.text = String(a.get("desc", a.get("description", "")))
		d.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
		d.add_theme_color_override("font_color", Color("#E4DBF2") if unlocked else Color("#7B708E"))
		d.add_theme_font_size_override("font_size", 13)
		vb.add_child(d)
