extends Node
## Central data registry: loads all JSON design data and exposes constants/helpers.
class_name GameData

const LAYER_WORLD: int = 1
const LAYER_PLAYER: int = 2
const LAYER_ZOMBIE: int = 4
const LAYER_BARRICADE: int = 8
const LAYER_INTERACT: int = 16

const CELL: float = 2.0
const WALL_H: float = 3.4

var WEAPONS: Dictionary = {}
var ZOMBIES: Dictionary = {}
var NIGHTS: Dictionary = {}
var SHOP: Dictionary = {}
var LAYOUT: Dictionary = {}

static var _instance: GameData

static func instance() -> GameData:
	return _instance

func _ready() -> void:
	_instance = self
	WEAPONS = _load_json("res://data/weapons.json")
	ZOMBIES = _load_json("res://data/zombies.json")
	NIGHTS = _load_json("res://data/nights.json")
	SHOP = _load_json("res://data/shop.json")
	LAYOUT = _load_json("res://data/level_layout.json")

func _load_json(path: String) -> Dictionary:
	var f := FileAccess.open(path, FileAccess.READ)
	assert(f != null, "missing data file: " + path)
	var parsed = JSON.parse_string(f.get_as_text())
	assert(parsed is Dictionary, "bad json: " + path)
	return parsed

# ------------------------------------------------------------------ helpers
func cell_to_world(cx: int, cz: int, y: float = 0.0) -> Vector3:
	return Vector3(cx * CELL + CELL * 0.5, y, cz * CELL + CELL * 0.5)

func world_to_cell(p: Vector3) -> Vector2i:
	return Vector2i(int(floor(p.x / CELL)), int(floor(p.z / CELL)))

func wall_cells() -> Array:
	var out: Array = []
	for w in LAYOUT["walls"]:
		# NOTE: `in`/has() misbehaves on JSON-parsed arrays in this engine
		# build, so gap membership goes through a plain Dictionary.
		var gap_set := {}
		for g in w.get("gaps", []):
			gap_set[int(g)] = true
		for i in range(int(w["from"]), int(w["to"]) + 1):
			if gap_set.has(i):
				continue
			if w["axis"] == "h":
				out.append(Vector2i(i, int(w["fixed"])))
			else:
				out.append(Vector2i(int(w["fixed"]), i))
	return out

func furniture_cells() -> Dictionary: # Vector2i -> furniture dict
	var out := {}
	for f in LAYOUT["furniture"]:
		out[Vector2i(int(f["at"][0]), int(f["at"][1]))] = f
	return out

func blocked_cells() -> Dictionary: # Vector2i -> true
	var out := {}
	for c in wall_cells():
		out[c] = true
	for f in LAYOUT["furniture"]:
		out[Vector2i(int(f["at"][0]), int(f["at"][1]))] = true
	return out

func door_by_id(id: String) -> Dictionary:
	for d in LAYOUT["doors"]:
		if d["id"] == id:
			return d
	return {}

func loot_table(tier: int) -> Array:
	match tier:
		1:
			return [
				{"kind": "ammo", "id": "9mm", "amount": [6, 14]},
				{"kind": "ammo", "id": "shells", "amount": [2, 5]},
				{"kind": "cash", "amount": [15, 45]},
				{"kind": "materials", "amount": [2, 5]},
			]
		2:
			return [
				{"kind": "ammo", "id": "rifle", "amount": [8, 20]},
				{"kind": "ammo", "id": "shells", "amount": [4, 8]},
				{"kind": "medkit", "amount": [1, 1]},
				{"kind": "cash", "amount": [35, 80]},
				{"kind": "materials", "amount": [4, 8]},
			]
		_:
			return [
				{"kind": "ammo", "id": "rifle", "amount": [15, 30]},
				{"kind": "ammo", "id": "bolts", "amount": [2, 5]},
				{"kind": "medkit", "amount": [1, 2]},
				{"kind": "cash", "amount": [70, 140]},
				{"kind": "materials", "amount": [6, 12]},
			]

func night_config(night_id: int) -> Dictionary:
	for n in NIGHTS["nights"]:
		if int(n["id"]) == night_id:
			return n
	return {}
