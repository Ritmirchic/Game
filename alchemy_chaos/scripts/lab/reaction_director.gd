class_name ReactionDirector
extends Control

signal reaction_sequence_finished(outcome: Dictionary)

const CAULDRON_CENTER := Vector2(1156, 560)
const PEDESTAL_POS := [
	Vector2(830, 742),
	Vector2(1482, 742),
	Vector2(1156, 878),
]

var stage_ref: LaboratoryStage
var shockwave_rect: ColorRect
var shockwave_mat: ShaderMaterial
var lightning_layer: Node2D
var orb_layer: Control

var emit_bubbles: GPUParticles2D
var emit_smoke: GPUParticles2D
var emit_sparks: GPUParticles2D
var emit_crystals: GPUParticles2D
var emit_frost: GPUParticles2D
var emit_droplets: GPUParticles2D
var emit_runes: GPUParticles2D

var anim_player: AnimationPlayer
var _electric_timer: float = 0.0
var _electric_bolts: Array[Line2D] = []
var is_playing: bool = false


func _ready() -> void:
	set_anchors_preset(Control.PRESET_FULL_RECT)
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	_build_visual_nodes()


func _build_emitter(tex_name: String, amount: int, lifetime: float, one_shot: bool, explosiveness: float, vel_min: float, vel_max: float, grav: Vector3, spread_deg: float, dir: Vector3, sc_min: float, sc_max: float) -> GPUParticles2D:
	var p := GPUParticles2D.new()
	p.position = CAULDRON_CENTER
	p.amount = amount
	p.lifetime = lifetime
	p.one_shot = one_shot
	p.explosiveness = explosiveness
	p.emitting = false
	p.texture = AssetVault.tex("res://assets/particles/" + tex_name + ".png")
	var mat := ParticleProcessMaterial.new()
	mat.direction = dir
	mat.spread = spread_deg
	mat.initial_velocity_min = vel_min
	mat.initial_velocity_max = vel_max
	mat.gravity = grav
	mat.scale_min = sc_min
	mat.scale_max = sc_max
	mat.emission_shape = ParticleProcessMaterial.EMISSION_SHAPE_SPHERE
	mat.emission_sphere_radius = 48.0
	p.process_material = mat
	add_child(p)
	return p


func _build_visual_nodes() -> void:
	shockwave_rect = ColorRect.new()
	shockwave_rect.position = CAULDRON_CENTER - Vector2(260, 260)
	shockwave_rect.size = Vector2(520, 520)
	shockwave_rect.pivot_offset = Vector2(260, 260)
	shockwave_rect.mouse_filter = Control.MOUSE_FILTER_IGNORE
	shockwave_mat = ShaderMaterial.new()
	shockwave_mat.shader = AssetVault.shader("res://shaders/reaction_burst.gdshader")
	shockwave_mat.set_shader_parameter("progress", 0.0)
	shockwave_rect.material = shockwave_mat
	add_child(shockwave_rect)

	emit_bubbles = _build_emitter("part_bubble", 38, 1.1, true, 0.35, 55.0, 130.0, Vector3(0, -65, 0), 42.0, Vector3(0, -1, 0), 0.35, 0.90)
	emit_smoke = _build_emitter("part_smoke", 34, 1.35, true, 0.45, 45.0, 115.0, Vector3(0, -45, 0), 65.0, Vector3(0, -1, 0), 0.65, 1.75)
	emit_sparks = _build_emitter("part_spark", 52, 0.85, true, 0.85, 160.0, 360.0, Vector3(0, 380, 0), 140.0, Vector3(0, -1, 0), 0.30, 0.85)
	emit_crystals = _build_emitter("part_crystal", 32, 1.05, true, 0.80, 95.0, 220.0, Vector3(0, 40, 0), 180.0, Vector3(0, -1, 0), 0.45, 1.10)
	emit_frost = _build_emitter("part_frost", 36, 1.25, true, 0.55, 50.0, 145.0, Vector3(0, 18, 0), 180.0, Vector3(0, -1, 0), 0.45, 1.05)
	emit_droplets = _build_emitter("part_droplet", 36, 0.90, true, 0.65, 95.0, 220.0, Vector3(0, 320, 0), 85.0, Vector3(0, -1, 0), 0.35, 0.75)
	emit_runes = _build_emitter("part_arcane_rune", 24, 1.25, true, 0.60, 55.0, 140.0, Vector3(0, -35, 0), 150.0, Vector3(0, -1, 0), 0.50, 1.15)

	lightning_layer = Node2D.new()
	add_child(lightning_layer)
	for i in range(6):
		var ln := Line2D.new()
		ln.width = 4.0 if i % 2 == 0 else 2.5
		ln.default_color = Color("#7AF8FF") if i % 2 == 0 else Color("#FFF89C")
		ln.visible = false
		lightning_layer.add_child(ln)
		_electric_bolts.append(ln)

	orb_layer = Control.new()
	orb_layer.set_anchors_preset(Control.PRESET_FULL_RECT)
	orb_layer.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(orb_layer)

	anim_player = AnimationPlayer.new()
	add_child(anim_player)
	var anim := Animation.new()
	anim.length = 0.75
	var tr := anim.add_track(Animation.TYPE_VALUE)
	anim.track_set_path(tr, NodePath(str(get_path_to(shockwave_rect)) + ":scale"))
	anim.track_insert_key(tr, 0.0, Vector2(0.55, 0.55))
	anim.track_insert_key(tr, 0.45, Vector2(1.18, 1.18))
	anim.track_insert_key(tr, 0.75, Vector2(1.30, 1.30))
	var lib := AnimationLibrary.new()
	lib.add_animation("burst_scale", anim)
	anim_player.add_animation_library("", lib)


func _process(delta: float) -> void:
	if _electric_timer > 0.0:
		_electric_timer -= delta
		for i in range(_electric_bolts.size()):
			var bolt := _electric_bolts[i]
			bolt.visible = true
			bolt.clear_points()
			var start := CAULDRON_CENTER + Vector2(randf_range(-55, 55), randf_range(-20, 20))
			var ang := randf() * TAU
			var end_pt := CAULDRON_CENTER + Vector2(cos(ang), sin(ang)) * randf_range(95.0, 205.0)
			bolt.add_point(start)
			for seg in range(1, 5):
				var t := float(seg) / 5.0
				var mid := start.lerp(end_pt, t) + Vector2(randf_range(-22, 22), randf_range(-22, 22))
				bolt.add_point(mid)
			bolt.add_point(end_pt)
		if _electric_timer <= 0.0:
			for b in _electric_bolts:
				b.visible = false


func _set_emitter_color(em: GPUParticles2D, col: Color) -> void:
	var pm := em.process_material as ParticleProcessMaterial
	if pm != null:
		pm.color = col
	em.restart()
	em.emitting = true


func play_reaction_choreography(slot_ids: Array, outcome: Dictionary, instant_mode: bool = false) -> void:
	if instant_mode:
		_trigger_particles_and_shaders(outcome)
		reaction_sequence_finished.emit(outcome)
		return

	is_playing = true
	var dur := GameState.get_reaction_duration()
	var flight_t := clampf(dur * 0.36, 0.14, 0.38)

	# Step 1: Animate slotted ingredient orbs flying along arcs into the cauldron!
	var active_orbs: Array[Control] = []
	for idx in range(min(slot_ids.size(), 3)):
		var sid := String(slot_ids[idx])
		if sid.is_empty():
			continue
		var ing := AlchemyDB.get_ingredient(sid)
		if ing == null:
			continue
		var orb := TextureRect.new()
		orb.texture = AssetVault.tex(ing.icon_path)
		orb.custom_minimum_size = Vector2(68, 68)
		orb.size = Vector2(68, 68)
		orb.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
		orb.pivot_offset = Vector2(34, 34)
		orb.position = PEDESTAL_POS[idx] - Vector2(34, 34)
		orb_layer.add_child(orb)
		active_orbs.append(orb)

		var start_p := orb.position
		var dest_p := CAULDRON_CENTER - Vector2(34, 34)
		var tw := create_tween()
		tw.tween_method(func(t: float) -> void:
			var linear := start_p.lerp(dest_p, t)
			linear.y -= sin(t * PI) * 95.0
			orb.position = linear
			orb.scale = Vector2.ONE * lerpf(1.0, 0.45, t)
		, 0.0, 1.0, flight_t).set_trans(Tween.TRANS_SINE)

	await get_tree().create_timer(flight_t).timeout
	for o in active_orbs:
		if is_instance_valid(o):
			o.queue_free()

	# Step 2: Trigger physical reaction shaders, GPUParticles2D, lightning, & SFX!
	_trigger_particles_and_shaders(outcome)

	var effect_wait := clampf(dur * 0.64, 0.22, 0.72)
	await get_tree().create_timer(effect_wait).timeout
	is_playing = false
	reaction_sequence_finished.emit(outcome)


func _trigger_particles_and_shaders(outcome: Dictionary) -> void:
	var rtype := String(outcome.get("reaction_type", "SMOKE"))
	var ok := bool(outcome.get("success", false))

	var c1 := Color("#8C829E")
	var c2 := Color("#C2B8D4")
	if ok and outcome.has("result_ingredient"):
		var res_ing: IngredientData = outcome["result_ingredient"]
		c1 = res_ing.primary_color
		c2 = res_ing.secondary_color

	SoundEngine.play_reaction(rtype)
	anim_player.stop()
	anim_player.play("burst_scale")

	# Shockwave ring shader tween
	shockwave_mat.set_shader_parameter("ring_color", c1)
	shockwave_mat.set_shader_parameter("secondary_color", c2)
	shockwave_mat.set_shader_parameter("thickness", 0.18 if rtype == "EXPLOSION" else 0.11)
	var stw := create_tween()
	shockwave_mat.set_shader_parameter("progress", 0.02)
	stw.tween_method(func(v: float) -> void: shockwave_mat.set_shader_parameter("progress", v), 0.02, 1.0, 0.68)

	# Cauldron liquid shader choreography
	if stage_ref != null and stage_ref.liquid_mat != null:
		var lm := stage_ref.liquid_mat
		lm.set_shader_parameter("color_a", c1)
		lm.set_shader_parameter("color_b", c2)
		lm.set_shader_parameter("color_c", c1.lightened(0.45))
		lm.set_shader_parameter("reaction_flash", 0.95 if ok else 0.35)
		var ltw := create_tween()
		ltw.tween_method(func(v: float) -> void: lm.set_shader_parameter("reaction_flash", v), 0.95 if ok else 0.35, 0.0, 0.55)

		match rtype:
			"FREEZING":
				lm.set_shader_parameter("freeze_amount", 1.0)
				create_tween().tween_method(func(v: float) -> void: lm.set_shader_parameter("freeze_amount", v), 1.0, 0.0, 1.4)
			"MELTING":
				lm.set_shader_parameter("heat_amount", 1.0)
				create_tween().tween_method(func(v: float) -> void: lm.set_shader_parameter("heat_amount", v), 1.0, 0.0, 1.4)
			"ELECTRICAL":
				lm.set_shader_parameter("electric_amount", 1.0)
				create_tween().tween_method(func(v: float) -> void: lm.set_shader_parameter("electric_amount", v), 1.0, 0.0, 1.1)
			"BUBBLING":
				lm.set_shader_parameter("boil_intensity", 1.85)
				create_tween().tween_method(func(v: float) -> void: lm.set_shader_parameter("boil_intensity", v), 1.85, 0.32, 1.3)
			"LIQUID_MIXING":
				lm.set_shader_parameter("swirl_speed", 7.2)
				create_tween().tween_method(func(v: float) -> void: lm.set_shader_parameter("swirl_speed", v), 7.2, 1.4, 1.3)

	if not ok:
		_set_emitter_color(emit_smoke, Color(0.55, 0.50, 0.64, 0.72))
		return

	# Trigger bespoke particle systems for each of the 10 required reaction types!
	match rtype:
		"BUBBLING":
			_set_emitter_color(emit_bubbles, Color(c1.r, c1.g, c1.b, 0.92))
			_set_emitter_color(emit_droplets, Color(c2.r, c2.g, c2.b, 0.85))
		"SMOKE":
			_set_emitter_color(emit_smoke, Color(c1.r, c1.g, c1.b, 0.82))
		"SPARKS":
			_set_emitter_color(emit_sparks, Color(c2.r, c2.g, c2.b, 1.0))
		"EXPLOSION":
			_set_emitter_color(emit_sparks, Color("#FFAE42"))
			_set_emitter_color(emit_smoke, Color(c1.r, c1.g, c1.b, 0.85))
			_shake_stage(12.0)
		"CRYSTALLIZATION":
			_set_emitter_color(emit_crystals, Color(c2.r, c2.g, c2.b, 0.96))
			_set_emitter_color(emit_runes, Color(c1.r, c1.g, c1.b, 0.90))
		"FREEZING":
			_set_emitter_color(emit_frost, Color("#DCF8FF"))
			_set_emitter_color(emit_smoke, Color(0.65, 0.92, 1.0, 0.55))
		"MELTING":
			_set_emitter_color(emit_droplets, Color("#FF7B1C"))
			_set_emitter_color(emit_sparks, Color("#FFE270"))
		"ELECTRICAL":
			_electric_timer = 0.65
			_set_emitter_color(emit_sparks, Color("#7AF8FF"))
		"LIQUID_MIXING":
			_set_emitter_color(emit_droplets, Color(c1.r, c1.g, c1.b, 0.92))
			_set_emitter_color(emit_bubbles, Color(c2.r, c2.g, c2.b, 0.88))
		"COLOR_TRANSITION":
			_set_emitter_color(emit_runes, Color(c2.r, c2.g, c2.b, 0.95))
			_set_emitter_color(emit_crystals, Color(c1.r, c1.g, c1.b, 0.88))

	if bool(outcome.get("is_new_ingredient", false)) or bool(outcome.get("is_rare", false)):
		SoundEngine.play_discovery(bool(outcome.get("is_rare", false)))


func _shake_stage(strength: float) -> void:
	if stage_ref == null or not bool(GameState.settings.get("screen_shake", true)):
		return
	var tw := create_tween()
	for i in range(6):
		var decay := 1.0 - float(i) / 6.0
		var off := Vector2(randf_range(-strength, strength), randf_range(-strength, strength)) * decay
		tw.tween_property(stage_ref, "position", off, 0.04)
	tw.tween_property(stage_ref, "position", Vector2.ZERO, 0.05)
