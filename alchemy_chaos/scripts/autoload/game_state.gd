extends Node

signal experiment_completed(outcome: Dictionary)
signal auto_experiment_triggered(inputs: Array, outcome: Dictionary)
signal ingredient_discovered(ing_id: String)
signal recipe_discovered(rec_id: String)
signal rp_changed(new_rp: int)
signal upgrade_purchased(upg_id: String, new_level: int)
signal challenge_completed(ch_id: String)
signal timed_challenge_updated()
signal achievement_unlocked(ach_id: String)
signal milestone_reached(ms_id: String)
signal toast_requested(kind: String, title: String, subtitle: String, icon_path: String)
signal state_reloaded()

const SAVE_VERSION: int = 2
const DEFAULT_SAVE_PATH: String = "user://alchemy_chaos_save.json"
const DEFAULT_BACKUP_PATH: String = "user://alchemy_chaos_save.bak"
const CHECKSUM_SALT: String = "ALCHEMY_CHAOS_V2_SANCTUM"

var research_points: int = 0
var discovered_ingredients: Dictionary = {} # ing_id -> discovery_order (int)
var discovered_recipes: Dictionary = {}     # rec_id -> true
var tried_keys: Dictionary = {}             # canonical_key -> true
var witnessed_reactions: Dictionary = {}    # reaction_type -> true
var upgrade_levels: Dictionary = {}         # upg_id -> int (0..3)
var pinned_ingredients: Array[String] = []
var recent_history: Array = []              # Array of {inputs: Array, result: String, rec_id: String}
var completed_challenges: Dictionary = {}   # ch_id -> true
var unlocked_achievements: Dictionary = {}  # ach_id -> true
var claimed_milestones: Dictionary = {}     # ms_id -> true
var total_experiments: int = 0

var settings: Dictionary = {
	"master_volume": 0.85,
	"sfx_volume": 0.85,
	"ambient_volume": 0.55,
	"screen_shake": true,
	"auto_transmute": false,
	"fullscreen": false,
}

# Active Timed Challenge runtime state
var active_timed_id: String = ""
var active_timed_remaining: float = 0.0
var active_timed_count: int = 0
var active_timed_targets_done: Dictionary = {}

# Automaton & Detector runtime timers
var auto_experiment_enabled: bool = false
var _auto_timer: float = 0.0
var detector_cooldown_remaining: float = 0.0

var last_load_recovered_from_corruption: bool = false
var suppress_autosave: bool = false


func _ready() -> void:
	AlchemyDB.load_database()
	reset_to_fresh_state(false)
	if FileAccess.file_exists(DEFAULT_SAVE_PATH) or FileAccess.file_exists(DEFAULT_BACKUP_PATH):
		load_game(DEFAULT_SAVE_PATH, DEFAULT_BACKUP_PATH)


func has_existing_save(path: String = DEFAULT_SAVE_PATH) -> bool:
	return FileAccess.file_exists(path)


func reset_to_fresh_state(write_disk: bool = false) -> void:
	research_points = 0
	discovered_ingredients.clear()
	discovered_recipes.clear()
	tried_keys.clear()
	witnessed_reactions.clear()
	upgrade_levels.clear()
	pinned_ingredients.clear()
	recent_history.clear()
	completed_challenges.clear()
	unlocked_achievements.clear()
	claimed_milestones.clear()
	total_experiments = 0
	active_timed_id = ""
	active_timed_remaining = 0.0
	active_timed_count = 0
	active_timed_targets_done.clear()
	auto_experiment_enabled = false
	_auto_timer = 0.0
	detector_cooldown_remaining = 0.0

	for u in AlchemyDB.upgrades:
		upgrade_levels[u["id"]] = 0

	var order := 1
	for ing in AlchemyDB.ingredients_ordered:
		if ing.starter:
			discovered_ingredients[ing.id] = order
			pinned_ingredients.append(ing.id)
			order += 1

	_apply_settings()
	if write_disk and not suppress_autosave:
		save_game()
	state_reloaded.emit()


func _process(delta: float) -> void:
	if detector_cooldown_remaining > 0.0:
		detector_cooldown_remaining = maxf(0.0, detector_cooldown_remaining - delta)

	if not active_timed_id.is_empty():
		active_timed_remaining -= delta
		if active_timed_remaining <= 0.0:
			var failed_title: String = AlchemyDB.challenges_by_id.get(active_timed_id, {}).get("title", "Timed Trial")
			active_timed_id = ""
			active_timed_remaining = 0.0
			toast_requested.emit("warning", "Hourglass Expired", failed_title + " - retry anytime in Challenges!", "res://assets/ui/icon_nav_challenges.png")
		timed_challenge_updated.emit()

	if auto_experiment_enabled and get_upgrade_level("automaton") >= 1:
		var interval := get_automaton_interval()
		_auto_timer += delta
		if _auto_timer >= interval:
			_auto_timer = 0.0
			_run_automaton_step()


# ==============================================================================
# UPGRADE HELPERS
# ==============================================================================
func get_upgrade_level(upg_id: String) -> int:
	return int(upgrade_levels.get(upg_id, 0))


func get_max_slots() -> int:
	return 3 if get_upgrade_level("mixer") >= 1 else 2


func can_chain_feed() -> bool:
	return get_upgrade_level("mixer") >= 2


func get_reaction_duration() -> float:
	match get_upgrade_level("burner"):
		1: return 0.85
		2: return 0.55
		3: return 0.35
		_: return 1.10


func get_pin_capacity() -> int:
	match get_upgrade_level("storage"):
		1: return 8
		2: return 12
		3: return 16
		_: return 4


func get_history_capacity() -> int:
	match get_upgrade_level("storage"):
		1: return 5
		2: return 8
		3: return 12
		_: return 3


func get_automaton_interval() -> float:
	match get_upgrade_level("automaton"):
		1: return 5.0
		2: return 2.8
		3: return 1.4
		_: return 999.0


func get_detector_cooldown_max() -> float:
	match get_upgrade_level("detector"):
		1: return 15.0
		2: return 6.0
		3: return 2.0
		_: return 20.0


func get_spectroscope_level() -> int:
	return get_upgrade_level("research_lens")


func can_purchase_upgrade(upg_id: String) -> bool:
	if not AlchemyDB.upgrades_by_id.has(upg_id):
		return false
	var u: Dictionary = AlchemyDB.upgrades_by_id[upg_id]
	var lvl := get_upgrade_level(upg_id)
	if lvl >= int(u["max_level"]):
		return false
	var cost := int(u["costs"][lvl])
	return research_points >= cost


func get_next_upgrade_cost(upg_id: String) -> int:
	if not AlchemyDB.upgrades_by_id.has(upg_id):
		return -1
	var u: Dictionary = AlchemyDB.upgrades_by_id[upg_id]
	var lvl := get_upgrade_level(upg_id)
	if lvl >= int(u["max_level"]):
		return -1
	return int(u["costs"][lvl])


func purchase_upgrade(upg_id: String) -> bool:
	if not can_purchase_upgrade(upg_id):
		return false
	var cost := get_next_upgrade_cost(upg_id)
	research_points -= cost
	var new_lvl := get_upgrade_level(upg_id) + 1
	upgrade_levels[upg_id] = new_lvl
	SoundEngine.play_upgrade()
	rp_changed.emit(research_points)
	upgrade_purchased.emit(upg_id, new_lvl)
	var u: Dictionary = AlchemyDB.upgrades_by_id[upg_id]
	toast_requested.emit("upgrade", u["name"] + " Lv." + str(new_lvl), u["tiers"][new_lvl - 1], u["icon"])
	_check_achievements()
	if not suppress_autosave:
		save_game()
	return true


# ==============================================================================
# EXPERIMENTATION & DISCOVERY ENGINE
# ==============================================================================
func is_ingredient_discovered(ing_id: String) -> bool:
	return discovered_ingredients.has(ing_id)


func is_recipe_discovered(rec_id: String) -> bool:
	return discovered_recipes.has(rec_id)


func toggle_pin_ingredient(ing_id: String) -> bool:
	if not is_ingredient_discovered(ing_id):
		return false
	if pinned_ingredients.has(ing_id):
		pinned_ingredients.erase(ing_id)
		if not suppress_autosave:
			save_game()
		return false
	if pinned_ingredients.size() >= get_pin_capacity():
		pinned_ingredients.pop_front()
	pinned_ingredients.append(ing_id)
	if not suppress_autosave:
		save_game()
	return true


func perform_experiment(inputs: Array) -> Dictionary:
	var clean: Array[String] = []
	for raw in inputs:
		var s := String(raw).strip_edges()
		if not s.is_empty():
			clean.append(s)

	if clean.size() < 2 or clean.size() > get_max_slots():
		return {"valid": false, "error": "INVALID_SLOT_COUNT"}

	for inp in clean:
		if not is_ingredient_discovered(inp):
			return {"valid": false, "error": "UNDISCOVERED_INPUT"}

	total_experiments += 1
	var ckey := AlchemyDB.canonical_key(clean)
	var was_tried := tried_keys.has(ckey)
	tried_keys[ckey] = true

	var rec := AlchemyDB.find_recipe(clean)
	if rec == null:
		var stab_lvl := get_upgrade_level("stabilizer")
		var salvaged := 0
		match stab_lvl:
			1: salvaged = 3
			2: salvaged = 6
			3: salvaged = 10
		if was_tried:
			salvaged = int(salvaged / 2)
		if salvaged > 0:
			research_points += salvaged
			rp_changed.emit(research_points)
		var hint_msg := ""
		if stab_lvl >= 2:
			var res_state := _compute_raw_resonance(clean[0])
			if res_state != "NONE":
				var first_name := AlchemyDB.get_ingredient(clean[0]).display_name
				hint_msg = "Stabilizer Analysis: %s still has undiscovered reactions with your owned substances!" % first_name
			else:
				hint_msg = "Stabilizer Analysis: All currently possible pairs for Slot 1 have been found."
		var fail_out := {
			"valid": false,
			"success": false,
			"inputs": clean,
			"key": ckey,
			"reaction_type": "SMOKE",
			"rp_gained": salvaged,
			"hint_message": hint_msg,
		}
		experiment_completed.emit(fail_out)
		if not suppress_autosave:
			save_game()
		return fail_out

	# Valid Recipe!
	witnessed_reactions[rec.reaction_type] = true
	var is_new_rec := not discovered_recipes.has(rec.id)
	var is_new_ing := not discovered_ingredients.has(rec.result)
	var out_ing := AlchemyDB.get_ingredient(rec.result)

	var rp_gain := 0
	if is_new_rec:
		discovered_recipes[rec.id] = true
		var base_rp := float(rec.rp_reward)
		if is_new_ing:
			base_rp += float(out_ing.rp_value)
		var mult := 1.0
		# Burner bonus
		var b_lvl := get_upgrade_level("burner")
		var is_thermal_or_metal := rec.reaction_type in ["MELTING", "EXPLOSION", "SPARKS"] or out_ing.category == "Metals"
		if b_lvl == 1 and is_thermal_or_metal:
			mult += 0.15
		elif b_lvl == 2 and is_thermal_or_metal:
			mult += 0.30
		elif b_lvl == 3:
			mult += 0.50
		# Mixer multi-stage bonus
		var m_lvl := get_upgrade_level("mixer")
		if rec.inputs.size() == 3:
			if m_lvl == 2:
				mult += 0.20
			elif m_lvl == 3:
				mult += 0.45
		# Spectroscope discovery bonus
		var l_lvl := get_upgrade_level("research_lens")
		if l_lvl == 1:
			mult += 0.15
		elif l_lvl == 2:
			mult += 0.25
		elif l_lvl == 3:
			mult += 0.40
		rp_gain = int(round(base_rp * mult))
	else:
		# Repeat synthesis study reward
		rp_gain = 3 if get_upgrade_level("burner") >= 3 else 2

	if is_new_ing:
		discovered_ingredients[rec.result] = discovered_ingredients.size() + 1

	research_points += rp_gain
	rp_changed.emit(research_points)

	# Update recent history (deduplicate by recipe id)
	for i in range(recent_history.size() - 1, -1, -1):
		if recent_history[i].get("rec_id", "") == rec.id:
			recent_history.remove_at(i)
	recent_history.push_front({
		"rec_id": rec.id,
		"inputs": clean.duplicate(),
		"result": rec.result,
	})
	while recent_history.size() > get_history_capacity():
		recent_history.pop_back()

	if is_new_ing:
		ingredient_discovered.emit(rec.result)
		var kind := "rare" if (rec.is_rare or out_ing.category == "Rare") else "discovery"
		toast_requested.emit(
			kind,
			"New Substance: " + out_ing.display_name,
			"%s (#%d/50)  +%d RP" % [out_ing.category, out_ing.num, rp_gain],
			out_ing.icon_path
		)
	elif is_new_rec:
		recipe_discovered.emit(rec.id)
		toast_requested.emit(
			"recipe",
			"New Formula Discovered!",
			"Produces %s  +%d RP" % [out_ing.display_name, rp_gain],
			out_ing.icon_path
		)

	if is_new_rec and is_new_ing:
		recipe_discovered.emit(rec.id)

	_update_timed_challenge_on_brew(rec)
	_check_passive_challenges(rec)
	_check_milestones()
	_check_achievements(rec)

	var outcome := {
		"valid": true,
		"success": true,
		"recipe": rec,
		"result_id": rec.result,
		"result_ingredient": out_ing,
		"reaction_type": rec.reaction_type,
		"is_new_ingredient": is_new_ing,
		"is_new_recipe": is_new_rec,
		"is_rare": rec.is_rare,
		"rp_gained": rp_gain,
		"inputs": clean,
		"key": ckey,
	}
	experiment_completed.emit(outcome)
	if not suppress_autosave:
		save_game()
	return outcome


# ==============================================================================
# DETECTOR & AUTOMATON MECHANICS
# ==============================================================================
func _compute_raw_resonance(ing_id: String) -> String:
	if ing_id.is_empty() or not is_ingredient_discovered(ing_id):
		return "NONE"
	var max_s := get_max_slots()
	var found_any := false
	for rec in AlchemyDB.recipes_ordered:
		if discovered_recipes.has(rec.id) or rec.inputs.size() > max_s:
			continue
		if not rec.inputs.has(ing_id):
			continue
		var all_owned := true
		for inp in rec.inputs:
			if not discovered_ingredients.has(inp):
				all_owned = false
				break
		if all_owned:
			if rec.is_rare or rec.is_hidden:
				return "RARE"
			found_any = true
	return "ACTIVE" if found_any else "NONE"


func get_slot1_resonance(ing_id: String) -> String:
	var dlvl := get_upgrade_level("detector")
	if dlvl <= 0:
		return "NONE"
	var raw := _compute_raw_resonance(ing_id)
	if dlvl == 1 and raw == "RARE":
		return "ACTIVE"
	return raw


func get_undiscovered_partners_for(ing_id: String) -> Dictionary:
	var partners: Dictionary = {}
	if get_upgrade_level("detector") < 3 or ing_id.is_empty():
		return partners
	var max_s := get_max_slots()
	for rec in AlchemyDB.recipes_ordered:
		if discovered_recipes.has(rec.id) or rec.inputs.size() > max_s:
			continue
		if not rec.inputs.has(ing_id):
			continue
		var all_owned := true
		for inp in rec.inputs:
			if not discovered_ingredients.has(inp):
				all_owned = false
				break
		if all_owned:
			var copy := Array(rec.inputs)
			copy.erase(ing_id)
			for p in copy:
				partners[String(p)] = true
	return partners


func trigger_detector_scan(preferred_ing_id: String = "") -> Dictionary:
	var dlvl := get_upgrade_level("detector")
	if dlvl <= 0:
		return {"ok": false, "reason": "Upgrade Rare Resonance Detector first."}
	if detector_cooldown_remaining > 0.01:
		return {"ok": false, "reason": "Resonance Crystal recharging (%.1fs)" % detector_cooldown_remaining}

	var max_s := get_max_slots()
	var candidates: Array[RecipeData] = []
	for rec in AlchemyDB.recipes_ordered:
		if discovered_recipes.has(rec.id) or rec.inputs.size() > max_s:
			continue
		if not preferred_ing_id.is_empty() and not rec.inputs.has(preferred_ing_id):
			continue
		var all_owned := true
		for inp in rec.inputs:
			if not discovered_ingredients.has(inp):
				all_owned = false
				break
		if all_owned:
			candidates.append(rec)

	# Fallback to any available undiscovered recipe if preferred had none
	if candidates.is_empty() and not preferred_ing_id.is_empty():
		for rec in AlchemyDB.recipes_ordered:
			if discovered_recipes.has(rec.id) or rec.inputs.size() > max_s:
				continue
			var all_owned := true
			for inp in rec.inputs:
				if not discovered_ingredients.has(inp):
					all_owned = false
					break
			if all_owned:
				candidates.append(rec)

	if candidates.is_empty():
		return {"ok": false, "reason": "All currently reachable formulas discovered!"}

	# Prioritize recipes that unlock a brand-new ingredient or Rare discovery
	candidates.sort_custom(func(a: RecipeData, b: RecipeData) -> bool:
		var a_new := not discovered_ingredients.has(a.result)
		var b_new := not discovered_ingredients.has(b.result)
		if a_new != b_new:
			return a_new
		return a.is_rare and not b.is_rare
	)
	var chosen := candidates[0]
	detector_cooldown_remaining = get_detector_cooldown_max()

	var i0 := AlchemyDB.get_ingredient(chosen.inputs[0]).display_name
	var out_cat := AlchemyDB.get_ingredient(chosen.result).category
	var msg := "Resonance Detected: Combine %s with another substance -> yields a %s substance (%s)!" % [i0, out_cat, chosen.reaction_type]
	if dlvl >= 2 and chosen.inputs.size() >= 2:
		var i1 := AlchemyDB.get_ingredient(chosen.inputs[1]).display_name
		if chosen.inputs.size() == 2:
			msg = "Resonance Decoded: Try %s + %s!" % [i0, i1]
		else:
			msg = "Tri-Resonance Decoded: %s + %s + 1 Catalyst!" % [i0, i1]

	var auto_slots: Array[String] = []
	if dlvl >= 3:
		for inp in chosen.inputs:
			auto_slots.append(String(inp))

	return {
		"ok": true,
		"message": msg,
		"recipe": chosen,
		"auto_slots": auto_slots,
	}


func _run_automaton_step() -> void:
	var alvl := get_upgrade_level("automaton")
	if alvl <= 0:
		return
	var max_s := get_max_slots()
	var chosen_inputs: Array[String] = []

	# At Lv.3 (or 55% chance at Lv.2), smartly target a valid undiscovered recipe!
	if alvl == 3 or (alvl == 2 and randf() < 0.55):
		for rec in AlchemyDB.recipes_ordered:
			if discovered_recipes.has(rec.id) or rec.inputs.size() > max_s:
				continue
			var ok := true
			for inp in rec.inputs:
				if not discovered_ingredients.has(inp):
					ok = false
					break
			if ok:
				for inp in rec.inputs:
					chosen_inputs.append(String(inp))
				break

	# Otherwise pick an untried pair of owned ingredients
	if chosen_inputs.is_empty():
		var owned := discovered_ingredients.keys()
		owned.shuffle()
		for i in range(owned.size()):
			for j in range(i, owned.size()):
				var pair: Array[String] = [String(owned[i]), String(owned[j])]
				var k := AlchemyDB.canonical_key(pair)
				if not tried_keys.has(k):
					chosen_inputs = pair
					break
			if not chosen_inputs.is_empty():
				break

	if chosen_inputs.is_empty():
		auto_experiment_enabled = false
		toast_requested.emit("info", "Automaton Idle", "Every reachable combination has been tested!", "res://assets/upgrades/upg_automaton.png")
		return

	var out := perform_experiment(chosen_inputs)
	auto_experiment_triggered.emit(chosen_inputs, out)


# ==============================================================================
# CHALLENGES, MILESTONES & ACHIEVEMENTS
# ==============================================================================
func start_timed_challenge(ch_id: String) -> bool:
	if not AlchemyDB.challenges_by_id.has(ch_id):
		return false
	var ch: Dictionary = AlchemyDB.challenges_by_id[ch_id]
	if ch.get("type", "") != "TIMED":
		return false
	active_timed_id = ch_id
	active_timed_remaining = float(ch.get("time_limit", 60))
	active_timed_count = 0
	active_timed_targets_done.clear()
	timed_challenge_updated.emit()
	toast_requested.emit("challenge", "Timed Trial Started!", ch["title"] + " - " + str(int(active_timed_remaining)) + "s!", "res://assets/ui/icon_nav_challenges.png")
	return true


func cancel_timed_challenge() -> void:
	active_timed_id = ""
	active_timed_remaining = 0.0
	timed_challenge_updated.emit()


func _update_timed_challenge_on_brew(rec: RecipeData) -> void:
	if active_timed_id.is_empty():
		return
	var ch: Dictionary = AlchemyDB.challenges_by_id.get(active_timed_id, {})
	if ch.is_empty():
		return
	if get_upgrade_level("stabilizer") >= 3:
		active_timed_remaining += 5.0

	var targets: Array = ch.get("timed_targets", [])
	if targets.is_empty():
		active_timed_count += 1
		if active_timed_count >= int(ch.get("timed_goal_count", 5)):
			_complete_challenge(active_timed_id)
			active_timed_id = ""
			active_timed_remaining = 0.0
	else:
		if targets.has(rec.result):
			active_timed_targets_done[rec.result] = true
		if active_timed_targets_done.size() >= targets.size():
			_complete_challenge(active_timed_id)
			active_timed_id = ""
			active_timed_remaining = 0.0
	timed_challenge_updated.emit()


func _check_passive_challenges(latest_rec: RecipeData = null) -> void:
	for ch in AlchemyDB.challenges:
		var cid := String(ch["id"])
		if completed_challenges.has(cid):
			continue
		var ctype := String(ch.get("type", ""))
		if ctype == "SUBSTANCE":
			if discovered_ingredients.has(String(ch["target_ingredient"])):
				_complete_challenge(cid)
		elif ctype == "MULTI_STEP":
			var all_ok := true
			for req in ch.get("required_ingredients", []):
				if not discovered_ingredients.has(String(req)):
					all_ok = false
					break
			if all_ok:
				_complete_challenge(cid)
		elif ctype == "HIDDEN_PUZZLE":
			var target_key := AlchemyDB.canonical_key(ch.get("target_recipe_inputs", []))
			if latest_rec != null and latest_rec.get_canonical_key() == target_key:
				_complete_challenge(cid)
			else:
				var r: RecipeData = AlchemyDB.recipes_by_key.get(target_key, null)
				if r != null and discovered_recipes.has(r.id):
					_complete_challenge(cid)
		elif ctype == "OBJECTIVE":
			if ch.has("objective_category"):
				var cat := String(ch["objective_category"])
				var cnt := _count_discovered_in_category(cat)
				if cnt >= int(ch["objective_count"]):
					_complete_challenge(cid)
			elif ch.has("objective_reactions"):
				if witnessed_reactions.size() >= int(ch["objective_reactions"]):
					_complete_challenge(cid)


func _complete_challenge(ch_id: String) -> void:
	if completed_challenges.has(ch_id):
		return
	completed_challenges[ch_id] = true
	var ch: Dictionary = AlchemyDB.challenges_by_id[ch_id]
	var reward := int(ch.get("rp_reward", 50))
	research_points += reward
	rp_changed.emit(research_points)
	challenge_completed.emit(ch_id)
	SoundEngine.play_discovery(true)
	toast_requested.emit(
		"challenge",
		"Challenge Complete: " + String(ch["title"]),
		"Rewarded +%d Research Points!" % reward,
		"res://assets/ui/icon_nav_challenges.png"
	)


func _count_discovered_in_category(cat: String) -> int:
	var n := 0
	for ing_id in discovered_ingredients.keys():
		var ing := AlchemyDB.get_ingredient(String(ing_id))
		if ing != null and ing.category == cat:
			n += 1
	return n


func _check_milestones() -> void:
	var ing_cnt := discovered_ingredients.size()
	var rec_cnt := discovered_recipes.size()
	for ms in AlchemyDB.milestones:
		var mid := String(ms["id"])
		if claimed_milestones.has(mid):
			continue
		if ing_cnt >= int(ms["req_ingredients"]) and rec_cnt >= int(ms["req_recipes"]):
			claimed_milestones[mid] = true
			var bonus := int(ms["rp_bonus"])
			research_points += bonus
			rp_changed.emit(research_points)
			milestone_reached.emit(mid)
			toast_requested.emit(
				"milestone",
				"Rank Attained: " + String(ms["title"]),
				"Milestone Bonus +%d RP!" % bonus,
				"res://assets/ui/icon_nav_journal.png"
			)


func _check_achievements(latest_rec: RecipeData = null) -> void:
	var hidden_cnt := 0
	var has_tri := false
	for rid in discovered_recipes.keys():
		var r: RecipeData = AlchemyDB.recipes_by_id.get(String(rid), null)
		if r != null:
			if r.is_hidden:
				hidden_cnt += 1
			if r.inputs.size() == 3:
				has_tri = true
	if latest_rec != null and latest_rec.inputs.size() == 3:
		has_tri = true

	var timed_done := 0
	for cid in completed_challenges.keys():
		var ch: Dictionary = AlchemyDB.challenges_by_id.get(String(cid), {})
		if ch.get("type", "") == "TIMED":
			timed_done += 1

	var max_upg := 0
	for u in AlchemyDB.upgrades:
		if get_upgrade_level(u["id"]) >= int(u["max_level"]):
			max_upg += 1

	for ach in AlchemyDB.achievements:
		var aid := String(ach["id"])
		if unlocked_achievements.has(aid):
			continue
		var metric := String(ach["metric"])
		var met := false
		match metric:
			"ingredients":
				met = discovered_ingredients.size() >= int(ach["target"])
			"recipes":
				met = discovered_recipes.size() >= int(ach["target"])
			"hidden_recipes":
				met = hidden_cnt >= int(ach["target"])
			"tri_recipe":
				met = has_tri
			"pair":
				met = true
				for it in ach["items"]:
					if not discovered_ingredients.has(String(it)):
						met = false
			"item":
				met = discovered_ingredients.has(String(ach["item"]))
			"timed_challenge":
				met = timed_done >= int(ach["target"])
			"max_single_upgrade":
				met = max_upg >= int(ach["target"])
			_:
				if metric.begins_with("cat_"):
					var cat := metric.substr(4)
					met = _count_discovered_in_category(cat) >= int(ach["target"])
		if met:
			unlocked_achievements[aid] = true
			achievement_unlocked.emit(aid)
			toast_requested.emit(
				"achievement",
				"Achievement Unlocked!",
				String(ach["title"]),
				"res://assets/ui/icon_nav_achievements.png"
			)


func get_current_rank_title() -> String:
	var best := "Initiate Alchemist"
	for ms in AlchemyDB.milestones:
		if claimed_milestones.has(String(ms["id"])):
			best = String(ms["title"])
	return best


# ==============================================================================
# SETTINGS & VERSIONED CHECKSUMMED SAVE / LOAD
# ==============================================================================
func update_setting(key: String, val: Variant) -> void:
	settings[key] = val
	_apply_settings()
	if not suppress_autosave:
		save_game()


func _apply_settings() -> void:
	SoundEngine.set_volumes(
		float(settings.get("master_volume", 0.85)),
		float(settings.get("sfx_volume", 0.85)),
		float(settings.get("ambient_volume", 0.55))
	)
	if DisplayServer.get_name() != "headless":
		var want_fs := bool(settings.get("fullscreen", false))
		var cur_mode := DisplayServer.window_get_mode()
		if want_fs and cur_mode != DisplayServer.WINDOW_MODE_FULLSCREEN:
			DisplayServer.window_set_mode(DisplayServer.WINDOW_MODE_FULLSCREEN)
		elif not want_fs and cur_mode == DisplayServer.WINDOW_MODE_FULLSCREEN:
			DisplayServer.window_set_mode(DisplayServer.WINDOW_MODE_WINDOWED)


func _compute_checksum(payload_json: String) -> String:
	return (payload_json + "|" + CHECKSUM_SALT).sha256_text()


func serialize_state() -> Dictionary:
	var raw_payload := {
		"research_points": research_points,
		"discovered_ingredients": discovered_ingredients,
		"discovered_recipes": discovered_recipes.keys(),
		"tried_keys": tried_keys.keys(),
		"witnessed_reactions": witnessed_reactions.keys(),
		"upgrade_levels": upgrade_levels,
		"pinned_ingredients": pinned_ingredients,
		"recent_history": recent_history,
		"completed_challenges": completed_challenges.keys(),
		"unlocked_achievements": unlocked_achievements.keys(),
		"claimed_milestones": claimed_milestones.keys(),
		"total_experiments": total_experiments,
		"settings": settings,
	}
	# Normalize numeric types via JSON roundtrip so SHA-256 checksum is 100% identical on save & load
	var jp := JSON.new()
	jp.parse(JSON.stringify(raw_payload, "", true))
	var normalized_payload: Dictionary = jp.data
	var canonical_payload := JSON.stringify(normalized_payload, "", true)
	return {
		"save_version": SAVE_VERSION,
		"timestamp": int(Time.get_unix_time_from_system()),
		"checksum": _compute_checksum(canonical_payload),
		"payload": normalized_payload,
	}


func save_game(path: String = DEFAULT_SAVE_PATH, backup_path: String = DEFAULT_BACKUP_PATH) -> bool:
	var doc := serialize_state()
	var text := JSON.stringify(doc, "\t", true)
	# Copy existing valid primary save to backup first
	if not backup_path.is_empty() and FileAccess.file_exists(path):
		var prev := FileAccess.get_file_as_string(path)
		if prev.length() > 20 and prev.find("\"save_version\"") != -1:
			var bf := FileAccess.open(backup_path, FileAccess.WRITE)
			if bf != null:
				bf.store_string(prev)
				bf.close()
	var f := FileAccess.open(path, FileAccess.WRITE)
	if f == null:
		return false
	f.store_string(text)
	f.close()
	return true


func load_game(path: String = DEFAULT_SAVE_PATH, backup_path: String = DEFAULT_BACKUP_PATH) -> bool:
	last_load_recovered_from_corruption = false
	if _try_load_single_file(path):
		state_reloaded.emit()
		return true

	# Primary file was missing or corrupted - attempt backup recovery!
	if not backup_path.is_empty() and FileAccess.file_exists(backup_path):
		if _try_load_single_file(backup_path):
			last_load_recovered_from_corruption = true
			save_game(path, "")
			toast_requested.emit("warning", "Save Restored from Backup", "Corrupted primary save was safely recovered from backup.", "res://assets/ui/icon_nav_settings.png")
			state_reloaded.emit()
			return true

	# Both missing or corrupted -> clean safe recovery without crashing!
	last_load_recovered_from_corruption = true
	reset_to_fresh_state(false)
	toast_requested.emit("warning", "Corrupted Save Reset Safely", "Invalid save data detected; initialized clean laboratory state.", "res://assets/ui/icon_nav_settings.png")
	return false


func _try_load_single_file(path: String) -> bool:
	if not FileAccess.file_exists(path):
		return false
	var raw := FileAccess.get_file_as_string(path)
	if raw.strip_edges().is_empty():
		return false
	var jp := JSON.new()
	if jp.parse(raw) != OK:
		return false
	var parsed: Variant = jp.data
	if typeof(parsed) != TYPE_DICTIONARY:
		return false
	var doc: Dictionary = parsed
	if not doc.has("save_version"):
		return false
	var ver := int(doc["save_version"])
	var payload: Dictionary = {}

	if ver == 1:
		# Version 1 -> Version 2 forward migration support!
		if typeof(doc.get("payload", null)) == TYPE_DICTIONARY:
			payload = doc["payload"]
		else:
			payload = doc
	elif ver == SAVE_VERSION:
		if typeof(doc.get("payload", null)) != TYPE_DICTIONARY:
			return false
		payload = doc["payload"]
		var expected_sum := String(doc.get("checksum", ""))
		var actual_sum := _compute_checksum(JSON.stringify(payload, "", true))
		if expected_sum != actual_sum:
			return false
	else:
		return false

	# Validate & sanitize every field defensively
	reset_to_fresh_state(false)
	research_points = max(0, int(payload.get("research_points", 0)))
	total_experiments = max(0, int(payload.get("total_experiments", 0)))

	var raw_ings: Variant = payload.get("discovered_ingredients", {})
	if typeof(raw_ings) == TYPE_DICTIONARY:
		for k in raw_ings.keys():
			var sid := String(k)
			if AlchemyDB.ingredients_by_id.has(sid):
				discovered_ingredients[sid] = max(1, int(raw_ings[k]))
	elif typeof(raw_ings) == TYPE_ARRAY:
		var idx := 1
		for k in raw_ings:
			var sid := String(k)
			if AlchemyDB.ingredients_by_id.has(sid):
				discovered_ingredients[sid] = idx
				idx += 1

	# Ensure starters are always unlocked
	for ing in AlchemyDB.ingredients_ordered:
		if ing.starter and not discovered_ingredients.has(ing.id):
			discovered_ingredients[ing.id] = discovered_ingredients.size() + 1

	if typeof(payload.get("discovered_recipes", null)) == TYPE_ARRAY:
		for rid in payload["discovered_recipes"]:
			var s := String(rid)
			if AlchemyDB.recipes_by_id.has(s):
				discovered_recipes[s] = true

	if typeof(payload.get("tried_keys", null)) == TYPE_ARRAY:
		for tk in payload["tried_keys"]:
			tried_keys[String(tk)] = true

	if typeof(payload.get("witnessed_reactions", null)) == TYPE_ARRAY:
		for rt in payload["witnessed_reactions"]:
			var srt := String(rt)
			if AlchemyDB.reaction_types.has(srt):
				witnessed_reactions[srt] = true

	if typeof(payload.get("upgrade_levels", null)) == TYPE_DICTIONARY:
		var udict: Dictionary = payload["upgrade_levels"]
		for u in AlchemyDB.upgrades:
			var uid := String(u["id"])
			var max_l := int(u["max_level"])
			upgrade_levels[uid] = clampi(int(udict.get(uid, 0)), 0, max_l)

	pinned_ingredients.clear()
	if typeof(payload.get("pinned_ingredients", null)) == TYPE_ARRAY:
		for pid in payload["pinned_ingredients"]:
			var sp := String(pid)
			if discovered_ingredients.has(sp) and not pinned_ingredients.has(sp):
				pinned_ingredients.append(sp)

	recent_history.clear()
	if typeof(payload.get("recent_history", null)) == TYPE_ARRAY:
		for h in payload["recent_history"]:
			if typeof(h) == TYPE_DICTIONARY and AlchemyDB.recipes_by_id.has(String(h.get("rec_id", ""))):
				recent_history.append(h)

	if typeof(payload.get("completed_challenges", null)) == TYPE_ARRAY:
		for cid in payload["completed_challenges"]:
			var sc := String(cid)
			if AlchemyDB.challenges_by_id.has(sc):
				completed_challenges[sc] = true

	if typeof(payload.get("unlocked_achievements", null)) == TYPE_ARRAY:
		for aid in payload["unlocked_achievements"]:
			var sa := String(aid)
			if AlchemyDB.achievements_by_id.has(sa):
				unlocked_achievements[sa] = true

	if typeof(payload.get("claimed_milestones", null)) == TYPE_ARRAY:
		for mid in payload["claimed_milestones"]:
			claimed_milestones[String(mid)] = true

	if typeof(payload.get("settings", null)) == TYPE_DICTIONARY:
		var sdict: Dictionary = payload["settings"]
		for sk in settings.keys():
			if sdict.has(sk):
				settings[sk] = sdict[sk]

	_apply_settings()
	return true
