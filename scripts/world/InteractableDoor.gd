class_name InteractableDoor
extends Node3D
## Real functional hinged architectural door using Blender .glb meshes,
## synchronized StaticBody3D collision, spatial audio, and acoustic AI emission.

signal door_toggled(door_id: String, is_open: bool)
signal door_interacted_locked(door_id: String)

var door_id: String = "door"
var display_name: String = "Door"
var is_open: bool = false
var is_locked: bool = false
var lock_reason: String = "Locked from the other side."
var open_angle_deg: float = 92.0
var closed_yaw_rad: float = 0.0
var anomaly_tag_id: int = 0

var _hinge_pivot: Node3D
var _body: StaticBody3D
var _tween: Tween
var on_custom_interact: Callable


func setup(
	p_id: String,
	p_label: String,
	hinge_world_pos: Vector3,
	base_yaw_deg: float,
	model_file: String = "door_wood_panel.glb",
	swing_deg: float = 92.0,
	start_locked: bool = false,
	p_lock_reason: String = "Deadbolted."
) -> void:
	door_id = p_id
	display_name = p_label
	position = hinge_world_pos
	closed_yaw_rad = deg_to_rad(base_yaw_deg)
	rotation.y = closed_yaw_rad
	open_angle_deg = swing_deg
	is_locked = start_locked
	lock_reason = p_lock_reason

	_hinge_pivot = Node3D.new()
	_hinge_pivot.name = "HingePivot"
	add_child(_hinge_pivot)

	var visual := MaterialLibrary.instantiate_model(model_file)
	_hinge_pivot.add_child(visual)

	_body = StaticBody3D.new()
	_body.name = "DoorBody"
	_body.set_meta("interactable", self)
	_hinge_pivot.add_child(_body)

	var col := CollisionShape3D.new()
	var box := BoxShape3D.new()
	box.size = Vector3(1.06, 2.22, 0.08)
	col.shape = box
	col.position = Vector3(0.53, 1.11, 0.0)
	_body.add_child(col)


func get_prompt() -> String:
	if on_custom_interact.is_valid():
		return "[E] Inspect " + display_name
	if is_locked:
		return "[E] " + display_name + " (Locked)"
	return ("[E] Close " if is_open else "[E] Open ") + display_name


func interact(_actor: Node3D = null) -> void:
	if on_custom_interact.is_valid():
		on_custom_interact.call()
		return
	if is_locked:
		AudioDirector.play_3d("door_locked_rattle", global_position + Vector3(0, 1.0, 0), 12.0, -2.0)
		SoundEventBus.emit_noise(global_position, 48.0, "door")
		GameState.notification_posted.emit(display_name + ": " + lock_reason, Color(0.90, 0.52, 0.36))
		door_interacted_locked.emit(door_id)
		# Subtle rattle jiggle
		if _tween and _tween.is_running():
			_tween.kill()
		_tween = create_tween()
		_tween.tween_property(_hinge_pivot, "rotation:y", deg_to_rad(2.5), 0.05)
		_tween.tween_property(_hinge_pivot, "rotation:y", deg_to_rad(-2.0), 0.05)
		_tween.tween_property(_hinge_pivot, "rotation:y", 0.0, 0.05)
		return

	set_open_state(not is_open, true)


func set_open_state(target_open: bool, play_audio: bool = true, custom_angle_deg: float = -999.0) -> void:
	is_open = target_open
	var target_deg := 0.0
	if custom_angle_deg > -900.0:
		target_deg = custom_angle_deg
	elif is_open:
		target_deg = open_angle_deg

	if _tween and _tween.is_running():
		_tween.kill()
	if play_audio and is_inside_tree():
		_tween = create_tween().set_trans(Tween.TRANS_CUBIC).set_ease(Tween.EASE_OUT)
		_tween.tween_property(_hinge_pivot, "rotation:y", deg_to_rad(target_deg), 0.42)
		AudioDirector.play_3d("door_open_creak" if is_open else "door_close_latch", global_position + Vector3(0, 1.1, 0), 15.0, -2.0)
		SoundEventBus.emit_noise(global_position, 54.0, "door")
	else:
		_hinge_pivot.rotation.y = deg_to_rad(target_deg)

	door_toggled.emit(door_id, is_open)


func slam_and_lock(reason: String) -> void:
	is_open = false
	is_locked = true
	lock_reason = reason
	if _tween and _tween.is_running():
		_tween.kill()
	if is_inside_tree():
		_tween = create_tween().set_trans(Tween.TRANS_EXPO).set_ease(Tween.EASE_IN)
		_tween.tween_property(_hinge_pivot, "rotation:y", 0.0, 0.16)
		AudioDirector.play_3d("door_slam_heavy", global_position + Vector3(0, 1.1, 0), 22.0, 1.0)
		SoundEventBus.emit_noise(global_position, 82.0, "door_slam")
	else:
		_hinge_pivot.rotation.y = 0.0
