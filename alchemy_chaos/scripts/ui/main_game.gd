class_name MainGame
extends Control

var lab_stage: LaboratoryStage
var reaction_director: ReactionDirector

# HUD Elements
var _rank_lbl: Label
var _sub_count_lbl: Label
var _rec_count_lbl: Label
var _rp_count_lbl: Label

# Left Inventory Cabinet
var _inv_search: LineEdit
var _inv_sort: OptionButton
var _storage_lbl: Label
var _inv_grid: GridContainer
var _inv_filter_cat: String = "All"
var _inv_search_q: String = ""
var _inv_sort_idx: int = 0

# Pedestals & Crucible Controls
var pedestals: Array[PedestalSlot] = []
var _transmute_btn: Button
var _clear_btn: Button
var _chain_btn: Button
var _scan_btn: Button
var _auto_btn: Button
var _last_result_id: String = ""

# Result Ledger & Timed Banner
var _result_icon: TextureRect
var _result_title: Label
var _result_lore: Label
var _recent_row: HBoxContainer
var _timed_panel: PanelContainer
var _timed_lbl: Label

# Tooltip & Toasts
var _tooltip_panel: PanelContainer
var _tooltip_title: Label
var _tooltip_meta: Label
var _tooltip_desc: Label
var _toast_vbox: VBoxContainer

# Modals
var journal_modal: JournalModal
var upgrades_modal: UpgradesModal
var challenges_modal: ChallengesModal
var achievements_modal: AchievementsModal
var settings_modal: SettingsModal


func _ready() -> void:
	theme = AssetVault.global_theme
	set_anchors_preset(Control.PRESET_FULL_RECT)
	SoundEngine.start_ambient()

	# 1. Multi-layer Laboratory Stage
	lab_stage = LaboratoryStage.new()
	lab_stage.hotspot_clicked.connect(_on_lab_hotspot)
	lab_stage.detector_lantern_clicked.connect(_on_resonance_scan)
	add_child(lab_stage)

	# 2. Cauldron Mouth Drop Target
	var cauldron_drop := CauldronDropZone.new()
	cauldron_drop.position = Vector2(1005, 490)
	cauldron_drop.size = Vector2(302, 230)
	cauldron_drop.ingredient_dropped.connect(_on_ingredient_clicked_or_dropped)
	add_child(cauldron_drop)

	# 3. Pedestal Slots over Brass Pedestals
	_build_pedestals()

	# 4. Top HUD Bar
	_build_top_hud()

	# 5. Left Inventory Cabinet
	_build_left_cabinet()

	# 6. Crucible Control Dock + Right Result Ledger + Timed Trial Banner
	_build_crucible_dock()
	_build_result_ledger()
	_build_timed_banner()

	# 7. Reaction Director (above workbench & pedestals, below modals)
	reaction_director = ReactionDirector.new()
	reaction_director.stage_ref = lab_stage
	reaction_director.z_index = 20
	reaction_director.reaction_sequence_finished.connect(func(_o: Dictionary) -> void: _refresh_hud_and_buttons())
	add_child(reaction_director)

	# 8. Toast Stack & Hover Tooltip
	_build_toast_and_tooltip()

	# 9. Modals
	_build_modals()

	# 10. Connect GameState signals
	GameState.rp_changed.connect(func(_v: int) -> void: _refresh_hud_and_buttons())
	GameState.upgrade_purchased.connect(func(_id: String, _l: int) -> void: refresh_all())
	GameState.toast_requested.connect(_on_toast_requested)
	GameState.auto_experiment_triggered.connect(_on_automaton_triggered)
	GameState.timed_challenge_updated.connect(_on_timed_updated)
	GameState.state_reloaded.connect(refresh_all)

	refresh_all()


func _process(_delta: float) -> void:
	if _tooltip_panel.visible:
		var mp := get_viewport().get_mouse_position()
		var pos := mp + Vector2(18, 14)
		pos.x = minf(pos.x, 1900.0 - _tooltip_panel.size.x)
		pos.y = minf(pos.y, 1066.0 - _tooltip_panel.size.y)
		_tooltip_panel.position = pos

	if GameState.detector_cooldown_remaining > 0.0:
		_scan_btn.text = "Scan (%.0fs)" % ceilf(GameState.detector_cooldown_remaining)


func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventKey and event.pressed and not event.echo:
		if _any_modal_open():
			if event.keycode == KEY_ESCAPE:
				_close_all_modals()
				get_viewport().set_input_as_handled()
			return
		var focus_owner := get_viewport().gui_get_focus_owner()
		if focus_owner is LineEdit:
			if event.keycode == KEY_ESCAPE:
				focus_owner.release_focus()
			return
		match event.keycode:
			KEY_SPACE:
				perform_transmute(false)
				get_viewport().set_input_as_handled()
			KEY_C:
				clear_pedestals()
				get_viewport().set_input_as_handled()
			KEY_J:
				open_modal_by_name("journal")
				get_viewport().set_input_as_handled()
			KEY_U:
				open_modal_by_name("upgrades")
				get_viewport().set_input_as_handled()
			KEY_K:
				open_modal_by_name("challenges")
				get_viewport().set_input_as_handled()
			KEY_A:
				open_modal_by_name("achievements")
				get_viewport().set_input_as_handled()
			KEY_ESCAPE:
				open_modal_by_name("settings")
				get_viewport().set_input_as_handled()


func _build_pedestals() -> void:
	var coords := [
		Vector2(830 - 63, 745 - 98),
		Vector2(1482 - 63, 745 - 98),
		Vector2(1156 - 63, 890 - 84),
	]
	for i in range(3):
		var slot := PedestalSlot.new()
		slot.position = coords[i]
		slot.configure(i, i == 2 and GameState.get_max_slots() < 3)
		slot.slot_changed.connect(_on_pedestal_changed)
		slot.locked_slot_clicked.connect(func() -> void: open_modal_by_name("upgrades"))
		add_child(slot)
		pedestals.append(slot)


func _build_top_hud() -> void:
	var bar := PanelContainer.new()
	bar.position = Vector2(16, 8)
	bar.size = Vector2(1888, 62)
	bar.add_theme_stylebox_override("panel", AssetVault.style_box("ui_panel_ornate", 14, 16, 8))
	add_child(bar)

	var hb := HBoxContainer.new()
	hb.add_theme_constant_override("separation", 14)
	bar.add_child(hb)

	var crest := TextureRect.new()
	crest.texture = AssetVault.tex("res://assets/branding/app_icon_256.png")
	crest.custom_minimum_size = Vector2(42, 42)
	crest.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	hb.add_child(crest)

	var brand := Label.new()
	brand.text = "ALCHEMY CHAOS"
	brand.add_theme_font_override("font", AssetVault.font_display)
	brand.add_theme_font_size_override("font_size", 22)
	brand.add_theme_color_override("font_color", Color("#FFE082"))
	hb.add_child(brand)

	_rank_lbl = Label.new()
	_rank_lbl.add_theme_color_override("font_color", Color("#7AF8D4"))
	_rank_lbl.add_theme_font_size_override("font_size", 14)
	hb.add_child(_rank_lbl)

	var spacer := Control.new()
	spacer.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	hb.add_child(spacer)

	_sub_count_lbl = Label.new()
	_sub_count_lbl.add_theme_color_override("font_color", Color("#F3EAD8"))
	hb.add_child(_sub_count_lbl)

	_rec_count_lbl = Label.new()
	_rec_count_lbl.add_theme_color_override("font_color", Color("#F3EAD8"))
	hb.add_child(_rec_count_lbl)

	var rp_ic := TextureRect.new()
	rp_ic.texture = AssetVault.tex("res://assets/ui/icon_currency_rp.png")
	rp_ic.custom_minimum_size = Vector2(30, 30)
	rp_ic.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	hb.add_child(rp_ic)

	_rp_count_lbl = Label.new()
	_rp_count_lbl.add_theme_font_override("font", AssetVault.font_display)
	_rp_count_lbl.add_theme_font_size_override("font_size", 19)
	_rp_count_lbl.add_theme_color_override("font_color", Color("#E4BEFF"))
	_rp_count_lbl.custom_minimum_size = Vector2(96, 28)
	hb.add_child(_rp_count_lbl)

	var nav_defs := [
		["journal", "Journal [J]", "res://assets/ui/icon_nav_journal.png"],
		["upgrades", "Upgrades [U]", "res://assets/ui/icon_nav_upgrades.png"],
		["challenges", "Challenges [K]", "res://assets/ui/icon_nav_challenges.png"],
		["achievements", "Honors [A]", "res://assets/ui/icon_nav_achievements.png"],
		["settings", "Settings", "res://assets/ui/icon_nav_settings.png"],
	]
	for nd in nav_defs:
		var mname: String = nd[0]
		var btn := Button.new()
		btn.text = nd[1]
		btn.icon = AssetVault.tex(nd[2])
		btn.expand_icon = true
		btn.custom_minimum_size = Vector2(142, 42)
		AssetVault.style_button(btn, false)
		btn.pressed.connect(func() -> void:
			SoundEngine.play_click(1.05)
			open_modal_by_name(mname)
		)
		hb.add_child(btn)


func _build_left_cabinet() -> void:
	var cab := PanelContainer.new()
	cab.position = Vector2(16, 78)
	cab.size = Vector2(382, 988)
	cab.add_theme_stylebox_override("panel", AssetVault.style_box("ui_panel_ornate", 18, 14, 14))
	add_child(cab)

	var vb := VBoxContainer.new()
	vb.add_theme_constant_override("separation", 8)
	cab.add_child(vb)

	var top_r := HBoxContainer.new()
	vb.add_child(top_r)

	var title := Label.new()
	title.text = "REAGENT CABINET"
	title.add_theme_font_override("font", AssetVault.font_display)
	title.add_theme_font_size_override("font_size", 18)
	title.add_theme_color_override("font_color", Color("#FFE082"))
	title.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	top_r.add_child(title)

	_storage_lbl = Label.new()
	_storage_lbl.add_theme_font_size_override("font_size", 12)
	_storage_lbl.add_theme_color_override("font_color", Color("#7AF8D4"))
	top_r.add_child(_storage_lbl)

	var s_row := HBoxContainer.new()
	s_row.add_theme_constant_override("separation", 6)
	vb.add_child(s_row)

	var s_ic := TextureRect.new()
	s_ic.texture = AssetVault.tex("res://assets/ui/icon_search_lens.png")
	s_ic.custom_minimum_size = Vector2(26, 26)
	s_ic.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	s_row.add_child(s_ic)

	_inv_search = LineEdit.new()
	_inv_search.placeholder_text = "Filter reagents..."
	_inv_search.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_inv_search.clear_button_enabled = true
	_inv_search.text_changed.connect(func(t: String) -> void:
		_inv_search_q = t.strip_edges().to_lower()
		_rebuild_inventory_grid()
	)
	s_row.add_child(_inv_search)

	_inv_sort = OptionButton.new()
	_inv_sort.add_item("#")
	_inv_sort.add_item("A-Z")
	_inv_sort.add_item("Type")
	_inv_sort.custom_minimum_size = Vector2(76, 32)
	_inv_sort.item_selected.connect(func(idx: int) -> void:
		_inv_sort_idx = idx
		_rebuild_inventory_grid()
	)
	s_row.add_child(_inv_sort)

	var cat_grid := GridContainer.new()
	cat_grid.columns = 4
	cat_grid.add_theme_constant_override("h_separation", 5)
	cat_grid.add_theme_constant_override("v_separation", 5)
	vb.add_child(cat_grid)

	var cats := [
		["All", "All", ""],
		["Pinned", "Pin", "res://assets/ui/icon_pin_star.png"],
		["Elements", "Elem", "res://assets/categories/cat_elements.png"],
		["Minerals", "Minr", "res://assets/categories/cat_minerals.png"],
		["Plants", "Herb", "res://assets/categories/cat_plants.png"],
		["Metals", "Metl", "res://assets/categories/cat_metals.png"],
		["Synthetic", "Synt", "res://assets/categories/cat_synthetic.png"],
		["Rare", "Rare", "res://assets/categories/cat_rare.png"],
	]
	for cdef in cats:
		var cname: String = cdef[0]
		var b := Button.new()
		b.text = cdef[1]
		if cdef[2] != "":
			b.icon = AssetVault.tex(cdef[2])
			b.expand_icon = true
		b.custom_minimum_size = Vector2(84, 32)
		b.add_theme_font_size_override("font_size", 12)
		b.tooltip_text = "Filter: " + cname
		b.set_meta("cat_key", cname)
		AssetVault.style_button(b, _inv_filter_cat == cname)
		b.pressed.connect(func() -> void:
			SoundEngine.play_click(1.03)
			_inv_filter_cat = cname
			for ch in cat_grid.get_children():
				if ch is Button:
					AssetVault.style_button(ch, ch.get_meta("cat_key") == _inv_filter_cat)
			_rebuild_inventory_grid()
		)
		cat_grid.add_child(b)

	var hint_lbl := Label.new()
	hint_lbl.text = "Left-click or drag to pedestal  |  Right-click to pin"
	hint_lbl.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	hint_lbl.add_theme_font_size_override("font_size", 11)
	hint_lbl.add_theme_color_override("font_color", Color("#ADA0C4"))
	vb.add_child(hint_lbl)

	var scroll := ScrollContainer.new()
	scroll.size_flags_vertical = Control.SIZE_EXPAND_FILL
	scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	vb.add_child(scroll)

	_inv_grid = GridContainer.new()
	_inv_grid.columns = 3
	_inv_grid.add_theme_constant_override("h_separation", 8)
	_inv_grid.add_theme_constant_override("v_separation", 8)
	scroll.add_child(_inv_grid)


func _build_crucible_dock() -> void:
	var dock := PanelContainer.new()
	dock.position = Vector2(706, 954)
	dock.size = Vector2(900, 96)
	dock.add_theme_stylebox_override("panel", AssetVault.style_box("ui_panel_ornate", 16, 16, 12))
	add_child(dock)

	var hb := HBoxContainer.new()
	hb.alignment = BoxContainer.ALIGNMENT_CENTER
	hb.add_theme_constant_override("separation", 10)
	dock.add_child(hb)

	_clear_btn = Button.new()
	_clear_btn.text = "Clear [C]"
	_clear_btn.icon = AssetVault.tex("res://assets/ui/icon_clear.png")
	_clear_btn.expand_icon = true
	_clear_btn.custom_minimum_size = Vector2(124, 52)
	AssetVault.style_button(_clear_btn, false)
	_clear_btn.pressed.connect(clear_pedestals)
	hb.add_child(_clear_btn)

	_chain_btn = Button.new()
	_chain_btn.text = "Feed Result"
	_chain_btn.custom_minimum_size = Vector2(138, 52)
	_chain_btn.disabled = true
	_chain_btn.tooltip_text = "Place the last synthesized substance onto Pedestal 1 for multi-stage chains."
	AssetVault.style_button(_chain_btn, false)
	_chain_btn.pressed.connect(_on_feed_last_result)
	hb.add_child(_chain_btn)

	_transmute_btn = Button.new()
	_transmute_btn.text = "TRANSMUTE [Space]"
	_transmute_btn.icon = AssetVault.tex("res://assets/ui/icon_transmute.png")
	_transmute_btn.expand_icon = true
	_transmute_btn.custom_minimum_size = Vector2(256, 62)
	_transmute_btn.add_theme_font_override("font", AssetVault.font_display)
	_transmute_btn.add_theme_font_size_override("font_size", 18)
	AssetVault.style_button(_transmute_btn, true)
	_transmute_btn.pressed.connect(func() -> void: perform_transmute(false))
	hb.add_child(_transmute_btn)

	_scan_btn = Button.new()
	_scan_btn.text = "Scan Hint"
	_scan_btn.custom_minimum_size = Vector2(158, 52)
	AssetVault.style_button(_scan_btn, false)
	_scan_btn.pressed.connect(_on_resonance_scan)
	hb.add_child(_scan_btn)

	_auto_btn = Button.new()
	_auto_btn.text = "Automaton"
	_auto_btn.custom_minimum_size = Vector2(150, 52)
	AssetVault.style_button(_auto_btn, false)
	_auto_btn.pressed.connect(_on_toggle_automaton)
	hb.add_child(_auto_btn)


func _build_result_ledger() -> void:
	var panel := PanelContainer.new()
	panel.position = Vector2(1560, 766)
	panel.size = Vector2(344, 178)
	panel.add_theme_stylebox_override("panel", AssetVault.style_box("ui_panel_inset", 14, 12, 10))
	add_child(panel)

	var vb := VBoxContainer.new()
	vb.add_theme_constant_override("separation", 6)
	panel.add_child(vb)

	var hdr := Label.new()
	hdr.text = "LATEST SYNTHESIS & HISTORY"
	hdr.add_theme_font_override("font", AssetVault.font_display)
	hdr.add_theme_font_size_override("font_size", 13)
	hdr.add_theme_color_override("font_color", Color("#FFE082"))
	vb.add_child(hdr)

	var top := HBoxContainer.new()
	top.add_theme_constant_override("separation", 10)
	vb.add_child(top)

	_result_icon = TextureRect.new()
	_result_icon.texture = AssetVault.tex("res://assets/ui/icon_transmute.png")
	_result_icon.custom_minimum_size = Vector2(52, 52)
	_result_icon.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	top.add_child(_result_icon)

	var info_v := VBoxContainer.new()
	info_v.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	top.add_child(info_v)

	_result_title = Label.new()
	_result_title.text = "Crucible Ready"
	_result_title.add_theme_font_size_override("font_size", 14)
	_result_title.add_theme_color_override("font_color", Color("#FFF3D1"))
	info_v.add_child(_result_title)

	_result_lore = Label.new()
	_result_lore.text = "Place 2 (or 3) reagents on the brass pedestals and press Transmute."
	_result_lore.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_result_lore.add_theme_font_size_override("font_size", 11)
	_result_lore.add_theme_color_override("font_color", Color("#C2B5DA"))
	info_v.add_child(_result_lore)

	_recent_row = HBoxContainer.new()
	_recent_row.add_theme_constant_override("separation", 6)
	vb.add_child(_recent_row)


func _build_timed_banner() -> void:
	_timed_panel = PanelContainer.new()
	_timed_panel.position = Vector2(906, 78)
	_timed_panel.size = Vector2(500, 56)
	_timed_panel.visible = false
	_timed_panel.add_theme_stylebox_override("panel", AssetVault.style_box("ui_panel_ornate", 14, 16, 10))
	add_child(_timed_panel)

	var hb := HBoxContainer.new()
	hb.alignment = BoxContainer.ALIGNMENT_CENTER
	hb.add_theme_constant_override("separation", 12)
	_timed_panel.add_child(hb)

	_timed_lbl = Label.new()
	_timed_lbl.add_theme_font_override("font", AssetVault.font_display)
	_timed_lbl.add_theme_font_size_override("font_size", 16)
	_timed_lbl.add_theme_color_override("font_color", Color("#FFE082"))
	hb.add_child(_timed_lbl)

	var cancel_b := Button.new()
	cancel_b.text = "Abort"
	AssetVault.style_button(cancel_b, false)
	cancel_b.pressed.connect(func() -> void: GameState.cancel_timed_challenge())
	hb.add_child(cancel_b)


func _build_toast_and_tooltip() -> void:
	_toast_vbox = VBoxContainer.new()
	_toast_vbox.position = Vector2(1468, 82)
	_toast_vbox.size = Vector2(436, 320)
	_toast_vbox.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_toast_vbox.add_theme_constant_override("separation", 8)
	_toast_vbox.z_index = 35
	add_child(_toast_vbox)

	_tooltip_panel = PanelContainer.new()
	_tooltip_panel.visible = false
	_tooltip_panel.custom_minimum_size = Vector2(300, 110)
	_tooltip_panel.size = Vector2(310, 128)
	_tooltip_panel.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_tooltip_panel.z_index = 45
	_tooltip_panel.add_theme_stylebox_override("panel", AssetVault.style_box("ui_panel_ornate", 14, 14, 12))
	add_child(_tooltip_panel)

	var tv := VBoxContainer.new()
	tv.mouse_filter = Control.MOUSE_FILTER_IGNORE
	tv.add_theme_constant_override("separation", 4)
	_tooltip_panel.add_child(tv)

	_tooltip_title = Label.new()
	_tooltip_title.add_theme_font_override("font", AssetVault.font_display)
	_tooltip_title.add_theme_font_size_override("font_size", 16)
	_tooltip_title.add_theme_color_override("font_color", Color("#FFE494"))
	tv.add_child(_tooltip_title)

	_tooltip_meta = Label.new()
	_tooltip_meta.add_theme_font_size_override("font_size", 12)
	_tooltip_meta.add_theme_color_override("font_color", Color("#7AF8D4"))
	tv.add_child(_tooltip_meta)

	_tooltip_desc = Label.new()
	_tooltip_desc.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_tooltip_desc.add_theme_font_size_override("font_size", 12)
	_tooltip_desc.add_theme_color_override("font_color", Color("#E9E0F6"))
	tv.add_child(_tooltip_desc)


func _build_modals() -> void:
	journal_modal = JournalModal.new()
	journal_modal.visible = false
	journal_modal.z_index = 50
	journal_modal.load_recipe_requested.connect(_load_inputs_into_pedestals)
	add_child(journal_modal)

	upgrades_modal = UpgradesModal.new()
	upgrades_modal.visible = false
	upgrades_modal.z_index = 50
	add_child(upgrades_modal)

	challenges_modal = ChallengesModal.new()
	challenges_modal.visible = false
	challenges_modal.z_index = 50
	add_child(challenges_modal)

	achievements_modal = AchievementsModal.new()
	achievements_modal.visible = false
	achievements_modal.z_index = 50
	add_child(achievements_modal)

	settings_modal = SettingsModal.new()
	settings_modal.visible = false
	settings_modal.z_index = 50
	settings_modal.return_to_menu_requested.connect(func() -> void:
		get_tree().change_scene_to_file("res://scenes/MainMenu.tscn")
	)
	settings_modal.progress_reset.connect(func() -> void:
		clear_pedestals()
		refresh_all()
	)
	add_child(settings_modal)


func refresh_all() -> void:
	var has_3 := GameState.get_max_slots() >= 3
	pedestals[2].configure(2, not has_3)
	_refresh_hud_and_buttons()
	_rebuild_inventory_grid()
	_rebuild_recent_history()
	lab_stage.refresh_equipment_visuals()


func _refresh_hud_and_buttons() -> void:
	_rank_lbl.text = "[ %s ]" % GameState.get_current_rank_title()
	_sub_count_lbl.text = "Substances: %d/50" % GameState.discovered_ingredients.size()
	_rec_count_lbl.text = "Formulas: %d/%d" % [GameState.discovered_recipes.size(), AlchemyDB.recipes_ordered.size()]
	_rp_count_lbl.text = "%d RP" % GameState.research_points

	var active := get_slotted_ingredients()
	_transmute_btn.disabled = reaction_director.is_playing or active.size() < 2
	_chain_btn.disabled = _last_result_id.is_empty()

	var det_lvl := GameState.get_upgrade_level("detector")
	if det_lvl <= 0:
		_scan_btn.text = "Detector (Lock)"
		_scan_btn.tooltip_text = "Unlock the Rare Resonance Detector in the Upgrade Shop."
	elif GameState.detector_cooldown_remaining <= 0.0:
		_scan_btn.text = "Resonance Scan"
		_scan_btn.tooltip_text = "Scan with the Resonance Detector lantern to reveal an undiscovered clue."

	var auto_lvl := GameState.get_upgrade_level("automaton")
	if auto_lvl <= 0:
		_auto_btn.text = "Automaton (Lock)"
		_auto_btn.tooltip_text = "Unlock the Clockwork Automaton Arm in the Upgrade Shop."
	else:
		_auto_btn.text = "Auto: ON" if GameState.auto_experiment_enabled else "Auto: PAUSED"
		_auto_btn.tooltip_text = "Toggle autonomous Automaton synthesis cycles."


func _rebuild_inventory_grid() -> void:
	while _inv_grid.get_child_count() > 0:
		var ch := _inv_grid.get_child(0)
		_inv_grid.remove_child(ch)
		ch.queue_free()

	_storage_lbl.text = "Pins: %d/%d  |  Owned: %d" % [
		GameState.pinned_ingredients.size(),
		GameState.get_pin_capacity(),
		GameState.discovered_ingredients.size()
	]

	var slot0_id := pedestals[0].current_ing_id if pedestals.size() > 0 else ""
	var partners := GameState.get_undiscovered_partners_for(slot0_id)

	var items: Array[IngredientData] = []
	for ing in AlchemyDB.ingredients_ordered:
		if not GameState.is_ingredient_discovered(ing.id):
			continue
		if _inv_filter_cat == "Pinned":
			if not GameState.pinned_ingredients.has(ing.id):
				continue
		elif _inv_filter_cat != "All" and ing.category != _inv_filter_cat:
			continue
		if not _inv_search_q.is_empty():
			var hay := (ing.display_name + " " + ing.category + " " + " ".join(ing.properties)).to_lower()
			if hay.find(_inv_search_q) == -1:
				continue
		items.append(ing)

	match _inv_sort_idx:
		0:
			items.sort_custom(func(a: IngredientData, b: IngredientData) -> bool:
				var fa := GameState.pinned_ingredients.has(a.id)
				var fb := GameState.pinned_ingredients.has(b.id)
				return fa if fa != fb else a.num < b.num
			)
		1:
			items.sort_custom(func(a: IngredientData, b: IngredientData) -> bool: return a.display_name < b.display_name)
		2:
			items.sort_custom(func(a: IngredientData, b: IngredientData) -> bool:
				return a.num < b.num if a.category == b.category else a.category < b.category
			)

	for ing in items:
		var card := IngredientCard.new()
		card.setup(ing, partners.has(ing.id), GameState.pinned_ingredients.has(ing.id))
		card.card_clicked.connect(_on_ingredient_clicked_or_dropped)
		card.card_pin_toggled.connect(func(id: String) -> void:
			SoundEngine.play_click(1.02)
			GameState.toggle_pin_ingredient(id)
			_rebuild_inventory_grid()
		)
		card.card_hovered.connect(_show_ing_tooltip)
		card.card_unhovered.connect(func() -> void: _tooltip_panel.visible = false)
		_inv_grid.add_child(card)


func _show_ing_tooltip(ing_id: String, _gpos: Vector2) -> void:
	var ing := AlchemyDB.get_ingredient(ing_id)
	if ing == null:
		return
	_tooltip_title.text = "#%d %s" % [ing.num, ing.display_name]
	_tooltip_meta.text = "%s  |  %s  |  [%s]" % [ing.category, ing.rarity, ", ".join(ing.properties)]
	_tooltip_desc.text = ing.description
	_tooltip_panel.visible = true


func slot_ingredient(ing_id: String) -> bool:
	var max_s := GameState.get_max_slots()
	for i in range(max_s):
		if pedestals[i].current_ing_id.is_empty():
			pedestals[i].set_ingredient(ing_id)
			_on_pedestal_changed(i, ing_id)
			return true
	pedestals[max_s - 1].set_ingredient(ing_id)
	_on_pedestal_changed(max_s - 1, ing_id)
	return true


func _on_ingredient_clicked_or_dropped(ing_id: String) -> void:
	SoundEngine.play_click(1.06)
	slot_ingredient(ing_id)
	if bool(GameState.settings.get("auto_transmute", false)):
		var filled := get_slotted_ingredients()
		if filled.size() == GameState.get_max_slots():
			perform_transmute(false)


func _on_pedestal_changed(_slot_idx: int, _ing_id: String) -> void:
	var active := get_slotted_ingredients()
	lab_stage.update_cauldron_idle_colors(active)
	var slot0 := pedestals[0].current_ing_id if pedestals.size() > 0 else ""
	lab_stage.set_detector_resonance_state(GameState.get_slot1_resonance(slot0))
	if GameState.get_upgrade_level("detector") >= 3:
		_rebuild_inventory_grid()
	_refresh_hud_and_buttons()


func get_slotted_ingredients() -> Array[String]:
	var res: Array[String] = []
	for i in range(GameState.get_max_slots()):
		if not pedestals[i].current_ing_id.is_empty():
			res.append(pedestals[i].current_ing_id)
	return res


func clear_pedestals() -> void:
	SoundEngine.play_click(0.90)
	for p in pedestals:
		p.set_ingredient("")
	_on_pedestal_changed(0, "")


func _load_inputs_into_pedestals(inputs: Array) -> void:
	clear_pedestals()
	for i in range(mini(inputs.size(), GameState.get_max_slots())):
		pedestals[i].set_ingredient(String(inputs[i]))
	_on_pedestal_changed(0, "")


func _on_feed_last_result() -> void:
	if _last_result_id.is_empty():
		return
	SoundEngine.play_click(1.1)
	clear_pedestals()
	pedestals[0].set_ingredient(_last_result_id)
	_on_pedestal_changed(0, _last_result_id)


func perform_transmute(instant_mode: bool = false) -> Dictionary:
	var inputs := get_slotted_ingredients()
	if inputs.size() < 2 or reaction_director.is_playing:
		return {"valid": false, "reason": "need_reagents"}

	var slot_snapshot: Array = [
		pedestals[0].current_ing_id,
		pedestals[1].current_ing_id,
		pedestals[2].current_ing_id if GameState.get_max_slots() >= 3 else ""
	]

	var outcome := GameState.perform_experiment(inputs)
	reaction_director.play_reaction_choreography(slot_snapshot, outcome, instant_mode)

	if bool(outcome.get("success", false)):
		var res_ing: IngredientData = outcome["result_ingredient"]
		var rec: RecipeData = outcome["recipe"]
		_last_result_id = res_ing.id

		_result_icon.texture = AssetVault.tex(res_ing.icon_path)
		var badge := " [NEW!]" if (bool(outcome["is_new_ingredient"]) or bool(outcome["is_new_recipe"])) else ""
		_result_title.text = "%s%s  (+%d RP)" % [res_ing.display_name, badge, int(outcome["rp_gained"])]
		_result_lore.text = rec.lore

		if GameState.can_chain_feed() and bool(outcome["is_new_ingredient"]):
			# Highlight feed button so player sees instant multi-stage chaining
			AssetVault.style_button(_chain_btn, true)
		else:
			AssetVault.style_button(_chain_btn, false)

		_rebuild_inventory_grid()
		_rebuild_recent_history()
		_refresh_hud_and_buttons()
	else:
		SoundEngine.play_fizzle()
		var refund := int(outcome.get("rp_gained", 0))
		var hint := String(outcome.get("hint_message", ""))
		_result_icon.texture = AssetVault.tex("res://assets/ui/icon_clear.png")
		_result_title.text = "Unstable Fizzle" + ((" (+%d RP Salvaged)" % refund) if refund > 0 else "")
		_result_lore.text = hint if not hint.is_empty() else "No stable synthesis formed. Consult the Compendium or Resonance Detector!"
		_refresh_hud_and_buttons()

	return outcome


func _rebuild_recent_history() -> void:
	for ch in _recent_row.get_children():
		ch.queue_free()
	var count := mini(4, GameState.recent_history.size())
	for i in range(count):
		var h: Dictionary = GameState.recent_history[i]
		var res_ing := AlchemyDB.get_ingredient(String(h["result"]))
		if res_ing == null:
			continue
		var b := Button.new()
		b.icon = AssetVault.tex(res_ing.icon_path)
		b.expand_icon = true
		b.text = res_ing.display_name.substr(0, 7)
		b.custom_minimum_size = Vector2(76, 30)
		b.add_theme_font_size_override("font_size", 10)
		b.tooltip_text = "Reload formula for " + res_ing.display_name
		var inps: Array = h["inputs"]
		AssetVault.style_button(b, false)
		b.pressed.connect(func() -> void:
			SoundEngine.play_click(1.04)
			_load_inputs_into_pedestals(inps)
		)
		_recent_row.add_child(b)


func _on_resonance_scan() -> void:
	if GameState.get_upgrade_level("detector") <= 0:
		open_modal_by_name("upgrades")
		return
	var pref := pedestals[0].current_ing_id if pedestals.size() > 0 else ""
	var res := GameState.trigger_detector_scan(pref)
	if bool(res.get("ok", false)):
		SoundEngine.play_discovery(false)
		show_toast("Aetheric Resonance Scan", String(res["message"]), Color("#7AF8D4"))
		var auto_s: Array = res.get("auto_slots", [])
		if not auto_s.is_empty():
			_load_inputs_into_pedestals(auto_s)
	else:
		SoundEngine.play_fizzle()
		show_toast("Resonance Detector", String(res.get("reason", "Unable to scan.")), Color("#FF9E80"))


func _on_toggle_automaton() -> void:
	if GameState.get_upgrade_level("automaton") <= 0:
		open_modal_by_name("upgrades")
		return
	SoundEngine.play_click(1.05)
	GameState.auto_experiment_enabled = not GameState.auto_experiment_enabled
	_refresh_hud_and_buttons()


func _on_automaton_triggered(inputs: Array, outcome: Dictionary) -> void:
	lab_stage.play_automaton_dip()
	reaction_director.play_reaction_choreography(inputs, outcome, true)
	if bool(outcome.get("success", false)):
		var res_ing: IngredientData = outcome["result_ingredient"]
		_last_result_id = res_ing.id
		_result_icon.texture = AssetVault.tex(res_ing.icon_path)
		_result_title.text = "Automaton: %s (+%d RP)" % [res_ing.display_name, int(outcome["rp_gained"])]
		_rebuild_inventory_grid()
		_rebuild_recent_history()
	_refresh_hud_and_buttons()


func _on_lab_hotspot(hname: String) -> void:
	SoundEngine.play_click(1.06)
	match hname:
		"challenges":
			open_modal_by_name("challenges")
		"journal":
			open_modal_by_name("journal")


func _on_toast_requested(kind: String, title_txt: String, subtitle_txt: String, _icon_path: String) -> void:
	var col := Color("#FFE082")
	match kind:
		"rare": col = Color("#E4B0FF")
		"upgrade": col = Color("#7AF8D4")
		"warning": col = Color("#FFAB91")
		"achievement": col = Color("#90CAF9")
	show_toast(title_txt, subtitle_txt, col)


func _on_timed_updated() -> void:
	if GameState.active_timed_id.is_empty():
		_timed_panel.visible = false
		return
	_timed_panel.visible = true
	var ch: Dictionary = AlchemyDB.challenges_by_id.get(GameState.active_timed_id, {})
	var targets: Array = ch.get("timed_targets", [])
	var prog := GameState.active_timed_targets_done.size() if not targets.is_empty() else GameState.active_timed_count
	var goal := targets.size() if not targets.is_empty() else int(ch.get("timed_goal_count", 5))
	_timed_lbl.text = "%s: %d/%d  |  %.1fs" % [String(ch.get("title", "Trial")), prog, goal, GameState.active_timed_remaining]


func show_toast(title_txt: String, body_txt: String, accent: Color) -> void:
	while _toast_vbox.get_child_count() >= 4:
		var old := _toast_vbox.get_child(0)
		_toast_vbox.remove_child(old)
		old.queue_free()

	var card := PanelContainer.new()
	card.mouse_filter = Control.MOUSE_FILTER_IGNORE
	card.add_theme_stylebox_override("panel", AssetVault.style_box("ui_panel_ornate", 12, 14, 10))
	card.modulate.a = 0.0
	_toast_vbox.add_child(card)

	var vb := VBoxContainer.new()
	vb.mouse_filter = Control.MOUSE_FILTER_IGNORE
	card.add_child(vb)

	var t := Label.new()
	t.text = title_txt
	t.add_theme_font_override("font", AssetVault.font_display)
	t.add_theme_font_size_override("font_size", 15)
	t.add_theme_color_override("font_color", accent)
	vb.add_child(t)

	var b := Label.new()
	b.text = body_txt
	b.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	b.add_theme_font_size_override("font_size", 12)
	b.add_theme_color_override("font_color", Color("#F3ECFA"))
	vb.add_child(b)

	var tw := create_tween()
	tw.tween_property(card, "modulate:a", 1.0, 0.2)
	tw.tween_interval(3.4)
	tw.tween_property(card, "modulate:a", 0.0, 0.32)
	tw.tween_callback(card.queue_free)


func open_modal_by_name(mname: String) -> void:
	_close_all_modals()
	_tooltip_panel.visible = false
	match mname:
		"journal":
			journal_modal.open_to_tab("ingredients")
		"recipes":
			journal_modal.open_to_tab("recipes")
		"upgrades":
			upgrades_modal.open_modal()
		"challenges":
			challenges_modal.open_modal()
		"achievements":
			achievements_modal.open_modal()
		"settings":
			settings_modal.open_modal()


func _any_modal_open() -> bool:
	return journal_modal.visible or upgrades_modal.visible or challenges_modal.visible or achievements_modal.visible or settings_modal.visible


func _close_all_modals() -> void:
	journal_modal.visible = false
	upgrades_modal.visible = false
	challenges_modal.visible = false
	achievements_modal.visible = false
	settings_modal.visible = false


class CauldronDropZone extends Control:
	signal ingredient_dropped(ing_id: String)

	func _can_drop_data(_at_position: Vector2, data: Variant) -> bool:
		return typeof(data) == TYPE_DICTIONARY and data.get("type", "") == "ingredient"

	func _drop_data(_at_position: Vector2, data: Variant) -> void:
		var iid := String(data.get("ing_id", ""))
		if not iid.is_empty():
			ingredient_dropped.emit(iid)
