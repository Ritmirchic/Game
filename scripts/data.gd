class_name GameData
extends RefCounted

const VERSION := "1.0.0"
const SAVE_VERSION := 3
const WORLD_SIZE := Vector2(4600, 3400)
const STATION_POSITION := Vector2(2300, 1700)
const PLAYER_SPAWN := Vector2(2300, 1950)

const RESOURCES := [
	{"id":"iron", "name":"Iron", "rarity":0, "value":12, "hardness":1.0, "color":"#a9b5b8"},
	{"id":"copper", "name":"Copper", "rarity":0, "value":18, "hardness":1.1, "color":"#d28558"},
	{"id":"nickel", "name":"Nickel", "rarity":0, "value":25, "hardness":1.25, "color":"#a9bbaa"},
	{"id":"titanium", "name":"Titanium", "rarity":1, "value":42, "hardness":1.5, "color":"#9db5cd"},
	{"id":"silver", "name":"Silver", "rarity":1, "value":65, "hardness":1.65, "color":"#d5e2e6"},
	{"id":"gold", "name":"Gold", "rarity":1, "value":95, "hardness":1.85, "color":"#edbe45"},
	{"id":"cobalt", "name":"Cobalt", "rarity":1, "value":120, "hardness":2.0, "color":"#487dc7"},
	{"id":"platinum", "name":"Platinum", "rarity":2, "value":180, "hardness":2.3, "color":"#b6d2d3"},
	{"id":"uranium", "name":"Uranium", "rarity":2, "value":240, "hardness":2.6, "color":"#76d05c"},
	{"id":"lumen_crystal", "name":"Lumen Crystal", "rarity":2, "value":310, "hardness":2.8, "color":"#47e0cf"},
	{"id":"iridium", "name":"Iridium", "rarity":3, "value":420, "hardness":3.2, "color":"#b198cd"},
	{"id":"plasma_ore", "name":"Plasma Ore", "rarity":3, "value":560, "hardness":3.7, "color":"#5c8dff"},
	{"id":"void_crystal", "name":"Void Crystal", "rarity":4, "value":850, "hardness":4.4, "color":"#b25bef"},
	{"id":"neutronium", "name":"Neutronium", "rarity":4, "value":1250, "hardness":5.0, "color":"#e873aa"},
	{"id":"quantum_core", "name":"Quantum Core", "rarity":5, "value":2200, "hardness":6.0, "color":"#5de7e2"}
]

const SECTORS := [
	{"name":"STARTER BELT", "code":"SB-01", "subtitle":"CERES THRESHOLD", "danger":"LOW RISK", "level":1, "cost":0, "color":"#55d9c8", "background":"res://assets/art/backgrounds/sector_1_starter_belt.jpg", "resources":["iron","copper","nickel","titanium","silver"], "weights":[34,28,20,12,6], "asteroids":46, "enemies":[0], "enemy_cap":2, "hazard":"MICROMETEORS", "description":"A sunlit belt of iron-rich planetoids. Calm shipping lanes and forgiving rock make this the perfect first claim."},
	{"name":"CRYSTAL NEBULA", "code":"CN-02", "subtitle":"LUMEN EXPANSE", "danger":"MODERATE", "level":2, "cost":3000, "color":"#c075e3", "background":"res://assets/art/backgrounds/sector_2_crystal_nebula.jpg", "resources":["copper","titanium","silver","gold","cobalt","lumen_crystal"], "weights":[15,20,20,17,18,10], "asteroids":50, "enemies":[0,1], "enemy_cap":3, "hazard":"CRYSTAL RESONANCE", "description":"Prismatic clouds hide valuable lumen growths. Resonant crystals periodically disrupt reactor regeneration."},
	{"name":"IRON GRAVEYARD", "code":"IG-03", "subtitle":"HEPHAESTUS DRIFT", "danger":"HIGH RISK", "level":4, "cost":9500, "color":"#d89260", "background":"res://assets/art/backgrounds/sector_3_iron_graveyard.jpg", "resources":["iron","nickel","titanium","cobalt","platinum","uranium","iridium"], "weights":[18,14,18,15,14,11,10], "asteroids":54, "enemies":[0,1,2], "enemy_cap":4, "hazard":"DEBRIS FIELDS", "description":"The shattered fleet of Hephaestus still carries rare alloys — and its autonomous raiders never stood down."},
	{"name":"PLASMA STORM", "code":"PS-04", "subtitle":"ION MAELSTROM", "danger":"SEVERE", "level":7, "cost":24000, "color":"#719cff", "background":"res://assets/art/backgrounds/sector_4_plasma_storm.jpg", "resources":["titanium","gold","platinum","uranium","lumen_crystal","iridium","plasma_ore"], "weights":[12,12,15,14,16,16,15], "asteroids":56, "enemies":[1,2,3], "enemy_cap":4, "hazard":"ELECTRICAL STORMS", "description":"A permanent ion tempest. Telegraphed lightning fronts punish stationary mining crews but expose plasma ore."},
	{"name":"VOID FRONTIER", "code":"VF-05", "subtitle":"EVENTIDE REACH", "danger":"EXTREME", "level":11, "cost":60000, "color":"#b275f2", "background":"res://assets/art/backgrounds/sector_5_void_frontier.jpg", "resources":["platinum","uranium","lumen_crystal","iridium","plasma_ore","void_crystal","neutronium","quantum_core"], "weights":[10,10,13,15,16,16,14,6], "asteroids":60, "enemies":[2,3,4], "enemy_cap":5, "hazard":"VOID SHEAR", "description":"Space folds around ancient sentinels. Gravitational shear, impossible minerals and the richest claims known to humanity."}
]

const UPGRADES := [
	{"id":"laser_power", "name":"MINING LASER", "category":"MINING", "max":10, "cost":180, "growth":1.52, "icon":"laser", "description":"High-density emitter increases mining damage against every rock."},
	{"id":"mining_speed", "name":"BEAM FOCUS", "category":"MINING", "max":10, "cost":240, "growth":1.55, "icon":"focus", "description":"Narrow-band focusing improves mining rate and effective beam reach."},
	{"id":"cargo_capacity", "name":"CARGO BAY", "category":"SHIP", "max":12, "cost":200, "growth":1.48, "icon":"cargo", "description":"Expanded pressurized hold. Each level carries 24 additional units."},
	{"id":"engine_speed", "name":"ION THRUSTERS", "category":"ENGINE", "max":10, "cost":280, "growth":1.53, "icon":"engine", "description":"Higher exhaust velocity raises cruise and afterburner speed."},
	{"id":"acceleration", "name":"INERTIAL DAMPERS", "category":"ENGINE", "max":10, "cost":220, "growth":1.50, "icon":"engine", "description":"Faster acceleration and stronger braking for tight asteroid threading."},
	{"id":"hull", "name":"HULL PLATING", "category":"DEFENSE", "max":12, "cost":200, "growth":1.49, "icon":"hull", "description":"Layered composite armour increases maximum hull integrity."},
	{"id":"shield", "name":"SHIELD CAPACITOR", "category":"DEFENSE", "max":12, "cost":280, "growth":1.51, "icon":"shield", "description":"Raises shield capacity and accelerates shield recovery."},
	{"id":"energy_regen", "name":"FUSION REACTOR", "category":"ENGINE", "max":12, "cost":280, "growth":1.51, "icon":"reactor", "description":"Sustained reactor output for lasers, shields and afterburner."},
	{"id":"scanner", "name":"DEEP SCANNER", "category":"UTILITY", "max":8, "cost":340, "growth":1.56, "icon":"scanner", "description":"Extends target scan range and reveals mineral rarity at greater distances."},
	{"id":"drone_capacity", "name":"DRONE BAY", "category":"DRONES", "max":6, "cost":700, "growth":1.72, "icon":"drone", "description":"Adds one functional autonomous mining drone slot per level."},
	{"id":"weapon_damage", "name":"PULSE CANNONS", "category":"COMBAT", "max":10, "cost":360, "growth":1.55, "icon":"weapon", "description":"Increases pulse projectile damage and slightly reduces cooldown."},
	{"id":"auto_collection", "name":"TRACTOR ARRAY", "category":"UTILITY", "max":8, "cost":300, "growth":1.53, "icon":"tractor", "description":"Expands automatic ore collection radius and pulling force."},
	{"id":"armor", "name":"NANO WEAVE", "category":"DEFENSE", "max":8, "cost":500, "growth":1.58, "icon":"hull", "description":"Reduces incoming hull damage and slowly repairs the ship in flight."},
	{"id":"reactor", "name":"AUXILIARY CELLS", "category":"ENGINE", "max":10, "cost":320, "growth":1.52, "icon":"reactor", "description":"Adds 20 units of maximum reactor energy per level."},
	{"id":"drone_mining", "name":"DRONE OVERCLOCK", "category":"DRONES", "max":8, "cost":650, "growth":1.60, "icon":"drone", "description":"Improves visible drone mining power, flight speed and cargo capacity."},
	{"id":"research", "name":"XENO RESEARCH", "category":"RESEARCH", "max":8, "cost":900, "growth":1.65, "icon":"research", "description":"Increases experience gains and the chance of bonus rare-mineral finds."}
]

const SHIPS := [
	{"id":"prospector", "name":"PROSPECTOR MK II", "class":"LIGHT MINING VESSEL", "level":1, "cost":0, "texture":"res://assets/art/ships/ship_1.png", "hull":120, "shield":45, "energy":110, "cargo":80, "speed":310, "accel":390, "laser":20, "weapon":17, "description":"Balanced independent prospector. Responsive, efficient and built for the Ceres relay."},
	{"id":"harvester", "name":"HARVESTER BULK", "class":"HEAVY MINING VESSEL", "level":3, "cost":8500, "texture":"res://assets/art/ships/ship_2.png", "hull":210, "shield":70, "energy":150, "cargo":160, "speed":260, "accel":300, "laser":27, "weapon":18, "description":"A broad industrial platform with double base cargo and a reinforced mining rig."},
	{"id":"lancet", "name":"LANCET INTERCEPTOR", "class":"COMBAT PROSPECTOR", "level":6, "cost":22000, "texture":"res://assets/art/ships/ship_3.png", "hull":135, "shield":105, "energy":170, "cargo":105, "speed":410, "accel":520, "laser":29, "weapon":32, "description":"Fast response frame for dangerous claims. Superior shields, speed and pulse damage."},
	{"id":"leviathan", "name":"LEVIATHAN FORGE", "class":"AUTOMATED MINING FORTRESS", "level":10, "cost":52000, "texture":"res://assets/art/ships/ship_4.png", "hull":340, "shield":150, "energy":230, "cargo":260, "speed":275, "accel":320, "laser":40, "weapon":30, "description":"The ultimate mobile refinery. Enormous hold, powerful emitter and deep-space endurance."}
]

const ENEMIES := [
	{"name":"SCOUT DRONE", "texture":"res://assets/art/enemies/enemy_1.png", "health":48, "shield":0, "speed":210, "damage":8, "fire_rate":1.8, "range":440, "reward":110, "xp":35, "behavior":"orbit", "scale":0.60},
	{"name":"RAIDER SHIP", "texture":"res://assets/art/enemies/enemy_2.png", "health":105, "shield":20, "speed":175, "damage":12, "fire_rate":1.35, "range":520, "reward":260, "xp":70, "behavior":"strafe", "scale":0.78},
	{"name":"HEAVY DRONE", "texture":"res://assets/art/enemies/enemy_3.png", "health":230, "shield":65, "speed":105, "damage":20, "fire_rate":2.2, "range":570, "reward":520, "xp":135, "behavior":"siege", "scale":0.98},
	{"name":"PLASMA HUNTER", "texture":"res://assets/art/enemies/enemy_4.png", "health":180, "shield":100, "speed":235, "damage":19, "fire_rate":1.15, "range":620, "reward":780, "xp":190, "behavior":"hunter", "scale":0.88},
	{"name":"VOID SENTINEL", "texture":"res://assets/art/enemies/enemy_5.png", "health":440, "shield":210, "speed":135, "damage":29, "fire_rate":1.45, "range":720, "reward":1600, "xp":390, "behavior":"sentinel", "scale":1.14}
]

const ACHIEVEMENTS := [
	{"id":"first_cut", "name":"FIRST CUT", "description":"Destroy your first asteroid."},
	{"id":"prospector", "name":"PROSPECTOR", "description":"Mine 25 asteroids."},
	{"id":"industrialist", "name":"INDUSTRIALIST", "description":"Mine 100 asteroids."},
	{"id":"first_sale", "name":"OPEN FOR BUSINESS", "description":"Sell your first cargo load."},
	{"id":"wealth", "name":"BELT MAGNATE", "description":"Hold 25,000 credits."},
	{"id":"first_blood", "name":"HOSTILE TAKEOVER", "description":"Destroy a hostile ship."},
	{"id":"veteran", "name":"DEEP SPACE VETERAN", "description":"Destroy 25 hostile ships."},
	{"id":"automation", "name":"AUTOMATION AGE", "description":"Deploy your first mining drone."},
	{"id":"fleet", "name":"MECHANICAL SYMPHONY", "description":"Operate four drones simultaneously."},
	{"id":"crystal", "name":"LUMEN PROSPECTOR", "description":"Discover a Lumen Crystal."},
	{"id":"void", "name":"BEYOND THE MAP", "description":"Discover a Void Crystal."},
	{"id":"frontier", "name":"FRONTIER LEGEND", "description":"Unlock all five sectors."}
]

static func resource_index(id: String) -> int:
	for i in range(RESOURCES.size()):
		if RESOURCES[i].id == id:
			return i
	return -1

static func get_resource(id: String) -> Dictionary:
	var i := resource_index(id)
	return RESOURCES[i] if i >= 0 else RESOURCES[0]

static func get_upgrade(id: String) -> Dictionary:
	for upgrade in UPGRADES:
		if upgrade.id == id:
			return upgrade
	return {}

static func get_ship(id: String) -> Dictionary:
	for ship in SHIPS:
		if ship.id == id:
			return ship
	return SHIPS[0]

static func upgrade_cost(id: String, level: int) -> int:
	var upgrade := get_upgrade(id)
	if upgrade.is_empty():
		return 999999999
	return int(round(float(upgrade.cost) * pow(float(upgrade.growth), level) / 10.0) * 10.0)

static func xp_for_level(level: int) -> int:
	return int(450 + pow(max(1, level), 1.55) * 280)

static func rarity_name(rarity: int) -> String:
	return ["COMMON", "UNCOMMON", "RARE", "EPIC", "LEGENDARY", "ANOMALOUS"][clampi(rarity, 0, 5)]

static func empty_resource_map() -> Dictionary:
	var result := {}
	for resource in RESOURCES:
		result[resource.id] = 0
	return result
