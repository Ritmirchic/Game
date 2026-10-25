extends Node
## Central Game Progression, 30-Anomaly Catalog, Story Mystery & Save/Settings System.

signal state_updated
signal notification_posted(message: String, accent_color: Color)
signal subtitle_triggered(speaker: String, text: String, duration: float)
signal ending_triggered(ending_id: String)

const SAVE_PATH := "user://dont_look_back_save.cfg"
const SETTINGS_PATH := "user://dont_look_back_settings.cfg"
const TARGET_CLEARANCE_CYCLES := 8

# Gameplay Progression State
var current_cycle: int = 0
var verified_cycles: int = 0
var total_loops: int = 0
var player_integrity: float = 100.0
var loop_instability: float = 0.0
var scanner_charges: int = 5
var max_scanner_charges: int = 5
var insight_points: int = 0
var correct_reports: int = 0
var false_reports: int = 0

# Mystery & Secret Ending Flags
var breaker_restored: bool = false
var sanctum_unlocked: bool = false
var master_override_key_taken: bool = false
var core_purged: bool = false
var pending_ending_id: String = ""

# Persistent & Session Collections
var discovered_anomalies: Dictionary = {}   # int(1..30) -> bool
var collected_documents: Dictionary = {}    # String("doc_01".."doc_08") -> bool
var played_tapes: Dictionary = {}           # String("tape_01".."tape_04") -> bool
var unlocked_endings: Dictionary = {}       # String("A"|"B"|"C") -> bool

# User Settings
var settings: Dictionary = {
	"brightness": 1.18,
	"mouse_sensitivity": 0.0022,
	"fov": 76.0,
	"master_volume": 0.88,
	"sfx_volume": 0.90,
	"ambience_volume": 0.78,
	"quality_preset": 2, # 0=Safe, 1=Medium, 2=High
	"fullscreen": false,
	"head_bob": true,
}

# ============================================================
# COMPLETE 30 ANOMALIES REGISTRY (Matches Section 6 1-to-1)
# ============================================================
const ANOMALY_CATALOG: Array[Dictionary] = [
	# VISUAL (1..10)
	{"id": 1, "code": "V-01", "category": "VISUAL", "room": "Main Corridor", "danger": 1,
	 "title": "Weeping Portrait Mutation", "hint": "Coastal lighthouse canvas warps into a hollow-eyed portrait tracking your gaze."},
	{"id": 2, "code": "V-02", "category": "VISUAL", "room": "Apartment 4A", "danger": 1,
	 "title": "Displaced Living Furniture", "hint": "Apartment 4A seating and table rearrange into an unnatural inward circle."},
	{"id": 3, "code": "V-03", "category": "VISUAL", "room": "Apartment 4A", "danger": 1,
	 "title": "Vanished Console Television", "hint": "The 1980s wood-veneer CRT television in Apartment 4A disappears completely."},
	{"id": 4, "code": "V-04", "category": "VISUAL", "room": "Main Corridor", "danger": 2,
	 "title": "Impossible Crimson Door", "hint": "A door that should not exist manifests flush in the West corridor plaster."},
	{"id": 5, "code": "V-05", "category": "VISUAL", "room": "Main Corridor", "danger": 1,
	 "title": "Damask Rot Wallpaper", "hint": "Striped beige corridor wallpaper mutates into damp crimson-ochre damask."},
	{"id": 6, "code": "V-06", "category": "VISUAL", "room": "Apartment 4A", "danger": 2,
	 "title": "Abyssal Window Horizon", "hint": "4th-floor night skyline is replaced by inverted monoliths in a crimson void."},
	{"id": 7, "code": "V-07", "category": "VISUAL", "room": "Main Corridor", "danger": 1,
	 "title": "Sodium-Cyan Hallway Shift", "hint": "Warm tungsten corridor sconces shift to cold flickering ultraviolet-cyan."},
	{"id": 8, "code": "V-08", "category": "VISUAL", "room": "Apartment 4B", "danger": 1,
	 "title": "Misplaced Commercial Washer", "hint": "A heavy laundry-room washing machine sits inside Apartment 4B's bedroom."},
	{"id": 9, "code": "V-09", "category": "VISUAL", "room": "Cross Hallway", "danger": 2,
	 "title": "Detached Swinging Troffer", "hint": "A ceiling fluorescent fixture hangs low off-center on twisted conduit."},
	{"id": 10, "code": "V-10", "category": "VISUAL", "room": "Apartment 4B", "danger": 2,
	 "title": "Osteoid Coat-Stand Morph", "hint": "The bentwood coat stand warps into a branching bone-like rib sculpture."},

	# AUDIO (11..18)
	{"id": 11, "code": "A-11", "category": "AUDIO", "room": "Main Corridor", "danger": 3,
	 "title": "Trailing Heel Footsteps", "hint": "Unseen footsteps echo two paces behind your heels whenever you walk."},
	{"id": 12, "code": "A-12", "category": "AUDIO", "room": "Apartment 4B", "danger": 2,
	 "title": "Wardrobe Chorus Whispers", "hint": "Layered hushed voices murmur from inside Apartment 4B's closed wardrobe."},
	{"id": 13, "code": "A-13", "category": "AUDIO", "room": "Storage Wing", "danger": 2,
	 "title": "Sealed Door Three-Beat Knock", "hint": "Urgent rhythmic knocking resonates from behind a locked storage door."},
	{"id": 14, "code": "A-14", "category": "AUDIO", "room": "Apartment 4C", "danger": 1,
	 "title": "Numbers-Station Interference", "hint": "Dr. Koslov's receiver erupts with heterodyne static and shortwave tones."},
	{"id": 15, "code": "A-15", "category": "AUDIO", "room": "Stairwell", "danger": 2,
	 "title": "Sub-Shaft Distant Scream", "hint": "A muffled human shriek reverberates up from the concrete stairwell shaft."},
	{"id": 16, "code": "A-16", "category": "AUDIO", "room": "Elevator Lobby", "danger": 2,
	 "title": "Phantom Hoistway Cables", "hint": "Elevator hoist cables groan and chime while the cab remains motionless."},
	{"id": 17, "code": "A-17", "category": "AUDIO", "room": "Main Corridor", "danger": 2,
	 "title": "Upper-Floor Pacing", "hint": "Heavy footsteps pace back and forth across the ceiling plaster overhead."},
	{"id": 18, "code": "A-18", "category": "AUDIO", "room": "Laundry Room", "danger": 2,
	 "title": "Corner Raspy Breathing", "hint": "Wet, close-range respiration emanates from an empty corner of Laundry."},

	# ENVIRONMENTAL (19..25)
	{"id": 19, "code": "E-19", "category": "ENVIRONMENTAL", "room": "Apartment 4A", "danger": 2,
	 "title": "Spontaneous Deadbolt Trap", "hint": "A normally open apartment door slams and locks unexpectedly behind you."},
	{"id": 20, "code": "E-20", "category": "ENVIRONMENTAL", "room": "North Corridor", "danger": 2,
	 "title": "Non-Euclidean Hallway Stretch", "hint": "The North spine corridor physically elongates by twelve extra meters."},
	{"id": 21, "code": "E-21", "category": "ENVIRONMENTAL", "room": "Apartment 4C", "danger": 2,
	 "title": "Mirrored Study Reconfiguration", "hint": "Dr. Koslov's acoustic study shifts into an inverted architectural layout."},
	{"id": 22, "code": "E-22", "category": "ENVIRONMENTAL", "room": "Elevator", "danger": 3,
	 "title": "Impossible Sub-Floor Arrival", "hint": "Elevator indicator reads -9 and opens onto a quarantined red landing."},
	{"id": 23, "code": "E-23", "category": "ENVIRONMENTAL", "room": "Complex-Wide", "danger": 2,
	 "title": "Crimson Bulkhead Lockdown", "hint": "Primary lamps dim as red emergency beacons pulse along every corridor."},
	{"id": 24, "code": "E-24", "category": "ENVIRONMENTAL", "room": "Cross Hallway", "danger": 2,
	 "title": "Shelving Barricade Obstruction", "hint": "Toppled cabinets and crates seal a familiar East hallway passage."},
	{"id": 25, "code": "E-25", "category": "ENVIRONMENTAL", "room": "East Crawlspace", "danger": 2,
	 "title": "Unsealed Surveillance Alcove", "hint": "A previously padlocked hidden wall door stands ominously ajar."},

	# ENTITY (26..30)
	{"id": 26, "code": "N-26", "category": "ENTITY", "room": "North Corridor", "danger": 3,
	 "title": "Corridor End Apparition", "hint": "A motionless dark figure stands beneath the far emergency exit sign."},
	{"id": 27, "code": "N-27", "category": "ENTITY", "room": "Laundry Room", "danger": 2,
	 "title": "Crossing Wall Shadow", "hint": "An elongated shadow figure strides across the room and vanishes."},
	{"id": 28, "code": "N-28", "category": "ENTITY", "room": "Apartment 4B", "danger": 3,
	 "title": "Fourth-Floor Window Watcher", "hint": "A pale long-necked creature presses against the exterior 4th-floor glass."},
	{"id": 29, "code": "N-29", "category": "ENTITY", "room": "Storage Wing", "danger": 3,
	 "title": "Threshold Gap Peeker", "hint": "Pale fingers and a sunken eye peer from behind a partially cracked door."},
	{"id": 30, "code": "N-30", "category": "ENTITY", "room": "Main Corridor", "danger": 3,
	 "title": "Silent Rear Stalker", "hint": "A lanky entity follows silently down hallways, freezing behind corners."},
]

# ============================================================
# HIDDEN DOCUMENTS & DISTORTED RECORDINGS
# ============================================================
const DOCUMENTS: Dictionary = {
	"doc_01": {
		"title": "Municipal Inspection Protocol #94-10",
		"location": "Threshold Security Desk",
		"body": "INSPECTOR JULIAN VANCE — MANDATORY SHIFT DIRECTIVE\n\nKestrel Court Building 4 is subject to recurring perceptual drift.\n\n1. Walk the 4th Floor, Cross-Hall, Laundry, Storage, and Basement.\n2. Cycle 0 is your BASELINE Calibration. Memorize normal furnishings.\n3. On Cycles 1-8: If everything is normal, pull the GREEN CLEARANCE LEVER at the South Threshold Desk.\n4. If ANY Anomaly is present, either aim at it and press [R] to LOG WITH FIELD SCANNER, and/or pull the RED CONTAINMENT LEVER.\n5. Eight verified cycles unlock the North Ground Emergency Exit."
	},
	"doc_02": {
		"title": "Superintendent Hollis's Shift Rules",
		"location": "Main Corridor Noticeboard",
		"body": "TO ALL NIGHT WATCH PERSONNEL:\n\n- Keep your flashlight [F] serviceable. The emergency ballasts hum at 60Hz when the floor is stable.\n- Four classes of distortion occur here: VISUAL, AUDIO, ENVIRONMENTAL, and LIVING ENTITIES.\n- If you hear heels matching your stride two paces behind you: DO NOT TURN AROUND IN THE NARROW SPINE. Scan and retreat to the Threshold."
	},
	"doc_03": {
		"title": "Dr. Helena Koslov — Perceptual Fixation Notes",
		"location": "Apartment 4C Acoustic Study",
		"body": "ACOUSTIC & QUANTUM OBSERVATION LOG 19-B\n\nSubject 'THE OBSERVER' exhibits macroscopic Zeno lock: while inside direct human line-of-sight and unoccluded by walls, its motor cortex freezes completely. Look away, blink behind a pillar, or shut a door, and it advances rapidly.\n\nConversely, 'THE LISTENER' in the lower shafts has no optic nerves. Crouch [Ctrl] to keep footsteps under 20 dB. Sprinting [Shift] rings through concrete like a dinner bell."
	},
	"doc_04": {
		"title": "Tenant Complaint Ledger — Apt 4D",
		"location": "Laundry Room Folding Table",
		"body": "OCT 14, 1994 — MRS. EVELYN GABLE:\n\nSomeone put an extra coat tree in the East Hall that twitches when the dryer stops. And the elevator bell chimed flat—415 Hz instead of E5—when nobody was inside the cab.\n\nHollis calls it 'THE IMITATOR.' It fakes safe cues to trick you into touching it or filing false reports. Keep 4 meters back and hit [R] on your Field Recorder."
	},
	"doc_05": {
		"title": "Maintenance Relay & Sub-Wall Latch Memo",
		"location": "Maintenance Room Workbench",
		"body": "ENGINEERING MEMO — ARTHUR HOLLIS\n\nMain Breaker Panel in Maintenance feeds the hidden magnetic latch in Dr. Koslov's North Study bookcase (Apartment 4C).\n\nStep 1: Throw the Maintenance Main Breaker ON.\nStep 2: Press the brass release behind Koslov's tall bookshelf in Apt 4C to enter the Architect's Sanctum."
	},
	"doc_06": {
		"title": "Forged Foundation Sign-Off (Oct 1994)",
		"location": "Storage Room Archive Shelf",
		"body": "CONFIDENTIAL MUNICIPAL ARCHIVE — SIGNED: J. VANCE\n\n'Sub-basement acoustic cavity poured and sealed per code.'\n\nHandwritten margin note in red ink:\nI lied. Commissioner Ward paid me twelve thousand dollars to sign the certificate while fourteen tenants were still sealed below Basement Level B. That is why the building will not let me leave."
	},
	"doc_07": {
		"title": "Resonance Containment Purge Schematic",
		"location": "Basement Boiler Room",
		"body": "CONTAINMENT CORE OVERRIDE PROCEDURE:\n\nEscaping out the Emergency Exit at Cycle 8 only frees one body while leaving Kestrel Court hungry.\n\nTo end the loop forever (TRUE ATONEMENT):\n1. Restore Maintenance Power.\n2. Open the Hidden Sanctum behind Apt 4C's Bookshelf.\n3. Claim the 1994 Master Override Key.\n4. Insert it into the Basement Boiler Resonance Core and survive the purge before exiting."
	},
	"doc_08": {
		"title": "Dr. Koslov's Final Testament",
		"location": "Hidden Architect's Sanctum",
		"body": "Julian—if you are reading this inside the Sanctum, this is your seventh or twentieth waking.\n\nYou have three choices now:\n- Complete 8 verified inspections and walk out into the cold morning.\n- Take my Master Override Key down to the Basement Boiler Core to collapse the cavity for good.\n- Or gaze into the Threshold Mirror beside this desk and take my place as Tenant 405, sparing everyone outside."
	},
}

const TAPE_LOGS: Dictionary = {
	"tape_01": {
		"title": "Microcassette #01 — 'Three Loops Ago'",
		"speaker": "JULIAN VANCE (TAPE)",
		"audio": "res://assets/audio/tape_log_01.wav",
		"transcript": "[Click] Julian—listen to your own voice! Do not trust an untested corridor. Calibrate on Cycle 0, watch the coastal lighthouse painting, and never sprint past the stairwell!"
	},
	"tape_02": {
		"title": "Microcassette #02 — 'Ventilation Echoes'",
		"speaker": "DR. HELENA KOSLOV",
		"audio": "res://assets/audio/tape_log_02.wav",
		"transcript": "[Static] It hears decibels, not light. Crouch when you hear dry bone-clicks in the hall. And if the tall Pale Inspector manifests—keep your beam on its chest and back away slowly!"
	},
	"tape_03": {
		"title": "Microcassette #03 — 'Welding the Hatch'",
		"speaker": "SUPT. ARTHUR HOLLIS",
		"audio": "res://assets/audio/tape_log_03.wav",
		"transcript": "[Metallic hum] I wired the hidden sanctum latch through the Maintenance breaker. If Vance ever comes back to make right what he signed in '94, the Master Key is behind Koslov's shelves."
	},
	"tape_04": {
		"title": "Microcassette #04 — 'October 25, 1994'",
		"speaker": "BLACK-BOX RECORDING",
		"audio": "res://assets/audio/tape_log_04.wav",
		"transcript": "[Heavy warble] Sub-basement pressure critical. The walls are remembering us. Either purge the Boiler Core with the Master Key... or step into the Mirror and stay forever."
	},
}


func _ready() -> void:
	_ensure_input_bindings()
	load_settings()
	load_persistent_unlocks()


func _ensure_input_bindings() -> void:
	var key_map := {
		"move_forward": [KEY_W, KEY_UP],
		"move_back": [KEY_S, KEY_DOWN],
		"move_left": [KEY_A, KEY_LEFT],
		"move_right": [KEY_D, KEY_RIGHT],
		"sprint": [KEY_SHIFT],
		"crouch": [KEY_CTRL, KEY_C],
		"interact": [KEY_E],
		"flashlight": [KEY_F],
		"scan_report": [KEY_R],
		"journal": [KEY_J, KEY_TAB],
		"pause": [KEY_ESCAPE],
	}
	for action in key_map.keys():
		if not InputMap.has_action(action):
			InputMap.add_action(action)
		InputMap.action_erase_events(action)
		for kc in key_map[action]:
			var ev := InputEventKey.new()
			ev.physical_keycode = kc
			InputMap.action_add_event(action, ev)

	# Right mouse button also toggles flashlight comfortably
	var mb_flash := InputEventMouseButton.new()
	mb_flash.button_index = MOUSE_BUTTON_RIGHT
	InputMap.action_add_event("flashlight", mb_flash)


func start_new_run() -> void:
	current_cycle = 0
	verified_cycles = 0
	total_loops = 0
	player_integrity = 100.0
	loop_instability = 0.0
	scanner_charges = max_scanner_charges
	insight_points = 0
	correct_reports = 0
	false_reports = 0
	breaker_restored = false
	sanctum_unlocked = false
	master_override_key_taken = false
	core_purged = false
	pending_ending_id = ""
	state_updated.emit()


func get_anomaly_meta(anomaly_id: int) -> Dictionary:
	for entry in ANOMALY_CATALOG:
		if int(entry["id"]) == anomaly_id:
			return entry
	return {}


func mark_anomaly_discovered(anomaly_id: int) -> void:
	var is_new: bool = not bool(discovered_anomalies.get(anomaly_id, false))
	discovered_anomalies[anomaly_id] = true
	correct_reports += 1
	insight_points += 25 if is_new else 10
	scanner_charges = mini(max_scanner_charges, scanner_charges + 1)
	loop_instability = maxf(0.0, loop_instability - 8.0)
	save_persistent_unlocks()
	state_updated.emit()


func apply_false_report_penalty(reason: String) -> void:
	false_reports += 1
	scanner_charges = maxi(0, scanner_charges - 1)
	loop_instability = minf(100.0, loop_instability + 18.0)
	player_integrity = maxf(10.0, player_integrity - 10.0)
	notification_posted.emit("FALSE REPORT PENALTY: " + reason, Color(0.92, 0.28, 0.24))
	state_updated.emit()


func collect_document(doc_id: String) -> void:
	if not DOCUMENTS.has(doc_id):
		return
	if not collected_documents.get(doc_id, false):
		collected_documents[doc_id] = true
		insight_points += 15
		notification_posted.emit("DOCUMENT ARCHIVED: " + DOCUMENTS[doc_id]["title"], Color(0.92, 0.78, 0.45))
		save_persistent_unlocks()
		state_updated.emit()


func mark_tape_played(tape_id: String) -> void:
	if not TAPE_LOGS.has(tape_id):
		return
	if not played_tapes.get(tape_id, false):
		played_tapes[tape_id] = true
		insight_points += 20
		save_persistent_unlocks()
	var t: Dictionary = TAPE_LOGS[tape_id]
	subtitle_triggered.emit(t["speaker"], t["transcript"], 6.5)
	state_updated.emit()


func damage_player(amount: float, cause: String) -> void:
	player_integrity = maxf(0.0, player_integrity - amount)
	loop_instability = minf(100.0, loop_instability + amount * 0.6)
	notification_posted.emit(cause, Color(0.95, 0.22, 0.20))
	state_updated.emit()
	if player_integrity <= 0.0:
		# High corruption forces Ending C if repeated, or resets current cycle with penalty
		if loop_instability >= 85.0 and total_loops >= 3:
			trigger_ending("C")
		else:
			player_integrity = 65.0
			verified_cycles = maxi(0, verified_cycles - 1)
			notification_posted.emit("CONSCIOUSNESS FRACTURED — CYCLE RESET TO " + str(verified_cycles), Color(0.95, 0.30, 0.25))
			state_updated.emit()


func trigger_ending(ending_id: String) -> void:
	pending_ending_id = ending_id
	unlocked_endings[ending_id] = true
	save_persistent_unlocks()
	ending_triggered.emit(ending_id)


func save_game(player_pos: Vector3 = Vector3.ZERO, player_yaw: float = 0.0) -> bool:
	var cfg := ConfigFile.new()
	cfg.set_value("run", "current_cycle", current_cycle)
	cfg.set_value("run", "verified_cycles", verified_cycles)
	cfg.set_value("run", "total_loops", total_loops)
	cfg.set_value("run", "player_integrity", player_integrity)
	cfg.set_value("run", "loop_instability", loop_instability)
	cfg.set_value("run", "scanner_charges", scanner_charges)
	cfg.set_value("run", "insight_points", insight_points)
	cfg.set_value("run", "breaker_restored", breaker_restored)
	cfg.set_value("run", "sanctum_unlocked", sanctum_unlocked)
	cfg.set_value("run", "master_override_key_taken", master_override_key_taken)
	cfg.set_value("run", "core_purged", core_purged)
	cfg.set_value("run", "player_pos", player_pos)
	cfg.set_value("run", "player_yaw", player_yaw)
	cfg.set_value("unlocks", "anomalies", discovered_anomalies)
	cfg.set_value("unlocks", "documents", collected_documents)
	cfg.set_value("unlocks", "tapes", played_tapes)
	cfg.set_value("unlocks", "endings", unlocked_endings)
	return cfg.save(SAVE_PATH) == OK


func has_save_file() -> bool:
	return FileAccess.file_exists(SAVE_PATH)


func load_game() -> Dictionary:
	var cfg := ConfigFile.new()
	if cfg.load(SAVE_PATH) != OK:
		return {}
	current_cycle = int(cfg.get_value("run", "current_cycle", 0))
	verified_cycles = int(cfg.get_value("run", "verified_cycles", 0))
	total_loops = int(cfg.get_value("run", "total_loops", 0))
	player_integrity = float(cfg.get_value("run", "player_integrity", 100.0))
	loop_instability = float(cfg.get_value("run", "loop_instability", 0.0))
	scanner_charges = int(cfg.get_value("run", "scanner_charges", 5))
	insight_points = int(cfg.get_value("run", "insight_points", 0))
	breaker_restored = bool(cfg.get_value("run", "breaker_restored", false))
	sanctum_unlocked = bool(cfg.get_value("run", "sanctum_unlocked", false))
	master_override_key_taken = bool(cfg.get_value("run", "master_override_key_taken", false))
	core_purged = bool(cfg.get_value("run", "core_purged", false))
	discovered_anomalies = cfg.get_value("unlocks", "anomalies", discovered_anomalies)
	collected_documents = cfg.get_value("unlocks", "documents", collected_documents)
	played_tapes = cfg.get_value("unlocks", "tapes", played_tapes)
	unlocked_endings = cfg.get_value("unlocks", "endings", unlocked_endings)
	state_updated.emit()
	return {
		"player_pos": cfg.get_value("run", "player_pos", Vector3(-5.5, 0.1, 9.0)),
		"player_yaw": float(cfg.get_value("run", "player_yaw", 0.0)),
	}


func save_persistent_unlocks() -> void:
	var cfg := ConfigFile.new()
	if FileAccess.file_exists(SAVE_PATH):
		cfg.load(SAVE_PATH)
	cfg.set_value("unlocks", "anomalies", discovered_anomalies)
	cfg.set_value("unlocks", "documents", collected_documents)
	cfg.set_value("unlocks", "tapes", played_tapes)
	cfg.set_value("unlocks", "endings", unlocked_endings)
	cfg.save(SAVE_PATH)


func load_persistent_unlocks() -> void:
	var cfg := ConfigFile.new()
	if cfg.load(SAVE_PATH) == OK:
		discovered_anomalies = cfg.get_value("unlocks", "anomalies", {})
		collected_documents = cfg.get_value("unlocks", "documents", {})
		played_tapes = cfg.get_value("unlocks", "tapes", {})
		unlocked_endings = cfg.get_value("unlocks", "endings", {})


func save_settings() -> void:
	var cfg := ConfigFile.new()
	for k in settings.keys():
		cfg.set_value("settings", k, settings[k])
	cfg.save(SETTINGS_PATH)
	apply_display_and_audio_settings()


func load_settings() -> void:
	var cfg := ConfigFile.new()
	if cfg.load(SETTINGS_PATH) == OK:
		for k in settings.keys():
			settings[k] = cfg.get_value("settings", k, settings[k])
	apply_display_and_audio_settings()


func apply_display_and_audio_settings() -> void:
	var master_idx := AudioServer.get_bus_index("Master")
	if master_idx >= 0:
		var mv: float = clampf(float(settings.get("master_volume", 0.88)), 0.001, 1.0)
		AudioServer.set_bus_volume_db(master_idx, linear_to_db(mv))
	if DisplayServer.get_name() != "headless":
		var want_fs := bool(settings.get("fullscreen", false))
		var cur_mode := DisplayServer.window_get_mode()
		if want_fs and cur_mode != DisplayServer.WINDOW_MODE_FULLSCREEN:
			DisplayServer.window_set_mode(DisplayServer.WINDOW_MODE_FULLSCREEN)
		elif (not want_fs) and cur_mode == DisplayServer.WINDOW_MODE_FULLSCREEN:
			DisplayServer.window_set_mode(DisplayServer.WINDOW_MODE_WINDOWED)
	state_updated.emit()
