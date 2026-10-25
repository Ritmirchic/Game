extends SceneTree

var failures := 0

func _initialize() -> void:
	call_deferred("run")

func check_resource(path: String) -> void:
	var resource := ResourceLoader.load(path)
	if resource == null:
		failures += 1
		print("FAIL  packaged resource loads: ", path)
	else:
		print("PASS  packaged resource loads: ", path)

func run() -> void:
	for path in [
		"res://scenes/main.tscn",
		"res://assets/fonts/Rajdhani-SemiBold.ttf",
		"res://assets/audio/laser.wav",
		"res://assets/audio/ambient_sector_1.wav",
		"res://assets/art/backgrounds/sector_1_starter_belt.jpg",
		"res://assets/art/icons/resource_iron.png",
		"res://tests/test_runner.gd",
	]:
		check_resource(path)
	if FileAccess.file_exists("res://.godot/global_script_class_cache.cfg"):
		print("PASS  global script cache packaged")
	else:
		failures += 1
		print("FAIL  global script cache packaged")
	var imported := DirAccess.open("res://.godot/imported")
	if imported == null:
		failures += 1
		print("FAIL  imported resource directory packaged")
	else:
		print("PACK  imported resource files: ", imported.get_files().size())
	print("PACKAGE INSPECTOR: ", "PASS" if failures == 0 else "FAIL (%d)" % failures)
	quit(0 if failures == 0 else 1)
