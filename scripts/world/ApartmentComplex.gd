class_name ApartmentComplex
extends Node3D
## Complete Multi-Floor Brutalist Apartment Complex Builder & Modular Controller.
## Creates all connected corridors, apartments (4A, 4B, 4C), stairwell, elevator,
## laundry room, storage rooms, maintenance room, basement boiler core, emergency
## exits, and 2 hidden secret rooms with zero wall/door overlaps, full PBR materials,
## NavigationRegion3D, and controlled procedural room/corridor variations.

signal threshold_decision_submitted(reported_anomaly: bool)
signal emergency_exit_used

# Registries exposed to AnomalyManager, AI Entities, and Automated Tests
var doors: Dictionary = {}               # door_id -> InteractableDoor
var corridor_lights: Array[OmniLight3D] = []
var corridor_wall_meshes: Array[MeshInstance3D] = []
var emergency_beacons: Array[Node3D] = []
var emergency_lights: Array[OmniLight3D] = []
var room_zones: Array[Dictionary] = []   # {name, aabb, surface}
var nav_waypoints: Dictionary = {}       # node_id -> {pos: Vector3, neighbors: Array[String]}
var nav_region: NavigationRegion3D
var elevator: ElevatorSystem

# Specific Anomaly & Story Hooks
var hero_painting: Node3D
var hero_painting_canvas: MeshInstance3D
var apt4a_furniture_root: Node3D
var apt4a_furniture_baseline: Array[Dictionary] = []
var apt4a_tv: Node3D
var apt4a_window_pane: MeshInstance3D
var apt4b_window_pane: MeshInstance3D
var apt4b_coat_normal: Node3D
var apt4b_coat_twisted: Node3D
var apt4b_washer_anomaly: Node3D
var apt4b_wardrobe: Node3D
var apt4c_radio: Node3D
var apt4c_modular_root: Node3D
var cross_swing_fixture: Node3D
var impossible_corridor_door: InteractableDoor
var blank_west_wall_plug: Node3D
var north_std_cap: Node3D
var north_ext_module: Node3D
var north_exit_door: InteractableDoor
var cross_barricade_root: Node3D
var secret_bookshelf_pivot: Node3D
var master_key_prop: Node3D
var boiler_core_console: WorldInteractable
var progress_board_label: Label3D

var current_corridor_extended: bool = false
var current_apt4c_variant: int = 0

var _geo_root: Node3D
var _props_root: Node3D


func build_full_complex() -> void:
	_geo_root = Node3D.new()
	_geo_root.name = "ArchitectureGeometry"
	add_child(_geo_root)

	_props_root = Node3D.new()
	_props_root.name = "FurnishingsAndProps"
	add_child(_props_root)

	_register_zones()
	_build_south_threshold_hub()
	_build_main_spine_corridor()
	_build_cross_hallway()
	_build_apartment_4a()
	_build_apartment_4b_and_crawlspace()
	_build_apartment_4c_and_sanctum()
	_build_laundry_room()
	_build_storage_wing()
	_build_maintenance_room()
	_build_stairwell_and_basement()
	_build_elevator_shaft_and_system()
	_build_navigation_system()


# ============================================================
# ARCHITECTURAL PRIMITIVE BUILDERS (Zero-Overlap Wall/Doorway Math)
# ============================================================
func _add_solid_box(parent: Node3D, sz: Vector3, center: Vector3, mat: Material, surface_tag: String = "wood", add_col: bool = true) -> MeshInstance3D:
	var mi := MeshInstance3D.new()
	var bm := BoxMesh.new()
	bm.size = sz
	mi.mesh = bm
	mi.position = center
	mi.material_override = mat
	parent.add_child(mi)

	if add_col:
		var sb := StaticBody3D.new()
		sb.set_meta("surface", surface_tag)
		mi.add_child(sb)
		var cs := CollisionShape3D.new()
		var bs := BoxShape3D.new()
		bs.size = sz
		cs.shape = bs
		sb.add_child(cs)
	return mi


func _add_wall_x(parent: Node3D, x1: float, x2: float, z: float, y_base: float, height: float, mat: Material, door_centers_x: Array = [], thick: float = 0.22) -> Array[MeshInstance3D]:
	# Builds a wall along X at fixed Z, cleanly splitting around 1.08m wide x 2.24m high doorways
	var meshes: Array[MeshInstance3D] = []
	var doors_sorted := door_centers_x.duplicate()
	doors_sorted.sort()
	var cur_x := minf(x1, x2)
	var end_x := maxf(x1, x2)
	var door_w := 1.08
	var door_h := 2.24

	for dcx in doors_sorted:
		var dl := float(dcx) - door_w * 0.5
		var dr := float(dcx) + door_w * 0.5
		if dl > cur_x + 0.02:
			var seg_w := dl - cur_x
			meshes.append(_add_solid_box(parent, Vector3(seg_w, height, thick), Vector3(cur_x + seg_w * 0.5, y_base + height * 0.5, z), mat, "concrete"))
		# Doorway header lintel
		var lintel_h := height - door_h
		if lintel_h > 0.04:
			meshes.append(_add_solid_box(parent, Vector3(door_w, lintel_h, thick), Vector3(float(dcx), y_base + door_h + lintel_h * 0.5, z), mat, "concrete"))
		cur_x = dr

	if end_x > cur_x + 0.02:
		var rem_w := end_x - cur_x
		meshes.append(_add_solid_box(parent, Vector3(rem_w, height, thick), Vector3(cur_x + rem_w * 0.5, y_base + height * 0.5, z), mat, "concrete"))
	return meshes


func _add_wall_z(parent: Node3D, x: float, z1: float, z2: float, y_base: float, height: float, mat: Material, door_centers_z: Array = [], thick: float = 0.22) -> Array[MeshInstance3D]:
	var meshes: Array[MeshInstance3D] = []
	var doors_sorted := door_centers_z.duplicate()
	doors_sorted.sort()
	var cur_z := minf(z1, z2)
	var end_z := maxf(z1, z2)
	var door_w := 1.08
	var door_h := 2.24

	for dcz in doors_sorted:
		var dl := float(dcz) - door_w * 0.5
		var dr := float(dcz) + door_w * 0.5
		if dl > cur_z + 0.02:
			var seg_l := dl - cur_z
			meshes.append(_add_solid_box(parent, Vector3(thick, height, seg_l), Vector3(x, y_base + height * 0.5, cur_z + seg_l * 0.5), mat, "concrete"))
		var lintel_h := height - door_h
		if lintel_h > 0.04:
			meshes.append(_add_solid_box(parent, Vector3(thick, lintel_h, door_w), Vector3(x, y_base + door_h + lintel_h * 0.5, float(dcz)), mat, "concrete"))
		cur_z = dr

	if end_z > cur_z + 0.02:
		var rem_l := end_z - cur_z
		meshes.append(_add_solid_box(parent, Vector3(thick, height, rem_l), Vector3(x, y_base + height * 0.5, cur_z + rem_l * 0.5), mat, "concrete"))
	return meshes


func _place_prop(parent: Node3D, glb_file: String, pos: Vector3, yaw_deg: float = 0.0, col_size: Vector3 = Vector3.ZERO, col_center_y: float = -1.0) -> Node3D:
	var inst := MaterialLibrary.instantiate_model(glb_file)
	inst.position = pos
	inst.rotation.y = deg_to_rad(yaw_deg)
	parent.add_child(inst)
	if col_size != Vector3.ZERO:
		var sb := StaticBody3D.new()
		inst.add_child(sb)
		var cs := CollisionShape3D.new()
		var bs := BoxShape3D.new()
		bs.size = col_size
		cs.shape = bs
		cs.position = Vector3(0, col_size.y * 0.5 if col_center_y < 0.0 else col_center_y, 0)
		sb.add_child(cs)
	return inst


func _place_door(id: String, label: String, hinge_pos: Vector3, yaw_deg: float, model: String = "door_wood_panel.glb", swing: float = 92.0, locked: bool = false, reason: String = "Locked.") -> InteractableDoor:
	var d := InteractableDoor.new()
	d.name = "Door_" + id
	d.setup(id, label, hinge_pos, yaw_deg, model, swing, locked, reason)
	_props_root.add_child(d)
	doors[id] = d
	return d


func _add_light_fixture(parent: Node3D, pos: Vector3, color: Color = Color(1.0, 0.88, 0.68), energy: float = 1.35, rng: float = 9.5, cast_shadow: bool = false, register_corridor: bool = false) -> OmniLight3D:
	var fx := MaterialLibrary.instantiate_model("fluorescent_fixture.glb")
	fx.position = pos
	parent.add_child(fx)

	var l := OmniLight3D.new()
	l.position = pos + Vector3(0, -0.22, 0)
	l.light_color = color
	l.light_energy = energy
	l.omni_range = rng
	l.omni_attenuation = 1.35
	l.shadow_enabled = cast_shadow
	parent.add_child(l)
	if register_corridor:
		corridor_lights.append(l)
	return l


func _add_emergency_beacon(parent: Node3D, pos: Vector3, yaw_deg: float) -> void:
	var b := MaterialLibrary.instantiate_model("emergency_beacon.glb")
	b.position = pos
	b.rotation.y = deg_to_rad(yaw_deg)
	b.visible = false
	parent.add_child(b)
	emergency_beacons.append(b)

	var rl := OmniLight3D.new()
	rl.position = pos + Vector3(0, -0.1, 0)
	rl.light_color = Color(1.0, 0.14, 0.10)
	rl.light_energy = 0.0
	rl.omni_range = 8.5
	rl.visible = false
	parent.add_child(rl)
	emergency_lights.append(rl)


func _place_document_pickup(parent: Node3D, doc_id: String, pos: Vector3, yaw_deg: float = 0.0) -> void:
	var meta: Dictionary = GameState.DOCUMENTS[doc_id]
	var wi := WorldInteractable.new()
	wi.name = "Doc_" + doc_id
	wi.interact_id = doc_id
	wi.position = pos
	wi.rotation.y = deg_to_rad(yaw_deg)
	wi.prompt_text = "[E] Read & Archive: " + meta["title"]
	wi.setup_box_collider(Vector3(0.32, 0.12, 0.40), Vector3(0, 0.04, 0))
	wi.on_interact = func(_a):
		AudioDirector.play_2d("scanner_confirm", -4.0, 1.15)
		GameState.collect_document(doc_id)
		get_tree().call_group("hud_listeners", "open_document_modal", doc_id)
	parent.add_child(wi)

	var sheet := MeshInstance3D.new()
	var qm := BoxMesh.new()
	qm.size = Vector3(0.24, 0.012, 0.32)
	sheet.mesh = qm
	sheet.position = Vector3(0, 0.01, 0)
	sheet.material_override = MaterialLibrary.get_image_mat("res://assets/textures/document_page.png", 0.15)
	wi.add_child(sheet)


func _place_tape_recorder(parent: Node3D, tape_id: String, pos: Vector3, yaw_deg: float = 0.0) -> void:
	var meta: Dictionary = GameState.TAPE_LOGS[tape_id]
	var wi := WorldInteractable.new()
	wi.name = "Tape_" + tape_id
	wi.interact_id = tape_id
	wi.position = pos
	wi.rotation.y = deg_to_rad(yaw_deg)
	wi.prompt_text = "[E] Play Recording: " + meta["title"]
	wi.setup_box_collider(Vector3(0.42, 0.24, 0.34), Vector3(0, 0.08, 0))
	wi.on_interact = func(_a):
		AudioDirector.play_tape(tape_id)
	parent.add_child(wi)

	var rec := MaterialLibrary.instantiate_model("tape_recorder.glb")
	wi.add_child(rec)


func _place_plaque(parent: Node3D, text: String, pos: Vector3, yaw_deg: float, col: Color = Color(0.90, 0.84, 0.68)) -> Label3D:
	var lbl := Label3D.new()
	lbl.text = text
	lbl.font_size = 28
	lbl.modulate = col
	lbl.outline_size = 6
	lbl.position = pos
	lbl.rotation.y = deg_to_rad(yaw_deg)
	parent.add_child(lbl)
	return lbl


# ============================================================
# ROOM & CORRIDOR CONSTRUCTION
# ============================================================
func _register_zones() -> void:
	room_zones = [
		{"name": "Threshold Security Hub", "aabb": AABB(Vector3(-4.2, -0.5, 14.8), Vector3(8.4, 4.0, 6.8)), "surface": "tile"},
		{"name": "Main Corridor (South)", "aabb": AABB(Vector3(-1.6, -0.5, -2.0), Vector3(3.2, 4.0, 17.0)), "surface": "carpet"},
		{"name": "Main Corridor (North)", "aabb": AABB(Vector3(-1.6, -0.5, -30.0), Vector3(3.2, 4.0, 25.0)), "surface": "carpet"},
		{"name": "Cross Hallway", "aabb": AABB(Vector3(-11.5, -0.5, -5.2), Vector3(27.0, 4.0, 3.2)), "surface": "carpet"},
		{"name": "Apartment 4A", "aabb": AABB(Vector3(-10.5, -0.5, 5.2), Vector3(8.9, 4.0, 9.3)), "surface": "wood"},
		{"name": "Apartment 4B", "aabb": AABB(Vector3(1.6, -0.5, 5.2), Vector3(8.9, 4.0, 9.3)), "surface": "wood"},
		{"name": "Surveillance Crawlspace", "aabb": AABB(Vector3(10.5, -0.5, 5.5), Vector3(3.3, 4.0, 5.0)), "surface": "concrete"},
		{"name": "Apartment 4C (Acoustic Study)", "aabb": AABB(Vector3(-13.5, -0.5, -14.5), Vector3(11.9, 4.0, 9.3)), "surface": "wood"},
		{"name": "Architect's Sanctum", "aabb": AABB(Vector3(-18.5, -0.5, -13.5), Vector3(5.0, 4.0, 7.0)), "surface": "concrete"},
		{"name": "Laundry Room", "aabb": AABB(Vector3(6.2, -0.5, -2.0), Vector3(9.0, 4.0, 6.8)), "surface": "tile"},
		{"name": "Storage Wing", "aabb": AABB(Vector3(1.6, -0.5, -14.5), Vector3(11.9, 4.0, 9.3)), "surface": "concrete"},
		{"name": "Maintenance Room", "aabb": AABB(Vector3(15.5, -0.5, -7.5), Vector3(5.4, 4.0, 8.0)), "surface": "concrete"},
		{"name": "Stairwell", "aabb": AABB(Vector3(-10.8, -4.8, -2.0), Vector3(6.0, 8.4, 6.6)), "surface": "metal"},
		{"name": "Basement Corridor", "aabb": AABB(Vector3(-10.8, -4.8, -2.5), Vector3(17.5, 4.0, 7.2)), "surface": "concrete"},
		{"name": "Basement Boiler Core", "aabb": AABB(Vector3(-2.5, -4.8, -11.5), Vector3(9.0, 4.0, 9.0)), "surface": "metal"},
	]


func get_zone_at(pos: Vector3) -> Dictionary:
	for z in room_zones:
		if (z["aabb"] as AABB).has_point(pos):
			return z
	return {"name": "Kestrel Court Complex", "surface": "concrete"}


func _build_south_threshold_hub() -> void:
	var m_tile := MaterialLibrary.get_pbr("floor_tile_linoleum", 0.45)
	var m_conc := MaterialLibrary.get_pbr("wall_concrete_brutalist", 0.40)
	var m_ceil := MaterialLibrary.get_pbr("ceiling_acoustic", 0.45)
	var m_wood := MaterialLibrary.get_pbr("wood_walnut_furniture", 0.45)

	_add_solid_box(_geo_root, Vector3(8.4, 0.2, 6.6), Vector3(0, -0.1, 18.2), m_tile, "tile")
	_add_solid_box(_geo_root, Vector3(8.4, 0.2, 6.6), Vector3(0, 3.3, 18.2), m_ceil, "concrete")
	_add_wall_x(_geo_root, -4.2, 4.2, 21.5, 0.0, 3.2, m_conc)
	_add_wall_z(_geo_root, -4.2, 15.0, 21.5, 0.0, 3.2, m_conc)
	_add_wall_z(_geo_root, 4.2, 15.0, 21.5, 0.0, 3.2, m_conc)
	# North wall joins Main Spine (-1.6..+1.6 open portal!)
	_add_wall_x(_geo_root, -4.2, -1.6, 15.0, 0.0, 3.2, m_conc)
	_add_wall_x(_geo_root, 1.6, 4.2, 15.0, 0.0, 3.2, m_conc)

	_add_light_fixture(_props_root, Vector3(0.0, 3.18, 18.2), Color(1.0, 0.92, 0.76), 1.6, 11.0, true, false)

	# Security Desk + Lamp + Document #01
	_place_prop(_props_root, "desk_study.glb", Vector3(0.0, 0.0, 19.6), 180.0, Vector3(1.36, 0.76, 0.68))
	_place_prop(_props_root, "lamp_desk.glb", Vector3(-0.46, 0.75, 19.55), 150.0)
	_place_document_pickup(_props_root, "doc_01", Vector3(0.18, 0.76, 19.50), 180.0)
	_place_prop(_props_root, "filing_cabinet.glb", Vector3(-3.6, 0.0, 20.6), 180.0, Vector3(0.5, 1.32, 0.64))
	_place_prop(_props_root, "wooden_chair.glb", Vector3(0.0, 0.0, 20.45), 180.0, Vector3(0.46, 0.88, 0.48))

	# Threshold Cycle LED Display Board
	_add_solid_box(_props_root, Vector3(2.8, 0.95, 0.08), Vector3(0.0, 2.15, 21.34), m_wood, "wood", false)
	progress_board_label = _place_plaque(_props_root, "INSPECTION CYCLE: 0 / 8\nBASELINE CALIBRATION WALK", Vector3(0.0, 2.15, 21.28), 180.0, Color(0.35, 0.95, 0.58))

	# GREEN CLEARANCE TERMINAL (Left side of Desk: Report NO Anomalies / Floor Clean)
	var clean_btn := WorldInteractable.new()
	clean_btn.name = "ThresholdCleanTerminal"
	clean_btn.position = Vector3(-2.15, 1.15, 21.22)
	clean_btn.prompt_text = "[E] GREEN CLEARANCE LEVER — Confirm Complex CLEAN (0 Anomalies)"
	clean_btn.setup_box_collider(Vector3(0.55, 0.65, 0.28))
	clean_btn.on_interact = func(_a): threshold_decision_submitted.emit(false)
	_props_root.add_child(clean_btn)
	_add_solid_box(clean_btn, Vector3(0.44, 0.54, 0.14), Vector3.ZERO, MaterialLibrary.get_emissive(Color(0.10, 0.35, 0.16), Color(0.18, 0.92, 0.42), 1.6), "metal", false)
	_place_plaque(clean_btn, "CLEAR FLOOR\n[NO ANOMALY]", Vector3(0, 0.42, -0.09), 180.0, Color(0.35, 0.98, 0.55))

	# RED CONTAINMENT TERMINAL (Right side of Desk: Report ANOMALY PRESENT & Reset Floor)
	var anom_btn := WorldInteractable.new()
	anom_btn.name = "ThresholdAnomalyTerminal"
	anom_btn.position = Vector3(2.15, 1.15, 21.22)
	anom_btn.prompt_text = "[E] RED CONTAINMENT LEVER — Report ANOMALY DETECTED & Purge"
	anom_btn.setup_box_collider(Vector3(0.55, 0.65, 0.28))
	anom_btn.on_interact = func(_a): threshold_decision_submitted.emit(true)
	_props_root.add_child(anom_btn)
	_add_solid_box(anom_btn, Vector3(0.44, 0.54, 0.14), Vector3.ZERO, MaterialLibrary.get_emissive(Color(0.42, 0.10, 0.10), Color(0.95, 0.20, 0.16), 1.8), "metal", false)
	_place_plaque(anom_btn, "CONTAINMENT\n[ANOMALY PRESENT]", Vector3(0, 0.42, -0.09), 180.0, Color(1.0, 0.36, 0.32))


func _build_main_spine_corridor() -> void:
	var m_carpet := MaterialLibrary.get_pbr("floor_carpet_corridor", 0.45)
	var m_wall := MaterialLibrary.get_pbr("wall_wallpaper_beige", 0.42)
	var m_ceil := MaterialLibrary.get_pbr("ceiling_acoustic", 0.45)
	var m_wain := MaterialLibrary.get_pbr("wood_walnut_furniture", 0.5)

	# Floor & Ceiling along South + Central + North Spine (-17 to +15)
	_add_solid_box(_geo_root, Vector3(3.2, 0.2, 32.0), Vector3(0, -0.1, -1.0), m_carpet, "carpet")
	_add_solid_box(_geo_root, Vector3(3.2, 0.2, 32.0), Vector3(0, 3.3, -1.0), m_ceil, "concrete")

	# South Spine Walls (Z = -2.0 to +15.0)
	# West wall has Door 4A at Z=+10.0 AND a special modular slot at Z=+3.8 for Anomaly #4 (New Door Appears!)
	corridor_wall_meshes.append_array(_add_wall_z(_geo_root, -1.6, -2.0, 15.0, 0.0, 3.2, m_wall, [3.8, 10.0]))
	# East wall has Elevator Lobby opening at Z=+1.0 (doorway width) and Door 4B at Z=+10.0
	corridor_wall_meshes.append_array(_add_wall_z(_geo_root, 1.6, 2.4, 15.0, 0.0, 3.2, m_wall, [10.0]))

	# Blank wall plug at X=-1.6, Z=+3.8 (visible normally; hidden when Anomaly #4 Impossible Door spawns!)
	blank_west_wall_plug = Node3D.new()
	blank_west_wall_plug.name = "BlankWestWallPlug"
	_geo_root.add_child(blank_west_wall_plug)
	var plug_mi := _add_solid_box(blank_west_wall_plug, Vector3(0.22, 2.25, 1.09), Vector3(-1.6, 1.125, 3.8), m_wall, "concrete")
	corridor_wall_meshes.append(plug_mi)

	# Dark bricked recess behind Anomaly #4 door so opening it reveals an eerie claustrophobic alcove!
	_add_solid_box(_geo_root, Vector3(1.3, 2.4, 1.4), Vector3(-2.35, -0.1, 3.8), m_Wall_conc(), "concrete")
	_add_wall_z(_geo_root, -2.95, 3.1, 4.5, 0.0, 2.4, m_Wall_conc())
	_add_wall_x(_geo_root, -2.95, -1.7, 3.1, 0.0, 2.4, m_Wall_conc())
	_add_wall_x(_geo_root, -2.95, -1.7, 4.5, 0.0, 2.4, m_Wall_conc())

	impossible_corridor_door = _place_door("impossible_v04", "Unlisted Crimson Door", Vector3(-1.6, 0.0, 4.33), 90.0, "door_emergency_exit.glb", -85.0, false)
	impossible_corridor_door.anomaly_tag_id = 4
	impossible_corridor_door.visible = false
	impossible_corridor_door.position.y = -20.0 # Parked out of collision until Anomaly #4 activates

	# North Spine Walls (Z = -17.0 to -5.2)
	corridor_wall_meshes.append_array(_add_wall_z(_geo_root, -1.6, -17.0, -5.2, 0.0, 3.2, m_wall))
	corridor_wall_meshes.append_array(_add_wall_z(_geo_root, 1.6, -17.0, -5.2, 0.0, 3.2, m_wall))

	# Wainscoting strips along Spine
	_add_solid_box(_geo_root, Vector3(0.05, 0.95, 9.8), Vector3(-1.48, 0.48, -11.0), m_wain, "wood", false)
	_add_solid_box(_geo_root, Vector3(0.05, 0.95, 9.8), Vector3(1.48, 0.48, -11.0), m_wain, "wood", false)

	# Standard North End Cap at Z = -17.0 vs Extended +12m Corridor Module (Z = -29.0..-17.0) for Anomaly #20
	north_std_cap = Node3D.new()
	north_std_cap.name = "NorthStandardCap"
	_geo_root.add_child(north_std_cap)
	corridor_wall_meshes.append_array(_add_wall_x(north_std_cap, -1.6, 1.6, -17.0, 0.0, 3.2, m_wall, [0.0]))

	north_ext_module = Node3D.new()
	north_ext_module.name = "NorthExtendedModule"
	north_ext_module.visible = false
	north_ext_module.position.y = -30.0
	_geo_root.add_child(north_ext_module)
	_add_solid_box(north_ext_module, Vector3(3.2, 0.2, 12.2), Vector3(0, -0.1, -23.0), m_carpet, "carpet")
	_add_solid_box(north_ext_module, Vector3(3.2, 0.2, 12.2), Vector3(0, 3.3, -23.0), m_ceil, "concrete")
	corridor_wall_meshes.append_array(_add_wall_z(north_ext_module, -1.6, -29.0, -17.0, 0.0, 3.2, m_wall))
	corridor_wall_meshes.append_array(_add_wall_z(north_ext_module, 1.6, -29.0, -17.0, 0.0, 3.2, m_wall))
	corridor_wall_meshes.append_array(_add_wall_x(north_ext_module, -1.6, 1.6, -29.0, 0.0, 3.2, m_wall, [0.0]))
	_add_light_fixture(north_ext_module, Vector3(0, 3.18, -21.0), Color(1.0, 0.88, 0.68), 1.25, 9.0, false, true)
	_add_light_fixture(north_ext_module, Vector3(0, 3.18, -26.5), Color(1.0, 0.88, 0.68), 1.25, 9.0, false, true)

	# North Emergency Exit Door (Unlockable at Cycle 8 -> Ending A, or Core Purged -> Ending B)
	north_exit_door = _place_door("emergency_exit", "Ground Emergency Exit", Vector3(-0.54, 0.0, -17.0), 0.0, "door_emergency_exit.glb", 90.0, true, "Requires 8/8 Verified Inspection Cycles (or Containment Core Purge).")
	north_exit_door.on_custom_interact = func(): _on_emergency_exit_interacted()
	_place_plaque(_props_root, "EMERGENCY EXIT // CLEARANCE GATE", Vector3(0.0, 2.55, -16.84), 0.0, Color(0.28, 0.96, 0.48))

	# Corridor Ceiling Fixtures & Emergency Beacons
	for lz in [12.0, 6.2, 0.2, -7.5, -13.8]:
		_add_light_fixture(_props_root, Vector3(0.0, 3.18, lz), Color(1.0, 0.88, 0.68), 1.38, 9.5, lz == 6.2, true)
		_add_emergency_beacon(_props_root, Vector3(1.46, 2.65, lz), -90.0)

	# Hero Framed Painting in Main Corridor (East Wall at Z=+6.4) — Anomaly #1 Target!
	hero_painting = _place_prop(_props_root, "painting_framed.glb", Vector3(1.48, 1.62, 6.4), -90.0)
	hero_painting_canvas = hero_painting.find_child("CanvasSurface", true, false) as MeshInstance3D
	if hero_painting_canvas:
		hero_painting_canvas.material_override = MaterialLibrary.get_image_mat("res://assets/textures/painting_coastal_normal.png")

	var paint_col := WorldInteractable.new()
	paint_col.name = "HeroPaintingTarget"
	paint_col.position = Vector3(1.44, 1.62, 6.4)
	paint_col.prompt_text = "[E] Inspect Framed Coastal Canvas  |  [R] Scan if Anomalous"
	paint_col.setup_box_collider(Vector3(0.18, 0.84, 1.06))
	paint_col.on_interact = func(_a):
		GameState.notification_posted.emit("Canvas Plaque: 'Cape Marrow Lighthouse, Oil on Linen, 1976.'", Color(0.84, 0.82, 0.76))
	_props_root.add_child(paint_col)

	# Noticeboard Document #02 in Corridor
	_place_document_pickup(_props_root, "doc_02", Vector3(-1.44, 1.42, 13.0), 90.0)
	_place_plaque(_props_root, "< 4A", Vector3(-1.46, 2.05, 10.8), 90.0)
	_place_plaque(_props_root, "4B >", Vector3(1.46, 2.05, 10.8), -90.0)
	_place_plaque(_props_root, "< STAIRWELL / 4C          ELEVATOR / LAUNDRY / MAINT >", Vector3(0.0, 2.62, -5.05), 0.0)


func m_Wall_conc() -> Material:
	return MaterialLibrary.get_pbr("wall_concrete_brutalist", 0.42)


func _build_cross_hallway() -> void:
	var m_carpet := MaterialLibrary.get_pbr("floor_carpet_corridor", 0.45)
	var m_wall := MaterialLibrary.get_pbr("wall_wallpaper_beige", 0.42)
	var m_ceil := MaterialLibrary.get_pbr("ceiling_acoustic", 0.45)

	# West Cross Hall (-11.5 .. -1.6) + East Cross Hall (+1.6 .. +15.5) at Z = -5.2 .. -2.0
	_add_solid_box(_geo_root, Vector3(9.9, 0.2, 3.2), Vector3(-6.55, -0.1, -3.6), m_carpet, "carpet")
	_add_solid_box(_geo_root, Vector3(9.9, 0.2, 3.2), Vector3(-6.55, 3.3, -3.6), m_ceil, "concrete")
	_add_solid_box(_geo_root, Vector3(13.9, 0.2, 3.2), Vector3(8.55, -0.1, -3.6), m_carpet, "carpet")
	_add_solid_box(_geo_root, Vector3(13.9, 0.2, 3.2), Vector3(8.55, 3.3, -3.6), m_ceil, "concrete")

	# West Cross Walls: North has Door 4C at X=-6.5; South has Stairwell Door at X=-7.8; West end cap at X=-11.5
	corridor_wall_meshes.append_array(_add_wall_x(_geo_root, -11.5, -1.6, -5.2, 0.0, 3.2, m_wall, [-6.5]))
	corridor_wall_meshes.append_array(_add_wall_x(_geo_root, -11.5, -1.6, -2.0, 0.0, 3.2, m_wall, [-7.8]))
	corridor_wall_meshes.append_array(_add_wall_z(_geo_root, -11.5, -5.2, -2.0, 0.0, 3.2, m_wall))

	# East Cross Walls: North has Storage A (X=4.5) & Storage B (X=10.5); South has Elevator Lobby open (X=2.0..6.2) & Laundry Door (X=10.2); East end has Maintenance Door (Z=-3.6 at X=15.5)
	corridor_wall_meshes.append_array(_add_wall_x(_geo_root, 1.6, 15.5, -5.2, 0.0, 3.2, m_wall, [4.5, 10.5]))
	corridor_wall_meshes.append_array(_add_wall_x(_geo_root, 6.2, 15.5, -2.0, 0.0, 3.2, m_wall, [10.2]))
	corridor_wall_meshes.append_array(_add_wall_z(_geo_root, 15.5, -5.2, -2.0, 0.0, 3.2, m_wall, [-3.6]))

	_add_light_fixture(_props_root, Vector3(-6.8, 3.18, -3.6), Color(1.0, 0.88, 0.68), 1.32, 9.5, false, true)
	_add_emergency_beacon(_props_root, Vector3(-6.8, 2.65, -5.06), 0.0)
	_add_emergency_beacon(_props_root, Vector3(11.8, 2.65, -5.06), 0.0)

	# Swinging Troffer Fixture in East Cross Hall (Anomaly #9 Ceiling Fixture Moves!)
	cross_swing_fixture = Node3D.new()
	cross_swing_fixture.name = "CrossHallFixture"
	cross_swing_fixture.position = Vector3(8.2, 3.18, -3.6)
	_props_root.add_child(cross_swing_fixture)
	var cfx := MaterialLibrary.instantiate_model("fluorescent_fixture.glb")
	cross_swing_fixture.add_child(cfx)
	var cl := OmniLight3D.new()
	cl.position = Vector3(0, -0.25, 0)
	cl.light_color = Color(1.0, 0.90, 0.72)
	cl.light_energy = 1.35
	cl.omni_range = 9.5
	cl.shadow_enabled = true
	cross_swing_fixture.add_child(cl)
	corridor_lights.append(cl)

	# Anomaly #24 Barricade in East Cross Hall (parked below floor when inactive)
	cross_barricade_root = Node3D.new()
	cross_barricade_root.name = "CrossHallBarricade"
	cross_barricade_root.visible = false
	cross_barricade_root.position = Vector3(12.6, -25.0, -3.6)
	_props_root.add_child(cross_barricade_root)
	_place_prop(cross_barricade_root, "shelving_metal_boxes.glb", Vector3(0, 0, -0.45), 18.0, Vector3(1.25, 1.9, 0.55))
	_place_prop(cross_barricade_root, "wardrobe_cabinet.glb", Vector3(0.15, 0, 0.62), -72.0, Vector3(1.15, 2.0, 0.60))
	var bar_tag := WorldInteractable.new()
	bar_tag.anomaly_tag_id = 24
	bar_tag.prompt_text = "[R] Scan Fallen Hallway Barricade"
	bar_tag.setup_box_collider(Vector3(1.4, 2.1, 2.8), Vector3(0, 1.05, 0))
	cross_barricade_root.add_child(bar_tag)


func _build_apartment_4a() -> void:
	var m_wood := MaterialLibrary.get_pbr("floor_wood_parquet", 0.45)
	var m_wall := MaterialLibrary.get_pbr("wall_plaster_aged", 0.42)
	var m_ceil := MaterialLibrary.get_pbr("ceiling_acoustic", 0.45)

	_add_solid_box(_geo_root, Vector3(8.9, 0.2, 9.3), Vector3(-6.05, -0.1, 9.85), m_wood, "wood")
	_add_solid_box(_geo_root, Vector3(8.9, 0.2, 9.3), Vector3(-6.05, 3.3, 9.85), m_ceil, "concrete")
	_add_wall_x(_geo_root, -10.5, -1.6, 5.2, 0.0, 3.2, m_wall)
	_add_wall_x(_geo_root, -10.5, -1.6, 14.5, 0.0, 3.2, m_wall)
	_add_wall_z(_geo_root, -10.5, 5.2, 14.5, 0.0, 3.2, m_wall)

	_place_door("apt_4a", "Apartment 4A", Vector3(-1.6, 0.0, 9.47), -90.0, "door_wood_panel.glb", 92.0, false)
	_add_light_fixture(_props_root, Vector3(-6.0, 3.18, 9.8), Color(1.0, 0.89, 0.70), 1.45, 11.0, true, false)

	# Exterior Window on West Wall (X = -10.36, Z = 9.8) — Anomaly #6 Target!
	apt4a_window_pane = _add_solid_box(_props_root, Vector3(0.06, 1.55, 2.20), Vector3(-10.35, 1.65, 9.8), MaterialLibrary.get_image_mat("res://assets/textures/window_city_night.png", 0.85), "concrete", false)
	var win_target := WorldInteractable.new()
	win_target.position = Vector3(-10.28, 1.65, 9.8)
	win_target.prompt_text = "[E] Look Out 4th-Floor Window  |  [R] Scan if Anomalous"
	win_target.setup_box_collider(Vector3(0.22, 1.6, 2.25))
	win_target.on_interact = func(_a):
		GameState.notification_posted.emit("Rain-slicked rooftops of Kestrel District stretch below Floor 4.", Color(0.75, 0.82, 0.90))
	_props_root.add_child(win_target)

	# Movable Living Room Furniture Group (Anomaly #2 Target!)
	apt4a_furniture_root = Node3D.new()
	apt4a_furniture_root.name = "Apt4AFurnitureGroup"
	_props_root.add_child(apt4a_furniture_root)

	var sofa := _place_prop(apt4a_furniture_root, "sofa_vintage.glb", Vector3(-6.2, 0.0, 12.8), 180.0, Vector3(2.0, 0.85, 0.85))
	var chair := _place_prop(apt4a_furniture_root, "armchair_worn.glb", Vector3(-8.4, 0.0, 11.4), 115.0, Vector3(0.9, 0.85, 0.85))
	var ctable := _place_prop(apt4a_furniture_root, "coffee_table.glb", Vector3(-6.2, 0.0, 11.2), 0.0, Vector3(1.2, 0.45, 0.62))
	var flamp := _place_prop(apt4a_furniture_root, "lamp_floor.glb", Vector3(-8.9, 0.0, 13.3), 0.0, Vector3(0.4, 1.62, 0.4))
	for item in [sofa, chair, ctable, flamp]:
		apt4a_furniture_baseline.append({"node": item, "pos": item.position, "rot_y": item.rotation.y})

	# CRT Television Console on Low Credenza (Anomaly #3 Vanishing Object Target!)
	apt4a_tv = _place_prop(_props_root, "tv_crt_console.glb", Vector3(-6.2, 0.45, 8.2), 0.0, Vector3(0.68, 0.52, 0.46))
	_place_prop(_props_root, "coffee_table.glb", Vector3(-6.2, 0.0, 8.2), 0.0, Vector3(1.2, 0.45, 0.62))

	# Kitchenette in North half of Apt 4A
	_place_prop(_props_root, "kitchen_counter_stove.glb", Vector3(-4.2, 0.0, 5.75), 0.0, Vector3(1.66, 0.90, 0.64))
	_place_prop(_props_root, "refrigerator_retro.glb", Vector3(-2.7, 0.0, 5.78), 0.0, Vector3(0.78, 1.75, 0.72))
	_place_prop(_props_root, "dining_table.glb", Vector3(-8.4, 0.0, 6.8), 90.0, Vector3(0.84, 0.76, 1.36))
	_place_prop(_props_root, "wooden_chair.glb", Vector3(-7.5, 0.0, 6.8), -90.0, Vector3(0.46, 0.88, 0.46))
	_place_prop(_props_root, "bookshelf_tall.glb", Vector3(-3.2, 0.0, 14.12), 180.0, Vector3(1.1, 2.0, 0.38))


func _build_apartment_4b_and_crawlspace() -> void:
	var m_wood := MaterialLibrary.get_pbr("floor_wood_parquet", 0.45)
	var m_wall := MaterialLibrary.get_pbr("wall_plaster_aged", 0.42)
	var m_conc := MaterialLibrary.get_pbr("wall_concrete_brutalist", 0.42)
	var m_ceil := MaterialLibrary.get_pbr("ceiling_acoustic", 0.45)

	_add_solid_box(_geo_root, Vector3(8.9, 0.2, 9.3), Vector3(6.05, -0.1, 9.85), m_wood, "wood")
	_add_solid_box(_geo_root, Vector3(8.9, 0.2, 9.3), Vector3(6.05, 3.3, 9.85), m_ceil, "concrete")
	_add_wall_x(_geo_root, 1.6, 10.5, 5.2, 0.0, 3.2, m_wall)
	_add_wall_x(_geo_root, 1.6, 10.5, 14.5, 0.0, 3.2, m_wall)
	# East wall of 4B has Hidden Room 2 (Surveillance Crawlspace) doorway at Z = +7.2!
	_add_wall_z(_geo_root, 10.5, 5.2, 14.5, 0.0, 3.2, m_wall, [7.2])

	# Hidden Room 2: Surveillance Crawlspace (X: 10.5..13.6, Z: 5.5..10.2)
	_add_solid_box(_geo_root, Vector3(3.1, 0.2, 4.7), Vector3(12.05, -0.1, 7.85), m_conc, "concrete")
	_add_solid_box(_geo_root, Vector3(3.1, 0.2, 4.7), Vector3(12.05, 3.3, 7.85), m_conc, "concrete")
	_add_wall_x(_geo_root, 10.5, 13.6, 5.5, 0.0, 3.2, m_conc)
	_add_wall_x(_geo_root, 10.5, 13.6, 10.2, 0.0, 3.2, m_conc)
	_add_wall_z(_geo_root, 13.6, 5.5, 10.2, 0.0, 3.2, m_conc)
	var crawl_light := OmniLight3D.new()
	crawl_light.position = Vector3(12.1, 2.4, 7.8)
	crawl_light.light_color = Color(0.95, 0.26, 0.20)
	crawl_light.light_energy = 1.15
	crawl_light.omni_range = 5.5
	_props_root.add_child(crawl_light)
	_place_prop(_props_root, "shelving_metal_boxes.glb", Vector3(12.9, 0.0, 7.8), -90.0, Vector3(0.5, 1.9, 1.25))
	_place_plaque(_props_root, "OBSERVATION GALLERY 4B\n[ACOUSTIC SURVEILLANCE POST]", Vector3(13.42, 1.75, 7.8), -90.0, Color(0.95, 0.36, 0.30))

	_place_door("apt_4b", "Apartment 4B", Vector3(1.6, 0.0, 10.53), 90.0, "door_wood_panel.glb", 92.0, false)
	var crawl_door := _place_door("crawlspace_4b", "Padlocked Service Panel", Vector3(10.5, 0.0, 7.73), 90.0, "door_steel_utility.glb", 82.0, true, "Padlocked utility hatch.")
	crawl_door.anomaly_tag_id = 25

	_add_light_fixture(_props_root, Vector3(6.0, 3.18, 9.8), Color(1.0, 0.87, 0.66), 1.40, 11.0, false, false)

	# Apt 4B Furnishings + Tape #01 + Coat Rack (Anomaly #10) + Window (Anomaly #28) + Misplaced Washer (Anomaly #8)
	_place_prop(_props_root, "bed_apartment.glb", Vector3(7.8, 0.0, 13.1), 180.0, Vector3(1.48, 0.85, 2.04))
	apt4b_wardrobe = _place_prop(_props_root, "wardrobe_cabinet.glb", Vector3(4.5, 0.0, 14.0), 180.0, Vector3(1.18, 2.0, 0.60))
	_place_prop(_props_root, "desk_study.glb", Vector3(5.2, 0.0, 5.85), 0.0, Vector3(1.36, 0.76, 0.68))
	_place_prop(_props_root, "wooden_chair.glb", Vector3(5.2, 0.0, 6.75), 0.0, Vector3(0.46, 0.88, 0.46))
	_place_tape_recorder(_props_root, "tape_01", Vector3(5.0, 0.76, 5.82), 180.0)
	_place_prop(_props_root, "lamp_desk.glb", Vector3(5.65, 0.76, 5.80), -30.0)

	apt4b_coat_normal = _place_prop(_props_root, "coat_rack_normal.glb", Vector3(3.1, 0.0, 7.2), 0.0, Vector3(0.45, 1.80, 0.45))
	apt4b_coat_twisted = _place_prop(_props_root, "coat_rack_twisted_anomaly.glb", Vector3(3.1, -25.0, 7.2), 0.0, Vector3(0.55, 1.85, 0.55))
	apt4b_coat_twisted.visible = false

	apt4b_washer_anomaly = _place_prop(_props_root, "washing_machine.glb", Vector3(5.8, -25.0, 10.2), 25.0, Vector3(0.72, 0.94, 0.72))
	apt4b_washer_anomaly.visible = false

	apt4b_window_pane = _add_solid_box(_props_root, Vector3(0.06, 1.55, 2.10), Vector3(10.35, 1.65, 11.8), MaterialLibrary.get_image_mat("res://assets/textures/window_city_night.png", 0.85), "concrete", false)


func _build_apartment_4c_and_sanctum() -> void:
	var m_wood := MaterialLibrary.get_pbr("floor_wood_parquet", 0.45)
	var m_wall := MaterialLibrary.get_pbr("wall_plaster_aged", 0.42)
	var m_conc := MaterialLibrary.get_pbr("wall_concrete_brutalist", 0.40)
	var m_ceil := MaterialLibrary.get_pbr("ceiling_acoustic", 0.45)

	_add_solid_box(_geo_root, Vector3(11.9, 0.2, 9.3), Vector3(-7.55, -0.1, -9.85), m_wood, "wood")
	_add_solid_box(_geo_root, Vector3(11.9, 0.2, 9.3), Vector3(-7.55, 3.3, -9.85), m_ceil, "concrete")
	_add_wall_x(_geo_root, -13.5, -1.6, -14.5, 0.0, 3.2, m_wall)
	# West wall of 4C has Secret Architect's Sanctum opening at Z = -10.0!
	_add_wall_z(_geo_root, -13.5, -14.5, -5.2, 0.0, 3.2, m_wall, [-10.0])

	_place_door("apt_4c", "Apt 4C — Dr. Koslov Study", Vector3(-7.03, 0.0, -5.2), 0.0, "door_wood_panel.glb", -92.0, false)
	_add_light_fixture(_props_root, Vector3(-7.5, 3.18, -9.8), Color(0.96, 0.90, 0.76), 1.45, 12.0, true, false)

	# Modular Study Arrangement Root (supports controlled procedural layouts + Anomaly #21!)
	apt4c_modular_root = Node3D.new()
	apt4c_modular_root.name = "Apt4CModularRoot"
	apt4c_modular_root.position = Vector3(-7.2, 0.0, -9.8)
	_props_root.add_child(apt4c_modular_root)

	_place_prop(apt4c_modular_root, "desk_study.glb", Vector3(0.0, 0.0, -2.2), 0.0, Vector3(1.36, 0.76, 0.68))
	_place_prop(apt4c_modular_root, "wooden_chair.glb", Vector3(0.0, 0.0, -1.2), 0.0, Vector3(0.46, 0.88, 0.46))
	apt4c_radio = _place_prop(apt4c_modular_root, "radio_vintage.glb", Vector3(-0.42, 0.76, -2.25), 15.0)
	_place_tape_recorder(apt4c_modular_root, "tape_02", Vector3(0.32, 0.76, -2.20), -10.0)
	_place_document_pickup(apt4c_modular_root, "doc_03", Vector3(-0.05, 0.76, -2.05), 0.0)
	_place_prop(apt4c_modular_root, "filing_cabinet.glb", Vector3(3.4, 0.0, -3.8), 0.0, Vector3(0.50, 1.32, 0.64))
	_place_prop(apt4c_modular_root, "filing_cabinet.glb", Vector3(4.0, 0.0, -3.8), 0.0, Vector3(0.50, 1.32, 0.64))
	_place_prop(apt4c_modular_root, "armchair_worn.glb", Vector3(3.2, 0.0, 1.8), -135.0, Vector3(0.9, 0.86, 0.86))
	_place_prop(apt4c_modular_root, "lamp_floor.glb", Vector3(4.1, 0.0, 2.2), 0.0, Vector3(0.4, 1.62, 0.4))

	# Sliding Secret Bookshelf hiding Hidden Room 1 (Architect's Sanctum) at X = -13.5, Z = -10.0
	secret_bookshelf_pivot = Node3D.new()
	secret_bookshelf_pivot.name = "SecretBookshelfDoor"
	secret_bookshelf_pivot.position = Vector3(-13.32, 0.0, -10.0)
	_props_root.add_child(secret_bookshelf_pivot)
	_place_prop(secret_bookshelf_pivot, "bookshelf_tall.glb", Vector3.ZERO, 90.0, Vector3(1.16, 2.15, 0.42))

	var latch := WorldInteractable.new()
	latch.name = "SanctumBookshelfLatch"
	latch.position = Vector3(-13.15, 1.25, -9.15)
	latch.prompt_text = "[E] Activate Magnetic Bookshelf Release"
	latch.setup_box_collider(Vector3(0.28, 0.36, 0.28))
	latch.on_interact = func(_a): _on_sanctum_latch_pressed()
	_props_root.add_child(latch)
	_add_solid_box(latch, Vector3(0.08, 0.18, 0.12), Vector3.ZERO, MaterialLibrary.get_emissive(Color(0.6, 0.45, 0.15), Color(0.95, 0.72, 0.25), 1.4), "metal", false)

	# Hidden Room 1: The Architect's Sanctum (X: -18.2..-13.5, Z: -13.2..-6.8)
	_add_solid_box(_geo_root, Vector3(4.7, 0.2, 6.4), Vector3(-15.85, -0.1, -10.0), m_conc, "concrete")
	_add_solid_box(_geo_root, Vector3(4.7, 0.2, 6.4), Vector3(-15.85, 3.3, -10.0), m_conc, "concrete")
	_add_wall_x(_geo_root, -18.2, -13.5, -13.2, 0.0, 3.2, m_conc)
	_add_wall_x(_geo_root, -18.2, -13.5, -6.8, 0.0, 3.2, m_conc)
	_add_wall_z(_geo_root, -18.2, -13.2, -6.8, 0.0, 3.2, m_conc)

	var sanctum_light := OmniLight3D.new()
	sanctum_light.position = Vector3(-15.9, 2.65, -10.0)
	sanctum_light.light_color = Color(0.95, 0.76, 0.46)
	sanctum_light.light_energy = 1.45
	sanctum_light.omni_range = 7.5
	_props_root.add_child(sanctum_light)

	_place_prop(_props_root, "desk_study.glb", Vector3(-16.8, 0.0, -10.0), 90.0, Vector3(0.68, 0.76, 1.36))
	_place_document_pickup(_props_root, "doc_08", Vector3(-16.75, 0.76, -9.85), 90.0)
	_place_tape_recorder(_props_root, "tape_04", Vector3(-16.75, 0.76, -10.35), 90.0)

	# 1994 Master Override Key Pickup (for Ending B!)
	master_key_prop = Node3D.new()
	master_key_prop.position = Vector3(-16.70, 0.79, -9.45)
	_props_root.add_child(master_key_prop)
	_add_solid_box(master_key_prop, Vector3(0.18, 0.04, 0.10), Vector3.ZERO, MaterialLibrary.get_emissive(Color(0.85, 0.68, 0.22), Color(1.0, 0.80, 0.28), 2.2), "metal", false)
	var key_int := WorldInteractable.new()
	key_int.prompt_text = "[E] Take 1994 Master Containment Override Key"
	key_int.setup_box_collider(Vector3(0.32, 0.24, 0.32))
	key_int.on_interact = func(_a):
		if not GameState.master_override_key_taken:
			GameState.master_override_key_taken = true
			master_key_prop.visible = false
			AudioDirector.play_2d("scanner_confirm", -2.0, 1.25)
			GameState.notification_posted.emit("ACQUIRED: 1994 Master Override Key (Use at Basement Boiler Core)", Color(0.98, 0.82, 0.34))
			GameState.state_updated.emit()
	master_key_prop.add_child(key_int)

	# Threshold Mirror inside Sanctum (Reachable path for Ending C: Tenant 405 Surrender!)
	var mirror_frame := _place_prop(_props_root, "painting_framed.glb", Vector3(-15.8, 1.55, -13.04), 0.0)
	var m_surf := mirror_frame.find_child("CanvasSurface", true, false) as MeshInstance3D
	if m_surf:
		m_surf.material_override = MaterialLibrary.get_void_window_mat()
	_place_plaque(_props_root, "THE THRESHOLD MIRROR // TENANT 405", Vector3(-15.8, 2.15, -12.98), 0.0, Color(0.95, 0.38, 0.42))
	var mirror_int := WorldInteractable.new()
	mirror_int.position = Vector3(-15.8, 1.55, -12.95)
	mirror_int.prompt_text = "[E] Gaze Into The Threshold Mirror (Surrender Identity — Ending C)"
	mirror_int.setup_box_collider(Vector3(1.1, 1.0, 0.26))
	mirror_int.on_interact = func(_a):
		AudioDirector.play_2d("entity_observer_sting", 0.0, 0.88)
		GameState.trigger_ending("C")
	_props_root.add_child(mirror_int)


func _build_laundry_room() -> void:
	var m_tile := MaterialLibrary.get_pbr("floor_tile_linoleum", 0.48)
	var m_wall := MaterialLibrary.get_pbr("wall_plaster_aged", 0.42)
	var m_ceil := MaterialLibrary.get_pbr("ceiling_acoustic", 0.45)

	_add_solid_box(_geo_root, Vector3(9.0, 0.2, 6.8), Vector3(10.7, -0.1, 1.4), m_tile, "tile")
	_add_solid_box(_geo_root, Vector3(9.0, 0.2, 6.8), Vector3(10.7, 3.3, 1.4), m_ceil, "concrete")
	_add_wall_x(_geo_root, 6.2, 15.2, 4.8, 0.0, 3.2, m_wall)
	_add_wall_z(_geo_root, 6.2, -2.0, 4.8, 0.0, 3.2, m_wall)
	_add_wall_z(_geo_root, 15.2, -2.0, 4.8, 0.0, 3.2, m_wall)

	_place_door("laundry", "Laundry Room", Vector3(9.67, 0.0, -2.0), 0.0, "door_wood_panel.glb", 92.0, false)
	_add_light_fixture(_props_root, Vector3(10.7, 3.18, 1.4), Color(0.92, 0.96, 0.92), 1.45, 11.0, false, false)

	for i in range(4):
		_place_prop(_props_root, "washing_machine.glb", Vector3(7.6 + i * 1.15, 0.0, 4.15), 180.0, Vector3(0.70, 0.92, 0.70))
	_place_prop(_props_root, "dining_table.glb", Vector3(13.4, 0.0, 1.6), 90.0, Vector3(0.84, 0.76, 1.36))
	_place_document_pickup(_props_root, "doc_04", Vector3(13.35, 0.76, 1.6), -90.0)
	_place_prop(_props_root, "shelving_metal_boxes.glb", Vector3(6.8, 0.0, 1.5), 90.0, Vector3(0.52, 1.92, 1.25))
	_place_prop(_props_root, "pipe_assembly.glb", Vector3(10.5, 2.65, 4.62), 0.0)


func _build_storage_wing() -> void:
	var m_conc := MaterialLibrary.get_pbr("wall_concrete_brutalist", 0.42)
	var m_ceil := MaterialLibrary.get_pbr("ceiling_acoustic", 0.45)

	_add_solid_box(_geo_root, Vector3(11.9, 0.2, 9.3), Vector3(7.55, -0.1, -9.85), m_conc, "concrete")
	_add_solid_box(_geo_root, Vector3(11.9, 0.2, 9.3), Vector3(7.55, 3.3, -9.85), m_ceil, "concrete")
	_add_wall_x(_geo_root, 1.6, 13.5, -14.5, 0.0, 3.2, m_conc)
	_add_wall_z(_geo_root, 13.5, -14.5, -5.2, 0.0, 3.2, m_conc)
	_add_wall_z(_geo_root, 7.5, -14.5, -5.2, 0.0, 3.2, m_conc) # Partition between Storage A & B

	_place_door("storage_a", "Archive Storage 4-A", Vector3(3.97, 0.0, -5.2), 0.0, "door_steel_utility.glb", -90.0, false)
	_place_door("storage_b", "Sealed Vault Storage 4-B", Vector3(9.97, 0.0, -5.2), 0.0, "door_steel_utility.glb", -90.0, true, "Heavy municipal padlock seals Storage 4-B.")

	_add_light_fixture(_props_root, Vector3(4.5, 3.18, -9.8), Color(1.0, 0.88, 0.68), 1.25, 9.5, false, false)
	_add_light_fixture(_props_root, Vector3(10.5, 3.18, -9.8), Color(0.95, 0.82, 0.62), 0.85, 8.0, false, false)

	_place_prop(_props_root, "shelving_metal_boxes.glb", Vector3(4.5, 0.0, -13.7), 0.0, Vector3(1.25, 1.92, 0.52))
	_place_prop(_props_root, "shelving_metal_boxes.glb", Vector3(2.4, 0.0, -9.8), 90.0, Vector3(0.52, 1.92, 1.25))
	_place_prop(_props_root, "desk_study.glb", Vector3(6.2, 0.0, -11.5), -90.0, Vector3(0.68, 0.76, 1.36))
	_place_document_pickup(_props_root, "doc_06", Vector3(6.15, 0.76, -11.5), -90.0)


func _build_maintenance_room() -> void:
	var m_conc := MaterialLibrary.get_pbr("wall_concrete_brutalist", 0.42)
	var m_tile := MaterialLibrary.get_pbr("floor_tile_linoleum", 0.48)

	_add_solid_box(_geo_root, Vector3(5.3, 0.2, 8.0), Vector3(18.15, -0.1, -3.5), m_tile, "concrete")
	_add_solid_box(_geo_root, Vector3(5.3, 0.2, 8.0), Vector3(18.15, 3.3, -3.5), m_conc, "concrete")
	_add_wall_x(_geo_root, 15.5, 20.8, -7.5, 0.0, 3.2, m_conc)
	_add_wall_x(_geo_root, 15.5, 20.8, 0.5, 0.0, 3.2, m_conc)
	_add_wall_z(_geo_root, 20.8, -7.5, 0.5, 0.0, 3.2, m_conc)
	_add_wall_z(_geo_root, 15.5, -7.5, -5.2, 0.0, 3.2, m_conc)
	_add_wall_z(_geo_root, 15.5, -2.0, 0.5, 0.0, 3.2, m_conc)

	_place_door("maintenance", "Maintenance & Breakers", Vector3(15.5, 0.0, -3.07), 90.0, "door_steel_utility.glb", 90.0, false)
	_add_light_fixture(_props_root, Vector3(18.1, 3.18, -3.5), Color(0.95, 0.92, 0.78), 1.40, 10.0, false, false)

	_place_prop(_props_root, "pipe_assembly.glb", Vector3(18.2, 2.1, -7.28), 0.0)
	_place_prop(_props_root, "desk_study.glb", Vector3(18.2, 0.0, -0.25), 180.0, Vector3(1.36, 0.76, 0.68))
	_place_document_pickup(_props_root, "doc_05", Vector3(18.0, 0.76, -0.30), 180.0)
	_place_tape_recorder(_props_root, "tape_03", Vector3(18.5, 0.76, -0.28), 180.0)

	# Interactive Main Electrical Breaker Panel on East Wall (X = 20.66, Z = -3.5)
	var panel_vis := _place_prop(_props_root, "electrical_panel.glb", Vector3(20.68, 1.50, -3.5), -90.0)
	var breaker := WorldInteractable.new()
	breaker.name = "MainBreakerSwitch"
	breaker.position = Vector3(20.52, 1.50, -3.5)
	breaker.prompt_text = "[E] Throw Main Breaker Relay (Feeds Apt 4C Secret Latch)"
	breaker.setup_box_collider(Vector3(0.32, 0.85, 0.62))
	breaker.on_interact = func(_a):
		if not GameState.breaker_restored:
			GameState.breaker_restored = true
			AudioDirector.play_3d("door_close_latch", breaker.global_position, 14.0, 1.0, 0.8)
			GameState.notification_posted.emit("MAINTENANCE RELAY ONLINE: Magnetic Latch in Apt 4C Powered", Color(0.35, 0.95, 0.58))
			GameState.state_updated.emit()
		else:
			GameState.notification_posted.emit("Main Breaker Relay is already energized.", Color(0.75, 0.85, 0.78))
	_props_root.add_child(breaker)
	panel_vis.set_meta("breaker_ref", breaker)


func _build_stairwell_and_basement() -> void:
	var m_conc := MaterialLibrary.get_pbr("wall_concrete_brutalist", 0.40)
	var m_iron := MaterialLibrary.get_pbr("metal_rusted_iron", 0.45, 0.65)
	var m_steel := MaterialLibrary.get_pbr("metal_brushed_steel", 0.45, 0.80)

	# Stairwell Shaft enclosing X: -10.8..-4.8, Z: -2.0..+4.6 from Y = -4.2 up to Y = +3.2
	_add_solid_box(_geo_root, Vector3(6.0, 0.2, 6.6), Vector3(-7.8, 3.3, 1.3), m_conc, "concrete")
	_add_wall_z(_geo_root, -10.8, -2.0, 4.6, -4.2, 7.4, m_conc)
	_add_wall_x(_geo_root, -10.8, -4.8, 4.6, -4.2, 7.4, m_conc)
	# Upper Floor 4 Landing (X: -10.8..-4.8, Z: -2.0..+0.4 at Y=0)
	_add_solid_box(_geo_root, Vector3(6.0, 0.2, 2.4), Vector3(-7.8, -0.1, -0.8), m_conc, "metal")
	_add_wall_z(_geo_root, -4.8, -2.0, 4.6, 0.0, 3.2, m_conc)

	_place_door("stairwell_4f", "Fire Stairwell (4F <-> Basement)", Vector3(-8.33, 0.0, -2.0), 0.0, "door_steel_utility.glb", 90.0, false)
	_place_plaque(_props_root, "STAIRWELL // 4F -> B1", Vector3(-7.8, 2.48, -1.85), 0.0)

	# Smooth navigable concrete stair steps descending along X=-10.6..-5.2, Z=+0.4..+4.4 down to Y=-4.2?
	# Even cleaner: descend Eastward from West Mid-Landing so player walks South (Z=-0.8->+2.8) down Flight 1 to Mid-Landing (Y=-2.1), then East into Basement Corridor (Y=-4.2)!
	# Flight 1 (West side X: -10.5..-8.0, Z: 0.3..3.9, 10 wide steps from Y=-0.21 down to Y=-2.10)
	for s in range(10):
		var sy := -0.21 * float(s + 1)
		var sz := 0.48 + float(s) * 0.34
		_add_solid_box(_geo_root, Vector3(2.4, 0.24, 0.38), Vector3(-9.25, sy - 0.10, sz), m_conc, "metal")
	# Mid-Landing at Y = -2.10 (X: -10.6..-5.0, Z: 3.6..4.55)
	_add_solid_box(_geo_root, Vector3(5.6, 0.24, 1.25), Vector3(-7.8, -2.22, 4.0), m_conc, "metal")
	# Flight 2 (East side X: -7.6..-5.1, Z: 3.4..0.0, 10 wide steps from Y=-2.31 down to Y=-4.20!)
	for s in range(10):
		var sy := -2.10 - 0.21 * float(s + 1)
		var sz := 3.25 - float(s) * 0.34
		_add_solid_box(_geo_root, Vector3(2.4, 0.24, 0.38), Vector3(-6.35, sy - 0.10, sz), m_conc, "metal")
	# Center handrail divider wall so player never falls between flights
	_add_solid_box(_geo_root, Vector3(0.22, 4.8, 3.2), Vector3(-7.8, -1.8, 1.85), m_steel, "metal")

	_add_light_fixture(_props_root, Vector3(-7.8, 3.12, 1.6), Color(0.95, 0.86, 0.66), 1.45, 11.0, false, false)

	# ============================================================
	# BASEMENT CORRIDOR & BOILER CORE (Y = -4.20 to Y = -1.00)
	# ============================================================
	# Basement Main Hall (X: -10.8..+6.5, Z: -2.5..+2.8, Y = -4.2)
	_add_solid_box(_geo_root, Vector3(17.3, 0.24, 5.4), Vector3(-2.15, -4.32, 0.2), m_conc, "concrete")
	_add_solid_box(_geo_root, Vector3(10.0, 0.24, 5.4), Vector3(1.5, -0.92, 0.2), m_conc, "concrete")
	_add_wall_x(_geo_root, -4.8, 4.2, 2.8, -4.2, 3.3, m_conc)
	# North wall of Basement Corridor has Boiler Room doorway at X = +2.0!
	_add_wall_x(_geo_root, -10.8, 4.2, -2.5, -4.2, 3.3, m_conc, [2.0])
	_add_wall_x(_geo_root, -10.8, -4.8, -2.0, -4.2, 4.0, m_conc)

	_add_light_fixture(_props_root, Vector3(-2.5, -1.08, 0.2), Color(0.96, 0.84, 0.62), 1.35, 11.0, false, false)
	_add_light_fixture(_props_root, Vector3(2.8, -1.08, 0.6), Color(0.96, 0.84, 0.62), 1.35, 10.0, false, false)
	_place_prop(_props_root, "pipe_assembly.glb", Vector3(-1.5, -1.65, 2.58), 180.0)

	# Basement Boiler & Containment Core Room (X: -2.5..+6.5, Z: -11.5..-2.5, Y = -4.2)
	_add_solid_box(_geo_root, Vector3(9.0, 0.24, 9.0), Vector3(2.0, -4.32, -7.0), m_conc, "metal")
	_add_solid_box(_geo_root, Vector3(9.0, 0.24, 9.0), Vector3(2.0, -0.92, -7.0), m_conc, "concrete")
	_add_wall_x(_geo_root, -2.5, 6.5, -11.5, -4.2, 3.3, m_conc)
	_add_wall_z(_geo_root, -2.5, -11.5, -2.5, -4.2, 3.3, m_conc)
	_add_wall_z(_geo_root, 6.5, -11.5, -2.5, -4.2, 3.3, m_conc)
	_add_wall_x(_geo_root, 4.2, 6.5, -2.5, -4.2, 3.3, m_conc)

	_place_door("boiler_room", "Sub-Basement Boiler & Core", Vector3(1.47, -4.2, -2.5), 0.0, "door_steel_utility.glb", -90.0, false)
	_add_light_fixture(_props_root, Vector3(2.0, -1.08, -7.0), Color(1.0, 0.72, 0.45), 1.55, 12.0, true, false)

	var boiler := _place_prop(_props_root, "boiler_industrial.glb", Vector3(2.0, -4.2, -9.2), 0.0, Vector3(2.8, 1.9, 1.45))
	AudioDirector.make_looping_3d("boiler_rumble", boiler, Vector3(0, 1.0, 0), 14.0, -6.0).play()
	_place_prop(_props_root, "pipe_assembly.glb", Vector3(-2.28, -2.2, -7.0), 90.0)
	_place_prop(_props_root, "desk_study.glb", Vector3(-1.2, -4.2, -4.6), 90.0, Vector3(0.68, 0.76, 1.36))
	_place_document_pickup(_props_root, "doc_07", Vector3(-1.15, -3.44, -4.6), 90.0)

	# 1994 Resonance Containment Core Console (Insert Master Key -> Unlocks True Ending B!)
	boiler_core_console = WorldInteractable.new()
	boiler_core_console.name = "BoilerResonanceCore"
	boiler_core_console.position = Vector3(5.6, -3.05, -7.0)
	boiler_core_console.prompt_text = "[E] Resonance Containment Core (Requires 1994 Master Override Key)"
	boiler_core_console.setup_box_collider(Vector3(0.65, 1.5, 0.85))
	boiler_core_console.on_interact = func(_a): _on_boiler_core_interacted()
	_props_root.add_child(boiler_core_console)
	_add_solid_box(boiler_core_console, Vector3(0.45, 1.35, 0.72), Vector3.ZERO, m_iron, "metal", false)
	_add_solid_box(boiler_core_console, Vector3(0.48, 0.28, 0.48), Vector3(-0.04, 0.35, 0), MaterialLibrary.get_emissive(Color(0.2, 0.6, 0.9), Color(0.25, 0.75, 1.0), 2.0), "metal", false)
	_place_plaque(boiler_core_console, "1994 CONTAINMENT CORE\n[MASTER KEY OVERRIDE]", Vector3(-0.26, 0.82, 0), -90.0, Color(0.45, 0.88, 1.0))


func _build_elevator_shaft_and_system() -> void:
	var m_conc := MaterialLibrary.get_pbr("wall_concrete_brutalist", 0.42)
	var m_tile := MaterialLibrary.get_pbr("floor_tile_linoleum", 0.48)

	# Floor 4 Elevator Lobby Alcove (X: 1.6..4.2, Z: -2.0..+2.4) & Basement Lobby (X: 4.2..6.6, Z: -2.0..+2.4)
	_add_solid_box(_geo_root, Vector3(2.8, 0.2, 4.4), Vector3(2.9, -0.1, 0.2), m_tile, "tile")
	_add_solid_box(_geo_root, Vector3(2.8, 0.2, 4.4), Vector3(2.9, 3.3, 0.2), m_conc, "concrete")
	_add_wall_x(_geo_root, 1.6, 6.6, 2.4, -4.2, 7.4, m_conc)

	# Shaft enclosure around X=5.3, Z=0.8 (Cab spans X: 4.2..6.4, Z: -0.3..+1.9)
	_add_wall_z(_geo_root, 6.5, -2.0, 2.4, -4.2, 7.4, m_conc)
	# Front shaft wall at X=4.15 with cab doors at Z=+0.8 on Floor 4 (Y=0) and Basement (Y=-4.2)
	_add_wall_z(_geo_root, 4.15, -0.6, 2.4, 0.0, 3.2, m_conc, [0.8])
	_add_wall_z(_geo_root, 4.15, -2.5, 2.8, -4.2, 3.3, m_conc, [0.8])

	elevator = ElevatorSystem.new()
	elevator.name = "ElevatorSystem"
	add_child(elevator)
	elevator.build_elevator(Vector2(5.3, 0.8))


# ============================================================
# STORY & MODULAR ENVIRONMENT INTERACTIONS
# ============================================================
func _on_sanctum_latch_pressed() -> void:
	if not GameState.breaker_restored:
		AudioDirector.play_2d("scanner_error", -4.0)
		GameState.notification_posted.emit("Magnetic Latch Dead — Restore Power in Maintenance Room first.", Color(0.92, 0.45, 0.32))
		return
	if not GameState.sanctum_unlocked:
		GameState.sanctum_unlocked = true
		AudioDirector.play_3d("door_open_creak", secret_bookshelf_pivot.global_position, 16.0, 0.0, 0.72)
		var tw := create_tween().set_trans(Tween.TRANS_CUBIC).set_ease(Tween.EASE_IN_OUT)
		tw.tween_property(secret_bookshelf_pivot, "position:z", -11.35, 1.2)
		GameState.notification_posted.emit("HIDDEN ROOM REVEALED: The Architect's Sanctum", Color(0.98, 0.82, 0.38))
		GameState.state_updated.emit()


func _on_boiler_core_interacted() -> void:
	if GameState.core_purged:
		GameState.notification_posted.emit("Containment Core already purged! Reach the North Emergency Exit!", Color(0.35, 0.95, 0.58))
		return
	if not GameState.master_override_key_taken:
		AudioDirector.play_2d("scanner_error", -4.0)
		GameState.notification_posted.emit("LOCKED: Requires 1994 Master Override Key from the Hidden Sanctum.", Color(0.92, 0.45, 0.32))
		return

	GameState.core_purged = true
	north_exit_door.is_locked = false
	AudioDirector.play_3d("door_slam_heavy", boiler_core_console.global_position, 24.0, 2.0, 0.75)
	SoundEventBus.emit_noise(boiler_core_console.global_position, 92.0, "alarm")
	GameState.notification_posted.emit("RESONANCE CORE PURGED! Emergency Exit Unlocked — DO NOT LOOK BACK!", Color(0.35, 0.98, 0.62))
	GameState.state_updated.emit()


func _on_emergency_exit_interacted() -> void:
	if GameState.core_purged:
		AudioDirector.play_2d("door_open_creak", -1.0)
		emergency_exit_used.emit()
		GameState.trigger_ending("B")
		return
	if GameState.verified_cycles >= GameState.TARGET_CLEARANCE_CYCLES:
		AudioDirector.play_2d("door_open_creak", -1.0)
		emergency_exit_used.emit()
		GameState.trigger_ending("A")
		return
	AudioDirector.play_3d("door_locked_rattle", north_exit_door.global_position + Vector3(0.5, 1.0, 0), 12.0, -1.0)
	var rem := GameState.TARGET_CLEARANCE_CYCLES - GameState.verified_cycles
	GameState.notification_posted.emit("EXIT MAG-LOCK ACTIVE: %d more Verified Cycles required (or Purge Basement Core)." % rem, Color(0.95, 0.52, 0.32))


func update_progress_board(has_baseline_hint: bool = false) -> void:
	if progress_board_label == null:
		return
	if GameState.core_purged:
		progress_board_label.text = "CONTAINMENT PURGE ACTIVE\nPROCEED TO NORTH EMERGENCY EXIT"
		progress_board_label.modulate = Color(0.35, 0.98, 0.62)
	elif GameState.verified_cycles >= GameState.TARGET_CLEARANCE_CYCLES:
		progress_board_label.text = "CLEARANCE 8 / 8 GRANTED\nNORTH EMERGENCY EXIT UNLOCKED"
		progress_board_label.modulate = Color(0.35, 0.98, 0.55)
		north_exit_door.is_locked = false
	elif has_baseline_hint:
		progress_board_label.text = "CYCLE 0 / 8 — BASELINE CALIBRATION\nCOMPLEX IS CLEAN -> PULL GREEN LEVER"
		progress_board_label.modulate = Color(0.45, 0.92, 0.65)
	else:
		progress_board_label.text = "VERIFIED CLEARANCE: %d / %d\nINSPECT ALL WINGS BEFORE REPORTING" % [GameState.verified_cycles, GameState.TARGET_CLEARANCE_CYCLES]
		progress_board_label.modulate = Color(0.96, 0.78, 0.36)


# ============================================================
# CONTROLLED MODULAR PROCEDURAL ARRANGEMENTS & VALIDATION (Sec 9)
# ============================================================
func set_corridor_extended(extended: bool) -> void:
	current_corridor_extended = extended
	if extended:
		north_std_cap.visible = false
		north_std_cap.position.y = -30.0
		north_ext_module.visible = true
		north_ext_module.position.y = 0.0
		north_exit_door.position.z = -29.0
	else:
		north_std_cap.visible = true
		north_std_cap.position.y = 0.0
		north_ext_module.visible = false
		north_ext_module.position.y = -30.0
		north_exit_door.position.z = -17.0


func set_apt4c_arrangement(variant: int) -> void:
	current_apt4c_variant = variant
	match variant:
		0: # Baseline
			apt4c_modular_root.scale = Vector3.ONE
			apt4c_modular_root.rotation.y = 0.0
		1: # Mirrored Anomaly #21 Layout
			apt4c_modular_root.scale = Vector3(-1.0, 1.0, 1.0)
			apt4c_modular_root.rotation.y = deg_to_rad(180.0)
		2: # Controlled subtle 90-deg research rotation
			apt4c_modular_root.scale = Vector3.ONE
			apt4c_modular_root.rotation.y = deg_to_rad(90.0)


func _build_navigation_system() -> void:
	# Explicit validated architectural graph + NavigationRegion3D polygon mesh
	nav_waypoints = {
		"hub": {"pos": Vector3(0.0, 0.0, 17.5), "neighbors": ["spine_s"]},
		"spine_s": {"pos": Vector3(0.0, 0.0, 10.0), "neighbors": ["hub", "apt_4a", "apt_4b", "spine_m"]},
		"apt_4a": {"pos": Vector3(-5.0, 0.0, 10.0), "neighbors": ["spine_s"]},
		"apt_4b": {"pos": Vector3(5.0, 0.0, 9.5), "neighbors": ["spine_s"]},
		"spine_m": {"pos": Vector3(0.0, 0.0, 1.0), "neighbors": ["spine_s", "cross_c", "elev_4f"]},
		"elev_4f": {"pos": Vector3(2.8, 0.0, 0.8), "neighbors": ["spine_m", "cross_e1"]},
		"cross_c": {"pos": Vector3(0.0, 0.0, -3.6), "neighbors": ["spine_m", "cross_w", "cross_e1", "spine_n1"]},
		"cross_w": {"pos": Vector3(-7.0, 0.0, -3.6), "neighbors": ["cross_c", "apt_4c", "stair_4f"]},
		"apt_4c": {"pos": Vector3(-7.0, 0.0, -8.5), "neighbors": ["cross_w"]},
		"stair_4f": {"pos": Vector3(-7.8, 0.0, -0.8), "neighbors": ["cross_w", "base_w"]},
		"cross_e1": {"pos": Vector3(5.0, 0.0, -3.6), "neighbors": ["cross_c", "elev_4f", "storage_a", "cross_e2"]},
		"storage_a": {"pos": Vector3(4.5, 0.0, -8.5), "neighbors": ["cross_e1"]},
		"cross_e2": {"pos": Vector3(10.2, 0.0, -3.6), "neighbors": ["cross_e1", "laundry", "maint"]},
		"laundry": {"pos": Vector3(10.2, 0.0, 1.2), "neighbors": ["cross_e2"]},
		"maint": {"pos": Vector3(17.5, 0.0, -3.5), "neighbors": ["cross_e2"]},
		"spine_n1": {"pos": Vector3(0.0, 0.0, -10.5), "neighbors": ["cross_c", "spine_n2"]},
		"spine_n2": {"pos": Vector3(0.0, 0.0, -15.5), "neighbors": ["spine_n1"]},
		"base_w": {"pos": Vector3(-5.0, -4.2, 0.2), "neighbors": ["stair_4f", "base_e"]},
		"base_e": {"pos": Vector3(2.0, -4.2, 0.2), "neighbors": ["base_w", "boiler"]},
		"boiler": {"pos": Vector3(2.0, -4.2, -5.5), "neighbors": ["base_e"]},
	}

	nav_region = NavigationRegion3D.new()
	nav_region.name = "ComplexNavigationRegion"
	var nmesh := NavigationMesh.new()
	# Build clean walkable corridor & room floor triangles on Y=0.05
	var verts := PackedVector3Array([
		Vector3(-1.4, 0.05, 20.5), Vector3(1.4, 0.05, 20.5), Vector3(1.4, 0.05, -16.5), Vector3(-1.4, 0.05, -16.5),
		Vector3(-11.0, 0.05, -2.2), Vector3(15.0, 0.05, -2.2), Vector3(15.0, 0.05, -5.0), Vector3(-11.0, 0.05, -5.0),
	])
	nmesh.set_vertices(verts)
	nmesh.add_polygon(PackedInt32Array([0, 1, 2, 3]))
	nmesh.add_polygon(PackedInt32Array([4, 5, 6, 7]))
	nav_region.navigation_mesh = nmesh
	add_child(nav_region)


func find_nav_path(from_pos: Vector3, to_pos: Vector3) -> Array[Vector3]:
	# Guaranteed multi-floor shortest-path BFS across validated Complex Waypoint Graph
	var start_id := _nearest_waypoint(from_pos)
	var goal_id := _nearest_waypoint(to_pos)
	if start_id == "" or goal_id == "":
		return [to_pos]
	if start_id == goal_id:
		return [to_pos]

	var queue: Array[String] = [start_id]
	var came_from: Dictionary = {start_id: ""}
	while not queue.is_empty():
		var cur: String = queue.pop_front()
		if cur == goal_id:
			break
		for nb in nav_waypoints[cur]["neighbors"]:
			# Respect Anomaly #24 East Cross Barricade so AI routes intelligently!
			if cross_barricade_root.visible and ((cur == "cross_e2" and nb == "maint") or (cur == "maint" and nb == "cross_e2")):
				continue
			if not came_from.has(nb):
				came_from[nb] = cur
				queue.append(nb)

	if not came_from.has(goal_id):
		return [nav_waypoints[start_id]["pos"]]

	var out: Array[Vector3] = [to_pos]
	var step := goal_id
	while step != "" and step != start_id:
		out.push_front(nav_waypoints[step]["pos"])
		step = came_from[step]
	return out


func _nearest_waypoint(pos: Vector3) -> String:
	var best_id := ""
	var best_d := 1e9
	for k in nav_waypoints.keys():
		var wp: Vector3 = nav_waypoints[k]["pos"]
		# Penalize floor mismatch heavily so entities don't pick a node through a floor slab
		var dy := absf(wp.y - pos.y) * 5.0
		var d := wp.distance_to(pos) + dy
		if d < best_d:
			best_d = d
			best_id = k
	return best_id


func validate_environment_integrity(player_pos: Vector3) -> Dictionary:
	# Validates connectivity of all required progression rooms, door states, and safe player bounds
	var visited: Dictionary = {"hub": true}
	var q: Array[String] = ["hub"]
	while not q.is_empty():
		var u: String = q.pop_front()
		for v in nav_waypoints[u]["neighbors"]:
			if not visited.has(v):
				visited[v] = true
				q.append(v)

	var required_nodes := ["hub", "spine_s", "apt_4a", "apt_4b", "cross_c", "apt_4c", "laundry", "storage_a", "spine_n2", "stair_4f", "boiler"]
	var all_connected := true
	for req in required_nodes:
		if not visited.has(req):
			all_connected = false

	var player_in_bounds := (player_pos.y > -5.5 and player_pos.y < 4.5 and absf(player_pos.x) < 22.0 and absf(player_pos.z) < 32.0)
	return {
		"valid": all_connected and doors.size() >= 9 and player_in_bounds,
		"reachable_waypoints": visited.size(),
		"total_waypoints": nav_waypoints.size(),
		"door_count": doors.size(),
		"player_safe": player_in_bounds,
	}
