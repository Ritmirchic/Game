extends RefCounted
## Loads/caches generated textures, sounds and PBR materials. All assets are
## original files produced by tools/gen_textures.py and tools/gen_sounds.py.
class_name AssetLib

var _tex: Dictionary = {}
var _mat: Dictionary = {}
var _snd: Dictionary = {}

func tex(name: String) -> Texture2D:
	if not _tex.has(name):
		var t: Texture2D = load("res://assets/textures/" + name + ".png")
		assert(t != null, "missing texture " + name)
		_tex[name] = t
	return _tex[name]

func sound(name: String) -> AudioStream:
	if not _snd.has(name):
		var s: AudioStream = load("res://assets/sounds/" + name + ".wav")
		assert(s != null, "missing sound " + name)
		_snd[name] = s
	return _snd[name]

func material(name: String, albedo_tex: String, roughness: float = 0.8, metallic: float = 0.0,
		rgba: Color = Color.WHITE, uv_scale: Vector3 = Vector3.ONE, emission: Color = Color.BLACK,
		emission_energy: float = 1.0, transparency: bool = false) -> StandardMaterial3D:
	var key := name
	if _mat.has(key):
		return _mat[key]
	var m := StandardMaterial3D.new()
	m.resource_name = name
	if albedo_tex != "":
		m.albedo_texture = tex(albedo_tex)
		m.uv1_scale = Vector3(uv_scale.x, uv_scale.y, uv_scale.z)
		m.texture_repeat = true
	m.albedo_color = rgba
	m.roughness = roughness
	m.metallic = metallic
	if emission != Color.BLACK:
		m.emission_enabled = true
		m.emission = emission
		m.emission_energy_multiplier = emission_energy
	if transparency:
		m.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
		m.shading_mode = BaseMaterial3D.SHADING_MODE_PER_PIXEL
	_mat[key] = m
	return m

func plain(name: String, color: Color, rough: float = 0.75, metal: float = 0.0,
		emission: Color = Color.BLACK, energy: float = 1.0, transparency: bool = false) -> StandardMaterial3D:
	var key := "plain_" + name + str(color) + str(emission)
	if _mat.has(key):
		return _mat[key]
	var m := StandardMaterial3D.new()
	m.resource_name = name
	m.albedo_color = color
	m.roughness = rough
	m.metallic = metal
	if emission != Color.BLACK:
		m.emission_enabled = true
		m.emission = emission
		m.emission_energy_multiplier = energy
	if transparency:
		m.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	_mat[key] = m
	return m

# Commonly shared materials -------------------------------------------------
func wall_paint() -> StandardMaterial3D:
	return material("wall_paint", "wall_paint", 0.92, 0.0, Color.WHITE, Vector3(2, 1.4, 1))
func wall_brick() -> StandardMaterial3D:
	return material("wall_brick", "wall_brick", 0.9, 0.0, Color.WHITE, Vector3(3, 1.6, 1))
func floor_tile() -> StandardMaterial3D:
	return material("floor_tile", "floor_tile", 0.55, 0.05, Color.WHITE, Vector3(8, 6, 1))
func floor_concrete() -> StandardMaterial3D:
	return material("floor_concrete", "floor_concrete", 0.85, 0.0, Color.WHITE, Vector3(6, 6, 1))
func ceiling_mat() -> StandardMaterial3D:
	return material("ceiling", "ceiling", 0.95, 0.0, Color.WHITE, Vector3(8, 6, 1))
func asphalt() -> StandardMaterial3D:
	return material("asphalt", "asphalt", 0.96, 0.0, Color.WHITE, Vector3(14, 14, 1))
func wood() -> StandardMaterial3D:
	return material("wood", "wood_planks", 0.7, 0.0, Color.WHITE, Vector3(1.5, 1.5, 1.5))
func shelf_metal() -> StandardMaterial3D:
	return material("shelf_metal", "metal_shelf", 0.45, 0.75)
func dark_metal() -> StandardMaterial3D:
	return material("dark_metal", "metal_dark", 0.5, 0.8)
func fridge_metal() -> StandardMaterial3D:
	return material("fridge_metal", "fridge_metal", 0.35, 0.85)
func cardboard() -> StandardMaterial3D:
	return material("cardboard", "cardboard", 0.9)
func products_mat() -> StandardMaterial3D:
	return material("products", "products", 0.7)
func hazard() -> StandardMaterial3D:
	return material("hazard", "hazard", 0.7)
func door_metal() -> StandardMaterial3D:
	return material("door_metal", "door_metal", 0.45, 0.8, Color.WHITE, Vector3(1, 2, 1))
func glass() -> StandardMaterial3D:
	return material("glass", "", 0.08, 0.0, Color(0.75, 0.88, 0.95, 0.28), Vector3.ONE, Color.BLACK, 1.0, true)
func blood_mat() -> StandardMaterial3D:
	return material("blood", "blood_splat", 0.6, 0.0, Color.WHITE, Vector3.ONE, Color.BLACK, 1.0, true)
