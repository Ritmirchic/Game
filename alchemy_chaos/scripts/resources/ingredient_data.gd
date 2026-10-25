class_name IngredientData
extends Resource

@export var id: String = ""
@export var num: int = 0
@export var display_name: String = ""
@export var category: String = "Elements"
@export var starter: bool = false
@export var rarity: String = "Common"
@export var primary_color: Color = Color.WHITE
@export var secondary_color: Color = Color.WHITE
@export var properties: PackedStringArray = PackedStringArray()
@export var rp_value: int = 15
@export var icon_path: String = ""
@export_multiline var description: String = ""
