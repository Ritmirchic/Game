extends Node
## Session: owns the world, all actors, campaign flow and network replication.
## Works identically offline (single player) and as host/authority in co-op.
class_name Session

const LevelBuilderScript := preload("res://scripts/level_builder.gd")
const ZombieScript := preload("res://scripts/zombie.gd")
const PlayerScript := preload("res://scripts/player.gd")
const BileScript := preload("res://scripts/bile_projectile.gd")
const DirectorScript := preload("res://scripts/wave_director.gd")
const AudioScript := preload("res://scripts/audio_manager.gd")
const SaveScript := preload("res://scripts/save_system.gd")
const NetScript := preload("res://scripts/net/game_net.gd")

var data: GameData
var assets: AssetLib
var factory: ModelFactory
var level: LevelBuilder
var audio: AudioManager
var director: WaveDirector
var ui: CanvasLayer
var net: GameNet
var save: SaveSystem

var actors: Node3D
var zombie_root: Node3D
var effect_root: Node3D

var players: Array[Player] = []
var zombies: Array[Zombie] = []
var next_zombie_id: int = 1
var resources: Dictionary = {}
var phase: String = "prep" # prep | night | over | victory
var night: int = 1
var prep_t: float = 0.0
var paused: bool = false
var net_mode: String = "single" # single | host | client
var stats: Dictionary = {"kills": 0, "damage": 0}
var snap_t: float = 0.0
var world_state_t: float = 1.0
var local_player: Player

func boot(game_data: GameData, ui_man: CanvasLayer, mode: String, ip: String) -> void:
	name = "Session"
	add_to_group("session")
	data = game_data
	ui = ui_man
	net_mode = mode
	assets = AssetLib.new()
	factory = ModelFactory.new(assets)
	# world
	level = LevelBuilderScript.new(data, assets, factory)
	add_child(level)
	level.build()
	actors = Node3D.new()
	actors.name = "Actors"
	level.add_child(actors)
	zombie_root = Node3D.new()
	zombie_root.name = "Zombies"
	level.add_child(zombie_root)
	effect_root = Node3D.new()
	effect_root.name = "Effects"
	level.add_child(effect_root)
	# audio + director + save + net
	audio = AudioScript.new(assets)
	add_child(audio)
	director = DirectorScript.new(self)
	add_child(director)
	save = SaveScript.new()
	net = NetScript.new(self)
	add_child(net)
	if mode != "single":
		net.start(mode, ip)
	else:
		_spawn_local_player()
		new_campaign()

func net_host_ready() -> void:
	_spawn_local_player()
	new_campaign()

func net_client_ready() -> void:
	_spawn_local_player()
	# host will push world state (zombies spawned, resources) via reliable RPCs
	_rpc_request_state.rpc_id(1)

# ------------------------------------------------------------- campaign flow
func new_campaign() -> void:
	night = 1
	var start: Dictionary = data.SHOP["start_loadout"]
	resources = {
		"cash": int(start["cash"]),
		"materials": int(start["materials"]),
		"medkits": int(start["medkits"]),
		"ammo": start["ammo"].duplicate(true),
	}
	stats = {"kills": 0, "damage": 0}
	for p in players:
		_equip_loadout(p, start)
	start_prep()
	save.write_campaign(self)

func _equip_loadout(p: Player, start: Dictionary) -> void:
	p.owned.clear()
	for w in start["weapons"]:
		p.add_weapon(w)
	p.mags.clear()
	for wid in p.owned:
		p._reset_mag(wid)
	p.current = 0
	p.health = p.max_health
	if is_instance_valid(p.current_vm):
		p.current_vm.queue_free()
		p._equip_viewmodel()

func start_prep() -> void:
	phase = "prep"
	var cfg := data.night_config(night)
	prep_t = float(cfg.get("prep_time", 120))
	director.stop()
	# respawn / restore players between nights
	for p in players:
		p.dead = false
		p.downed = false
		p.health = p.max_health
		var sp: Vector3 = level.player_spawn.global_position
		p.global_position = sp + Vector3(randf_range(-0.6, 0.6), 0, randf_range(-0.6, 0.6))
	ui.banner("DAY %d — PREPARE THE DEFENSES" % night)
	notify("Search supplies, repair barricades, then start the night at the terminal.")

func begin_night() -> void:
	if phase != "prep":
		return
	phase = "night"
	director.start_night(night)
	save.write_campaign(self)
	_sync_resources()

func night_complete() -> void:
	var cfg := data.night_config(night)
	resources["cash"] = int(resources["cash"]) + int(cfg["reward_cash"])
	resources["materials"] = int(resources["materials"]) + int(cfg["reward_materials"])
	notify("Night %d survived!  +$%d  +%d materials" % [night, cfg["reward_cash"], cfg["reward_materials"]])
	night += 1
	if night > 10:
		victory()
		return
	save.write_campaign(self)
	start_prep()
	_sync_resources()

func game_over() -> void:
	phase = "over"
	director.stop()
	ui.show_death()

func victory() -> void:
	phase = "victory"
	director.stop()
	save.write_campaign(self)
	ui.show_victory()

# ------------------------------------------------------------------- players
func _spawn_local_player() -> void:
	var pid: int = multiplayer.get_unique_id() if multiplayer else 1
	_spawn_player(pid, true)

func _spawn_player(pid: int, local: bool) -> void:
	for p in players:
		if p.peer_id == pid:
			return
	var p: Player = PlayerScript.new()
	actors.add_child(p)
	p.setup(self, pid, local)
	var sp: Vector3 = level.player_spawn.global_position
	p.global_position = sp + Vector3(players.size() * 0.7, 0, 0)
	players.append(p)
	if local:
		local_player = p
		ui.bind_player(p)
	# restore campaign loadout if rejoining
	if resources.has("ammo"):
		var start: Dictionary = data.SHOP["start_loadout"]
		_equip_loadout(p, start)

func _on_peer_spawned(id: int) -> void:
	if is_authority() and id != multiplayer.get_unique_id():
		_rpc_spawn_player.rpc(id)

@rpc("authority", "call_remote", "reliable")
func _rpc_spawn_player(pid: int) -> void:
	_spawn_player(pid, multiplayer.get_unique_id() == pid)

@rpc("authority", "call_remote", "reliable")
func _rpc_remove_player(pid: int) -> void:
	for p in players:
		if p.peer_id == pid:
			players.erase(p)
			p.queue_free()
			return

func nearest_player(from: Vector3) -> Player:
	var best: Player = null
	var best_d := INF
	for p in players:
		if p.dead:
			continue
		var d: float = p.global_position.distance_squared_to(from)
		if d < best_d:
			best_d = d
			best = p
	return best

func on_player_dead(_p: Player) -> void:
	var alive := players.any(func(p): return not p.dead)
	if not alive:
		game_over()

# ------------------------------------------------------------------- zombies
func is_authority() -> bool:
	return multiplayer == null or multiplayer.is_server()

func is_singleplayer() -> bool:
	return net_mode == "single"

func random_spawn_id() -> String:
	var keys: Array = level.spawn_points.keys()
	return keys[randi() % keys.size()]

func spawn_zombie(type_id: String, point_id: String) -> void:
	if not is_authority():
		return
	var nid := next_zombie_id
	next_zombie_id += 1
	if multiplayer != null:
		_rpc_spawn_zombie.rpc(nid, type_id, point_id)
	_create_zombie(nid, type_id, point_id)

func _create_zombie(nid: int, type_id: String, point_id: String) -> Zombie:
	var z: Zombie = ZombieScript.new()
	zombie_root.add_child(z)
	z.setup(data, self, type_id, nid)
	var marker: Node3D = level.spawn_points[point_id]
	z.global_position = marker.global_position + Vector3(randf_range(-0.7, 0.7), 0, randf_range(-0.7, 0.7))
	zombies.append(z)
	return z

@rpc("authority", "call_remote", "reliable")
func _rpc_spawn_zombie(nid: int, type_id: String, point_id: String) -> void:
	if is_authority():
		return # host already created it locally
	_create_zombie(nid, type_id, point_id)

func despawn_zombie(z: Zombie) -> void:
	if is_authority() and multiplayer != null:
		_rpc_despawn_zombie.rpc(z.net_id)
	zombies.erase(z)
	z.queue_free()

@rpc("authority", "call_remote", "reliable")
func _rpc_despawn_zombie(nid: int) -> void:
	for z in zombies:
		if z.net_id == nid and not is_authority():
			zombies.erase(z)
			z.queue_free()
			return

func alive_zombie_count() -> int:
	var c := 0
	for z in zombies:
		if z.state != Zombie.State.DEAD:
			c += 1
	return c

func on_zombie_died(z: Zombie) -> void:
	stats["kills"] += 1
	var reward: int = int(z.stats["score"])
	resources["cash"] = int(resources["cash"]) + reward
	_sync_resources()

func apply_zombie_damage(z: Zombie, amount: float, pos: Vector3, knock: Vector3 = Vector3.ZERO) -> void:
	if not is_authority():
		_rpc_request_damage.rpc(z.net_id, amount, pos, knock)
		spawn_blood(pos)
		return
	stats["damage"] += amount
	z.take_damage(amount, pos, knock)

@rpc("any_peer", "call_remote", "reliable")
func _rpc_request_damage(nid: int, amount: float, pos: Vector3, knock: Vector3) -> void:
	if not is_authority():
		return
	for z in zombies:
		if z.net_id == nid:
			stats["damage"] += amount
			z.take_damage(amount, pos, knock)
			spawn_blood(pos)
			return

func _process(delta: float) -> void:
	if paused:
		return
	if phase == "prep":
		prep_t -= delta
		if prep_t <= 0.0:
			begin_night()
	# zombie snapshot replication
	if is_authority() and multiplayer != null and zombies.size() > 0:
		snap_t -= delta
		if snap_t <= 0.0:
			snap_t = 0.08
			var snaps: Array = []
			for z in zombies:
				snaps.append(z.snapshot())
			_rpc_zombie_snapshots.rpc(snaps)
	# barricade / loot world-state replication
	if is_authority() and multiplayer != null:
		world_state_t -= delta
		if world_state_t <= 0.0:
			world_state_t = 1.0
			_broadcast_world_state()

# ----------------------------------------------------- interactable net routes
func barricade_interact(b: Barricade, p: Player) -> void:
	if not is_authority():
		_rpc_barricade_interact.rpc(b.door["id"], p.peer_id)
	else:
		b.interact_authority(p)

@rpc("any_peer", "call_remote", "reliable")
func _rpc_barricade_interact(door_id: String, pid: int) -> void:
	if is_authority() and level.barricades.has(door_id):
		level.barricades[door_id].interact_authority(_player_by_id(pid))

func loot_interact(l: LootCrate, p: Player) -> void:
	if not is_authority():
		_rpc_loot_interact.rpc(l.name, p.peer_id)
	else:
		l.interact_authority(p)

@rpc("any_peer", "call_remote", "reliable")
func _rpc_loot_interact(loot_name: String, _pid: int) -> void:
	if not is_authority():
		return
	var n = level.interactables.get_node_or_null(loot_name)
	if n is LootCrate:
		n.interact_authority(local_player)

func _broadcast_world_state() -> void:
	var payload := {"barricades": [], "loots": []}
	for b in level.barricades.values():
		payload["barricades"].append(b.state_dict())
	for n in level.interactables.get_children():
		if n is LootCrate:
			payload["loots"].append({"name": n.name, "searched": n.searched})
	_rpc_world_state.rpc(payload)

@rpc("authority", "call_remote", "reliable")
func _rpc_world_state(payload: Dictionary) -> void:
	if is_authority():
		return
	for st in payload["barricades"]:
		if level.barricades.has(st["door"]):
			level.barricades[st["door"]].load_state(st)
	for st in payload["loots"]:
		var n = level.interactables.get_node_or_null(st["name"])
		if n is LootCrate:
			n.load_state(st)

@rpc("authority", "call_remote", "unreliable")
func _rpc_zombie_snapshots(snaps: Array) -> void:
	if is_authority():
		return
	var by_id := {}
	for s in snaps:
		by_id[int(s["id"])] = s
	for z in zombies:
		if by_id.has(z.net_id):
			z.apply_snapshot(by_id[z.net_id])

# ------------------------------------------------------------------- effects
func spawn_tracer(from: Vector3, to: Vector3) -> void:
	var mi := MeshInstance3D.new()
	var bm := BoxMesh.new()
	bm.size = Vector3(0.025, 0.025, from.distance_to(to))
	bm.material = assets.plain("tracer", Color(1.0, 0.85, 0.5, 0.85), 0.3, 0.0, Color(1, 0.8, 0.4), 2.5, true)
	mi.mesh = bm
	mi.position = (from + to) * 0.5
	mi.look_at(to, Vector3.UP)
	effect_root.add_child(mi)
	var tw := create_tween()
	tw.tween_property(mi, "transparency", 1.0, 0.08)
	tw.tween_callback(mi.queue_free)

func spawn_spark(pos: Vector3) -> void:
	var p := GPUParticles3D.new()
	p.amount = 10
	p.lifetime = 0.35
	p.one_shot = true
	var mat := ParticleProcessMaterial.new()
	mat.gravity = Vector3(0, -6, 0)
	mat.initial_velocity_min = 1.5
	mat.initial_velocity_max = 3.5
	p.process_material = mat
	var em := SphereMesh.new()
	em.radius = 0.03
	em.height = 0.06
	em.material = assets.plain("spark", Color(1, 0.8, 0.5), 0.5, 0.0, Color(1, 0.7, 0.3), 3.0)
	p.draw_pass_1 = em
	p.position = pos
	effect_root.add_child(p)
	p.emitting = true
	get_tree().create_timer(1.0).timeout.connect(p.queue_free)

func spawn_blood(pos: Vector3, count: int = 1) -> void:
	for i in count:
		var p := GPUParticles3D.new()
		p.amount = 8
		p.lifetime = 0.5
		p.one_shot = true
		var mat := ParticleProcessMaterial.new()
		mat.gravity = Vector3(0, -8, 0)
		mat.initial_velocity_min = 1.0
		mat.initial_velocity_max = 2.6
		p.process_material = mat
		var em := SphereMesh.new()
		em.radius = 0.045
		em.height = 0.09
		em.material = assets.plain("blood_p", Color(0.45, 0.03, 0.02), 0.7)
		p.draw_pass_1 = em
		p.position = pos + Vector3(randf_range(-0.15, 0.15), randf_range(-0.1, 0.2), randf_range(-0.15, 0.15))
		effect_root.add_child(p)
		p.emitting = true
		get_tree().create_timer(1.2).timeout.connect(p.queue_free)
	# floor decal
	var decal := MeshInstance3D.new()
	var qm := QuadMesh.new()
	qm.size = Vector2(0.7 + randf() * 0.5, 0.7 + randf() * 0.5)
	qm.material = assets.blood_mat()
	decal.mesh = qm
	decal.position = Vector3(pos.x, 0.02, pos.z)
	decal.rotation.x = -PI / 2
	effect_root.add_child(decal)

func spawn_bile(origin: Vector3, dir: Vector3, dmg: float) -> void:
	if is_authority() and multiplayer != null:
		_rpc_spawn_bile.rpc(origin, dir, dmg)
	var b: BileProjectile = BileScript.new()
	effect_root.add_child(b)
	b.setup(self, origin, dir, dmg)

@rpc("authority", "call_remote", "reliable")
func _rpc_spawn_bile(origin: Vector3, dir: Vector3, dmg: float) -> void:
	if is_authority():
		return
	var b: BileProjectile = BileScript.new()
	effect_root.add_child(b)
	b.setup(self, origin, dir, 0.0) # cosmetic on clients

# --------------------------------------------------------------- resources
func spend_cash(amount: int) -> bool:
	if int(resources["cash"]) < amount:
		notify("Not enough cash")
		return false
	resources["cash"] = int(resources["cash"]) - amount
	_sync_resources()
	return true

@rpc("authority", "call_remote", "reliable")
func _sync_resources() -> void:
	if is_authority() and multiplayer != null:
		_rpc_resources.rpc(resources)

@rpc("authority", "call_remote", "reliable")
func _rpc_resources(r: Dictionary) -> void:
	if not is_authority():
		resources = r

@rpc("any_peer", "call_remote", "reliable")
func _rpc_request_state() -> void:
	if not is_authority():
		return
	_rpc_resources.rpc_id(multiplayer.get_remote_sender_id(), resources)
	for z in zombies:
		_rpc_spawn_zombie.rpc_id(multiplayer.get_remote_sender_id(), z.net_id, z.type_id, _spawn_point_of(z))

func _spawn_point_of(z: Zombie) -> String:
	var best := ""
	var best_d := INF
	for id in level.spawn_points:
		var d: float = level.spawn_points[id].global_position.distance_to(z.global_position)
		if d < best_d:
			best_d = d
			best = id
	return best

func notify(text: String) -> void:
	if ui and ui.has_method("toast"):
		ui.toast(text)

func use_medkit(p: Player) -> void:
	if int(resources.get("medkits", 0)) <= 0 or p.health >= p.max_health:
		return
	if not is_authority():
		_rpc_medkit.rpc(p.peer_id)
		return
	resources["medkits"] = int(resources["medkits"]) - 1
	p.heal(50)
	audio.play_ui("sfx_revive")
	_sync_resources()

@rpc("any_peer", "call_remote", "reliable")
func _rpc_medkit(pid: int) -> void:
	if is_authority():
		use_medkit(_player_by_id(pid))

func _player_by_id(pid: int) -> Player:
	for p in players:
		if p.peer_id == pid:
			return p
	return local_player

func buy_shop_item(item: Dictionary, p: Player) -> void:
	if not is_authority():
		_rpc_buy.rpc(item, p.peer_id)
		return
	if phase != "prep" or not spend_cash(int(item["price"])):
		audio.play_ui("sfx_empty_click")
		return
	var target := _player_by_id(p.peer_id)
	if item.has("give"):
		for k in item["give"]:
			var amount: int = int(item["give"][k])
			if k == "medkits":
				resources["medkits"] = int(resources["medkits"]) + amount
			elif k == "materials":
				resources["materials"] = int(resources["materials"]) + amount
			elif resources["ammo"].has(k):
				resources["ammo"][k] = int(resources["ammo"][k]) + amount
	if item.has("give_weapon") and target:
		target.add_weapon(item["give_weapon"])
		_rpc_weapon.rpc(target.peer_id, item["give_weapon"])
	if item.has("give_upgrade") and target:
		var up: String = item["give_upgrade"]
		if int(target.upgrades.get(up, 0)) < int(item.get("max", 99)):
			target.apply_upgrade(up)
	audio.play_ui("sfx_buy")
	_sync_resources()

@rpc("any_peer", "call_remote", "reliable")
func _rpc_buy(item: Dictionary, pid: int) -> void:
	if is_authority():
		buy_shop_item(item, _player_by_id(pid))

@rpc("authority", "call_remote", "reliable")
func _rpc_weapon(pid: int, wid: String) -> void:
	var p := _player_by_id(pid)
	if p and not p.is_local:
		p.add_weapon(wid)
