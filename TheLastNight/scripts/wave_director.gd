extends Node
## Data-driven ten-night campaign director: schedules spawn pulses across the
## five attack routes, runs environmental events and validates wave completion.
## Host/authority only simulation; clients receive spawns via the session.
class_name WaveDirector

var session
var night_id: int = 0
var cfg: Dictionary = {}
var active: bool = false
var elapsed: float = 0.0
var queue: Array = [] # {at, point, type, count}
var spawned: int = 0
var total_to_spawn: int = 0
var flicker_t: float = 0.0
var flicker_on: bool = true

func _init(sess) -> void:
	session = sess
	name = "WaveDirector"

func start_night(id: int) -> void:
	night_id = id
	cfg = session.data.night_config(id)
	elapsed = 0.0
	queue.clear()
	spawned = 0
	total_to_spawn = 0
	for pulse in cfg["pulses"]:
		var point: String = pulse["point"]
		for mob in pulse["mobs"]:
			for i in int(mob["count"]):
				queue.append({
					"at": float(pulse["at"]) + i * 0.7,
					"point": point,
					"type": mob["type"],
				})
				total_to_spawn += 1
	queue.sort_custom(func(a, b): return a["at"] < b["at"])
	active = true
	session.level.set_night_atmosphere(cfg, true)
	session.audio.set_storm(true)
	session.ui.banner("NIGHT %d — %s" % [id, cfg["name"]])

func stop() -> void:
	active = false
	session.level.set_night_atmosphere({}, false)
	session.audio.set_storm(false)

func _process(delta: float) -> void:
	if not active or not session.is_authority():
		return
	elapsed += delta
	var max_alive: int = int(session.data.NIGHTS["max_alive"])
	while queue.size() > 0 and float(queue[0]["at"]) <= elapsed:
		if session.alive_zombie_count() >= max_alive:
			queue[0]["at"] = float(queue[0]["at"]) + 1.5
			break
		var entry: Dictionary = queue.pop_front()
		var point: String = entry["point"]
		if point == "any":
			point = session.random_spawn_id()
		session.spawn_zombie(entry["type"], point)
		spawned += 1
	_run_events(delta)
	if queue.is_empty() and session.alive_zombie_count() == 0:
		active = false
		session.night_complete()

func _run_events(delta: float) -> void:
	var ev: String = cfg.get("event", "")
	if cfg.get("flicker", false) or ev == "power_flicker" or ev == "final_siege":
		flicker_t -= delta
		if flicker_t <= 0.0:
			flicker_on = not flicker_on
			flicker_t = randf_range(0.08, 0.5) if not flicker_on else randf_range(2.0, 7.0)
			for L in session.level.lights:
				L.visible = flicker_on
	elif not flicker_on:
		flicker_on = true
		for L in session.level.lights:
			L.visible = true
	if ev == "thunderstorm":
		pass # handled by AudioManager storm scheduling

func request_reinforcement(around: Vector3, count: int) -> void:
	if not active or not session.is_authority():
		return
	# find spawn nearest to the screamer/boss
	var best_id: String = session.random_spawn_id()
	var best_d := INF
	for id in session.level.spawn_points:
		var d: float = session.level.spawn_points[id].global_position.distance_to(around)
		if d < best_d:
			best_d = d
			best_id = id
	for i in count:
		var t := "runner" if i % 2 == 0 else "walker"
		session.spawn_zombie(t, best_id)

func progress_text() -> String:
	var remaining: int = queue.size() + session.alive_zombie_count()
	return "Hostile contacts remaining: %d" % remaining
