extends SceneTree

func _initialize() -> void:
	call_deferred("run")

func frames(count: int) -> void:
	for i in range(count):
		await process_frame

func run() -> void:
	var main: Node = load("res://scenes/main.tscn").instantiate()
	root.add_child(main)
	await frames(120)
	if main.interface == null or main.game_state == null or main.audio == null:
		print("SMOKE TEST: FAIL — main systems missing")
		quit(1)
		return
	main.audio.shutdown()
	main.queue_free()
	await frames(12)
	print("SMOKE TEST: PASS")
	quit()
