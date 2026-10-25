extends SceneTree


func _init() -> void:
	var out_pck := "/tmp/alchemy_chaos.pck"
	var packer := PCKPacker.new()
	var err := packer.pck_start(out_pck)
	if err != OK:
		printerr("Failed to start PCK: ", err)
		quit(1)
		return

	var files: Array[String] = [
		"res://project.godot",
		"res://.godot/global_script_class_cache.cfg",
	]
	_collect_all("res://.godot/imported", files)
	_collect_all("res://assets", files)
	_collect_all("res://data", files)
	_collect_all("res://shaders", files)
	_collect_all("res://scripts", files)
	_collect_all("res://scenes", files)
	_collect_all("res://tests", files)

	var added := 0
	for res_path in files:
		var abs_p := ProjectSettings.globalize_path(res_path)
		if FileAccess.file_exists(abs_p):
			var aerr := packer.add_file(res_path, abs_p)
			if aerr != OK:
				printerr("Failed to pack file: ", res_path)
				quit(1)
				return
			added += 1

	var ferr := packer.flush(true)
	if ferr != OK:
		printerr("Failed to flush PCK: ", ferr)
		quit(1)
		return

	print("[PCK] Packed %d files into %s" % [added, out_pck])
	quit(0)


func _collect_all(dir_path: String, out: Array[String]) -> void:
	var d := DirAccess.open(dir_path)
	if d == null:
		return
	d.list_dir_begin()
	var nm := d.get_next()
	while nm != "":
		if not nm.begins_with("."):
			var full := dir_path + "/" + nm
			if d.current_is_dir():
				_collect_all(full, out)
			else:
				if not nm.ends_with(".uid"):
					out.append(full)
		nm = d.get_next()
	d.list_dir_end()
