extends SceneTree


func _init() -> void:
	DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path("res://.godot/imported"))
	var png_paths: Array[String] = []
	_collect_pngs("res://assets", png_paths)
	png_paths.sort()

	var imported_count := 0
	for res_path in png_paths:
		var abs_path := ProjectSettings.globalize_path(res_path)
		var img := Image.load_from_file(abs_path)
		if img == null or img.is_empty():
			push_error("Failed to load PNG: " + res_path)
			quit(1)
			return
		var tex := ImageTexture.create_from_image(img)
		var hash_str := res_path.md5_text()
		var fname := res_path.get_file()
		var dest_res := "res://.godot/imported/%s-%s.res" % [fname, hash_str]
		var err := ResourceSaver.save(tex, dest_res, ResourceSaver.FLAG_COMPRESS)
		if err != OK:
			push_error("Failed to save imported texture: " + dest_res)
			quit(1)
			return

		var imp_txt := "[remap]\n\nimporter=\"texture\"\ntype=\"Texture2D\"\npath=\"%s\"\n\n[deps]\n\nsource_file=\"%s\"\ndest_files=[\"%s\"]\n" % [
			dest_res, res_path, dest_res
		]
		var f := FileAccess.open(res_path + ".import", FileAccess.WRITE)
		f.store_string(imp_txt)
		f.close()
		imported_count += 1

	print("[IMPORT] Imported %d PNG textures into res://.godot/imported/" % imported_count)
	quit(0)


func _collect_pngs(dir_path: String, out: Array[String]) -> void:
	var d := DirAccess.open(dir_path)
	if d == null:
		return
	d.list_dir_begin()
	var name := d.get_next()
	while name != "":
		if not name.begins_with("."):
			var full := dir_path + "/" + name
			if d.current_is_dir():
				_collect_pngs(full, out)
			elif name.ends_with(".png"):
				out.append(full)
		name = d.get_next()
	d.list_dir_end()
