class_name MiningWorld
extends Node2D
const Data = preload("res://scripts/data.gd")
const State = preload("res://scripts/game_state.gd")
const Ship = preload("res://scripts/ship.gd")
const Asteroid = preload("res://scripts/asteroid.gd")
const Drone = preload("res://scripts/drone.gd")
const Enemy = preload("res://scripts/enemy.gd")
const Pickup = preload("res://scripts/pickup.gd")
const Projectile = preload("res://scripts/projectile.gd")
const Effect = preload("res://scripts/effect.gd")
const Hazard = preload("res://scripts/hazard.gd")

signal notification(text: String, color: Color)
signal sfx_requested(name: String)
signal dock_requested
signal player_died
signal sector_ready(index: int)

var game_state: State
var player: Ship
var station: Sprite2D
var station_glow: Sprite2D
var background: TextureRect
var background_material: ShaderMaterial
var asteroid_root: Node2D
var enemy_root: Node2D
var drone_root: Node2D
var pickup_root: Node2D
var projectile_root: Node2D
var effect_root: Node2D
var hazard_root: Node2D
var asteroids: Array[Asteroid] = []
var enemies: Array[Enemy] = []
var drones: Array[Drone] = []
var pickups: Array[Pickup] = []
var wells: Array[Hazard] = []
var scan_target: Asteroid
var mining_target: Asteroid
var mining := false
var beam: Line2D
var beam_glow: Line2D
var rng := RandomNumberGenerator.new()
var sector_index := 0
var weapon_cooldown := 0.0
var enemy_spawn_timer := 8.0
var asteroid_spawn_timer := 4.0
var hazard_timer := 10.0
var laser_sound_timer := 0.0
var spark_timer := 0.0
var full_notice_timer := 0.0
var elapsed := 0.0
var target_scan_timer := 0.0

func _ready() -> void:
	_build_background()
	asteroid_root = _make_root("Asteroids")
	enemy_root = _make_root("Hostiles")
	drone_root = _make_root("Mining drones")
	pickup_root = _make_root("Resource pickups")
	projectile_root = _make_root("Projectiles")
	effect_root = _make_root("VFX")
	hazard_root = _make_root("Hazards")
	_build_station()
	_build_player()
	_build_beam()

func _make_root(node_name: String) -> Node2D:
	var root := Node2D.new()
	root.name = node_name
	add_child(root)
	return root

func _build_background() -> void:
	var layer := CanvasLayer.new()
	layer.layer = -100
	add_child(layer)
	background = TextureRect.new()
	background.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	background.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	background.stretch_mode = TextureRect.STRETCH_SCALE
	background.mouse_filter = Control.MOUSE_FILTER_IGNORE
	background_material = ShaderMaterial.new()
	background_material.shader = load("res://assets/shaders/nebula.gdshader")
	background.material = background_material
	layer.add_child(background)

func _build_station() -> void:
	station_glow = Sprite2D.new()
	station_glow.texture = load("res://assets/art/particles/glow.png")
	station_glow.position = Data.STATION_POSITION
	station_glow.scale = Vector2.ONE * 8.0
	station_glow.modulate = Color(0.24, 0.75, 0.70, 0.09)
	station_glow.z_index = -2
	add_child(station_glow)
	station = Sprite2D.new()
	station.name = "CeresRelay"
	station.texture = load("res://assets/art/station/ceres_relay.png")
	station.position = Data.STATION_POSITION
	station.scale = Vector2.ONE * 0.78
	station.z_index = 1
	add_child(station)
	var dock_area := Area2D.new()
	dock_area.name = "DockingApproach"
	dock_area.collision_layer = 64
	dock_area.collision_mask = 1
	var shape := CollisionShape2D.new()
	var circle := CircleShape2D.new()
	circle.radius = 260.0
	shape.shape = circle
	dock_area.add_child(shape)
	station.add_child(dock_area)

func _build_player() -> void:
	player = Ship.new()
	player.name = "Ship"
	player.position = Data.PLAYER_SPAWN
	add_child(player)
	player.destroyed.connect(_on_player_destroyed)
	player.damaged.connect(_on_player_damaged)
	player.impacted.connect(func(): sfx_requested.emit("impact"))

func _build_beam() -> void:
	beam_glow = Line2D.new()
	beam_glow.width = 14.0
	beam_glow.default_color = Color(0.2, 0.95, 0.78, 0.10)
	beam_glow.z_index = 20
	beam_glow.visible = false
	add_child(beam_glow)
	beam = Line2D.new()
	beam.width = 3.0
	beam.default_color = Color(0.52, 1.0, 0.84, 0.95)
	beam.z_index = 21
	beam.visible = false
	var material := ShaderMaterial.new()
	material.shader = load("res://assets/shaders/laser.gdshader")
	beam.material = material
	add_child(beam)

func setup(state: State) -> void:
	game_state = state
	player.configure(state, true)
	load_sector(int(state.profile.current_sector))

func load_sector(index: int) -> void:
	sector_index = clampi(index, 0, Data.SECTORS.size() - 1)
	game_state.travel_sector(sector_index)
	_clear_root(asteroid_root)
	_clear_root(enemy_root)
	_clear_root(pickup_root)
	_clear_root(projectile_root)
	_clear_root(hazard_root)
	asteroids.clear()
	enemies.clear()
	pickups.clear()
	wells.clear()
	scan_target = null
	mining_target = null
	rng.seed = 9042026 + sector_index * 1009
	var sector: Dictionary = Data.SECTORS[sector_index]
	background.texture = load(sector.background)
	background_material.set_shader_parameter("sector_tint", Color(sector.color))
	player.global_position = Data.PLAYER_SPAWN
	player.velocity = Vector2.ZERO
	player.rotation = -PI / 2.0
	player.full_repair()
	_generate_asteroids(int(sector.asteroids))
	for i in range(maxi(0, int(sector.enemy_cap) - 1)):
		spawn_enemy()
	if sector_index == 4:
		for i in range(3):
			_spawn_well(i)
	enemy_spawn_timer = 10.0
	asteroid_spawn_timer = 4.0
	hazard_timer = 8.0
	sync_drones()
	sector_ready.emit(sector_index)
	notification.emit("%s  •  %s" % [sector.name, sector.subtitle], Color(sector.color))

func _clear_root(root: Node) -> void:
	for child in root.get_children():
		child.queue_free()

func _generate_asteroids(count: int) -> void:
	for i in range(count):
		var radius := rng.randf_range(43.0, 98.0)
		if rng.randf() < 0.10:
			radius = rng.randf_range(100.0, 124.0)
		var position := Vector2.ZERO
		var valid := false
		for attempt in range(160):
			position = Vector2(rng.randf_range(150.0, Data.WORLD_SIZE.x - 150.0), rng.randf_range(150.0, Data.WORLD_SIZE.y - 150.0))
			if position.distance_to(Data.PLAYER_SPAWN) < radius + 360.0:
				continue
			if position.distance_to(Data.STATION_POSITION) < radius + 390.0:
				continue
			valid = true
			for rock in asteroids:
				if is_instance_valid(rock) and position.distance_to(rock.global_position) < radius + rock.radius + 65.0:
					valid = false
					break
			if valid:
				break
		if valid:
			spawn_asteroid(position, radius)

func spawn_asteroid(position: Vector2, radius: float = -1.0, velocity_override: Vector2 = Vector2.INF) -> Asteroid:
	var sector: Dictionary = Data.SECTORS[sector_index]
	if radius < 0.0:
		radius = rng.randf_range(45.0, 102.0)
	var resource_id := _weighted_resource(sector.resources, sector.weights)
	var rock := Asteroid.new()
	asteroid_root.add_child(rock)
	rock.configure(sector_index, radius, resource_id, rng.randi_range(0, 5), rng)
	rock.global_position = position
	rock.rotation = rng.randf() * TAU
	var drift := 3.0 if sector_index != 2 else 12.0
	rock.linear_velocity = Vector2.from_angle(rng.randf() * TAU) * rng.randf_range(drift, drift + 14.0)
	if velocity_override != Vector2.INF:
		rock.linear_velocity = velocity_override
	rock.angular_velocity = rng.randf_range(-0.28, 0.28)
	rock.depleted.connect(_on_asteroid_depleted)
	asteroids.append(rock)
	return rock

func _weighted_resource(ids: Array, weights: Array) -> String:
	var total := 0.0
	for weight in weights:
		total += float(weight)
	var roll := rng.randf() * total
	for i in range(ids.size()):
		roll -= float(weights[i])
		if roll <= 0.0:
			return String(ids[i])
	return String(ids[0])

func _spawn_well(index: int) -> void:
	var well := Hazard.new()
	well.kind = "well"
	var angle := float(index) / 3.0 * TAU + 0.7
	well.position = Data.STATION_POSITION + Vector2.from_angle(angle) * 1050.0
	hazard_root.add_child(well)
	wells.append(well)

func _physics_process(delta: float) -> void:
	if game_state == null or not is_instance_valid(player):
		return
	elapsed += delta
	weapon_cooldown = maxf(0.0, weapon_cooldown - delta)
	laser_sound_timer = maxf(0.0, laser_sound_timer - delta)
	spark_timer = maxf(0.0, spark_timer - delta)
	full_notice_timer = maxf(0.0, full_notice_timer - delta)
	if player.alive and player.controls_enabled:
		_update_mining(delta)
		if Input.is_action_pressed("fire") and weapon_cooldown <= 0.0:
			fire_player()
	else:
		mining = false
		beam.visible = false
		beam_glow.visible = false
	_update_pickups(delta)
	_update_spawning(delta)
	_update_hazards(delta)
	game_state.profile.stats.play_seconds = int(game_state.profile.stats.play_seconds) + int(delta)
	# Fractional play time is tracked separately to avoid losing sub-second updates.
	game_state.profile.stats["_play_fraction"] = float(game_state.profile.stats.get("_play_fraction", 0.0)) + delta
	if float(game_state.profile.stats._play_fraction) >= 1.0:
		var seconds := int(game_state.profile.stats._play_fraction)
		game_state.profile.stats.play_seconds = int(game_state.profile.stats.play_seconds) + seconds
		game_state.profile.stats._play_fraction = float(game_state.profile.stats._play_fraction) - seconds

func _process(delta: float) -> void:
	if not is_instance_valid(player):
		return
	background_material.set_shader_parameter("camera_offset", player.global_position)
	station_glow.modulate.a = 0.075 + sin(elapsed * 1.4) * 0.018
	target_scan_timer -= delta
	if target_scan_timer <= 0.0:
		target_scan_timer = 0.08
		_update_scan_target()
	queue_redraw()

func _update_scan_target() -> void:
	if not player.alive:
		scan_target = null
		return
	var mouse := get_global_mouse_position()
	var range_limit := float(game_state.get_stats().scanner)
	scan_target = null
	var best := 70.0
	for rock in asteroids:
		if not is_instance_valid(rock) or rock.destroyed:
			continue
		if player.global_position.distance_to(rock.global_position) > range_limit:
			continue
		var mouse_distance := mouse.distance_to(rock.global_position) - rock.radius
		if mouse_distance < best:
			best = mouse_distance
			scan_target = rock

func _update_mining(delta: float) -> void:
	mining = Input.is_action_pressed("mining") and player.energy > 0.5
	if not mining:
		beam.visible = false
		beam_glow.visible = false
		mining_target = null
		return
	var stats := game_state.get_stats()
	var mouse := get_global_mouse_position()
	var origin: Vector2 = player.emitter_position()
	var direction := (mouse - origin).normalized()
	var max_range := float(stats.mine_range)
	var end := origin + direction * minf(max_range, origin.distance_to(mouse))
	mining_target = null
	if is_instance_valid(scan_target) and not scan_target.destroyed and origin.distance_to(scan_target.global_position) <= max_range + scan_target.radius:
		mining_target = scan_target
		direction = (scan_target.global_position - origin).normalized()
		end = scan_target.global_position - direction * scan_target.radius * 0.60
	player.energy = maxf(0.0, player.energy - delta * 11.5)
	beam.visible = true
	beam_glow.visible = true
	beam.points = PackedVector2Array([origin, end])
	beam_glow.points = beam.points
	var beam_color := Color("62f1ce")
	if is_instance_valid(mining_target):
		beam_color = Color(Data.get_resource(mining_target.resource_id).color).lightened(0.25)
		mine_asteroid(mining_target, float(stats.laser) * delta, player)
		if spark_timer <= 0.0:
			spawn_effect("sparks", end, beam_color, 0.65)
			spark_timer = 0.11
	beam.default_color = beam_color
	beam_glow.default_color = Color(beam_color, 0.12)
	if laser_sound_timer <= 0.0:
		sfx_requested.emit("laser")
		laser_sound_timer = 0.43

func mine_asteroid(rock: Asteroid, damage: float, collector: Node) -> void:
	if not is_instance_valid(rock) or rock.destroyed:
		return
	var actual := rock.apply_damage(damage, collector)
	if actual <= 0.0:
		return
	var units := rock.take_yield()
	if units > 0:
		if collector is Drone:
			collector.note_mined(units)
		var velocity := Vector2.from_angle(rng.randf() * TAU) * rng.randf_range(35.0, 85.0)
		var pickup := spawn_pickup(rock.resource_id, units, rock.global_position + Vector2.from_angle(rng.randf() * TAU) * rock.radius * 0.4, velocity)
		if collector is Drone:
			pickup.attract_target = collector

func _on_asteroid_depleted(rock: Asteroid) -> void:
	call_deferred("_finish_asteroid", rock)

func _finish_asteroid(rock: Asteroid) -> void:
	if not is_instance_valid(rock) or rock.has_meta("finished"):
		return
	rock.set_meta("finished", true)
	var collector := rock.last_hit_by
	var remaining := rock.remaining + int(floor(rock.yield_progress))
	rock.remaining = 0
	var chunks := mini(6, remaining)
	for i in range(chunks):
		var amount := remaining / chunks
		if i < remaining % chunks:
			amount += 1
		var velocity := Vector2.from_angle(rng.randf() * TAU) * rng.randf_range(55.0, 135.0)
		var pickup := spawn_pickup(rock.resource_id, amount, rock.global_position, velocity)
		if collector is Drone and is_instance_valid(collector):
			collector.note_mined(amount)
			pickup.attract_target = collector
	if float(game_state.get_stats().rare_chance) > 0.0 and rng.randf() < float(game_state.get_stats().rare_chance):
		var sector: Dictionary = Data.SECTORS[sector_index]
		var rare_id: String = sector.resources[-1]
		spawn_pickup(rare_id, 1, rock.global_position, Vector2.from_angle(rng.randf() * TAU) * 90.0)
	spawn_effect("explosion", rock.global_position, Color(Data.get_resource(rock.resource_id).color), rock.radius / 65.0, rock.sprite.texture)
	sfx_requested.emit("explosion")
	game_state.record_asteroid()
	asteroids.erase(rock)
	rock.queue_free()

func spawn_pickup(id: String, amount: int, position: Vector2, velocity: Vector2) -> Pickup:
	var pickup := Pickup.new()
	pickup_root.add_child(pickup)
	pickup.configure(id, amount, velocity)
	pickup.global_position = position
	pickup.collected.connect(_on_pickup_collected)
	pickups.append(pickup)
	return pickup

func _update_pickups(_delta: float) -> void:
	pickups = pickups.filter(func(p): return is_instance_valid(p) and not p.claimed)
	var stats := game_state.get_stats()
	for pickup in pickups:
		if is_instance_valid(pickup.attract_target) and pickup.attract_target is Drone:
			continue
		pickup.attract_target = null
		if player.alive and pickup.global_position.distance_to(player.global_position) < float(stats.collection) and player.can_collect():
			pickup.attract_target = player
			continue
		var nearest: Drone
		var best := 155.0
		for drone in drones:
			if not is_instance_valid(drone) or not drone.can_collect():
				continue
			var distance := pickup.global_position.distance_to(drone.global_position)
			if distance < best:
				best = distance
				nearest = drone
		if nearest != null:
			pickup.attract_target = nearest

func _on_pickup_collected(pickup: Pickup, collector: Node) -> void:
	var accepted := 0
	if collector is Ship:
		accepted = game_state.add_resource(pickup.resource_id, pickup.amount)
	elif collector is Drone:
		accepted = collector.add_carried(pickup.resource_id, pickup.amount)
	if accepted > 0:
		spawn_effect("collect", pickup.global_position, Color(Data.get_resource(pickup.resource_id).color), 0.55)
		sfx_requested.emit("pickup")
	if accepted < pickup.amount:
		call_deferred("spawn_pickup", pickup.resource_id, pickup.amount - accepted, pickup.global_position, Vector2.from_angle(rng.randf() * TAU) * 45.0)
		if collector is Ship and full_notice_timer <= 0.0:
			notification.emit("CARGO HOLD FULL  •  RETURN TO CERES RELAY", Color("e9b45f"))
			full_notice_timer = 4.0

func fire_player() -> void:
	var stats := game_state.get_stats()
	weapon_cooldown = float(stats.fire_cooldown)
	var direction := player.aim_direction()
	spawn_projectile(true, player.emitter_position() + direction * 14.0, direction, float(stats.weapon), 960.0, Color("69ead5"))
	sfx_requested.emit("cannon")

func spawn_projectile(friendly: bool, position: Vector2, direction: Vector2, damage: float, speed: float, color: Color) -> Projectile:
	var projectile := Projectile.new()
	projectile.friendly = friendly
	projectile.damage = damage
	projectile.velocity = direction.normalized() * speed
	projectile.projectile_color = color
	projectile_root.add_child(projectile)
	projectile.global_position = position
	projectile.hit.connect(_on_projectile_hit)
	return projectile

func _on_projectile_hit(projectile: Projectile, body: Node) -> void:
	if projectile.friendly:
		if body is Enemy:
			body.take_damage(projectile.damage)
			spawn_effect("sparks", projectile.global_position, projectile.projectile_color, 0.6)
			sfx_requested.emit("impact")
		elif body is Asteroid:
			call_deferred("mine_asteroid", body, projectile.damage * 0.24, player)
			spawn_effect("sparks", projectile.global_position, projectile.projectile_color, 0.45)
	else:
		if body is Ship or body is Drone:
			body.take_damage(projectile.damage)
			spawn_effect("sparks", projectile.global_position, projectile.projectile_color, 0.65)
			sfx_requested.emit("impact")

func spawn_enemy(kind: int = -1, position: Vector2 = Vector2.INF) -> Enemy:
	var sector: Dictionary = Data.SECTORS[sector_index]
	if kind < 0:
		kind = int(sector.enemies[rng.randi_range(0, sector.enemies.size() - 1)])
	if position == Vector2.INF:
		for attempt in range(80):
			position = Vector2(rng.randf_range(100, Data.WORLD_SIZE.x - 100), rng.randf_range(100, Data.WORLD_SIZE.y - 100))
			if position.distance_to(player.global_position) > 850.0 and position.distance_to(Data.STATION_POSITION) > 650.0:
				break
	var enemy := Enemy.new()
	enemy_root.add_child(enemy)
	enemy.configure(kind, player, float(game_state.profile.settings.difficulty))
	enemy.global_position = position
	enemy.fire_requested.connect(_on_enemy_fire)
	enemy.special_attack.connect(_on_enemy_special)
	enemy.destroyed.connect(_on_enemy_destroyed)
	enemies.append(enemy)
	return enemy

func _on_enemy_fire(enemy: Enemy, direction: Vector2, damage: float, speed: float) -> void:
	var count := 1
	var spread := 0.0
	if enemy.kind == 2:
		count = 3
		spread = 0.13
	elif enemy.kind == 3:
		count = 2
		spread = 0.08
	for i in range(count):
		var offset := (float(i) - float(count - 1) / 2.0) * spread
		var color := Color("ef6e62") if enemy.kind < 3 else Color("887cff")
		spawn_projectile(false, enemy.global_position + direction * 45.0, direction.rotated(offset), damage, speed, color)
	sfx_requested.emit("enemy_shot")

func _on_enemy_special(enemy: Enemy) -> void:
	if enemy.kind == 4:
		for i in range(10):
			var direction := Vector2.from_angle(float(i) / 10.0 * TAU + enemy.elapsed * 0.2)
			spawn_projectile(false, enemy.global_position + direction * 45.0, direction, float(enemy.data.damage) * 0.72, 360.0, Color("b371f0"))
		notification.emit("VOID SENTINEL  •  RADIAL DISCHARGE", Color("bd82ed"))
	elif enemy.kind == 2:
		var direction := (player.global_position - enemy.global_position).normalized()
		for i in range(5):
			spawn_projectile(false, enemy.global_position + direction * 40.0, direction.rotated((i - 2) * 0.18), float(enemy.data.damage) * 0.65, 390.0, Color("ef8b62"))

func _on_enemy_destroyed(enemy: Enemy) -> void:
	enemies.erase(enemy)
	var reward := int(enemy.data.reward)
	game_state.add_credits(reward)
	game_state.add_xp(int(enemy.data.xp))
	game_state.record_kill()
	spawn_effect("explosion", enemy.global_position, Color("e97c68") if enemy.kind < 3 else Color("9d79ed"), 1.0 + enemy.kind * 0.14)
	sfx_requested.emit("explosion")
	notification.emit("%s DESTROYED  •  +%d CR" % [enemy.data.name, reward], Color("e8b96b"))
	if rng.randf() < 0.32:
		var sector: Dictionary = Data.SECTORS[sector_index]
		call_deferred("spawn_pickup", sector.resources[-1], 1 + enemy.kind / 2, enemy.global_position, Vector2.from_angle(rng.randf() * TAU) * 80.0)

func sync_drones() -> void:
	if drone_root == null:
		return
	drones = drones.filter(func(d): return is_instance_valid(d))
	while drones.size() < int(game_state.profile.drones):
		var drone := Drone.new()
		drone_root.add_child(drone)
		drone.configure(game_state, self, player, drones.size() + 1)
		drone.global_position = player.global_position + Vector2(-50 + drones.size() * 35, 100)
		drone.mine_requested.connect(_on_drone_mine)
		drone.destroyed.connect(_on_drone_destroyed)
		drones.append(drone)
	for drone in drones:
		drone.refresh_stats()
		drone.enabled = bool(game_state.profile.drones_active)
		drone.visible = drone.enabled

func set_drones_active(value: bool) -> void:
	game_state.profile.drones_active = value
	if not value:
		for drone in drones:
			if is_instance_valid(drone):
				drone._deposit()
	sync_drones()

func _on_drone_mine(drone: Drone, rock: Asteroid, power: float) -> void:
	mine_asteroid(rock, power, drone)
	if rng.randf() < 0.08:
		var end := rock.global_position - (rock.global_position - drone.global_position).normalized() * rock.radius * 0.6
		spawn_effect("sparks", end, Color("61d9c5"), 0.40)

func _on_drone_destroyed(drone: Drone) -> void:
	drones.erase(drone)
	game_state.profile.drones = maxi(0, int(game_state.profile.drones) - 1)
	spawn_effect("explosion", drone.global_position, Color("65d9c7"), 0.65)
	notification.emit("MINING DRONE LOST  •  REPLACEMENT AVAILABLE AT RELAY", Color("e4776c"))

func find_drone_target(from: Vector2) -> Asteroid:
	var nearest: Asteroid
	var best := 1500.0
	for rock in asteroids:
		if not is_instance_valid(rock) or rock.destroyed or rock.remaining <= 0:
			continue
		var distance := from.distance_to(rock.global_position)
		if distance < best:
			best = distance
			nearest = rock
	return nearest

func _update_spawning(delta: float) -> void:
	enemies = enemies.filter(func(e): return is_instance_valid(e) and not e.dead)
	asteroids = asteroids.filter(func(a): return is_instance_valid(a) and not a.destroyed)
	enemy_spawn_timer -= delta
	asteroid_spawn_timer -= delta
	var sector: Dictionary = Data.SECTORS[sector_index]
	if enemy_spawn_timer <= 0.0:
		if enemies.size() < int(sector.enemy_cap) and player.global_position.distance_to(Data.STATION_POSITION) > 600.0:
			spawn_enemy()
		enemy_spawn_timer = rng.randf_range(10.0, 17.0) - sector_index * 0.6
	if asteroid_spawn_timer <= 0.0:
		if asteroids.size() < int(sector.asteroids):
			for attempt in range(40):
				var position := player.global_position + Vector2.from_angle(rng.randf() * TAU) * rng.randf_range(800.0, 1450.0)
				if position.x > 100 and position.y > 100 and position.x < Data.WORLD_SIZE.x - 100 and position.y < Data.WORLD_SIZE.y - 100 and position.distance_to(Data.STATION_POSITION) > 450:
					spawn_asteroid(position)
					break
		asteroid_spawn_timer = 3.0

func _update_hazards(delta: float) -> void:
	if not player.alive:
		return
	hazard_timer -= delta
	if sector_index == 4:
		for well in wells:
			if not is_instance_valid(well):
				continue
			var offset: Vector2 = well.global_position - player.global_position
			var distance := offset.length()
			if distance < well.radius:
				player.velocity += offset.normalized() * (1.0 - distance / well.radius) * 340.0 * delta
				if distance < 55.0:
					player.take_damage(delta * 14.0)
			for drone in drones:
				if is_instance_valid(drone):
					var drone_offset: Vector2 = well.global_position - drone.global_position
					if drone_offset.length() < well.radius:
						drone.velocity += drone_offset.normalized() * (1.0 - drone_offset.length() / well.radius) * 220.0 * delta
	if hazard_timer > 0.0:
		return
	match sector_index:
		0:
			_spawn_debris(95.0)
			hazard_timer = 19.0
		1:
			spawn_effect("resonance", player.global_position, Color("b677e2"), 1.0)
			player.energy = maxf(0.0, player.energy - 24.0)
			player.take_damage(4.0)
			notification.emit("CRYSTAL RESONANCE  •  REACTOR OUTPUT DISRUPTED", Color("c68ce7"))
			sfx_requested.emit("alarm")
			hazard_timer = 14.0
		2:
			_spawn_debris(175.0)
			notification.emit("DEBRIS FRONT  •  COLLISION WARNING", Color("d89a70"))
			hazard_timer = 12.0
		3:
			var storm := Hazard.new()
			storm.kind = "storm"
			storm.position = player.global_position + Vector2.from_angle(rng.randf() * TAU) * rng.randf_range(0.0, 110.0)
			hazard_root.add_child(storm)
			storm.triggered.connect(_on_storm_triggered)
			notification.emit("PLASMA FRONT INBOUND  •  CLEAR THE MARKED ZONE", Color("85a9f5"))
			sfx_requested.emit("alarm")
			hazard_timer = 9.0
		4:
			player.velocity += Vector2.from_angle(rng.randf() * TAU) * 130.0
			notification.emit("VOID SHEAR  •  TRAJECTORY DISPLACED", Color("b47be5"))
			hazard_timer = 13.0

func _spawn_debris(speed: float) -> void:
	if player.global_position.distance_to(Data.STATION_POSITION) < 500.0:
		return
	var direction := Vector2.from_angle(rng.randf() * TAU)
	var position := player.global_position + direction * 850.0
	position.x = clampf(position.x, 80.0, Data.WORLD_SIZE.x - 80.0)
	position.y = clampf(position.y, 80.0, Data.WORLD_SIZE.y - 80.0)
	spawn_asteroid(position, rng.randf_range(20.0, 31.0), (player.global_position - position).normalized() * speed)

func _on_storm_triggered(storm: Hazard) -> void:
	spawn_effect("lightning", storm.global_position, Color("88aaff"), 1.0)
	if player.global_position.distance_to(storm.global_position) < storm.radius:
		player.take_damage(26.0 * float(game_state.profile.settings.difficulty))
		player.energy = maxf(0.0, player.energy - 30.0)
		spawn_effect("sparks", player.global_position, Color("99baff"), 1.0)
	sfx_requested.emit("explosion")

func spawn_effect(kind: String, position: Vector2, color: Color, scale_value: float = 1.0, texture: Texture2D = null) -> void:
	if not bool(game_state.profile.settings.particles) and kind in ["sparks", "collect", "resonance"]:
		return
	var effect := Effect.new()
	effect.effect_kind = kind
	effect.effect_color = color
	effect.effect_scale = scale_value
	effect.rock_texture = texture
	effect_root.add_child(effect)
	effect.global_position = position

func can_dock() -> bool:
	return is_instance_valid(player) and player.alive and player.global_position.distance_to(Data.STATION_POSITION) <= 285.0

func dock() -> void:
	if can_dock():
		player.velocity = Vector2.ZERO
		player.controls_enabled = false
		sfx_requested.emit("dock")
		dock_requested.emit()

func undock() -> void:
	player.controls_enabled = true
	player.global_position = Data.PLAYER_SPAWN
	player.velocity = Vector2.ZERO

func respawn() -> void:
	_clear_root(enemy_root)
	_clear_root(projectile_root)
	enemies.clear()
	player.global_position = Data.PLAYER_SPAWN
	player.full_repair()
	player.controls_enabled = true
	player.camera.make_current()
	sync_drones()

func _on_player_damaged(_amount: float) -> void:
	# Camera trauma is applied gently to preserve aim.
	if bool(game_state.profile.settings.screen_shake):
		player.camera.offset = Vector2(rng.randf_range(-4, 4), rng.randf_range(-4, 4))
		var tween := player.camera.create_tween()
		tween.tween_property(player.camera, "offset", Vector2.ZERO, 0.20)

func _on_player_destroyed() -> void:
	game_state.profile.stats.deaths = int(game_state.profile.stats.deaths) + 1
	# Insurance recovers most cargo; some loose ore is lost in the accident.
	for id in game_state.profile.resources.keys():
		game_state.profile.resources[id] = int(game_state.profile.resources[id]) * 3 / 4
	spawn_effect("explosion", player.global_position, Color("72dcd2"), 1.8, player.sprite.texture)
	sfx_requested.emit("explosion")
	player_died.emit()

func nearest_enemy_distance() -> float:
	var best := INF
	for enemy in enemies:
		if is_instance_valid(enemy):
			best = minf(best, player.global_position.distance_to(enemy.global_position))
	return best

func _draw() -> void:
	if game_state == null or not is_instance_valid(player):
		return
	# Docking approach and mining reticle are world-space UI, not screen stickers.
	var station_distance := player.global_position.distance_to(Data.STATION_POSITION)
	if station_distance < 800.0:
		var color := Color("65d9c7") if can_dock() else Color(0.35, 0.65, 0.65, 0.24)
		draw_arc(Data.STATION_POSITION, 275.0, elapsed * 0.12, elapsed * 0.12 + PI * 1.65, 100, color, 2.0, true)
	if is_instance_valid(scan_target) and not scan_target.destroyed:
		var color := Color(Data.get_resource(scan_target.resource_id).color)
		var r := scan_target.radius + 15.0 + sin(elapsed * 4.0) * 3.0
		for i in range(4):
			var a := i * PI / 2.0 + elapsed * 0.18
			draw_arc(scan_target.global_position, r, a, a + 0.38, 12, Color(color, 0.8), 2.0, true)
