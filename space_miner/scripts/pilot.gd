extends CharacterBody2D
var thrust = Vector2.ZERO
var acceleration = 430.0
var top_speed = 290.0
var active = false
func _physics_process(delta):
 if active:
  thrust=Vector2(float(Input.is_physical_key_pressed(KEY_D))-float(Input.is_physical_key_pressed(KEY_A)),float(Input.is_physical_key_pressed(KEY_S))-float(Input.is_physical_key_pressed(KEY_W))).normalized()
  velocity=velocity.move_toward(thrust*top_speed,acceleration*delta)
  rotation=lerp_angle(rotation,(get_global_mouse_position()-global_position).angle(),minf(delta*9,1))
 else:
  thrust=Vector2.ZERO
  velocity=velocity.move_toward(Vector2.ZERO,acceleration*delta)
 move_and_slide()
 position=position.limit_length(2150)
