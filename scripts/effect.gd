class_name SpaceEffect
extends Node2D

var effect_kind := "sparks"
var effect_color := Color("76e5d3")
var effect_scale := 1.0
var rock_texture: Texture2D
var age := 0.0
var duration := 1.0
var fragments: Array[Dictionary] = []
var lightning_points := PackedVector2Array()

func _ready() -> void:
	z_index = 30
	match effect_kind:
		"explosion":
			duration = 1.35
			_make_glow(2.8, 0.8)
			_make_particles(46, 1.0, 90.0, 310.0, 0.16, 0.52)
			_make_fragments()
		"sparks":
			duration = 0.55
			_make_particles(12, 0.42, 45.0, 180.0, 0.08, 0.25)
		"collect":
			duration = 0.45
			_make_glow(1.4, 0.4)
		"lightning":
			duration = 0.48
			_make_lightning()
		"resonance":
			duration = 1.2
			_make_glow(5.0, 1.0)

func _make_glow(size: float, time: float) -> void:
	var sprite := Sprite2D.new()
	sprite.texture = load("res://assets/art/particles/glow.png")
	sprite.modulate = Color(effect_color, 0.75)
	sprite.scale = Vector2.ONE * effect_scale * 0.4
	add_child(sprite)
	var tween := sprite.create_tween().set_parallel(true)
	tween.tween_property(sprite, "scale", Vector2.ONE * effect_scale * size, time).set_trans(Tween.TRANS_QUAD).set_ease(Tween.EASE_OUT)
	tween.tween_property(sprite, "modulate:a", 0.0, time).set_trans(Tween.TRANS_QUAD).set_ease(Tween.EASE_IN)

func _make_particles(count: int, lifetime: float, min_speed: float, max_speed: float, min_size: float, max_size: float) -> void:
	var particles := GPUParticles2D.new()
	particles.amount = count
	particles.lifetime = lifetime
	particles.one_shot = true
	particles.explosiveness = 0.96
	particles.local_coords = false
	particles.texture = load("res://assets/art/particles/spark.png")
	particles.modulate = effect_color
	var material := ParticleProcessMaterial.new()
	material.direction = Vector3(1, 0, 0)
	material.spread = 180.0
	material.gravity = Vector3.ZERO
	material.initial_velocity_min = min_speed * effect_scale
	material.initial_velocity_max = max_speed * effect_scale
	material.damping_min = 40.0
	material.damping_max = 100.0
	material.scale_min = min_size * effect_scale
	material.scale_max = max_size * effect_scale
	material.emission_shape = ParticleProcessMaterial.EMISSION_SHAPE_SPHERE
	material.emission_sphere_radius = 5.0 * effect_scale
	particles.process_material = material
	add_child(particles)
	particles.emitting = true

func _make_fragments() -> void:
	if rock_texture == null:
		return
	var rng := RandomNumberGenerator.new()
	rng.seed = int(global_position.x * 13.0 + global_position.y * 7.0)
	for i in range(7):
		var sprite := Sprite2D.new()
		sprite.texture = rock_texture
		sprite.scale = Vector2.ONE * rng.randf_range(0.045, 0.11) * effect_scale
		sprite.modulate = Color(0.85, 0.88, 0.90, 1.0)
		add_child(sprite)
		var angle := rng.randf() * TAU
		fragments.append({"node":sprite, "velocity":Vector2.from_angle(angle) * rng.randf_range(65.0, 190.0) * effect_scale, "spin":rng.randf_range(-5.0, 5.0)})

func _make_lightning() -> void:
	var rng := RandomNumberGenerator.new()
	rng.seed = int(global_position.x + global_position.y)
	var current := Vector2(0, -650.0 * effect_scale)
	lightning_points.append(current)
	for i in range(18):
		current += Vector2(rng.randf_range(-32, 32), 650.0 * effect_scale / 17.0)
		lightning_points.append(current)

func _process(delta: float) -> void:
	age += delta
	for fragment in fragments:
		var node: Sprite2D = fragment.node
		var velocity: Vector2 = fragment.velocity
		node.position += velocity * delta
		node.rotation += float(fragment.spin) * delta
		fragment.velocity = velocity.move_toward(Vector2.ZERO, delta * 45.0)
		node.modulate.a = maxf(0.0, 1.0 - age / duration)
	if effect_kind == "lightning":
		queue_redraw()
	if age >= duration:
		queue_free()

func _draw() -> void:
	if effect_kind != "lightning":
		return
	var alpha := maxf(0.0, 1.0 - age / duration)
	for i in range(lightning_points.size() - 1):
		draw_line(lightning_points[i], lightning_points[i + 1], Color(0.35, 0.55, 1.0, alpha * 0.22), 13.0, true)
		draw_line(lightning_points[i], lightning_points[i + 1], Color(0.55, 0.75, 1.0, alpha * 0.65), 5.0, true)
		draw_line(lightning_points[i], lightning_points[i + 1], Color(0.92, 0.96, 1.0, alpha), 1.6, true)
