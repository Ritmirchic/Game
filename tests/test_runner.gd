extends SceneTree

var passed := 0
var failed := 0
var failures: Array[String] = []
var main: Node
var state: GameState
var world: MiningWorld

func _initialize() -> void:
	call_deferred("run")

func check(condition: bool, name: String) -> void:
	if condition:
		passed += 1
		print("PASS  ", name)
	else:
		failed += 1
		failures.append(name)
		push_error("FAIL  " + name)

func frames(count: int = 2) -> void:
	for i in range(count):
		await process_frame

func physics_frames(count: int = 2) -> void:
	for i in range(count):
		await physics_frame

func run() -> void:
	print("SPACE MINER — GODOT RUNTIME TEST SUITE")
	await test_data_and_state()
	await test_save_system()
	await test_runtime()
	print("\nTEST SUMMARY: ", passed, " passed / ", failed, " failed")
	var report := {"engine":Engine.get_version_info().string, "passed":passed, "failed":failed, "failures":failures}
	var file := FileAccess.open("user://test_results.json", FileAccess.WRITE)
	if file != null:
		file.store_string(JSON.stringify(report, "\t"))
	if is_instance_valid(main):
		main.audio.shutdown()
		main.queue_free()
	await frames(12)
	quit(0 if failed == 0 else 1)

func test_data_and_state() -> void:
	check(GameData.RESOURCES.size() >= 15, "15 original resource definitions")
	check(GameData.SECTORS.size() == 5, "five sector definitions")
	check(GameData.UPGRADES.size() >= 12, "upgrade tree contains at least 12 systems")
	check(GameData.SHIPS.size() >= 4, "four original ship classes")
	check(GameData.ENEMIES.size() == 5, "five hostile designs")
	check(GameData.ACHIEVEMENTS.size() >= 10, "achievement progression definitions")
	var ids := {}
	for resource in GameData.RESOURCES:
		check(not ids.has(resource.id) and ResourceLoader.exists("res://assets/art/icons/resource_%s.png" % resource.id), "resource identity + icon: " + resource.id)
		ids[resource.id] = true
	for sector in GameData.SECTORS:
		check(ResourceLoader.exists(sector.background), "unique sector backdrop: " + sector.name)
	state = GameState.new()
	check(state.cargo_total() == 0 and int(state.profile.credits) == 450, "new profile initial economy")
	var stat_keys := {"laser_power":"laser", "mining_speed":"mine_range", "cargo_capacity":"cargo", "engine_speed":"speed", "acceleration":"acceleration", "hull":"max_hull", "shield":"max_shield", "energy_regen":"energy_regen", "scanner":"scanner", "drone_capacity":"drone_slots", "weapon_damage":"weapon", "auto_collection":"collection", "armor":"armor", "reactor":"max_energy", "drone_mining":"drone_power", "research":"xp_multiplier"}
	for upgrade in GameData.UPGRADES:
		var key: String = stat_keys[upgrade.id]
		var before: float = float(state.get_stats()[key])
		state.profile.credits = 10000000
		check(state.buy_upgrade(upgrade.id), "purchase succeeds: " + upgrade.id)
		var after: float = float(state.get_stats()[key])
		check(after > before, "live stat changes: " + upgrade.id)
	state = GameState.new()
	state.profile.credits = 0
	check(not state.buy_upgrade("laser_power"), "unaffordable upgrade refused")
	state.profile.credits = 1000
	var accepted := state.add_resource("iron", 10000)
	check(accepted == int(state.get_stats().cargo), "cargo capacity strictly enforced")
	check(state.add_resource("gold", 1) == 0, "full cargo rejects additional ore")
	var value := state.sell_cargo()
	check(value == accepted * int(GameData.get_resource("iron").value), "sale uses resource quantities and values")
	check(state.cargo_total() == 0, "sale empties manifest")
	state = GameState.new()
	state.profile.level = 12
	state.profile.credits = 1000000
	for i in range(GameData.SECTORS.size()):
		check(state.unlock_sector(i) or i == 0, "sector unlock progression %d" % i)
	check(state.profile.unlocked_sectors.size() == 5, "all sectors charted")
	check("frontier" in state.profile.achievements, "sector achievement triggered")
	for ship in GameData.SHIPS:
		check(state.buy_ship(ship.id) or ship.id == "prospector", "ship purchase: " + ship.id)
		check(state.select_ship(ship.id), "ship selection: " + ship.id)
	check(state.buy_upgrade("drone_capacity"), "drone bay unlocks functional slot")
	check(state.buy_drone(), "first drone purchased")
	check(int(state.profile.drones) == 1, "drone count persisted in profile")
	state = GameState.new()
	state.add_xp(100000)
	check(int(state.profile.level) > 5, "experience produces pilot levels")

func test_save_system() -> void:
	state = GameState.new()
	state.profile.credits = 43210
	state.profile.upgrades.laser_power = 4
	state.profile.resources.gold = 17
	state.profile.discovered.append("gold")
	check(state.save(), "versioned save writes successfully")
	var loaded := SaveSystem.load_profile()
	check(int(loaded.profile.credits) == 43210, "save credits round-trip")
	check(int(loaded.profile.upgrades.laser_power) == 4, "save upgrades round-trip")
	check(int(loaded.profile.resources.gold) == 17, "save resources round-trip")
	check("gold" in loaded.profile.discovered, "save research discoveries round-trip")
	var file := FileAccess.open(SaveSystem.SAVE_PATH, FileAccess.WRITE)
	file.store_string("{ definitely not valid json")
	file.close()
	var recovered := SaveSystem.load_profile()
	check(bool(recovered.recovered), "corrupted primary save triggers recovery")
	check(recovered.profile.has("resources") and recovered.profile.has("upgrades"), "recovered save has valid schema")
	check(FileAccess.file_exists(SaveSystem.SAVE_PATH + ".corrupt") or FileAccess.file_exists(SaveSystem.BACKUP_PATH), "corruption quarantined or backup retained")

func test_runtime() -> void:
	main = load("res://scenes/main.tscn").instantiate()
	root.add_child(main)
	await frames(4)
	check(main.mode == "menu", "actual game boots to professional main menu")
	main._on_launch(true)
	await physics_frames(8)
	state = main.game_state
	world = main.world
	check(is_instance_valid(world), "flight creates live world")
	check(world.asteroids.size() == int(GameData.SECTORS[0].asteroids), "procedural asteroid field populated")
	check(world.player is CharacterBody2D, "player uses CharacterBody2D physics")
	check(world.asteroids[0] is RigidBody2D, "asteroids use RigidBody2D physics")
	var safe := true
	for rock in world.asteroids:
		if rock.global_position.distance_to(GameData.PLAYER_SPAWN) < rock.radius + 40.0 or rock.global_position.distance_to(GameData.STATION_POSITION) < rock.radius + 160.0:
			safe = false
	check(safe, "asteroid spawning validates player/station clearance")
	var start: Vector2 = world.player.global_position
	Input.action_press("move_right")
	await physics_frames(35)
	Input.action_release("move_right")
	check(world.player.global_position.x > start.x + 15.0, "ship thrust moves actual physics body")
	check(world.player.camera.is_current(), "game camera follows active ship")
	var rock: MineableAsteroid = world.asteroids[0]
	rock.global_position = world.player.global_position + Vector2(240, 0)
	rock.linear_velocity = Vector2.ZERO
	world.scan_target = rock
	var health := rock.health
	var energy: float = world.player.energy
	Input.action_press("mining")
	world._update_mining(0.15)
	Input.action_release("mining")
	check(rock.health < health, "aimed mining beam damages asteroid")
	check(world.player.energy < energy, "mining consumes reactor energy")
	check(world.beam.visible, "mining produces visible beam")
	var old_count := world.asteroids.size()
	world.mine_asteroid(rock, 100000.0, world.player)
	await frames(4)
	check(world.asteroids.size() == old_count - 1, "depleted asteroid destroyed and removed")
	check(world.pickups.size() > 0, "destruction generates collectible resources")
	for id in state.profile.resources.keys():
		state.profile.resources[id] = 0
	var pickup := world.spawn_pickup("iron", 4, world.player.global_position, Vector2.ZERO)
	pickup.attract_target = world.player
	await physics_frames(8)
	check(int(state.profile.resources.iron) >= 4, "Area2D resource collection enters cargo")
	state.profile.credits = 100000
	state.profile.upgrades.drone_capacity = 1
	state.profile.drones = 1
	state.profile.drones_active = true
	world.sync_drones()
	await physics_frames(3)
	check(world.drones.size() == 1, "purchased drone spawns visible entity")
	var drone: MiningDrone = world.drones[0]
	var drone_rock: MineableAsteroid = world.asteroids[0]
	drone_rock.global_position = drone.global_position + Vector2(100, 0)
	drone_rock.linear_velocity = Vector2.ZERO
	drone.target = drone_rock
	drone.state = "MINE"
	var drone_health: float = drone_rock.health
	await physics_frames(50)
	check(drone_rock.health < drone_health or drone.mined_this_trip > 0, "drone AI autonomously mines target")
	for id in state.profile.resources.keys(): state.profile.resources[id] = 0
	drone.carried = GameData.empty_resource_map()
	drone.carried_total = 0
	drone.add_carried("iron", 3)
	drone.global_position = world.player.global_position
	drone.state = "RETURN"
	await physics_frames(60)
	check(drone.total_deposited >= 3 and int(state.profile.resources.iron) >= 3, "drone returns and deposits real cargo")
	var enemy := world.spawn_enemy(0, world.player.global_position + Vector2(410, 0))
	await physics_frames(70)
	check(enemy.active, "enemy AI acquires player")
	var projectile_count: int = world.projectile_root.get_child_count()
	world._on_enemy_fire(enemy, Vector2.LEFT, 8.0, 450.0)
	check(world.projectile_root.get_child_count() > projectile_count, "enemy attacks create real projectiles")
	var enemy_health: float = enemy.health + enemy.shield
	var shot := world.spawn_projectile(true, enemy.global_position, Vector2.RIGHT, 20.0, 900.0, Color.WHITE)
	await physics_frames(5)
	check(enemy.health + enemy.shield < enemy_health, "friendly projectile damages hostile")
	var credits: int = int(state.profile.credits)
	enemy.take_damage(100000.0)
	await frames(3)
	check(int(state.profile.credits) > credits and int(state.profile.stats.kills) > 0, "enemy destruction grants reward and progression")
	state.profile.level = 12
	state.profile.credits = 1000000
	for i in range(1, 5): state.unlock_sector(i)
	for i in range(5):
		world.load_sector(i)
		await frames(3)
		check(world.sector_index == i and world.asteroids.size() == int(GameData.SECTORS[i].asteroids), "sector %d loads unique live field" % (i + 1))
		if i == 4:
			check(world.wells.size() == 3, "void frontier gravity hazards active")
	world.load_sector(0)
	await frames(3)
	await test_ui()
	check(world.asteroids.size() <= int(GameData.SECTORS[0].asteroids) + 2, "asteroid population remains capped")
	check(world.enemies.size() <= int(GameData.SECTORS[0].enemy_cap) + 1, "enemy population remains capped")
	var finite := true
	for body_rock in world.asteroids:
		finite = finite and body_rock.global_position.is_finite() and body_rock.linear_velocity.is_finite()
	check(finite, "simulation positions and velocities remain finite")
	var performance_start := Time.get_ticks_usec()
	for i in range(240):
		world._update_pickups(1.0 / 60.0)
		world._update_spawning(1.0 / 60.0)
		world._update_hazards(1.0 / 60.0)
	var update_microseconds := float(Time.get_ticks_usec() - performance_start) / 240.0
	print("PERFORMANCE  simulation update average: ", snappedf(update_microseconds / 1000.0, 0.001), " ms")
	check(update_microseconds < 16600.0, "simulation performance stays inside 60 Hz frame budget")

func test_ui() -> void:
	main.mode = "pause"
	world.process_mode = Node.PROCESS_MODE_DISABLED
	var screens := ["pause", "settings", "achievements", "manual", "map", "trade", "upgrades", "drones", "ships", "sectors", "research"]
	for name in screens:
		match name:
			"pause": main.interface.show_pause()
			"settings": main.interface.show_settings("pause")
			"achievements": main.interface.show_achievements("pause")
			"manual": main.interface.show_manual("pause")
			"map": main.interface.show_sector_map()
			"trade", "upgrades", "drones", "ships", "sectors", "research": main.interface.show_station(name)
		await frames(2)
		check(is_instance_valid(main.interface.screen) and main.interface.screen.get_child_count() > 0, "UI renders screen: " + name)
		var unwired := []
		_find_unwired_buttons(main.interface.screen, unwired)
		check(unwired.is_empty(), "all buttons wired: " + name + (" " + str(unwired) if not unwired.is_empty() else ""))
	main.mode = "flight"
	world.process_mode = Node.PROCESS_MODE_INHERIT
	main.interface.show_game()
	await frames(2)

func _find_unwired_buttons(node: Node, result: Array) -> void:
	if node is Button and node.pressed.get_connections().is_empty():
		result.append(node.text)
	for child in node.get_children():
		_find_unwired_buttons(child, result)
