class_name PlayerShip
extends CharacterBody2D
const Data = preload("res://scripts/data.gd")
const State = preload("res://scripts/game_state.gd")

signal destroyed
signal damaged(amount: float)
signal impacted

var game_state: State
var stats: Dictionary = {}
var hull := 100.0
var shield := 40.0
var energy := 100.0
var alive := true
var controls_enabled := true
var boosting := false
var thrusting := false
var damage_timer := 0.0
var shield_impact := 0.0
var collision_cooldown := 0.0
var elapsed := 0.0
var sprite: Sprite2D
var camera: Camera2D
var thrusters: GPUParticles2D
var shield_sprite: Sprite2D
var shield_material: ShaderMaterial
var engine_light: Sprite2D

func _ready() -> void:
	collision_layer = 1
	collision_mask = 2 | 4
	var shape := CollisionShape2D.new()
	var circle := CircleShape2D.new()
	circle.radius = 35.0
	shape.shape = circle
	add_child(shape)
	sprite = Sprite2D.new()
	sprite.texture_filter = CanvasItem.TEXTURE_FILTER_LINEAR_WITH_MIPMAPS
	sprite.scale = Vector2.ONE * 0.66
	sprite.z_index = 3
	add_child(sprite)
	_setup_thrusters()
	_setup_shield()
	engine_light = Sprite2D.new()
	engine_light.texture = load("res://assets/art/particles/glow.png")
	engine_light.position = Vector2(-47, 0)
	engine_light.scale = Vector2(0.85, 0.55)
	engine_light.modulate = Color(0.25, 0.95, 0.85, 0.42)
	engine_light.z_index = 1
	add_child(engine_light)
	camera = Camera2D.new()
	camera.zoom = Vector2.ONE * 0.90
	camera.position_smoothing_enabled = true
	camera.position_smoothing_speed = 5.5
	camera.limit_left = 0
	camera.limit_top = 0
	camera.limit_right = int(Data.WORLD_SIZE.x)
	camera.limit_bottom = int(Data.WORLD_SIZE.y)
	camera.limit_smoothed = true
	add_child(camera)
	camera.make_current()

func _setup_thrusters() -> void:
	thrusters = GPUParticles2D.new()
	thrusters.amount = 42
	thrusters.lifetime = 0.48
	thrusters.local_coords = false
	thrusters.texture = load("res://assets/art/particles/thruster.png")
	thrusters.position = Vector2(-52, 0)
	thrusters.z_index = 0
	thrusters.emitting = false
	var particles := ParticleProcessMaterial.new()
	particles.direction = Vector3(-1, 0, 0)
	particles.spread = 13.0
	particles.gravity = Vector3.ZERO
	particles.initial_velocity_min = 85.0
	particles.initial_velocity_max = 160.0
	particles.damping_min = 45.0
	particles.damping_max = 80.0
	particles.scale_min = 0.18
	particles.scale_max = 0.48
	particles.emission_shape = ParticleProcessMaterial.EMISSION_SHAPE_SPHERE
	particles.emission_sphere_radius = 8.0
	thrusters.process_material = particles
	add_child(thrusters)

func _setup_shield() -> void:
	shield_sprite = Sprite2D.new()
	shield_sprite.texture = load("res://assets/art/particles/glow.png")
	shield_sprite.scale = Vector2.ONE * 3.0
	shield_sprite.z_index = 5
	shield_material = ShaderMaterial.new()
	shield_material.shader = load("res://assets/shaders/shield.gdshader")
	shield_sprite.material = shield_material
	add_child(shield_sprite)

func configure(state: State, restore: bool = false) -> void:
	var old_hull := float(stats.get("max_hull", 0.0))
	var old_shield := float(stats.get("max_shield", 0.0))
	var old_energy := float(stats.get("max_energy", 0.0))
	game_state = state
	stats = state.get_stats()
	sprite.texture = load(Data.get_ship(state.profile.current_ship).texture)
	if restore or old_hull <= 0.0:
		hull = float(stats.max_hull)
		shield = float(stats.max_shield)
		energy = float(stats.max_energy)
	else:
		hull = minf(float(stats.max_hull), hull + maxf(0.0, float(stats.max_hull) - old_hull))
		shield = minf(float(stats.max_shield), shield + maxf(0.0, float(stats.max_shield) - old_shield))
		energy = minf(float(stats.max_energy), energy + maxf(0.0, float(stats.max_energy) - old_energy))
	alive = hull > 0.0
	visible = alive
	queue_redraw()

func _physics_process(delta: float) -> void:
	if game_state == null or not alive:
		return
	elapsed += delta
	damage_timer = maxf(0.0, damage_timer - delta)
	shield_impact = maxf(0.0, shield_impact - delta * 2.8)
	collision_cooldown = maxf(0.0, collision_cooldown - delta)
	shield_material.set_shader_parameter("impact", shield_impact)
	shield_material.set_shader_parameter("shield_fraction", shield / maxf(1.0, float(stats.max_shield)))
	shield_sprite.visible = shield > 0.0 or shield_impact > 0.0
	if controls_enabled:
		var mouse := get_global_mouse_position()
		if global_position.distance_to(mouse) > 8.0:
			rotation = lerp_angle(rotation, global_position.angle_to_point(mouse), minf(1.0, delta * 12.0))
		var input := Input.get_vector("move_left", "move_right", "move_up", "move_down")
		thrusting = input.length_squared() > 0.02
		boosting = Input.is_action_pressed("boost") and thrusting and energy > 2.0
		var max_speed := float(stats.speed) * (1.55 if boosting else 1.0)
		if boosting:
			energy = maxf(0.0, energy - delta * 22.0)
		var target := input * max_speed
		var acceleration := float(stats.acceleration) * (1.4 if boosting else 1.0)
		velocity = velocity.move_toward(target, acceleration * delta)
	else:
		thrusting = false
		boosting = false
		velocity = velocity.move_toward(Vector2.ZERO, float(stats.get("acceleration", 350.0)) * delta * 0.35)
	thrusters.emitting = thrusting and controls_enabled
	thrusters.speed_scale = 1.65 if boosting else 1.0
	engine_light.modulate.a = lerpf(engine_light.modulate.a, 0.72 if thrusting else 0.24, delta * 8.0)
	engine_light.scale = Vector2(1.15 if boosting else 0.85, 0.55)
	energy = minf(float(stats.max_energy), energy + float(stats.energy_regen) * delta * (0.32 if boosting else 1.0))
	if damage_timer <= 0.0:
		shield = minf(float(stats.max_shield), shield + float(stats.shield_regen) * delta)
		hull = minf(float(stats.max_hull), hull + float(stats.nano_repair) * delta)
	var speed_before := velocity.length()
	move_and_slide()
	if collision_cooldown <= 0.0:
		for i in range(get_slide_collision_count()):
			var collision := get_slide_collision(i)
			var body := collision.get_collider()
			if body is RigidBody2D and speed_before > 185.0:
				body.apply_central_impulse(-collision.get_normal() * speed_before * 1.6)
				take_damage((speed_before - 150.0) * 0.038)
				collision_cooldown = 0.8
				impacted.emit()
				break
	global_position.x = clampf(global_position.x, 35.0, Data.WORLD_SIZE.x - 35.0)
	global_position.y = clampf(global_position.y, 35.0, Data.WORLD_SIZE.y - 35.0)

func can_collect() -> bool:
	return alive and game_state != null and game_state.cargo_free() > 0

func take_damage(amount: float) -> void:
	if not alive or amount <= 0.0:
		return
	var remaining := amount * (1.0 - float(stats.get("armor", 0.0)))
	if shield > 0.0:
		var absorbed := minf(shield, remaining)
		shield -= absorbed
		remaining -= absorbed
		shield_impact = 1.0
	if remaining > 0.0:
		hull = maxf(0.0, hull - remaining)
	damage_timer = 4.0
	damaged.emit(amount)
	if hull <= 0.0:
		alive = false
		controls_enabled = false
		thrusters.emitting = false
		visible = false
		destroyed.emit()

func full_repair() -> void:
	hull = float(stats.max_hull)
	shield = float(stats.max_shield)
	energy = float(stats.max_energy)
	alive = true
	visible = true
	velocity = Vector2.ZERO

func emitter_position() -> Vector2:
	return global_position + Vector2(72, 0).rotated(global_rotation)

func aim_direction() -> Vector2:
	return Vector2.RIGHT.rotated(global_rotation)

func _draw() -> void:
	if game_state == null:
		return
	var upgrades: Dictionary = game_state.profile.upgrades
	var accent := Color("62daca")
	# Real upgrades have visible hard-surface attachments, not only changed text.
	if int(upgrades.hull) >= 3:
		draw_arc(Vector2.ZERO, 47.0, -1.05, 1.05, 18, Color(0.45, 0.58, 0.62, 0.9), 4.0, true)
		draw_arc(Vector2.ZERO, 47.0, 2.1, 4.2, 18, Color(0.45, 0.58, 0.62, 0.9), 4.0, true)
	if int(upgrades.cargo_capacity) >= 2:
		for side in [-1, 1]:
			var rect := Rect2(-28, side * 48 - 9, 43, 18)
			draw_rect(rect, Color("293c43"), true)
			draw_rect(rect, accent.darkened(0.35), false, 2.0)
	if int(upgrades.drone_capacity) > 0:
		for i in range(mini(3, int(upgrades.drone_capacity))):
			draw_circle(Vector2(-24 + i * 15, -56), 3.5, accent)
	if int(upgrades.weapon_damage) >= 2:
		draw_line(Vector2(26, -45), Vector2(65, -57), Color("a8bec4"), 4.0, true)
		draw_line(Vector2(26, 45), Vector2(65, 57), Color("a8bec4"), 4.0, true)
