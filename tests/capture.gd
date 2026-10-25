extends SceneTree

var main: Node
var world: MiningWorld
var output := "res://shots"

func _initialize() -> void:
	call_deferred("run")

func frames(count: int) -> void:
	for i in range(count):
		await process_frame

func capture(name: String) -> void:
	await RenderingServer.frame_post_draw
	var image := root.get_texture().get_image()
	if image == null or image.is_empty():
		push_error("Unable to capture " + name)
		return
	var error := image.save_png(output + "/" + name + ".png")
	print("CAPTURE ", name, "  ", image.get_width(), "x", image.get_height(), "  error=", error)

func run() -> void:
	DirAccess.make_dir_recursive_absolute(output)
	root.size = Vector2i(1600, 900)
	main = load("res://scenes/main.tscn").instantiate()
	root.add_child(main)
	await frames(35)
	await capture("01_main_menu")
	main._on_launch(true)
	await frames(20)
	world = main.world
	var state: GameState = main.game_state
	state.profile.credits = 50000
	state.profile.upgrades.drone_capacity = 3
	state.profile.drones = 3
	world.sync_drones()
	for drone in world.drones:
		drone.enabled = false
	var positions := [Vector2(340, -40), Vector2(-330, -190), Vector2(270, 260), Vector2(-390, 210)]
	for i in range(mini(positions.size(), world.asteroids.size())):
		var rock := world.asteroids[i]
		rock.global_position = world.player.global_position + positions[i]
		rock.linear_velocity = Vector2.ZERO
	await frames(25)
	await capture("02_starter_belt")
	var target: MineableAsteroid = world.asteroids[0]
	main.interface.hud.sector_title_time = 0.0
	main.interface.hud.toasts.clear()
	world.scan_target = target
	world.player.rotation = 0.0
	Input.action_press("mining")
	await frames(22)
	await capture("03_mining_laser")
	Input.action_release("mining")
	for drone in world.drones:
		drone.enabled = true
	world.spawn_effect("explosion", target.global_position, Color("67dfca"), 1.1, target.sprite.texture)
	await frames(5)
	await capture("04_destruction_vfx")
	var enemy_a := world.spawn_enemy(1, world.player.global_position + Vector2(460, -190))
	var enemy_b := world.spawn_enemy(0, world.player.global_position + Vector2(-440, 170))
	world._on_enemy_fire(enemy_a, (world.player.global_position - enemy_a.global_position).normalized(), 0.0, 330.0)
	world._on_enemy_fire(enemy_b, (world.player.global_position - enemy_b.global_position).normalized(), 0.0, 300.0)
	await frames(12)
	await capture("05_combat")
	main.mode = "station"
	world.process_mode = Node.PROCESS_MODE_DISABLED
	for tab in ["trade", "upgrades", "drones", "ships", "sectors", "research"]:
		main.interface.show_station(tab)
		await frames(14)
		await capture("06_station_" + tab)
	main.interface.show_sector_map()
	await frames(14)
	await capture("07_sector_map")
	main.interface.show_settings("pause")
	await frames(14)
	await capture("08_settings")
	main.interface.show_achievements("menu")
	await frames(14)
	await capture("09_achievements")
	# Unique environments are captured in live flight, not as static mockups.
	main.mode = "flight"
	world.process_mode = Node.PROCESS_MODE_INHERIT
	main.interface.show_game()
	state.profile.level = 12
	for i in range(1, 5):
		state.unlock_sector(i)
	for i in range(1, 5):
		world.load_sector(i)
		await frames(25)
		await capture("10_sector_%d" % (i + 1))
	main.audio.shutdown()
	main.queue_free()
	await frames(12)
	print("CAPTURE ROUTE COMPLETE")
	quit()
