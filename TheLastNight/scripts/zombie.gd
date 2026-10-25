extends CharacterBody3D
## Zombie actor. AI runs only on the authority (host / offline); remote peers
## render interpolated snapshots. Five base types + mini/final bosses.
class_name Zombie

enum State { RISE, CHASE, ATTACK_DOOR, ATTACK_PLAYER, STAGGER, DEAD }

var data: GameData
var session
var type_id: String
var stats: Dictionary
var rig: Dictionary
var state: int = State.RISE
var net_id: int = -1

var hp: float
var speed: float
var damage: float
var attack_cd_t: float = 0.0
var repath_t: float = 0.0
var path: Array[Vector3] = []
var path_idx: int = 0
var rise_t: float = 0.7
var stagger_t: float = 0.0
var dead_t: float = 0.0
var anim_phase: float = 0.0
var stuck_t: float = 0.0
var last_pos: Vector3
var flash_t: float = 0.0
var scream_t: float = 3.0
var groan_t: float = 2.0
var current_barricade: Barricade = null
var lunge_t: float = 0.0
var boss_phase: int = 0
var ability_t: float = 4.0
var charge_t: float = 0.0
var charge_dir: Vector3 = Vector3.ZERO
var enraged: bool = false

const SERVER_SNAP_RATE := 1.0 / 12.0

func setup(game: GameData, sess, ztype: String, nid: int) -> void:
	data = game
	session = sess
	type_id = ztype
	net_id = nid
	stats = data.ZOMBIES[ztype]
	name = "Zombie_%s_%d" % [ztype, nid]
	collision_layer = GameData.LAYER_ZOMBIE
	collision_mask = GameData.LAYER_WORLD | GameData.LAYER_BARRICADE | GameData.LAYER_PLAYER
	add_to_group("zombie")
	# collision body
	var cs := CollisionShape3D.new()
	var cap := CapsuleShape3D.new()
	cap.radius = float(stats["radius"])
	var crouched: bool = stats.get("crouched", false)
	cap.height = (1.1 if crouched else 1.85) * float(stats["scale"])
	cs.shape = cap
	cs.position.y = cap.height * 0.5
	add_child(cs)
	# rig
	var F: ModelFactory = session.factory
	rig = F.make_zombie(ztype)
	add_child(rig["root"])
	hp = float(stats["hp"])
	speed = float(stats["speed"])
	damage = float(stats["damage"])
	last_pos = global_position
	# rise effect
	var rise_fx := GPUParticles3D.new()
	rise_fx.amount = 18
	rise_fx.lifetime = 0.7
	var pmat := ParticleProcessMaterial.new()
	pmat.gravity = Vector3(0, 1.5, 0)
	pmat.initial_velocity_min = 0.8
	pmat.initial_velocity_max = 2.0
	rise_fx.process_material = pmat
	var em := SphereMesh.new()
	em.radius = 0.05
	em.height = 0.1
	em.material = session.assets.plain("dust", Color(0.6, 0.55, 0.45, 0.5), 0.9, 0.0, Color.BLACK, 1.0, true)
	rise_fx.draw_pass_1 = em
	add_child(rise_fx)
	rise_fx.one_shot = true
	rise_fx.emitting = true

func _ready() -> void:
	last_pos = global_position

func is_server() -> bool:
	return multiplayer == null or multiplayer.is_server()

func _physics_process(delta: float) -> void:
	if state == State.DEAD:
		_handle_dead(delta)
		return
	if not is_server():
		return # remote peers are driven by snapshots
	attack_cd_t -= delta
	flash_t -= delta
	groan_t -= delta
	lunge_t -= delta
	if state == State.RISE:
		rise_t -= delta
		velocity = velocity.move_toward(Vector3.ZERO, 10 * delta)
		if rise_t <= 0.0:
			state = State.CHASE
		move_and_slide()
		return
	if state == State.STAGGER:
		stagger_t -= delta
		velocity = velocity.move_toward(Vector3.ZERO, 12 * delta)
		if stagger_t <= 0.0:
			state = State.CHASE
		move_and_slide()
		return
	if charge_t > 0.0:
		charge_t -= delta
		velocity = charge_dir * speed * 3.2
		move_and_slide()
		if charge_t <= 0.0:
			state = State.CHASE
		return
	_think(delta)
	move_and_slide()
	_stuck_watch(delta)

func _think(delta: float) -> void:
	var target = session.nearest_player(global_position)
	if target == null:
		velocity = velocity.move_toward(Vector3.ZERO, 8 * delta)
		return
	# screamer behavior
	if stats.get("screamer", false):
		scream_t -= delta
		if scream_t <= 0.0:
			scream_t = float(stats["scream_cd"])
			_do_scream()
	# boss ability cycle
	if stats.get("boss", "") != "":
		_boss_think(delta, target)
	# barricade blocking the route?
	var door := _blocking_barricade(target)
	if door != null and global_position.distance_to(door.global_position) <= float(stats["attack_range"]) + 0.9:
		current_barricade = door
		state = State.ATTACK_DOOR
		_attack_barricade(delta)
		return
	current_barricade = null
	var dist: float = global_position.distance_to(target.global_position)
	if dist <= float(stats["attack_range"]) and _has_los(target):
		state = State.ATTACK_PLAYER
		velocity = velocity.move_toward(Vector3.ZERO, 10 * delta)
		_attack_player(delta, target)
		return
	state = State.CHASE
	_navigate(delta, target)

func _navigate(delta: float, target) -> void:
	repath_t -= delta
	if repath_t <= 0.0 or path.is_empty():
		repath_t = 0.45 + randf() * 0.25
		path = session.level.nav.find_path(global_position, target.global_position)
		path_idx = 0
	if path.is_empty():
		# fallback: steer directly
		var dir: Vector3 = target.global_position - global_position
		dir.y = 0
		velocity = dir.normalized() * speed * 0.6
		return
	var goal: Vector3 = path[mini(path_idx, path.size() - 1)]
	while global_position.distance_to(goal) < data.CELL * 0.6 and path_idx < path.size() - 1:
		path_idx += 1
		goal = path[path_idx]
	var move_dir := goal - global_position
	move_dir.y = 0
	# separation from other zombies
	var sep := Vector3.ZERO
	for other in session.zombies:
		if other == self or other.state == State.DEAD:
			continue
		var d: Vector3 = global_position - other.global_position
		d.y = 0
		if d.length() < float(stats["radius"]) + float(other.stats["radius"]) + 0.15:
			sep += d.normalized()
	move_dir = move_dir.normalized() + sep * 0.55
	velocity = move_dir.normalized() * speed
	if move_dir.length_squared() > 0.01:
		var look := Vector3(move_dir.x, 0, move_dir.z)
		rotation.y = lerp_angle(rotation.y, atan2(look.x, look.z), 10 * delta)

func _blocking_barricade(target) -> Barricade:
	var best: Barricade = null
	var best_d := INF
	for b in session.level.barricades.values():
		if b.level == 0 or b.hp <= 0.0:
			continue
		var d: float = global_position.distance_to(b.global_position)
		# only consider doors roughly between us and target
		var to_t: Vector3 = target.global_position - global_position
		var to_b: Vector3 = b.global_position - global_position
		if to_t.length() > 1.0 and to_b.normalized().dot(to_t.normalized()) > 0.2 and d < best_d:
			best_d = d
			best = b
	return best

func _stuck_watch(delta: float) -> void:
	if state != State.CHASE:
		stuck_t = 0.0
		last_pos = global_position
		return
	if global_position.distance_to(last_pos) < 0.12:
		stuck_t += delta
		if stuck_t > 1.4:
			# recover: force fresh path; if still stuck, snap to next nav cell
			repath_t = 0.0
			path = session.level.nav.find_path(global_position, _nearest_player_pos())
			if path.size() > 1:
				var target_cell: Vector3 = path[1]
				if global_position.distance_to(target_cell) < data.CELL * 1.2:
					global_position = target_cell
			stuck_t = 0.0
	else:
		stuck_t = 0.0
	last_pos = global_position

func _nearest_player_pos() -> Vector3:
	var p = session.nearest_player(global_position)
	return p.global_position if p else global_position

func _has_los(target) -> bool:
	var space := get_world_3d().direct_space_state
	var q := PhysicsRayQueryParameters3D.create(
		global_position + Vector3(0, 1.4, 0), target.global_position + Vector3(0, 1.2, 0))
	q.collision_mask = GameData.LAYER_WORLD | GameData.LAYER_BARRICADE
	q.exclude = [self.get_rid()]
	var hit := space.intersect_ray(q)
	return hit.is_empty()

func _attack_barricade(delta: float) -> void:
	if current_barricade == null or current_barricade.hp <= 0.0:
		state = State.CHASE
		return
	var look: Vector3 = current_barricade.global_position - global_position
	rotation.y = lerp_angle(rotation.y, atan2(look.x, look.z), 12 * delta)
	velocity = velocity.move_toward(Vector3.ZERO, 12 * delta)
	if attack_cd_t <= 0.0:
		attack_cd_t = float(stats["attack_cd"])
		lunge_t = 0.25
		current_barricade.take_damage(float(stats["barricade_damage"]))
		# trap triggers when zombie stands at the door
		var trap_dmg: float = current_barricade.trigger_trap()
		if trap_dmg > 0.0:
			take_damage(trap_dmg, global_position, Vector3.ZERO)

func _attack_player(delta: float, target) -> void:
	var look: Vector3 = target.global_position - global_position
	rotation.y = lerp_angle(rotation.y, atan2(look.x, look.z), 14 * delta)
	if attack_cd_t <= 0.0:
		attack_cd_t = float(stats["attack_cd"])
		lunge_t = 0.28
		if _has_los(target):
			target.take_damage(damage, global_position)
			session.audio.play_3d("sfx_hit", global_position)

func _do_scream() -> void:
	session.audio.play_3d("sfx_scream", global_position)
	session.director.request_reinforcement(global_position, 3)

# --------------------------------------------------------------------- bosses
func _boss_think(delta: float, target) -> void:
	ability_t -= delta
	var frac: float = hp / float(stats["hp"])
	var boss_kind: String = stats.get("boss", "")
	if boss_kind == "final":
		if frac < 0.66 and boss_phase == 0:
			boss_phase = 1
			enraged = true
			session.director.request_reinforcement(global_position, 4)
			session.audio.play_3d("sfx_boss_roar", global_position)
		elif frac < 0.33 and boss_phase == 1:
			boss_phase = 2
			speed *= 1.25
			session.director.request_reinforcement(global_position, 5)
			session.audio.play_3d("sfx_boss_roar", global_position)
	if ability_t <= 0.0 and global_position.distance_to(target.global_position) < 22.0:
		var roll: float = randf()
		if roll < 0.45:
			_start_charge(target)
		else:
			_spit_bile(target, 3 if boss_kind == "final" else 1)
		ability_t = (5.0 if boss_kind == "mini" else 4.0) * (0.7 if enraged else 1.0)

func _start_charge(target) -> void:
	charge_t = 1.4
	charge_dir = (target.global_position - global_position)
	charge_dir.y = 0
	charge_dir = charge_dir.normalized()
	state = State.CHASE
	session.audio.play_3d("sfx_brute", global_position)

func _spit_bile(target, count: int) -> void:
	for i in count:
		var base: Vector3 = (target.global_position - global_position).normalized()
		var spread := Vector3(randf_range(-0.2, 0.2), 0, randf_range(-0.2, 0.2))
		session.spawn_bile(global_position + Vector3(0, 1.5, 0), (base + spread).normalized(), damage * 0.6)

# --------------------------------------------------------------- damage/death
func take_damage(amount: float, hit_pos: Vector3, knock: Vector3 = Vector3.ZERO) -> void:
	if state == State.DEAD or not is_server():
		return
	hp -= amount
	flash_t = 0.12
	stagger_t = 0.22 if not stats.get("armored", false) else 0.08
	if state != State.ATTACK_DOOR:
		state = State.STAGGER
	velocity += knock * (0.4 if stats.get("armored", false) else 1.0)
	session.spawn_blood(hit_pos if hit_pos != Vector3.ZERO else global_position + Vector3.UP)
	if hp <= 0.0:
		_die()

func _die() -> void:
	state = State.DEAD
	dead_t = 0.0
	collision_layer = 0
	collision_mask = 0
	session.on_zombie_died(self)
	session.audio.play_3d(stats["death_sound"], global_position)
	session.spawn_blood(global_position + Vector3(0, 1.0, 0), 2)

func _handle_dead(delta: float) -> void:
	dead_t += delta
	rig["root"].rotation.x = lerp(rig["root"].rotation.x, deg_to_rad(-88), delta * 5.0)
	rig["root"].position.y = max(-0.25, rig["root"].position.y - delta * 0.3)
	if dead_t > 6.0:
		session.despawn_zombie(self)

# ------------------------------------------------------------------ animation
func _process(delta: float) -> void:
	if rig.is_empty():
		return
	var moving: float = clamp(velocity.length() / max(speed, 0.01), 0.0, 1.5)
	anim_phase += delta * speed * 2.4 * moving
	var swing: float = sin(anim_phase) * 0.55 * moving
	rig["leg_l"].rotation.x = swing
	rig["leg_r"].rotation.x = -swing
	var arm_swing: float = -swing * 0.7
	if lunge_t > 0.0:
		arm_swing = -1.2 * (lunge_t / 0.28)
	rig["arm_l"].rotation.x = arm_swing - 0.25
	rig["arm_r"].rotation.x = arm_swing - 0.25
	rig["hips"].rotation.z = sin(anim_phase * 0.5) * 0.05 * moving
	rig["head"].rotation.z = sin(anim_phase * 0.33) * 0.08
	# material hit flash
	if flash_t > 0.0:
		rig["eye_mat"].emission_energy_multiplier = 5.0
	else:
		rig["eye_mat"].emission_energy_multiplier = 2.2
	# ambient groans (server driven, replicated via 3D audio on server only in MP)
	if is_server() and groan_t <= 0.0 and state != State.DEAD:
		groan_t = randf_range(4.0, 9.0)
		if randf() < 0.55:
			session.audio.play_3d(stats["sound"], global_position, -6.0)

# --------------------------------------------------------------- net snapshot
func snapshot() -> Dictionary:
	return {
		"id": net_id, "p": [global_position.x, global_position.y, global_position.z],
		"r": rotation.y, "s": state, "ph": anim_phase, "hp": hp,
	}

func apply_snapshot(s: Dictionary) -> void:
	var target := Vector3(s["p"][0], s["p"][1], s["p"][2])
	global_position = global_position.lerp(target, 0.35)
	rotation.y = lerp_angle(rotation.y, float(s["r"]), 0.4)
	anim_phase = float(s["ph"])
	if int(s["s"]) == State.DEAD and state != State.DEAD:
		state = State.DEAD
		collision_layer = 0
		collision_mask = 0
