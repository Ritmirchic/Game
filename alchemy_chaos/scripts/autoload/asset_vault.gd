extends Node

var _tex_cache: Dictionary = {}
var _wav_cache: Dictionary = {}
var _shader_cache: Dictionary = {}
var _style_cache: Dictionary = {}

var font_display: FontFile
var font_body: FontFile
var font_italic: FontFile
var global_theme: Theme


func _ready() -> void:
	font_display = _load_font("res://assets/fonts/Cinzel.ttf")
	font_body = _load_font("res://assets/fonts/Nunito.ttf")
	font_italic = _load_font("res://assets/fonts/CormorantItalic.ttf")
	global_theme = build_global_theme()


func _exit_tree() -> void:
	_wav_cache.clear()
	_tex_cache.clear()
	_shader_cache.clear()
	_style_cache.clear()



func _load_font(path: String) -> FontFile:
	var f := FontFile.new()
	if FileAccess.file_exists(path):
		var bytes := FileAccess.get_file_as_bytes(path)
		if bytes.size() > 0:
			f.data = bytes
	return f


func tex(path: String) -> Texture2D:
	if path.is_empty():
		return null
	if _tex_cache.has(path):
		return _tex_cache[path]
	if ResourceLoader.exists(path):
		var loaded = ResourceLoader.load(path)
		if loaded is Texture2D:
			_tex_cache[path] = loaded
			return loaded
	if FileAccess.file_exists(path):
		var bytes := FileAccess.get_file_as_bytes(path)
		if bytes.size() > 0:
			var img := Image.new()
			var err := img.load_png_from_buffer(bytes)
			if err == OK:
				var itex := ImageTexture.create_from_image(img)
				_tex_cache[path] = itex
				return itex
	return null


func shader(path: String) -> Shader:
	if _shader_cache.has(path):
		return _shader_cache[path]
	if ResourceLoader.exists(path):
		var s = ResourceLoader.load(path)
		if s is Shader:
			_shader_cache[path] = s
			return s
	if FileAccess.file_exists(path):
		var code := FileAccess.get_file_as_string(path)
		var sh := Shader.new()
		sh.code = code
		_shader_cache[path] = sh
		return sh
	return null


func wav(path: String, loop: bool = false) -> AudioStreamWAV:
	var cache_key := path + ("#loop" if loop else "")
	if _wav_cache.has(cache_key):
		return _wav_cache[cache_key]
	if not FileAccess.file_exists(path):
		return null
	var bytes := FileAccess.get_file_as_bytes(path)
	if bytes.size() < 44:
		return null
	# Parse standard RIFF PCM 16-bit WAV chunks cleanly
	var pos := 12
	var sample_rate := 22050
	var channels := 1
	var pcm_data := PackedByteArray()
	while pos + 8 <= bytes.size():
		var chunk_id := bytes.slice(pos, pos + 4).get_string_from_ascii()
		var chunk_size := bytes.decode_u32(pos + 4)
		pos += 8
		if pos + chunk_size > bytes.size():
			break
		if chunk_id == "fmt " and chunk_size >= 16:
			channels = bytes.decode_u16(pos + 2)
			sample_rate = bytes.decode_u32(pos + 4)
		elif chunk_id == "data":
			pcm_data = bytes.slice(pos, pos + chunk_size)
			break
		pos += chunk_size + (chunk_size % 2)
	if pcm_data.is_empty():
		return null
	var stream := AudioStreamWAV.new()
	stream.format = AudioStreamWAV.FORMAT_16_BITS
	stream.mix_rate = sample_rate
	stream.stereo = (channels == 2)
	stream.data = pcm_data
	if loop:
		stream.loop_mode = AudioStreamWAV.LOOP_FORWARD
		stream.loop_begin = 0
		stream.loop_end = int(pcm_data.size() / (2 * max(1, channels)))
	_wav_cache[cache_key] = stream
	return stream


func style_box(ui_name: String, margin: int = 16, content_h: int = 12, content_v: int = 8) -> StyleBoxTexture:
	var key := "%s_%d_%d_%d" % [ui_name, margin, content_h, content_v]
	if _style_cache.has(key):
		return _style_cache[key]
	var sb := StyleBoxTexture.new()
	sb.texture = tex("res://assets/ui/" + ui_name + ".png")
	sb.texture_margin_left = margin
	sb.texture_margin_right = margin
	sb.texture_margin_top = margin
	sb.texture_margin_bottom = margin
	sb.content_margin_left = content_h
	sb.content_margin_right = content_h
	sb.content_margin_top = content_v
	sb.content_margin_bottom = content_v
	_style_cache[key] = sb
	return sb


func style_button(btn: BaseButton, hero: bool = false) -> void:
	if hero:
		btn.add_theme_stylebox_override("normal", style_box("ui_btn_transmute_normal", 16, 18, 10))
		btn.add_theme_stylebox_override("hover", style_box("ui_btn_transmute_hover", 16, 18, 10))
		btn.add_theme_stylebox_override("pressed", style_box("ui_btn_transmute_pressed", 16, 18, 10))
		btn.add_theme_stylebox_override("disabled", style_box("ui_btn_disabled", 14, 18, 10))
		btn.add_theme_stylebox_override("focus", style_box("ui_btn_transmute_hover", 16, 18, 10))
	else:
		btn.add_theme_stylebox_override("normal", style_box("ui_btn_normal", 14, 14, 8))
		btn.add_theme_stylebox_override("hover", style_box("ui_btn_hover", 14, 14, 8))
		btn.add_theme_stylebox_override("pressed", style_box("ui_btn_pressed", 14, 14, 8))
		btn.add_theme_stylebox_override("disabled", style_box("ui_btn_disabled", 14, 14, 8))
		btn.add_theme_stylebox_override("focus", style_box("ui_btn_hover", 14, 14, 8))
	btn.mouse_default_cursor_shape = Control.CURSOR_POINTING_HAND


func build_global_theme() -> Theme:
	var th := Theme.new()
	if font_body:
		th.default_font = font_body
		th.default_font_size = 16

	# Buttons - zero unstyled default buttons anywhere!
	th.set_stylebox("normal", "Button", style_box("ui_btn_normal", 14, 14, 8))
	th.set_stylebox("hover", "Button", style_box("ui_btn_hover", 14, 14, 8))
	th.set_stylebox("pressed", "Button", style_box("ui_btn_pressed", 14, 14, 8))
	th.set_stylebox("disabled", "Button", style_box("ui_btn_disabled", 14, 14, 8))
	th.set_stylebox("focus", "Button", style_box("ui_btn_hover", 14, 14, 8))
	th.set_color("font_color", "Button", Color("#F8EDD4"))
	th.set_color("font_hover_color", "Button", Color("#FFF8E6"))
	th.set_color("font_pressed_color", "Button", Color("#E2C58A"))
	th.set_color("font_disabled_color", "Button", Color("#80788A"))

	# OptionButton & CheckButton
	for cls in ["OptionButton", "CheckButton"]:
		th.set_stylebox("normal", cls, style_box("ui_btn_normal", 12, 12, 6))
		th.set_stylebox("hover", cls, style_box("ui_btn_hover", 12, 12, 6))
		th.set_stylebox("pressed", cls, style_box("ui_btn_pressed", 12, 12, 6))
		th.set_stylebox("focus", cls, style_box("ui_btn_hover", 12, 12, 6))

	# Panels
	th.set_stylebox("panel", "PanelContainer", style_box("ui_panel_ornate", 18, 14, 12))
	th.set_stylebox("panel", "PopupMenu", style_box("ui_panel_ornate", 16, 12, 10))
	th.set_color("font_color", "PopupMenu", Color("#F5EAD0"))
	th.set_color("font_hover_color", "PopupMenu", Color("#FFE082"))

	# LineEdit
	th.set_stylebox("normal", "LineEdit", style_box("ui_panel_inset", 12, 12, 6))
	th.set_stylebox("focus", "LineEdit", style_box("ui_card_hover", 12, 12, 6))
	th.set_color("font_color", "LineEdit", Color("#FAF0DC"))
	th.set_color("font_placeholder_color", "LineEdit", Color("#988EA8"))

	# ProgressBar
	th.set_stylebox("background", "ProgressBar", style_box("ui_progress_bg", 8, 2, 2))
	th.set_stylebox("fill", "ProgressBar", style_box("ui_progress_fill", 8, 2, 2))

	# Scrollbars
	var sb_bg := StyleBoxFlat.new()
	sb_bg.bg_color = Color(0.08, 0.06, 0.13, 0.85)
	sb_bg.set_corner_radius_all(5)
	sb_bg.content_margin_left = 4
	sb_bg.content_margin_right = 4
	var sb_grab := StyleBoxFlat.new()
	sb_grab.bg_color = Color("#C6984A")
	sb_grab.set_corner_radius_all(5)
	var sb_grab_hi := StyleBoxFlat.new()
	sb_grab_hi.bg_color = Color("#F5CE78")
	sb_grab_hi.set_corner_radius_all(5)
	th.set_stylebox("scroll", "VScrollBar", sb_bg)
	th.set_stylebox("grabber", "VScrollBar", sb_grab)
	th.set_stylebox("grabber_highlight", "VScrollBar", sb_grab_hi)
	th.set_stylebox("grabber_pressed", "VScrollBar", sb_grab_hi)

	# HSlider
	var sl_bg := StyleBoxFlat.new()
	sl_bg.bg_color = Color(0.10, 0.08, 0.16, 1.0)
	sl_bg.border_color = Color("#A88042")
	sl_bg.set_border_width_all(1)
	sl_bg.set_corner_radius_all(5)
	sl_bg.content_margin_top = 6
	sl_bg.content_margin_bottom = 6
	var sl_fill := StyleBoxFlat.new()
	sl_fill.bg_color = Color("#48C69C")
	sl_fill.set_corner_radius_all(5)
	sl_fill.content_margin_top = 6
	sl_fill.content_margin_bottom = 6
	th.set_stylebox("slider", "HSlider", sl_bg)
	th.set_stylebox("grabber_area", "HSlider", sl_fill)
	th.set_stylebox("grabber_area_highlight", "HSlider", sl_fill)

	# Labels
	th.set_color("font_color", "Label", Color("#F4EBDC"))
	return th
