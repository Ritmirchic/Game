class_name LaboratoryStage
extends Control

signal hotspot_clicked(target: String)
signal detector_lantern_clicked()

var moonbeams: ColorRect
var storage_extra_layer: TextureRect
var gauge_needle: TextureRect
var shelf_bottles: Array[TextureRect] = []
var burner_flame: ColorRect
var sconce_left_flame: ColorRect
var sconce_right_flame: ColorRect
var stab_outer: TextureRect
var stab_inner: TextureRect
var cauldron_liquid: TextureRect
var liquid_mat: ShaderMaterial
var burner_mat: ShaderMaterial
var automaton_arm: TextureRect
var detector_lantern: TextureRect
var detector_glow: ColorRect
var pedestal_catalyst_sprite: TextureRect
var ambient_anim: AnimationPlayer
var condenser_steam: GPUParticles2D

var _time: float = 0.0


func _ready() -> void:
	set_anchors_preset(Control.PRESET_FULL_RECT)
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	_build_layers()
	_build_ambient_animation()
	refresh_equipment_visuals()
	GameState.upgrade_purchased.connect(func(_id: String, _lvl: int) -> void: refresh_equipment_visuals())
	GameState.state_reloaded.connect(refresh_equipment_visuals)


func _make_tex_rect(path: String, pos: Vector2, sz: Vector2 = Vector2.ZERO) -> TextureRect:
	var tr := TextureRect.new()
	tr.texture = AssetVault.tex(path)
	tr.position = pos
	if sz != Vector2.ZERO:
		tr.custom_minimum_size = sz
		tr.size = sz
		tr.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	elif tr.texture != null:
		tr.size = tr.texture.get_size()
	tr.mouse_filter = Control.MOUSE_FILTER_IGNORE
	return tr


func _build_layers() -> void:
	# 1. Vaulted Stone Wall & Gothic Arched Window
	add_child(_make_tex_rect("res://assets/lab/lab_bg_stone_arch.png", Vector2.ZERO, Vector2(1920, 1080)))

	# 2. Volumetric Moonbeams Shader from Arched Window
	moonbeams = ColorRect.new()
	moonbeams.position = Vector2(880, 96)
	moonbeams.size = Vector2(552, 620)
	moonbeams.mouse_filter = Control.MOUSE_FILTER_IGNORE
	var ray_mat := ShaderMaterial.new()
	ray_mat.shader = AssetVault.shader("res://shaders/ambient_light_rays.gdshader")
	moonbeams.material = ray_mat
	add_child(moonbeams)

	# 3. Back Furniture: Shelves, Cabinets, Pipes, Condenser, Research Board, Bookcase
	add_child(_make_tex_rect("res://assets/lab/lab_furniture_back.png", Vector2.ZERO, Vector2(1920, 1080)))

	# Extra Apothecary Rack (Storage Upgrade visual!)
	storage_extra_layer = _make_tex_rect("res://assets/lab/lab_storage_extra.png", Vector2.ZERO, Vector2(1920, 1080))
	add_child(storage_extra_layer)

	# Animated Shelf Bottles (6 bottles across shelves)
	var bottle_positions := [
		Vector2(518 - 36, 385 - 102),
		Vector2(635 - 36, 385 - 102),
		Vector2(750 - 36, 385 - 102),
		Vector2(502 - 36, 225 - 102),
		Vector2(660 - 36, 225 - 102),
		Vector2(772 - 36, 225 - 102),
	]
	for i in range(6):
		var sb := _make_tex_rect("res://assets/lab/prop_shelf_bottle_%d.png" % (i + 1), bottle_positions[i])
		sb.pivot_offset = Vector2(36, 95)
		add_child(sb)
		shelf_bottles.append(sb)

	# Animated Brass Pressure Manometer & Needle
	add_child(_make_tex_rect("res://assets/lab/prop_gauge_dial.png", Vector2(895 - 48, 210)))
	gauge_needle = _make_tex_rect("res://assets/lab/prop_gauge_needle.png", Vector2(895 - 48, 210))
	gauge_needle.pivot_offset = Vector2(48, 48)
	add_child(gauge_needle)

	# Subtle Ambient Condenser Vapor GPUParticles2D
	condenser_steam = GPUParticles2D.new()
	condenser_steam.position = Vector2(1355, 192)
	condenser_steam.amount = 14
	condenser_steam.lifetime = 2.8
	condenser_steam.texture = AssetVault.tex("res://assets/particles/part_smoke.png")
	var cmat := ParticleProcessMaterial.new()
	cmat.direction = Vector3(0, -1, 0)
	cmat.spread = 22.0
	cmat.initial_velocity_min = 12.0
	cmat.initial_velocity_max = 26.0
	cmat.gravity = Vector3(0, -8, 0)
	cmat.scale_min = 0.22
	cmat.scale_max = 0.55
	cmat.color = Color(0.68, 0.94, 1.0, 0.22)
	condenser_steam.process_material = cmat
	add_child(condenser_steam)

	# Wall Sconce Flickering Arcane Flames
	var sconce_mat := ShaderMaterial.new()
	sconce_mat.shader = AssetVault.shader("res://shaders/arcane_flame.gdshader")
	for scx in [825, 1465]:
		var fl := ColorRect.new()
		fl.position = Vector2(scx - 24, 214)
		fl.size = Vector2(48, 62)
		fl.mouse_filter = Control.MOUSE_FILTER_IGNORE
		fl.material = sconce_mat
		add_child(fl)

	# Desk Candlesticks on Left & Right
	add_child(_make_tex_rect("res://assets/lab/prop_candle_holder.png", Vector2(585, 608)))
	var cfl := ColorRect.new()
	cfl.position = Vector2(585 + 27, 608 + 8)
	cfl.size = Vector2(36, 44)
	cfl.mouse_filter = Control.MOUSE_FILTER_IGNORE
	cfl.material = sconce_mat
	add_child(cfl)

	# 4. Heavy Oak Foreground Desk + Transmutation Circle + Copper Conduits
	add_child(_make_tex_rect("res://assets/lab/lab_desk_foreground.png", Vector2.ZERO, Vector2(1920, 1080)))

	# 5. Under-Cauldron Burner & Live Shader Flame
	add_child(_make_tex_rect("res://assets/lab/prop_burner_base.png", Vector2(1156 - 120, 718)))
	burner_flame = ColorRect.new()
	burner_flame.position = Vector2(1156 - 78, 652)
	burner_flame.size = Vector2(156, 118)
	burner_flame.mouse_filter = Control.MOUSE_FILTER_IGNORE
	burner_mat = ShaderMaterial.new()
	burner_mat.shader = AssetVault.shader("res://shaders/arcane_flame.gdshader")
	burner_flame.material = burner_mat
	add_child(burner_flame)

	# 6. Gyroscopic Reaction Stabilizer Rings (spin smoothly around cauldron!)
	stab_outer = _make_tex_rect("res://assets/lab/prop_stabilizer_ring_outer.png", Vector2(1156 - 190, 622 - 190))
	stab_outer.pivot_offset = Vector2(190, 190)
	add_child(stab_outer)

	stab_inner = _make_tex_rect("res://assets/lab/prop_stabilizer_ring_inner.png", Vector2(1156 - 190, 622 - 190))
	stab_inner.pivot_offset = Vector2(190, 190)
	add_child(stab_inner)

	# 7. Central Alchemical Cauldron (Back Rim -> Shader Liquid Pool -> Cutaway Front Body)
	add_child(_make_tex_rect("res://assets/lab/prop_cauldron_back.png", Vector2(1156 - 190, 480)))

	cauldron_liquid = _make_tex_rect("res://assets/lab/prop_cauldron_liquid_base.png", Vector2(1156 - 130, 514))
	liquid_mat = ShaderMaterial.new()
	liquid_mat.shader = AssetVault.shader("res://shaders/cauldron_liquid.gdshader")
	cauldron_liquid.material = liquid_mat
	add_child(cauldron_liquid)

	add_child(_make_tex_rect("res://assets/lab/prop_cauldron_front.png", Vector2(1156 - 190, 480)))

	# 8. Brass Reagent Pedestal Sprites (Left, Right, and 3rd Catalyst Center-Bottom)
	add_child(_make_tex_rect("res://assets/lab/prop_pedestal.png", Vector2(830 - 78, 710)))
	add_child(_make_tex_rect("res://assets/lab/prop_pedestal.png", Vector2(1482 - 78, 710)))
	pedestal_catalyst_sprite = _make_tex_rect("res://assets/lab/prop_pedestal_gold.png", Vector2(1156 - 78, 845))
	add_child(pedestal_catalyst_sprite)

	# 9. Clockwork Auto-Experimenter Arm & Rare Resonance Detector Lantern
	automaton_arm = _make_tex_rect("res://assets/lab/prop_automaton_arm.png", Vector2(902, 434))
	automaton_arm.pivot_offset = Vector2(54, 166)
	add_child(automaton_arm)

	detector_glow = ColorRect.new()
	detector_glow.position = Vector2(1605 - 26, 635 + 34)
	detector_glow.size = Vector2(192, 160)
	detector_glow.mouse_filter = Control.MOUSE_FILTER_IGNORE
	var dmat := ShaderMaterial.new()
	dmat.shader = AssetVault.shader("res://shaders/reaction_burst.gdshader")
	dmat.set_shader_parameter("progress", 0.45)
	dmat.set_shader_parameter("thickness", 0.25)
	detector_glow.material = dmat
	detector_glow.visible = false
	add_child(detector_glow)

	detector_lantern = _make_tex_rect("res://assets/lab/prop_detector_lantern.png", Vector2(1605, 635))
	add_child(detector_lantern)

	# 10. Interactive Laboratory Prop Hotspots (Research Board, Bookcase, Detector Lantern)
	_add_hotspot(Rect2(1470, 132, 398, 324), "challenges", "Research & Commissions Board - Click to inspect Challenges")
	_add_hotspot(Rect2(1485, 482, 386, 252), "journal", "Alchemical Library & Grimoires - Click to open Research Journal")
	var det_btn := Button.new()
	det_btn.flat = true
	det_btn.position = Vector2(1605, 635)
	det_btn.size = Vector2(140, 200)
	det_btn.tooltip_text = "Rare Resonance Detector Lantern - Click to scan for undiscovered pairs"
	det_btn.mouse_default_cursor_shape = Control.CURSOR_POINTING_HAND
	det_btn.focus_mode = Control.FOCUS_NONE
	det_btn.pressed.connect(func() -> void: detector_lantern_clicked.emit())
	add_child(det_btn)


func _add_hotspot(rect: Rect2, target: String, tip: String) -> void:
	var b := Button.new()
	b.flat = true
	b.position = rect.position
	b.size = rect.size
	b.tooltip_text = tip
	b.focus_mode = Control.FOCUS_NONE
	b.mouse_default_cursor_shape = Control.CURSOR_POINTING_HAND
	b.pressed.connect(func() -> void: hotspot_clicked.emit(target))
	add_child(b)


func _build_ambient_animation() -> void:
	ambient_anim = AnimationPlayer.new()
	add_child(ambient_anim)
	var anim := Animation.new()
	anim.length = 4.0
	anim.loop_mode = Animation.LOOP_LINEAR

	var tr_idx := anim.add_track(Animation.TYPE_VALUE)
	anim.track_set_path(tr_idx, NodePath(str(get_path_to(gauge_needle)) + ":rotation_degrees"))
	anim.track_insert_key(tr_idx, 0.0, -28.0)
	anim.track_insert_key(tr_idx, 1.8, 34.0)
	anim.track_insert_key(tr_idx, 3.1, 8.0)
	anim.track_insert_key(tr_idx, 4.0, -28.0)

	var lib := AnimationLibrary.new()
	lib.add_animation("ambient_loop", anim)
	ambient_anim.add_animation_library("", lib)
	ambient_anim.play("ambient_loop")


func _process(delta: float) -> void:
	_time += delta
	# Gentle shimmering bob on shelf reagent flasks
	for i in range(shelf_bottles.size()):
		shelf_bottles[i].rotation_degrees = sin(_time * 1.4 + float(i) * 1.1) * 1.8

	var stab_lvl := GameState.get_upgrade_level("stabilizer")
	if stab_lvl >= 1:
		stab_outer.rotation += delta * (0.45 + stab_lvl * 0.22)
	if stab_lvl >= 2:
		stab_inner.rotation -= delta * (0.75 + stab_lvl * 0.25)

	if detector_glow.visible:
		var pulse := 0.35 + 0.25 * (0.5 + 0.5 * sin(_time * 3.8))
		(detector_glow.material as ShaderMaterial).set_shader_parameter("progress", pulse)


func refresh_equipment_visuals() -> void:
	# Storage rack visibility
	var st_lvl := GameState.get_upgrade_level("storage")
	storage_extra_layer.visible = st_lvl >= 1
	storage_extra_layer.modulate = Color(1, 1, 1, 0.75 + st_lvl * 0.08)

	# Burner flame color & intensity by upgrade tier!
	var b_lvl := GameState.get_upgrade_level("burner")
	match b_lvl:
		0:
			burner_mat.set_shader_parameter("outer_color", Color("#FF5912"))
			burner_mat.set_shader_parameter("inner_color", Color("#FFE066"))
			burner_mat.set_shader_parameter("intensity", 0.90)
		1:
			burner_mat.set_shader_parameter("outer_color", Color("#1C88FF"))
			burner_mat.set_shader_parameter("inner_color", Color("#9EF4FF"))
			burner_mat.set_shader_parameter("intensity", 1.10)
		2:
			burner_mat.set_shader_parameter("outer_color", Color("#5B3BFF"))
			burner_mat.set_shader_parameter("inner_color", Color("#7AF8FF"))
			burner_mat.set_shader_parameter("intensity", 1.25)
		3:
			burner_mat.set_shader_parameter("outer_color", Color("#DF3BFF"))
			burner_mat.set_shader_parameter("inner_color", Color("#FFFFFF"))
			burner_mat.set_shader_parameter("intensity", 1.42)

	# Stabilizer gyroscopic rings
	var stab_lvl := GameState.get_upgrade_level("stabilizer")
	stab_outer.visible = stab_lvl >= 1
	stab_inner.visible = stab_lvl >= 2

	# 3rd Catalyst Pedestal highlight
	var mix_lvl := GameState.get_upgrade_level("mixer")
	pedestal_catalyst_sprite.modulate = Color(1, 1, 1, 1.0) if mix_lvl >= 1 else Color(0.55, 0.5, 0.6, 0.55)

	# Clockwork Automaton Arm
	automaton_arm.visible = GameState.get_upgrade_level("automaton") >= 1

	# Rare Resonance Detector Lantern
	var det_lvl := GameState.get_upgrade_level("detector")
	detector_lantern.modulate = Color(1, 1, 1, 1.0) if det_lvl >= 1 else Color(0.55, 0.52, 0.62, 0.72)


func set_detector_resonance_state(state: String) -> void:
	if state == "NONE" or GameState.get_upgrade_level("detector") <= 0:
		detector_glow.visible = false
		return
	detector_glow.visible = true
	var dmat := detector_glow.material as ShaderMaterial
	if state == "RARE":
		dmat.set_shader_parameter("ring_color", Color("#D85CFF"))
		dmat.set_shader_parameter("secondary_color", Color("#FFD54F"))
	else:
		dmat.set_shader_parameter("ring_color", Color("#48F0B8"))
		dmat.set_shader_parameter("secondary_color", Color("#64D8FF"))


func update_cauldron_idle_colors(slot_ids: Array) -> void:
	var cols: Array[Color] = []
	for sid in slot_ids:
		var ing := AlchemyDB.get_ingredient(String(sid))
		if ing != null:
			cols.append(ing.primary_color)
	if cols.is_empty():
		liquid_mat.set_shader_parameter("color_a", Color("#22A898"))
		liquid_mat.set_shader_parameter("color_b", Color("#4F359B"))
		liquid_mat.set_shader_parameter("color_c", Color("#D8A838"))
		liquid_mat.set_shader_parameter("swirl_speed", 1.15)
		liquid_mat.set_shader_parameter("boil_intensity", 0.22)
	elif cols.size() == 1:
		liquid_mat.set_shader_parameter("color_a", cols[0])
		liquid_mat.set_shader_parameter("color_b", cols[0].darkened(0.32))
		liquid_mat.set_shader_parameter("color_c", cols[0].lightened(0.35))
		liquid_mat.set_shader_parameter("swirl_speed", 1.85)
		liquid_mat.set_shader_parameter("boil_intensity", 0.38)
	else:
		liquid_mat.set_shader_parameter("color_a", cols[0])
		liquid_mat.set_shader_parameter("color_b", cols[1])
		liquid_mat.set_shader_parameter("color_c", cols[2] if cols.size() >= 3 else cols[0].lerp(cols[1], 0.5).lightened(0.25))
		liquid_mat.set_shader_parameter("swirl_speed", 2.65)
		liquid_mat.set_shader_parameter("boil_intensity", 0.55)


func play_automaton_dip() -> void:
	if not automaton_arm.visible:
		return
	var tw := create_tween()
	tw.tween_property(automaton_arm, "rotation_degrees", 16.0, 0.22).set_trans(Tween.TRANS_SINE)
	tw.tween_property(automaton_arm, "rotation_degrees", 0.0, 0.30).set_trans(Tween.TRANS_BACK)
