class_name ResourcePickup
extends Area2D

signal collected(pickup: ResourcePickup, collector: Node)

var resource_id := "iron"
var amount := 1
var velocity := Vector2.ZERO
var attract_target: Node2D
var age := 0.0
var life := 42.0
var claimed := false
var sprite: Sprite2D
var glow: Sprite2D

func _ready() -> void:
	collision_layer = 8
	collision_mask = 1 | 16
	monitoring = true
	var shape := CollisionShape2D.new()
	var circle := CircleShape2D.new()
	circle.radius = 22.0
	shape.shape = circle
	add_child(shape)
	glow = Sprite2D.new()
	glow.texture = load("res://assets/art/particles/glow.png")
	glow.scale = Vector2.ONE * 0.58
	add_child(glow)
	sprite = Sprite2D.new()
	sprite.scale = Vector2.ONE * 0.37
	add_child(sprite)
	body_entered.connect(_on_body_entered)

func configure(id: String, quantity: int, initial_velocity: Vector2) -> void:
	resource_id = id
	amount = quantity
	velocity = initial_velocity
	sprite.texture = load("res://assets/art/icons/resource_%s.png" % id)
	var color := Color(GameData.get_resource(id).color)
	glow.modulate = Color(color, 0.48)

func _physics_process(delta: float) -> void:
	if claimed:
		return
	age += delta
	if age >= life:
		queue_free()
		return
	if age > life - 5.0:
		modulate.a = maxf(0.0, (life - age) / 5.0)
	velocity = velocity.move_toward(Vector2.ZERO, delta * 35.0)
	if is_instance_valid(attract_target):
		var offset := attract_target.global_position - global_position
		var distance := offset.length()
		if distance < 420.0:
			velocity += offset.normalized() * (780.0 + (420.0 - distance) * 3.0) * delta
			velocity = velocity.limit_length(560.0)
		if distance < 35.0:
			if not attract_target.has_method("can_collect") or attract_target.can_collect():
				_collect(attract_target)
			return
	global_position += velocity * delta
	sprite.rotation += delta * 1.6
	glow.scale = Vector2.ONE * (0.55 + sin(age * 5.0) * 0.08)

func _on_body_entered(body: Node) -> void:
	if body is PlayerShip or body is MiningDrone:
		if body.has_method("can_collect") and not body.can_collect():
			return
		_collect(body)

func _collect(collector: Node) -> void:
	if claimed:
		return
	claimed = true
	set_deferred("monitoring", false)
	collected.emit(self, collector)
	call_deferred("queue_free")
