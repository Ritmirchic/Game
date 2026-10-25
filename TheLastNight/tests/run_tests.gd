extends SceneTree
## Headless integration test harness. Run with:
##   godot --headless --path . -s tests/run_tests.gd
## Exercises data, navigation, building, combat, AI, waves, save and UI.

const GameDataScript := preload("res://scripts/game_data.gd")
const AssetLibScript := preload("res://scripts/asset_lib.gd")
const UIScript := preload("res://scripts/ui/ui_manager.gd")
const SessionScript := preload("res://scripts/session.gd")

var passed := 0
var failed := 0
var failures: Array[String] = []
var data: GameData
var assets
var ui
var session: Session

func check(label: String, cond: bool) -> void:
	if cond:
		passed += 1
		print("  PASS  ", label)
	else:
		failed += 1
		failures.append(label)
		push_error("FAIL  " + label)
		print("  FAIL  ", label)

func _initialize() -> void:
	call_deferred("run")

func run() -> void:
	print("===== THE LAST NIGHT — TEST HARNESS =====")
	InputSetup.define_all()
	data = GameDataScript.new()
	root.add_child(data)
	assets = AssetLibScript.new()
	ui = UIScript.new(assets)
	root.add_child(ui)
	session = SessionScript.new()
	root.add_child(session)
	ui.session = session
	session.boot(data, ui, "single", "")
	await process_frame
	await process_frame
	_test_data()
	_test_assets()
	_test_navigation()
	_test_barricades()
	_test_loot()
	await _test_zombie_ai()
	_test_combat()
	_test_bosses()
	await _test_campaign()
	_test_save()
	_test_ui()
	_finish()

# --------------------------------------------------------------------- data
func _test_data() -> void:
	print("[1] design data")
	check("7 weapons defined", data.WEAPONS.size() == 7)
	check("7 actor types (5 zombies + 2 bosses)", data.ZOMBIES.size() == 7)
	check("10 nights", data.NIGHTS["nights"].size() == 10)
	for n in data.NIGHTS["nights"]:
		var mobs := 0
		for p in n["pulses"]:
			for m in p["mobs"]:
				mobs += int(m["count"])
		check("night %d spawns mobs" % n["id"], mobs > 0)
	check("night 5 has mini-boss", _night_has(5, "hulk"))
	check("night 10 has final boss", _night_has(10, "overlord"))
	check("shop has 12+ items", data.SHOP["items"].size() >= 12)

func _night_has(nid: int, mob: String) -> bool:
	for n in data.NIGHTS["nights"]:
		if int(n["id"]) != nid:
			continue
		for p in n["pulses"]:
			for m in p["mobs"]:
				if m["type"] == mob:
					return true
	return false

# ------------------------------------------------------------------- assets
func _test_assets() -> void:
	print("[2] generated assets load")
	var tex_dir := DirAccess.open("res://assets/textures")
	var t_count := 0
	for f in tex_dir.get_files():
		if f.ends_with(".import") or f.begins_with("."):
			continue
		check("texture " + f, load("res://assets/textures/" + f) != null)
		t_count += 1
	check("40+ texture files", t_count >= 40)
	var snd_dir := DirAccess.open("res://assets/sounds")
	var s_count := 0
	for f in snd_dir.get_files():
		if f.ends_with(".import") or f.begins_with("."):
			continue
		var s: AudioStream = load("res://assets/sounds/" + f)
		check("sound " + f, s != null)
		s_count += 1
	check("25+ sound files", s_count >= 25)

# --------------------------------------------------------------------- nav
func _test_navigation() -> void:
	print("[3] navigation / level connectivity")
	var nav = session.level.nav
	check("nav has 300+ points", nav.astar.get_point_count() >= 300)
	var spawn: Vector3 = session.level.player_spawn.global_position
	for id in session.level.spawn_points:
		var p: Vector3 = session.level.spawn_points[id].global_position
		var path: Array = nav.find_path(p, spawn)
		check("spawn %s routes into store" % id, path.size() > 0)
	for b in session.level.barricades.values():
		var path: Array = nav.find_path(spawn, b.global_position)
		check("barricade %s reachable" % b.door["id"], path.size() > 0)
	for room in data.LAYOUT["rooms"]:
		var r: Array = room["rect"]
		var center := data.cell_to_world(int((r[0] + r[2]) / 2), int((r[1] + r[3]) / 2))
		check("room reachable: " + room["name"], nav.find_path(spawn, center).size() > 0)

# --------------------------------------------------------------- barricades
func _test_barricades() -> void:
	print("[4] barricade build / damage / upgrade")
	var b: Barricade = session.level.barricades["main_entrance"]
	var p: Player = session.local_player
	session.resources["materials"] = 60
	session.resources["cash"] = 2000
	b.interact_authority(p)
	check("wood barricade built", b.level == 1 and b.hp > 0)
	check("door cells closed in nav", not session.level.nav.is_walkable_cell(Vector2i(14, 0)))
	var hp_before: float = b.hp
	b.take_damage(40)
	check("barricade takes damage", b.hp < hp_before)
	b.interact_authority(p)
	check("barricade repaired", absf(b.hp - b.max_hp) < 0.01)
	b.interact_authority(p)
	check("upgraded to reinforced", b.level == 2)
	b.interact_authority(p)
	check("upgraded to metal", b.level == 3)
	check("trap placeable", b.trap_uses == 0)
	b.interact_authority(p)
	check("trap armed", b.trap_uses > 0)
	var dmg: float = b.trigger_trap()
	check("trap damages", dmg > 0 and b.trap_uses == 7)
	# reset for later AI test
	b.load_state({"level": 0, "hp": 0, "max_hp": 1, "trap": 0, "door": "main_entrance"})

# --------------------------------------------------------------------- loot
func _test_loot() -> void:
	print("[5] loot search")
	var crate: LootCrate = session.level.interactables.get_child(0) as LootCrate
	check("loot crate exists", crate != null)
	var before := _resource_total()
	session.local_player.global_position = crate.global_position + Vector3(0.8, 0.0, 0.0)
	crate.interact_authority(session.local_player)
	for i in 120:
		await physics_frame
	check("crate searched", crate.searched and not crate.visible)
	check("granted resources", _resource_total() > before)

func _resource_total() -> int:
	var r: Dictionary = session.resources
	var total: int = int(r["cash"]) + int(r["materials"]) + int(r["medkits"])
	for k in r["ammo"]:
		total += int(r["ammo"][k])
	return total

# ---------------------------------------------------------------- zombie AI
func _test_zombie_ai() -> void:
	print("[6] zombie AI for all types")
	for ztype in ["walker", "runner", "crawler", "brute", "screamer"]:
		var z: Zombie = session._create_zombie(1000 + data.ZOMBIES.keys().find(ztype), ztype, "sp_main")
		z.rise_t = 0.01
		z.state = Zombie.State.CHASE
		var start := z.global_position
		for i in 520:
			await physics_frame
		var moved: float = start.distance_to(z.global_position)
		check("%s navigates toward player" % ztype, moved > 2.5)
		var close: bool = z.global_position.distance_to(session.local_player.global_position) < 4.5
		check("%s reaches attack range" % ztype, close)
		z.take_damage(99999, z.global_position)
		check("%s dies" % ztype, z.state == Zombie.State.DEAD)
		session.zombies.erase(z)
		z.queue_free()
	# barricade interaction
	print("[7] zombies attack barricades, not through walls")
	var b: Barricade = session.level.barricades["loading_dock"]
	b.load_state({"level": 1, "hp": 220, "max_hp": 220, "trap": 0, "door": "loading_dock"})
	var z: Zombie = session._create_zombie(2001, "walker", "sp_dock")
	z.rise_t = 0.01
	z.state = Zombie.State.CHASE
	var hp0: float = b.hp
	for i in 600:
		await physics_frame
		if b.hp < hp0:
			break
	check("barricade damaged by zombie", b.hp < hp0)
	session.zombies.erase(z)
	z.queue_free()
	b.load_state({"level": 0, "hp": 0, "max_hp": 1, "trap": 0, "door": "loading_dock"})

# ------------------------------------------------------------------ combat
func _test_combat() -> void:
	print("[8] weapon combat")
	var p: Player = session.local_player
		" zombies_alive=", session.zombies.size(), " pos=", p.global_position)
	# give all weapons
	for wid in data.WEAPONS.keys():
		if not (wid in p.owned):
			p.add_weapon(wid)
	for wid in data.WEAPONS.keys():
		p.current = p.owned.find(wid)
		p._equip_viewmodel()
		var w: Dictionary = data.WEAPONS[wid]
		if w["kind"] == "gun":
			var at: String = w["ammo_type"]
			session.resources["ammo"][at] = 999
			p._reset_mag(wid)
		# target zombie in front of the camera (close for melee, farther for guns)
		var z: Zombie = session._create_zombie(3000 + p.current, "walker", "sp_west")
		z.rise_t = 0.01
		z.state = Zombie.State.CHASE
		var hit_dist: float = 1.2 if w["kind"] == "melee" else 4.0
		var aim_point: Vector3 = p.camera.global_position + (-p.camera.global_transform.basis.z * hit_dist)
		z.global_position = Vector3(aim_point.x, 0.0, aim_point.z)
		await physics_frame # let the physics server sync the moved collider
		z.velocity = Vector3.ZERO
		z.speed = 0.0 # freeze in place for a deterministic shot
		p.aim_amount = 1.0 # tighten hip-fire spread for deterministic tests
		var hp0: float = z.hp
		p._fire()
		if w["kind"] == "gun":
			check("%s consumes ammo" % wid, int(p.mags[wid]) < int(w["mag_size"]))
		check("%s deals damage" % wid, z.hp < hp0)
		session.zombies.erase(z)
		z.free() # immediate: queue_free would leave the collider in this frame's ray tests
	# reload flow
	p.current = p.owned.find("pistol")
	p._equip_viewmodel()
	p.mags["pistol"] = 0
	p._start_reload()
	for i in 90:
		await physics_frame
	check("reload refills magazine", int(p.mags["pistol"]) == 12)
	# effects exist
	check("tracer/spark effects spawn", session.effect_root.get_child_count() > 0)

# ------------------------------------------------------------------- bosses
func _test_bosses() -> void:
	print("[9] bosses")
	var hulk: Zombie = session._create_zombie(4001, "hulk", "sp_east1")
	hulk.rise_t = 0.01
	check("hulk is mini boss", hulk.stats.get("boss", "") == "mini")
	hulk._spit_bile(session.local_player, 1)
	check("bile projectile spawns", session.effect_root.get_child_count() > 0)
	hulk._start_charge(session.local_player)
	check("hulk charge state", hulk.charge_t > 0)
	var overlord: Zombie = session._create_zombie(4002, "overlord", "sp_east2")
	check("overlord is final boss", overlord.stats.get("boss", "") == "final")
	overlord.hp = float(overlord.stats["hp"]) * 0.5
	overlord._boss_think(0.01, session.local_player)
	check("overlord phase advances", overlord.boss_phase >= 1)
	for z in [hulk, overlord]:
		session.zombies.erase(z)
		z.queue_free()

# ---------------------------------------------------------------- campaign
func _test_campaign() -> void:
	print("[10] ten-night campaign flow")
	for nid in range(1, 11):
		var cfg := data.night_config(nid)
		check("night %d config valid" % nid, cfg.has("pulses") and int(cfg["prep_time"]) > 0)
	# full fast run of night 1
	session.phase = "prep"
	session.begin_night()
	check("night starts", session.phase == "night" and session.director.active)
	var guard := 0
	while session.phase == "night" and guard < 1500:
		guard += 1
		await physics_frame
		# fast-forward the spawn clock so the full pulse schedule flushes
		# (the test verifies completion logic, not real-time pacing)
		session.director.elapsed = max(session.director.elapsed, 60.0)
		# help the director along: kill everything that spawned
		for z in session.zombies.duplicate():
			if z.state != Zombie.State.DEAD:
				z.take_damage(99999, z.global_position)
	check("night 1 completes", session.phase == "prep" and session.night == 2)
	check("reward cash granted", int(session.resources["cash"]) > 0)

# --------------------------------------------------------------------- save
func _test_save() -> void:
	print("[11] save / load")
	session.save.write_campaign(session)
	var loaded := session.save.read_campaign()
	check("save file written", loaded.size() > 0)
	check("save keeps night", int(loaded["night"]) == session.night)
	check("save keeps resources", loaded["resources"].has("cash"))
	var settings_path := "user://settings.json"
	session.save.write_settings({"master": -3.0})
	check("settings persist", FileAccess.file_exists(settings_path))

# ----------------------------------------------------------------------- UI
func _test_ui() -> void:
	print("[12] UI screens")
	check("HUD built", ui.hud != null and ui.hud.get_child_count() > 5)
	ui.open_shop(session.local_player)
	check("shop opens in prep phase", ui.shop_win.visible)
	var item: Dictionary = data.SHOP["items"][0]
	var cash0: int = int(session.resources["cash"])
	ui._buy(item)
	check("shop purchase works", int(session.resources["cash"]) == cash0 - int(item["price"]))
	ui.close_shop()
	ui.show_death()
	check("death screen", ui.death_win.visible)
	ui.death_win.visible = false
	var b: Barricade = session.level.barricades["main_entrance"]
	check("prompt system works", b.get_prompt() != "")

func _finish() -> void:
	print("===== RESULT: %d passed, %d failed =====" % [passed, failed])
	var f := FileAccess.open("user://test_results.log", FileAccess.WRITE)
	if f:
		f.store_string("passed=%d\nfailed=%d\n" % [passed, failed])
		for x in failures:
			f.store_string("FAIL " + x + "\n")
	quit(1 if failed > 0 else 0)
