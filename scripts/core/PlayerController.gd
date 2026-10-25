class_name PlayerController
extends CharacterBody3D
## First-Person Inspector Controller with Dual-Cone Shadow-Casting Flashlight,
## Handheld Field Anomaly Scanner, Surface-Aware Spatial Footsteps, Crouch Stealth,
## and Contextual Raycast Interaction.

signal prompt_changed(prompt_str: String, can_scan_hint: bool)
signal scan_performed(success: bool, message: String)

var camera: Camera3D
var flashlight_on: bool = true
var is_crouching: bool = false
var controls_locked: bool = false

var _head_pivot: Node3D
var _spot_main: SpotLight3D
var _spot_fill: SpotLight3D
var _torch_model: Node3D
var _scanner_model: Node3D
var _col_shape: CollisionShape3D
var _capsule: CapsuleShape3D
var _ray: RayCast3D

var _complex: ApartmentComplex
var _anomalies: AnomalyManager
var _entities: Array[Node3D] = []

var _pitch: float = 0.0
var _bob_phase: float = 0.0
var _step_dist: float = 0.0
var _scan_cooldown: float = 0.0
var _current_interactable: Object = null


func setup(complex_ref: ApartmentComplex, anomaly_ref: AnomalyManager, entity_list: Array[Node3D]) -> void:
	_complex = complex_ref
	_anomalies = anomaly_ref
	_entities = entity_list

	_col_shape = CollisionShape3D.new()
	_capsule = CapsuleShape3D.new()
	_capsule.radius = 0.33
	_capsule.height = 1.74
	_col_shape.shape = _capsule
	_col_shape.position = Vector3(0, 0.87, 0)
	add_child(_col_shape)

	_head_pivot = Node3D.new()
	_head_pivot.name = "HeadPivot"
	_head_pivot.position = Vector3(0, 1.60, 0)
	add_child(_head_pivot)

	camera = Camera3D.new()
	camera.name = "PlayerCamera"
	camera.fov = float(GameState.settings.get("fov", 76.0))
	camera.near = 0.06
	camera.far = 90.0
	camera.current = true
	_head_pivot.add_child(camera)

	# Interaction Raycast (3.2m reach)
	_ray = RayCast3D.new()
	_ray.target_position = Vector3(0, 0, -3.25)
	_ray.enabled = true
	camera.add_child(_ray)

	# Dual-Cone Realistic Flashlight (Guaranteed clear visibility & shadows!)
	_spot_main = SpotLight3D.new()
	_spot_main.name = "FlashlightMainBeam"
	_spot_main.light_color = Color(1.0, 0.96, 0.88)
	_spot_main.light_energy = 3.55
	_spot_main.spot_range = 24.5
	_spot_main.spot_angle = 27.5
	_spot_main.spot_attenuation = 1.25
	_spot_main.shadow_enabled = true
	_spot_main.position = Vector3(0.20, -0.14, -0.18)
	camera.add_child(_spot_main)

	_spot_fill = SpotLight3D.new()
	_spot_fill.name = "FlashlightSoftBounce"
	_spot_fill.light_color = Color(0.95, 0.92, 0.86)
	_spot_fill.light_energy = 0.92
	_spot_fill.spot_range = 9.5
	_spot_fill.spot_angle = 58.0
	_spot_fill.shadow_enabled = false
	_spot_fill.position = Vector3(0.18, -0.14, -0.15)
	camera.add_child(_spot_fill)

	# Handheld 3D Blender Viewmodels (Flashlight on Right, Field Recorder on Left)
	_torch_model = MaterialLibrary.instantiate_model("flashlight_handheld.glb")
	_torch_model.position = Vector3(0.24, -0.22, -0.34)
	camera.add_child(_torch_model)

	_scanner_model = MaterialLibrary.instantiate_model("scanner_clipboard.glb")
	_scanner_model.position = Vector3(-0.27, -0.26, -0.38)
	_scanner_model.rotation_degrees = Vector3(24.0, -18.0, 8.0)
	camera.add_child(_scanner_model)

	GameState.state_updated.connect(_apply_camera_settings)
	_apply_camera_settings()


func _apply_camera_settings() -> void:
	if camera:
		camera.fov = float(GameState.settings.get("fov", 76.0))


func set_look_angles(yaw_rad: float, pitch_rad: float = 0.0) -> void:
	rotation.y = yaw_rad
	_pitch = clampf(pitch_rad, deg_to_rad(-82.0), deg_to_rad(82.0))
	if _head_pivot:
		_head_pivot.rotation.x = _pitch


func _unhandled_input(event: InputEvent) -> void:
	if controls_locked:
		return

	if event is InputEventMouseMotion and Input.get_mouse_mode() == Input.MOUSE_MODE_CAPTURED:
		var sens: float = float(GameState.settings.get("mouse_sensitivity", 0.0022))
		var mm := event as InputEventMouseMotion
		rotate_y(-mm.relative.x * sens)
		_pitch = clampf(_pitch - mm.relative.y * sens, deg_to_rad(-82.0), deg_to_rad(82.0))
		_head_pivot.rotation.x = _pitch

	elif event.is_action_pressed("flashlight"):
		toggle_flashlight()

	elif event.is_action_pressed("interact"):
		trigger_interact()

	elif event.is_action_pressed("scan_report"):
		trigger_field_scan()


func toggle_flashlight() -> void:
	flashlight_on = not flashlight_on
	_spot_main.visible = flashlight_on
	_spot_fill.visible = flashlight_on
	AudioDirector.play_2d("flashlight_click", -3.0, 1.05 if flashlight_on else 0.92)


func trigger_interact() -> void:
	if _current_interactable != null and _current_interactable.has_method("interact"):
		_current_interactable.call("interact", self)


func trigger_field_scan() -> void:
	if _scan_cooldown > 0.0:
		return
	if GameState.scanner_charges <= 0:
		AudioDirector.play_2d("scanner_error", -3.0)
		GameState.notification_posted.emit("SCANNER DEPLETED — Submit Cycle Report at South Threshold Desk to recharge.", Color(0.92, 0.44, 0.30))
		return

	_scan_cooldown = 0.65
	var cam_pos := camera.global_position
	var cam_fwd := -camera.global_transform.basis.z

	# 1. Check if aiming at an active Horror Entity (Observer / Listener / Imitator) within 14m!
	for ent in _entities:
		if ent != null and ent.visible and ent.get("is_active_threat") == true:
			var edist := cam_pos.distance_to(ent.global_position + Vector3(0, 1.2, 0))
			var edir := ((ent.global_position + Vector3(0, 1.2, 0)) - cam_pos).normalized()
			if edist <= 14.5 and cam_fwd.dot(edir) >= 0.78:
				AudioDirector.play_2d("scanner_confirm", -1.5, 1.18)
				if ent is ImitatorEntity:
					# Scanning the Imitator safely exposes & banishes it without taking ambush damage!
					(ent as ImitatorEntity).set_threat_active(false, Vector3(0, -30, 0))
					GameState.insight_points += 20
					var msg_i := "SCANNER LOCK: THE IMITATOR EXPOSED & REPELLED (+20 Insight)"
					GameState.notification_posted.emit(msg_i, Color(0.35, 0.96, 0.60))
					scan_performed.emit(true, msg_i)
					return
				else:
					var ename := "THE OBSERVER" if ent is ObserverEntity else "THE LISTENER"
					var msg_e := "ENTITY SIGNATURE CONFIRMED: " + ename + " — Retreat to Threshold!"
					GameState.notification_posted.emit(msg_e, Color(0.98, 0.75, 0.32))
					scan_performed.emit(true, msg_e)
					return

	# 2. Check AnomalyManager for any active visual/audio/environmental/entity anomaly
	if _anomalies != null:
		var res := _anomalies.evaluate_scanner_attempt(cam_pos, cam_fwd)
		if bool(res.get("success", false)):
			AudioDirector.play_2d("scanner_confirm", -1.5, 1.0)
			var meta: Dictionary = res["meta"]
			var msg := "ANOMALY LOGGED [%s]: %s (%s)" % [meta["code"], meta["title"], meta["category"]]
			GameState.notification_posted.emit(msg, Color(0.35, 0.96, 0.58))
			scan_performed.emit(true, msg)
			return

	# 3. False Report Penalty!
	AudioDirector.play_2d("scanner_error", -2.0)
	GameState.apply_false_report_penalty("No anomalous signature at scanned target.")
	scan_performed.emit(false, "FALSE SCAN — Baseline normal object.")


func _physics_process(delta: float) -> void:
	_scan_cooldown = maxf(0.0, _scan_cooldown - delta)

	if controls_locked:
		velocity = Vector3.ZERO
		return

	# Crouch smoothly adjusts capsule & eye height
	is_crouching = Input.is_action_pressed("crouch")
	var target_h := 1.08 if is_crouching else 1.74
	var target_eye := 0.96 if is_crouching else 1.60
	_capsule.height = move_toward(_capsule.height, target_h, delta * 5.5)
	_col_shape.position.y = _capsule.height * 0.5
	_head_pivot.position.y = move_toward(_head_pivot.position.y, target_eye, delta * 5.5)

	var in_dir := Input.get_vector("move_left", "move_right", "move_forward", "move_back")
	var wish := (transform.basis * Vector3(in_dir.x, 0.0, in_dir.y)).normalized()
	var sprinting := Input.is_action_pressed("sprint") and not is_crouching and in_dir.y < -0.1

	var speed := 3.55
	if is_crouching:
		speed = 1.85
	elif sprinting:
		speed = 5.65

	if wish.length() > 0.01:
		velocity.x = move_toward(velocity.x, wish.x * speed, delta * 24.0)
		velocity.z = move_toward(velocity.z, wish.z * speed, delta * 24.0)
	else:
		velocity.x = move_toward(velocity.x, 0.0, delta * 22.0)
		velocity.z = move_toward(velocity.z, 0.0, delta * 22.0)

	if not is_on_floor():
		velocity.y -= 16.0 * delta
	else:
		velocity.y = -0.1

	var prev_pos := global_position
	move_and_slide()
	var horiz_moved := Vector2(global_position.x - prev_pos.x, global_position.z - prev_pos.z).length()

	# Head-bob, handheld sway, and surface-aware spatial footsteps + AI noise emission
	if is_on_floor() and horiz_moved > 0.002:
		_step_dist += horiz_moved
		var stride := 1.35 if is_crouching else (2.15 if sprinting else 1.72)
		if bool(GameState.settings.get("head_bob", true)):
			_bob_phase += horiz_moved * 3.4
			camera.position.y = sin(_bob_phase) * (0.018 if is_crouching else (0.042 if sprinting else 0.028))
		_torch_model.position.y = -0.22 + sin(_bob_phase * 0.5) * 0.008
		_scanner_model.position.y = -0.26 - sin(_bob_phase * 0.5) * 0.008

		if _step_dist >= stride:
			_step_dist = 0.0
			var zone := _complex.get_zone_at(global_position) if _complex else {"surface": "wood"}
			var surf: String = zone.get("surface", "wood")
			var step_vol := -14.0 if is_crouching else (-2.0 if sprinting else -6.5)
			var step_db := 12.0 if is_crouching else (78.0 if sprinting else 40.0)
			AudioDirector.play_footstep(surf, global_position, step_vol)
			SoundEventBus.emit_noise(global_position, step_db, "player_step")
	else:
		camera.position.y = move_toward(camera.position.y, 0.0, delta * 4.0)

	_update_interaction_ray()


func _update_interaction_ray() -> void:
	_ray.force_raycast_update()
	var found: Object = null
	if _ray.is_colliding():
		var col := _ray.get_collider()
		if col != null and col.has_meta("interactable"):
			found = col.get_meta("interactable")

	if found != _current_interactable:
		_current_interactable = found
		var p_txt := ""
		if _current_interactable != null and _current_interactable.has_method("get_prompt"):
			p_txt = str(_current_interactable.call("get_prompt"))
		prompt_changed.emit(p_txt, _current_interactable != null)
