class_name FlightHUD
extends Control
const Data = preload("res://scripts/data.gd")
const State = preload("res://scripts/game_state.gd")
const World = preload("res://scripts/world.gd")

const INK := Color("071018")
const PANEL := Color(0.025, 0.065, 0.085, 0.91)
const LINE := Color(0.24, 0.52, 0.54, 0.42)
const TEAL := Color("66ddc9")
const MUTED := Color("78949b")
const TEXT := Color("d5e6e7")
const GOLD := Color("e8ba69")

var game_state: State
var world: World
var font: Font = preload("res://assets/fonts/Rajdhani-SemiBold.ttf")
var bold: Font = preload("res://assets/fonts/Rajdhani-Bold.ttf")
var mono: Font = preload("res://assets/fonts/SpaceMono-Regular.ttf")
var cargo_expanded := false
var toasts: Array[Dictionary] = []
var sector_title := ""
var sector_subtitle := ""
var sector_color := TEAL
var sector_title_time := 0.0
var achievement_title := ""
var achievement_time := 0.0
var elapsed := 0.0

func _ready() -> void:
	set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	mouse_filter = Control.MOUSE_FILTER_IGNORE

func configure(state: State, world_ref: World) -> void:
	game_state = state
	world = world_ref

func notify(text: String, color: Color = TEAL) -> void:
	toasts.push_front({"text":text, "color":color, "time":4.2, "duration":4.2})
	if toasts.size() > 4:
		toasts.resize(4)

func show_sector(index: int) -> void:
	var sector: Dictionary = Data.SECTORS[index]
	sector_title = sector.name
	sector_subtitle = "%s  •  %s" % [sector.subtitle, sector.danger]
	sector_color = Color(sector.color)
	sector_title_time = 4.4

func show_achievement(id: String) -> void:
	for achievement in Data.ACHIEVEMENTS:
		if achievement.id == id:
			achievement_title = achievement.name
			achievement_time = 5.0
			notify("ACHIEVEMENT  •  " + achievement.name, GOLD)
			return

func _process(delta: float) -> void:
	elapsed += delta
	sector_title_time = maxf(0.0, sector_title_time - delta)
	achievement_time = maxf(0.0, achievement_time - delta)
	for toast in toasts:
		toast.time = float(toast.time) - delta
	toasts = toasts.filter(func(t): return float(t.time) > 0.0)
	queue_redraw()

func _draw() -> void:
	if game_state == null or world == null or not is_instance_valid(world.player):
		return
	var w := size.x
	var h := size.y
	_draw_status(Rect2(24, 24, 342, 156))
	_draw_objective(Rect2(w * 0.5 - 280, 24, 560, 64))
	_draw_credits(Rect2(w - 362, 24, 338, 118))
	_draw_sector(Rect2(w - 362, 154, 338, 66))
	_draw_radar(Vector2(126, h - 130), 96.0)
	_draw_drones(Rect2(w - 326, h - 205, 302, 174))
	if cargo_expanded:
		_draw_cargo(Rect2(w - 396, h * 0.5 - 268, 372, 478))
	if is_instance_valid(world.scan_target) and not world.scan_target.destroyed:
		_draw_target(Rect2(w * 0.5 - 220, h - 172, 440, 90))
	_draw_prompts(w, h)
	_draw_toasts(w)
	_draw_navigation(w, h)
	_draw_crosshair()
	if sector_title_time > 0.0:
		_draw_sector_title(w, h)
	if world.player.damage_timer > 3.55:
		var alpha := clampf((world.player.damage_timer - 3.55) * 0.18, 0.0, 0.12)
		draw_rect(Rect2(0, 0, w, 10), Color(0.9, 0.2, 0.18, alpha * 3.0))
		draw_rect(Rect2(0, h - 10, w, 10), Color(0.9, 0.2, 0.18, alpha * 3.0))
		draw_rect(Rect2(0, 0, 10, h), Color(0.9, 0.2, 0.18, alpha * 3.0))
		draw_rect(Rect2(w - 10, 0, 10, h), Color(0.9, 0.2, 0.18, alpha * 3.0))

func _panel(rect: Rect2, accent: Color = TEAL, fill: Color = PANEL) -> void:
	var x := rect.position.x
	var y := rect.position.y
	var w := rect.size.x
	var h := rect.size.y
	var points := PackedVector2Array([
		Vector2(x + 11, y), Vector2(x + w - 3, y), Vector2(x + w, y + 3),
		Vector2(x + w, y + h - 11), Vector2(x + w - 11, y + h),
		Vector2(x + 3, y + h), Vector2(x, y + h - 3), Vector2(x, y + 11)
	])
	draw_colored_polygon(points, fill)
	points.append(points[0])
	draw_polyline(points, LINE, 1.0, true)
	draw_line(Vector2(x + 2, y + 18), Vector2(x + 2, y + 48), accent, 2.0, true)
	draw_line(Vector2(x + 11, y), Vector2(x + 42, y), accent, 2.0, true)
	draw_line(Vector2(x + w - 35, y + h), Vector2(x + w - 11, y + h), accent.darkened(0.25), 2.0, true)

func _text(position: Vector2, text: String, font_size: int = 20, color: Color = TEXT, use_mono: bool = false, use_bold: bool = false) -> void:
	var selected := mono if use_mono else (bold if use_bold else font)
	draw_string(selected, position, text, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, color)

func _bar(rect: Rect2, value: float, maximum: float, color: Color, label: String, value_text: String) -> void:
	_text(rect.position + Vector2(0, 12), label, 14, MUTED, true)
	var bar_rect := Rect2(rect.position + Vector2(28, 0), Vector2(rect.size.x - 110, 12))
	draw_rect(bar_rect, Color(0.12, 0.19, 0.21, 0.75), true)
	var fraction := clampf(value / maxf(1.0, maximum), 0.0, 1.0)
	if fraction > 0.0:
		draw_rect(Rect2(bar_rect.position, Vector2(bar_rect.size.x * fraction, bar_rect.size.y)), color.darkened(0.12), true)
		draw_line(bar_rect.position + Vector2(0, 1), bar_rect.position + Vector2(bar_rect.size.x * fraction, 1), color.lightened(0.25), 1.0, true)
	_text(rect.position + Vector2(rect.size.x - 69, 12), value_text, 12, TEXT, true)

func _draw_status(rect: Rect2) -> void:
	_panel(rect)
	var ship := world.player
	var stats := game_state.get_stats()
	_text(rect.position + Vector2(22, 27), Data.get_ship(game_state.profile.current_ship).name, 19, TEXT, false, true)
	_bar(Rect2(rect.position + Vector2(22, 40), Vector2(296, 12)), ship.hull, stats.max_hull, Color("74d6a4"), "H", "%d/%d" % [ship.hull, stats.max_hull])
	_bar(Rect2(rect.position + Vector2(22, 67), Vector2(296, 12)), ship.shield, stats.max_shield, Color("6bafe4"), "S", "%d/%d" % [ship.shield, stats.max_shield])
	_bar(Rect2(rect.position + Vector2(22, 94), Vector2(296, 12)), ship.energy, stats.max_energy, GOLD, "E", "%d/%d" % [ship.energy, stats.max_energy])
	_bar(Rect2(rect.position + Vector2(22, 121), Vector2(296, 12)), game_state.cargo_total(), stats.cargo, TEAL, "C", "%d/%d" % [game_state.cargo_total(), stats.cargo])

func _draw_objective(rect: Rect2) -> void:
	_panel(rect, GOLD, Color(0.035, 0.065, 0.075, 0.88))
	var next := -1
	for i in range(Data.SECTORS.size()):
		if i not in game_state.profile.unlocked_sectors:
			next = i
			break
	if next < 0:
		_text(rect.position + Vector2(22, 27), "FRONTIER OPERATIONS", 19, GOLD, false, true)
		_text(rect.position + Vector2(22, 49), "ALL SECTORS CHARTED  •  BUILD YOUR MINING FLEET", 11, MUTED, true)
	else:
		var sector: Dictionary = Data.SECTORS[next]
		_text(rect.position + Vector2(22, 27), "NEXT: " + sector.name, 19, GOLD, false, true)
		_text(rect.position + Vector2(22, 49), "PILOT LVL %d  +  %s CR  •  DOCK TO CHART" % [int(sector.level), State.format_number(int(sector.cost))], 11, MUTED, true)

func _draw_credits(rect: Rect2) -> void:
	_panel(rect, GOLD)
	_text(rect.position + Vector2(22, 43), State.format_number(int(game_state.profile.credits)), 34, GOLD, false, true)
	_text(rect.position + Vector2(rect.size.x - 45, 41), "CR", 13, MUTED, true)
	_text(rect.position + Vector2(22, 71), "PILOT LVL %d" % int(game_state.profile.level), 15, TEAL, true)
	var needed := Data.xp_for_level(int(game_state.profile.level))
	var fraction := float(game_state.profile.xp) / maxf(1.0, float(needed))
	draw_rect(Rect2(rect.position + Vector2(125, 63), Vector2(180, 7)), Color("1c3035"), true)
	draw_rect(Rect2(rect.position + Vector2(125, 63), Vector2(180 * clampf(fraction, 0, 1), 7)), TEAL.darkened(0.1), true)
	_text(rect.position + Vector2(125, 91), "%d / %d XP" % [int(game_state.profile.xp), needed], 10, MUTED, true)

func _draw_sector(rect: Rect2) -> void:
	var sector: Dictionary = Data.SECTORS[world.sector_index]
	var color := Color(sector.color)
	_panel(rect, color, Color(0.025, 0.055, 0.075, 0.88))
	_text(rect.position + Vector2(22, 29), sector.name, 21, TEXT, false, true)
	_text(rect.position + Vector2(22, 50), sector.code, 11, MUTED, true)
	_text(rect.position + Vector2(rect.size.x - 105, 49), sector.danger, 10, color, true)

func _draw_radar(center: Vector2, radius: float) -> void:
	_panel(Rect2(center - Vector2(radius + 22, radius + 22), Vector2.ONE * (radius + 22) * 2), TEAL, Color(0.02, 0.05, 0.07, 0.78))
	draw_circle(center, radius, Color(0.015, 0.05, 0.06, 0.92))
	for r in [radius * 0.33, radius * 0.66, radius]:
		draw_arc(center, r, 0, TAU, 72, Color(0.28, 0.62, 0.60, 0.26), 1.0, true)
	draw_line(center - Vector2(radius, 0), center + Vector2(radius, 0), Color(0.28, 0.62, 0.60, 0.18), 1.0, true)
	draw_line(center - Vector2(0, radius), center + Vector2(0, radius), Color(0.28, 0.62, 0.60, 0.18), 1.0, true)
	var scan_angle := elapsed * 0.7
	draw_line(center, center + Vector2.from_angle(scan_angle) * radius, Color(0.4, 0.9, 0.8, 0.18), 1.0, true)
	var range_world := 1550.0
	for rock in world.asteroids:
		if is_instance_valid(rock):
			_radar_dot(center, radius, range_world, rock.global_position, Color(0.55, 0.65, 0.66, 0.42), 1.3)
	for enemy in world.enemies:
		if is_instance_valid(enemy):
			_radar_dot(center, radius, range_world, enemy.global_position, Color("ef6e68"), 3.0)
	for drone in world.drones:
		if is_instance_valid(drone) and drone.enabled:
			_radar_dot(center, radius, range_world, drone.global_position, TEAL, 2.0)
	_radar_dot(center, radius, range_world, Data.STATION_POSITION, GOLD, 4.5)
	draw_colored_polygon(PackedVector2Array([center + Vector2(0, -6), center + Vector2(4, 5), center + Vector2(-4, 5)]), Color("d9faf1"))
	_text(center + Vector2(-radius - 5, -radius - 31), "SECTOR SCAN", 11, MUTED, true)
	_text(center + Vector2(25, -radius - 31), "M  CHART", 10, GOLD, true)

func _radar_dot(center: Vector2, radius: float, range_world: float, position: Vector2, color: Color, dot_size: float) -> void:
	var offset := (position - world.player.global_position) / range_world * radius
	if offset.length() <= radius:
		draw_circle(center + offset, dot_size, color)

func _draw_drones(rect: Rect2) -> void:
	_panel(rect)
	_text(rect.position + Vector2(18, 26), "MINING DRONES", 15, TEXT, false, true)
	var status := "AUTO" if bool(game_state.profile.drones_active) else "DOCKED"
	_text(rect.position + Vector2(rect.size.x - 70, 25), status, 11, TEAL, true)
	var count := mini(4, world.drones.size())
	if count == 0:
		_text(rect.position + Vector2(18, 57), "NO DRONES DEPLOYED", 12, MUTED, true)
		_text(rect.position + Vector2(18, 82), "DRONE BAY UPGRADE", 10, MUTED, true)
		_text(rect.position + Vector2(18, 101), "AVAILABLE AT RELAY", 10, MUTED, true)
	for i in range(count):
		var drone := world.drones[i]
		if not is_instance_valid(drone):
			continue
		var y := 53.0 + i * 27.0
		_text(rect.position + Vector2(18, y), "D-%02d" % drone.drone_id, 11, TEAL, true)
		_text(rect.position + Vector2(68, y), drone.state, 10, TEXT, true)
		draw_rect(Rect2(rect.position + Vector2(155, y - 8), Vector2(116, 5)), Color("193138"), true)
		draw_rect(Rect2(rect.position + Vector2(155, y - 8), Vector2(116 * float(drone.carried_total) / maxf(1, drone.capacity), 5)), TEAL.darkened(0.15), true)
	if world.drones.size() > 4:
		_text(rect.position + Vector2(18, rect.size.y - 13), "+%d MORE UNITS" % (world.drones.size() - 4), 10, MUTED, true)

func _draw_cargo(rect: Rect2) -> void:
	_panel(rect, GOLD, Color(0.025, 0.06, 0.08, 0.97))
	_text(rect.position + Vector2(20, 31), "CARGO MANIFEST", 22, TEXT, false, true)
	_text(rect.position + Vector2(rect.size.x - 102, 29), "%d / %d" % [game_state.cargo_total(), int(game_state.get_stats().cargo)], 11, GOLD, true)
	var row := 0
	for resource in Data.RESOURCES:
		var amount := int(game_state.profile.resources[resource.id])
		var y := 56.0 + row * 26.0
		var color := Color(resource.color) if amount > 0 else Color("52676c")
		var texture: Texture2D = load("res://assets/art/icons/resource_%s.png" % resource.id)
		draw_texture_rect(texture, Rect2(rect.position + Vector2(17, y - 14), Vector2(25, 25)), false, Color(1, 1, 1, 1.0 if amount > 0 else 0.28))
		_text(rect.position + Vector2(51, y + 3), resource.name.to_upper(), 13, color)
		_text(rect.position + Vector2(rect.size.x - 94, y + 3), "%d" % amount, 12, color, true)
		_text(rect.position + Vector2(rect.size.x - 53, y + 3), "u", 10, MUTED, true)
		row += 1
	_text(rect.position + Vector2(20, rect.size.y - 18), "TAB  CLOSE MANIFEST", 10, MUTED, true)

func _draw_target(rect: Rect2) -> void:
	var rock := world.scan_target
	var resource := Data.get_resource(rock.resource_id)
	var color := Color(resource.color)
	_panel(rect, color, Color(0.025, 0.055, 0.07, 0.92))
	_text(rect.position + Vector2(18, 25), resource.name.to_upper() + " DEPOSIT", 19, color, false, true)
	_text(rect.position + Vector2(rect.size.x - 128, 23), Data.rarity_name(int(resource.rarity)), 10, MUTED, true)
	_bar(Rect2(rect.position + Vector2(18, 38), Vector2(rect.size.x - 36, 12)), rock.health, rock.max_health, color, "R", "%d%%" % int(rock.health / rock.max_health * 100.0))
	_text(rect.position + Vector2(18, 75), "YIELD %d u   •   HARDNESS %.1f   •   %s" % [rock.remaining, rock.hardness, "MINING" if world.mining_target == rock else "HOLD LMB TO MINE"], 10, MUTED, true)

func _draw_prompts(w: float, h: float) -> void:
	var prompt := ""
	var color := TEAL
	if world.can_dock():
		prompt = "E  •  DOCK AT CERES RELAY"
		color = GOLD
	elif game_state.cargo_free() <= 0:
		prompt = "CARGO FULL  •  RETURN TO RELAY"
		color = GOLD
	elif world.nearest_enemy_distance() < 620.0:
		prompt = "HOSTILE CONTACT  •  SPACE / RMB TO FIRE"
		color = Color("ed7b71")
	elif bool(game_state.profile.settings.show_hints) and int(game_state.profile.stats.asteroids) == 0:
		prompt = "WASD  THRUST   •   LMB  MINE   •   SPACE  FIRE   •   M  SECTOR CHART"
	if not prompt.is_empty():
		var width := minf(760.0, 28.0 + prompt.length() * 7.2)
		_panel(Rect2(w * 0.5 - width * 0.5, h - 64, width, 36), color, Color(0.02, 0.05, 0.065, 0.86))
		_text(Vector2(w * 0.5 - width * 0.5 + 18, h - 40), prompt, 12, color, true)

func _draw_toasts(w: float) -> void:
	for i in range(toasts.size()):
		var toast := toasts[i]
		var alpha := minf(1.0, float(toast.time) * 2.0)
		var color: Color = toast.color
		color.a = alpha
		var rect := Rect2(w * 0.5 - 330, 102 + i * 42, 660, 34)
		_panel(rect, color, Color(0.02, 0.05, 0.065, 0.78 * alpha))
		_text(rect.position + Vector2(18, 23), toast.text, 12, color, true)

func _draw_sector_title(w: float, h: float) -> void:
	var alpha := minf(1.0, sector_title_time) * minf(1.0, (4.4 - sector_title_time) * 1.8)
	var color := Color(sector_color, alpha)
	var title_size := 44
	var width := font.get_string_size(sector_title, HORIZONTAL_ALIGNMENT_LEFT, -1, title_size).x
	_text(Vector2(w * 0.5 - width * 0.5, h * 0.30), sector_title, title_size, Color(TEXT, alpha), false, true)
	var sub_width := mono.get_string_size(sector_subtitle, HORIZONTAL_ALIGNMENT_LEFT, -1, 13).x
	_text(Vector2(w * 0.5 - sub_width * 0.5, h * 0.30 + 34), sector_subtitle, 13, color, true)
	draw_line(Vector2(w * 0.5 - 210, h * 0.30 + 52), Vector2(w * 0.5 + 210, h * 0.30 + 52), Color(color, alpha * 0.4), 1.0, true)

func _draw_navigation(w: float, h: float) -> void:
	var offset: Vector2 = Data.STATION_POSITION - world.player.global_position
	if offset.length() < 800.0:
		return
	var direction := offset.normalized()
	var center := Vector2(w * 0.5, h * 0.5)
	var edge := center + direction * minf((w * 0.5 - 80) / maxf(0.01, absf(direction.x)), (h * 0.5 - 80) / maxf(0.01, absf(direction.y)))
	edge.x = clampf(edge.x, 70, w - 70)
	edge.y = clampf(edge.y, 90, h - 90)
	var side := direction.orthogonal()
	var points := PackedVector2Array([edge + direction * 12, edge - direction * 8 + side * 7, edge - direction * 8 - side * 7])
	draw_colored_polygon(points, GOLD)
	_text(edge + Vector2(14, 4), "RELAY %d" % int(offset.length() / 100.0), 10, GOLD, true)

func _draw_crosshair() -> void:
	if not world.player.controls_enabled or Input.mouse_mode != Input.MOUSE_MODE_HIDDEN:
		return
	var p := get_local_mouse_position()
	var color := Color("ef7c70") if world.nearest_enemy_distance() < 500.0 else TEAL
	draw_arc(p, 9.0, 0, TAU, 24, Color(color, 0.65), 1.0, true)
	for direction in [Vector2.LEFT, Vector2.RIGHT, Vector2.UP, Vector2.DOWN]:
		draw_line(p + direction * 13, p + direction * 20, color, 1.5, true)
	draw_circle(p, 1.5, color)
