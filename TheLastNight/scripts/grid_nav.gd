extends RefCounted
## Grid navigation built on Godot's AStar3D. One point per walkable cell across
## the whole level (interior + parking lot / exterior); barricades dynamically
## disable door cells so zombies path around or stop to attack.
class_name GridNav

var astar := AStar3D.new()
var ids: Dictionary = {} # Vector2i -> point id
var blocked: Dictionary = {} # Vector2i -> true (walls + furniture + fence + cars)
var cell_size: float
var bounds_min := Vector2i.ZERO
var bounds_max := Vector2i.ZERO

func _init(layout: Dictionary, cell: float) -> void:
	cell_size = cell
	var data := GameData.instance()
	blocked = data.blocked_cells()
	# exterior bounds (fall back to interior if no exterior defined)
	var b: Dictionary = layout.get("exterior", layout["interior"])
	bounds_min = Vector2i(int(b["x0"]), int(b["z0"]))
	bounds_max = Vector2i(int(b["x1"]) - 1, int(b["z1"]) - 1)
	_add_fence_and_vehicles(layout)
	var pid := 0
	for cx in range(bounds_min.x, bounds_max.x + 1):
		for cz in range(bounds_min.y, bounds_max.y + 1):
			var c := Vector2i(cx, cz)
			if blocked.has(c):
				continue
			var w := data.cell_to_world(cx, cz)
			astar.add_point(pid, Vector3(w.x, 0.0, w.z))
			ids[c] = pid
			pid += 1
	for c in ids:
		var id: int = ids[c]
		for off in [Vector2i(1, 0), Vector2i(0, 1)]:
			var n: Vector2i = c + off
			if ids.has(n):
				astar.connect_points(id, ids[n], true)

func _add_fence_and_vehicles(layout: Dictionary) -> void:
	if not layout.has("exterior"):
		return
	var e: Dictionary = layout["exterior"]
	var x0 := int(e["x0"])
	var z0 := int(e["z0"])
	var x1 := int(e["x1"])
	var z1 := int(e["z1"])
	# fence entrance gaps live on the south perimeter row
	var gap_set := {}
	for g in e.get("fence_gaps", []):
		for gx in g:
			gap_set[int(gx)] = true
	for x in range(x0, x1):
		if not gap_set.has(x):
			blocked[Vector2i(x, z0)] = true
			blocked[Vector2i(x, z1 - 1)] = true
	for z in range(z0 + 1, z1 - 1):
		blocked[Vector2i(x0, z)] = true
		blocked[Vector2i(x1 - 1, z)] = true
	# vehicles are solid obstacles
	for car in e.get("cars", []):
		blocked[Vector2i(int(car["at"][0]), int(car["at"][1]))] = true
	var truck: Dictionary = e.get("truck", {})
	if not truck.is_empty():
		var tx := int(truck["at"][0])
		var tz := int(truck["at"][1])
		for c in [Vector2i(tx, tz), Vector2i(tx + 1, tz), Vector2i(tx, tz - 1), Vector2i(tx + 1, tz - 1)]:
			blocked[c] = true

func is_walkable_cell(c: Vector2i) -> bool:
	return ids.has(c) and not astar.is_point_disabled(ids[c])

func set_door_closed(cells: Array, closed: bool) -> void:
	for c in cells:
		var cell := Vector2i(int(c[0]), int(c[1]))
		if ids.has(cell):
			astar.set_point_disabled(ids[cell], closed)

func nearest_cell(world_pos: Vector3) -> Vector2i:
	var best := Vector2i.ZERO
	var best_d := INF
	var found := false
	for c in ids:
		if astar.is_point_disabled(ids[c]):
			continue
		var w: Vector3 = astar.get_point_position(ids[c])
		var d: float = w.distance_squared_to(world_pos)
		if not found or d < best_d:
			best_d = d
			best = c
			found = true
	return best

func find_path(from: Vector3, to: Vector3) -> Array[Vector3]:
	var c1 := nearest_cell(from)
	var c2 := nearest_cell(to)
	if not ids.has(c1) or not ids.has(c2):
		return []
	var raw: PackedVector3Array = astar.get_point_path(ids[c1], ids[c2])
	var out: Array[Vector3] = []
	for p in raw:
		out.append(p)
	return out

func has_path(from: Vector3, to: Vector3) -> bool:
	return find_path(from, to).size() > 0
