class_name IngredientCard
extends PanelContainer

signal card_clicked(ing_id: String)
signal card_pin_toggled(ing_id: String)
signal card_hovered(ing_id: String, global_pos: Vector2)
signal card_unhovered()

var ing_data: IngredientData
var is_partner_resonant: bool = false
var is_pinned: bool = false

var _icon_rect: TextureRect
var _name_lbl: Label
var _badge_rect: ColorRect


func setup(data: IngredientData, partner_resonant: bool = false, pinned: bool = false) -> void:
	ing_data = data
	is_partner_resonant = partner_resonant
	is_pinned = pinned
	custom_minimum_size = Vector2(106, 118)
	mouse_default_cursor_shape = Control.CURSOR_POINTING_HAND
	add_theme_stylebox_override("panel", AssetVault.style_box("ui_card_selected" if partner_resonant else "ui_card_normal", 10, 6, 6))

	for ch in get_children():
		ch.queue_free()

	var vbox := VBoxContainer.new()
	vbox.alignment = BoxContainer.ALIGNMENT_CENTER
	vbox.add_theme_constant_override("separation", 3)
	vbox.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(vbox)

	var top_row := HBoxContainer.new()
	top_row.mouse_filter = Control.MOUSE_FILTER_IGNORE
	vbox.add_child(top_row)

	var num_lbl := Label.new()
	num_lbl.text = "#%d" % data.num
	num_lbl.add_theme_font_size_override("font_size", 11)
	num_lbl.add_theme_color_override("font_color", Color("#B9ADCC"))
	num_lbl.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	num_lbl.mouse_filter = Control.MOUSE_FILTER_IGNORE
	top_row.add_child(num_lbl)

	if partner_resonant:
		var res_dot := ColorRect.new()
		res_dot.custom_minimum_size = Vector2(10, 10)
		res_dot.color = Color("#58F6C4")
		res_dot.mouse_filter = Control.MOUSE_FILTER_IGNORE
		top_row.add_child(res_dot)

	if pinned:
		var pin_tag := Label.new()
		pin_tag.text = "PIN"
		pin_tag.add_theme_font_size_override("font_size", 10)
		pin_tag.add_theme_color_override("font_color", Color("#FFD56B"))
		pin_tag.mouse_filter = Control.MOUSE_FILTER_IGNORE
		top_row.add_child(pin_tag)

	_icon_rect = TextureRect.new()
	_icon_rect.texture = AssetVault.tex(data.icon_path)
	_icon_rect.custom_minimum_size = Vector2(62, 62)
	_icon_rect.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	_icon_rect.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	_icon_rect.size_flags_horizontal = Control.SIZE_SHRINK_CENTER
	_icon_rect.mouse_filter = Control.MOUSE_FILTER_IGNORE
	if data.rarity in ["Rare", "Mythic"]:
		var sm := ShaderMaterial.new()
		sm.shader = AssetVault.shader("res://shaders/card_shimmer.gdshader")
		_icon_rect.material = sm
	vbox.add_child(_icon_rect)

	_name_lbl = Label.new()
	_name_lbl.text = data.display_name
	_name_lbl.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_name_lbl.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_name_lbl.custom_minimum_size = Vector2(92, 28)
	_name_lbl.add_theme_font_size_override("font_size", 12)
	var name_col := Color("#FFE082") if data.rarity in ["Rare", "Mythic"] else Color("#F5ECDC")
	_name_lbl.add_theme_color_override("font_color", name_col)
	_name_lbl.mouse_filter = Control.MOUSE_FILTER_IGNORE
	vbox.add_child(_name_lbl)

	mouse_entered.connect(_on_hover_enter)
	mouse_exited.connect(_on_hover_exit)


func _on_hover_enter() -> void:
	add_theme_stylebox_override("panel", AssetVault.style_box("ui_card_hover", 10, 6, 6))
	var tw := create_tween()
	tw.tween_property(self, "scale", Vector2(1.04, 1.04), 0.09)
	if ing_data != null:
		card_hovered.emit(ing_data.id, global_position + Vector2(size.x + 8, 0))


func _on_hover_exit() -> void:
	add_theme_stylebox_override("panel", AssetVault.style_box("ui_card_selected" if is_partner_resonant else "ui_card_normal", 10, 6, 6))
	var tw := create_tween()
	tw.tween_property(self, "scale", Vector2.ONE, 0.09)
	card_unhovered.emit()


func _gui_input(event: InputEvent) -> void:
	if event is InputEventMouseButton and event.pressed:
		var mb := event as InputEventMouseButton
		if mb.button_index == MOUSE_BUTTON_LEFT:
			card_clicked.emit(ing_data.id)
		elif mb.button_index == MOUSE_BUTTON_RIGHT:
			card_pin_toggled.emit(ing_data.id)


func _get_drag_data(_at_position: Vector2) -> Variant:
	if ing_data == null:
		return null
	if get_viewport() != null and get_viewport().gui_is_dragging():
		var preview := Control.new()
		var icon := TextureRect.new()
		icon.texture = AssetVault.tex(ing_data.icon_path)
		icon.custom_minimum_size = Vector2(76, 76)
		icon.size = Vector2(76, 76)
		icon.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
		icon.position = Vector2(-38, -38)
		preview.add_child(icon)
		set_drag_preview(preview)
	return {"type": "ingredient", "ing_id": ing_data.id}
