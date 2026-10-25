extends Node
## ENet multiplayer: LAN / direct-IP co-op for up to four survivors.
## Host is authoritative; all gameplay replication lives in Session.
class_name GameNet

const PORT := 25565
const MAX_PLAYERS := 4

var session
var peer: ENetMultiplayerPeer
var status: String = "idle"

func _init(sess) -> void:
	session = sess
	name = "GameNet"

func start(mode: String, ip: String) -> void:
	peer = ENetMultiplayerPeer.new()
	var err: int
	if mode == "host":
		err = peer.create_server(PORT, MAX_PLAYERS)
	else:
		err = peer.create_client(ip if ip != "" else "127.0.0.1", PORT)
	if err != OK:
		session.notify("Network error %d — check IP/firewall" % err)
		status = "error"
		return
	multiplayer.multiplayer_peer = peer
	multiplayer.peer_connected.connect(_on_peer_connected)
	multiplayer.peer_disconnected.connect(_on_peer_disconnected)
	multiplayer.connected_to_server.connect(_on_connected)
	multiplayer.connection_failed.connect(_on_failed)
	status = mode
	if mode == "host":
		session.net_host_ready()

func _on_connected() -> void:
	session.net_client_ready()

func _on_failed() -> void:
	status = "failed"
	session.notify("Connection failed.")

func _on_peer_connected(id: int) -> void:
	if not multiplayer.is_server():
		return
	# replicate every existing player to the newcomer first
	for p in session.players:
		session._rpc_spawn_player.rpc_id(id, p.peer_id)
	# then spawn the newcomer everywhere
	session._rpc_spawn_player.rpc(id)

func _on_peer_disconnected(id: int) -> void:
	session._rpc_remove_player.rpc(id)
	for p in session.players:
		if p.peer_id == id:
			session.players.erase(p)
			p.queue_free()
			break

func leave() -> void:
	if peer:
		peer.close()
	multiplayer.multiplayer_peer = null
