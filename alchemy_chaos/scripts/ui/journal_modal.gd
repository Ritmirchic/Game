class_name JournalModal
extends Control

signal load_recipe_requested(inputs: Array)
signal closed()

var current_tab: String = "ingredients" # "ingredients", "recipes", "milestones"
var search_query: String = ""
var selected_category: String = "All"
var recipe_filter: String = "All"
var sort_mode: int = 0
var selected_ing_id: String = "fire"

var _tab_buttons: Dictionary = {}
var _search_edit: LineEdit
var _filter_bar: HBoxContainer
var _sort_opt: OptionButton
var _progress_bar: ProgressBar
var _progress_lbl: Label
var _left_list_vbox: VBoxContainer
var _right_inspector_vbox: VBoxContainer
var _main_panel: PanelContainer


func _ready() -> void:
	set_anchors_preset(Control.PRESET_FULL_RECT)
	_build_modal()
	refresh()


func open_to_tab(tab_name: String = "ingredients") -> void:
	current_tab = tab_name
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
	_main_panel.position = Vector2(140, 68)
	_main_panel.size = Vector2(1640, 944)
	_main_panel.pivot_offset = Vector2(820, 472)
	_main_panel.add_theme_stylebox_override("panel", AssetVault.style_box("ui_panel_ornate", 20, 22, 18))
	add_child(_main_panel)

	var root_v := VBoxContainer.new()
	root_v.add_theme_constant_override("separation", 12)
	_main_panel.add_child(root_v)

	# Header Row
	var hdr := HBoxContainer.new()
	hdr.add_theme_constant_override("separation", 14)
	root_v.add_child(hdr)

	var ic := TextureRect.new()
	ic.texture = AssetVault.tex("res://assets/ui/icon_nav_journal.png")
	ic.custom_minimum_size = Vector2(44, 44)
	ic.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	hdr.add_child(ic)

	var title := Label.new()
	title.text = "GRAND ALCHEMICAL ENCYCLOPEDIA & RESEARCH JOURNAL"
	title.add_theme_font_override("font", AssetVault.font_display)
	title.add_theme_font_size_override("font_size", 26)
	title.add_theme_color_override("font_color", Color("#FFE082"))
	title.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	hdr.add_child(title)

	for tab_spec in [["ingredients", "Substances (50)"], ["recipes", "Recipes (120)"], ["milestones", "Milestones (8)"]]:
		var tkey: String = tab_spec[0]
		var btn := Button.new()
		btn.text = tab_spec[1]
		btn.custom_minimum_size = Vector2(175, 42)
		AssetVault.style_button(btn, false)
		btn.pressed.connect(func() -> void:
			SoundEngine.play_click(1.05)
			current_tab = tkey
			sort_mode = 0
			refresh()
		)
		hdr.add_child(btn)
		_tab_buttons[tkey] = btn

	var close_btn := Button.new()
	close_btn.text = "Close [Esc]"
	close_btn.custom_minimum_size = Vector2(124, 42)
	AssetVault.style_button(close_btn, false)
	close_btn.pressed.connect(close_modal)
	hdr.add_child(close_btn)

	# Progress + Search + Filter + Sort Row
	var ctrl_row := HBoxContainer.new()
	ctrl_row.add_theme_constant_override("separation", 10)
	root_v.add_child(ctrl_row)

	var lens_ic := TextureRect.new()
	lens_ic.texture = AssetVault.tex("res://assets/ui/icon_search_lens.png")
	lens_ic.custom_minimum_size = Vector2(32, 32)
	lens_ic.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	ctrl_row.add_child(lens_ic)

	_search_edit = LineEdit.new()
	_search_edit.placeholder_text = "Search substances, properties, reactions, or lore..."
	_search_edit.custom_minimum_size = Vector2(310, 36)
	_search_edit.clear_button_enabled = true
	_search_edit.text_changed.connect(func(new_t: String) -> void:
		search_query = new_t.strip_edges().to_lower()
		_populate_content()
	)
	ctrl_row.add_child(_search_edit)

	_filter_bar = HBoxContainer.new()
	_filter_bar.add_theme_constant_override("separation", 6)
	_filter_bar.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	ctrl_row.add_child(_filter_bar)

	_sort_opt = OptionButton.new()
	_sort_opt.custom_minimum_size = Vector2(185, 36)
	_sort_opt.item_selected.connect(func(idx: int) -> void:
		sort_mode = idx
		_populate_content()
	)
	ctrl_row.add_child(_sort_opt)

	# Discovery Progress Bar
	var prog_row := HBoxContainer.new()
	prog_row.add_theme_constant_override("separation", 12)
	root_v.add_child(prog_row)

	_progress_lbl = Label.new()
	_progress_lbl.custom_minimum_size = Vector2(340, 26)
	_progress_lbl.add_theme_color_override("font_color", Color("#EED8A1"))
	prog_row.add_child(_progress_lbl)

	_progress_bar = ProgressBar.new()
	_progress_bar.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_progress_bar.custom_minimum_size = Vector2(400, 24)
	_progress_bar.show_percentage = true
	prog_row.add_child(_progress_bar)

	# Body Split: Left Scrollable List + Right Parchment Inspector
	var body := HBoxContainer.new()
	body.size_flags_vertical = Control.SIZE_EXPAND_FILL
	body.add_theme_constant_override("separation", 16)
	root_v.add_child(body)

	var left_panel := PanelContainer.new()
	left_panel.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	left_panel.size_flags_stretch_ratio = 1.25
	left_panel.add_theme_stylebox_override("panel", AssetVault.style_box("ui_panel_inset", 14, 12, 12))
	body.add_child(left_panel)

	var left_scroll := ScrollContainer.new()
	left_scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	left_panel.add_child(left_scroll)

	_left_list_vbox = VBoxContainer.new()
	_left_list_vbox.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_left_list_vbox.add_theme_constant_override("separation", 8)
	left_scroll.add_child(_left_list_vbox)

	var right_panel := PanelContainer.new()
	right_panel.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	right_panel.size_flags_stretch_ratio = 0.95
	right_panel.add_theme_stylebox_override("panel", AssetVault.style_box("ui_panel_parchment", 18, 22, 20))
	body.add_child(right_panel)

	var right_scroll := ScrollContainer.new()
	right_scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	right_panel.add_child(right_scroll)

	_right_inspector_vbox = VBoxContainer.new()
	_right_inspector_vbox.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_right_inspector_vbox.add_theme_constant_override("separation", 10)
	right_scroll.add_child(_right_inspector_vbox)


func refresh() -> void:
	for k in _tab_buttons.keys():
		var btn: Button = _tab_buttons[k]
		AssetVault.style_button(btn, k == current_tab)

	for ch in _filter_bar.get_children():
		ch.queue_free()
	_sort_opt.clear()

	if current_tab == "ingredients":
		_sort_opt.add_item("Sort: Encyclopedia #")
		_sort_opt.add_item("Sort: Name (A-Z)")
		_sort_opt.add_item("Sort: Category")
		_sort_opt.add_item("Sort: Discovered First")
		_sort_opt.select(sort_mode)
		for cat in ["All", "Elements", "Minerals", "Plants", "Metals", "Synthetic", "Rare"]:
			var b := Button.new()
			b.text = cat
			AssetVault.style_button(b, selected_category == cat)
			b.pressed.connect(func() -> void:
				selected_category = cat
				refresh()
			)
			_filter_bar.add_child(b)
		_progress_bar.max_value = AlchemyDB.ingredients_ordered.size()
		_progress_bar.value = GameState.discovered_ingredients.size()
		_progress_lbl.text = "Substances Discovered: %d / %d" % [GameState.discovered_ingredients.size(), AlchemyDB.ingredients_ordered.size()]

	elif current_tab == "recipes":
		_sort_opt.add_item("Sort: Formula #")
		_sort_opt.add_item("Sort: Result Name")
		_sort_opt.add_item("Sort: Reaction Type")
		_sort_opt.select(sort_mode)
		for rf in ["All", "Discovered", "Undiscovered", "Hidden", "Multi-Stage", "Rare"]:
			var b := Button.new()
			b.text = rf
			AssetVault.style_button(b, recipe_filter == rf)
			b.pressed.connect(func() -> void:
				recipe_filter = rf
				refresh()
			)
			_filter_bar.add_child(b)
		_progress_bar.max_value = AlchemyDB.recipes_ordered.size()
		_progress_bar.value = GameState.discovered_recipes.size()
		_progress_lbl.text = "Formulas Discovered: %d / %d" % [GameState.discovered_recipes.size(), AlchemyDB.recipes_ordered.size()]

	else:
		_sort_opt.add_item("Rank Progression")
		_progress_bar.max_value = AlchemyDB.milestones.size()
		_progress_bar.value = GameState.claimed_milestones.size()
		_progress_lbl.text = "Research Ranks Attained: %d / %d  (%s)" % [
			GameState.claimed_milestones.size(),
			AlchemyDB.milestones.size(),
			GameState.get_current_rank_title()
		]

	_populate_content()


func _populate_content() -> void:
	for ch in _left_list_vbox.get_children():
		ch.queue_free()

	if current_tab == "ingredients":
		_populate_ingredients()
		_populate_ingredient_inspector(selected_ing_id)
	elif current_tab == "recipes":
		_populate_recipes()
		_populate_recipe_summary_inspector()
	else:
		_populate_milestones()
		_populate_recipe_summary_inspector()


func _populate_ingredients() -> void:
	var list: Array[IngredientData] = []
	for ing in AlchemyDB.ingredients_ordered:
		if selected_category != "All" and ing.category != selected_category:
			continue
		var known := GameState.is_ingredient_discovered(ing.id)
		if not search_query.is_empty():
			if not known:
				if ing.category.to_lower().find(search_query) == -1:
					continue
			else:
				var hay := (ing.display_name + " " + ing.category + " " + " ".join(ing.properties) + " " + ing.description).to_lower()
				if hay.find(search_query) == -1:
					continue
		list.append(ing)

	match sort_mode:
		1:
			list.sort_custom(func(a: IngredientData, b: IngredientData) -> bool: return a.display_name < b.display_name)
		2:
			list.sort_custom(func(a: IngredientData, b: IngredientData) -> bool:
				return a.num < b.num if a.category == b.category else a.category < b.category
			)
		3:
			list.sort_custom(func(a: IngredientData, b: IngredientData) -> bool:
				var ka := GameState.is_ingredient_discovered(a.id)
				var kb := GameState.is_ingredient_discovered(b.id)
				return ka if ka != kb else a.num < b.num
			)

	var grid := GridContainer.new()
	grid.columns = 4
	grid.add_theme_constant_override("h_separation", 10)
	grid.add_theme_constant_override("v_separation", 10)
	_left_list_vbox.add_child(grid)

	for ing in list:
		var known := GameState.is_ingredient_discovered(ing.id)
		var card := PanelContainer.new()
		card.custom_minimum_size = Vector2(204, 86)
		card.mouse_default_cursor_shape = Control.CURSOR_POINTING_HAND
		var sb_name := "ui_card_selected" if ing.id == selected_ing_id else ("ui_card_normal" if known else "ui_card_locked")
		card.add_theme_stylebox_override("panel", AssetVault.style_box(sb_name, 10, 8, 8))

		var hb := HBoxContainer.new()
		hb.add_theme_constant_override("separation", 8)
		hb.mouse_filter = Control.MOUSE_FILTER_IGNORE
		card.add_child(hb)

		var ic := TextureRect.new()
		ic.texture = AssetVault.tex(ing.icon_path if known else "res://assets/ui/icon_lock_mystery.png")
		ic.custom_minimum_size = Vector2(56, 56)
		ic.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
		ic.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
		ic.mouse_filter = Control.MOUSE_FILTER_IGNORE
		hb.add_child(ic)

		var vb := VBoxContainer.new()
		vb.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		vb.alignment = BoxContainer.ALIGNMENT_CENTER
		vb.mouse_filter = Control.MOUSE_FILTER_IGNORE
		hb.add_child(vb)

		var l1 := Label.new()
		l1.text = ("#%d %s" % [ing.num, ing.display_name]) if known else ("#%d Unknown Substance" % ing.num)
		l1.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
		l1.add_theme_font_size_override("font_size", 13)
		l1.add_theme_color_override("font_color", Color("#FFF3D1") if known else Color("#8D829E"))
		vb.add_child(l1)

		var l2 := Label.new()
		l2.text = "%s | %s" % [ing.category, ing.rarity]
		l2.add_theme_font_size_override("font_size", 11)
		l2.add_theme_color_override("font_color", Color("#C9BBE2") if known else Color("#706680"))
		vb.add_child(l2)

		var iid := ing.id
		card.gui_input.connect(func(ev: InputEvent) -> void:
			if ev is InputEventMouseButton and ev.pressed and ev.button_index == MOUSE_BUTTON_LEFT:
				SoundEngine.play_click(1.04)
				selected_ing_id = iid
				_populate_content()
		)
		grid.add_child(card)


func _populate_ingredient_inspector(ing_id: String) -> void:
	for ch in _right_inspector_vbox.get_children():
		ch.queue_free()

	var ing := AlchemyDB.get_ingredient(ing_id)
	if ing == null:
		return
	var known := GameState.is_ingredient_discovered(ing.id)

	var top := HBoxContainer.new()
	top.add_theme_constant_override("separation", 16)
	_right_inspector_vbox.add_child(top)

	var big_ic := TextureRect.new()
	big_ic.texture = AssetVault.tex(ing.icon_path if known else "res://assets/ui/icon_lock_mystery.png")
	big_ic.custom_minimum_size = Vector2(118, 118)
	big_ic.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	top.add_child(big_ic)

	var head_v := VBoxContainer.new()
	head_v.alignment = BoxContainer.ALIGNMENT_CENTER
	head_v.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	top.add_child(head_v)

	var nm := Label.new()
	nm.text = ("#%d - %s" % [ing.num, ing.display_name]) if known else ("#%d - Undiscovered Substance" % ing.num)
	nm.add_theme_font_override("font", AssetVault.font_display)
	nm.add_theme_font_size_override("font_size", 24)
	nm.add_theme_color_override("font_color", Color("#3B2312"))
	nm.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	head_v.add_child(nm)

	var sub := Label.new()
	sub.text = "Category: %s   |   Rarity: %s" % [ing.category, ing.rarity]
	sub.add_theme_color_override("font_color", Color("#644226"))
	head_v.add_child(sub)

	var desc := Label.new()
	desc.text = ing.description if known else "This substance remains veiled in alchemical mystery. Experiment in the Crucible or upgrade your Hermetic Spectroscope to uncover new syntheses."
	desc.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	desc.add_theme_font_override("font", AssetVault.font_italic)
	desc.add_theme_font_size_override("font_size", 20)
	desc.add_theme_color_override("font_color", Color("#2E1B0E"))
	_right_inspector_vbox.add_child(desc)

	if known:
		var prop_lbl := Label.new()
		prop_lbl.text = "Alchemical Properties:  [ " + " ]  [ ".join(ing.properties) + " ]"
		prop_lbl.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
		prop_lbl.add_theme_color_override("font_color", Color("#5B3418"))
		_right_inspector_vbox.add_child(prop_lbl)

	var prod_hdr := Label.new()
	prod_hdr.text = "Known Synthesis Pathways:"
	prod_hdr.add_theme_font_override("font", AssetVault.font_display)
	prod_hdr.add_theme_font_size_override("font_size", 18)
	prod_hdr.add_theme_color_override("font_color", Color("#4A2912"))
	_right_inspector_vbox.add_child(prod_hdr)

	var prod_recs := AlchemyDB.get_recipes_producing(ing.id)
	if ing.starter:
		var st_lbl := Label.new()
		st_lbl.text = "* Primal Starter Element - Innately stocked in your Sanctum."
		st_lbl.add_theme_color_override("font_color", Color("#3B2312"))
		_right_inspector_vbox.add_child(st_lbl)
	else:
		for r in prod_recs:
			var row_l := Label.new()
			if GameState.is_recipe_discovered(r.id):
				var parts: Array[String] = []
				for inp in r.inputs:
					parts.append(AlchemyDB.get_ingredient(inp).display_name)
				row_l.text = "* %s  ->  %s (%s)" % [" + ".join(parts), ing.display_name, r.reaction_type]
			else:
				var tag := "[HIDDEN MYSTERY] " if r.is_hidden else ""
				row_l.text = "* %sUndiscovered %d-Reagent Synthesis" % [tag, r.inputs.size()]
			row_l.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
			row_l.add_theme_color_override("font_color", Color("#3B2312"))
			_right_inspector_vbox.add_child(row_l)


func _populate_recipes() -> void:
	var spec_lvl := GameState.get_spectroscope_level()
	var list: Array[RecipeData] = []
	for rec in AlchemyDB.recipes_ordered:
		var known := GameState.is_recipe_discovered(rec.id)
		match recipe_filter:
			"Discovered":
				if not known: continue
			"Undiscovered":
				if known: continue
			"Hidden":
				if not rec.is_hidden: continue
			"Multi-Stage":
				if rec.inputs.size() < 3: continue
			"Rare":
				if not rec.is_rare: continue

		if not search_query.is_empty():
			var out_ing := AlchemyDB.get_ingredient(rec.result)
			var hay := rec.reaction_type.to_lower()
			if known:
				hay += " " + out_ing.display_name.to_lower() + " " + rec.lore.to_lower()
				for inp in rec.inputs:
					hay += " " + AlchemyDB.get_ingredient(inp).display_name.to_lower()
			elif spec_lvl >= 1:
				hay += " " + out_ing.category.to_lower()
			if hay.find(search_query) == -1:
				continue
		list.append(rec)

	match sort_mode:
		1:
			list.sort_custom(func(a: RecipeData, b: RecipeData) -> bool:
				return AlchemyDB.get_ingredient(a.result).display_name < AlchemyDB.get_ingredient(b.result).display_name
			)
		2:
			list.sort_custom(func(a: RecipeData, b: RecipeData) -> bool: return a.reaction_type < b.reaction_type)

	for rec in list:
		var known := GameState.is_recipe_discovered(rec.id)
		var out_ing := AlchemyDB.get_ingredient(rec.result)
		var row := PanelContainer.new()
		row.add_theme_stylebox_override("panel", AssetVault.style_box("ui_card_normal" if known else "ui_card_locked", 10, 12, 8))

		var hb := HBoxContainer.new()
		hb.add_theme_constant_override("separation", 12)
		row.add_child(hb)

		var ic := TextureRect.new()
		ic.texture = AssetVault.tex(out_ing.icon_path if known else "res://assets/ui/icon_lock_mystery.png")
		ic.custom_minimum_size = Vector2(46, 46)
		ic.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
		hb.add_child(ic)

		var vb := VBoxContainer.new()
		vb.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		hb.add_child(vb)

		var formula_txt := ""
		if known:
			var pnames: Array[String] = []
			for inp in rec.inputs:
				pnames.append(AlchemyDB.get_ingredient(inp).display_name)
			formula_txt = "#%03d:  %s  =  %s" % [rec.num, " + ".join(pnames), out_ing.display_name]
		else:
			var prefix := "[HIDDEN] " if rec.is_hidden else ""
			var first_clue := "???"
			if (spec_lvl >= 2 and not rec.is_hidden) or spec_lvl >= 3:
				if GameState.is_ingredient_discovered(rec.inputs[0]):
					first_clue = AlchemyDB.get_ingredient(rec.inputs[0]).display_name
			var slots_str := (first_clue + " + ???") if rec.inputs.size() == 2 else (first_clue + " + ??? + ???")
			var res_clue := ("[" + out_ing.category + " Substance]") if spec_lvl >= 1 else "???"
			formula_txt = "#%03d:  %s%s  =  %s" % [rec.num, prefix, slots_str, res_clue]

		var l1 := Label.new()
		l1.text = formula_txt
		l1.add_theme_font_size_override("font_size", 14)
		l1.add_theme_color_override("font_color", Color("#FFE8B0") if known else Color("#9E92B2"))
		vb.add_child(l1)

		var l2 := Label.new()
		if known:
			l2.text = "%s  |  %s" % [rec.reaction_type, rec.lore]
		elif spec_lvl >= 1:
			l2.text = "Spectroscope Reading: %s Reaction" % rec.reaction_type
		else:
			l2.text = "Undiscovered Formula - Experiment to reveal"
		l2.add_theme_font_size_override("font_size", 12)
		l2.add_theme_color_override("font_color", Color("#C9BBE2") if known else Color("#786E8C"))
		l2.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
		vb.add_child(l2)

		if known:
			var load_btn := Button.new()
			load_btn.text = "Load into Crucible"
			load_btn.disabled = (rec.inputs.size() > GameState.get_max_slots())
			AssetVault.style_button(load_btn, false)
			var r_inputs := Array(rec.inputs)
			load_btn.pressed.connect(func() -> void:
				SoundEngine.play_click(1.1)
				load_recipe_requested.emit(r_inputs)
				close_modal()
			)
			hb.add_child(load_btn)

		_left_list_vbox.add_child(row)


func _populate_milestones() -> void:
	for ms in AlchemyDB.milestones:
		var done := GameState.claimed_milestones.has(String(ms["id"]))
		var row := PanelContainer.new()
		row.add_theme_stylebox_override("panel", AssetVault.style_box("ui_card_selected" if done else "ui_card_normal", 10, 14, 10))
		var hb := HBoxContainer.new()
		hb.add_theme_constant_override("separation", 14)
		row.add_child(hb)

		var ic := TextureRect.new()
		ic.texture = AssetVault.tex("res://assets/ui/icon_nav_achievements.png" if done else "res://assets/ui/icon_lock_mystery.png")
		ic.custom_minimum_size = Vector2(48, 48)
		ic.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
		hb.add_child(ic)

		var vb := VBoxContainer.new()
		vb.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		hb.add_child(vb)

		var t := Label.new()
		t.text = "%s   %s" % [String(ms["title"]), "[ATTAINED]" if done else "[IN PROGRESS]"]
		t.add_theme_font_override("font", AssetVault.font_display)
		t.add_theme_font_size_override("font_size", 18)
		t.add_theme_color_override("font_color", Color("#FFE082") if done else Color("#E5DCF2"))
		vb.add_child(t)

		var req := Label.new()
		req.text = "Requires %d Substances (%d/%d) & %d Recipes (%d/%d)   |   Bonus: +%d RP" % [
			int(ms["req_ingredients"]), GameState.discovered_ingredients.size(), int(ms["req_ingredients"]),
			int(ms["req_recipes"]), GameState.discovered_recipes.size(), int(ms["req_recipes"]),
			int(ms["rp_bonus"])
		]
		req.add_theme_color_override("font_color", Color("#C9BBE2"))
		vb.add_child(req)

		_left_list_vbox.add_child(row)


func _populate_recipe_summary_inspector() -> void:
	for ch in _right_inspector_vbox.get_children():
		ch.queue_free()

	var hdr := Label.new()
	hdr.text = "Sanctum Research Ledger"
	hdr.add_theme_font_override("font", AssetVault.font_display)
	hdr.add_theme_font_size_override("font_size", 24)
	hdr.add_theme_color_override("font_color", Color("#3B2312"))
	_right_inspector_vbox.add_child(hdr)

	var hidden_total := 0
	var hidden_done := 0
	var rare_total := 0
	var rare_done := 0
	for r in AlchemyDB.recipes_ordered:
		if r.is_hidden:
			hidden_total += 1
			if GameState.is_recipe_discovered(r.id):
				hidden_done += 1
		if r.is_rare:
			rare_total += 1
			if GameState.is_recipe_discovered(r.id):
				rare_done += 1

	var stats_lines := [
		"Current Guild Rank: %s" % GameState.get_current_rank_title(),
		"Total Crucible Experiments: %d" % GameState.total_experiments,
		"Substances Cataloged: %d / 50" % GameState.discovered_ingredients.size(),
		"Valid Formulas Recorded: %d / %d" % [GameState.discovered_recipes.size(), AlchemyDB.recipes_ordered.size()],
		"Hidden Mysteries Uncovered: %d / %d" % [hidden_done, hidden_total],
		"Rare Transmutations Mastered: %d / %d" % [rare_done, rare_total],
		"Reaction Phenomena Witnessed: %d / 10" % GameState.witnessed_reactions.size(),
	]
	for line in stats_lines:
		var lbl := Label.new()
		lbl.text = "* " + line
		lbl.add_theme_color_override("font_color", Color("#3B2312"))
		lbl.add_theme_font_size_override("font_size", 16)
		_right_inspector_vbox.add_child(lbl)

	var tip := Label.new()
	tip.text = "\nScholar's Note: Upgrade the Hermetic Spectroscope in the Upgrade Shop to automatically decode partial clues for undiscovered and Hidden formulas right inside this Compendium!"
	tip.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	tip.add_theme_font_override("font", AssetVault.font_italic)
	tip.add_theme_font_size_override("font_size", 19)
	tip.add_theme_color_override("font_color", Color("#5B3418"))
	_right_inspector_vbox.add_child(tip)
