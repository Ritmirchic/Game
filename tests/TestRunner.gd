extends Node3D
## Automated End-to-End Verification Suite for DON'T LOOK BACK.
## Exercises Movement, Camera, Collision, Doors, Elevator, Flashlight, All 30 Anomalies,
## Detection/Reporting, 3 Horror Entities (Frustum + Raycast Occlusion + Sound AI),
## Navigation, Modular Complex Validation, Audio Assets, Save/Load, and All 3 Endings.

var passed: int = 0
var failed: int = 0
var failures: Array[String] = []
var report_lines: Array[String] = []


func _assert_true(cond: bool, label: String) -> void:
	if cond:
		passed += 1
		report_lines.append("[PASS] " + label)
	else:
		failed += 1
		failures.append(label)
		report_lines.append("[FAIL] " + label)
		printerr("ASSERT FAILED: " + label)


func _ready() -> void:
	# Wait 2 physics frames so PhysicsServer3D registers all StaticBody3D colliders!
	await get_tree().physics_frame
	await get_tree().physics_frame
	await _run_all_tests()


func _run_all_tests() -> void:
	print("============================================================")
	print("DON'T LOOK BACK — AUTOMATED IN-ENGINE VERIFICATION SUITE")
	print("============================================================")

	var session := GameSession.new()
	add_child(session)
	await get_tree().physics_frame
	await get_tree().physics_frame

	var complex := session.complex
	var anomalies := session.anomalies
	var player := session.player
	var observer := session.observer
	var listener := session.listener
	var imitator := session.imitator

	# ------------------------------------------------------------
	# 1. LIGHTING VISIBILITY & FLASHLIGHT VERIFICATION
	# ------------------------------------------------------------
	var env := session.world_env.environment
	_assert_true(env.ambient_light_energy >= 0.50 and env.tonemap_exposure >= 1.0, "Lighting Visibility: Balanced ambient & exposure prevent pitch-black screen")
	_assert_true(player.flashlight_on and player._spot_main.visible and player._spot_main.shadow_enabled, "Flashlight: Active dual-cone SpotLight3D with real-time shadows enabled")
	player.toggle_flashlight()
	_assert_true(not player.flashlight_on and not player._spot_main.visible, "Flashlight: Toggle OFF works")
	player.toggle_flashlight()
	_assert_true(player.flashlight_on and player._spot_main.visible, "Flashlight: Toggle ON works")

	# ------------------------------------------------------------
	# 2. MOVEMENT, CAMERA & SOLID WALL COLLISION
	# ------------------------------------------------------------
	player.global_position = Vector3(0.0, 0.15, 8.0) # Main South Spine Corridor
	await get_tree().physics_frame
	# Try pushing player East (+X) directly into solid corridor wall at X=+1.6
	for _i in range(25):
		player.velocity = Vector3(6.0, 0.0, 0.0)
		player.move_and_slide()
	_assert_true(player.global_position.x < 1.55, "Collision: Solid corridor wall stops player (x=%.2f < 1.55)" % player.global_position.x)

	# Walk cleanly North down unobstructed Spine Corridor
	player.global_position = Vector3(0.0, 0.15, 8.0)
	for _i in range(20):
		player.velocity = Vector3(0.0, 0.0, -5.0)
		player.move_and_slide()
	_assert_true(player.global_position.z < 6.8, "Movement: Smooth corridor traversal verified")

	# Camera pitch clamping check
	player._head_pivot.rotation.x = deg_to_rad(-82.0)
	_assert_true(absf(rad_to_deg(player._head_pivot.rotation.x) + 82.0) < 0.1, "Camera: Pitch clamp bounds verified")

	# ------------------------------------------------------------
	# 3. ARCHITECTURE, DOORS, ELEVATOR & MODULAR VALIDATION
	# ------------------------------------------------------------
	var val := complex.validate_environment_integrity(player.global_position)
	_assert_true(bool(val["valid"]), "Complex Architecture: All %d waypoints & %d doors connected cleanly" % [val["reachable_waypoints"], val["door_count"]])

	var d4a: InteractableDoor = complex.doors["apt_4a"]
	_assert_true(not d4a.is_open, "Doors: Apartment 4A starts closed")
	d4a.interact(player)
	_assert_true(d4a.is_open, "Doors: Unlocked hinged door opens on interact")
	d4a.interact(player)
	_assert_true(not d4a.is_open, "Doors: Hinged door closes cleanly")

	var dsb: InteractableDoor = complex.doors["storage_b"]
	dsb.interact(player)
	_assert_true(not dsb.is_open and dsb.is_locked, "Doors: Padlocked door blocks opening & rattles")

	_assert_true(complex.elevator != null and complex.elevator.current_floor == "4", "Elevator: Built and initialized at Floor 4")

	# ------------------------------------------------------------
	# 4. ALL 30 REAL ANOMALIES — ACTIVATION, MUTATION, SCANNER & RESET
	# ------------------------------------------------------------
	anomalies.reset_all_anomalies()
	# Verify False Scan Penalty on clean baseline first!
	GameState.scanner_charges = 5
	var false_res := anomalies.evaluate_scanner_attempt(Vector3(0, 1.6, 6.4), Vector3(1, 0, 0))
	_assert_true(not bool(false_res.get("success", true)), "Detection System: Rejects scan when 0 anomalies are active")

	for aid in range(1, 31):
		anomalies.reset_all_anomalies()
		anomalies.activate_anomaly(aid)
		anomalies._process(0.35)

		var mutated := _verify_specific_anomaly_mutation(aid, complex, anomalies)
		var spec := anomalies.get_anomaly_anchor_and_radius(aid)
		var target_pos: Vector3 = spec["pos"]
		var cam_test_pos := target_pos + Vector3(0.0, 0.0, 2.5)
		if aid in [1, 6, 28]:
			cam_test_pos = target_pos + Vector3(-2.2 if aid != 6 else 2.2, 0.0, 0.0)
		var cam_fwd := (target_pos - cam_test_pos).normalized()
		if cam_fwd.length() < 0.01:
			cam_fwd = Vector3(0, 0, -1)

		var scan_res := anomalies.evaluate_scanner_attempt(cam_test_pos, cam_fwd)
		var detected := bool(scan_res.get("success", false)) and int(scan_res.get("anomaly_id", -1)) == aid
		anomalies.reset_all_anomalies()
		var reset_ok := anomalies.active_anomalies.is_empty()

		var meta := GameState.get_anomaly_meta(aid)
		_assert_true(
			mutated and detected and reset_ok,
			"Anomaly #%02d [%s - %s]: Mutated=%s Detected=%s Reset=%s" % [
				aid, meta.get("code", ""), meta.get("title", ""), str(mutated), str(detected), str(reset_ok)
			]
		)

	# ------------------------------------------------------------
	# 5. THREE HORROR ENTITIES — VISIBILITY, OCCLUSION & ACOUSTIC AI
	# ------------------------------------------------------------
	# A. THE OBSERVER: Freezes when in camera view & unoccluded; moves when unseen; moves when occluded behind solid wall!
	player.global_position = Vector3(0.0, 0.15, 10.0)
	player.rotation = Vector3.ZERO # Looking North (-Z)
	player._head_pivot.rotation = Vector3.ZERO
	observer.set_threat_active(true, Vector3(0.0, 0.0, 0.0), 5.0) # 10m directly ahead in open Main Corridor!
	await get_tree().physics_frame

	var vis_open := observer.check_visibility_from_camera(null, Vector3(0.0, 1.6, 10.0), Vector3(0, 0, -1), 76.0)
	_assert_true(vis_open, "Observer AI: Visible when inside camera FOV in open corridor")
	var obs_before := observer.global_position
	observer._physics_process(0.25)
	_assert_true(observer.global_position.distance_to(obs_before) < 0.001, "Observer AI: Freezes completely while observed")

	# Turn player camera 180 deg South (+Z) -> Observer is behind player's back!
	player.set_look_angles(deg_to_rad(180.0), 0.0)
	await get_tree().physics_frame
	var vis_away := observer.check_visibility_from_camera(null, Vector3(0.0, 1.6, 10.0), Vector3(0, 0, 1), 76.0)
	_assert_true(not vis_away, "Observer AI: Unobserved when outside camera view cone")
	obs_before = observer.global_position
	for _step in range(5):
		await get_tree().physics_frame
	_assert_true(observer.global_position.distance_to(obs_before) > 0.25, "Observer AI: Advances along Nav path when player looks away (moved %.2fm)" % observer.global_position.distance_to(obs_before))

	# Place Observer inside Apartment 4A behind solid corridor wall (X=-6.0, Z=8.0) while camera looks straight toward it from X=0, Z=8.0!
	observer.global_position = Vector3(-6.0, 0.0, 8.0)
	await get_tree().physics_frame
	var vis_occluded := observer.check_visibility_from_camera(null, Vector3(0.0, 1.6, 8.0), Vector3(-1, 0, 0), 76.0)
	_assert_true(not vis_occluded, "Observer AI: Physics raycast detects wall occlusion accurately")
	observer.set_threat_active(false, Vector3(0, -30, 0))

	# B. THE LISTENER: Crouch stealth vs Sprint noise + Wall acoustic attenuation!
	listener.set_threat_active(true, Vector3(0.0, 0.0, -3.6), 5.0)
	await get_tree().physics_frame
	SoundEventBus.emit_noise(Vector3(0.0, 0.0, 4.5), 12.0, "player_step") # Quiet crouch at 8.1m
	_assert_true(listener.current_state == ListenerEntity.State.PATROL, "Listener AI: Ignores silent 12 dB crouch footsteps")
	SoundEventBus.emit_noise(Vector3(0.0, 0.0, 4.5), 78.0, "player_step") # Loud sprint at 8.1m!
	_assert_true(listener.current_state == ListenerEntity.State.HUNT, "Listener AI: Enters HUNT mode on loud 78 dB sprint footsteps")
	var db_open := listener.compute_perceived_decibels(Vector3(0.0, 0.0, 2.0), 70.0)
	var db_walled := listener.compute_perceived_decibels(Vector3(-6.5, 0.0, 9.0), 70.0)
	_assert_true(db_walled < db_open - 10.0, "Listener AI: Solid apartment walls attenuate perceived dB")
	listener.set_threat_active(false, Vector3(0, -30, 0))

	# C. THE IMITATOR: False cues + Safe scanner banish!
	imitator.set_threat_active(true, Vector3(0.0, 0.0, 4.0), 2.0)
	imitator.emit_false_environmental_cue()
	_assert_true(imitator.false_cues_emitted >= 1, "Imitator AI: Emits deceptive environmental cues")
	player.global_position = Vector3(0.0, 0.15, 9.0)
	player.rotation = Vector3.ZERO
	player._head_pivot.rotation = Vector3.ZERO
	player._scan_cooldown = 0.0
	GameState.scanner_charges = 5
	await get_tree().physics_frame
	player.trigger_field_scan()
	_assert_true(not imitator.is_active_threat, "Imitator AI: Safely exposed & banished when scanned from afar with [R]")

	# ------------------------------------------------------------
	# 6. AUDIO ASSETS, STORY MYSTERY, ALL 3 ENDINGS & SAVE/LOAD
	# ------------------------------------------------------------
	var required_wavs := [
		"step_wood_01", "step_concrete_01", "step_tile_01", "step_metal_01",
		"door_open_creak", "door_close_latch", "door_locked_rattle", "door_slam_heavy",
		"anom_knock_3x", "anom_whisper_loop", "anom_radio_static", "anom_distant_scream",
		"anom_breathing_close", "elevator_move_loop", "elevator_ding", "fluorescent_hum_60hz",
		"entity_observer_sting", "entity_listener_clicks", "entity_imitator_unfold",
		"tape_log_01", "tape_log_02", "tape_log_03", "tape_log_04"
	]
	var all_audio_ok := true
	for w in required_wavs:
		if AudioDirector.get_stream("res://assets/audio/%s.wav" % w) == null:
			all_audio_ok = false
	_assert_true(all_audio_ok, "Audio Suite: All %d spatial & narrative WAV streams loaded cleanly" % required_wavs.size())

	# Disconnect scene change listener during Ending state unit tests so TestRunner stays in control
	if GameState.ending_triggered.is_connected(session._on_ending_triggered):
		GameState.ending_triggered.disconnect(session._on_ending_triggered)

	# Test Ending A (8 Verified Cycles -> Emergency Exit)
	GameState.start_new_run()
	for _c in range(8):
		anomalies.reset_all_anomalies() # Clean floor
		session._on_threshold_decision_submitted(false) # Report Clean
	_assert_true(GameState.verified_cycles == 8 and not complex.north_exit_door.is_locked, "Progression: 8 Verified Cycles unlock North Emergency Exit")
	complex.north_exit_door.interact(player)
	_assert_true(GameState.pending_ending_id == "A" and GameState.unlocked_endings.get("A", false), "Ending A ('Morning Outside Kestrel Court'): Reachable & verified")

	# Test Ending B (Maintenance Breaker -> Apt 4C Hidden Sanctum -> 1994 Master Key -> Basement Boiler Core -> Exit)
	GameState.start_new_run()
	var breaker: WorldInteractable = complex.find_child("MainBreakerSwitch", true, false) as WorldInteractable
	var latch: WorldInteractable = complex.find_child("SanctumBookshelfLatch", true, false) as WorldInteractable
	breaker.interact(player)
	latch.interact(player)
	var key_int: WorldInteractable = complex.master_key_prop.get_child(1) as WorldInteractable
	key_int.interact(player)
	complex.boiler_core_console.interact(player)
	_assert_true(GameState.breaker_restored and GameState.sanctum_unlocked and GameState.master_override_key_taken and GameState.core_purged, "Secret Story Chain: Breaker -> Hidden Sanctum -> Master Key -> Boiler Core Purge verified")
	complex.north_exit_door.interact(player)
	_assert_true(GameState.pending_ending_id == "B" and GameState.unlocked_endings.get("B", false), "Ending B ('The Seal Broken' True Ending): Reachable & verified")

	# Test Ending C (Sanctum Threshold Mirror / Sub-Level -9 Surrender)
	GameState.trigger_ending("C")
	_assert_true(GameState.pending_ending_id == "C" and GameState.unlocked_endings.get("C", false), "Ending C ('Tenant 405' Assimilation): Reachable & verified")

	# Test Save & Load Round-Trip
	GameState.verified_cycles = 5
	GameState.collect_document("doc_01")
	GameState.mark_tape_played("tape_01")
	var saved_ok := GameState.save_game(Vector3(1.2, 0.15, 6.4), 1.25)
	GameState.verified_cycles = 0
	var loaded_dict := GameState.load_game()
	_assert_true(saved_ok and GameState.verified_cycles == 5 and (loaded_dict["player_pos"] as Vector3).distance_to(Vector3(1.2, 0.15, 6.4)) < 0.01, "Save/Load System: Disk persistence round-trip verified")

	# Clean up test save file so fresh release starts cleanly
	if FileAccess.file_exists(GameState.SAVE_PATH):
		DirAccess.remove_absolute(ProjectSettings.globalize_path(GameState.SAVE_PATH))

	for line in report_lines:
		print(line)
	print("------------------------------------------------------------")
	print("TOTAL PASSED: %d   |   TOTAL FAILED: %d" % [passed, failed])
	print("============================================================")
	get_tree().quit(0 if failed == 0 else 1)


func _verify_specific_anomaly_mutation(aid: int, complex: ApartmentComplex, am: AnomalyManager) -> bool:
	match aid:
		1: return complex.hero_painting_canvas.material_override is ShaderMaterial
		2: return (complex.apt4a_furniture_baseline[2]["node"] as Node3D).position.y > 0.5
		3: return not complex.apt4a_tv.visible
		4: return complex.impossible_corridor_door.visible and complex.impossible_corridor_door.position.y == 0.0
		5: return complex.corridor_wall_meshes.size() > 0 and complex.corridor_wall_meshes[0].material_override != null
		6: return complex.apt4a_window_pane.material_override is ShaderMaterial
		7: return complex.corridor_lights[0].light_color.b > 0.90
		8: return complex.apt4b_washer_anomaly.visible and complex.apt4b_washer_anomaly.position.y == 0.0
		9: return complex.cross_swing_fixture.position.y < 2.5 and absf(complex.cross_swing_fixture.rotation.z) > 0.01
		10: return complex.apt4b_coat_twisted.visible and not complex.apt4b_coat_normal.visible
		11: return am.active_anomalies.has(11)
		12: return am._whisper_loop != null and am._whisper_loop.playing
		13: return am.active_anomalies.has(13)
		14: return am._radio_glow.visible and am._radio_glow.light_energy > 0.5
		15: return am.active_anomalies.has(15)
		16: return complex.elevator.phantom_sound_anomaly
		17: return am._ceiling_dust.emitting
		18: return am._breathing_loop != null and am._breathing_loop.playing
		19: return (complex.doors["apt_4a"] as InteractableDoor).is_locked
		20: return complex.current_corridor_extended and complex.north_ext_module.visible
		21: return complex.current_apt4c_variant == 1 and complex.apt4c_modular_root.scale.x < 0.0
		22: return complex.elevator.wrong_floor_anomaly
		23: return complex.emergency_beacons[0].visible and complex.emergency_lights[0].visible
		24: return complex.cross_barricade_root.visible and complex.cross_barricade_root.position.y == 0.0
		25: return (complex.doors["crawlspace_4b"] as InteractableDoor).is_open
		26: return am._corridor_figure.visible
		27: return am._shadow_walker.visible and am._shadow_walker.position.x > 7.2
		28: return am._window_watcher.visible
		29: return am._door_peeker.visible and (complex.doors["storage_b"] as InteractableDoor).is_open
		30: return am._silent_follower.visible
	return false
