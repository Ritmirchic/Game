extends Node
## Global Acoustic Perception Bus.
## Broadcasts world-space decibel impulses from footsteps, sprinting, doors,
## and alarms so The Listener AI and the Inspector's dB meter respond accurately.

signal noise_emitted(world_position: Vector3, decibels: float, source_tag: String)

var current_player_db: float = 0.0
var last_loud_position: Vector3 = Vector3.ZERO
var last_loud_db: float = 0.0
var last_loud_time: float = -999.0


func _process(delta: float) -> void:
	current_player_db = move_toward(current_player_db, 8.0, delta * 62.0)


func emit_noise(world_position: Vector3, decibels: float, source_tag: String = "player") -> void:
	if source_tag.begins_with("player") or source_tag == "door":
		current_player_db = maxf(current_player_db, decibels)
	if decibels >= 32.0:
		last_loud_position = world_position
		last_loud_db = decibels
		last_loud_time = Time.get_ticks_msec() / 1000.0
	noise_emitted.emit(world_position, decibels, source_tag)
