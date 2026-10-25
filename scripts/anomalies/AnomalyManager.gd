class_name AnomalyManager
extends Node3D
## Complete Implementation of All 30 Real Functional Anomalies (Visual 1-10,
## Audio 11-18, Environmental 19-25, Entity 26-30) with Detection, Consequences,
## Particles, Spatial Audio, and Exact Baseline Reset.

signal anomaly_verified_by_scanner(anomaly_id: int)

var active_anomalies: Array[int] = []
var verified_in_current_cycle: Dictionary = {} # anomaly_id -> true

var _complex: ApartmentComplex
var _player: Node3D
var _time: float = 0.0

# Dedicated 3D Nodes & Audio Loops for Anomalies
var _whisper_loop: AudioStreamPlayer3D
var _radio_loop: AudioStreamPlayer3D
var _radio_glow: OmniLight3D
var _breathing_loop: AudioStreamPlayer3D
var _ceiling_dust: CPUParticles3D
var _corridor_figure: Node3D
var _shadow_walker: Node3D
var _window_watcher: Node3D
var _door_peeker: Node3D
var _silent_follower: Node3D

# Runtime timers & state flags for active anomalies
var _knock_timer: float = 0.0
var _scream_timer: float = 0.0
var _upper_step_timer: float = 0.0
var _upper_step_z: float = 10.0
var _upper_step_dir: float = -1.0
var _trail_step_accum: float = 0.0
var _last_player_pos: Vector3 = Vector3.ZERO
var _door19_triggered: bool = false
var _shadow27_x: float = 7.4
var _shadow27_dir: float = 1.0
var _figure26_stare_time: float = 0.0
var _follower_path: Array[Vector3] = []
var _follower_repath: float = 0.0


func setup(complex_ref: ApartmentComplex, player_ref: Node3D) -> void:
	_complex = complex_ref
	_player = player_ref
	_build_anomaly_actors()
	reset_all_anomalies()


func _build_anomaly_actors() -> void:
	# Audio #12: Wardrobe Whispering in Apt 4B
	_whisper_loop = AudioDirector.make_looping_3d("anom_whisper_loop", self, Vector3(4.5, 1.35, 13.8), 11.0, -3.0)

	# Audio #14: Radio Interference in Apt 4C
	_radio_loop = AudioDirector.make_looping_3d("anom_radio_static", self, Vector3(-7.6, 0.9, -12.0), 13.0, -2.5)
	_radio_glow = OmniLight3D.new()
	_radio_glow.position = Vector3(-7.6, 0.95, -11.9)
	_radio_glow.light_color = Color(1.0, 0.58, 0.18)
	_radio_glow.light_energy = 0.0
	_radio_glow.omni_range = 3.5
	_radio_glow.visible = false
	add_child(_radio_glow)

	# Audio #18: Unexplained Raspy Breathing in Laundry Room NE Corner
	_breathing_loop = AudioDirector.make_looping_3d("anom_breathing_close", self, Vector3(14.3, 1.45, 4.0), 10.5, -2.0)

	# Audio #17: Falling plaster dust particles under pacing ceiling footsteps
	_ceiling_dust = CPUParticles3D.new()
	_ceiling_dust.emitting = false
	_ceiling_dust.amount = 28
	_ceiling_dust.lifetime = 1.6
	var dm := BoxMesh.new()
	dm.size = Vector3(0.025, 0.025, 0.025)
	_ceiling_dust.mesh = dm
	_ceiling_dust.emission_shape = CPUParticles3D.EMISSION_SHAPE_BOX
	_ceiling_dust.emission_box_extents = Vector3(0.55, 0.05, 0.55)
	_ceiling_dust.direction = Vector3(0, -1, 0)
	_ceiling_dust.gravity = Vector3(0, -1.8, 0)
	_ceiling_dust.initial_velocity_min = 0.1
	_ceiling_dust.initial_velocity_max = 0.4
	add_child(_ceiling_dust)

	# Entity #26: Motionless Corridor End Figure
	_corridor_figure = MaterialLibrary.instantiate_model("observer_entity.glb")
	_corridor_figure.visible = false
	add_child(_corridor_figure)

	# Entity #27: Shadow Crossing Laundry Room
	_shadow_walker = MaterialLibrary.instantiate_model("observer_entity.glb")
	_apply_shadow_material_recursive(_shadow_walker)
	_shadow_walker.visible = false
	add_child(_shadow_walker)

	# Entity #28: Creature Watching Through 4th-Floor Window (Outside Apt 4B Window)
	_window_watcher = MaterialLibrary.instantiate_model("listener_entity.glb")
	_window_watcher.visible = false
	add_child(_window_watcher)
	var ww_light := OmniLight3D.new()
	ww_light.position = Vector3(-0.3, 1.75, 0.0)
	ww_light.light_color = Color(0.75, 0.22, 0.22)
	ww_light.light_energy = 0.85
	ww_light.omni_range = 2.6
	_window_watcher.add_child(ww_light)

	# Entity #29: Entity Peeking Behind Partially Open Storage 4-B Door
	_door_peeker = MaterialLibrary.instantiate_model("listener_entity.glb")
	_door_peeker.visible = false
	add_child(_door_peeker)

	# Entity #30: Creature Silently Following Player
	_silent_follower = MaterialLibrary.instantiate_model("imitator_entity.glb")
	_silent_follower.visible = false
	add_child(_silent_follower)


func _apply_shadow_material_recursive(n: Node) -> void:
	if n is MeshInstance3D:
		var sm := StandardMaterial3D.new()
		sm.albedo_color = Color(0.02, 0.02, 0.03, 0.86)
		sm.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
		sm.roughness = 0.98
		(n as MeshInstance3D).material_override = sm
	for c in n.get_children():
		_apply_shadow_material_recursive(c)


# ============================================================
# ACTIVATE & RESET ALL 30 ANOMALIES
# ============================================================
func reset_all_anomalies() -> void:
	active_anomalies.clear()
	verified_in_current_cycle.clear()
	_door19_triggered = false
	_figure26_stare_time = 0.0
	_follower_path.clear()

	if _complex == null:
		return

	# 1. Reset Painting (#1)
	if _complex.hero_painting_canvas:
		_complex.hero_painting_canvas.material_override = MaterialLibrary.get_image_mat("res://assets/textures/painting_coastal_normal.png")

	# 2. Reset Apt 4A Furniture (#2)
	for item in _complex.apt4a_furniture_baseline:
		var nd: Node3D = item["node"]
		nd.position = item["pos"]
		nd.rotation.y = item["rot_y"]

	# 3. Reset Apt 4A TV (#3)
	if _complex.apt4a_tv:
		_complex.apt4a_tv.visible = true
		_complex.apt4a_tv.position.y = 0.45

	# 4. Reset Impossible Corridor Door (#4)
	if _complex.blank_west_wall_plug:
		_complex.blank_west_wall_plug.visible = true
		_complex.blank_west_wall_plug.position.y = 0.0
	if _complex.impossible_corridor_door:
		_complex.impossible_corridor_door.set_open_state(false, false)
		_complex.impossible_corridor_door.visible = false
		_complex.impossible_corridor_door.position.y = -20.0

	# 5. Reset Corridor Wallpaper (#5)
	var m_beige := MaterialLibrary.get_pbr("wall_wallpaper_beige", 0.42)
	for wm in _complex.corridor_wall_meshes:
		if is_instance_valid(wm):
			wm.material_override = m_beige

	# 6. Reset Apt 4A Window (#6)
	if _complex.apt4a_window_pane:
		_complex.apt4a_window_pane.material_override = MaterialLibrary.get_image_mat("res://assets/textures/window_city_night.png", 0.85)

	# 7 & 23. Reset Hallway Lighting & Emergency Beacons (#7, #23)
	for l in _complex.corridor_lights:
		if is_instance_valid(l):
			l.light_color = Color(1.0, 0.88, 0.68)
			l.light_energy = 1.35
	for b in _complex.emergency_beacons:
		b.visible = false
	for el in _complex.emergency_lights:
		el.visible = false
		el.light_energy = 0.0

	# 8. Reset Misplaced Washing Machine in Apt 4B (#8)
	if _complex.apt4b_washer_anomaly:
		_complex.apt4b_washer_anomaly.visible = false
		_complex.apt4b_washer_anomaly.position.y = -25.0

	# 9. Reset Cross Hall Ceiling Fixture (#9)
	if _complex.cross_swing_fixture:
		_complex.cross_swing_fixture.position = Vector3(8.2, 3.18, -3.6)
		_complex.cross_swing_fixture.rotation = Vector3.ZERO

	# 10. Reset Coat Rack Shape (#10)
	if _complex.apt4b_coat_normal and _complex.apt4b_coat_twisted:
		_complex.apt4b_coat_normal.visible = true
		_complex.apt4b_coat_normal.position.y = 0.0
		_complex.apt4b_coat_twisted.visible = false
		_complex.apt4b_coat_twisted.position.y = -25.0

	# 11-18. Stop Audio Anomaly Loops & Particles
	for lp in [_whisper_loop, _radio_loop, _breathing_loop]:
		if lp and lp.playing:
			lp.stop()
	if _radio_glow:
		_radio_glow.visible = false
		_radio_glow.light_energy = 0.0
	if _ceiling_dust:
		_ceiling_dust.emitting = false

	# 19, 25, 29. Reset Door Lock & Ajar States
	if _complex.doors.has("apt_4a"):
		var d4a: InteractableDoor = _complex.doors["apt_4a"]
		d4a.is_locked = false
		d4a.set_open_state(false, false)
	if _complex.doors.has("crawlspace_4b"):
		var dc: InteractableDoor = _complex.doors["crawlspace_4b"]
		dc.is_locked = true
		dc.lock_reason = "Padlocked utility hatch."
		dc.set_open_state(false, false)
	if _complex.doors.has("storage_b"):
		var dsb: InteractableDoor = _complex.doors["storage_b"]
		dsb.is_locked = true
		dsb.lock_reason = "Heavy municipal padlock seals Storage 4-B."
		dsb.set_open_state(false, false)

	# 20. Reset Corridor Length (#20)
	_complex.set_corridor_extended(false)

	# 21. Reset Apt 4C Arrangement (#21)
	_complex.set_apt4c_arrangement(0)

	# 22 & 16. Reset Elevator (#16, #22)
	if _complex.elevator:
		_complex.elevator.reset_normal()

	# 24. Reset Cross Hall Barricade (#24)
	if _complex.cross_barricade_root:
		_complex.cross_barricade_root.visible = false
		_complex.cross_barricade_root.position.y = -25.0

	# 26-30. Hide Entity Anomaly Actors
	for ent in [_corridor_figure, _shadow_walker, _window_watcher, _door_peeker, _silent_follower]:
		if ent:
			ent.visible = false
			ent.position = Vector3(0, -30, 0)


func activate_anomaly(anomaly_id: int) -> void:
	if anomaly_id < 1 or anomaly_id > 30:
		return
	if not active_anomalies.has(anomaly_id):
		active_anomalies.append(anomaly_id)

	match anomaly_id:
		1: # Painting changes (Coastal -> Hollow-Eyed Watcher Shader)
			if _complex.hero_painting_canvas:
				_complex.hero_painting_canvas.material_override = MaterialLibrary.get_watcher_canvas_mat()
		2: # Furniture moves in Apt 4A into eerie inward circle
			if _complex.apt4a_furniture_baseline.size() >= 4:
				var n0: Node3D = _complex.apt4a_furniture_baseline[0]["node"]
				var n1: Node3D = _complex.apt4a_furniture_baseline[1]["node"]
				var n2: Node3D = _complex.apt4a_furniture_baseline[2]["node"]
				var n3: Node3D = _complex.apt4a_furniture_baseline[3]["node"]
				n0.position = Vector3(-6.2, 0.0, 10.6)
				n0.rotation.y = deg_to_rad(32.0)
				n1.position = Vector3(-4.6, 0.0, 11.8)
				n1.rotation.y = deg_to_rad(-115.0)
				n2.position = Vector3(-6.2, 0.85, 10.6) # Stacked table!
				n2.rotation.y = deg_to_rad(45.0)
				n3.position = Vector3(-7.6, 0.0, 11.6)
		3: # Object disappears (Apt 4A Console TV vanishes)
			if _complex.apt4a_tv:
				_complex.apt4a_tv.visible = false
				_complex.apt4a_tv.position.y = -25.0
		4: # New door appears on West Main Corridor wall
			if _complex.blank_west_wall_plug and _complex.impossible_corridor_door:
				_complex.blank_west_wall_plug.visible = false
				_complex.blank_west_wall_plug.position.y = -25.0
				_complex.impossible_corridor_door.visible = true
				_complex.impossible_corridor_door.position.y = 0.0
		5: # Wallpaper changes across Main & Cross Corridors
			var m_rot := MaterialLibrary.get_pbr("wall_wallpaper_anomaly", 0.42)
			for wm in _complex.corridor_wall_meshes:
				if is_instance_valid(wm):
					wm.material_override = m_rot
		6: # Window reveals impossible crimson void view
			if _complex.apt4a_window_pane:
				_complex.apt4a_window_pane.material_override = MaterialLibrary.get_void_window_mat()
		7: # Hallway lighting changes to flickering sodium-cyan
			for l in _complex.corridor_lights:
				if is_instance_valid(l):
					l.light_color = Color(0.22, 0.88, 0.98)
					l.light_energy = 1.25
		8: # Object appears in wrong room (Washing Machine inside Apt 4B Bedroom!)
			if _complex.apt4b_washer_anomaly:
				_complex.apt4b_washer_anomaly.visible = true
				_complex.apt4b_washer_anomaly.position = Vector3(5.8, 0.0, 10.2)
		9: # Ceiling fixture detaches low & swings
			if _complex.cross_swing_fixture:
				_complex.cross_swing_fixture.position = Vector3(8.55, 2.25, -3.2)
		10: # Familiar object changes shape (Coat rack -> Osteoid rib sculpture)
			if _complex.apt4b_coat_normal and _complex.apt4b_coat_twisted:
				_complex.apt4b_coat_normal.visible = false
				_complex.apt4b_coat_normal.position.y = -25.0
				_complex.apt4b_coat_twisted.visible = true
				_complex.apt4b_coat_twisted.position = Vector3(3.1, 0.0, 7.2)
		11: # Footsteps behind player
			_trail_step_accum = 0.0
			if _player:
				_last_player_pos = _player.global_position
		12: # Whispering from Apt 4B wardrobe
			if _whisper_loop:
				_whisper_loop.play()
		13: # Knocking behind locked Storage 4-B door
			_knock_timer = 0.5
		14: # Radio interference in Apt 4C
			if _radio_loop:
				_radio_loop.play()
			if _radio_glow:
				_radio_glow.visible = true
				_radio_glow.light_energy = 1.6
		15: # Distant scream from Stairwell shaft
			_scream_timer = 1.2
		16: # Elevator sounds without movement
			if _complex.elevator:
				_complex.elevator.phantom_sound_anomaly = true
		17: # Repeated footsteps on upper floor overhead
			_upper_step_z = 11.0
			_upper_step_timer = 0.25
			_ceiling_dust.emitting = true
		18: # Unexplained close breathing in Laundry Room
			if _breathing_loop:
				_breathing_loop.play()
		19: # Door locks unexpectedly (Apartment 4A deadbolt engages on its own)
			_door19_triggered = false
			if _complex.doors.has("apt_4a"):
				var d4a: InteractableDoor = _complex.doors["apt_4a"]
				d4a.slam_and_lock("ANOMALOUS DEADBOLT ENGAGED BY ITSELF! Scan [R] to release.")
		20: # Corridor becomes longer (+12m North extension)
			_complex.set_corridor_extended(true)
		21: # Room arrangement changes (Apt 4C inverted layout)
			_complex.set_apt4c_arrangement(1)
		22: # Elevator arrives at wrong floor (-9)
			if _complex.elevator:
				_complex.elevator.wrong_floor_anomaly = true
		23: # Emergency lights activate across complex
			AudioDirector.trigger_intentional_silence(3.0)
			for l in _complex.corridor_lights:
				if is_instance_valid(l):
					l.light_energy = 0.42
			for b in _complex.emergency_beacons:
				b.visible = true
			for el in _complex.emergency_lights:
				el.visible = true
				el.light_energy = 1.85
		24: # Furniture blocks familiar East Cross-Hall passage
			if _complex.cross_barricade_root:
				_complex.cross_barricade_root.visible = true
				_complex.cross_barricade_root.position.y = 0.0
		25: # Previously locked East Surveillance Crawlspace opens ajar!
			if _complex.doors.has("crawlspace_4b"):
				var dc: InteractableDoor = _complex.doors["crawlspace_4b"]
				dc.is_locked = false
				dc.set_open_state(true, false, 42.0)
		26: # Figure appears at North Corridor end
			var end_z := -27.2 if _complex.current_corridor_extended else -15.4
			_corridor_figure.visible = true
			_corridor_figure.position = Vector3(0.0, 0.0, end_z)
			_corridor_figure.rotation.y = 0.0
			_figure26_stare_time = 0.0
		27: # Shadow crosses Laundry Room
			_shadow_walker.visible = true
			_shadow27_x = 7.2
			_shadow27_dir = 1.0
			_shadow_walker.position = Vector3(_shadow27_x, 0.0, 2.2)
		28: # Creature watches through 4th-floor window (Outside Apt 4B East Window)
			_window_watcher.visible = true
			_window_watcher.position = Vector3(10.78, 0.0, 11.8)
			_window_watcher.rotation.y = deg_to_rad(-90.0)
		29: # Entity appears behind partially open Storage 4-B door
			if _complex.doors.has("storage_b"):
				var dsb: InteractableDoor = _complex.doors["storage_b"]
				dsb.set_open_state(true, false, -24.0)
			_door_peeker.visible = true
			_door_peeker.position = Vector3(10.72, 0.0, -5.68)
			_door_peeker.rotation.y = deg_to_rad(15.0)
		30: # Creature silently follows player
			_silent_follower.visible = true
			var start_p := Vector3(0.0, 0.0, -9.5)
			if _player and start_p.distance_to(_player.global_position) < 7.0:
				start_p = Vector3(0.0, 0.0, 12.5)
			_silent_follower.position = start_p


# ============================================================
# PER-FRAME LIVE BEHAVIORS & GAMEPLAY CONSEQUENCES
# ============================================================
func _process(delta: float) -> void:
	_time += delta
	if active_anomalies.is_empty() or _complex == null:
		return

	# #7 Flickering cyan light
	if active_anomalies.has(7):
		var flick := 1.15 + 0.35 * sin(_time * 13.0) * cos(_time * 29.0)
		for l in _complex.corridor_lights:
			if is_instance_valid(l):
				l.light_energy = maxf(0.45, flick)

	# #9 Swinging detached fluorescent fixture
	if active_anomalies.has(9) and _complex.cross_swing_fixture:
		_complex.cross_swing_fixture.rotation.z = sin(_time * 2.35) * 0.36
		_complex.cross_swing_fixture.rotation.x = cos(_time * 1.7) * 0.14

	# #11 Footsteps directly behind player
	if active_anomalies.has(11) and _player != null:
		var moved := _player.global_position.distance_to(_last_player_pos)
		_last_player_pos = _player.global_position
		if moved > 0.015:
			_trail_step_accum += moved
			if _trail_step_accum >= 1.55:
				_trail_step_accum = 0.0
				var back_dir := _player.global_transform.basis.z.normalized()
				var phantom_pos := _player.global_position + back_dir * 2.15
				AudioDirector.play_footstep("wood", phantom_pos, -1.5, 0.90)

	# #13 Knocking behind locked Storage 4-B door
	if active_anomalies.has(13):
		_knock_timer -= delta
		if _knock_timer <= 0.0:
			_knock_timer = randf_range(4.2, 6.4)
			var dpos := Vector3(10.5, 1.3, -5.25)
			AudioDirector.play_3d("anom_knock_3x", dpos, 17.0, 1.0)

	# #14 Pulsing radio interference dial
	if active_anomalies.has(14) and _radio_glow:
		_radio_glow.light_energy = 1.2 + 0.8 * absf(sin(_time * 9.5))

	# #15 Distant stairwell scream
	if active_anomalies.has(15):
		_scream_timer -= delta
		if _scream_timer <= 0.0:
			_scream_timer = randf_range(8.5, 12.5)
			AudioDirector.play_3d("anom_distant_scream", Vector3(-7.8, -2.5, 2.2), 24.0, 0.5)

	# #17 Pacing footsteps on upper floor overhead + dust
	if active_anomalies.has(17):
		_upper_step_timer -= delta
		if _upper_step_timer <= 0.0:
			_upper_step_timer = 0.62
			_upper_step_z += _upper_step_dir * 1.35
			if _upper_step_z < -10.0:
				_upper_step_dir = 1.0
			elif _upper_step_z > 12.0:
				_upper_step_dir = -1.0
			var up_pos := Vector3(0.0, 3.35, _upper_step_z)
			_ceiling_dust.global_position = Vector3(0.0, 3.12, _upper_step_z)
			AudioDirector.play_footstep("wood", up_pos, 0.0, 0.78)

	# #19 Door slams & locks if player walks inside Apt 4A
	if active_anomalies.has(19) and _player != null and not _door19_triggered:
		if _player.global_position.x < -2.6 and _player.global_position.z > 6.0 and _player.global_position.z < 14.0:
			_door19_triggered = true
			var d4a: InteractableDoor = _complex.doors["apt_4a"]
			d4a.slam_and_lock("THE DOOR SLAMMED & LOCKED ITSELF! Scan [R] to break the seal!")

	# #23 Pulsing Crimson Emergency Lights
	if active_anomalies.has(23):
		var pulse := 0.4 + 1.8 * (0.5 + 0.5 * sin(_time * 4.6))
		for el in _complex.emergency_lights:
			el.light_energy = pulse

	# #26 Corridor End Figure consequence if approached recklessly without scanning
	if active_anomalies.has(26) and _corridor_figure.visible and _player != null:
		var d26 := _player.global_position.distance_to(_corridor_figure.global_position)
		if d26 < 4.8:
			_corridor_figure.visible = false
			AudioDirector.play_3d("entity_observer_sting", _player.global_position, 18.0, 1.5)
			GameState.damage_player(16.0, "CORRIDOR APPARITION DISSOLVED INTO STATIC!")

	# #27 Shadow strides across Laundry Room
	if active_anomalies.has(27) and _shadow_walker.visible:
		_shadow27_x += _shadow27_dir * delta * 2.35
		if _shadow27_x > 13.8:
			_shadow27_dir = -1.0
			AudioDirector.play_3d("floor_creak_01", _shadow_walker.global_position, 14.0, -2.0)
		elif _shadow27_x < 7.0:
			_shadow27_dir = 1.0
			AudioDirector.play_3d("floor_creak_02", _shadow_walker.global_position, 14.0, -2.0)
		_shadow_walker.position = Vector3(_shadow27_x, 0.0, 2.2)
		_shadow_walker.rotation.y = deg_to_rad(90.0 if _shadow27_dir > 0 else -90.0)

	# #28 Window Watcher subtly tilts head toward player
	if active_anomalies.has(28) and _window_watcher.visible and _player != null:
		_window_watcher.rotation.z = sin(_time * 1.4) * 0.10

	# #29 Door-Gap Entity slams door shut if approached too closely without scanning
	if active_anomalies.has(29) and _door_peeker.visible and _player != null:
		if _player.global_position.distance_to(_door_peeker.global_position) < 2.1:
			_door_peeker.visible = false
			var dsb: InteractableDoor = _complex.doors["storage_b"]
			dsb.slam_and_lock("SOMETHING INSIDE STORAGE 4-B SLAMMED THE DOOR!")
			GameState.damage_player(14.0, "STARTLED BY THRESHOLD PEEKER!")

	# #30 Silent Follower stalks behind player, halting when player turns toward it
	if active_anomalies.has(30) and _silent_follower.visible and _player != null:
		var cam: Camera3D = _player.get("camera")
		var fwd := (-cam.global_transform.basis.z) if cam else (-_player.global_transform.basis.z)
		var to_ent := (_silent_follower.global_position - _player.global_position).normalized()
		var player_looking := fwd.dot(to_ent) > 0.30
		if not player_looking and _silent_follower.global_position.distance_to(_player.global_position) > 4.2:
			_follower_repath -= delta
			if _follower_repath <= 0.0 or _follower_path.is_empty():
				_follower_repath = 0.7
				_follower_path = _complex.find_nav_path(_silent_follower.global_position, _player.global_position)
			if not _follower_path.is_empty():
				var nxt := _follower_path[0]
				var step := Vector3(nxt.x - _silent_follower.global_position.x, 0, nxt.z - _silent_follower.global_position.z)
				if step.length() < 0.5:
					_follower_path.pop_front()
				else:
					var dir := step.normalized()
					_silent_follower.global_position += dir * delta * 2.85
					_silent_follower.look_at(_silent_follower.global_position + dir, Vector3.UP)


# ============================================================
# ANOMALY DETECTION & SCANNER VERIFICATION LOGIC (Sec 6 & Sec 7)
# ============================================================
func get_anomaly_anchor_and_radius(anomaly_id: int) -> Dictionary:
	# Returns {pos: Vector3, radius: float, needs_aim: bool} for accurate scanner detection
	match anomaly_id:
		1: return {"pos": Vector3(1.48, 1.62, 6.4), "radius": 8.5, "needs_aim": true}
		2: return {"pos": Vector3(-6.0, 0.8, 11.2), "radius": 8.5, "needs_aim": true}
		3: return {"pos": Vector3(-6.2, 0.7, 8.2), "radius": 8.0, "needs_aim": true}
		4: return {"pos": Vector3(-1.6, 1.1, 3.8), "radius": 9.0, "needs_aim": true}
		5: return {"pos": _player.global_position if _player else Vector3(0, 1.5, 4.0), "radius": 18.0, "needs_aim": false}
		6: return {"pos": Vector3(-10.3, 1.65, 9.8), "radius": 10.0, "needs_aim": true}
		7: return {"pos": Vector3(0.0, 2.8, 4.0), "radius": 20.0, "needs_aim": false}
		8: return {"pos": Vector3(5.8, 0.6, 10.2), "radius": 8.5, "needs_aim": true}
		9: return {"pos": Vector3(8.5, 2.3, -3.2), "radius": 11.0, "needs_aim": true}
		10: return {"pos": Vector3(3.1, 1.0, 7.2), "radius": 8.0, "needs_aim": true}
		11: return {"pos": _player.global_position if _player else Vector3(0, 1.0, 5.0), "radius": 25.0, "needs_aim": false}
		12: return {"pos": Vector3(4.5, 1.35, 13.8), "radius": 7.5, "needs_aim": false}
		13: return {"pos": Vector3(10.5, 1.3, -5.2), "radius": 8.5, "needs_aim": false}
		14: return {"pos": Vector3(-7.6, 0.9, -12.0), "radius": 8.5, "needs_aim": false}
		15: return {"pos": Vector3(-7.8, 0.0, -0.5), "radius": 10.5, "needs_aim": false}
		16: return {"pos": Vector3(3.5, 1.2, 0.8), "radius": 8.5, "needs_aim": false}
		17: return {"pos": Vector3(0.0, 3.1, _upper_step_z), "radius": 11.0, "needs_aim": false}
		18: return {"pos": Vector3(14.3, 1.45, 4.0), "radius": 7.5, "needs_aim": false}
		19: return {"pos": Vector3(-1.6, 1.1, 10.0), "radius": 7.5, "needs_aim": true}
		20: return {"pos": Vector3(0.0, 1.5, -20.0), "radius": 14.0, "needs_aim": false}
		21: return {"pos": Vector3(-7.2, 1.0, -9.8), "radius": 8.5, "needs_aim": false}
		22: return {"pos": Vector3(4.2, 1.2, 0.8), "radius": 7.5, "needs_aim": false}
		23: return {"pos": _player.global_position if _player else Vector3.ZERO, "radius": 35.0, "needs_aim": false}
		24: return {"pos": Vector3(12.6, 1.0, -3.6), "radius": 9.5, "needs_aim": true}
		25: return {"pos": Vector3(10.5, 1.1, 7.2), "radius": 8.5, "needs_aim": true}
		26: return {"pos": _corridor_figure.global_position + Vector3(0, 1.3, 0), "radius": 22.0, "needs_aim": true}
		27: return {"pos": _shadow_walker.global_position + Vector3(0, 1.2, 0), "radius": 11.0, "needs_aim": true}
		28: return {"pos": Vector3(10.6, 1.5, 11.8), "radius": 9.5, "needs_aim": true}
		29: return {"pos": Vector3(10.5, 1.3, -5.4), "radius": 9.5, "needs_aim": true}
		30: return {"pos": _silent_follower.global_position + Vector3(0, 1.2, 0), "radius": 16.0, "needs_aim": true}
	return {"pos": Vector3.ZERO, "radius": 6.0, "needs_aim": true}


func evaluate_scanner_attempt(cam_pos: Vector3, cam_forward: Vector3) -> Dictionary:
	# Checks if any active unverified anomaly satisfies spatial & gaze detection conditions
	var fwd := cam_forward.normalized()
	for aid in active_anomalies:
		if verified_in_current_cycle.get(aid, false):
			continue
		var spec := get_anomaly_anchor_and_radius(aid)
		var apos: Vector3 = spec["pos"]
		var rad: float = spec["radius"]
		var dist := cam_pos.distance_to(apos)
		if dist > rad:
			continue

		if bool(spec["needs_aim"]):
			var dir_to := (apos - cam_pos).normalized()
			if fwd.dot(dir_to) < 0.72: # ~44 deg scanner cone
				continue

		# Validated! Mark verified, neutralize trap locks (#19) if needed, and reward player
		verified_in_current_cycle[aid] = true
		if aid == 19 and _complex.doors.has("apt_4a"):
			(_complex.doors["apt_4a"] as InteractableDoor).is_locked = false
		GameState.mark_anomaly_discovered(aid)
		anomaly_verified_by_scanner.emit(aid)
		var meta := GameState.get_anomaly_meta(aid)
		return {
			"success": true,
			"anomaly_id": aid,
			"meta": meta,
		}

	return {"success": false}
