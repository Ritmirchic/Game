class_name PulseProjectile
extends Area2D

signal hit(projectile: PulseProjectile, body: Node)

var friendly := true
var damage := 10.0
var velocity := Vector2.ZERO
var projectile_color := Color("69e6d2")
var life := 2.6
var age := 0.0
var sprite: Sprite2D
var trail: Line2D

func _ready() -> void:
	collision_layer = 32
	collision_mask = (4 | 2) if friendly else (1 | 16)
	monitoring = true
	var shape := CollisionShape2D.new()
	var circle := CircleShape2D.new()
	circle.radius = 8.0 if friendly else 11.0
	shape.shape = circle
	add_child(shape)
	sprite = Sprite2D.new()
	sprite.texture = load("res://assets/art/particles/glow.png" if friendly else "res://assets/art/particles/plasma.png")
	sprite.scale = Vector2(0.46, 0.25)
	sprite.modulate = projectile_color
	add_child(sprite)
	trail = Line2D.new()
	trail.points = PackedVector2Array([Vector2(-32, 0), Vector2(4, 0)])
	trail.width = 4.0 if friendly else 6.0
	trail.default_color = Color(projectile_color, 0.65)
	add_child(trail)
	rotation = velocity.angle()
	body_entered.connect(_on_body_entered)

func _physics_process(delta: float) -> void:
	age += delta
	if age >= life:
		queue_free()
		return
	global_position += velocity * delta
	if global_position.x < -100 or global_position.y < -100 or global_position.x > GameData.WORLD_SIZE.x + 100 or global_position.y > GameData.WORLD_SIZE.y + 100:
		queue_free()

func _on_body_entered(body: Node) -> void:
	if body == self:
		return
	set_deferred("monitoring", false)
	hit.emit(self, body)
	call_deferred("queue_free")
