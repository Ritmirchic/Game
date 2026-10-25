extends Node

var categories: Array = []
var reaction_types: Array = []

var ingredients_by_id: Dictionary = {}
var ingredients_ordered: Array[IngredientData] = []

var recipes_by_id: Dictionary = {}
var recipes_by_key: Dictionary = {}
var recipes_ordered: Array[RecipeData] = []

var upgrades: Array = []
var upgrades_by_id: Dictionary = {}

var challenges: Array = []
var challenges_by_id: Dictionary = {}

var achievements: Array = []
var achievements_by_id: Dictionary = {}

var milestones: Array = []


func _ready() -> void:
	load_database()


func load_database() -> void:
	if not ingredients_ordered.is_empty():
		return
	var raw_json := FileAccess.get_file_as_string("res://data/database_manifest.json")
	var manifest: Dictionary = JSON.parse_string(raw_json)
	categories = manifest.get("categories", [])
	reaction_types = manifest.get("reaction_types", [])
	upgrades = manifest.get("upgrades", [])
	for u in upgrades:
		upgrades_by_id[u["id"]] = u
	challenges = manifest.get("challenges", [])
	for c in challenges:
		challenges_by_id[c["id"]] = c
	achievements = manifest.get("achievements", [])
	for a in achievements:
		achievements_by_id[a["id"]] = a
	milestones = manifest.get("milestones", [])

	# Load Custom Resource .tres files for all 50 Ingredients & 120 Recipes
	for item in manifest.get("ingredients", []):
		var res_path := "res://data/ingredients/%02d_%s.tres" % [int(item["num"]), String(item["id"])]
		var ing_res: IngredientData = null
		if ResourceLoader.exists(res_path):
			ing_res = ResourceLoader.load(res_path) as IngredientData
		if ing_res == null:
			ing_res = IngredientData.new()
			ing_res.id = item["id"]
			ing_res.num = int(item["num"])
			ing_res.display_name = item["name"]
			ing_res.category = item["category"]
			ing_res.starter = bool(item["starter"])
			ing_res.rarity = item["rarity"]
			ing_res.primary_color = Color(item["primary"])
			ing_res.secondary_color = Color(item["secondary"])
			var parr := PackedStringArray()
			for p in item["properties"]:
				parr.append(String(p))
			ing_res.properties = parr
			ing_res.rp_value = int(item["rp_value"])
			ing_res.icon_path = "res://assets/ingredients/%s.png" % String(item["id"])
			ing_res.description = item["description"]
		ingredients_by_id[ing_res.id] = ing_res
		ingredients_ordered.append(ing_res)

	for rdict in manifest.get("recipes", []):
		var r_path := "res://data/recipes/%s.tres" % String(rdict["id"])
		var rec_res: RecipeData = null
		if ResourceLoader.exists(r_path):
			rec_res = ResourceLoader.load(r_path) as RecipeData
		if rec_res == null:
			rec_res = RecipeData.new()
			rec_res.id = rdict["id"]
			rec_res.num = int(rdict["num"])
			var iarr := PackedStringArray()
			for inp in rdict["inputs"]:
				iarr.append(String(inp))
			rec_res.inputs = iarr
			rec_res.result = rdict["result"]
			rec_res.reaction_type = rdict["reaction_type"]
			rec_res.is_hidden = bool(rdict["is_hidden"])
			rec_res.is_rare = bool(rdict["is_rare"])
			rec_res.rp_reward = int(rdict["rp_reward"])
			rec_res.lore = rdict["lore"]
		recipes_by_id[rec_res.id] = rec_res
		recipes_by_key[rec_res.get_canonical_key()] = rec_res
		recipes_ordered.append(rec_res)


func canonical_key(inputs: Array) -> String:
	var clean: Array[String] = []
	for item in inputs:
		var s := String(item).strip_edges()
		if not s.is_empty():
			clean.append(s)
	clean.sort()
	return "+".join(clean)


func find_recipe(inputs: Array) -> RecipeData:
	var k := canonical_key(inputs)
	return recipes_by_key.get(k, null)


func get_ingredient(ing_id: String) -> IngredientData:
	return ingredients_by_id.get(ing_id, null)


func get_recipe(rec_id: String) -> RecipeData:
	return recipes_by_id.get(rec_id, null)


func get_upgrade(upg_id: String) -> Dictionary:
	return upgrades_by_id.get(upg_id, {})



func get_recipes_producing(ing_id: String) -> Array[RecipeData]:
	var out: Array[RecipeData] = []
	for r in recipes_ordered:
		if r.result == ing_id:
			out.append(r)
	return out


func get_recipes_using(ing_id: String) -> Array[RecipeData]:
	var out: Array[RecipeData] = []
	for r in recipes_ordered:
		if r.inputs.has(ing_id):
			out.append(r)
	return out
