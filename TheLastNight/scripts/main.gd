extends Node
## Bootstrap: input mapping, global services, menu -> session lifecycle.

const GameDataScript := preload("res://scripts/game_data.gd")
const AssetLibScript := preload("res://scripts/asset_lib.gd")
const UIScript := preload("res://scripts/ui/ui_manager.gd")
const SessionScript := preload("res://scripts/session.gd")

var data: GameData
var assets: AssetLib
var ui: UIManager
var session: Session

func _ready() -> void:
	process_mode = Node.PROCESS_MODE_ALWAYS
	InputSetup.define_all()
	data = GameDataScript.new()
	add_child(data) # loads all JSON design data
	assets = AssetLibScript.new()
	ui = UIScript.new(assets)
	add_child(ui)
	ui.action_requested.connect(_on_ui_action)
	_apply_settings()
	# preload every audio stream so first hits never stall
	for n in ["sfx_pistol", "sfx_shotgun", "sfx_walker", "sfx_scream", "amb_drone"]:
		assets.sound(n)

func _apply_settings() -> void:
	var s: Dictionary = {}
	var f := FileAccess.open("user://settings.json", FileAccess.READ)
	if f:
		var parsed = JSON.parse_string(f.get_as_text())
		if parsed is Dictionary:
			s = parsed
	AudioServer.set_bus_volume_db(0, float(s.get("master", 0.0)))

# -------------------------------------------------------------- session flow
func _on_ui_action(action: String, payload: String) -> void:
	match action:
		"single", "host", "join":
			_start_session(action, payload)
		"quit_to_menu":
			_end_session()

func _start_session(mode: String, ip: String) -> void:
	ui.loading_rect.visible = true
	ui.set_load(20)
	# loading screen needs a frame before the heavy world build
	await RenderingServer.frame_post_draw
	ui.set_load(45)
	session = SessionScript.new()
	add_child(session)
	ui.session = session
	session.boot(data, ui, mode, ip)
	ui.set_load(85)
	await RenderingServer.frame_post_draw
	ui.set_load(100)
	ui.loading_rect.visible = false
	ui.main_menu.visible = false
	ui.hud.visible = true
	Input.mouse_mode = Input.MOUSE_MODE_CAPTURED

func _end_session() -> void:
	if session:
		if session.net:
			session.net.leave()
		session.queue_free()
		session = null
	ui.session = null
	ui.player = null
	ui.show_main_menu()
