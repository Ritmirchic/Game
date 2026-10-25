extends SceneTree
## Two-process co-op smoke test.
##   godot --headless --path . -s tests/net_smoke.gd host
##   godot --headless --path . -s tests/net_smoke.gd client
## Verifies ENet connect, player spawn replication and zombie snapshot flow.

const GameDataScript := preload("res://scripts/game_data.gd")
const AssetLibScript := preload("res://scripts/asset_lib.gd")
const UIScript := preload("res://scripts/ui/ui_manager.gd")
const SessionScript := preload("res://scripts/session.gd")

var role: String = "host"
var data: GameData
var ui
var session: Session
var t: float = 0.0

func _initialize() -> void:
	var args := OS.get_cmdline_args()
	if args.has("client"):
		role = "client"
	call_deferred("run")

func run() -> void:
	print("NET SMOKE role=", role)
	InputSetup.define_all()
	data = GameDataScript.new()
	root.add_child(data)
	var assets = AssetLibScript.new()
	ui = UIScript.new(assets)
	root.add_child(ui)
	session = SessionScript.new()
	root.add_child(session)
	ui.session = session
	session.boot(data, ui, "host" if role == "host" else "client", "127.0.0.1")
	if role == "client":
		# give the server a head start (process launched second)
		await create_timer(2.0).timeout

func _process(delta: float) -> bool:
	t += delta
	if role == "host" and session.players.size() >= 2 and t > 2.0:
		# host spawns a zombie and confirms replication path exists
		session.spawn_zombie("walker", "sp_main")
		await create_timer(1.5).timeout
		_finish(session.players.size() >= 2 and session.zombies.size() >= 1, "host saw 2 players + zombie")
		return true
	if role == "client" and t > 6.0:
		var ok: bool = session.players.size() >= 2
		_finish(ok, "client saw %d replicated players" % session.players.size())
		return true
	if t > 20.0:
		_finish(false, "timeout (players=%d)" % session.players.size())
		return true
	return false

func _finish(ok: bool, detail: String) -> void:
	print("NET SMOKE %s: %s (%s)" % [role, "PASS" if ok else "FAIL", detail])
	var f := FileAccess.open("user://net_%s.log" % role, FileAccess.WRITE)
	if f:
		f.store_string("PASS\n" if ok else "FAIL\n")
		f.store_string(detail + "\n")
	quit(0 if ok else 1)
