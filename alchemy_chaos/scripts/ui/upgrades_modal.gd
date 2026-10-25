class_name UpgradesModal
extends Control

signal closed()

var _main_panel: PanelContainer
var _rp_lbl: Label
var _list_vbox: VBoxContainer


func _ready() -> void:
	set_anchors_preset(Control.PRESET_FULL_RECT)
	_build_modal()
	GameState.rp_changed.connect(func(_v: int) -> void: if visible: refresh())
	GameState.upgrade_purchased.connect(func(_id: String, _l: int) -> void: if visible: refresh())


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
	_main_panel.position = Vector2(220, 82)
	_main_panel.size = Vector2(1480, 916)
	_main_panel.pivot_offset = Vector2(740, 458)
	_main_panel.add_theme_stylebox_override("panel", AssetVault.style_box("ui_panel_ornate", 20, 24, 18))
	add_child(_main_panel)

	var vbox := VBoxContainer.new()
	vbox.add_theme_constant_override("separation", 14)
	_main_panel.add_child(vbox)

	var hdr := HBoxContainer.new()
	hdr.add_theme_constant_override("separation", 14)
	vbox.add_child(hdr)

	var ic := TextureRect.new()
	ic.texture = AssetVault.tex("res://assets/ui/icon_nav_upgrades.png")
	ic.custom_minimum_size = Vector2(44, 44)
	ic.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	hdr.add_child(ic)

	var title := Label.new()
	title.text = "SANCTUM APPARATUS & LABORATORY UPGRADES"
	title.add_theme_font_override("font", AssetVault.font_display)
	title.add_theme_font_size_override("font_size", 26)
	title.add_theme_color_override("font_color", Color("#FFE082"))
	title.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	hdr.add_child(title)

	var rp_ic := TextureRect.new()
	rp_ic.texture = AssetVault.tex("res://assets/ui/icon_currency_rp.png")
	rp_ic.custom_minimum_size = Vector2(36, 36)
	rp_ic.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	hdr.add_child(rp_ic)

	_rp_lbl = Label.new()
	_rp_lbl.add_theme_font_override("font", AssetVault.font_display)
	_rp_lbl.add_theme_font_size_override("font_size", 22)
	_rp_lbl.add_theme_color_override("font_color", Color("#DFB8FF"))
	hdr.add_child(_rp_lbl)

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

	_list_vbox = VBoxContainer.new()
	_list_vbox.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_list_vbox.add_theme_constant_override("separation", 12)
	scroll.add_child(_list_vbox)


func refresh() -> void:
	_rp_lbl.text = "%d RP Available   " % GameState.research_points
	for ch in _list_vbox.get_children():
		ch.queue_free()

	for u in AlchemyDB.upgrades:
		var uid := String(u["id"])
		var lvl := GameState.get_upgrade_level(uid)
		var max_l := int(u["max_level"])
		var is_max := lvl >= max_l

		var card := PanelContainer.new()
		card.add_theme_stylebox_override("panel", AssetVault.style_box("ui_card_selected" if is_max else "ui_card_normal", 14, 16, 12))
		_list_vbox.add_child(card)

		var hb := HBoxContainer.new()
		hb.add_theme_constant_override("separation", 18)
		card.add_child(hb)

		var uic := TextureRect.new()
		uic.texture = AssetVault.tex(String(u["icon"]))
		uic.custom_minimum_size = Vector2(88, 88)
		uic.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
		hb.add_child(uic)

		var mid_v := VBoxContainer.new()
		mid_v.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		mid_v.add_theme_constant_override("separation", 4)
		hb.add_child(mid_v)

		var name_row := HBoxContainer.new()
		name_row.add_theme_constant_override("separation", 14)
		mid_v.add_child(name_row)

		var nm := Label.new()
		nm.text = String(u["name"])
		nm.add_theme_font_override("font", AssetVault.font_display)
		nm.add_theme_font_size_override("font_size", 20)
		nm.add_theme_color_override("font_color", Color("#FFE494"))
		name_row.add_child(nm)

		# Level Pips
		for p in range(max_l):
			var pip := PanelContainer.new()
			pip.custom_minimum_size = Vector2(28, 16)
			var sb := StyleBoxFlat.new()
			sb.bg_color = Color("#52E8B4") if p < lvl else Color(0.18, 0.15, 0.26, 1.0)
			sb.border_color = Color("#F2CA74")
			sb.set_border_width_all(1)
			sb.set_corner_radius_all(4)
			pip.add_theme_stylebox_override("panel", sb)
			name_row.add_child(pip)

		var desc := Label.new()
		desc.text = String(u["description"])
		desc.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
		desc.add_theme_color_override("font_color", Color("#DDD2EE"))
		desc.add_theme_font_size_override("font_size", 14)
		mid_v.add_child(desc)

		var tier_txt := ""
		if lvl > 0:
			tier_txt = "Active (Lv.%d): %s" % [lvl, String(u["tiers"][lvl - 1])]
		if not is_max:
			if not tier_txt.is_empty():
				tier_txt += "\n"
			tier_txt += "Next (Lv.%d): %s" % [lvl + 1, String(u["tiers"][lvl])]
		var tier_lbl := Label.new()
		tier_lbl.text = tier_txt
		tier_lbl.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
		tier_lbl.add_theme_color_override("font_color", Color("#7AF8D4") if lvl > 0 else Color("#FFCF78"))
		tier_lbl.add_theme_font_size_override("font_size", 13)
		mid_v.add_child(tier_lbl)

		var buy_btn := Button.new()
		buy_btn.custom_minimum_size = Vector2(210, 56)
		buy_btn.size_flags_vertical = Control.SIZE_SHRINK_CENTER
		if is_max:
			buy_btn.text = "MAX LEVEL (Lv.3)"
			buy_btn.disabled = true
			AssetVault.style_button(buy_btn, false)
		else:
			var cost := GameState.get_next_upgrade_cost(uid)
			var afford := GameState.can_purchase_upgrade(uid)
			buy_btn.text = "Upgrade (%d RP)" % cost
			buy_btn.disabled = not afford
			AssetVault.style_button(buy_btn, afford)
			buy_btn.pressed.connect(func() -> void:
				GameState.purchase_upgrade(uid)
			)
		hb.add_child(buy_btn)
