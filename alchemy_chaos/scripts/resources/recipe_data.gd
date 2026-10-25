class_name RecipeData
extends Resource

@export var id: String = ""
@export var num: int = 0
@export var inputs: PackedStringArray = PackedStringArray()
@export var result: String = ""
@export var reaction_type: String = "BUBBLING"
@export var is_hidden: bool = false
@export var is_rare: bool = false
@export var rp_reward: int = 15
@export_multiline var lore: String = ""

func get_canonical_key() -> String:
	var arr: Array[String] = []
	for item in inputs:
		arr.append(item)
	arr.sort()
	return "+".join(arr)
