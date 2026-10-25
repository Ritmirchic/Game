extends CharacterBody3D
## First-person survivor: movement, stamina, full weapon system, interactions,
## flashlight, downed/revive co-op state and network synchronization.
class_name Player

var session
var peer_id: int = 1
var is_local: bool = false

var head: Node3D
var pitch: Node3D
var camera: Camera3D
var flashlight: SpotLight3D
var vm_holder: Node3D
var body_model: Node3D
var muzzle_light: OmniLight3D
var muzzle_fx: GPUParticles3D
var current_vm: Node3D

var max_health: float = 100.0
var health: float = 100.0
var max_stamina: float = 100.0
var stamina: float = 100.0
var downed: bool = false
var bleedout: float = 30.0
var revive_t: float = 0.0
var dead: bool = false

var owned: Array[String] = ["bat"]
var current: int = 0
var mags: Dictionary = {}
var fire_cd: float = 0.0
var reload_t: float = 0.0
var recoil_kick: float = 0.0
var look_pitch: float = 0.0
var aim_amount: float = 0.0
var bob_t: float = 0.0
var step_t: float = 0.0
var sync_t: float = 0.0
var mouse_sens: float = 0.0022
var base_fov: float = 72.0
var swing_t: float = 0.0
var focused: Node3D = null
var upgrades: Dictionary = {"max_health": 0, "max_stamina": 0, "reload_speed": 0}

const EYE_HEIGHT := 1.66
const INTERACT_RANGE := 2.7

func setup(sess, pid: int, local: bool) -> void:
	session = sess
	peer_id = pid
	is_local = local
	name = "Player_%d" % pid
	collision_layer = GameData.LAYER_PLAYER
	collision_mask = GameData.LAYER_WORLD | GameData.LAYER_BARRICADE | GameData.LAYER_ZOMBIE
	add_to_group("player")
	# body collision
	var cs := CollisionShape3D.new()
	var cap := CapsuleShape3D.new()
	cap.radius = 0.4
	cap.height = 1.75
	cs.shape = cap
	cs.position.y = 0.88
	add_child(cs)
	# head rig
	head = Node3D.new()
	head.position.y = EYE_HEIGHT
	add_child(head)
	pitch = Node3D.new()
	head.add_child(pitch)
	# third-person body (visible on remote peers / own shadow)
	body_model = session.factory.make_survivor(pid % 4)
	body_model.position.y = 0.02
	add_child(body_model)
	if is_local:
		camera = Camera3D.new()
		camera.fov = base_fov
		camera.near = 0.08
		pitch.add_child(camera)
		body_model.visible = false
		flashlight = SpotLight3D.new()
		flashlight.spot_range = 22
		flashlight.spot_angle = 38
		flashlight.spot_angle_attenuation = 1.2
		flashlight.light_energy = 5.5
		flashlight.shadow_enabled = true
		flashlight.position = Vector3(0.15, -0.12, 0)
		camera.add_child(flashlight)
		vm_holder = Node3D.new()
		camera.add_child(vm_holder)
		# muzzle flash
		muzzle_light = OmniLight3D.new()
		muzzle_light.omni_range = 7
		muzzle_light.light_color = Color(1.0, 0.8, 0.45)
		muzzle_light.light_energy = 0
		muzzle_light.position = Vector3(0, -0.1, -1.0)
		camera.add_child(muzzle_light)
		muzzle_fx = GPUParticles3D.new()
		muzzle_fx.amount = 6
		muzzle_fx.lifetime = 0.09
		var pmat := ParticleProcessMaterial.new()
		pmat.gravity = Vector3.ZERO
		pmat.initial_velocity_min = 2.5
		pmat.initial_velocity_max = 4.5
		pmat.direction = Vector3(0, 0, -1)
		pmat.spread = 25
		muzzle_fx.process_material = pmat
		var fm := SphereMesh.new()
		fm.radius = 0.03
		fm.height = 0.06
		fm.material = session.assets.plain("muzzle", Color(1, 0.85, 0.5), 0.4, 0.0, Color(1, 0.8, 0.4), 4.0)
		muzzle_fx.draw_pass_1 = fm
		muzzle_fx.one_shot = true
		muzzle_fx.emitting = false
		camera.add_child(muzzle_fx)
		_equip_viewmodel()
	# starting magazines
	for wid in owned:
		_reset_mag(wid)

func _reset_mag(wid: String) -> void:
	mags[wid] = int(session.data.WEAPONS[wid].get("mag_size", 1))

func add_weapon(wid: String) -> bool:
	if wid in owned:
		return false
	owned.append(wid)
	_reset_mag(wid)
	current = owned.size() - 1
	if is_local:
		_equip_viewmodel()
	return true

func weapon_id() -> String:
	return owned[current] if owned.size() > 0 else ""

func weapon() -> Dictionary:
	return session.data.WEAPONS[weapon_id()] if weapon_id() != "" else {}

# ------------------------------------------------------------------ lifecycle
func _ready() -> void:
	if is_local:
		Input.mouse_mode = Input.MOUSE_MODE_CAPTURED

func _exit_tree() -> void:
	if is_local:
		Input.mouse_mode = Input.MOUSE_MODE_VISIBLE

func _physics_process(delta: float) -> void:
	if not is_local:
		return
	if session.paused:
		return
	_remote_sync_tick(delta)
	if dead:
		return
	if downed:
		_downed_tick(delta)
		return
	_look(delta)
	_move(delta)
	_weapon_tick(delta)
	_interact_tick(delta)

# --------------------------------------------------------------------- look
func _look(delta: float) -> void:
	var m := Input.get_last_mouse_velocity()
	head.rotation.y -= m.x * mouse_sens
	look_pitch = clamp(look_pitch - m.y * mouse_sens, deg_to_rad(-85), deg_to_rad(85))

# ------------------------------------------------------------------- move
func _move(delta: float) -> void:
	var input := Input.get_vector("move_left", "move_right", "move_forward", "move_back")
	var dir := (head.global_transform.basis * Vector3(input.x, 0, input.y)).normalized()
	var sprinting: bool = Input.is_action_pressed("sprint") and input.y < -0.1 and stamina > 1.0
	var speed := 6.2 if not sprinting else 9.4
	if aim_amount > 0.5:
		speed *= 0.55
	if sprinting:
		stamina = max(0.0, stamina - 26 * delta)
	else:
		stamina = min(max_stamina, stamina + 16 * delta)
	velocity.x = dir.x * speed
	velocity.z = dir.z * speed
	if Input.is_action_just_pressed("jump") and is_on_floor():
		velocity.y = 5.2
	velocity.y -= 18.0 * delta
	move_and_slide()
	# footsteps + head bob
	if dir.length() > 0.1 and is_on_floor():
		bob_t += delta * speed
		step_t -= delta * speed
		if step_t <= 0.0:
			step_t = 2.6
			session.audio.play_3d("sfx_footstep", global_position, -10.0)
	if vm_holder:
		var bob_y: float = sin(bob_t * 1.6) * 0.012 * min(speed / 6.2, 1.4)
		var bob_x: float = cos(bob_t * 0.8) * 0.01 * min(speed / 6.2, 1.4)
		vm_holder.position = Vector3(bob_x, bob_y - 0.18, -0.05)
		vm_holder.rotation = Vector3(cos(bob_t) * 0.008, 0, sin(bob_t) * 0.006)

# ------------------------------------------------------------------ weapons
func _weapon_tick(delta: float) -> void:
	fire_cd -= delta
	recoil_kick = max(0.0, recoil_kick - delta * 6.0)
	# recoil is a temporary offset on top of the player's aim, not a permanent
	# camera rotation (otherwise sustained fire drifts the view into the sky)
	pitch.rotation.x = clamp(look_pitch + recoil_kick * 0.6, deg_to_rad(-85), deg_to_rad(85))
	muzzle_light.light_energy = max(0.0, muzzle_light.light_energy - delta * 40.0)
	# weapon switching
	for i in 7:
		if Input.is_action_just_pressed("weapon_%d" % (i + 1)) and i < owned.size():
			_switch_weapon(i)
	# aim
	var aiming: bool = Input.is_action_pressed("aim") and weapon().get("kind", "") == "gun"
	aim_amount = lerp(aim_amount, 1.0 if aiming else 0.0, delta * 10.0)
	camera.fov = lerp(camera.fov, base_fov / float(weapon().get("zoom", 1.0)) if aiming else base_fov, delta * 8.0)
	if current_vm:
		var aim_pos := Vector3(0, -0.14, -0.32)
		var hip_pos := Vector3(0.22, -0.24, -0.45)
		current_vm.position = current_vm.position.lerp(hip_pos.lerp(aim_pos, aim_amount), delta * 12.0)
		current_vm.rotation.x = recoil_kick * 0.9 + (swing_t * 0.9 if swing_t > 0 else 0.0)
	# reload
	if reload_t > 0.0:
		reload_t -= delta
		if reload_t <= 0.0:
			_finish_reload()
	elif Input.is_action_just_pressed("reload") and weapon().get("kind", "") == "gun":
		_start_reload()
	# firing
	if swing_t > 0.0:
		swing_t -= delta
	var want_fire: bool
	if weapon().get("auto", false):
		want_fire = Input.is_action_pressed("fire")
	else:
		want_fire = Input.is_action_just_pressed("fire")
	if want_fire and fire_cd <= 0.0 and reload_t <= 0.0:
		_fire()
	if Input.is_action_just_pressed("flashlight"):
		flashlight.visible = not flashlight.visible
		session.audio.play_ui("sfx_ui_click")

func _switch_weapon(i: int) -> void:
	if i == current:
		return
	current = i
	reload_t = 0.0
	_equip_viewmodel()
	session.audio.play_ui("sfx_ui_click")
	session.ui.hud_flash("Switched to " + weapon().get("name", ""))

func _equip_viewmodel() -> void:
	if current_vm:
		current_vm.queue_free()
	current_vm = session.factory.make_weapon(weapon_id())
	vm_holder.add_child(current_vm)
	current_vm.position = Vector3(0.22, -0.24, -0.45)

func _start_reload() -> void:
	var w := weapon()
	var ammo_type: String = w["ammo_type"]
	if mags[weapon_id()] >= w["mag_size"] or int(session.resources["ammo"].get(ammo_type, 0)) <= 0:
		return
	var mult: float = 1.0 - 0.15 * upgrades["reload_speed"]
	reload_t = float(w["reload_time"]) * mult
	session.audio.play_3d("sfx_reload", global_position, -8.0)

func _finish_reload() -> void:
	var w := weapon()
	var ammo_type: String = w["ammo_type"]
	var need: int = int(w["mag_size"]) - int(mags[weapon_id()])
	var take: int = min(need, int(session.resources["ammo"].get(ammo_type, 0)))
	session.resources["ammo"][ammo_type] = int(session.resources["ammo"][ammo_type]) - take
	mags[weapon_id()] = int(mags[weapon_id()]) + take

func _fire() -> void:
	var w := weapon()
	var wid := weapon_id()
	if w.get("kind", "") == "melee":
		fire_cd = float(w["swing_time"])
		swing_t = float(w["swing_time"])
		session.audio.play_3d(w["sound_swing"], global_position, -6.0)
		_melee_hit(w)
		return
	if int(mags.get(wid, 0)) <= 0:
		fire_cd = 0.25
		session.audio.play_3d("sfx_empty_click", global_position, -8.0)
		return
	fire_cd = 60.0 / float(w["rpm"])
	mags[wid] = int(mags[wid]) - 1
	recoil_kick += float(w.get("recoil", 0.01))
	muzzle_light.light_energy = 6.0
	muzzle_fx.emitting = false
	muzzle_fx.emitting = true
	if not w.get("silent", false):
		session.audio.play_3d(w["sound"], global_position)
	else:
		session.audio.play_3d(w["sound"], global_position, -14.0)
	var pellets: int = int(w.get("pellets", 1))
	for p in pellets:
		_hitscan(w)

func _hitscan(w: Dictionary) -> void:
	var spread: float = float(w.get("spread", 0.0)) * (1.4 if aim_amount < 0.5 else 0.5)
	var dir := -camera.global_transform.basis.z
	dir = (dir + Vector3(randf_range(-spread, spread), randf_range(-spread, spread), randf_range(-spread, spread))).normalized()
	var from := camera.global_position
	var space := get_world_3d().direct_space_state
	var q := PhysicsRayQueryParameters3D.create(from, from + dir * float(w.get("range", 50)))
	q.collision_mask = GameData.LAYER_ZOMBIE | GameData.LAYER_WORLD | GameData.LAYER_BARRICADE
	q.exclude = [get_rid()]
	var hit := space.intersect_ray(q)
	var end := from + dir * float(w.get("range", 50))
	if not hit.is_empty():
		end = hit["position"]
		var collider = hit["collider"]
		if collider is Zombie:
			session.apply_zombie_damage(collider, float(w["damage"]), hit["position"], dir)
		elif collider is Barricade:
			collider.take_damage(float(w["barricade_damage"]) if w.has("barricade_damage") else 6.0)
			session.spawn_spark(end)
		else:
			session.spawn_spark(end)
	else:
		session.spawn_spark(end)
	session.spawn_tracer(muzzle_light.global_position, end)

func _melee_hit(w: Dictionary) -> void:
	var from := global_position + Vector3(0, 1.3, 0)
	var fwd := -head.global_transform.basis.z
	var best: Zombie = null
	var best_d := float(w["range"])
	for z in session.zombies:
		if z.state == Zombie.State.DEAD:
			continue
		var to: Vector3 = z.global_position - from
		if to.length() > best_d:
			continue
		if to.normalized().dot(fwd) < 0.35:
			continue
		best = z
		best_d = to.length()
	if best:
		session.apply_zombie_damage(best, float(w["damage"]), best.global_position + Vector3.UP, fwd * 3.0)
		session.audio.play_3d(w["sound_hit"], best.global_position)

# --------------------------------------------------------------- interaction
func _interact_tick(_delta: float) -> void:
	focused = _find_focus()
	if Input.is_action_just_pressed("interact") and focused:
		if focused.has_method("interact"):
			focused.interact(self)
		elif focused.is_in_group("shop_terminal"):
			session.ui.open_shop(self)

func _find_focus() -> Node3D:
	var best: Node3D = null
	var best_d := INTERACT_RANGE
	var check: Array = []
	check.append_array(get_tree().get_nodes_in_group("interactable"))
	check.append_array(get_tree().get_nodes_in_group("shop_terminal"))
	var from := camera.global_position
	var fwd := -camera.global_transform.basis.z
	for n in check:
		if not (n is Node3D) or not n.visible:
			continue
		if n.has_method("can_interact") and not n.can_interact(self):
			continue
		var to: Vector3 = (n as Node3D).global_position + Vector3.UP - from
		var d := to.length()
		if d > INTERACT_RANGE + 0.6:
			continue
		if d < best_d and to.normalized().dot(fwd) > 0.25:
			best = n
			best_d = d
	return best

# ------------------------------------------------------------ health / revive
func take_damage(amount: float, from_pos: Vector3) -> void:
	if dead or downed:
		return
	health -= amount
	if multiplayer != null and multiplayer.is_server():
		_rpc_health.rpc(health, false)
	session.audio.play_3d("sfx_hit", global_position, -4.0)
	_damage_feedback()
	if health <= 0.0:
		_begin_down(from_pos)

@rpc("authority", "call_remote", "reliable")
func _rpc_health(hp: float, is_downed: bool) -> void:
	if not is_local:
		health = hp
		downed = is_downed

func _damage_feedback() -> void:
	if is_local and session.ui:
		session.ui.damage_flash()

func _begin_down(_from_pos: Vector3) -> void:
	downed = true
	health = 0.0
	bleedout = 30.0
	if multiplayer != null and multiplayer.is_server():
		_rpc_health.rpc(0.0, true)
	if session.is_singleplayer():
		session.game_over()
	else:
		session.notify("You are downed! Teammates can revive you.")
		if is_local:
			Input.mouse_mode = Input.MOUSE_MODE_CAPTURED

func _downed_tick(delta: float) -> void:
	bleedout -= delta
	# look around slowly only
	_look(delta)
	var reviver := _find_reviver()
	if reviver:
		revive_t += delta
		if revive_t >= 3.0:
			_revive()
	else:
		revive_t = max(0.0, revive_t - delta)
	if bleedout <= 0.0:
		dead = true
		session.on_player_dead(self)

func _find_reviver() -> Player:
	for p in session.players:
		if p == self or not p.is_local or p.dead or p.downed:
			continue
		if p.global_position.distance_to(global_position) < 2.2 and Input.is_action_pressed("interact"):
			return p
	return null

func _revive() -> void:
	downed = false
	revive_t = 0.0
	health = max_health * 0.4
	if multiplayer != null and multiplayer.is_server():
		_rpc_health.rpc(health, false)
	session.notify("Revived!")

func heal(amount: float) -> void:
	health = min(max_health, health + amount)

func apply_upgrade(id: String) -> void:
	upgrades[id] += 1
	match id:
		"max_health":
			max_health += 25
			health += 25
		"max_stamina":
			max_stamina += 20
			stamina += 20

# -------------------------------------------------------------- net movement
func _remote_sync_tick(delta: float) -> void:
	sync_t -= delta
	if sync_t > 0.0 or multiplayer == null:
		return
	sync_t = 0.05
	_sync_transform.rpc(global_position, head.rotation.y, pitch.rotation.x, velocity)

@rpc("any_peer", "unreliable", "call_remote")
func _sync_transform(p: Vector3, yaw: float, look_pitch: float, v: Vector3) -> void:
	if is_local or not multiplayer:
		return
	global_position = global_position.lerp(p, 0.35)
	head.rotation.y = yaw
	self.look_pitch = look_pitch
	pitch.rotation.x = look_pitch
	velocity = v
	if body_model:
		body_model.rotation.y = yaw

func set_remote_data(d: Dictionary) -> void:
	if d.has("health"):
		health = float(d["health"])
		downed = bool(d.get("downed", false))
