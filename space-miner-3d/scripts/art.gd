extends RefCounted
var alloy: StandardMaterial3D
var dark: StandardMaterial3D
var trim: StandardMaterial3D
var glass: StandardMaterial3D
var rock: StandardMaterial3D
var glow: StandardMaterial3D

func _init() -> void:
 alloy = material(Color("9fbbc4"),0.7)
 alloy.albedo_texture = load("res://assets/alloy.png")
 dark = material(Color("233747"),0.8)
 trim = material(Color("d9a15e"),0.6)
 glass = material(Color("2e829c"),0.75,Color("277f9e"))
 glow = material(Color("5ee9de"),0.1,Color("52d9dd"))
 rock = material(Color("a49b8d"),0.04)
 rock.albedo_texture = load("res://assets/rock.png")

func material(color: Color, metal: float = 0, emission: Color = Color.BLACK) -> StandardMaterial3D:
 var m = StandardMaterial3D.new()
 m.albedo_color = color
 m.metallic = metal
 m.roughness = 0.67
 if emission!=Color.BLACK:
  m.emission_enabled = true
  m.emission = emission
  m.emission_energy_multiplier = 1.5
 return m

func mesh(parent: Node3D, shape: Mesh, pos: Vector3, mat: Material, scale_v: Vector3 = Vector3.ONE) -> MeshInstance3D:
 var n = MeshInstance3D.new()
 n.mesh = shape
 n.material_override = mat
 n.position = pos
 n.scale = scale_v
 parent.add_child(n)
 return n

func box(parent: Node3D, size: Vector3, pos: Vector3, mat: Material) -> MeshInstance3D:
 var b = BoxMesh.new(); b.size = size
 return mesh(parent,b,pos,mat)

func cylinder(parent: Node3D, radius: float, height: float, pos: Vector3, mat: Material, top: float = -1) -> MeshInstance3D:
 var c = CylinderMesh.new()
 c.top_radius = radius if top<0 else top
 c.bottom_radius = radius
 c.height = height
 c.radial_segments = 12
 return mesh(parent,c,pos,mat)

func module(kind: String) -> Node3D:
 var n = Node3D.new()
 # Beveled hexagonal armor chassis, inset plates, attachment lugs, warning stripes.
 var base = cylinder(n,1.2,0.34,Vector3.ZERO,dark)
 base.mesh.radial_segments = 6
 cylinder(n,1.08,0.2,Vector3(0,0.24,0),alloy).mesh.radial_segments = 6
 for s in [-1,1]:
  box(n,Vector3(.12,.12,1.1),Vector3(s*.86,.4,0),trim)
  for z in [-.65,.65]: cylinder(n,.08,.12,Vector3(s*.77,.43,z),dark)
 match kind:
  "Cockpit":
   var canopy = PrismMesh.new();canopy.size=Vector3(1.45,1.0,1.9)
   mesh(n,canopy,Vector3(0,.85,-.1),glass)
   for s in [-1,1]:box(n,Vector3(.12,.7,1.65),Vector3(s*.68,.62,0),alloy)
   box(n,Vector3(1.3,.12,.12),Vector3(0,1.35,.38),trim)
  "Engine":
   var drive = cylinder(n,.64,1.65,Vector3(0,.7,.1),dark,.45);drive.rotation.x=PI/2
   for z in [-.5,0,.5]:
    var ring = cylinder(n,.7,.12,Vector3(0,.7,z),alloy);ring.rotation.x=PI/2
   var exhaust = cylinder(n,.49,.1,Vector3(0,.7,.97),glow);exhaust.rotation.x=PI/2
   var flame = cylinder(n,.38,1.6,Vector3(0,.7,1.75),glow,0.04);flame.rotation.x=PI/2;flame.name="Exhaust";flame.visible=false
  "Mining laser","Pulse cannon","Rocket launcher":
   cylinder(n,.55,.55,Vector3(0,.6,0),dark)
   var turret = Node3D.new();turret.name="Turret";n.add_child(turret);turret.position.y=.86
   var count: int = 2 if kind=="Rocket launcher" else 1
   for i in range(count):
    var x: float = (float(i)-float(count-1)/2)*.65
    var barrel = cylinder(turret,.25 if count==1 else .32,1.65,Vector3(x,0,-.55),alloy);barrel.rotation.x=PI/2
    var aperture = cylinder(turret,.18,.15,Vector3(x,0,-1.42),glow if kind=="Mining laser" else trim);aperture.rotation.x=PI/2
   box(turret,Vector3(1.1,.35,.7),Vector3(0,0,.32),dark)
  "Magnet":
   for s in [-1,1]:
    box(n,Vector3(.3,.9,.9),Vector3(s*.65,.8,0),alloy)
    box(n,Vector3(.35,.15,.45),Vector3(s*.65,1.3,-.3),glow)
   box(n,Vector3(1.4,.3,.3),Vector3(0,.65,.5),trim)
  "Shield":
   cylinder(n,.67,.55,Vector3(0,.65,0),alloy,.35)
   var t = TorusMesh.new();t.inner_radius=.48;t.outer_radius=.65
   var ring = mesh(n,t,Vector3(0,1.14,0),glow);ring.rotation.x=PI/3
  "Cargo":
   box(n,Vector3(1.65,.9,1.55),Vector3(0,.85,0),alloy)
   for x in [-.65,0,.65]:box(n,Vector3(.1,1.0,1.6),Vector3(x,.85,0),dark)
   box(n,Vector3(.9,.25,.05),Vector3(0,.88,-.8),trim)
  "Generator":
   cylinder(n,.57,1.15,Vector3(0,.95,0),dark)
   for y in [.55,.85,1.15,1.45]:cylinder(n,.67,.13,Vector3(0,y,0),glow if y==.85 else alloy)
  "Repair":
   cylinder(n,.5,.7,Vector3(0,.8,0),alloy)
   box(n,Vector3(1.2,.2,.25),Vector3(0,1.3,0),glow)
   box(n,Vector3(.25,.2,1.2),Vector3(0,1.3,0),glow)
  "Scanner":
   cylinder(n,.15,1.1,Vector3(0,1,0),alloy)
   var dish = cylinder(n,.7,.3,Vector3(0,1.5,0),trim,.12);dish.rotation.z=.4
  _:
   box(n,Vector3(1.5,.12,1.5),Vector3(0,.48,0),alloy)
   box(n,Vector3(.6,.04,1.3),Vector3(0,.57,0),trim)
 return n

func asteroid(seed_value: int, radius: float) -> ArrayMesh:
 var rng = RandomNumberGenerator.new();rng.seed=seed_value
 var points: Array[Vector3] = []
 var rings: int = 8
 var segments: int = 14
 for y in range(rings+1):
  var phi: float = PI*float(y)/rings
  for x in range(segments):
   var theta: float = TAU*float(x)/segments
   var rough: float = rng.randf_range(.76,1.23)
   var p = Vector3(sin(phi)*cos(theta),cos(phi),sin(phi)*sin(theta))
   points.append(p*radius*rough*Vector3(1,.82,1.17))
 var st = SurfaceTool.new();st.begin(Mesh.PRIMITIVE_TRIANGLES)
 for y in range(rings):
  for x in range(segments):
   var a: int = y*segments+x
   var b: int = y*segments+(x+1)%segments
   var c: int = (y+1)*segments+x
   var d: int = (y+1)*segments+(x+1)%segments
   for idx in [a,b,c,b,d,c]:
    st.set_uv(Vector2(float(idx%segments)/segments,float(idx/segments)/rings))
    st.add_vertex(points[idx])
 st.generate_normals()
 return st.commit()

func beam(parent: Node3D, start: Vector3, end: Vector3, color: Color, width: float) -> MeshInstance3D:
 var tube = CylinderMesh.new();tube.top_radius=width;tube.bottom_radius=width;tube.height=start.distance_to(end);tube.radial_segments=6
 var n = mesh(parent,tube,(start+end)/2,material(color,0,color))
 if start.distance_to(end)>.001:
  n.quaternion=Quaternion(Vector3.UP,(end-start).normalized())
 return n
