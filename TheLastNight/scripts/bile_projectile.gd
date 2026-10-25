extends Area3D
## Boss bile projectile. Only the authority deals damage; all peers render it.
class_name BileProjectile

var velocity: Vector3
var damage: float
var life: float = 3.0
var session

func setup(sess, origin: Vector3, dir: Vector3, dmg: float) -> void:
	session = sess
	damage = dmg
	position = origin
	velocity = dir * 13.0
	collision_layer = 0
	collision_mask = GameData.LAYER_PLAYER | GameData.LAYER_WORLD
	monitoring = true
	var sphere := SphereMesh.new()
	sphere.radius = 0.16
	sphere.height = 0.32
	sphere.material = sess.assets.plain("bile", Color(0.35, 0.5, 0.15), 0.5, 0.0, Color(0.5, 0.9, 0.2), 1.4)
	var mi := MeshInstance3D.new()
	mi.mesh = sphere
	add_child(mi)
	var light := OmniLight3D.new()
	light.omni_range = 2.5
	light.light_color = Color(0.5, 1.0, 0.3)
	light.light_energy = 1.5
	add_child(light)
	body_entered.connect(_on_hit)

func _physics_process(delta: float) -> void:
	life -= delta
	velocity.y -= 9.5 * delta
	position += velocity * delta
	if life <= 0.0 or position.y < 0.05:
		queue_free()

func _on_hit(body: Node3D) -> void:
	if session != null and session.is_authority() and body is Player:
		(body as Player).take_damage(damage, global_position)
		session.spawn_spark(global_position)
	queue_free()
