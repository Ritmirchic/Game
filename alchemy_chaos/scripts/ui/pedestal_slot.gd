class_name PedestalSlot
extends PanelContainer

signal slot_changed(slot_index: int, new_ing_id: String)
signal locked_slot_clicked()

var slot_index: int = 0
var is_locked: bool = false
var current_ing_id: String = ""

var _icon_rect: TextureRect
var _title_lbl: Label


func _ready() -> void:
	custom_minimum_size = Vector2(126, 126)
	size = Vector2(126, 126)
	mouse_default_cursor_shape = Control.CURSOR_POINTING_HAND
	_rebuild()


func configure(idx: int, locked: bool) -> void:
	slot_index = idx
	is_locked = locked
	if is_locked:
		current_ing_id = ""
	_rebuild()


func set_ingredient(ing_id: String) -> void:
	if is_locked:
		return
	current_ing_id = ing_id
	_rebuild()


func _rebuild() -> void:
	for ch in get_children():
		ch.queue_free()

	if is_locked:
		add_theme_stylebox_override("panel", AssetVault.style_box("ui_card_locked", 10, 6, 6))
	elif not current_ing_id.is_empty():
		add_theme_stylebox_override("panel", AssetVault.style_box("ui_card_selected", 10, 6, 6))
	else:
		add_theme_stylebox_override("panel", AssetVault.style_box("ui_card_normal", 10, 6, 6))

	var vbox := VBoxContainer.new()
	vbox.alignment = BoxContainer.ALIGNMENT_CENTER
	vbox.add_theme_constant_override("separation", 3)
	vbox.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(vbox)

	_icon_rect = TextureRect.new()
	_icon_rect.custom_minimum_size = Vector2(72, 72)
	_icon_rect.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	_icon_rect.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	_icon_rect.size_flags_horizontal = Control.SIZE_SHRINK_CENTER
	_icon_rect.mouse_filter = Control.MOUSE_FILTER_IGNORE
	vbox.add_child(_icon_rect)

	_title_lbl = Label.new()
	_title_lbl.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_title_lbl.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_title_lbl.add_theme_font_size_override("font_size", 12)
	_title_lbl.mouse_filter = Control.MOUSE_FILTER_IGNORE
	vbox.add_child(_title_lbl)

	if is_locked:
		_icon_rect.texture = AssetVault.tex("res://assets/ui/icon_lock_mystery.png")
		_title_lbl.text = "Catalyst\n(Upgrade)"
		_title_lbl.add_theme_color_override("font_color", Color("#9E92B2"))
		tooltip_text = "3rd Catalyst Pedestal - Unlock 'Tri-Crucible Mixing Manifold' in Upgrades!"
	elif current_ing_id.is_empty():
		_icon_rect.texture = AssetVault.tex("res://assets/ui/icon_transmute.png")
		_icon_rect.modulate = Color(1, 1, 1, 0.30)
		_title_lbl.text = "Catalyst" if slot_index == 2 else ("Reagent %d" % (slot_index + 1))
		_title_lbl.add_theme_color_override("font_color", Color("#B9ADCC"))
		tooltip_text = "Click an ingredient in your cabinet or drag it onto this pedestal."
	else:
		var ing := AlchemyDB.get_ingredient(current_ing_id)
		_icon_rect.texture = AssetVault.tex(ing.icon_path)
		_icon_rect.modulate = Color.WHITE
		_title_lbl.text = ing.display_name
		_title_lbl.add_theme_color_override("font_color", Color("#FFF3D1"))
		tooltip_text = "%s - Click pedestal to clear." % ing.display_name


func _gui_input(event: InputEvent) -> void:
	if event is InputEventMouseButton and event.pressed and event.button_index == MOUSE_BUTTON_LEFT:
		if is_locked:
			SoundEngine.play_click(0.85)
			locked_slot_clicked.emit()
		elif not current_ing_id.is_empty():
			SoundEngine.play_click(0.92)
			current_ing_id = ""
			_rebuild()
			slot_changed.emit(slot_index, "")


func _can_drop_data(_at_position: Vector2, data: Variant) -> bool:
	if is_locked:
		return false
	return typeof(data) == TYPE_DICTIONARY and data.get("type", "") == "ingredient"


func _drop_data(_at_position: Vector2, data: Variant) -> void:
	if is_locked:
		return
	var ing_id := String(data.get("ing_id", ""))
	if not ing_id.is_empty():
		SoundEngine.play_click(1.08)
		current_ing_id = ing_id
		_rebuild()
		slot_changed.emit(slot_index, ing_id)
