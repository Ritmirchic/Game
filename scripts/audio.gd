class_name GameAudio
extends Node
const State = preload("res://scripts/game_state.gd")

var game_state: State
var music: AudioStreamPlayer
var voices: Array[AudioStreamPlayer] = []
var cache: Dictionary = {}
var voice_index := 0
var current_sector := -1

func _ready() -> void:
	music = AudioStreamPlayer.new()
	music.bus = "Music"
	add_child(music)
	for i in range(14):
		var voice := AudioStreamPlayer.new()
		voice.bus = "SFX"
		add_child(voice)
		voices.append(voice)
	for name in ["laser","cannon","enemy_shot","impact","explosion","pickup","ui","dock","achievement","alarm"]:
		cache[name] = load("res://assets/audio/%s.wav" % name)

func configure(state: State) -> void:
	game_state = state
	apply_settings()

func apply_settings() -> void:
	if game_state == null:
		return
	var settings: Dictionary = game_state.profile.settings
	AudioServer.set_bus_volume_db(AudioServer.get_bus_index("Master"), linear_to_db(maxf(0.0001, float(settings.master))))
	AudioServer.set_bus_volume_db(AudioServer.get_bus_index("Music"), linear_to_db(maxf(0.0001, float(settings.music))))
	AudioServer.set_bus_volume_db(AudioServer.get_bus_index("SFX"), linear_to_db(maxf(0.0001, float(settings.sfx))))

func play_sector(index: int) -> void:
	if current_sector == index and music.playing:
		return
	current_sector = index
	var stream: AudioStreamWAV = load("res://assets/audio/ambient_sector_%d.wav" % (index + 1)).duplicate()
	stream.loop_mode = AudioStreamWAV.LOOP_FORWARD
	stream.loop_begin = 0
	stream.loop_end = int(stream.get_length() * stream.mix_rate)
	music.stream = stream
	music.volume_db = -32.0
	music.play()
	var tween := create_tween()
	tween.tween_property(music, "volume_db", 0.0, 2.0)

func shutdown() -> void:
	if is_instance_valid(music):
		music.stop()
	for voice in voices:
		if is_instance_valid(voice):
			voice.stop()

func _exit_tree() -> void:
	shutdown()
	if is_instance_valid(music):
		music.stop()
		music.stream = null
	for voice in voices:
		if is_instance_valid(voice):
			voice.stop()
			voice.stream = null
	cache.clear()

func play(name: String) -> void:
	if not cache.has(name) or voices.is_empty():
		return
	var voice := voices[voice_index]
	voice_index = (voice_index + 1) % voices.size()
	voice.stream = cache[name]
	voice.pitch_scale = 1.0
	voice.volume_db = 0.0
	match name:
		"explosion": voice.volume_db = -4.0
		"laser": voice.volume_db = -9.0
		"pickup": voice.pitch_scale = 1.0 + float(voice_index % 4) * 0.07
		"ui": voice.volume_db = -7.0
		"alarm": voice.volume_db = -5.0
	voice.play()
