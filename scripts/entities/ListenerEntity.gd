class_name ListenerEntity
extends CharacterBody3D
## THE LISTENER: Blind acoustic horror entity that tracks player-generated decibels
## via SoundEventBus, accounting for distance attenuation and wall raycast occlusion.

enum State { PATROL, INVESTIGATE, HUNT }

var is_active_threat: bool = false
var current_state: State = State.PATROL
var last_heard_pos: Vector3 = Vector3.ZERO
var last_perceived_db: float = 0.0

var _complex: ApartmentComplex
var _player: Node3D
var _model: Node3D
var _anim: AnimationPlayer
var _path: Array[Vector3] = []
var _state_timer: float = 0.0
var _click_timer: float = 2.0
var _attack_cooldown: float = 0.0
var _patrol_nodes: Array[String] = ["cross_w", "cross_c", "cross_e1", "spine_n1", "spine_m"]
var _patrol_idx: int = 0


func setup(complex_ref: ApartmentComplex, player_ref: Node3D) -> void:
	_complex = complex_ref
	_player = player_ref

	var col := CollisionShape3D.new()
	var cap := CapsuleShape3D.new()
	cap.radius = 0.34
	cap.height = 1.95
	col.shape = cap
	col.position = Vector3(0, 0.98, 0)
	add_child(col)

	_model = MaterialLibrary.instantiate_model("listener_entity.glb")
	add_child(_model)
	_anim = _model.find_child("AnimationPlayer", true, false) as AnimationPlayer

	SoundEventBus.noise_emitted.connect(_on_noise_emitted)
	set_threat_active(false, Vector3(0, -30, 0))


func set_threat_active(active: bool, spawn_pos: Vector3, min_safe_dist: float = 10.0) -> void:
	is_active_threat = active
	visible = active
	if not active:
		global_position = Vector3(0, -30, 0)
		velocity = Vector3.ZERO
		return

	var chosen := spawn_pos
	if _player != null and chosen.distance_to(_player.global_position) < min_safe_dist and _complex != null:
		for k in _patrol_nodes:
			var p: Vector3 = _complex.nav_waypoints[k]["pos"]
			if p.distance_to(_player.global_position) >= min_safe_dist:
				chosen = p
				break
	global_position = chosen
	current_state = State.PATROL
	_path.clear()
	_play_anim("IdleListen", true)


func compute_perceived_decibels(sound_pos: Vector3, raw_db: float) -> float:
	var dist := maxf(1.0, global_position.distance_to(sound_pos))
	# Inverse-square acoustic falloff + solid wall attenuation check
	var atten_db := raw_db - 20.0 * (log(dist) / log(10.0))
	if absf(sound_pos.y - global_position.y) > 2.2:
		atten_db -= 18.0 # Separate floor slab muffling

	if is_inside_tree():
		var space := get_world_3d().direct_space_state
		var q := PhysicsRayQueryParameters3D.create(global_position + Vector3(0, 1.65, 0), sound_pos + Vector3(0, 1.0, 0))
		q.exclude = [get_rid()]
		if _player is CollisionObject3D:
			q.exclude.append((_player as CollisionObject3D).get_rid())
		var hit := space.intersect_ray(q)
		if not hit.is_empty():
			atten_db -= 15.0 # Wall/closed door acoustic occlusion!
	return atten_db


func _on_noise_emitted(world_pos: Vector3, decibels: float, _tag: String) -> void:
	if not is_active_threat:
		return
	var perceived := compute_perceived_decibels(world_pos, decibels)
	last_perceived_db = perceived
	if perceived >= 45.0:
		last_heard_pos = world_pos
		if current_state != State.HUNT:
			AudioDirector.play_3d("entity_listener_shriek", global_position + Vector3(0, 1.7, 0), 22.0, 0.0)
		current_state = State.HUNT
		_state_timer = 7.5
		if _complex != null:
			_path = _complex.find_nav_path(global_position, last_heard_pos)
	elif perceived >= 24.0 and current_state != State.HUNT:
		last_heard_pos = world_pos
		current_state = State.INVESTIGATE
		_state_timer = 6.0
		if _complex != null:
			_path = _complex.find_nav_path(global_position, last_heard_pos)


func _physics_process(delta: float) -> void:
	if not is_active_threat:
		return

	_attack_cooldown = maxf(0.0, _attack_cooldown - delta)
	_click_timer -= delta
	if _click_timer <= 0.0:
		_click_timer = randf_range(3.2, 5.4)
		AudioDirector.play_3d("entity_listener_clicks", global_position + Vector3(0, 1.6, 0), 15.0, -4.0)

	var speed := 1.75
	match current_state:
		State.PATROL:
			speed = 1.65
			if _path.is_empty() and _complex != null:
				_patrol_idx = (_patrol_idx + 1) % _patrol_nodes.size()
				var wp_pos: Vector3 = _complex.nav_waypoints[_patrol_nodes[_patrol_idx]]["pos"]
				_path = _complex.find_nav_path(global_position, wp_pos)
			_play_anim("PatrolWalk", true)
		State.INVESTIGATE:
			speed = 2.65
			_state_timer -= delta
			_play_anim("PatrolWalk", true)
			if _state_timer <= 0.0:
				current_state = State.PATROL
		State.HUNT:
			speed = 4.65
			_state_timer -= delta
			_play_anim("HuntSprint", true)
			if _state_timer <= 0.0:
				current_state = State.INVESTIGATE
				_state_timer = 3.5

	while not _path.is_empty():
		var chk := _path[0]
		if Vector2(chk.x - global_position.x, chk.z - global_position.z).length() < 0.60:
			_path.pop_front()
		else:
			break

	if not _path.is_empty():
		var target := _path[0]
		var flat := Vector3(target.x - global_position.x, 0.0, target.z - global_position.z)
		var dir := flat.normalized()
		velocity.x = dir.x * speed
		velocity.z = dir.z * speed
		velocity.y = 0.0 if is_on_floor() else velocity.y - 14.0 * delta
		look_at(global_position + dir, Vector3.UP)
		move_and_slide()
	else:
		velocity = Vector3.ZERO
		_play_anim("IdleListen", true)

	if _player != null and _attack_cooldown <= 0.0:
		# Blind contact range: crouching motionless lets you slip past outside 1.15m!
		var strike_r := 1.55 if current_state == State.HUNT else 1.05
		if global_position.distance_to(_player.global_position) < strike_r:
			_attack_cooldown = 3.0
			_play_anim("Strike", false)
			AudioDirector.play_3d("entity_listener_shriek", global_position + Vector3(0, 1.6, 0), 20.0, 1.5)
			GameState.damage_player(26.0, "THE LISTENER TRACKED YOUR SOUND!")
			set_threat_active(true, global_position, 11.0)


func _play_anim(anim_name: String, loop_it: bool) -> void:
	if _anim == null or not _anim.has_animation(anim_name):
		return
	if loop_it:
		var a := _anim.get_animation(anim_name)
		if a:
			a.loop_mode = Animation.LOOP_LINEAR
	if _anim.current_animation != anim_name:
		_anim.play(anim_name)
