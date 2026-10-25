extends Node

var _sfx_players: Array[AudioStreamPlayer] = []
var _ambient_player: AudioStreamPlayer
var master_vol: float = 0.85
var sfx_vol: float = 0.85
var ambient_vol: float = 0.55

const REACTION_SFX_MAP := {
	"BUBBLING": "res://assets/audio/sfx_bubble.wav",
	"SMOKE": "res://assets/audio/sfx_smoke.wav",
	"SPARKS": "res://assets/audio/sfx_spark.wav",
	"EXPLOSION": "res://assets/audio/sfx_explosion.wav",
	"CRYSTALLIZATION": "res://assets/audio/sfx_crystal.wav",
	"FREEZING": "res://assets/audio/sfx_freeze.wav",
	"MELTING": "res://assets/audio/sfx_melt.wav",
	"ELECTRICAL": "res://assets/audio/sfx_electric.wav",
	"LIQUID_MIXING": "res://assets/audio/sfx_bubble.wav",
	"COLOR_TRANSITION": "res://assets/audio/sfx_crystal.wav",
}


func _ready() -> void:
	for i in range(6):
		var p := AudioStreamPlayer.new()
		add_child(p)
		_sfx_players.append(p)
	_ambient_player = AudioStreamPlayer.new()
	add_child(_ambient_player)
	_apply_ambient_volume()


func _exit_tree() -> void:
	if _ambient_player != null:
		_ambient_player.stop()
		_ambient_player.stream = null
	for p in _sfx_players:
		if p != null:
			p.stop()
			p.stream = null



func start_ambient() -> void:
	if _ambient_player == null:
		return
	if not _ambient_player.playing:
		var st := AssetVault.wav("res://assets/audio/amb_lab_loop.wav", true)
		if st != null:
			_ambient_player.stream = st
			_apply_ambient_volume()
			_ambient_player.play()


func set_volumes(p_master: float, p_sfx: float, p_ambient: float) -> void:
	master_vol = clampf(p_master, 0.0, 1.0)
	sfx_vol = clampf(p_sfx, 0.0, 1.0)
	ambient_vol = clampf(p_ambient, 0.0, 1.0)
	_apply_ambient_volume()


func _apply_ambient_volume() -> void:
	if _ambient_player == null:
		return
	var gain := master_vol * ambient_vol
	_ambient_player.volume_db = -80.0 if gain <= 0.001 else linear_to_db(gain)


func play_sfx(path: String, pitch: float = 1.0) -> void:
	var gain := master_vol * sfx_vol
	if gain <= 0.001:
		return
	var st := AssetVault.wav(path, false)
	if st == null:
		return
	for p in _sfx_players:
		if not p.playing:
			p.stream = st
			p.volume_db = linear_to_db(gain)
			p.pitch_scale = pitch
			p.play()
			return
	var fallback := _sfx_players[0]
	fallback.stream = st
	fallback.volume_db = linear_to_db(gain)
	fallback.pitch_scale = pitch
	fallback.play()


func play_click(pitch: float = 1.0) -> void:
	play_sfx("res://assets/audio/sfx_click.wav", pitch)


func play_reaction(reaction_type: String) -> void:
	var path: String = REACTION_SFX_MAP.get(reaction_type, "res://assets/audio/sfx_bubble.wav")
	play_sfx(path, randf_range(0.95, 1.06))


func play_discovery(is_rare: bool) -> void:
	if is_rare:
		play_sfx("res://assets/audio/sfx_rare_discover.wav", 1.0)
	else:
		play_sfx("res://assets/audio/sfx_discover.wav", 1.0)


func play_upgrade() -> void:
	play_sfx("res://assets/audio/sfx_upgrade.wav", 1.0)


func play_fizzle() -> void:
	play_sfx("res://assets/audio/sfx_fizzle.wav", 1.0)

