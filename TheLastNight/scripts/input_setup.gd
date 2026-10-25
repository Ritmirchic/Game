extends Object
## Registers the gameplay input map. Called by Main and by headless test runners.
class_name InputSetup

static func define_all() -> void:
	_add_key("move_forward", KEY_W)
	_add_key("move_back", KEY_S)
	_add_key("move_left", KEY_A)
	_add_key("move_right", KEY_D)
	_add_key("jump", KEY_SPACE)
	_add_key("sprint", KEY_SHIFT)
	_add_key("reload", KEY_R)
	_add_key("interact", KEY_E)
	_add_key("flashlight", KEY_F)
	_add_key("medkit", KEY_H)
	_add_key("pause", KEY_ESCAPE)
	for i in 7:
		_add_key("weapon_%d" % (i + 1), KEY_1 + i)
	_add_mouse("fire", MOUSE_BUTTON_LEFT)
	_add_mouse("aim", MOUSE_BUTTON_RIGHT)

static func _add_key(action: String, keycode: int) -> void:
	if not InputMap.has_action(action):
		InputMap.add_action(action)
	var ev := InputEventKey.new()
	ev.physical_keycode = keycode
	InputMap.action_add_event(action, ev)

static func _add_mouse(action: String, button: int) -> void:
	if not InputMap.has_action(action):
		InputMap.add_action(action)
	var ev := InputEventMouseButton.new()
	ev.button_index = button
	InputMap.action_add_event(action, ev)
