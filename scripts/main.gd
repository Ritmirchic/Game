extends Node2D

var game_state: GameState
var world: MiningWorld
var interface: GameInterface
var audio: GameAudio
var mode := "menu"
var autosave_timer := 30.0
var recovered_save := false
var load_message := ""

func _ready() -> void:
	get_tree().auto_accept_quit = false
	_configure_input()
	audio = GameAudio.new()
	add_child(audio)
	interface = GameInterface.new()
	add_child(interface)
	var loaded := SaveSystem.load_profile()
	recovered_save = bool(loaded.recovered)
	load_message = String(loaded.message)
	interface.has_save = FileAccess.file_exists(SaveSystem.SAVE_PATH)
	_bind_state(GameState.new(loaded.profile))
	interface.show_main_menu()
	audio.play_sector(int(game_state.profile.current_sector))
	if recovered_save:
		call_deferred("_show_recovery_notice")

func _bind_state(state: GameState) -> void:
	game_state = state
	audio.configure(state)
	interface.configure(state, world)
	game_state.achievement_unlocked.connect(_on_achievement)
	game_state.level_reached.connect(_on_level_reached)

func _show_recovery_notice() -> void:
	# Main-menu notice is readable and does not block launch.
	var label := interface._label(interface.screen, load_message.to_upper(), Rect2(115, 738, 900, 28), 11, Color("e8ba69"), 2)
	label.modulate.a = 0.0
	label.create_tween().tween_property(label, "modulate:a", 1.0, 0.4)

func _configure_input() -> void:
	var keys := {
		"move_left":[KEY_A, KEY_LEFT], "move_right":[KEY_D, KEY_RIGHT],
		"move_up":[KEY_W, KEY_UP], "move_down":[KEY_S, KEY_DOWN],
		"boost":[KEY_SHIFT], "fire":[KEY_SPACE], "dock":[KEY_E],
		"sector_map":[KEY_M], "cargo":[KEY_TAB], "pause_game":[KEY_ESCAPE]
	}
	for action in keys.keys():
		if not InputMap.has_action(action):
			InputMap.add_action(action)
		for key in keys[action]:
			var event := InputEventKey.new()
			event.physical_keycode = key
			InputMap.action_add_event(action, event)
	if not InputMap.has_action("mining"):
		InputMap.add_action("mining")
	var left := InputEventMouseButton.new()
	left.button_index = MOUSE_BUTTON_LEFT
	InputMap.action_add_event("mining", left)
	var right := InputEventMouseButton.new()
	right.button_index = MOUSE_BUTTON_RIGHT
	InputMap.action_add_event("fire", right)

func _connect_interface() -> void:
	# Connections are established once after the interface enters the tree.
	if interface.launch_requested.is_connected(_on_launch):
		return
	interface.launch_requested.connect(_on_launch)
	interface.resume_requested.connect(_on_resume)
	interface.menu_requested.connect(_on_menu)
	interface.quit_requested.connect(_on_quit)
	interface.respawn_requested.connect(_on_respawn)
	interface.travel_requested.connect(_on_travel)
	interface.settings_changed.connect(_on_settings_changed)
	interface.save_requested.connect(_on_save)
	interface.sfx_requested.connect(func(name: String): audio.play(name))

func _process(delta: float) -> void:
	if interface != null:
		_connect_interface()
	if mode == "flight" and game_state != null and bool(game_state.profile.settings.autosave):
		autosave_timer -= delta
		if autosave_timer <= 0.0:
			game_state.save()
			autosave_timer = 30.0

func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventKey and event.pressed and not event.echo:
		if event.physical_keycode == KEY_F11:
			var full := DisplayServer.window_get_mode() == DisplayServer.WINDOW_MODE_FULLSCREEN
			DisplayServer.window_set_mode(DisplayServer.WINDOW_MODE_WINDOWED if full else DisplayServer.WINDOW_MODE_FULLSCREEN)
			get_viewport().set_input_as_handled()
			return
		if event.is_action_pressed("pause_game"):
			match mode:
				"flight": _pause()
				"pause", "station", "map": _on_resume()
			get_viewport().set_input_as_handled()
		elif event.is_action_pressed("sector_map"):
			if mode == "flight":
				mode = "map"
				world.process_mode = Node.PROCESS_MODE_DISABLED
				interface.show_sector_map()
			elif mode == "map":
				_on_resume()
			get_viewport().set_input_as_handled()
		elif event.is_action_pressed("cargo") and mode == "flight":
			interface.hud.cargo_expanded = not interface.hud.cargo_expanded
			get_viewport().set_input_as_handled()
		elif event.is_action_pressed("dock"):
			if mode == "flight" and world.can_dock():
				world.dock()
			elif mode == "station":
				_on_resume()
			get_viewport().set_input_as_handled()

func _on_launch(fresh_profile: bool) -> void:
	if fresh_profile:
		_bind_state(GameState.new())
		interface.has_save = true
	_create_world()
	mode = "flight"
	world.process_mode = Node.PROCESS_MODE_INHERIT
	interface.show_game()
	interface.hud.notify(load_message if not fresh_profile else "NEW FLIGHT PROFILE  •  WELCOME TO CERES RELAY", Color("66ddc9"))
	game_state.save()

func _create_world() -> void:
	if is_instance_valid(world):
		remove_child(world)
		world.queue_free()
	world = MiningWorld.new()
	world.name = "MiningWorld"
	add_child(world)
	world.notification.connect(func(text: String, color: Color): interface.hud.notify(text, color))
	world.sfx_requested.connect(func(name: String): audio.play(name))
	world.dock_requested.connect(_on_dock)
	world.player_died.connect(_on_player_died)
	world.sector_ready.connect(_on_sector_ready)
	world.setup(game_state)
	interface.configure(game_state, world)

func _on_dock() -> void:
	if mode != "flight":
		return
	mode = "station"
	world.process_mode = Node.PROCESS_MODE_DISABLED
	interface.show_station("trade")
	game_state.save()

func _pause() -> void:
	mode = "pause"
	world.process_mode = Node.PROCESS_MODE_DISABLED
	interface.show_pause()

func _on_resume() -> void:
	if not is_instance_valid(world):
		interface.show_main_menu()
		mode = "menu"
		return
	if mode == "station":
		world.undock()
	mode = "flight"
	world.process_mode = Node.PROCESS_MODE_INHERIT
	interface.show_game()

func _on_menu() -> void:
	if game_state != null:
		game_state.save()
	if is_instance_valid(world):
		remove_child(world)
		world.queue_free()
	world = null
	mode = "menu"
	interface.has_save = true
	interface.configure(game_state, null)
	interface.show_main_menu()
	audio.play_sector(int(game_state.profile.current_sector))

func _on_travel(index: int) -> void:
	if not is_instance_valid(world) or index not in game_state.profile.unlocked_sectors:
		return
	mode = "flight"
	world.process_mode = Node.PROCESS_MODE_INHERIT
	world.load_sector(index)
	world.undock()
	interface.show_game()
	game_state.save()

func _on_player_died() -> void:
	mode = "death"
	world.process_mode = Node.PROCESS_MODE_DISABLED
	interface.show_death()
	game_state.save()

func _on_respawn() -> void:
	mode = "flight"
	world.process_mode = Node.PROCESS_MODE_INHERIT
	world.respawn()
	interface.show_game()
	interface.hud.notify("INSURANCE REDEPLOY COMPLETE  •  FLY SAFE", Color("66ddc9"))
	game_state.save()

func _on_sector_ready(index: int) -> void:
	interface.hud.show_sector(index)
	audio.play_sector(index)

func _on_achievement(id: String) -> void:
	interface.hud.show_achievement(id)
	audio.play("achievement")

func _on_level_reached(level: int) -> void:
	interface.hud.notify("PILOT LEVEL %d  •  NEW CLEARANCES AVAILABLE" % level, Color("e8ba69"))
	audio.play("achievement")

func _on_settings_changed() -> void:
	audio.apply_settings()
	game_state.save()

func _on_save() -> void:
	if game_state.save():
		interface.hud.notify("FLIGHT PROFILE SAVED", Color("66ddc9"))
	else:
		interface.hud.notify("SAVE FAILED  •  CHECK STORAGE ACCESS", Color("e77870"))

func _on_quit() -> void:
	if game_state != null:
		game_state.save()
	if is_instance_valid(audio):
		audio.shutdown()
	await get_tree().create_timer(0.08).timeout
	get_tree().quit()

func _notification(what: int) -> void:
	if what == NOTIFICATION_WM_CLOSE_REQUEST:
		_on_quit()
