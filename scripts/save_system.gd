class_name SaveSystem
extends RefCounted

const SAVE_PATH := "user://space_miner_profile.json"
const BACKUP_PATH := "user://space_miner_profile.backup.json"
const TEMP_PATH := "user://space_miner_profile.tmp"

static func default_profile() -> Dictionary:
	var upgrades := {}
	for upgrade in GameData.UPGRADES:
		upgrades[upgrade.id] = 0
	return {
		"version": GameData.SAVE_VERSION,
		"created": int(Time.get_unix_time_from_system()),
		"updated": int(Time.get_unix_time_from_system()),
		"credits": 450,
		"level": 1,
		"xp": 0,
		"resources": GameData.empty_resource_map(),
		"upgrades": upgrades,
		"unlocked_ships": ["prospector"],
		"current_ship": "prospector",
		"unlocked_sectors": [0],
		"current_sector": 0,
		"drones": 0,
		"drones_active": true,
		"achievements": [],
		"discovered": [],
		"stats": {"asteroids":0, "kills":0, "earned":0, "mined_units":0, "deaths":0, "play_seconds":0, "total_xp":0},
		"settings": {"master":0.8, "music":0.42, "sfx":0.8, "screen_shake":true, "particles":true, "show_hints":true, "autosave":true, "difficulty":1.0}
	}

static func save_profile(profile: Dictionary) -> bool:
	var payload := profile.duplicate(true)
	payload.version = GameData.SAVE_VERSION
	payload.updated = int(Time.get_unix_time_from_system())
	var payload_text := JSON.stringify(payload)
	var wrapper := {"version":GameData.SAVE_VERSION, "payload":payload_text, "checksum":payload_text.sha256_text()}
	var file := FileAccess.open(TEMP_PATH, FileAccess.WRITE)
	if file == null:
		push_error("Unable to open temporary save file")
		return false
	file.store_string(JSON.stringify(wrapper, "\t"))
	file.flush()
	file.close()
	if FileAccess.file_exists(SAVE_PATH):
		DirAccess.copy_absolute(SAVE_PATH, BACKUP_PATH)
	var error := DirAccess.rename_absolute(TEMP_PATH, SAVE_PATH)
	if error != OK:
		push_error("Unable to commit save: %s" % error)
		return false
	return true

static func load_profile() -> Dictionary:
	if not FileAccess.file_exists(SAVE_PATH):
		return {"profile":default_profile(), "recovered":false, "message":"New flight profile created."}
	var result := _read_save(SAVE_PATH)
	if not result.is_empty():
		return {"profile":result, "recovered":false, "message":"Flight profile restored."}
	# A valid backup is preferred over silently wiping progress.
	if FileAccess.file_exists(BACKUP_PATH):
		result = _read_save(BACKUP_PATH)
		if not result.is_empty():
			return {"profile":result, "recovered":true, "message":"Primary save was damaged; backup restored."}
	if FileAccess.file_exists(SAVE_PATH):
		DirAccess.copy_absolute(SAVE_PATH, SAVE_PATH + ".corrupt")
	return {"profile":default_profile(), "recovered":true, "message":"Damaged save quarantined. A safe profile was created."}

static func _read_save(path: String) -> Dictionary:
	var file := FileAccess.open(path, FileAccess.READ)
	if file == null:
		return {}
	var text := file.get_as_text()
	var json := JSON.new()
	if json.parse(text) != OK or not json.data is Dictionary:
		return {}
	var wrapper: Dictionary = json.data
	if not wrapper.has("payload") or not wrapper.has("checksum") or not wrapper.payload is String:
		return {}
	if String(wrapper.payload).sha256_text() != str(wrapper.checksum):
		return {}
	var payload_json := JSON.new()
	if payload_json.parse(String(wrapper.payload)) != OK or not payload_json.data is Dictionary:
		return {}
	var profile: Dictionary = payload_json.data
	if not _valid_shape(profile):
		return {}
	return _normalize(profile)

static func _valid_shape(p: Dictionary) -> bool:
	var required := ["credits","level","xp","resources","upgrades","unlocked_ships","current_ship","unlocked_sectors","drones"]
	for key in required:
		if not p.has(key):
			return false
	if not p.resources is Dictionary or not p.upgrades is Dictionary:
		return false
	if not p.unlocked_ships is Array or not p.unlocked_sectors is Array:
		return false
	if float(p.credits) < 0 or int(p.level) < 1 or int(p.drones) < 0:
		return false
	for resource in GameData.RESOURCES:
		if not p.resources.has(resource.id):
			return false
		if float(p.resources[resource.id]) < 0:
			return false
	for upgrade in GameData.UPGRADES:
		if not p.upgrades.has(upgrade.id):
			return false
		if int(p.upgrades[upgrade.id]) < 0 or int(p.upgrades[upgrade.id]) > int(upgrade.max):
			return false
	return true

static func _normalize(p: Dictionary) -> Dictionary:
	var d := default_profile()
	d.created = int(p.get("created", d.created))
	d.updated = int(p.get("updated", d.updated))
	d.credits = clampi(int(p.credits), 0, 2000000000)
	d.level = clampi(int(p.level), 1, 99)
	d.xp = clampi(int(p.xp), 0, 2000000000)
	for resource in GameData.RESOURCES:
		d.resources[resource.id] = clampi(int(p.resources[resource.id]), 0, 1000000000)
	for upgrade in GameData.UPGRADES:
		d.upgrades[upgrade.id] = clampi(int(p.upgrades[upgrade.id]), 0, int(upgrade.max))
	for ship in GameData.SHIPS:
		if ship.id in p.unlocked_ships:
			d.unlocked_ships.append(ship.id)
	if d.unlocked_ships.is_empty():
		d.unlocked_ships = ["prospector"]
	d.current_ship = p.current_ship if p.current_ship in d.unlocked_ships else "prospector"
	for sector in p.unlocked_sectors:
		var index := int(sector)
		if index >= 0 and index < GameData.SECTORS.size() and index not in d.unlocked_sectors:
			d.unlocked_sectors.append(index)
	d.current_sector = clampi(int(p.get("current_sector", 0)), 0, GameData.SECTORS.size() - 1)
	if d.current_sector not in d.unlocked_sectors:
		d.current_sector = 0
	d.drones = clampi(int(p.drones), 0, 6)
	d.drones_active = bool(p.get("drones_active", true))
	var valid_achievements := []
	for achievement in GameData.ACHIEVEMENTS:
		valid_achievements.append(achievement.id)
	for id in p.get("achievements", []):
		if id in valid_achievements and id not in d.achievements:
			d.achievements.append(id)
	for resource in GameData.RESOURCES:
		if resource.id in p.get("discovered", []) and resource.id not in d.discovered:
			d.discovered.append(resource.id)
	for key in d.stats.keys():
		d.stats[key] = maxi(0, int(p.get("stats", {}).get(key, 0)))
	for key in d.settings.keys():
		if p.get("settings", {}).has(key):
			d.settings[key] = p.settings[key]
	d.settings.master = clampf(float(d.settings.master), 0.0, 1.0)
	d.settings.music = clampf(float(d.settings.music), 0.0, 1.0)
	d.settings.sfx = clampf(float(d.settings.sfx), 0.0, 1.0)
	d.settings.difficulty = clampf(float(d.settings.difficulty), 0.7, 1.5)
	return d
