extends Node3D
## Builds the complete supermarket complex + exterior from level_layout.json.
class_name LevelBuilder

const BarricadeScript := preload("res://scripts/barricade.gd")
const LootScript := preload("res://scripts/loot.gd")

var A: AssetLib
var F: ModelFactory
var data: GameData

var spawn_points: Dictionary = {} # id -> Node3D
var player_spawn: Node3D
var barricades: Dictionary = {} # door id -> Barricade
var lights: Array[OmniLight3D] = []
var rain: GPUParticles3D
var env: WorldEnvironment
var sun: DirectionalLight3D
var interactables: Node3D
var nav: GridNav

var _wall_meshes: Dictionary = {}
var _colliders: Dictionary = {}

func _init(game: GameData, assets: AssetLib, factory: ModelFactory) -> void:
	data = game
	A = assets
	F = factory
	name = "Level"

func build() -> void:
	nav = GridNav.new(data.LAYOUT, data.CELL)
	_build_ground()
	_build_walls()
	_build_furniture()
	_build_doors_and_barricades()
	_build_loot()
	_build_lights()
	_build_exterior()
	_build_weather()
	_build_markers()

# ------------------------------------------------------------------- ground
func _build_ground() -> void:
	var b: Dictionary = data.LAYOUT["interior"]
	var w: float = b["x1"] * data.CELL
	var d: float = b["z1"] * data.CELL
	# base sales floor
	_floor_rect(Rect2(0, 0, w, d), A.floor_tile(), Vector3(w / 2, 0, d / 2))
	for room in data.LAYOUT["rooms"]:
		var r: Array = room["rect"]
		var size := Vector2((r[2] - r[0] + 1) * data.CELL, (r[3] - r[1] + 1) * data.CELL)
		var pos := Vector3((r[0] + r[2] + 1) * 0.5 * data.CELL, 0.01, (r[1] + r[3] + 1) * 0.5 * data.CELL)
		var mat: Material = A.floor_concrete() if room["floor"] == "concrete" else A.floor_tile()
		_floor_rect(Rect2(0, 0, size.x, size.y), mat, pos)
	# ceiling slab
	var ceil_mesh := BoxMesh.new()
	ceil_mesh.size = Vector3(w, 0.2, d)
	ceil_mesh.material = A.ceiling_mat()
	var ci := MeshInstance3D.new()
	ci.mesh = ceil_mesh
	ci.position = Vector3(w / 2, data.WALL_H + 0.1, d / 2)
	add_child(ci)
	# exterior asphalt
	var e: Dictionary = data.LAYOUT["exterior"]
	var ew: float = (e["x1"] - e["x0"]) * data.CELL
	var ed: float = (e["z1"] - e["z0"]) * data.CELL
	var ground := PlaneMesh.new()
	ground.size = Vector2(ew, ed)
	ground.material = A.asphalt()
	var gi := MeshInstance3D.new()
	gi.mesh = ground
	var gpos := Vector3((e["x0"] + e["x1"]) * 0.5 * data.CELL, -0.05, (e["z0"] + e["z1"]) * 0.5 * data.CELL)
	gi.position = gpos
	add_child(gi)
	# exterior ground collider
	var gbody := StaticBody3D.new()
	gbody.collision_layer = GameData.LAYER_WORLD
	gbody.collision_mask = 0
	var gcs := CollisionShape3D.new()
	var gshape := BoxShape3D.new()
	gshape.size = Vector3(ew, 0.2, ed)
	gcs.shape = gshape
	gcs.position = Vector3(gpos.x, -0.15, gpos.z)
	gbody.add_child(gcs)
	add_child(gbody)
	# sidewalk curb around building
	var curb := A.floor_concrete()
	for spec in [
		[Vector3(w / 2, 0.05, -data.CELL * 0.5), Vector3(w + 1.5, 0.12, 1.2)],
		[Vector3(w / 2, 0.05, d + data.CELL * 0.5), Vector3(w + 1.5, 0.12, 1.2)],
		[Vector3(-data.CELL * 0.5, 0.05, d / 2), Vector3(1.2, 0.12, d + 1.5)],
		[Vector3(w + data.CELL * 0.5, 0.05, d / 2), Vector3(1.2, 0.12, d + 1.5)],
	]:
		var mi := MeshInstance3D.new()
		var bm := BoxMesh.new()
		bm.size = spec[1]
		bm.material = curb
		mi.mesh = bm
		mi.position = spec[0]
		add_child(mi)

func _floor_rect(rect: Rect2, mat: Material, pos: Vector3) -> void:
	var pm := PlaneMesh.new()
	pm.size = rect.size
	pm.material = mat
	var mi := MeshInstance3D.new()
	mi.mesh = pm
	mi.position = pos
	add_child(mi)
	# solid collider under every floor surface (top face at y = 0)
	var body := StaticBody3D.new()
	body.collision_layer = GameData.LAYER_WORLD
	body.collision_mask = 0
	var cs := CollisionShape3D.new()
	var shape := BoxShape3D.new()
	shape.size = Vector3(rect.size.x, 0.2, rect.size.y)
	cs.shape = shape
	cs.position = Vector3(pos.x, -0.1, pos.z)
	body.add_child(cs)
	add_child(body)

# -------------------------------------------------------------------- walls
func _wall_mesh(style: String) -> BoxMesh:
	if _wall_meshes.has(style):
		return _wall_meshes[style]
	var bm := BoxMesh.new()
	bm.size = Vector3(data.CELL, data.WALL_H, data.CELL)
	bm.material = A.wall_brick() if style == "brick" else A.wall_paint()
	_wall_meshes[style] = bm
	return bm

func _build_walls() -> void:
	for w in data.LAYOUT["walls"]:
		var body := StaticBody3D.new()
		body.name = "Wall_" + w["id"]
		body.collision_layer = GameData.LAYER_WORLD
		body.collision_mask = 0
		add_child(body)
		var shape := BoxShape3D.new()
		shape.size = Vector3(data.CELL, data.WALL_H, data.CELL)
		var gap_set := {}
		for g in w.get("gaps", []):
			gap_set[int(g)] = true
		for i in range(int(w["from"]), int(w["to"]) + 1):
			if gap_set.has(i):
				continue
			var c := Vector2i(i, int(w["fixed"])) if w["axis"] == "h" else Vector2i(int(w["fixed"]), i)
			var pos: Vector3 = data.cell_to_world(c.x, c.y, data.WALL_H * 0.5)
			var mi := MeshInstance3D.new()
			mi.mesh = _wall_mesh(w["style"])
			mi.position = pos
			body.add_child(mi)
			var cs := CollisionShape3D.new()
			cs.shape = shape
			cs.position = pos
			body.add_child(cs)

# --------------------------------------------------------------- furniture
func _build_furniture() -> void:
	interactables = Node3D.new()
	interactables.name = "Interactables"
	add_child(interactables)
	var bodies := Node3D.new()
	bodies.name = "FurnitureBodies"
	add_child(bodies)
	for f in data.LAYOUT["furniture"]:
		var c: Array = f["at"]
		var pos: Vector3 = data.cell_to_world(int(c[0]), int(c[1]))
		var kind: String = f["type"]
		var model: Node3D
		match kind:
			"shelf": model = F.make_shelf()
			"cart": model = F.make_cart()
			"checkout": model = F.make_checkout()
			"fridge": model = F.make_fridge()
			"box": model = F.make_box()
			"pallet": model = F.make_pallet()
			"desk": model = F.make_desk()
			"console": model = F.make_console()
			"locker": model = F.make_locker()
			"panel": model = F.make_panel()
			"terminal":
				model = F.make_terminal()
				model.add_to_group("shop_terminal")
			"stairs":
				model = F.make_stairs()
			_:
				continue
		model.position = pos
		add_child(model)
		if kind != "stairs":
			_add_furniture_collision(bodies, kind, pos)

func _add_furniture_collision(parent: Node3D, kind: String, pos: Vector3) -> void:
	var sizes := {
		"shelf": Vector3(1.8, 1.95, 0.6), "cart": Vector3(0.8, 1.1, 0.7),
		"checkout": Vector3(1.75, 1.0, 0.8), "fridge": Vector3(1.85, 2.15, 0.8),
		"box": Vector3(0.7, 0.7, 0.7), "pallet": Vector3(1.2, 0.2, 1.0),
		"desk": Vector3(1.45, 0.85, 0.75), "console": Vector3(1.65, 0.9, 0.65),
		"locker": Vector3(0.65, 1.9, 0.55), "panel": Vector3(1.25, 1.85, 0.35),
		"terminal": Vector3(0.55, 1.2, 0.45),
	}
	if not sizes.has(kind):
		return
	var body := StaticBody3D.new()
	body.collision_layer = GameData.LAYER_WORLD
	body.collision_mask = 0
	body.position = pos
	var cs := CollisionShape3D.new()
	var shape := BoxShape3D.new()
	shape.size = sizes[kind]
	cs.shape = shape
	cs.position = Vector3(0, shape.size.y * 0.5, 0)
	body.add_child(cs)
	parent.add_child(body)

# ----------------------------------------------------- doors + barricades
func _build_doors_and_barricades() -> void:
	for d in data.LAYOUT["doors"]:
		var mid: Array = d["cells"][d["cells"].size() / 2]
		var center: Vector3 = data.cell_to_world(int(mid[0]), int(mid[1]))
		var first_p: Vector3 = data.cell_to_world(int(d["cells"][0][0]), int(d["cells"][0][1]))
		var last_p: Vector3 = data.cell_to_world(int(d["cells"][-1][0]), int(d["cells"][-1][1]))
		var horizontal_wall: bool = abs(first_p.z - last_p.z) < 0.01
		# lintel + frame posts
		var frame := Node3D.new()
		frame.position = center
		add_child(frame)
		var span: float = d["cells"].size() * data.CELL
		var lintel := MeshInstance3D.new()
		var lm := BoxMesh.new()
		lm.size = Vector3(span, 0.35, 0.3) if horizontal_wall else Vector3(0.3, 0.35, span)
		lm.material = A.dark_metal()
		lintel.mesh = lm
		lintel.position.y = data.WALL_H - 0.25
		frame.add_child(lintel)
		for sgn in [-1, 1]:
			var post := MeshInstance3D.new()
			var pm := BoxMesh.new()
			pm.size = Vector3(0.22, data.WALL_H, 0.22)
			pm.material = A.dark_metal()
			post.mesh = pm
			var off: float = sgn * span * 0.5
			post.position = Vector3(off, data.WALL_H * 0.5, 0) if horizontal_wall else Vector3(0, data.WALL_H * 0.5, off)
			frame.add_child(post)
		# sign above door
		var sign := MeshInstance3D.new()
		var qm := QuadMesh.new()
		qm.size = Vector2(span * 0.9, 0.45)
		var tex_name := "sign_exit" if d["exterior"] else "sign_staff"
		qm.material = A.material("sign_" + d["id"], tex_name, 0.6)
		sign.mesh = qm
		sign.position = Vector3(0, data.WALL_H - 0.75, 0)
		if not horizontal_wall:
			sign.rotation_degrees.y = 90
		frame.add_child(sign)
		# barricade
		var b: Node3D = BarricadeScript.new(data, A, F, d)
		b.position = center
		add_child(b)
		barricades[d["id"]] = b
		nav.set_door_closed(d["cells"], false)

# ------------------------------------------------------------------- loot
func _build_loot() -> void:
	for i in data.LAYOUT["loot"].size():
		var spot: Dictionary = data.LAYOUT["loot"][i]
		var pos: Vector3 = data.cell_to_world(int(spot["at"][0]), int(spot["at"][1]))
		var crate: Node3D = LootScript.new(data, A, F, i, int(spot["tier"]), pos)
		interactables.add_child(crate)

# ------------------------------------------------------------------ lights
func _build_lights() -> void:
	for L in data.LAYOUT["lights"]:
		var pos: Vector3 = data.cell_to_world(int(L["at"][0]), int(L["at"][1]), float(L["y"]))
		var lamp := F.make_ceiling_light()
		lamp.position = pos
		add_child(lamp)
		var light := OmniLight3D.new()
		light.position = pos + Vector3(0, -0.15, 0)
		light.light_color = Color(1.0, 0.92, 0.78)
		light.light_energy = 2.2
		light.omni_range = 8.5
		light.shadow_enabled = false
		add_child(light)
		lights.append(light)
	# moonlight
	sun = DirectionalLight3D.new()
	sun.rotation_degrees = Vector3(-55, -35, 0)
	sun.light_color = Color(0.55, 0.65, 0.85)
	sun.light_energy = 0.35
	sun.shadow_enabled = true
	sun.directional_shadow_max_distance = 90
	add_child(sun)

# --------------------------------------------------------------- exterior
func _build_exterior() -> void:
	var e: Dictionary = data.LAYOUT["exterior"]
	# fence along outer perimeter with entrance gaps
	var fence_pts: Array = []
	for x in range(int(e["x0"]), int(e["x1"])):
		fence_pts.append(Vector2i(x, int(e["z0"])))
		fence_pts.append(Vector2i(x, int(e["z1"]) - 1))
	for z in range(int(e["z0"]) + 1, int(e["z1"]) - 1):
		fence_pts.append(Vector2i(int(e["x0"]), z))
		fence_pts.append(Vector2i(int(e["x1"]) - 1, z))
	var gap_set := {}
	for g in e["fence_gaps"]:
		for x in g:
			gap_set[Vector2i(int(x), int(e["z0"]))] = true
	var body := StaticBody3D.new()
	body.collision_layer = GameData.LAYER_WORLD
	body.collision_mask = 0
	add_child(body)
	var fshape := BoxShape3D.new()
	fshape.size = Vector3(2.0, 1.8, 0.1)
	for c in fence_pts:
		if gap_set.has(c):
			continue
		var seg := F.make_fence_segment()
		seg.position = data.cell_to_world(c.x, c.y)
		if c.x == int(e["x0"]) or c.x == int(e["x1"]) - 1:
			seg.rotation_degrees.y = 90
		add_child(seg)
		var cs := CollisionShape3D.new()
		cs.shape = fshape
		cs.position = seg.position + Vector3(0, 0.9, 0)
		if c.x == int(e["x0"]) or c.x == int(e["x1"]) - 1:
			cs.rotation_degrees.y = 90
		body.add_child(cs)
	# vehicles
	for car in e["cars"]:
		var v := F.make_car(car["color"])
		v.position = data.cell_to_world(int(car["at"][0]), int(car["at"][1]))
		v.rotation_degrees.y = 90 if (int(car["at"][0]) % 2 == 0) else -90
		add_child(v)
		_add_simple_body(v.global_position, Vector3(2.0, 1.5, 4.1), v.rotation_degrees.y)
	var truck := F.make_truck()
	truck.position = data.cell_to_world(int(e["truck"]["at"][0]), int(e["truck"]["at"][1]))
	truck.rotation_degrees.y = 180
	add_child(truck)
	_add_simple_body(truck.global_position, Vector3(2.5, 2.6, 6.5), 180)
	# streetlights
	for sl in e["streetlights"]:
		var node := F.make_streetlight()
		var holder := Node3D.new()
		holder.position = data.cell_to_world(int(sl[0]), int(sl[1]))
		holder.add_child(node)
		add_child(holder)
		var spot := SpotLight3D.new()
		spot.position = node.position
		spot.light_energy = 3.0
		spot.spot_range = 14
		spot.spot_angle = 55
		spot.light_color = Color(0.9, 0.85, 0.7)
		spot.shadow_enabled = false
		holder.add_child(spot)
		spot.look_at(node.position + Vector3(0, -1, 0.3), Vector3.FORWARD)

func _add_simple_body(pos: Vector3, size: Vector3, rot_y: float) -> void:
	var body := StaticBody3D.new()
	body.collision_layer = GameData.LAYER_WORLD
	body.collision_mask = 0
	body.position = pos
	body.rotation_degrees.y = rot_y
	var cs := CollisionShape3D.new()
	var shape := BoxShape3D.new()
	shape.size = size
	cs.shape = shape
	cs.position.y = size.y * 0.5
	body.add_child(cs)
	add_child(body)

# ---------------------------------------------------------------- weather
func _build_weather() -> void:
	env = WorldEnvironment.new()
	var environment := Environment.new()
	var sky := Sky.new()
	var sky_mat := ProceduralSkyMaterial.new()
	sky_mat.sky_top_color = Color(0.02, 0.03, 0.05)
	sky_mat.sky_horizon_color = Color(0.08, 0.09, 0.12)
	sky_mat.ground_bottom_color = Color(0.03, 0.03, 0.04)
	sky_mat.ground_horizon_color = Color(0.1, 0.1, 0.12)
	sky.sky_material = sky_mat
	environment.background_mode = Environment.BG_SKY
	environment.sky = sky
	environment.ambient_light_source = Environment.AMBIENT_SOURCE_SKY
	environment.ambient_light_energy = 0.35
	environment.fog_enabled = true
	environment.fog_light_color = Color(0.25, 0.27, 0.32)
	environment.fog_density = 0.008
	environment.tonemap_mode = Environment.TONE_MAPPER_FILMIC
	environment.glow_enabled = true
	environment.glow_intensity = 0.35
	environment.glow_bloom = 0.1
	env.environment = environment
	add_child(env)
	# rain particles
	rain = GPUParticles3D.new()
	rain.amount = 1400
	rain.lifetime = 1.1
	var mat := ParticleProcessMaterial.new()
	mat.gravity = Vector3(0, -18, 0)
	mat.direction = Vector3(0, -1, 0.08)
	mat.spread = 4
	mat.initial_velocity_min = 14
	mat.initial_velocity_max = 20
	mat.emission_shape = ParticleProcessMaterial.EMISSION_SHAPE_BOX
	mat.emission_box_extents = Vector3(35, 10, 35)
	var box := BoxMesh.new()
	box.size = Vector3(0.015, 0.5, 0.015)
	box.material = A.plain("rain_drop", Color(0.6, 0.7, 0.85, 0.5), 0.2, 0.0, Color.BLACK, 1.0, true)
	rain.process_material = mat
	rain.draw_pass_1 = box
	rain.position = data.cell_to_world(15, 10, 12)
	rain.visible = false
	add_child(rain)

func _build_markers() -> void:
	for s in data.LAYOUT["spawns"]:
		var m := Node3D.new()
		m.name = "Spawn_" + s["id"]
		m.position = data.cell_to_world(int(s["at"][0]), int(s["at"][1]))
		add_child(m)
		spawn_points[s["id"]] = m
	player_spawn = Node3D.new()
	player_spawn.name = "PlayerSpawn"
	player_spawn.position = data.cell_to_world(14, 1)
	player_spawn.rotation_degrees.y = 180
	add_child(player_spawn)
	# roof deck above storage B with safety railing
	var deck := MeshInstance3D.new()
	var dm := BoxMesh.new()
	dm.size = Vector3(10, 0.3, 10)
	dm.material = A.floor_concrete()
	deck.mesh = dm
	deck.position = data.cell_to_world(27, 16, data.WALL_H)
	add_child(deck)
	for i in 6:
		var rail := MeshInstance3D.new()
		var rm := BoxMesh.new()
		rm.size = Vector3(10 if i < 3 else 0.1, 1.0, 0.1 if i < 3 else 10)
		rm.material = A.dark_metal()
		rail.mesh = rm
		var off := -4.5 + i * 1.8
		rail.position = deck.position + Vector3(off if i < 3 else (-4.5 if i == 4 else 4.5),
			0.6, -4.5 if i < 3 else (off if i >= 3 else 0))
		add_child(rail)

func set_night_atmosphere(night: Dictionary, active: bool) -> void:
	var e: Environment = env.environment
	if active:
		e.fog_density = float(night.get("fog", 0.012))
		var rain_strength: float = float(night.get("rain", 0.2))
		rain.visible = rain_strength > 0.35
		rain.amount = int(600 + rain_strength * 1600)
		sun.light_energy = 0.18
		for L in lights:
			L.light_energy = 2.6
	else:
		e.fog_density = 0.004
		rain.visible = false
		sun.light_energy = 0.5
		for L in lights:
			L.light_energy = 2.0
