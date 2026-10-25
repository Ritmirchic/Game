class_name MaterialLibrary
extends RefCounted
## Builds and caches world-triplanar PBR StandardMaterial3D and ShaderMaterial
## assets from our seamless procedural PBR textures and custom shaders.

static var _mat_cache: Dictionary = {}
static var _model_cache: Dictionary = {}


static func clear_caches() -> void:
	_mat_cache.clear()
	_model_cache.clear()


static func get_pbr(tex_prefix: String, uv_scale: float = 0.42, metallic: float = 0.0, tint: Color = Color.WHITE, triplanar: bool = true) -> StandardMaterial3D:
	var key := "%s_%.2f_%.2f_%s_%s" % [tex_prefix, uv_scale, metallic, tint.to_html(), str(triplanar)]
	if _mat_cache.has(key):
		return _mat_cache[key]

	var mat := StandardMaterial3D.new()
	mat.albedo_color = tint
	mat.metallic = metallic
	var alb_path := "res://assets/textures/%s_alb.png" % tex_prefix
	var nrm_path := "res://assets/textures/%s_nrm.png" % tex_prefix
	var rgh_path := "res://assets/textures/%s_rgh.png" % tex_prefix

	if ResourceLoader.exists(alb_path):
		mat.albedo_texture = load(alb_path)
	if ResourceLoader.exists(nrm_path):
		mat.normal_enabled = true
		mat.normal_scale = 0.95
		mat.normal_texture = load(nrm_path)
	if ResourceLoader.exists(rgh_path):
		mat.roughness_texture = load(rgh_path)
	else:
		mat.roughness = 0.78

	if triplanar:
		mat.uv1_triplanar = true
		mat.uv1_world_triplanar = true
		mat.uv1_scale = Vector3(uv_scale, uv_scale, uv_scale)
	else:
		mat.uv1_scale = Vector3(uv_scale, uv_scale, uv_scale)

	_mat_cache[key] = mat
	return mat


static func get_emissive(albedo: Color, emission_col: Color, energy: float = 2.0) -> StandardMaterial3D:
	var key := "em_%s_%s_%.2f" % [albedo.to_html(), emission_col.to_html(), energy]
	if _mat_cache.has(key):
		return _mat_cache[key]
	var mat := StandardMaterial3D.new()
	mat.albedo_color = albedo
	mat.roughness = 0.25
	mat.emission_enabled = true
	mat.emission = emission_col
	mat.emission_energy_multiplier = energy
	_mat_cache[key] = mat
	return mat


static func get_image_mat(tex_path: String, emissive_energy: float = 0.0) -> StandardMaterial3D:
	var key := "img_%s_%.2f" % [tex_path, emissive_energy]
	if _mat_cache.has(key):
		return _mat_cache[key]
	var mat := StandardMaterial3D.new()
	if ResourceLoader.exists(tex_path):
		var tex: Texture2D = load(tex_path)
		mat.albedo_texture = tex
		if emissive_energy > 0.0:
			mat.emission_enabled = true
			mat.emission_texture = tex
			mat.emission_energy_multiplier = emissive_energy
	mat.roughness = 0.65
	_mat_cache[key] = mat
	return mat


static func get_watcher_canvas_mat() -> ShaderMaterial:
	var key := "shader_watcher_canvas"
	if _mat_cache.has(key):
		return _mat_cache[key]
	var sm := ShaderMaterial.new()
	sm.shader = load("res://shaders/anomaly_watcher_canvas.gdshader")
	sm.set_shader_parameter("canvas_tex", load("res://assets/textures/painting_hollow_anomaly.png"))
	sm.set_shader_parameter("warp_amount", 1.0)
	_mat_cache[key] = sm
	return sm


static func get_void_window_mat() -> ShaderMaterial:
	var key := "shader_void_window"
	if _mat_cache.has(key):
		return _mat_cache[key]
	var sm := ShaderMaterial.new()
	sm.shader = load("res://shaders/impossible_void.gdshader")
	sm.set_shader_parameter("void_tex", load("res://assets/textures/window_void_anomaly.png"))
	_mat_cache[key] = sm
	return sm


static func instantiate_model(glb_name: String) -> Node3D:
	var path := glb_name if glb_name.begins_with("res://") else "res://assets/models/%s" % glb_name
	if not _model_cache.has(path):
		if ResourceLoader.exists(path):
			_model_cache[path] = load(path)
		else:
			push_error("Missing GLB model: " + path)
			return Node3D.new()
	var packed: PackedScene = _model_cache[path]
	return packed.instantiate() as Node3D
