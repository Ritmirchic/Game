class_name ChallengesModal
extends Control

signal closed()

var filter_type: String = "ALL"
var _main_panel: PanelContainer
var _filter_row: HBoxContainer
var _prog_lbl: Label
var _list_vbox: VBoxContainer


func _ready() -> void:
	set_anchors_preset(Control.PRESET_FULL_RECT)
	_build_modal()
	GameState.challenge_completed.connect(func(_id: String) -> void: if visible: refresh())


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
	_main_panel.position = Vector2(200, 74)
	_main_panel.size = Vector2(1520, 932)
	_main_panel.pivot_offset = Vector2(760, 466)
	_main_panel.add_theme_stylebox_override("panel", AssetVault.style_box("ui_panel_ornate", 20, 24, 18))
	add_child(_main_panel)

	var vbox := VBoxContainer.new()
	vbox.add_theme_constant_override("separation", 12)
	_main_panel.add_child(vbox)

	var hdr := HBoxContainer.new()
	hdr.add_theme_constant_override("separation", 14)
	vbox.add_child(hdr)

	var ic := TextureRect.new()
	ic.texture = AssetVault.tex("res://assets/ui/icon_nav_challenges.png")
	ic.custom_minimum_size = Vector2(44, 44)
	ic.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	hdr.add_child(ic)

	var title := Label.new()
	title.text = "ROYAL COMMISSIONS, RIDDLES & TIMED TRIALS"
	title.add_theme_font_override("font", AssetVault.font_display)
	title.add_theme_font_size_override("font_size", 26)
	title.add_theme_color_override("font_color", Color("#FFE082"))
	title.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	hdr.add_child(title)

	_prog_lbl = Label.new()
	_prog_lbl.add_theme_color_override("font_color", Color("#7AF8D4"))
	hdr.add_child(_prog_lbl)

	var close_btn := Button.new()
	close_btn.text = "Close [Esc]"
	close_btn.custom_minimum_size = Vector2(124, 42)
	AssetVault.style_button(close_btn, false)
	close_btn.pressed.connect(close_modal)
	hdr.add_child(close_btn)

	_filter_row = HBoxContainer.new()
	_filter_row.add_theme_constant_override("separation", 8)
	vbox.add_child(_filter_row)

	var scroll := ScrollContainer.new()
	scroll.size_flags_vertical = Control.SIZE_EXPAND_FILL
	scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	vbox.add_child(scroll)

	_list_vbox = VBoxContainer.new()
	_list_vbox.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_list_vbox.add_theme_constant_override("separation", 10)
	scroll.add_child(_list_vbox)


func refresh() -> void:
	_prog_lbl.text = "Completed: %d / %d   " % [GameState.completed_challenges.size(), AlchemyDB.challenges.size()]
	for ch in _filter_row.get_children():
		ch.queue_free()
	var filters := [
		["ALL", "All Challenges"],
		["SUBSTANCE", "Substance Commissions"],
		["MULTI_STEP", "Multi-Step Chains"],
		["HIDDEN_PUZZLE", "Hidden Riddles"],
		["TIMED", "Timed Trials"],
		["OBJECTIVE", "Research Objectives"],
	]
	for f in filters:
		var b := Button.new()
		b.text = f[1]
		AssetVault.style_button(b, filter_type == f[0])
		var ft: String = f[0]
		b.pressed.connect(func() -> void:
			filter_type = ft
			refresh()
		)
		_filter_row.add_child(b)

	for ch in _list_vbox.get_children():
		ch.queue_free()

	for ch in AlchemyDB.challenges:
		var ctype := String(ch["type"])
		if filter_type != "ALL" and ctype != filter_type:
			continue
		var cid := String(ch["id"])
		var done := GameState.completed_challenges.has(cid)
		var is_active_timed := (GameState.active_timed_id == cid)

		var card := PanelContainer.new()
		card.add_theme_stylebox_override("panel", AssetVault.style_box("ui_card_selected" if done else "ui_card_normal", 12, 16, 12))
		_list_vbox.add_child(card)

		var hb := HBoxContainer.new()
		hb.add_theme_constant_override("separation", 16)
		card.add_child(hb)

		var badge_ic := TextureRect.new()
		badge_ic.texture = AssetVault.tex("res://assets/ui/icon_nav_achievements.png" if done else "res://assets/ui/icon_nav_challenges.png")
		badge_ic.custom_minimum_size = Vector2(54, 54)
		badge_ic.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
		hb.add_child(badge_ic)

		var vb := VBoxContainer.new()
		vb.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		hb.add_child(vb)

		var t := Label.new()
		t.text = "[%s]  %s   %s" % [ctype.replace("_", " "), String(ch["title"]), "- COMPLETED" if done else ""]
		t.add_theme_font_override("font", AssetVault.font_display)
		t.add_theme_font_size_override("font_size", 18)
		t.add_theme_color_override("font_color", Color("#7AF8D4") if done else Color("#FFE494"))
		vb.add_child(t)

		var desc := Label.new()
		desc.text = String(ch["description"])
		desc.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
		desc.add_theme_color_override("font_color", Color("#DDD2EE"))
		vb.add_child(desc)

		var rew := Label.new()
		rew.text = "Reward: +%d Research Points" % int(ch["rp_reward"])
		rew.add_theme_color_override("font_color", Color("#DFB8FF"))
		rew.add_theme_font_size_override("font_size", 13)
		vb.add_child(rew)

		if ctype == "TIMED":
			var act_btn := Button.new()
			act_btn.custom_minimum_size = Vector2(195, 48)
			act_btn.size_flags_vertical = Control.SIZE_SHRINK_CENTER
			if done:
				act_btn.text = "Replay Trial"
				AssetVault.style_button(act_btn, false)
			elif is_active_timed:
				act_btn.text = "Trial Active!"
				act_btn.disabled = true
				AssetVault.style_button(act_btn, true)
			else:
				act_btn.text = "Start Timed Trial"
				AssetVault.style_button(act_btn, true)
			act_btn.pressed.connect(func() -> void:
				SoundEngine.play_click(1.1)
				GameState.start_timed_challenge(cid)
				close_modal()
			)
			hb.add_child(act_btn)
