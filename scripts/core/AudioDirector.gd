extends Node
## Professional Spatial Audio & Intentional Silence Director.

var _cache: Dictionary = {}
var _ambience_player: AudioStreamPlayer
var _tape_player: AudioStreamPlayer
var _silence_timer: float = 0.0
var _target_ambience_db: float = -9.0


func _ready() -> void:
	process_mode = Node.PROCESS_MODE_ALWAYS
	_ambience_player = AudioStreamPlayer.new()
	_ambience_player.name = "AmbiencePlayer"
	add_child(_ambience_player)

	_tape_player = AudioStreamPlayer.new()
	_tape_player.name = "TapePlayer"
	add_child(_tape_player)


func get_stream(res_path: String) -> AudioStream:
	if _cache.has(res_path):
		return _cache[res_path]
	if ResourceLoader.exists(res_path):
		var s: AudioStream = load(res_path)
		_cache[res_path] = s
		return s
	return null


func start_ambience(res_path: String, volume_db: float = -9.0) -> void:
	var stream := get_stream(res_path)
	if stream == null:
		return
	if stream is AudioStreamWAV:
		(stream as AudioStreamWAV).loop_mode = AudioStreamWAV.LOOP_FORWARD
		(stream as AudioStreamWAV).loop_end = int((stream as AudioStreamWAV).data.size() / (2 * (2 if (stream as AudioStreamWAV).stereo else 1)))
	_target_ambience_db = volume_db
	if _ambience_player.stream != stream or not _ambience_player.playing:
		_ambience_player.stream = stream
		_ambience_player.volume_db = volume_db
		_ambience_player.play()


func stop_ambience() -> void:
	if _ambience_player and _ambience_player.playing:
		_ambience_player.stop()


func trigger_intentional_silence(duration: float = 4.5) -> void:
	_silence_timer = maxf(_silence_timer, duration)


func _process(delta: float) -> void:
	if _ambience_player and _ambience_player.playing:
		var amb_scale: float = clampf(float(GameState.settings.get("ambience_volume", 0.78)), 0.001, 1.0)
		var base_db := _target_ambience_db + linear_to_db(amb_scale)
		if _silence_timer > 0.0:
			_silence_timer -= delta
			_ambience_player.volume_db = move_toward(_ambience_player.volume_db, -42.0, delta * 18.0)
		else:
			_ambience_player.volume_db = move_toward(_ambience_player.volume_db, base_db, delta * 8.0)


func play_2d(sound_name: String, volume_db: float = -3.0, pitch: float = 1.0) -> void:
	var path := sound_name if sound_name.begins_with("res://") else "res://assets/audio/%s.wav" % sound_name
	var stream := get_stream(path)
	if stream == null:
		return
	var p := AudioStreamPlayer.new()
	p.stream = stream
	var sfx_scale: float = clampf(float(GameState.settings.get("sfx_volume", 0.90)), 0.001, 1.0)
	p.volume_db = volume_db + linear_to_db(sfx_scale)
	p.pitch_scale = pitch
	add_child(p)
	p.finished.connect(p.queue_free)
	p.play()


func play_3d(sound_name: String, world_pos: Vector3, max_dist: float = 18.0, volume_db: float = 0.0, pitch: float = 1.0) -> AudioStreamPlayer3D:
	var path := sound_name if sound_name.begins_with("res://") else "res://assets/audio/%s.wav" % sound_name
	var stream := get_stream(path)
	if stream == null or get_tree().current_scene == null:
		return null
	var p := AudioStreamPlayer3D.new()
	p.stream = stream
	var sfx_scale: float = clampf(float(GameState.settings.get("sfx_volume", 0.90)), 0.001, 1.0)
	p.volume_db = volume_db + linear_to_db(sfx_scale)
	p.max_distance = max_dist
	p.unit_size = 3.5
	p.pitch_scale = pitch
	get_tree().current_scene.add_child(p)
	p.global_position = world_pos
	p.finished.connect(p.queue_free)
	p.play()
	return p


func make_looping_3d(sound_name: String, parent: Node3D, local_pos: Vector3 = Vector3.ZERO, max_dist: float = 14.0, volume_db: float = -4.0) -> AudioStreamPlayer3D:
	var path := sound_name if sound_name.begins_with("res://") else "res://assets/audio/%s.wav" % sound_name
	var stream := get_stream(path)
	var p := AudioStreamPlayer3D.new()
	p.max_distance = max_dist
	p.unit_size = 3.2
	p.volume_db = volume_db
	if stream is AudioStreamWAV:
		var dup := (stream as AudioStreamWAV).duplicate() as AudioStreamWAV
		dup.loop_mode = AudioStreamWAV.LOOP_FORWARD
		dup.loop_end = int(dup.data.size() / 2)
		p.stream = dup
	parent.add_child(p)
	p.position = local_pos
	return p


func play_footstep(surface: String, world_pos: Vector3, volume_db: float = -5.0, pitch: float = 1.0) -> void:
	var candidates: Array[String] = ["step_wood_01", "step_wood_02", "step_wood_03"]
	match surface:
		"concrete", "carpet":
			candidates = ["step_concrete_01", "step_concrete_02", "step_concrete_03"]
		"tile":
			candidates = ["step_tile_01", "step_tile_02"]
		"metal":
			candidates = ["step_metal_01", "step_metal_02"]
	var chosen := candidates[randi() % candidates.size()]
	play_3d(chosen, world_pos, 12.0, volume_db, pitch * randf_range(0.94, 1.06))


func play_tape(tape_id: String) -> void:
	if not GameState.TAPE_LOGS.has(tape_id):
		return
	var meta: Dictionary = GameState.TAPE_LOGS[tape_id]
	var stream := get_stream(meta["audio"])
	if stream:
		_tape_player.stream = stream
		var sfx_scale: float = clampf(float(GameState.settings.get("sfx_volume", 0.90)), 0.001, 1.0)
		_tape_player.volume_db = -1.5 + linear_to_db(sfx_scale)
		_tape_player.play()
	GameState.mark_tape_played(tape_id)
