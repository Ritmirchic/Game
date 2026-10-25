class_name EnemyShip
extends CharacterBody2D

signal fire_requested(enemy: EnemyShip, direction: Vector2, damage: float, speed: float)
signal special_attack(enemy: EnemyShip)
signal destroyed(enemy: EnemyShip)

var kind := 0
var data: Dictionary = {}
var player: PlayerShip
var health := 50.0
var max_health := 50.0
var shield := 0.0
var max_shield := 0.0
var difficulty := 1.0
var active := false
var dead := false
var fire_cooldown := 1.0
var special_timer := 3.0
var contact_timer := 0.0
var damage_timer := 0.0
var elapsed := 0.0
var orbit_sign := 1.0
var hit_flash := 0.0
var sprite: Sprite2D
var engine: Sprite2D

func _ready() -> void:
	collision_layer = 4
	collision_mask = 2
	var shape := CollisionShape2D.new()
	var circle := CircleShape2D.new()
	circle.radius = 38.0
	shape.shape = circle
	add_child(shape)
	engine = Sprite2D.new()
	engine.texture = load("res://assets/art/particles/plasma.png")
	engine.position = Vector2(-40, 0)
	engine.scale = Vector2(0.55, 0.32)
	engine.modulate = Color(1.0, 0.32, 0.24, 0.55)
	add_child(engine)
	sprite = Sprite2D.new()
	add_child(sprite)
	orbit_sign = -1.0 if int(get_instance_id()) % 2 == 0 else 1.0

func configure(enemy_kind: int, player_ref: PlayerShip, difficulty_scale: float) -> void:
	kind = clampi(enemy_kind, 0, GameData.ENEMIES.size() - 1)
	data = GameData.ENEMIES[kind]
	player = player_ref
	difficulty = difficulty_scale
	max_health = float(data.health) * (0.85 + difficulty * 0.15)
	health = max_health
	max_shield = float(data.shield) * difficulty
	shield = max_shield
	sprite.texture = load(data.texture)
	sprite.scale = Vector2.ONE * float(data.scale)
	engine.position.x = -43.0 * float(data.scale)
	fire_cooldown = 0.6 + float(get_instance_id() % 10) * 0.1
	special_timer = 2.5 + float(get_instance_id() % 5) * 0.35

func _physics_process(delta: float) -> void:
	if dead or not is_instance_valid(player) or not player.alive:
		velocity = velocity.move_toward(Vector2.ZERO, delta * 80.0)
		move_and_slide()
		return
	elapsed += delta
	fire_cooldown -= delta
	special_timer -= delta
	contact_timer -= delta
	damage_timer -= delta
	var offset := player.global_position - global_position
	var distance := offset.length()
	active = distance < 1550.0
	if not active:
		velocity = velocity.move_toward(Vector2.ZERO, delta * 60.0)
		move_and_slide()
		return
	var direction := offset.normalized()
	var tangent := direction.orthogonal() * orbit_sign
	var desired := Vector2.ZERO
	var speed := float(data.speed)
	match String(data.behavior):
		"orbit":
			desired = direction * clampf((distance - 320.0) * 1.8, -speed, speed) + tangent * speed * 0.78
		"strafe":
			desired = direction * clampf((distance - 410.0) * 1.5, -speed, speed) + tangent * sin(elapsed * 1.7) * speed
		"siege":
			desired = direction * clampf((distance - 560.0) * 0.9, -speed * 0.6, speed) + tangent * speed * 0.16
			if special_timer <= 0.0 and distance < float(data.range):
				special_attack.emit(self)
				special_timer = 5.2
		"hunter":
			desired = direction * clampf((distance - 260.0) * 2.1, -speed, speed) + tangent * speed * 0.45
			if special_timer <= 0.0:
				velocity += direction * speed * 2.0
				special_timer = 4.0
		"sentinel":
			desired = direction * clampf((distance - 630.0) * 0.9, -speed, speed) + tangent * speed * 0.35
			if special_timer <= 0.0 and distance < 900.0:
				special_attack.emit(self)
				special_timer = 5.8
	velocity = velocity.move_toward(desired, delta * speed * 1.8)
	velocity = velocity.limit_length(speed * (1.75 if String(data.behavior) == "hunter" else 1.0))
	rotation = lerp_angle(rotation, direction.angle(), minf(1.0, delta * 5.0))
	engine.modulate.a = 0.35 + minf(0.5, velocity.length() / maxf(1.0, speed) * 0.45)
	move_and_slide()
	if distance < float(data.range) and fire_cooldown <= 0.0:
		fire_requested.emit(self, direction, float(data.damage) * difficulty, 480.0 + kind * 45.0)
		fire_cooldown = float(data.fire_rate) * (1.15 - difficulty * 0.15)
	if distance < 72.0 and contact_timer <= 0.0:
		player.take_damage(float(data.damage) * 0.75 * difficulty)
		velocity = -direction * speed * 1.8
		contact_timer = 1.4
	if damage_timer <= 0.0 and max_shield > 0.0:
		shield = minf(max_shield, shield + delta * (2.0 + kind))

func take_damage(amount: float) -> void:
	if dead or amount <= 0.0:
		return
	var remaining := amount
	if shield > 0.0:
		var absorbed := minf(shield, remaining)
		shield -= absorbed
		remaining -= absorbed
	health -= remaining
	hit_flash = 1.0
	damage_timer = 4.0
	if health <= 0.0:
		dead = true
		destroyed.emit(self)
		call_deferred("queue_free")

func _process(delta: float) -> void:
	hit_flash = maxf(0.0, hit_flash - delta * 7.0)
	if sprite != null:
		sprite.modulate = Color(1.0, 1.0 - hit_flash * 0.45, 1.0 - hit_flash * 0.55)
	queue_redraw()

func _draw() -> void:
	if data.is_empty() or not active or dead:
		return
	var width := 74.0
	var y := -67.0 * float(data.get("scale", 1.0))
	if health < max_health or shield < max_shield:
		draw_rect(Rect2(-width / 2, y, width, 5), Color(0.05, 0.08, 0.11, 0.85), true)
		draw_rect(Rect2(-width / 2, y, width * clampf(health / max_health, 0, 1), 5), Color("e26763"), true)
		if max_shield > 0.0:
			draw_rect(Rect2(-width / 2, y - 7, width, 3), Color(0.05, 0.08, 0.11, 0.85), true)
			draw_rect(Rect2(-width / 2, y - 7, width * clampf(shield / max_shield, 0, 1), 3), Color("6a9fe8"), true)
	# Target brackets make enemy lock-on legible without obscuring the sprite.
	var color := Color(0.95, 0.38, 0.34, 0.48)
	var r := 53.0 * float(data.get("scale", 1.0))
	for sx in [-1, 1]:
		for sy in [-1, 1]:
			var corner := Vector2(sx * r, sy * r)
			draw_line(corner, corner - Vector2(sx * 9, 0), color, 1.5, true)
			draw_line(corner, corner - Vector2(0, sy * 9), color, 1.5, true)
