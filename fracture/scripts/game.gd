extends Node3D

const WorldScript = preload("res://scripts/voxel_world.gd")
const SAVE_PATH := "user://fracture_sandbox.json"
var world: Node3D
var player: CharacterBody3D
var camera: Camera3D
var ui: CanvasLayer
var hud: Control
var menu: Control
var status: Label
var tool_label: Label
var hint: Label
var menu_status: Label
var tool_mesh: Node3D
var audio: AudioStreamPlayer3D
var flashlight: SpotLight3D
var sensitivity := 0.0022
var selected := 0
var tools := ["SLEDGEHAMMER", "BREACH SHOTGUN", "REMOTE CHARGE"]
var charges: Array[Node3D] = []
var paused := true
var swing := 0.0
var cooldown := 0.0
var pitch := 0.0
var rng := RandomNumberGenerator.new()

func _ready() -> void:
 rng.seed = 228
 setup_input()
 setup_environment()
 world = WorldScript.new(); world.name = "DestructibleWorld"; add_child(world)
 world.generate()
 setup_player()
 setup_ui()
 show_menu(true)

func setup_input() -> void:
 bind_key("move_forward", KEY_W); bind_key("move_back", KEY_S)
 bind_key("move_left", KEY_A); bind_key("move_right", KEY_D)
 bind_key("jump", KEY_SPACE); bind_key("sprint", KEY_SHIFT)
 bind_key("crouch", KEY_CTRL); bind_key("flashlight", KEY_F)
 bind_key("pause", KEY_ESCAPE)

func bind_key(action: String, key: Key) -> void:
 if not InputMap.has_action(action): InputMap.add_action(action)
 var event := InputEventKey.new(); event.physical_keycode = key
 InputMap.action_add_event(action, event)

func setup_environment() -> void:
 var env := WorldEnvironment.new()
 var e := Environment.new()
 e.background_mode = Environment.BG_COLOR
 e.background_color = Color(0.50,0.66,0.72)
 e.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
 e.ambient_light_color = Color(0.70,0.78,0.76)
 e.ambient_light_energy = 0.8
 e.tonemap_mode = Environment.TONE_MAPPER_FILMIC
 e.fog_enabled = true; e.fog_light_color = Color(0.55,0.66,0.68); e.fog_density = 0.003
 env.environment = e; add_child(env)
 var sun := DirectionalLight3D.new()
 sun.rotation_degrees = Vector3(-55,-30,0); sun.light_energy = 1.1; sun.shadow_enabled = true
 add_child(sun)

func setup_player() -> void:
 player = CharacterBody3D.new(); player.name = "Player"; add_child(player)
 player.position = Vector3(-10,0.05,8)
 var shape := CapsuleShape3D.new(); shape.radius = 0.33; shape.height = 1.7
 var collider := CollisionShape3D.new(); collider.shape = shape; collider.position.y = 0.85; player.add_child(collider)
 camera = Camera3D.new(); camera.position.y = 1.55; camera.fov = 78; player.add_child(camera)
 flashlight = SpotLight3D.new(); flashlight.light_energy = 2.0; flashlight.spot_range = 18; flashlight.spot_angle = 35; flashlight.visible = false; camera.add_child(flashlight)
 audio = AudioStreamPlayer3D.new(); audio.max_distance = 30; player.add_child(audio)
 tool_mesh = Node3D.new(); camera.add_child(tool_mesh)
 tool_mesh.position = Vector3(0.48,-0.40,-0.85)
 update_tool_model()

func model_box(parent: Node3D, size: Vector3, pos: Vector3, color: Color) -> void:
 var m := MeshInstance3D.new(); var box_mesh := BoxMesh.new(); box_mesh.size = size
 var mat := StandardMaterial3D.new(); mat.albedo_color = color; mat.roughness = 0.75
 box_mesh.material = mat; m.mesh = box_mesh; m.position = pos; parent.add_child(m)

func update_tool_model() -> void:
 for n in tool_mesh.get_children(): n.queue_free()
 if selected == 0:
  model_box(tool_mesh, Vector3(0.09,0.6,0.09), Vector3(0,-0.15,0), Color(0.44,0.27,0.14))
  model_box(tool_mesh, Vector3(0.42,0.16,0.17), Vector3(0,0.18,0), Color(0.18,0.21,0.22))
 elif selected == 1:
  model_box(tool_mesh, Vector3(0.16,0.16,0.70), Vector3(0,0,-0.12), Color(0.16,0.18,0.19))
  model_box(tool_mesh, Vector3(0.15,0.19,0.25), Vector3(0,-0.07,0.20), Color(0.38,0.25,0.15))
 else:
  model_box(tool_mesh, Vector3(0.32,0.22,0.14), Vector3.ZERO, Color(0.36,0.24,0.14))
  model_box(tool_mesh, Vector3(0.15,0.05,0.15), Vector3(0,0.13,0), Color(0.75,0.17,0.08))

func _unhandled_input(event: InputEvent) -> void:
 if event.is_action_pressed("pause"):
  show_menu(not paused); get_viewport().set_input_as_handled(); return
 if paused: return
 if event is InputEventMouseMotion and Input.mouse_mode == Input.MOUSE_MODE_CAPTURED:
  player.rotate_y(-event.relative.x * sensitivity)
  pitch = clampf(pitch - event.relative.y * sensitivity, -1.5, 1.5)
  camera.rotation.x = pitch
 elif event is InputEventMouseButton and event.pressed:
  if event.button_index == MOUSE_BUTTON_LEFT: use_tool()
  elif event.button_index == MOUSE_BUTTON_RIGHT and selected == 2: detonate()
  elif event.button_index == MOUSE_BUTTON_WHEEL_UP: select_tool(posmod(selected - 1, 3))
  elif event.button_index == MOUSE_BUTTON_WHEEL_DOWN: select_tool(posmod(selected + 1, 3))
 elif event is InputEventKey and event.pressed and not event.echo:
  if event.physical_keycode >= KEY_1 and event.physical_keycode <= KEY_3: select_tool(event.physical_keycode - KEY_1)
  if event.is_action_pressed("flashlight"): flashlight.visible = not flashlight.visible

func _physics_process(delta: float) -> void:
 if paused: return
 var input := Input.get_vector("move_left","move_right","move_forward","move_back")
 var direction := (player.transform.basis * Vector3(input.x,0,input.y)).normalized()
 var speed := 7.5 if Input.is_action_pressed("sprint") else 4.5
 if Input.is_action_pressed("crouch"): speed = 2.5
 player.velocity.x = move_toward(player.velocity.x, direction.x * speed, delta * 24)
 player.velocity.z = move_toward(player.velocity.z, direction.z * speed, delta * 24)
 if not player.is_on_floor(): player.velocity.y -= 18 * delta
 elif Input.is_action_just_pressed("jump"): player.velocity.y = 6.5
 player.move_and_slide()
 camera.position.y = move_toward(camera.position.y, 1.0 if Input.is_action_pressed("crouch") else 1.55, delta * 4.0)
 cooldown = maxf(0, cooldown-delta)
 swing = maxf(0, swing-delta*3.5)
 tool_mesh.rotation.x = -sin(swing*PI)*0.8
 if player.position.y < -20: player.position = Vector3(-10,2,8); player.velocity = Vector3.ZERO

func select_tool(i: int) -> void:
 selected = i; update_tool_model(); update_hud()

func ray_hit(distance: float, offset := Vector2.ZERO) -> Dictionary:
 var direction := -camera.global_basis.z + camera.global_basis.x * offset.x + camera.global_basis.y * offset.y
 var query := PhysicsRayQueryParameters3D.create(camera.global_position, camera.global_position + direction.normalized() * distance)
 query.exclude = [player.get_rid()]
 return get_world_3d().direct_space_state.intersect_ray(query)

func use_tool() -> void:
 if cooldown > 0: return
 cooldown = 0.45 if selected == 0 else 0.7
 swing = 1.0
 var hit := ray_hit(4.0 if selected == 0 else 45.0)
 if hit.is_empty(): return
 var point: Vector3 = hit.position - hit.normal * 0.18
 if selected == 0:
  var n := world.damage(point,0.75,1.6)
  play_impact(false); notify("IMPACT  /  %d CELLS REMOVED" % n)
 elif selected == 1:
  var total := 0
  for i in range(7):
   var pellet := ray_hit(30.0,Vector2(rng.randf_range(-0.035,0.035),rng.randf_range(-0.035,0.035)))
   if not pellet.is_empty(): total += world.damage(pellet.position - pellet.normal * 0.15,0.38,1.5)
  play_impact(true); notify("BREACH  /  %d CELLS REMOVED" % total)
 else:
  var charge := Node3D.new(); charge.position = hit.position + hit.normal * 0.08
  add_child(charge)
  model_box(charge,Vector3(0.24,0.24,0.24),Vector3.ZERO,Color(0.95,0.30,0.07))
  charges.append(charge); play_impact(false); notify("CHARGE PLACED  /  RIGHT CLICK TO DETONATE")
 update_hud()

func detonate() -> void:
 if charges.is_empty(): notify("NO CHARGES PLACED"); return
 var count := 0
 for charge in charges:
  if is_instance_valid(charge):
   count += world.damage(charge.global_position,3.0,4.0)
   charge.queue_free()
 charges.clear(); play_impact(true); notify("DETONATION  /  %d CELLS REMOVED" % count); update_hud()

func play_impact(loud: bool) -> void:
 # Original synthesized impact; no external audio assets.
 var sample_rate := 22050
 var length := 0.35 if loud else 0.12
 var samples := PackedByteArray(); samples.resize(int(sample_rate * length) * 2)
 for i in range(int(sample_rate * length)):
  var t := float(i) / sample_rate
  var envelope := exp(-t * (9.0 if loud else 24.0))
  var value := (rng.randf_range(-1,1)*0.6 + sin(t*TAU*(60.0 if loud else 140.0))*0.4) * envelope
  var v := int(clampf(value,-1,1) * 22000)
  samples.encode_s16(i*2,v)
 var stream := AudioStreamWAV.new(); stream.format = AudioStreamWAV.FORMAT_16_BITS; stream.mix_rate = sample_rate; stream.data = samples
 audio.stream = stream; audio.play()

func setup_ui() -> void:
 ui = CanvasLayer.new(); add_child(ui)
 hud = Control.new(); hud.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT); hud.mouse_filter = Control.MOUSE_FILTER_IGNORE; ui.add_child(hud)
 var cross := Label.new(); cross.text = "+"; cross.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
 cross.set_anchors_preset(Control.PRESET_CENTER); cross.position = Vector2(-12,-18); cross.size = Vector2(24,36)
 cross.add_theme_font_size_override("font_size",28); cross.add_theme_color_override("font_color",Color(1,0.9,0.7)); hud.add_child(cross)
 var top := Label.new(); top.text = "F / T D     •     WESTHAVEN PROVING GROUND"; top.position = Vector2(30,24)
 top.add_theme_font_size_override("font_size",14); top.add_theme_color_override("font_color",Color(0.95,0.82,0.59)); hud.add_child(top)
 tool_label = Label.new(); tool_label.set_anchors_preset(Control.PRESET_BOTTOM_LEFT); tool_label.position = Vector2(30,-105)
 tool_label.add_theme_font_size_override("font_size",25); hud.add_child(tool_label)
 hint = Label.new(); hint.set_anchors_preset(Control.PRESET_BOTTOM_LEFT); hint.position = Vector2(30,-68)
 hint.add_theme_font_size_override("font_size",14); hint.add_theme_color_override("font_color",Color(0.85,0.83,0.76)); hud.add_child(hint)
 status = Label.new(); status.set_anchors_preset(Control.PRESET_CENTER_TOP); status.position = Vector2(-240,35); status.size.x = 480
 status.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER; status.add_theme_font_size_override("font_size",15); hud.add_child(status)
 update_hud()
 menu = Control.new(); menu.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT); ui.add_child(menu)
 var shade := ColorRect.new(); shade.color = Color(0.025,0.045,0.05,0.94); shade.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT); menu.add_child(shade)
 var column := VBoxContainer.new(); column.set_anchors_preset(Control.PRESET_CENTER_LEFT); column.position = Vector2(76,-285); column.size = Vector2(500,570)
 column.add_theme_constant_override("separation",11); menu.add_child(column)
 var eyebrow := Label.new(); eyebrow.text = "INDEPENDENT DEMOLITION LAB  /  01"; eyebrow.add_theme_font_size_override("font_size",14); eyebrow.add_theme_color_override("font_color",Color(1,0.52,0.22)); column.add_child(eyebrow)
 var title := Label.new(); title.text = "FRACTURE"; title.add_theme_font_size_override("font_size",58); column.add_child(title)
 var sub := Label.new(); sub.text = "T O T A L   D E S T R U C T I O N"; sub.add_theme_font_size_override("font_size",16); sub.add_theme_color_override("font_color",Color(0.75,0.81,0.77)); column.add_child(sub)
 var description := Label.new(); description.text = "WESTHAVEN PROVING GROUND\nA small, editable destruction sandbox."; description.add_theme_font_size_override("font_size",16); description.add_theme_color_override("font_color",Color(0.72,0.76,0.72)); column.add_child(description)
 add_button(column,"ENTER SANDBOX",func():
  if world.dirty.is_empty(): show_menu(false)
  else: notify("PREPARING SITE — PLEASE WAIT") )
 add_button(column,"SAVE WORLD",save_world)
 add_button(column,"LOAD WORLD",load_world)
 add_button(column,"RESET WORLD",reset_world)
 add_button(column,"TOGGLE FULLSCREEN",func():
  DisplayServer.window_set_mode(DisplayServer.WINDOW_MODE_WINDOWED if DisplayServer.window_get_mode() == DisplayServer.WINDOW_MODE_FULLSCREEN else DisplayServer.WINDOW_MODE_FULLSCREEN))
 add_button(column,"QUIT",func(): get_tree().quit())
 menu_status = Label.new(); menu_status.add_theme_font_size_override("font_size",13); menu_status.add_theme_color_override("font_color",Color(1,0.52,0.22)); column.add_child(menu_status)
 var controls := Label.new(); controls.text = "WASD MOVE  /  MOUSE LOOK  /  SHIFT SPRINT  /  CTRL CROUCH\nSPACE JUMP  /  1–3 OR WHEEL TOOLS  /  F FLASHLIGHT\nLEFT CLICK USE  /  RIGHT CLICK DETONATE  /  ESC MENU"
 controls.add_theme_font_size_override("font_size",12); controls.add_theme_color_override("font_color",Color(0.62,0.69,0.67)); column.add_child(controls)
 var note := Label.new(); note.text = "FIELD NOTES  /  PROTOTYPE\n\nEvery visible structure in the site is made\nof editable voxels. Breach a wall, enter the\nhouse, and prepare a remote demolition.\n\nThe site ends at the edge of the terrain.\n\nWorld saves are stored locally."
 note.set_anchors_preset(Control.PRESET_CENTER_RIGHT); note.position = Vector2(-435,-140); note.size = Vector2(370,300)
 note.add_theme_font_size_override("font_size",16); note.add_theme_color_override("font_color",Color(0.65,0.73,0.69)); menu.add_child(note)

func add_button(parent: Control, label: String, action: Callable) -> void:
 var button := Button.new(); button.text = label; button.custom_minimum_size = Vector2(320,35); button.alignment = HORIZONTAL_ALIGNMENT_LEFT
 var normal := StyleBoxFlat.new(); normal.bg_color = Color(0.10,0.15,0.15); normal.content_margin_left = 16
 var hover := normal.duplicate(); hover.bg_color = Color(0.58,0.25,0.10)
 button.add_theme_stylebox_override("normal",normal); button.add_theme_stylebox_override("hover",hover); button.add_theme_stylebox_override("pressed",hover)
 button.add_theme_font_size_override("font_size",14); button.pressed.connect(action); parent.add_child(button)

func show_menu(value: bool) -> void:
 paused = value
 menu.visible = value; hud.visible = not value
 Input.mouse_mode = Input.MOUSE_MODE_VISIBLE if value else Input.MOUSE_MODE_CAPTURED

func update_hud() -> void:
 if tool_label == null: return
 tool_label.text = "%02d  /  %s" % [selected+1,tools[selected]]
 hint.text = "LMB  USE    •    RMB  DETONATE (%d)    •    ESC  MENU" % charges.size() if selected == 2 else "LMB  USE    •    1–3  SWITCH TOOL    •    ESC  MENU"

func notify(text: String) -> void:
 status.text = text
 if menu_status != null: menu_status.text = text

func save_world() -> void:
 var file := FileAccess.open(SAVE_PATH,FileAccess.WRITE)
 if file == null: notify("SAVE FAILED"); return
 var data := {"version":1,"removed":world.serialize_removed(),"position":[player.position.x,player.position.y,player.position.z],"yaw":player.rotation.y,"pitch":pitch}
 file.store_string(JSON.stringify(data)); file.close(); notify("WORLD SAVED")

func load_world() -> void:
 if not FileAccess.file_exists(SAVE_PATH): notify("NO SAVE FOUND"); return
 var file := FileAccess.open(SAVE_PATH,FileAccess.READ)
 if file == null: notify("LOAD FAILED"); return
 var data: Variant = JSON.parse_string(file.get_as_text())
 if not data is Dictionary or data.get("version",0) != 1 or not data.get("removed") is Array or not data.get("position") is Array or data.position.size() != 3:
  notify("INVALID SAVE"); return
 clear_charges(); world.generate(); world.apply_removed(data.removed)
 player.position = Vector3(float(data.position[0]),float(data.position[1]),float(data.position[2])); player.velocity = Vector3.ZERO
 player.rotation.y = float(data.get("yaw",0)); pitch = clampf(float(data.get("pitch",0)),-1.5,1.5); camera.rotation.x = pitch
 notify("WORLD LOADED"); update_hud()

func reset_world() -> void:
 clear_charges(); world.generate(); player.position = Vector3(-10,0.05,8); player.velocity = Vector3.ZERO
 notify("SITE RESET"); update_hud()

func clear_charges() -> void:
 for charge in charges:
  if is_instance_valid(charge): charge.queue_free()
 charges.clear()
