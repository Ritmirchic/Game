class_name GameState
extends RefCounted
const Data = preload("res://scripts/data.gd")
const Saves = preload("res://scripts/save_system.gd")

signal changed
signal achievement_unlocked(id: String)
signal level_reached(level: int)

var profile: Dictionary

func _init(source: Dictionary = {}) -> void:
	profile = Saves.default_profile() if source.is_empty() else source.duplicate(true)

func get_stats() -> Dictionary:
	var ship: Dictionary = Data.get_ship(profile.current_ship)
	var u: Dictionary = profile.upgrades
	var laser := float(ship.laser) * (1.0 + float(u.laser_power) * 0.24) * (1.0 + float(u.mining_speed) * 0.10)
	return {
		"max_hull":float(ship.hull) + float(u.hull) * 25.0,
		"max_shield":float(ship.shield) + float(u.shield) * 18.0,
		"max_energy":float(ship.energy) + float(u.reactor) * 20.0,
		"cargo":float(ship.cargo) + float(u.cargo_capacity) * 24.0,
		"speed":float(ship.speed) * (1.0 + float(u.engine_speed) * 0.085),
		"acceleration":float(ship.accel) * (1.0 + float(u.acceleration) * 0.12),
		"laser":laser,
		"mine_range":500.0 + float(u.mining_speed) * 22.0,
		"energy_regen":18.0 + float(u.energy_regen) * 3.2,
		"shield_regen":4.0 + float(u.shield) * 0.65,
		"scanner":750.0 + float(u.scanner) * 160.0,
		"drone_slots":int(u.drone_capacity),
		"weapon":float(ship.weapon) + float(u.weapon_damage) * 6.0,
		"fire_cooldown":maxf(0.12, 0.34 - float(u.weapon_damage) * 0.012),
		"collection":105.0 + float(u.auto_collection) * 90.0,
		"armor":minf(0.42, float(u.armor) * 0.045),
		"nano_repair":float(u.armor) * 0.12,
		"drone_power":12.0 * (1.0 + float(u.drone_mining) * 0.22),
		"drone_speed":190.0 * (1.0 + float(u.drone_mining) * 0.08),
		"drone_cargo":12 + int(u.drone_mining) * 3,
		"xp_multiplier":1.0 + float(u.research) * 0.10,
		"rare_chance":float(u.research) * 0.018
	}

func cargo_total() -> int:
	var total := 0
	for value in profile.resources.values():
		total += int(value)
	return total

func cargo_free() -> int:
	return maxi(0, int(get_stats().cargo) - cargo_total())

func add_resource(id: String, amount: int) -> int:
	if Data.resource_index(id) < 0 or amount <= 0:
		return 0
	var accepted := mini(amount, cargo_free())
	if accepted <= 0:
		return 0
	profile.resources[id] = int(profile.resources.get(id, 0)) + accepted
	if id not in profile.discovered:
		profile.discovered.append(id)
	profile.stats.mined_units = int(profile.stats.mined_units) + accepted
	var resource := Data.get_resource(id)
	if resource.rarity >= 2:
		add_xp(int(resource.rarity) * accepted * 2)
	if id == "lumen_crystal":
		unlock_achievement("crystal")
	if id == "void_crystal":
		unlock_achievement("void")
	changed.emit()
	return accepted

func sell_cargo() -> int:
	var value := 0
	for resource in Data.RESOURCES:
		var amount := int(profile.resources[resource.id])
		value += amount * int(resource.value)
		profile.resources[resource.id] = 0
	if value > 0:
		profile.credits = int(profile.credits) + value
		profile.stats.earned = int(profile.stats.earned) + value
		unlock_achievement("first_sale")
		if int(profile.credits) >= 25000:
			unlock_achievement("wealth")
		add_xp(maxi(1, int(value / 35)))
		changed.emit()
	return value

func buy_upgrade(id: String) -> bool:
	var upgrade := Data.get_upgrade(id)
	if upgrade.is_empty():
		return false
	var level := int(profile.upgrades[id])
	if level >= int(upgrade.max):
		return false
	var cost := Data.upgrade_cost(id, level)
	if int(profile.credits) < cost:
		return false
	profile.credits = int(profile.credits) - cost
	profile.upgrades[id] = level + 1
	changed.emit()
	return true

func sector_status(index: int) -> Dictionary:
	if index < 0 or index >= Data.SECTORS.size():
		return {"unlocked":false, "can_unlock":false, "reason":"UNKNOWN SECTOR"}
	var sector: Dictionary = Data.SECTORS[index]
	if index in profile.unlocked_sectors:
		return {"unlocked":true, "can_unlock":false, "reason":"CHARTED"}
	if int(profile.level) < int(sector.level):
		return {"unlocked":false, "can_unlock":false, "reason":"REQUIRES PILOT LEVEL %d" % int(sector.level)}
	if int(profile.credits) < int(sector.cost):
		return {"unlocked":false, "can_unlock":false, "reason":"REQUIRES %s CR" % format_number(int(sector.cost))}
	return {"unlocked":false, "can_unlock":true, "reason":"READY TO CHART"}

func unlock_sector(index: int) -> bool:
	var status := sector_status(index)
	if not status.can_unlock:
		return false
	profile.credits = int(profile.credits) - int(Data.SECTORS[index].cost)
	profile.unlocked_sectors.append(index)
	profile.unlocked_sectors.sort()
	if profile.unlocked_sectors.size() == Data.SECTORS.size():
		unlock_achievement("frontier")
	changed.emit()
	return true

func travel_sector(index: int) -> bool:
	if index not in profile.unlocked_sectors:
		return false
	profile.current_sector = index
	changed.emit()
	return true

func buy_ship(id: String) -> bool:
	var ship := Data.get_ship(id)
	if ship.id != id or id in profile.unlocked_ships:
		return false
	if int(profile.level) < int(ship.level) or int(profile.credits) < int(ship.cost):
		return false
	profile.credits = int(profile.credits) - int(ship.cost)
	profile.unlocked_ships.append(id)
	changed.emit()
	return true

func select_ship(id: String) -> bool:
	if id not in profile.unlocked_ships:
		return false
	profile.current_ship = id
	changed.emit()
	return true

func drone_cost() -> int:
	return int(1100 * (1.0 + int(profile.drones) * 0.65))

func buy_drone() -> bool:
	var stats := get_stats()
	if int(profile.drones) >= int(stats.drone_slots) or int(profile.drones) >= 6:
		return false
	var cost := drone_cost()
	if int(profile.credits) < cost:
		return false
	profile.credits = int(profile.credits) - cost
	profile.drones = int(profile.drones) + 1
	unlock_achievement("automation")
	if int(profile.drones) >= 4:
		unlock_achievement("fleet")
	changed.emit()
	return true

func add_credits(amount: int) -> void:
	profile.credits = maxi(0, int(profile.credits) + amount)
	if int(profile.credits) >= 25000:
		unlock_achievement("wealth")
	changed.emit()

func add_xp(amount: int) -> void:
	if amount <= 0:
		return
	var adjusted := maxi(1, int(round(float(amount) * float(get_stats().xp_multiplier))))
	profile.xp = int(profile.xp) + adjusted
	profile.stats.total_xp = int(profile.stats.total_xp) + adjusted
	while int(profile.level) < 99 and int(profile.xp) >= Data.xp_for_level(int(profile.level)):
		profile.xp = int(profile.xp) - Data.xp_for_level(int(profile.level))
		profile.level = int(profile.level) + 1
		level_reached.emit(int(profile.level))
	changed.emit()

func record_asteroid() -> void:
	profile.stats.asteroids = int(profile.stats.asteroids) + 1
	unlock_achievement("first_cut")
	if int(profile.stats.asteroids) >= 25:
		unlock_achievement("prospector")
	if int(profile.stats.asteroids) >= 100:
		unlock_achievement("industrialist")
	add_xp(28 + int(profile.current_sector) * 12)

func record_kill() -> void:
	profile.stats.kills = int(profile.stats.kills) + 1
	unlock_achievement("first_blood")
	if int(profile.stats.kills) >= 25:
		unlock_achievement("veteran")

func unlock_achievement(id: String) -> void:
	if id in profile.achievements:
		return
	for achievement in Data.ACHIEVEMENTS:
		if achievement.id == id:
			profile.achievements.append(id)
			achievement_unlocked.emit(id)
			changed.emit()
			return

func save() -> bool:
	return Saves.save_profile(profile)

static func format_number(value: int) -> String:
	if value >= 1000000:
		return "%.2fM" % (float(value) / 1000000.0)
	if value >= 10000:
		return "%.1fK" % (float(value) / 1000.0)
	return str(value)
