class_name GameSession
extends Node3D
## Master Gameplay Director orchestrating WorldEnvironment, ApartmentComplex,
## 30-Anomaly Cycle Progression, 3 Horror Entities, Save/Load, and All 3 Endings.

static var load_saved_on_entry: bool = false

var world_env: WorldEnvironment
var complex: ApartmentComplex
var anomalies: AnomalyManager
var player: PlayerController
var observer: ObserverEntity
var listener: ListenerEntity
var imitator: ImitatorEntity
var hud: GameHUD

var _recent_anomaly_history: Array[int] = []


func _ready() -> void:
	if DisplayServer.get_name() != "headless":
		Input.set_mouse_mode(Input.MOUSE_MODE_CAPTURED)

	_setup_world_environment()

	complex = ApartmentComplex.new()
	complex.name = "ApartmentComplex"
	add_child(complex)
	complex.build_full_complex()

	anomalies = AnomalyManager.new()
	anomalies.name = "AnomalyManager"
	add_child(anomalies)

	player = PlayerController.new()
	player.name = "Player"
	player.position = Vector3(-4.2, 0.15, 11.2) # Wake up inside Apartment 4A!
	player.rotation.y = deg_to_rad(-90.0)
	add_child(player)

	observer = ObserverEntity.new()
	observer.name = "TheObserver"
	add_child(observer)
	observer.setup(complex, player)

	listener = ListenerEntity.new()
	listener.name = "TheListener"
	add_child(listener)
	listener.setup(complex, player)

	imitator = ImitatorEntity.new()
	imitator.name = "TheImitator"
	add_child(imitator)
	imitator.setup(complex, player)

	anomalies.setup(complex, player)
	player.setup(complex, anomalies, [observer, listener, imitator])

	hud = GameHUD.new()
	hud.name = "GameHUD"
	add_child(hud)
	hud.setup(player, complex)

	complex.threshold_decision_submitted.connect(_on_threshold_decision_submitted)
	hud.request_save_game.connect(_on_save_requested)
	hud.request_return_main_menu.connect(_on_return_main_menu)
	GameState.state_updated.connect(_apply_environment_settings)
	GameState.ending_triggered.connect(_on_ending_triggered)

	AudioDirector.start_ambience("res://assets/audio/ambience_building.wav", -8.5)

	if load_saved_on_entry and GameState.has_save_file():
		load_saved_on_entry = false
		var saved := GameState.load_game()
		if saved.has("player_pos"):
			player.global_position = saved["player_pos"]
			player.set_look_angles(float(saved.get("player_yaw", 0.0)), 0.0)
		_setup_cycle_state(GameState.verified_cycles == 0)
		GameState.notification_posted.emit("INSPECTION SHIFT RESTORED — CYCLE %d / 8" % GameState.verified_cycles, ThemeHelper.ACCENT_AMBER)
	else:
		load_saved_on_entry = false
		GameState.start_new_run()
		_setup_cycle_state(true)
		GameState.notification_posted.emit("03:14 AM — APARTMENT 4A. REPORT TO SOUTH THRESHOLD DESK.", ThemeHelper.ACCENT_AMBER)


func _setup_world_environment() -> void:
	world_env = WorldEnvironment.new()
	world_env.name = "WorldEnvironment"
	var env := Environment.new()
	env.background_mode = Environment.BG_COLOR
	env.background_color = Color(0.03, 0.035, 0.045)

	# Balanced Ambient Illumination — GUARANTEES THE GAME NEVER TURNS PITCH BLACK!
	env.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	env.ambient_light_color = Color(0.20, 0.21, 0.25)
	env.ambient_light_energy = 0.62

	env.tonemap_mode = Environment.TONE_MAPPER_ACES
	env.tonemap_exposure = float(GameState.settings.get("brightness", 1.18))
	env.tonemap_white = 6.0

	# Careful atmospheric depth & volumetric fog (subtle depth shafts, high clarity!)
	env.fog_enabled = true
	env.fog_light_color = Color(0.11, 0.12, 0.15)
	env.fog_density = 0.0065

	world_env.environment = env
	add_child(world_env)
	_apply_environment_settings()


func _apply_environment_settings() -> void:
	if world_env == null or world_env.environment == null:
		return
	var env := world_env.environment
	env.tonemap_exposure = float(GameState.settings.get("brightness", 1.18))
	var q: int = int(GameState.settings.get("quality_preset", 2))
	env.glow_enabled = q >= 1
	env.glow_intensity = 0.45
	env.ssao_enabled = q >= 2
	env.ssr_enabled = q >= 2
	env.volumetric_fog_enabled = q >= 2
	env.volumetric_fog_density = 0.012

	# If Core Purged (True Ending B Finale), unleash Observer + Listener for intense escape run!
	if GameState.core_purged and observer and listener:
		if not observer.is_active_threat:
			observer.set_threat_active(true, Vector3(0.0, 0.0, 8.0), 9.5)
		if not listener.is_active_threat:
			listener.set_threat_active(true, Vector3(-6.5, 0.0, -3.6), 9.5)
		complex.update_progress_board(false)


func _setup_cycle_state(force_clean_baseline: bool = false) -> void:
	anomalies.reset_all_anomalies()
	observer.set_threat_active(false, Vector3(0, -30, 0))
	listener.set_threat_active(false, Vector3(0, -30, 0))
	imitator.set_threat_active(false, Vector3(0, -30, 0))

	if force_clean_baseline:
		complex.update_progress_board(true)
		complex.validate_environment_integrity(player.global_position)
		return

	var difficulty := GameState.verified_cycles
	var has_anomaly := randf() < 0.72
	if has_anomaly:
		var count := 2 if (difficulty >= 5 and randf() < 0.45) else 1
		for _i in range(count):
			var aid := _pick_next_anomaly()
			anomalies.activate_anomaly(aid)

	# Escalating Entity AI Presence on Cycles 2+
	if difficulty >= 2 and randf() < (0.35 + difficulty * 0.06):
		imitator.set_threat_active(true, Vector3(1.15, 0.0, 2.8), 8.5)
	if difficulty >= 3 and randf() < (0.32 + difficulty * 0.06):
		listener.set_threat_active(true, Vector3(-7.0, 0.0, -3.6), 10.0)
	if difficulty >= 4 and randf() < (0.35 + difficulty * 0.07):
		observer.set_threat_active(true, Vector3(0.0, 0.0, -12.0), 10.5)

	complex.update_progress_board(false)
	var check := complex.validate_environment_integrity(player.global_position)
	if not bool(check.get("player_safe", true)):
		player.global_position = Vector3(0.0, 0.15, 17.2)


func _pick_next_anomaly() -> int:
	var pool: Array[int] = []
	for id in range(1, 31):
		if not _recent_anomaly_history.has(id) and not anomalies.active_anomalies.has(id):
			pool.append(id)
	if pool.is_empty():
		_recent_anomaly_history.clear()
		for id in range(1, 31):
			pool.append(id)
	var chosen: int = pool[randi() % pool.size()]
	_recent_anomaly_history.append(chosen)
	if _recent_anomaly_history.size() > 14:
		_recent_anomaly_history.pop_front()
	return chosen


func _on_threshold_decision_submitted(reported_anomaly: bool) -> void:
	GameState.total_loops += 1
	var actual_has_anomaly := not anomalies.active_anomalies.is_empty()
	var all_already_scanned := actual_has_anomaly and (anomalies.verified_in_current_cycle.size() >= anomalies.active_anomalies.size())

	var is_correct := (reported_anomaly == actual_has_anomaly) or (all_already_scanned and not reported_anomaly)

	if is_correct:
		# Mark any active anomalies into compendium on successful containment report!
		for aid in anomalies.active_anomalies:
			GameState.discovered_anomalies[aid] = true
		GameState.save_persistent_unlocks()

		GameState.verified_cycles += 1
		GameState.current_cycle += 1
		GameState.scanner_charges = GameState.max_scanner_charges
		GameState.player_integrity = minf(100.0, GameState.player_integrity + 15.0)
		AudioDirector.play_2d("scanner_confirm", -1.5, 1.08)

		if GameState.verified_cycles >= GameState.TARGET_CLEARANCE_CYCLES:
			anomalies.reset_all_anomalies()
			complex.update_progress_board(false)
			GameState.notification_posted.emit("CLEARANCE 8/8 ACHIEVED! North Ground Emergency Exit is UNLOCKED!", ThemeHelper.ACCENT_GREEN)
		else:
			_setup_cycle_state(false)
			GameState.notification_posted.emit("VERIFIED! Cycle %d / %d — Reality Shifted Across Complex." % [
				GameState.verified_cycles, GameState.TARGET_CLEARANCE_CYCLES
			], ThemeHelper.ACCENT_GREEN)
	else:
		# Penalty for missed anomaly or false containment alarm!
		AudioDirector.play_2d("scanner_error", 0.0, 0.88)
		var missed_info := "Floor was baseline clean!"
		if actual_has_anomaly:
			var first_meta := GameState.get_anomaly_meta(anomalies.active_anomalies[0])
			missed_info = "Missed %s in %s!" % [first_meta.get("title", "Anomaly"), first_meta.get("room", "Complex")]
		GameState.verified_cycles = maxi(0, GameState.verified_cycles - 2)
		GameState.loop_instability = minf(100.0, GameState.loop_instability + 22.0)
		GameState.player_integrity = maxf(15.0, GameState.player_integrity - 14.0)
		_setup_cycle_state(GameState.verified_cycles == 0)
		GameState.notification_posted.emit("PROTOCOL FAILURE (%s) — Clearance dropped to %d/8!" % [missed_info, GameState.verified_cycles], ThemeHelper.ACCENT_CRIMSON)

	GameState.state_updated.emit()


func _on_save_requested() -> void:
	if GameState.save_game(player.global_position, player.rotation.y):
		AudioDirector.play_2d("scanner_confirm", -3.0)
		GameState.notification_posted.emit("SHIFT PROGRESS SAVED TO DISK.", ThemeHelper.ACCENT_GREEN)


func _on_return_main_menu() -> void:
	get_tree().paused = false
	if DisplayServer.get_name() != "headless":
		Input.set_mouse_mode(Input.MOUSE_MODE_VISIBLE)
	get_tree().change_scene_to_file("res://scenes/MainMenu.tscn")


func _on_ending_triggered(_ending_id: String) -> void:
	get_tree().paused = false
	if DisplayServer.get_name() != "headless":
		Input.set_mouse_mode(Input.MOUSE_MODE_VISIBLE)
	get_tree().change_scene_to_file("res://scenes/EndingScreen.tscn")
