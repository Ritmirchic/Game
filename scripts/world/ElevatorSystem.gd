class_name ElevatorSystem
extends Node3D
## Functional Hydraulic/Traction Elevator connecting Floor 4 (Y=0.0) and Basement B (Y=-4.2),
## with dual steel sliding cab & shaft doors, floor display, spatial audio, and Anomaly 16 / 22 hooks.

signal arrived_at_floor(floor_code: String)

var current_floor: String = "4"
var is_moving: bool = false
var doors_open: bool = true

# Anomaly states
var phantom_sound_anomaly: bool = false   # Anomaly #16
var wrong_floor_anomaly: bool = false     # Anomaly #22

var _cab_root: AnimatableBody3D
var _door_l: MeshInstance3D
var _door_r: MeshInstance3D
var _door_col_l: CollisionShape3D
var _door_col_r: CollisionShape3D
var _cab_light: OmniLight3D
var _floor_labels: Array[Label3D] = []
var _motor_loop: AudioStreamPlayer3D
var _phantom_timer: float = 0.0
var _wrong_floor_backdrop: Node3D


func build_elevator(shaft_center_xz: Vector2) -> void:
	position = Vector3(shaft_center_xz.x, 0.0, shaft_center_xz.y)
	var m_steel := MaterialLibrary.get_pbr("metal_brushed_steel", 0.5, 0.82)
	var m_wood := MaterialLibrary.get_pbr("wood_walnut_furniture", 0.5, 0.05)
	var m_floor := MaterialLibrary.get_pbr("floor_tile_linoleum", 0.6, 0.05)

	# Movable Elevator Cab (2.2m wide x 2.2m deep x 2.65m tall, open on -X facing Main/Elevator Lobby)
	_cab_root = AnimatableBody3D.new()
	_cab_root.name = "ElevatorCab"
	_cab_root.sync_to_physics = false
	add_child(_cab_root)

	_add_cab_panel(_cab_root, Vector3(2.2, 0.16, 2.2), Vector3(0, -0.08, 0), m_floor) # Floor
	_add_cab_panel(_cab_root, Vector3(2.2, 0.14, 2.2), Vector3(0, 2.68, 0), m_steel)  # Ceiling
	_add_cab_panel(_cab_root, Vector3(0.12, 2.75, 2.2), Vector3(1.06, 1.35, 0), m_wood) # Back (+X)
	_add_cab_panel(_cab_root, Vector3(2.2, 2.75, 0.12), Vector3(0, 1.35, -1.06), m_wood) # North (-Z)
	_add_cab_panel(_cab_root, Vector3(2.2, 2.75, 0.12), Vector3(0, 1.35, 1.06), m_wood)  # South (+Z)

	# Sliding front doors on -X face (X = -1.04)
	var door_mesh := BoxMesh.new()
	door_mesh.size = Vector3(0.06, 2.30, 0.98)
	_door_l = MeshInstance3D.new()
	_door_l.mesh = door_mesh
	_door_l.material_override = m_steel
	_door_l.position = Vector3(-1.04, 1.15, -0.88)
	_cab_root.add_child(_door_l)

	_door_r = MeshInstance3D.new()
	_door_r.mesh = door_mesh
	_door_r.material_override = m_steel
	_door_r.position = Vector3(-1.04, 1.15, 0.88)
	_cab_root.add_child(_door_r)

	var dshape := BoxShape3D.new()
	dshape.size = Vector3(0.08, 2.30, 0.98)
	_door_col_l = CollisionShape3D.new()
	_door_col_l.shape = dshape
	_door_col_l.position = _door_l.position
	_door_col_l.disabled = true
	_cab_root.add_child(_door_col_l)

	_door_col_r = CollisionShape3D.new()
	_door_col_r.shape = dshape
	_door_col_r.position = _door_r.position
	_door_col_r.disabled = true
	_cab_root.add_child(_door_col_r)

	# Cab ceiling light
	_cab_light = OmniLight3D.new()
	_cab_light.position = Vector3(0.0, 2.48, 0.0)
	_cab_light.light_color = Color(1.0, 0.92, 0.76)
	_cab_light.light_energy = 1.45
	_cab_light.omni_range = 5.5
	_cab_root.add_child(_cab_light)

	# Interior Control Panel (ride between Floor 4 and Basement B)
	var panel := WorldInteractable.new()
	panel.name = "CabFloorSelector"
	panel.position = Vector3(-0.88, 1.25, 0.86)
	panel.prompt_text = "[E] Operate Elevator Floor Panel (4F / Basement)"
	panel.setup_box_collider(Vector3(0.18, 0.42, 0.24))
	panel.on_interact = func(actor: Node3D): _on_panel_pressed(actor)
	_cab_root.add_child(panel)

	var pmesh := MeshInstance3D.new()
	var pb := BoxMesh.new()
	pb.size = Vector3(0.06, 0.38, 0.20)
	pmesh.mesh = pb
	pmesh.material_override = m_steel
	panel.add_child(pmesh)

	# Interior LED Floor Display
	var cab_lbl := _make_floor_label(Vector3(-0.96, 2.32, 0.0), deg_to_rad(-90))
	_cab_root.add_child(cab_lbl)
	_floor_labels.append(cab_lbl)

	# Exterior Call Buttons & Floor Indicators on Floor 4 (Y=0) and Basement (Y=-4.2)
	for f_info in [{"y": 0.0, "code": "4"}, {"y": -4.2, "code": "B"}]:
		var fy: float = f_info["y"]
		var fcode: String = f_info["code"]
		var call_btn := WorldInteractable.new()
		call_btn.position = Vector3(-1.22, fy + 1.18, 1.22)
		call_btn.prompt_text = "[E] Call Elevator to Floor " + fcode
		call_btn.setup_box_collider(Vector3(0.16, 0.24, 0.18))
		call_btn.on_interact = func(_actor: Node3D): call_to_floor(fcode)
		add_child(call_btn)

		var bm := MeshInstance3D.new()
		var bbox := BoxMesh.new()
		bbox.size = Vector3(0.06, 0.20, 0.12)
		bm.mesh = bbox
		bm.material_override = MaterialLibrary.get_emissive(Color(0.2, 0.22, 0.24), Color(0.85, 0.55, 0.2), 1.2)
		call_btn.add_child(bm)

		var ext_lbl := _make_floor_label(Vector3(-1.20, fy + 2.46, 0.0), deg_to_rad(-90))
		add_child(ext_lbl)
		_floor_labels.append(ext_lbl)

	# Wrong-Floor Anomaly Quarantine Backdrop (visible outside cab when Anomaly #22 triggers)
	_wrong_floor_backdrop = Node3D.new()
	_wrong_floor_backdrop.visible = false
	_cab_root.add_child(_wrong_floor_backdrop)
	var abyss_wall := MeshInstance3D.new()
	var aw_mesh := BoxMesh.new()
	aw_mesh.size = Vector3(0.08, 2.6, 2.1)
	abyss_wall.mesh = aw_mesh
	abyss_wall.position = Vector3(-1.35, 1.3, 0.0)
	abyss_wall.material_override = MaterialLibrary.get_void_window_mat()
	_wrong_floor_backdrop.add_child(abyss_wall)

	var abyss_lbl := Label3D.new()
	abyss_lbl.text = "SUB-LEVEL -9 // QUARANTINED\n[STEP FORWARD AT CRITICAL INSTABILITY OR SCAN WITH R]"
	abyss_lbl.font_size = 24
	abyss_lbl.modulate = Color(1.0, 0.28, 0.24)
	abyss_lbl.position = Vector3(-1.28, 1.62, 0.0)
	abyss_lbl.rotation.y = deg_to_rad(90)
	_wrong_floor_backdrop.add_child(abyss_lbl)

	var abyss_portal := WorldInteractable.new()
	abyss_portal.position = Vector3(-1.30, 1.2, 0.0)
	abyss_portal.anomaly_tag_id = 22
	abyss_portal.prompt_text = "[E] Step Into Sub-Level -9 Threshold  |  [R] Scan Anomaly"
	abyss_portal.setup_box_collider(Vector3(0.24, 2.2, 1.8))
	abyss_portal.on_interact = func(_a):
		if wrong_floor_anomaly:
			GameState.trigger_ending("C")
	_wrong_floor_backdrop.add_child(abyss_portal)

	_motor_loop = AudioDirector.make_looping_3d("elevator_move_loop", _cab_root, Vector3(0, 2.6, 0), 16.0, -5.0)
	_update_floor_labels("4")


func _add_cab_panel(parent: Node3D, sz: Vector3, pos: Vector3, mat: Material) -> void:
	var mi := MeshInstance3D.new()
	var bm := BoxMesh.new()
	bm.size = sz
	mi.mesh = bm
	mi.position = pos
	mi.material_override = mat
	parent.add_child(mi)

	var cs := CollisionShape3D.new()
	var bs := BoxShape3D.new()
	bs.size = sz
	cs.shape = bs
	cs.position = pos
	parent.add_child(cs)


func _make_floor_label(pos: Vector3, rot_y: float) -> Label3D:
	var lbl := Label3D.new()
	lbl.text = "[ 4F ]"
	lbl.font_size = 38
	lbl.modulate = Color(0.98, 0.72, 0.28)
	lbl.outline_size = 8
	lbl.position = pos
	lbl.rotation.y = rot_y
	return lbl


func _update_floor_labels(code: String) -> void:
	for l in _floor_labels:
		l.text = "[ " + code + " ]"
		l.modulate = Color(0.95, 0.22, 0.20) if code == "-9" else Color(0.98, 0.72, 0.28)


func _process(delta: float) -> void:
	if phantom_sound_anomaly and not is_moving:
		_phantom_timer -= delta
		if _phantom_timer <= 0.0:
			_phantom_timer = randf_range(5.5, 8.5)
			AudioDirector.play_3d("elevator_ding_warped", global_position + Vector3(-1.0, 2.1, 0), 18.0, -2.0)
			if not _motor_loop.playing:
				_motor_loop.play()
	elif not is_moving and _motor_loop and _motor_loop.playing:
		_motor_loop.stop()


func _on_panel_pressed(actor: Node3D) -> void:
	if is_moving:
		return
	if wrong_floor_anomaly:
		# Anomaly #22 hijacks first ride to Floor -9 display, or returns to 4F on second press
		if current_floor != "-9":
			_ride_to("-9", _cab_root.position.y, actor)
		else:
			_wrong_floor_backdrop.visible = false
			_ride_to("4", 0.0, actor)
		return

	var next_floor := "B" if current_floor == "4" else "4"
	var target_y := -4.2 if next_floor == "B" else 0.0
	_ride_to(next_floor, target_y, actor)


func call_to_floor(floor_code: String) -> void:
	if is_moving:
		return
	var target_y := -4.2 if floor_code == "B" else 0.0
	if current_floor == floor_code and not doors_open:
		_set_doors(true)
		return
	_ride_to(floor_code, target_y, null)


func _ride_to(dest_code: String, target_y: float, passenger: Node3D) -> void:
	is_moving = true
	_set_doors(false)
	if _motor_loop:
		_motor_loop.play()
	SoundEventBus.emit_noise(_cab_root.global_position, 62.0, "elevator")

	var start_y := _cab_root.position.y
	var dy := target_y - start_y
	var tw := create_tween().set_trans(Tween.TRANS_SINE).set_ease(Tween.EASE_IN_OUT)
	tw.tween_interval(0.45)
	tw.tween_method(func(alpha: float):
		var new_y := lerpf(start_y, target_y, alpha)
		var step_y := new_y - _cab_root.position.y
		_cab_root.position.y = new_y
		if passenger != null and is_instance_valid(passenger):
			var rel := passenger.global_position - _cab_root.global_position
			if absf(rel.x) < 1.15 and absf(rel.z) < 1.15:
				passenger.global_position.y += step_y
	, 0.0, 1.0, maxf(1.2, absf(dy) * 0.48))
	tw.tween_callback(func():
		if _motor_loop:
			_motor_loop.stop()
		current_floor = dest_code
		_update_floor_labels(dest_code)
		if dest_code == "-9":
			_cab_light.light_color = Color(0.95, 0.16, 0.14)
			_wrong_floor_backdrop.visible = true
			AudioDirector.play_3d("elevator_ding_warped", _cab_root.global_position + Vector3(0, 2.2, 0), 16.0, 0.0)
		else:
			_cab_light.light_color = Color(1.0, 0.92, 0.76)
			_wrong_floor_backdrop.visible = false
			AudioDirector.play_3d("elevator_ding", _cab_root.global_position + Vector3(0, 2.2, 0), 16.0, -1.0)
		_set_doors(true)
		is_moving = false
		arrived_at_floor.emit(dest_code)
	)


func _set_doors(open_state: bool) -> void:
	doors_open = open_state
	_door_col_l.disabled = open_state
	_door_col_r.disabled = open_state
	var z_off := 0.92 if open_state else 0.49
	var tw := create_tween().set_parallel(true)
	tw.tween_property(_door_l, "position:z", -z_off, 0.38)
	tw.tween_property(_door_r, "position:z", z_off, 0.38)


func reset_normal() -> void:
	phantom_sound_anomaly = false
	wrong_floor_anomaly = false
	_wrong_floor_backdrop.visible = false
	if _motor_loop and _motor_loop.playing:
		_motor_loop.stop()
	_cab_light.light_color = Color(1.0, 0.92, 0.76)
	if current_floor == "-9":
		current_floor = "4"
		_cab_root.position.y = 0.0
	_update_floor_labels(current_floor)
	_set_doors(true)
