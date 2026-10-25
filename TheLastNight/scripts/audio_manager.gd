extends Node
## Positional 3D audio + UI sounds + layered ambient loops. All streams are
## original synthesized WAVs (see tools/gen_sounds.py).
class_name AudioManager

var A: AssetLib
# untyped pool: finished players free themselves and stale freed objects
# cannot be assigned back into a typed Array in this engine build.
var _pool3d: Array = []
var _ui_player: AudioStreamPlayer
var loops: Dictionary = {}
var thunder_t: float = 0.0
var storm: bool = false

func _init(assets: AssetLib) -> void:
	A = assets
	name = "AudioManager"

func _ready() -> void:
	_ui_player = AudioStreamPlayer.new()
	_ui_player.bus = "UI"
	add_child(_ui_player)
	# ensure ambient streams loop
	for n in ["amb_rain", "amb_hum", "amb_drone"]:
		var s: AudioStreamWAV = A.sound(n)
		s.loop_mode = AudioStreamWAV.LOOP_FORWARD
		s.loop_begin = 0
		s.loop_end = int(s.mix_rate * (4.0 if n != "amb_hum" else 2.0))

func play_3d(snd: String, pos: Vector3, volume_db: float = 0.0, pitch: float = 1.0) -> void:
	var player := AudioStreamPlayer3D.new()
	player.stream = A.sound(snd)
	player.unit_size = 3.5
	player.max_distance = 48
	player.position = pos
	player.volume_db = volume_db
	player.pitch_scale = pitch
	player.bus = "SFX"
	player.attenuation_model = AudioStreamPlayer3D.ATTENUATION_INVERSE_DISTANCE
	add_child(player)
	player.finished.connect(player.queue_free)
	player.play()
	_pool3d.append(player)
	# drop entries whose players already finished and freed themselves
	while _pool3d.size() > 0 and not is_instance_valid(_pool3d[0]):
		_pool3d.pop_front()
	if _pool3d.size() > 48:
		var old = _pool3d.pop_front()
		if is_instance_valid(old):
			old.queue_free()

func play_ui(snd: String) -> void:
	_ui_player.stream = A.sound(snd)
	_ui_player.play()

func set_loop(name: String, playing: bool, volume_db: float = 0.0) -> void:
	if playing:
		if loops.has(name):
			return
		var p := AudioStreamPlayer.new()
		p.stream = A.sound(name)
		p.bus = "Ambient"
		p.volume_db = volume_db
		add_child(p)
		p.play()
		loops[name] = p
	else:
		if loops.has(name):
			loops[name].stop()
			loops[name].queue_free()
			loops.erase(name)

func set_storm(enabled: bool) -> void:
	storm = enabled
	thunder_t = randf_range(6, 14)
	set_loop("amb_rain", enabled, -4.0 if enabled else 0.0)
	set_loop("amb_drone", true, -14.0)
	set_loop("amb_hum", true, -16.0)

func _process(delta: float) -> void:
	if not storm:
		return
	thunder_t -= delta
	if thunder_t <= 0.0:
		thunder_t = randf_range(14.0, 30.0)
		# thunder sounds positioned far around the store
		var ang := randf() * TAU
		var pos := Vector3(30 + cos(ang) * 40, 6, 20 + sin(ang) * 40)
		play_3d("sfx_thunder", pos, -4.0)
