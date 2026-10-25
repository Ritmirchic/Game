class_name ObserverEntity
extends CharacterBody3D
## THE OBSERVER: Moves ONLY when outside the player's camera frustum OR occluded
## by walls/doors. Uses real 3D frustum + multi-ray physics occlusion checks.

var is_active_threat: bool = false
var is_observed: bool = false
var move_speed: float = 3.85
var anomaly_tag_id: int = 0

var _complex: ApartmentComplex
var _player: Node3D
var _model: Node3D
var _anim: AnimationPlayer
var _hum_loop: AudioStreamPlayer3D
var _path: Array[Vector3] = []
var _repath_timer: float = 0.0
var _sting_cooldown: float = 0.0
var _attack_cooldown: float = 0.0
var _frozen_pose_idx: int = 0


func setup(complex_ref: ApartmentComplex, player_ref: Node3D) -> void:
	_complex = complex_ref
	_player = player_ref

	var col := CollisionShape3D.new()
	var cap := CapsuleShape3D.new()
	cap.radius = 0.34
	cap.height = 2.15
	col.shape = cap
	col.position = Vector3(0, 1.08, 0)
	add_child(col)

	_model = MaterialLibrary.instantiate_model("observer_entity.glb")
	add_child(_model)
	_anim = _model.find_child("AnimationPlayer", true, false) as AnimationPlayer
	_hum_loop = AudioDirector.make_looping_3d("entity_observer_hum", self, Vector3(0, 1.6, 0), 12.0, -7.0)
	set_threat_active(false, Vector3(0, -30, 0))


func set_threat_active(active: bool, requested_spawn: Vector3, min_safe_dist: float = 9.5) -> void:
	is_active_threat = active
	visible = active
	if not active:
		if _hum_loop and _hum_loop.playing:
			_hum_loop.stop()
		global_position = Vector3(0, -30, 0)
		velocity = Vector3.ZERO
		return

	# Enforce fair spawn rule: NEVER spawn/teleport directly onto the player!
	var chosen := requested_spawn
	if _player != null and chosen.distance_to(_player.global_position) < min_safe_dist and _complex != null:
		var best_d := -1.0
		for wp_id in _complex.nav_waypoints.keys():
			var wp_pos: Vector3 = _complex.nav_waypoints[wp_id]["pos"]
			if absf(wp_pos.y - _player.global_position.y) < 1.0:
				var d := wp_pos.distance_to(_player.global_position)
				if d >= min_safe_dist and d > best_d:
					best_d = d
					chosen = wp_pos
	global_position = chosen
	_frozen_pose_idx = randi() % 2
	_play_anim("FrozenA" if _frozen_pose_idx == 0 else "FrozenB", false)
	if _hum_loop and not _hum_loop.playing:
		_hum_loop.play()


func check_visibility_from_camera(cam: Camera3D, cam_pos: Vector3, cam_forward: Vector3, fov_deg: float = 78.0) -> bool:
	var dist := global_position.distance_to(cam_pos)
	if dist > 27.0:
		return false

	var half_rad := deg_to_rad(fov_deg * 0.56)
	var cos_thresh := cos(half_rad)
	var sample_offsets: Array[Vector3] = [
		Vector3(0.0, 2.12, 0.0),   # Head
		Vector3(0.0, 1.52, 0.0),   # Chest
		Vector3(0.0, 0.96, 0.0),   # Pelvis
		Vector3(0.24, 1.38, 0.0),  # Left shoulder/arm
		Vector3(-0.24, 0.55, 0.0), # Right leg
	]

	var space := get_world_3d().direct_space_state
	var exclude: Array[RID] = [get_rid()]
	if _player is CollisionObject3D:
		exclude.append((_player as CollisionObject3D).get_rid())

	for off in sample_offsets:
		var pt := global_position + off
		var in_cone := false
		if cam != null and cam.is_inside_tree() and DisplayServer.get_name() != "headless":
			in_cone = cam.is_position_in_frustum(pt)
		else:
			var dir := (pt - cam_pos).normalized()
			in_cone = cam_forward.normalized().dot(dir) >= cos_thresh

		if not in_cone:
			continue

		var query := PhysicsRayQueryParameters3D.create(cam_pos, pt)
		query.exclude = exclude
		var hit := space.intersect_ray(query)
		if hit.is_empty():
			return true
		var hit_pos: Vector3 = hit["position"]
		if hit_pos.distance_to(pt) < 0.38:
			return true

	return false


func _physics_process(delta: float) -> void:
	if not is_active_threat or _player == null:
		return

	_sting_cooldown = maxf(0.0, _sting_cooldown - delta)
	_attack_cooldown = maxf(0.0, _attack_cooldown - delta)

	var cam: Camera3D = _player.get("camera") if _player else null
	var cam_pos := cam.global_position if cam != null else (_player.global_position + Vector3(0, 1.62, 0))
	var cam_fwd := (-cam.global_transform.basis.z) if cam != null else (-_player.global_transform.basis.z)
	var fov := cam.fov if cam != null else 76.0

	var was_observed := is_observed
	is_observed = check_visibility_from_camera(cam, cam_pos, cam_fwd, fov)

	if is_observed:
		velocity = Vector3.ZERO
		if not was_observed:
			_frozen_pose_idx = (_frozen_pose_idx + 1) % 2
			_play_anim("FrozenA" if _frozen_pose_idx == 0 else "FrozenB", false)
			if _sting_cooldown <= 0.0 and global_position.distance_to(_player.global_position) < 15.0:
				_sting_cooldown = 9.0
				AudioDirector.play_3d("entity_observer_sting", global_position + Vector3(0, 1.8, 0), 18.0, -2.0)
		return

	# Unobserved: advance along navigation path toward player
	_repath_timer -= delta
	if _repath_timer <= 0.0 or _path.is_empty():
		_repath_timer = 0.65
		if _complex != null:
			_path = _complex.find_nav_path(global_position, _player.global_position)

	while not _path.is_empty():
		var chk := _path[0]
		if Vector2(chk.x - global_position.x, chk.z - global_position.z).length() < 0.55:
			_path.pop_front()
		else:
			break

	if not _path.is_empty():
		var target := _path[0]
		var flat_delta := Vector3(target.x - global_position.x, 0.0, target.z - global_position.z)
		var dir := flat_delta.normalized()
		velocity.x = dir.x * move_speed
		velocity.z = dir.z * move_speed
		if not is_on_floor():
			velocity.y -= 14.0 * delta
		else:
			velocity.y = 0.0
		look_at(global_position + dir, Vector3.UP)
		_play_anim("StalkWalk", true)
		move_and_slide()

	# Melee reach if unobserved and right behind the player
	var p_dist := global_position.distance_to(_player.global_position)
	if p_dist < 1.45 and _attack_cooldown <= 0.0:
		_attack_cooldown = 3.2
		_play_anim("Lunge", false)
		AudioDirector.play_3d("entity_observer_sting", global_position + Vector3(0, 1.7, 0), 20.0, 2.0, 0.92)
		GameState.damage_player(28.0, "THE OBSERVER REACHED YOU IN THE BLIND SPOT!")
		# Reposition to a distant waypoint so it never stunlocks the player unfairly
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
