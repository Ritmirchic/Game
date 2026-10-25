class_name ImitatorEntity
extends CharacterBody3D
## THE IMITATOR: Deceptive horror entity that mimics corridor furnishings and
## broadcasts false environmental cues (warped elevator bell, phantom door latches)
## to lure the player into ambushes or provoke false anomaly reports.

var is_active_threat: bool = false
var is_unfolded: bool = false
var false_cues_emitted: int = 0

var _complex: ApartmentComplex
var _player: Node3D
var _model: Node3D
var _anim: AnimationPlayer
var _twitch_timer: float = 4.5
var _lure_timer: float = 7.0
var _relocate_path: Array[Vector3] = []
var _ambush_spots: Array[Vector3] = [
	Vector3(1.15, 0.0, 2.8),
	Vector3(-3.8, 0.0, -3.6),
	Vector3(7.8, 0.0, -3.6),
	Vector3(8.5, 0.0, 1.8),
]
var _spot_idx: int = 0


func setup(complex_ref: ApartmentComplex, player_ref: Node3D) -> void:
	_complex = complex_ref
	_player = player_ref
	set_meta("interactable", self)

	var col := CollisionShape3D.new()
	var cap := CapsuleShape3D.new()
	cap.radius = 0.32
	cap.height = 2.05
	col.shape = cap
	col.position = Vector3(0, 1.02, 0)
	add_child(col)

	_model = MaterialLibrary.instantiate_model("imitator_entity.glb")
	add_child(_model)
	_anim = _model.find_child("AnimationPlayer", true, false) as AnimationPlayer
	set_threat_active(false, Vector3(0, -30, 0))


func get_prompt() -> String:
	return "[E] Inspect Tall Floor Lamp  |  [R] Scan Suspicious Silhouette"


func interact(_actor: Node3D = null) -> void:
	if not is_active_threat:
		return
	trigger_ambush("IT WAS THE IMITATOR — NEVER TOUCH MIMIC PROPS!")


func set_threat_active(active: bool, requested_pos: Vector3, min_safe_dist: float = 8.0) -> void:
	is_active_threat = active
	visible = active
	is_unfolded = false
	_relocate_path.clear()
	if not active:
		global_position = Vector3(0, -30, 0)
		velocity = Vector3.ZERO
		return

	var chosen := requested_pos
	if _player != null and chosen.distance_to(_player.global_position) < min_safe_dist:
		for s in _ambush_spots:
			if s.distance_to(_player.global_position) >= min_safe_dist:
				chosen = s
				break
	global_position = chosen
	_play_anim("DisguisePose", false)


func emit_false_environmental_cue() -> Vector3:
	false_cues_emitted += 1
	var cue_pos := global_position + Vector3(randf_range(-2.5, 2.5), 1.4, randf_range(-2.5, 2.5))
	if false_cues_emitted % 2 == 0:
		AudioDirector.play_3d("elevator_ding_warped", cue_pos, 18.0, -3.0)
	else:
		AudioDirector.play_3d("door_close_latch", cue_pos, 16.0, -2.5, 0.86)
	return cue_pos


func trigger_ambush(message: String) -> void:
	is_unfolded = true
	_play_anim("Ambush", false)
	AudioDirector.play_3d("entity_imitator_unfold", global_position + Vector3(0, 1.5, 0), 18.0, 1.5)
	GameState.damage_player(22.0, message)
	# Scuttle along navigation waypoints to next disguise spot
	_spot_idx = (_spot_idx + 1) % _ambush_spots.size()
	if _complex != null:
		_relocate_path = _complex.find_nav_path(global_position, _ambush_spots[_spot_idx])


func _physics_process(delta: float) -> void:
	if not is_active_threat:
		return

	if not _relocate_path.is_empty():
		var target := _relocate_path[0]
		var flat := Vector3(target.x - global_position.x, 0.0, target.z - global_position.z)
		if flat.length() < 0.55:
			_relocate_path.pop_front()
			if _relocate_path.is_empty():
				is_unfolded = false
				_play_anim("DisguisePose", false)
		else:
			var dir := flat.normalized()
			velocity.x = dir.x * 4.4
			velocity.z = dir.z * 4.4
			velocity.y = 0.0
			look_at(global_position + dir, Vector3.UP)
			_play_anim("UnfoldWalk", true)
			move_and_slide()
		return

	# Subtle periodic organic twitch (fair clue for observant players!)
	_twitch_timer -= delta
	if _twitch_timer <= 0.0:
		_twitch_timer = randf_range(4.8, 7.2)
		_play_anim("Twitch", false)

	# Broadcast false environmental sound cues to deceive player
	_lure_timer -= delta
	if _lure_timer <= 0.0:
		_lure_timer = randf_range(8.5, 13.0)
		emit_false_environmental_cue()

	if _player != null and global_position.distance_to(_player.global_position) < 1.32:
		trigger_ambush("AMBUSHED BY THE IMITATOR!")


func _play_anim(anim_name: String, loop_it: bool) -> void:
	if _anim == null or not _anim.has_animation(anim_name):
		return
	if loop_it:
		var a := _anim.get_animation(anim_name)
		if a:
			a.loop_mode = Animation.LOOP_LINEAR
	_anim.play(anim_name)
