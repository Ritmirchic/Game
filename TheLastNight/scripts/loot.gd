extends Node3D
## Searchable supply crate. Loot rolls are server authoritative and replicated.
class_name LootCrate

var data: GameData
var A: AssetLib
var F: ModelFactory
var tier: int
var searched: bool = false
var search_t: float = 0.0
var searcher = null
var label: Label3D

func _init(game: GameData, assets: AssetLib, factory: ModelFactory, idx: int, t: int, pos: Vector3) -> void:
	data = game
	A = assets
	F = factory
	tier = t
	name = "LootCrate_%d" % idx
	position = pos
	add_to_group("interactable")
	var model := F.make_loot_crate(tier)
	add_child(model)
	var body := StaticBody3D.new()
	body.collision_layer = GameData.LAYER_INTERACT
	body.collision_mask = 0
	var cs := CollisionShape3D.new()
	var shape := BoxShape3D.new()
	shape.size = Vector3(0.6, 0.55, 0.6)
	cs.shape = shape
	cs.position.y = 0.28
	body.add_child(cs)
	add_child(body)
	label = Label3D.new()
	label.text = "SUPPLIES"
	label.position = Vector3(0, 0.95, 0)
	label.modulate = Color(1, 0.85, 0.6)
	label.font_size = 28
	label.no_depth_test = false
	label.visible = false
	add_child(label)

func can_interact(player) -> bool:
	return not searched and searcher == null

func get_prompt() -> String:
	return "Search supplies  [hold E 1.2s]"

func interact(player) -> void:
	if searched or searcher != null:
		return
	player.session.loot_interact(self, player)

func interact_authority(player) -> void:
	if searched or searcher != null:
		return
	searcher = player
	search_t = 1.2
	label.visible = true
	label.text = "SEARCHING..."

func _process(delta: float) -> void:
	if multiplayer != null and not multiplayer.is_server():
		return # countdown is authority-only; state arrives via world sync
	if searcher == null or searched:
		return
	# cancel if player walks away
	if is_instance_valid(searcher) and global_position.distance_to(searcher.global_position) > 2.2:
		searcher = null
		search_t = 0.0
		label.visible = false
		label.text = "SUPPLIES"
		return
	search_t -= delta
	if search_t <= 0.0:
		_finish_search()

func _finish_search() -> void:
	searched = true
	var session = searcher.session
	var roll: Dictionary = data.loot_table(tier)[randi() % data.loot_table(tier).size()]
	var amount: int = randi_range(int(roll["amount"][0]), int(roll["amount"][1]))
	match roll["kind"]:
		"cash":
			session.resources["cash"] += amount
			session.notify("Found $%d cash" % amount)
		"materials":
			session.resources["materials"] += amount
			session.notify("Found %d scrap materials" % amount)
		"medkit":
			session.resources["medkits"] += amount
			session.notify("Found a medkit")
		"ammo":
			var ammo_id: String = roll["id"]
			session.resources["ammo"][ammo_id] = int(session.resources["ammo"].get(ammo_id, 0)) + amount
			session.notify("Found %d x %s ammo" % [amount, ammo_id.to_upper()])
	session.audio.play_3d("sfx_pickup", global_position)
	visible = false
	label.visible = false
	set_deferred("process_mode", Node.PROCESS_MODE_DISABLED)

func state_dict() -> Dictionary:
	return {"searched": searched}

func load_state(st: Dictionary) -> void:
	if st.get("searched", false):
		searched = true
		visible = false
