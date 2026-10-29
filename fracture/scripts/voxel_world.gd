extends Node3D
class_name VoxelWorld

# Half-metre cells. The dictionary is the authoritative world; render and physics
# are regenerated only for chunks touching a changed cell.
const CELL := 0.5
const CHUNK := 16
const COLORS := [Color.TRANSPARENT, Color(0.40, 0.34, 0.25), Color(0.48, 0.52, 0.50), Color(0.64, 0.32, 0.24), Color(0.60, 0.45, 0.29), Color(0.42, 0.67, 0.72), Color(0.24, 0.27, 0.27), Color(0.64, 0.62, 0.52), Color(0.29, 0.38, 0.25)]
const RESISTANCE := [0.0, 0.7, 2.0, 1.3, 0.65, 0.25, 2.8, 0.75, 0.5]
const DIRS := [Vector3i(1,0,0), Vector3i(-1,0,0), Vector3i(0,1,0), Vector3i(0,-1,0), Vector3i(0,0,1), Vector3i(0,0,-1)]
const FACE_VERTS := [
 [Vector3(1,0,0),Vector3(1,1,0),Vector3(1,1,1),Vector3(1,0,1)],
 [Vector3(0,0,1),Vector3(0,1,1),Vector3(0,1,0),Vector3(0,0,0)],
 [Vector3(0,1,0),Vector3(0,1,1),Vector3(1,1,1),Vector3(1,1,0)],
 [Vector3(0,0,1),Vector3(0,0,0),Vector3(1,0,0),Vector3(1,0,1)],
 [Vector3(1,0,1),Vector3(1,1,1),Vector3(0,1,1),Vector3(0,0,1)],
 [Vector3(0,0,0),Vector3(0,1,0),Vector3(1,1,0),Vector3(1,0,0)]
]
var voxels: Dictionary = {}
var chunk_cells: Dictionary = {}
var chunks: Dictionary = {}
var dirty: Dictionary = {}
var removed: Dictionary = {}
var material: StandardMaterial3D
var debris: Array[RigidBody3D] = []
var rebuild_count := 0

func _ready() -> void:
 material = StandardMaterial3D.new()
 material.vertex_color_use_as_albedo = true
 material.cull_mode = BaseMaterial3D.CULL_DISABLED
 material.roughness = 0.88

func chunk_of(p: Vector3i) -> Vector3i:
 return Vector3i(floori(float(p.x)/CHUNK), floori(float(p.y)/CHUNK), floori(float(p.z)/CHUNK))

func put(p: Vector3i, kind: int) -> void:
 if voxels.has(p):
  voxels[p] = kind
  return
 voxels[p] = kind
 var c := chunk_of(p)
 if not chunk_cells.has(c): chunk_cells[c] = {}
 chunk_cells[c][p] = true

func box(a: Vector3i, b: Vector3i, kind: int) -> void:
 for x in range(a.x, b.x + 1):
  for y in range(a.y, b.y + 1):
   for z in range(a.z, b.z + 1):
    put(Vector3i(x,y,z), kind)

func erase_cell(p: Vector3i, record := true) -> void:
 if not voxels.has(p): return
 voxels.erase(p)
 var c := chunk_of(p)
 chunk_cells[c].erase(p)
 dirty[c] = true
 for d in DIRS:
  var nc := chunk_of(p + d)
  if nc != c and chunk_cells.has(nc): dirty[nc] = true
 if record: removed[p] = true

func generate() -> void:
 voxels.clear(); chunk_cells.clear(); removed.clear()
 for child in chunks.values(): child.queue_free()
 chunks.clear(); dirty.clear()
 # Soil and asphalt are destructible too; beyond this finite parcel is open sky.
 box(Vector3i(-48,-2,-48), Vector3i(47,-1,47), 1)
 box(Vector3i(-48,-1,17), Vector3i(47,-1,28), 6)
 for x in range(-46, 48, 8): box(Vector3i(x,-1,22),Vector3i(x+3,-1,22),7)
 house(Vector3i(-30,0,-20), 3)
 house(Vector3i(6,0,-18), 7)
 # timber yard, fence, and a small supported canopy
 for x in range(-42, 44):
  if x < -34 or x > -2:
   put(Vector3i(x,0,12),4); put(Vector3i(x,2,12),4)
   if x % 4 == 0: put(Vector3i(x,1,12),4)
 for x in [23,35]:
  for z in [0,10]: box(Vector3i(x,0,z),Vector3i(x,7,z),4)
 box(Vector3i(22,8,-1),Vector3i(36,8,11),6)
 for x in range(25,34,3):
  box(Vector3i(x,0,3),Vector3i(x+1,2,4),4)
 for c in chunk_cells: dirty[c] = true

func house(o: Vector3i, wall: int) -> void:
 # 11 x 10m shell, front door and window openings are real openings.
 box(o + Vector3i(0,0,0), o + Vector3i(21,0,19), 4)
 for x in range(22):
  for y in range(1,8):
   for z in [0,19]:
    var door: bool = z == 19 and x >= 9 and x <= 12 and y <= 5
    var window: bool = y >= 3 and y <= 5 and ((x >= 3 and x <= 6) or (x >= 15 and x <= 18))
    if not door: put(o + Vector3i(x,y,z), 5 if window else wall)
 for z in range(1,19):
  for y in range(1,8):
   for x in [0,21]:
    var window: bool = y >= 3 and y <= 5 and z >= 7 and z <= 11
    put(o + Vector3i(x,y,z), 5 if window else wall)
 # Interior dividing wall, with doorway, and furnishings made of voxels.
 for z in range(1,19):
  for y in range(1,7):
   if not (z >= 9 and z <= 12 and y <= 5): put(o + Vector3i(11,y,z),7)
 box(o + Vector3i(0,8,0),o + Vector3i(21,8,19),4)
 box(o + Vector3i(-1,9,-1),o + Vector3i(22,9,20),6)
 box(o + Vector3i(3,1,3),o + Vector3i(7,2,5),4)
 box(o + Vector3i(15,1,3),o + Vector3i(19,2,6),4)
 box(o + Vector3i(16,3,3),o + Vector3i(18,4,3),7)

func build_all() -> void:
 while not dirty.is_empty():
  var c: Vector3i = dirty.keys()[0]
  dirty.erase(c)
  rebuild_chunk(c)

func _process(_delta: float) -> void:
 # Bound expensive mesh/collision work to one chunk per frame.
 if not dirty.is_empty():
  var c: Vector3i = dirty.keys()[0]
  dirty.erase(c)
  rebuild_chunk(c)

func rebuild_chunk(c: Vector3i) -> void:
 rebuild_count += 1
 if chunks.has(c):
  chunks[c].queue_free()
  chunks.erase(c)
 if not chunk_cells.has(c) or chunk_cells[c].is_empty(): return
 var vertices := PackedVector3Array()
 var normals := PackedVector3Array()
 var colors := PackedColorArray()
 var indices := PackedInt32Array()
 for p in chunk_cells[c]:
  var kind: int = voxels[p]
  for face in range(6):
   if voxels.has(p + DIRS[face]): continue
   var base := vertices.size()
   var shade: float = 0.92 + 0.08 * float(posmod(p.x * 13 + p.y * 7 + p.z * 17, 5)) / 4.0
   for v in FACE_VERTS[face]:
    vertices.append((Vector3(p) + v) * CELL)
    normals.append(Vector3(DIRS[face]))
    colors.append(COLORS[kind] * Color(shade, shade, shade, 1))
   indices.append_array(PackedInt32Array([base,base+1,base+2,base,base+2,base+3]))
 if vertices.is_empty(): return
 var arrays := []
 arrays.resize(Mesh.ARRAY_MAX)
 arrays[Mesh.ARRAY_VERTEX] = vertices
 arrays[Mesh.ARRAY_NORMAL] = normals
 arrays[Mesh.ARRAY_COLOR] = colors
 arrays[Mesh.ARRAY_INDEX] = indices
 var mesh := ArrayMesh.new()
 mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
 mesh.surface_set_material(0, material)
 var body := StaticBody3D.new()
 body.name = "Chunk_%s_%s_%s" % [c.x,c.y,c.z]
 var visual := MeshInstance3D.new()
 visual.mesh = mesh
 body.add_child(visual)
 var collision := CollisionShape3D.new()
 collision.shape = mesh.create_trimesh_shape()
 body.add_child(collision)
 add_child(body)
 chunks[c] = body

func damage(center: Vector3, radius: float, power: float) -> int:
 var lo := Vector3i(floori((center.x-radius)/CELL), floori((center.y-radius)/CELL), floori((center.z-radius)/CELL))
 var hi := Vector3i(floori((center.x+radius)/CELL), floori((center.y+radius)/CELL), floori((center.z+radius)/CELL))
 var hits: Array[Vector3i] = []
 var kinds: Array[int] = []
 for x in range(lo.x,hi.x+1):
  for y in range(lo.y,hi.y+1):
   for z in range(lo.z,hi.z+1):
    var p := Vector3i(x,y,z)
    if not voxels.has(p): continue
    var distance := (Vector3(p) * CELL + Vector3.ONE * CELL * 0.5).distance_to(center)
    var strength := power * (1.0 - distance / radius * 0.65)
    if distance <= radius and strength >= RESISTANCE[voxels[p]]:
     hits.append(p); kinds.append(voxels[p])
 for p in hits: erase_cell(p)
 if not hits.is_empty():
  for i in range(mini(5, hits.size())):
   if debris.size() >= 30: break
   var index := floori(float(i * hits.size()) / mini(5,hits.size()))
   spawn_debris(hits[index], kinds[index], center)
 return hits.size()

func spawn_debris(p: Vector3i, kind: int, origin: Vector3) -> void:
 var body := RigidBody3D.new()
 body.position = Vector3(p) * CELL + Vector3.ONE * CELL * 0.5
 body.collision_layer = 2
 body.collision_mask = 1
 body.mass = 0.7 if kind == 4 else 2.0
 var shape := BoxShape3D.new(); shape.size = Vector3.ONE * 0.28
 var cs := CollisionShape3D.new(); cs.shape = shape; body.add_child(cs)
 var cube := BoxMesh.new(); cube.size = shape.size
 var mat := StandardMaterial3D.new(); mat.albedo_color = COLORS[kind]; mat.roughness = 0.9
 cube.material = mat
 var visual := MeshInstance3D.new(); visual.mesh = cube; body.add_child(visual)
 add_child(body)
 body.linear_velocity = (body.position - origin).normalized() * 3.0 + Vector3.UP * 2.0
 debris.append(body)
 get_tree().create_timer(12.0).timeout.connect(func():
  debris.erase(body)
  if is_instance_valid(body): body.queue_free())

func serialize_removed() -> Array:
 var data := []
 for p in removed: data.append([p.x,p.y,p.z])
 return data

func apply_removed(data: Array) -> void:
 for item in data:
  if item is Array and item.size() == 3: erase_cell(Vector3i(int(item[0]),int(item[1]),int(item[2])))
