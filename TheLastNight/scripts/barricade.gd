extends Node3D
## Buildable / repairable / upgradeable door defense. Server authoritative;
## visual and nav state synced through reliable RPCs.
class_name Barricade

var data: GameData
var A: AssetLib
var F: ModelFactory
var door: Dictionary

var level: int = 0 # 0 none, 1 wood, 2 reinforced, 3 metal
var hp: float = 0
var max_hp: float = 1
var trap_uses: int = 0
var visual: Node3D
var body: StaticBody3D
var collision: CollisionShape3D
var span: float
var horizontal: bool
var flash_t: float = 0.0

signal changed

func _init(game: GameData, assets: AssetLib, factory: ModelFactory, door_dict: Dictionary) -> void:
	data = game
	A = assets
	F = factory
	door = door_dict
	name = "Barricade_" + door["id"]
	add_to_group("interactable")
	add_to_group("barricade")
	var cells: Array = door["cells"]
	span = cells.size() * data.CELL
	var mid: Array = cells[cells.size() / 2]
	var first: Vector3 = data.cell_to_world(int(cells[0][0]), int(cells[0][1]))
	var last: Vector3 = data.cell_to_world(int(cells[-1][0]), int(cells[-1][1]))
	horizontal = abs(first.z - last.z) < 0.01
	body = StaticBody3D.new()
	body.collision_layer = GameData.LAYER_BARRICADE
	body.collision_mask = GameData.LAYER_WORLD
	add_child(body)
	collision = CollisionShape3D.new()
	var shape := BoxShape3D.new()
	shape.size = Vector3(span, 2.6, 0.3) if horizontal else Vector3(0.3, 2.6, span)
	collision.shape = shape
	collision.position.y = 1.3
	body.add_child(collision)
	body.collision_layer = 0 # unbuilt doors stay open

func _process(delta: float) -> void:
	if flash_t > 0.0:
		flash_t -= delta

# ------------------------------------------------------------ player facing
func can_interact(_player) -> bool:
	return true

func get_prompt() -> String:
	var b: Dictionary = data.SHOP["barricades"]
	if level == 0:
		return "Build %s  [%d materials]" % [b["wood"]["name"], b["wood"]["build_materials"]]
	if hp < max_hp:
		return "Repair %s  [%d materials]" % [b[["", "wood", "reinforced", "metal"][level]]["name"], _repair_cost()]
	if level == 1:
		return "Upgrade to %s  [%d$ / %d mats]" % [b["reinforced"]["name"], b["reinforced"]["upgrade_cash"], b["reinforced"]["upgrade_materials"]]
	if level == 2:
		return "Upgrade to %s  [%d$ / %d mats]" % [b["metal"]["name"], b["metal"]["upgrade_cash"], b["metal"]["upgrade_materials"]]
	if trap_uses == 0:
		return "Place %s  [%d$]" % [b["trap"]["name"], b["trap"]["price"]]
	return "Defense ready  (trap %d uses)" % trap_uses

func interact(player) -> void:
	var session = player.session
	session.barricade_interact(self, player)

func interact_authority(player) -> void:
	var session = player.session
	if level == 0:
		_try_build(session)
	elif hp < max_hp:
		_try_repair(session)
	elif level < 3:
		_try_upgrade(session)
	elif trap_uses == 0:
		if session.spend_cash(int(data.SHOP["barricades"]["trap"]["price"])):
			trap_uses = int(data.SHOP["barricades"]["trap"]["uses"])
			session.audio.play_ui("sfx_buy")
			_apply()

func _try_build(session) -> void:
	var cost: int = int(data.SHOP["barricades"]["wood"]["build_materials"])
	if session.resources["materials"] >= cost:
		session.resources["materials"] -= cost
		level = 1
		max_hp = float(data.SHOP["barricades"]["wood"]["hp"])
		hp = max_hp
		session.audio.play_ui("sfx_buy")
		_apply()

func _try_repair(session) -> void:
	var cost: int = _repair_cost()
	if session.resources["materials"] >= cost:
		session.resources["materials"] -= cost
		hp = max_hp
		session.audio.play_ui("sfx_buy")
		_apply()

func _repair_cost() -> int:
	return int(data.SHOP["barricades"][["", "wood", "reinforced", "metal"][level]]["repair_materials"])

func _try_upgrade(session) -> void:
	var key := "reinforced" if level == 1 else "metal"
	var def: Dictionary = data.SHOP["barricades"][key]
	if session.resources["cash"] >= int(def["upgrade_cash"]) and session.resources["materials"] >= int(def["upgrade_materials"]):
		session.resources["cash"] -= int(def["upgrade_cash"])
		session.resources["materials"] -= int(def["upgrade_materials"])
		level += 1
		max_hp = float(def["hp"])
		hp = max_hp
		session.audio.play_ui("sfx_buy")
		_apply()

# --------------------------------------------------------------- simulation
func take_damage(amount: float) -> void:
	if level == 0 or hp <= 0.0:
		return
	hp = max(0.0, hp - amount)
	flash_t = 0.12
	if level == 3:
		session_play_sound("sfx_metal_hit")
	else:
		session_play_sound("sfx_wood_hit")
	if hp <= 0.0:
		session_play_sound("sfx_wood_break")
	_apply()

func session_play_sound(snd: String) -> void:
	var tree := get_tree()
	if tree and tree.has_group("session"):
		tree.get_first_node_in_group("session").audio.play_3d(snd, global_position)

func _apply() -> void:
	if visual != null:
		visual.queue_free()
	var ratio: float = hp / max_hp if max_hp > 0 else 0.0
	visual = F.make_barricade(level, ratio)
	add_child(visual)
	var active: bool = level > 0 and hp > 0.0
	body.collision_layer = GameData.LAYER_BARRICADE if active else 0
	var cells: Array = door["cells"]
	if get_parent() is LevelBuilder:
		(get_parent() as LevelBuilder).nav.set_door_closed(cells, active)
	changed.emit()

func trigger_trap() -> float:
	if trap_uses <= 0:
		return 0.0
	trap_uses -= 1
	return float(data.SHOP["barricades"]["trap"]["damage"])

func state_dict() -> Dictionary:
	return {"level": level, "hp": hp, "max_hp": max_hp, "trap": trap_uses, "door": door["id"]}

func load_state(st: Dictionary) -> void:
	level = int(st["level"])
	max_hp = float(st["max_hp"])
	hp = float(st["hp"])
	trap_uses = int(st["trap"])
	_apply()
