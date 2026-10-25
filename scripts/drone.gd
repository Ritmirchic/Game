class_name MiningDrone
extends CharacterBody2D
const Data = preload("res://scripts/data.gd")
const State = preload("res://scripts/game_state.gd")
const Ship = preload("res://scripts/ship.gd")
const Asteroid = preload("res://scripts/asteroid.gd")

signal mine_requested(drone: MiningDrone, asteroid: Asteroid, power: float)
signal destroyed(drone: MiningDrone)

var world: Node2D
var player: Ship
var game_state: State
var drone_id := 1
var state := "SEEK"
var target: Asteroid
var carried: Dictionary = {}
var carried_total := 0
var total_deposited := 0
var mined_this_trip := 0
var capacity := 12
var mining_power := 12.0
var flight_speed := 190.0
var health := 60.0
var enabled := true
var collect_timer := 0.0
var orbit_offset := 0.0
var sprite: Sprite2D
var beam: Line2D
var beam_glow: Line2D
var label: Label
var thrusters: GPUParticles2D
var elapsed := 0.0

func _ready() -> void:
	collision_layer = 16
	collision_mask = 2
	var shape := CollisionShape2D.new()
	var circle := CircleShape2D.new()
	circle.radius = 20.0
	shape.shape = circle
	add_child(shape)
	sprite = Sprite2D.new()
	sprite.texture = load("res://assets/art/ships/mining_drone.png")
	sprite.scale = Vector2.ONE * 0.58
	add_child(sprite)
	beam_glow = Line2D.new()
	beam_glow.width = 9.0
	beam_glow.default_color = Color(0.20, 0.95, 0.78, 0.13)
	beam_glow.visible = false
	beam_glow.z_index = -1
	add_child(beam_glow)
	beam = Line2D.new()
	beam.width = 2.5
	beam.default_color = Color(0.43, 1.0, 0.83, 0.94)
	beam.visible = false
	beam.z_index = 2
	add_child(beam)
	label = Label.new()
	label.position = Vector2(-54, -63)
	label.add_theme_font_override("font", load("res://assets/fonts/SpaceMono-Regular.ttf"))
	label.add_theme_font_size_override("font_size", 11)
	label.add_theme_color_override("font_color", Color("8ed7cd"))
	add_child(label)
	_setup_thrusters()

func _setup_thrusters() -> void:
	thrusters = GPUParticles2D.new()
	thrusters.amount = 18
	thrusters.lifetime = 0.32
	thrusters.local_coords = false
	thrusters.texture = load("res://assets/art/particles/glow.png")
	thrusters.position = Vector2(-22, 0)
	thrusters.z_index = -1
	var material := ParticleProcessMaterial.new()
	material.direction = Vector3(-1, 0, 0)
	material.spread = 18.0
	material.gravity = Vector3.ZERO
	material.initial_velocity_min = 45.0
	material.initial_velocity_max = 85.0
	material.scale_min = 0.08
	material.scale_max = 0.22
	thrusters.process_material = material
	add_child(thrusters)

func configure(state_ref: State, world_ref: Node2D, player_ref: Ship, id: int) -> void:
	game_state = state_ref
	world = world_ref
	player = player_ref
	drone_id = id
	orbit_offset = float(id) * 2.1
	refresh_stats()
	carried = Data.empty_resource_map()

func refresh_stats() -> void:
	var stats := game_state.get_stats()
	capacity = int(stats.drone_cargo)
	mining_power = float(stats.drone_power)
	flight_speed = float(stats.drone_speed)

func _physics_process(delta: float) -> void:
	if not enabled or not is_instance_valid(player) or not player.alive:
		beam.visible = false
		beam_glow.visible = false
		thrusters.emitting = false
		velocity = velocity.move_toward(Vector2.ZERO, delta * 200.0)
		return
	elapsed += delta
	beam.visible = false
	beam_glow.visible = false
	var desired := Vector2.ZERO
	match state:
		"SEEK":
			if not is_instance_valid(target) or target.destroyed:
				target = world.find_drone_target(global_position)
			if is_instance_valid(target):
				state = "MINE"
			else:
				var orbit := player.global_position + Vector2(110 + drone_id * 24, 0).rotated(elapsed * 0.5 + orbit_offset)
				desired = (orbit - global_position).limit_length(flight_speed)
		"MINE":
			if not is_instance_valid(target) or target.destroyed:
				target = null
				state = "SEEK"
			elif mined_this_trip >= capacity:
				state = "COLLECT"
				collect_timer = 1.7
			else:
				var offset := target.global_position - global_position
				if offset.length() > target.radius + 65.0:
					desired = offset.normalized() * flight_speed
				else:
					desired = -offset.normalized() * 12.0
					beam.visible = true
					beam_glow.visible = true
					var end := to_local(target.global_position)
					beam.points = PackedVector2Array([Vector2(20, 0).rotated(rotation), end])
					beam_glow.points = beam.points
					mine_requested.emit(self, target, mining_power * delta)
		"COLLECT":
			collect_timer -= delta
			desired = Vector2.ZERO
			if collect_timer <= 0.0:
				state = "RETURN"
		"RETURN":
			var dock := player.global_position + Vector2(-45 + drone_id * 18, 65 + drone_id * 9).rotated(player.rotation)
			var offset := dock - global_position
			if offset.length() > 62.0:
				desired = offset.normalized() * flight_speed
			else:
				_deposit()
				if carried_total <= 0:
					mined_this_trip = 0
					target = null
					state = "SEEK"
				else:
					desired = Vector2.ZERO
	velocity = velocity.move_toward(desired, delta * flight_speed * 3.0)
	if velocity.length_squared() > 100.0:
		rotation = lerp_angle(rotation, velocity.angle(), minf(1.0, delta * 8.0))
	elif state == "MINE" and is_instance_valid(target):
		rotation = lerp_angle(rotation, global_position.angle_to_point(target.global_position), minf(1.0, delta * 8.0))
	thrusters.emitting = velocity.length() > 30.0
	move_and_slide()
	label.rotation = -rotation
	label.text = "D-%02d  %s  %d/%d" % [drone_id, state, carried_total, capacity]

func note_mined(units: int) -> void:
	mined_this_trip += units

func can_collect() -> bool:
	return enabled and carried_total < capacity

func add_carried(id: String, amount: int) -> int:
	var accepted := mini(amount, capacity - carried_total)
	if accepted <= 0:
		return 0
	carried[id] = int(carried.get(id, 0)) + accepted
	carried_total += accepted
	return accepted

func _deposit() -> void:
	for id in carried.keys():
		var amount := int(carried[id])
		if amount <= 0:
			continue
		var accepted := game_state.add_resource(id, amount)
		carried[id] = amount - accepted
		carried_total -= accepted
		total_deposited += accepted

func take_damage(amount: float) -> void:
	health -= amount
	if health <= 0.0:
		enabled = false
		destroyed.emit(self)
		call_deferred("queue_free")

func recall() -> void:
	target = null
	state = "RETURN"
