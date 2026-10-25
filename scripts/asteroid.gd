class_name MineableAsteroid
extends RigidBody2D

signal depleted(asteroid: MineableAsteroid)

var sector_index := 0
var resource_id := "iron"
var resource_quantity := 20
var remaining := 20
var radius := 60.0
var max_health := 100.0
var health := 100.0
var hardness := 1.0
var destroyed := false
var yield_progress := 0.0
var hit_flash := 0.0
var variant := 0
var sprite: Sprite2D
var rock_material: ShaderMaterial
var collision: CollisionPolygon2D
var last_hit_by: Node

func _ready() -> void:
	gravity_scale = 0.0
	linear_damp = 0.18
	angular_damp = 0.12
	collision_layer = 2
	collision_mask = 1 | 2 | 4 | 16
	continuous_cd = RigidBody2D.CCD_MODE_CAST_RAY
	sprite = Sprite2D.new()
	sprite.texture_filter = CanvasItem.TEXTURE_FILTER_LINEAR_WITH_MIPMAPS
	add_child(sprite)
	rock_material = ShaderMaterial.new()
	rock_material.shader = load("res://assets/shaders/asteroid_damage.gdshader")
	sprite.material = rock_material
	collision = CollisionPolygon2D.new()
	add_child(collision)

func configure(sector: int, rock_radius: float, resource: String, rock_variant: int, rng: RandomNumberGenerator) -> void:
	sector_index = sector
	radius = rock_radius
	resource_id = resource
	variant = rock_variant
	var resource_data := GameData.get_resource(resource_id)
	hardness = float(resource_data.hardness)
	max_health = radius * 1.25 * (0.70 + hardness * 0.30)
	health = max_health
	resource_quantity = maxi(8, int(radius / 3.4 * (1.0 + float(resource_data.rarity) * 0.13)))
	remaining = resource_quantity
	sprite.texture = load("res://assets/art/asteroids/sector_%d_rock_%d.png" % [sector + 1, variant + 1])
	sprite.scale = Vector2.ONE * (radius * 2.25 / 256.0)
	rock_material.set_shader_parameter("seed", rng.randf() * 100.0)
	rock_material.set_shader_parameter("vein_color", Color(resource_data.color))
	var points := PackedVector2Array()
	for i in range(18):
		var angle := float(i) / 18.0 * TAU
		var r := radius * rng.randf_range(0.79, 1.02)
		points.append(Vector2(cos(angle), sin(angle)) * r)
	collision.polygon = points
	mass = maxf(1.0, radius / 30.0)

func apply_damage(amount: float, collector: Node) -> float:
	if destroyed or amount <= 0.0:
		return 0.0
	last_hit_by = collector
	var actual := minf(amount, health)
	health -= actual
	hit_flash = 1.0
	yield_progress += actual / max_health * float(resource_quantity)
	var damage := 1.0 - health / max_health
	rock_material.set_shader_parameter("damage", damage)
	if health <= 0.001:
		destroyed = true
		collision.set_deferred("disabled", true)
		set_deferred("freeze", true)
		depleted.emit(self)
	return actual

func take_yield() -> int:
	var units := mini(int(floor(yield_progress)), remaining)
	if units <= 0:
		return 0
	yield_progress -= units
	remaining -= units
	return units

func damage_fraction() -> float:
	return 1.0 - health / maxf(1.0, max_health)

func _process(delta: float) -> void:
	hit_flash = maxf(0.0, hit_flash - delta * 7.0)
	if rock_material != null:
		rock_material.set_shader_parameter("hit_flash", hit_flash)
