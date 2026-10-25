extends RefCounted
## Procedural model factory: every gameplay object is assembled from shaped
## primitives with PBR materials (no placeholder cubes-as-characters).
## Humanoid zombies are articulated rigs of pivoted limb segments animated in code.
class_name ModelFactory

var A: AssetLib

func _init(asset_lib: AssetLib) -> void:
	A = asset_lib

# ---------------------------------------------------------------- primitives
static func box(parent: Node3D, size: Vector3, pos: Vector3, mat: Material,
		rot: Vector3 = Vector3.ZERO) -> MeshInstance3D:
	var me := BoxMesh.new()
	me.size = size
	me.material = mat
	var mi := MeshInstance3D.new()
	mi.mesh = me
	mi.position = pos
	mi.rotation_degrees = rot
	parent.add_child(mi)
	return mi

static func sphere(parent: Node3D, radius: float, pos: Vector3, mat: Material,
		scale: Vector3 = Vector3.ONE) -> MeshInstance3D:
	var me := SphereMesh.new()
	me.radius = radius
	me.height = radius * 2.0
	me.material = mat
	var mi := MeshInstance3D.new()
	mi.mesh = me
	mi.position = pos
	mi.scale = scale
	parent.add_child(mi)
	return mi

static func cyl(parent: Node3D, radius_top: float, radius_bottom: float, height: float,
		pos: Vector3, mat: Material, sides: int = 12, rot: Vector3 = Vector3.ZERO) -> MeshInstance3D:
	var me := CylinderMesh.new()
	me.top_radius = radius_top
	me.bottom_radius = radius_bottom
	me.height = height
	me.radial_segments = sides
	me.material = mat
	var mi := MeshInstance3D.new()
	mi.mesh = me
	mi.position = pos
	mi.rotation_degrees = rot
	parent.add_child(mi)
	return mi

static func capsule(parent: Node3D, radius: float, height: float, pos: Vector3, mat: Material) -> MeshInstance3D:
	var me := CapsuleMesh.new()
	me.radius = radius
	me.height = height
	me.material = mat
	var mi := MeshInstance3D.new()
	mi.mesh = me
	mi.position = pos
	parent.add_child(mi)
	return mi

# ----------------------------------------------------------------- zombies
func make_zombie(type_id: String) -> Dictionary:
	var d: Dictionary = GameData.instance().ZOMBIES[type_id]
	var skin := A.plain("skin_" + type_id, Color(d["skin"][0], d["skin"][1], d["skin"][2]), 0.85)
	var cloth := A.plain("cloth_" + type_id, Color(d["cloth"][0], d["cloth"][1], d["cloth"][2]), 0.95)
	var pants := A.plain("pants_" + type_id, Color(d["cloth"][0] * 0.6, d["cloth"][1] * 0.6, d["cloth"][2] * 0.6), 0.95)
	var eye := A.plain("eye_" + type_id, Color(0.05, 0.02, 0.01), 0.4, 0.0,
		Color(1.0, 0.25 if type_id != "screamer" else 0.9, 0.1), 2.2)
	var root := Node3D.new()
	root.name = "ZombieRig"
	var crouched: bool = d.get("crouched", false)
	var s: float = d["scale"]

	var hips := Node3D.new()
	hips.name = "Hips"
	hips.position = Vector3(0, 0.95 if not crouched else 0.55, 0)
	root.add_child(hips)
	# pelvis + torso (tapered)
	box(hips, Vector3(0.34, 0.22, 0.24), Vector3(0, 0.02, 0), pants)
	box(hips, Vector3(0.42, 0.52, 0.26), Vector3(0, 0.34, 0), cloth)
	box(hips, Vector3(0.46, 0.18, 0.27), Vector3(0, 0.62, 0), cloth) # shoulders
	# spine tatter details
	box(hips, Vector3(0.3, 0.16, 0.28), Vector3(0.08, 0.2, -0.02), pants, Vector3(0, 0, 12))
	# head
	var head := Node3D.new()
	head.name = "Head"
	head.position = Vector3(0, 0.82, 0)
	hips.add_child(head)
	sphere(head, 0.145, Vector3.ZERO, skin, Vector3(0.92, 1.08, 0.95))
	# jaw (elongated for screamer)
	var jaw_h := 0.12 if type_id == "screamer" else 0.07
	box(head, Vector3(0.14, jaw_h, 0.12), Vector3(0, -0.12, 0.05), skin)
	sphere(head, 0.026, Vector3(-0.055, 0.03, 0.125), eye)
	sphere(head, 0.026, Vector3(0.055, 0.03, 0.125), eye)
	# nose
	box(head, Vector3(0.04, 0.05, 0.05), Vector3(0, -0.02, 0.14), skin)
	# arms: shoulder pivot -> upper -> elbow pivot -> lower -> hand
	var arm_l := _build_arm(hips, Vector3(-0.27, 0.6, 0), skin, cloth)
	var arm_r := _build_arm(hips, Vector3(0.27, 0.6, 0), skin, cloth)
	# legs: hip pivot -> thigh -> knee -> shin -> foot
	var leg_l := _build_leg(hips, Vector3(-0.12, -0.05, 0), pants, skin)
	var leg_r := _build_leg(hips, Vector3(0.12, -0.05, 0), pants, skin)
	# type identity
	if d.get("armored", false):
		var plate := A.dark_metal()
		box(hips, Vector3(0.46, 0.3, 0.06), Vector3(0, 0.4, 0.15), plate)
		box(hips, Vector3(0.16, 0.12, 0.14), Vector3(-0.27, 0.62, 0), plate)
		box(hips, Vector3(0.16, 0.12, 0.14), Vector3(0.27, 0.62, 0), plate)
		box(hips, Vector3(0.3, 0.1, 0.08), Vector3(0, 0.0, -0.14), plate)
	if type_id == "screamer":
		# stretched neck + hollow cheeks
		cyl(head, 0.06, 0.07, 0.18, Vector3(0, -0.12, -0.02), skin, 8)
		box(head, Vector3(0.05, 0.07, 0.04), Vector3(-0.09, -0.04, 0.1), skin)
		box(head, Vector3(0.05, 0.07, 0.04), Vector3(0.09, -0.04, 0.1), skin)
	if d.get("boss", "") != "":
		var spike_mat := A.plain("boss_spike", Color(0.12, 0.1, 0.09), 0.5, 0.6)
		for sx in [-0.18, 0.0, 0.18]:
			cyl(hips, 0.0, 0.045, 0.22, Vector3(sx, 0.78, 0.0), spike_mat, 6, Vector3(0, 0, 180))
		sphere(head, 0.035, Vector3(-0.06, 0.03, 0.13), eye)
		sphere(head, 0.035, Vector3(0.06, 0.03, 0.13), eye)
	if crouched:
		root.rotation_degrees.x = 62
		root.position.y = 0.15
	root.scale = Vector3(s, s, s)
	return {"root": root, "arm_l": arm_l, "arm_r": arm_r, "leg_l": leg_l, "leg_r": leg_r,
		"head": head, "hips": hips, "eye_mat": eye, "crouched": crouched}

func _build_arm(hips: Node3D, shoulder: Vector3, skin: Material, cloth: Material) -> Node3D:
	var p := Node3D.new()
	p.name = "ArmPivot"
	p.position = shoulder
	hips.add_child(p)
	box(p, Vector3(0.1, 0.3, 0.1), Vector3(0, -0.15, 0), cloth)
	var elbow := Node3D.new()
	elbow.position = Vector3(0, -0.3, 0)
	p.add_child(elbow)
	box(elbow, Vector3(0.09, 0.28, 0.09), Vector3(0, -0.14, 0), skin)
	sphere(elbow, 0.06, Vector3(0, -0.3, 0), skin)
	return p

func _build_leg(hips: Node3D, hip: Vector3, pants: Material, skin: Material) -> Node3D:
	var p := Node3D.new()
	p.name = "LegPivot"
	p.position = hip
	hips.add_child(p)
	box(p, Vector3(0.13, 0.38, 0.14), Vector3(0, -0.19, 0), pants)
	var knee := Node3D.new()
	knee.position = Vector3(0, -0.38, 0)
	p.add_child(knee)
	box(knee, Vector3(0.11, 0.36, 0.12), Vector3(0, -0.18, 0), pants)
	box(knee, Vector3(0.12, 0.09, 0.22), Vector3(0, -0.4, 0.06), skin) # foot
	return p

# ----------------------------------------------------------------- weapons
func make_weapon(id: String) -> Node3D:
	var g := Node3D.new()
	g.name = "ViewModel_" + id
	match id:
		"bat":
			var wood := A.wood()
			cyl(g, 0.035, 0.05, 0.82, Vector3(0, 0, -0.25), wood, 10, Vector3(deg_to_rad(90), 0, 0))
			cyl(g, 0.04, 0.045, 0.18, Vector3(0, 0, 0.22), A.plain("grip", Color(0.12, 0.09, 0.07)), 10, Vector3(deg_to_rad(90), 0, 0))
		"axe":
			cyl(g, 0.025, 0.03, 0.75, Vector3(0, 0, -0.2), A.wood(), 10, Vector3(deg_to_rad(90), 0, 0))
			var head := A.dark_metal()
			box(g, Vector3(0.26, 0.12, 0.03), Vector3(0.08, 0.02, -0.58), head)
			box(g, Vector3(0.05, 0.18, 0.03), Vector3(-0.04, 0.0, -0.58), head)
		"pistol":
			var metal := A.dark_metal()
			box(g, Vector3(0.05, 0.06, 0.24), Vector3(0, 0.03, -0.1), metal)
			box(g, Vector3(0.05, 0.05, 0.2), Vector3(0, 0.07, -0.12), A.shelf_metal())
			box(g, Vector3(0.05, 0.14, 0.08), Vector3(0, -0.07, 0.0), A.plain("pgrip", Color(0.15, 0.13, 0.12)))
			cyl(g, 0.018, 0.018, 0.06, Vector3(0, 0.05, -0.24), metal, 8, Vector3(deg_to_rad(90), 0, 0))
		"shotgun":
			var metal := A.dark_metal()
			cyl(g, 0.022, 0.022, 0.62, Vector3(0, 0.03, -0.28), metal, 10, Vector3(deg_to_rad(90), 0, 0))
			cyl(g, 0.03, 0.03, 0.3, Vector3(0, 0.0, 0.05), A.wood(), 10, Vector3(deg_to_rad(90), 0, 0))
			box(g, Vector3(0.07, 0.1, 0.22), Vector3(0, -0.05, 0.18), A.wood())
			box(g, Vector3(0.05, 0.05, 0.16), Vector3(0, -0.02, -0.1), metal)
		"rifle":
			var metal := A.dark_metal()
			box(g, Vector3(0.07, 0.1, 0.5), Vector3(0, 0.0, -0.1), metal)
			cyl(g, 0.016, 0.016, 0.4, Vector3(0, 0.03, -0.5), metal, 8, Vector3(deg_to_rad(90), 0, 0))
			box(g, Vector3(0.06, 0.16, 0.08), Vector3(0, -0.1, 0.05), A.plain("rmag", Color(0.1, 0.1, 0.11)))
			box(g, Vector3(0.06, 0.09, 0.2), Vector3(0, -0.02, 0.27), A.plain("rstock", Color(0.18, 0.14, 0.1)))
			box(g, Vector3(0.04, 0.05, 0.12), Vector3(0, 0.09, -0.1), metal)
		"hunting":
			var wood := A.wood()
			cyl(g, 0.018, 0.018, 0.7, Vector3(0, 0.04, -0.3), A.dark_metal(), 10, Vector3(deg_to_rad(90), 0, 0))
			box(g, Vector3(0.07, 0.1, 0.45), Vector3(0, -0.01, 0.12), wood)
			cyl(g, 0.03, 0.03, 0.16, Vector3(0, 0.08, -0.05), A.dark_metal(), 10)
			box(g, Vector3(0.05, 0.05, 0.1), Vector3(0, 0.05, 0.3), wood)
		"crossbow":
			var wood := A.wood()
			box(g, Vector3(0.06, 0.08, 0.5), Vector3(0, 0, 0.05), wood)
			cyl(g, 0.02, 0.02, 0.6, Vector3(0, 0.05, -0.2), A.dark_metal(), 8, Vector3(0, 0, deg_to_rad(90)))
			# limbs (angled)
			cyl(g, 0.015, 0.02, 0.34, Vector3(-0.17, 0.05, -0.2), A.dark_metal(), 8, Vector3(0, 0, deg_to_rad(28)))
			cyl(g, 0.015, 0.02, 0.34, Vector3(0.17, 0.05, -0.2), A.dark_metal(), 8, Vector3(0, 0, deg_to_rad(-28)))
			box(g, Vector3(0.01, 0.01, 0.45), Vector3(0, 0.05, -0.2), A.plain("string", Color(0.9, 0.9, 0.85)))
			box(g, Vector3(0.05, 0.05, 0.12), Vector3(0, -0.06, 0.25), wood)
	g.scale = Vector3(0.85, 0.85, 0.85)
	return g

# ------------------------------------------------------------- environment
func make_shelf() -> Node3D:
	var g := Node3D.new()
	var metal := A.shelf_metal()
	for x in [-0.85, 0.85]:
		box(g, Vector3(0.07, 1.9, 0.5), Vector3(x, 0.95, 0), metal)
	for y in [0.35, 0.95, 1.55]:
		box(g, Vector3(1.8, 0.06, 0.55), Vector3(0, y, 0), metal)
		for i in 5:
			var pw := 0.3
			box(g, Vector3(pw, 0.28, 0.3), Vector3(-0.72 + i * 0.36, y + 0.17, 0.02), A.products_mat())
	# price rail
	box(g, Vector3(1.8, 0.05, 0.05), Vector3(0, 0.9, 0.29), A.plain("rail", Color(0.8, 0.65, 0.2), 0.5, 0.4))
	return g

func make_cart() -> Node3D:
	var g := Node3D.new()
	var metal := A.shelf_metal()
	box(g, Vector3(0.7, 0.05, 0.5), Vector3(0, 0.45, 0), metal)
	box(g, Vector3(0.7, 0.35, 0.05), Vector3(0, 0.72, -0.24), metal)
	box(g, Vector3(0.05, 0.35, 0.5), Vector3(-0.35, 0.72, 0), metal)
	box(g, Vector3(0.05, 0.35, 0.5), Vector3(0.35, 0.72, 0), metal)
	box(g, Vector3(0.7, 0.35, 0.05), Vector3(0, 0.72, 0.24), metal)
	box(g, Vector3(0.7, 0.05, 0.5), Vector3(0, 0.18, 0.05), metal)
	cyl(g, 0.025, 0.025, 0.8, Vector3(0, 1.0, 0.28), metal, 8, Vector3(0, 0, deg_to_rad(90)))
	for x in [-0.28, 0.28]:
		for z in [-0.18, 0.18]:
			sphere(g, 0.07, Vector3(x, 0.07, z), A.dark_metal())
	return g

func make_checkout() -> Node3D:
	var g := Node3D.new()
	box(g, Vector3(1.7, 0.9, 0.7), Vector3(0, 0.45, 0), A.plain("counter", Color(0.25, 0.3, 0.38), 0.6, 0.3))
	box(g, Vector3(1.75, 0.08, 0.75), Vector3(0, 0.94, 0), A.dark_metal())
	# conveyor
	box(g, Vector3(1.1, 0.06, 0.5), Vector3(-0.25, 1.0, 0), A.plain("belt", Color(0.08, 0.08, 0.09), 0.95))
	# register
	box(g, Vector3(0.35, 0.25, 0.3), Vector3(0.55, 1.12, -0.1), A.plain("reg", Color(0.2, 0.22, 0.25), 0.5, 0.4))
	box(g, Vector3(0.3, 0.2, 0.03), Vector3(0.55, 1.18, 0.06), A.plain("screen", Color(0.1, 0.2, 0.25), 0.3, 0.0, Color(0.2, 0.7, 0.8), 1.4))
	return g

func make_fridge() -> Node3D:
	var g := Node3D.new()
	var metal := A.fridge_metal()
	box(g, Vector3(1.8, 2.1, 0.7), Vector3(0, 1.05, -0.05), metal)
	for i in 2:
		box(g, Vector3(0.82, 1.7, 0.03), Vector3(-0.43 + i * 0.86, 1.1, 0.32), A.glass())
		box(g, Vector3(0.04, 1.7, 0.04), Vector3(i * 0.86, 1.1, 0.34), metal)
	for y in [0.5, 1.1, 1.7]:
		for i in 4:
			box(g, Vector3(0.3, 0.22, 0.25), Vector3(-0.6 + i * 0.4, y, 0.1), A.products_mat())
	box(g, Vector3(1.8, 0.12, 0.1), Vector3(0, 2.15, 0.2), A.plain("fridgelight", Color(0.8, 0.9, 1.0), 0.4, 0.0, Color(0.7, 0.85, 1.0), 2.0))
	return g

func make_box() -> Node3D:
	var g := Node3D.new()
	var s := 0.5 + (0.3 * float(hash(g.get_instance_id()) & 7) / 7.0)
	box(g, Vector3(s, s, s), Vector3(0, s * 0.5, 0), A.cardboard())
	box(g, Vector3(s * 1.02, 0.08, s * 1.02), Vector3(0, s * 0.5, 0), A.plain("tape", Color(0.75, 0.68, 0.5), 0.8))
	return g

func make_pallet() -> Node3D:
	var g := Node3D.new()
	var wood := A.wood()
	for i in 3:
		box(g, Vector3(1.2, 0.06, 0.18), Vector3(0, 0.14, -0.4 + i * 0.4), wood)
	for i in 3:
		box(g, Vector3(0.18, 0.12, 1.0), Vector3(-0.45 + i * 0.45, 0.06, 0), wood)
	return g

func make_desk() -> Node3D:
	var g := Node3D.new()
	box(g, Vector3(1.4, 0.08, 0.7), Vector3(0, 0.75, 0), A.wood())
	for x in [-0.6, 0.6]:
		box(g, Vector3(0.08, 0.75, 0.6), Vector3(x, 0.37, 0), A.dark_metal())
	box(g, Vector3(0.4, 0.25, 0.04), Vector3(0.3, 0.92, -0.2), A.plain("monitor", Color(0.1, 0.12, 0.15), 0.4, 0.3, Color(0.2, 0.5, 0.7), 1.2))
	return g

func make_console() -> Node3D:
	var g := Node3D.new()
	box(g, Vector3(1.6, 0.8, 0.6), Vector3(0, 0.4, 0), A.dark_metal())
	for i in 4:
		box(g, Vector3(0.32, 0.22, 0.05), Vector3(-0.6 + i * 0.4, 0.75, 0.15),
			A.plain("mon" + str(i), Color(0.08, 0.12, 0.14), 0.3, 0.0, Color(0.25, 0.8, 0.6), 1.6))
	for i in 8:
		box(g, Vector3(0.1, 0.04, 0.1), Vector3(-0.55 + i * 0.16, 0.85, -0.1),
			A.plain("btn" + str(i), Color(0.8, 0.3, 0.25), 0.5, 0.2, Color(0.9, 0.4, 0.3), 0.8))
	return g

func make_locker() -> Node3D:
	var g := Node3D.new()
	box(g, Vector3(0.6, 1.85, 0.5), Vector3(0, 0.92, 0), A.fridge_metal())
	for i in 2:
		box(g, Vector3(0.27, 1.7, 0.03), Vector3(-0.15 + i * 0.3, 0.95, 0.26), A.door_metal())
	return g

func make_panel() -> Node3D:
	var g := Node3D.new()
	box(g, Vector3(1.2, 1.8, 0.3), Vector3(0, 0.9, 0), A.dark_metal())
	box(g, Vector3(1.0, 1.5, 0.05), Vector3(0, 0.9, 0.17), A.plain("panelface", Color(0.25, 0.27, 0.3), 0.6, 0.5))
	for i in 5:
		cyl(g, 0.04, 0.04, 0.06, Vector3(-0.4 + i * 0.2, 1.3, 0.2),
			A.plain("lamp" + str(i), Color(0.1, 0.1, 0.1), 0.4, 0.0, Color(1.0, 0.6, 0.1), 2.0), 8, Vector3(deg_to_rad(90), 0, 0))
	return g

func make_terminal() -> Node3D:
	var g := Node3D.new()
	box(g, Vector3(0.5, 1.1, 0.4), Vector3(0, 0.55, 0), A.dark_metal())
	box(g, Vector3(0.42, 0.3, 0.05), Vector3(0, 1.05, 0.15),
		A.plain("termscreen", Color(0.05, 0.1, 0.08), 0.3, 0.0, Color(0.3, 1.0, 0.6), 1.8))
	return g

func make_stairs() -> Node3D:
	var g := Node3D.new()
	var wood := A.wood()
	for i in 8:
		box(g, Vector3(1.2, 0.18, 0.34), Vector3(0, 0.09 + i * 0.42, -i * 0.34), wood)
	box(g, Vector3(1.4, 0.12, 1.4), Vector3(0, 3.4, -3.1), A.floor_concrete())
	return g

func make_car(col: Array) -> Node3D:
	var g := Node3D.new()
	var rgb: Array = col
	var paint := A.plain("carpaint", Color(float(rgb[0]) / 255.0, float(rgb[1]) / 255.0, float(rgb[2]) / 255.0), 0.35, 0.6)
	var glass := A.glass()
	box(g, Vector3(1.9, 0.55, 4.0), Vector3(0, 0.55, 0), paint)
	box(g, Vector3(1.7, 0.6, 2.0), Vector3(0, 1.05, -0.2), paint)
	box(g, Vector3(1.55, 0.45, 1.9), Vector3(0, 1.1, -0.2), glass)
	for x in [-0.95, 0.95]:
		for z in [-1.3, 1.3]:
			cyl(g, 0.32, 0.32, 0.25, Vector3(x, 0.32, z), A.dark_metal(), 14, Vector3(0, 0, deg_to_rad(90)))
	box(g, Vector3(1.6, 0.12, 0.1), Vector3(0, 0.6, 2.02), A.plain("headlight", Color(0.9, 0.9, 0.8), 0.3, 0.0, Color(1, 0.95, 0.8), 2.5))
	return g

func make_truck() -> Node3D:
	var g := Node3D.new()
	var paint := A.plain("truckpaint", Color(0.35, 0.4, 0.45), 0.5, 0.5)
	box(g, Vector3(2.4, 2.2, 4.5), Vector3(0, 1.5, 0.8), A.plain("cargo", Color(0.7, 0.7, 0.68), 0.8))
	box(g, Vector3(2.3, 1.8, 1.8), Vector3(0, 1.3, -2.3), paint)
	box(g, Vector3(2.0, 0.8, 1.6), Vector3(0, 1.9, -2.3), A.glass())
	for x in [-1.15, 1.15]:
		for z in [-2.4, 0.2, 2.4]:
			cyl(g, 0.42, 0.42, 0.3, Vector3(x, 0.42, z), A.dark_metal(), 14, Vector3(0, 0, deg_to_rad(90)))
	return g

func make_fence_segment() -> Node3D:
	var g := Node3D.new()
	var metal := A.dark_metal()
	for x in [-0.95, 0.95]:
		box(g, Vector3(0.08, 1.8, 0.08), Vector3(x, 0.9, 0), metal)
	for y in [0.4, 0.9, 1.4]:
		box(g, Vector3(2.0, 0.05, 0.05), Vector3(0, y, 0), metal)
	return g

func make_streetlight() -> Node3D:
	var g := Node3D.new()
	cyl(g, 0.06, 0.09, 5.0, Vector3(0, 2.5, 0), A.dark_metal(), 10)
	box(g, Vector3(0.9, 0.08, 0.12), Vector3(0.4, 4.9, 0), A.dark_metal())
	var lamp := Node3D.new()
	lamp.position = Vector3(0.8, 4.8, 0)
	g.add_child(lamp)
	box(lamp, Vector3(0.45, 0.12, 0.25), Vector3.ZERO,
		A.plain("lamphead", Color(0.9, 0.9, 0.8), 0.4, 0.0, Color(1.0, 0.9, 0.7), 3.0))
	return lamp

func make_ceiling_light() -> Node3D:
	var g := Node3D.new()
	box(g, Vector3(1.2, 0.08, 0.35), Vector3.ZERO,
		A.plain("ceil", Color(0.85, 0.85, 0.8), 0.5, 0.0, Color(1.0, 0.95, 0.85), 2.4))
	return g

func make_loot_crate(tier: int) -> Node3D:
	var g := Node3D.new()
	var cols: Array = [Color(0.45, 0.35, 0.25), Color(0.3, 0.4, 0.5), Color(0.5, 0.35, 0.55)]
	var col: Color = cols[clampi(tier - 1, 0, 2)]
	box(g, Vector3(0.55, 0.45, 0.55), Vector3(0, 0.225, 0), A.plain("crate", col, 0.8))
	box(g, Vector3(0.6, 0.08, 0.6), Vector3(0, 0.45, 0), A.dark_metal())
	return g

func make_barricade(level: int, hp_ratio: float) -> Node3D:
	var g := Node3D.new()
	var plank_count: int = [0, 3, 5, 6][clampi(level, 0, 3)]
	var shown: int = max(1, int(round(plank_count * hp_ratio))) if level > 0 else 0
	var mat: Material = A.wood() if level < 2 else A.dark_metal()
	var gap := 0.55
	for i in range(shown):
		var y := 0.45 + i * gap
		if level == 3:
			box(g, Vector3(0.12, 0.42, 1.9), Vector3(0, y, 0), mat)
		else:
			box(g, Vector3(0.1, 0.4, 1.9), Vector3(0, y, 0), mat, Vector3(0, (i % 2) * 4 - 2, 0))
	if level == 3:
		box(g, Vector3(0.16, 2.4, 0.12), Vector3(0, 1.2, -0.95), mat)
		box(g, Vector3(0.16, 2.4, 0.12), Vector3(0, 1.2, 0.95), mat)
	return g

func make_survivor(variant: int) -> Node3D:
	var g := Node3D.new()
	g.name = "SurvivorRig"
	var jacket_cols := [Color(0.25, 0.32, 0.38), Color(0.4, 0.28, 0.24), Color(0.3, 0.36, 0.26), Color(0.34, 0.3, 0.4)]
	var jacket := A.plain("surv_jacket" + str(variant), jacket_cols[variant % 4], 0.9)
	var pants := A.plain("surv_pants" + str(variant), Color(0.16, 0.17, 0.2), 0.95)
	var skin := A.plain("surv_skin" + str(variant), Color(0.72, 0.58, 0.48), 0.8)
	# legs
	for sx in [-0.11, 0.11]:
		var leg := Node3D.new()
		leg.position = Vector3(sx, 0.92, 0)
		g.add_child(leg)
		box(leg, Vector3(0.14, 0.8, 0.16), Vector3(0, -0.4, 0), pants)
		box(leg, Vector3(0.15, 0.1, 0.26), Vector3(0, -0.82, 0.05), A.dark_metal())
	# torso
	box(g, Vector3(0.42, 0.55, 0.26), Vector3(0, 1.22, 0), jacket)
	box(g, Vector3(0.46, 0.14, 0.28), Vector3(0, 1.52, 0), A.dark_metal()) # vest
	# arms
	for sx in [-0.26, 0.26]:
		var arm := Node3D.new()
		arm.position = Vector3(sx, 1.46, 0)
		g.add_child(arm)
		box(arm, Vector3(0.11, 0.55, 0.12), Vector3(0, -0.27, 0.04), jacket)
		sphere(arm, 0.06, Vector3(0, -0.58, 0.05), skin)
	# head
	sphere(g, 0.15, Vector3(0, 1.72, 0), skin)
	box(g, Vector3(0.32, 0.14, 0.3), Vector3(0, 1.82, 0), A.plain("cap" + str(variant), Color(0.2, 0.2, 0.22), 0.8))
	box(g, Vector3(0.2, 0.04, 0.12), Vector3(0, 1.78, 0.16), A.plain("capbrim", Color(0.15, 0.15, 0.16), 0.8))
	return g

func make_trap() -> Node3D:
	var g := Node3D.new()
	box(g, Vector3(0.8, 0.04, 0.8), Vector3(0, 0.02, 0), A.dark_metal())
	for i in 4:
		for j in 4:
			cyl(g, 0.0, 0.02, 0.12, Vector3(-0.3 + i * 0.2, 0.1, -0.3 + j * 0.2), A.shelf_metal(), 6)
	return g
