extends Node2D
const Catalog = preload("res://scripts/catalog.gd")
const Profile = preload("res://scripts/profile.gd")
const Pilot = preload("res://scripts/pilot.gd")
var profile = Profile.new()
var player: CharacterBody2D
var camera: Camera2D
var ui: CanvasLayer
var hud: Control
var panel: PanelContainer
var screen = "menu"
var sector = 0
var rocks: Array = []
var drops: Array = []
var enemies: Array = []
var bullets: Array = []
var drones: Array = []
var effects: Array = []
var textures = {}
var clock = 0.0
var health = 100.0
var shield = 60.0
var energy = 100.0
var cooldown = 0.0
var enemy_timer = 0.0
var save_timer = 0.0
var hazard_timer = 0.0
var target = -1
var mining = false
var beam_end = Vector2.ZERO
var status_label: Label
var cargo_label: Label
var nav_label: Label
var objective_label: Label
var notification: Label
var toast_time = 0.0
var hull_bar: ProgressBar
var shield_bar: ProgressBar
var energy_bar: ProgressBar
var font: Font
var mono: Font
var rng=RandomNumberGenerator.new()
var station_pos=Vector2(-350,0)
var sound_player: AudioStreamPlayer
var audio: AudioStreamGeneratorPlayback
var audio_time=0.0
var accent=Color("67ded2")
var dim=Color("8a9dab")

func _ready():
 rng.seed=764812
 font=load("res://assets/Interface.ttf")
 mono=load("res://assets/Telemetry.ttf")
 for i in range(5):
  textures["bg"+str(i)]=load("res://assets/nebula_%d.jpg"%i)
  textures["enemy"+str(i)]=load("res://assets/enemy_%d.png"%i)
  for j in range(5): textures["rock%d_%d"%[i,j]]=load("res://assets/rock_%d_%d.png"%[i,j])
 for i in range(3): textures["ship"+str(i)]=load("res://assets/ship_%d.png"%i)
 for i in range(15): textures["mineral"+str(i)]=load("res://assets/mineral_%d.png"%i)
 textures.station=load("res://assets/station.png")
 textures.drone=load("res://assets/drone.png")
 profile.read_data()
 player=Pilot.new()
 player.name="Pilot"
 var collision=CollisionShape2D.new()
 var shape=CircleShape2D.new()
 shape.radius=24
 collision.shape=shape
 player.add_child(collision)
 add_child(player)
 camera=Camera2D.new()
 camera.position_smoothing_enabled=true
 camera.position_smoothing_speed=5
 player.add_child(camera)
 ui=CanvasLayer.new()
 add_child(ui)
 var theme=Theme.new()
 theme.default_font=font
 theme.default_font_size=15
 hud=Control.new()
 hud.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
 hud.mouse_filter=Control.MOUSE_FILTER_IGNORE
 hud.theme=theme
 ui.add_child(hud)
 setup_hud()
 setup_sound()
 generate_sector(0)
 apply_stats()
 show_menu()
 if "--smoke" in OS.get_cmdline_user_args():
  screen="flight"
  panel.queue_free()
  panel=null
  player.active=true
  await get_tree().create_timer(3).timeout
  print("RUNTIME_SMOKE_OK rocks=",rocks.size()," drones=",drones.size()," player=",player.position)
  get_tree().quit()

func setup_sound():
 sound_player=AudioStreamPlayer.new()
 var generator=AudioStreamGenerator.new()
 generator.mix_rate=22050
 generator.buffer_length=0.12
 sound_player.stream=generator
 add_child(sound_player)
 sound_player.play()
 audio=sound_player.get_stream_playback()

func synth_audio():
 if audio==null: return
 var count=mini(audio.get_frames_available(),4096)
 var volume=float(profile.settings.volume)*0.12
 for i in range(count):
  audio_time+=1.0/22050.0
  var value=sin(audio_time*TAU*55)*0.10+sin(audio_time*TAU*82.41)*0.035
  if mining: value+=sin(audio_time*TAU*330)*0.11+sin(audio_time*TAU*660)*0.04
  if player.thrust.length()>0.1 and screen=="flight": value+=rng.randf_range(-0.07,0.07)
  audio.push_frame(Vector2.ONE*value*volume)

func style(bg: Color, border: Color, radius=8) -> StyleBoxFlat:
 var s=StyleBoxFlat.new()
 s.bg_color=bg
 s.border_color=border
 s.set_border_width_all(1)
 s.set_corner_radius_all(radius)
 s.content_margin_left=18
 s.content_margin_right=18
 s.content_margin_top=12
 s.content_margin_bottom=12
 return s

func label(text: String, size=16, color=Color("dce9ed")) -> Label:
 var l=Label.new()
 l.text=text
 l.add_theme_font_size_override("font_size",size)
 l.add_theme_color_override("font_color",color)
 return l

func button(text: String, action: Callable, primary=false) -> Button:
 var b=Button.new()
 b.text=text
 b.custom_minimum_size=Vector2(0,43)
 b.mouse_default_cursor_shape=Control.CURSOR_POINTING_HAND
 b.add_theme_stylebox_override("normal",style(Color("163439") if primary else Color("101f30"),Color("4d8e8a") if primary else Color("293f50")))
 b.add_theme_stylebox_override("hover",style(Color("25474e"),accent))
 b.add_theme_stylebox_override("pressed",style(Color("305760"),accent))
 b.add_theme_stylebox_override("disabled",style(Color("0c1522"),Color("1c2a39")))
 b.add_theme_color_override("font_color",accent if primary else Color("cfdee6"))
 b.add_theme_color_override("font_disabled_color",Color("526578"))
 b.pressed.connect(action)
 return b

func setup_hud():
 var top=PanelContainer.new()
 top.set_anchors_and_offsets_preset(Control.PRESET_TOP_WIDE)
 top.offset_left=24;top.offset_top=20;top.offset_right=-24;top.offset_bottom=108
 top.add_theme_stylebox_override("panel",style(Color(0.025,0.055,0.09,0.95),Color("294253")))
 hud.add_child(top)
 var row=HBoxContainer.new();row.add_theme_constant_override("separation",28);top.add_child(row)
 var brand=VBoxContainer.new();row.add_child(brand)
 brand.add_child(label("SPACE MINER",22,accent));brand.add_child(label("H E L I X   E X P E D I T I O N",10,dim))
 var sep=VSeparator.new();row.add_child(sep)
 var loc=VBoxContainer.new();loc.size_flags_horizontal=Control.SIZE_EXPAND_FILL;row.add_child(loc)
 nav_label=label("",17);loc.add_child(nav_label)
 objective_label=label("Mine asteroids. Return to Helix refinery to trade.",12,dim);loc.add_child(objective_label)
 var readouts=VBoxContainer.new();row.add_child(readouts)
 status_label=label("",15,Color("eac895"));status_label.add_theme_font_override("font",mono);readouts.add_child(status_label)
 cargo_label=label("",12,dim);readouts.add_child(cargo_label)
 var bottom=PanelContainer.new()
 bottom.set_anchors_and_offsets_preset(Control.PRESET_BOTTOM_WIDE)
 bottom.offset_left=24;bottom.offset_right=-24;bottom.offset_top=-104;bottom.offset_bottom=-20
 bottom.add_theme_stylebox_override("panel",style(Color(0.025,0.055,0.09,0.96),Color("294253")))
 hud.add_child(bottom)
 var br=HBoxContainer.new();br.add_theme_constant_override("separation",18);bottom.add_child(br)
 for pair in [["HULL",Color("eeac8b")],["SHIELD",Color("67c4f1")],["ENERGY",accent]]:
  var box=VBoxContainer.new();br.add_child(box);box.add_child(label(pair[0],10,dim))
  var bar=ProgressBar.new();bar.custom_minimum_size=Vector2(115,12);bar.show_percentage=false
  bar.add_theme_stylebox_override("background",style(Color("16283a"),Color("16283a"),2))
  var st=StyleBoxFlat.new();st.bg_color=pair[1];st.set_corner_radius_all(2);bar.add_theme_stylebox_override("fill",st)
  box.add_child(bar)
  if pair[0]=="HULL": hull_bar=bar
  elif pair[0]=="SHIELD": shield_bar=bar
  else: energy_bar=bar
 var spacer=Control.new();spacer.size_flags_horizontal=Control.SIZE_EXPAND_FILL;br.add_child(spacer)
 for pair in [["I  Cargo","cargo"],["U  Workshop","upgrades"],["M  Sectors","map"],["Esc  Menu","pause"]]:
  br.add_child(button(pair[0],open_screen.bind(pair[1])))
 var keys=label("W A S D   thrust     /     LMB   mine     /     RMB or SPACE   fire     /     E   dock",12,dim)
 keys.set_anchors_and_offsets_preset(Control.PRESET_BOTTOM_LEFT)
 keys.position=Vector2(32,-137);hud.add_child(keys)
 notification=label("",16,accent)
 notification.set_anchors_and_offsets_preset(Control.PRESET_CENTER_TOP)
 notification.position=Vector2(-310,127);notification.size=Vector2(620,40);notification.horizontal_alignment=HORIZONTAL_ALIGNMENT_CENTER
 hud.add_child(notification)

func clear_panel():
 if is_instance_valid(panel):
  hud.remove_child(panel)
  panel.queue_free()
 panel=null

func make_panel(title: String, subtitle: String, width=920) -> VBoxContainer:
 clear_panel()
 panel=PanelContainer.new()
 panel.add_theme_stylebox_override("panel",style(Color(0.025,0.047,0.078,0.99),Color("355466"),12))
 hud.add_child(panel)
 panel.set_anchors_and_offsets_preset(Control.PRESET_CENTER)
 panel.offset_left=-width/2.0;panel.offset_right=width/2.0;panel.offset_top=-310;panel.offset_bottom=310
 var v=VBoxContainer.new();v.add_theme_constant_override("separation",12);panel.add_child(v)
 var header=HBoxContainer.new();v.add_child(header)
 var titlebox=VBoxContainer.new();titlebox.size_flags_horizontal=Control.SIZE_EXPAND_FILL;header.add_child(titlebox)
 titlebox.add_child(label(title,27,accent));titlebox.add_child(label(subtitle,12,dim))
 if screen!="menu": header.add_child(button("BACK  /  ESC",resume))
 var line=HSeparator.new();v.add_child(line)
 return v

func scroll_box(parent: Control) -> VBoxContainer:
 var s=ScrollContainer.new();s.size_flags_vertical=Control.SIZE_EXPAND_FILL;s.custom_minimum_size.y=370;parent.add_child(s)
 var v=VBoxContainer.new();v.size_flags_horizontal=Control.SIZE_EXPAND_FILL;v.add_theme_constant_override("separation",8);s.add_child(v)
 return v

func show_menu():
 screen="menu";player.active=false
 var v=make_panel("SPACE MINER","THE HELIX EXPEDITION  /  ORIGINAL SPACE MINING ADVENTURE",880)
 var body=HBoxContainer.new();body.size_flags_vertical=Control.SIZE_EXPAND_FILL;v.add_child(body)
 var left=VBoxContainer.new();left.size_flags_horizontal=Control.SIZE_EXPAND_FILL;left.add_theme_constant_override("separation",15);body.add_child(left)
 left.add_child(label("A quiet belt.\nAn uncharted frontier.\nAn empire to build.",30))
 left.add_child(label("Extract fifteen minerals across five sectors.\nTrade your cargo. Refine your ship.\nBring a fleet of autonomous miners online.",15,dim))
 left.add_child(button("CONTINUE EXPEDITION   →",resume,true))
 left.add_child(button("FLIGHT MANUAL",open_screen.bind("help")))
 left.add_child(button("SETTINGS",open_screen.bind("settings")))
 left.add_child(button("SAVE & QUIT",quit_game))
 var image=TextureRect.new();image.texture=textures["ship"+str(int(profile.ship))];image.expand_mode=TextureRect.EXPAND_IGNORE_SIZE;image.stretch_mode=TextureRect.STRETCH_KEEP_ASPECT_CENTERED;image.custom_minimum_size=Vector2(280,300);body.add_child(image)
 v.add_child(label("MANTIS MINING CO.     /     EST. 2186                                        SYSTEMS ONLINE",11,accent))
 if profile.warning!="": toast(profile.warning)

func resume():
 clear_panel();screen="flight";player.active=true

func open_screen(which: String):
 screen=which;player.active=false;mining=false
 if which=="pause": show_menu();return
 if which=="cargo": show_cargo()
 elif which=="upgrades": show_upgrades()
 elif which=="map": show_map()
 elif which=="shop": show_shop()
 elif which=="drones": show_drones()
 elif which=="achievements": show_achievements()
 elif which=="settings": show_settings()
 elif which=="help": show_help()
 elif which=="dock": show_dock()

func docked() -> bool: return player.position.distance_to(station_pos)<240
func transaction_allowed() -> bool:
 if not docked(): toast("Return to Helix refinery to install equipment.");return false
 return true

func show_dock():
 if not docked(): toast("Refinery is %d m away. Follow the amber beacon."%int(player.position.distance_to(station_pos)));resume();return
 var v=make_panel("HELIX REFINERY","BERTH 07  /  TRADE · REPAIR · RESEARCH · OUTFITTING")
 v.add_child(label("Welcome home, prospector.",26))
 v.add_child(label("Cargo valuation: %s CR    /    Available: %s CR"%[cargo_value(),int(profile.credits)],18,Color("e8c58b")))
 var grid=GridContainer.new();grid.columns=2;grid.add_theme_constant_override("h_separation",14);grid.add_theme_constant_override("v_separation",14);v.add_child(grid)
 for pair in [["SELL ALL CARGO",sell],["REPAIR & RECHARGE · FREE",repair],["UPGRADE WORKSHOP",open_screen.bind("upgrades")],["SHIP SHOWROOM",open_screen.bind("shop")],["DRONE OPERATIONS",open_screen.bind("drones")],["RESEARCH / SECTOR CHART",open_screen.bind("map")],["ACHIEVEMENT LOG",open_screen.bind("achievements")],["SAVE PROGRESS",save_now]]:
  var b=button(pair[0],pair[1]);b.custom_minimum_size.x=400;grid.add_child(b)
 v.add_child(label("All trades are final. Refinery services available in every sector.\nTip: Upgrade your cargo hold early; a full hold cannot collect more ore.",14,dim))

func cargo_value() -> int:
 var n=0
 for i in range(15): n+=int(profile.cargo[i])*int(Catalog.RESOURCES[i][1])
 return n

func sell():
 if not transaction_allowed(): return
 var value=cargo_value()
 if value==0: toast("Cargo hold is empty.");return
 profile.credits+=value;profile.sold+=value;profile.xp+=value/6
 profile.cargo.fill(0)
 unlock(2)
 if profile.sold>=15000: unlock(7)
 save_now();toast("Cargo sold · +%d CR"%value);show_dock()

func repair():
 if not transaction_allowed(): return
 health=max_hull();shield=max_shield();energy=100;toast("Hull restored. Shields and reactor charged.")

func show_cargo():
 var v=make_panel("CARGO MANIFEST","%d / %d UNITS  ·  ESTIMATED VALUE %d CR"%[profile.total(),capacity(),cargo_value()])
 var list=scroll_box(v)
 for i in range(15):
  var row=HBoxContainer.new();row.add_theme_constant_override("separation",16);list.add_child(row)
  var icon=TextureRect.new();icon.texture=textures["mineral"+str(i)];icon.custom_minimum_size=Vector2(42,42);icon.expand_mode=TextureRect.EXPAND_IGNORE_SIZE;row.add_child(icon)
  var l=label("%-14s   %s   /   %d CR   /   hardness %.1f"%[Catalog.RESOURCES[i][0],Catalog.RESOURCES[i][3],Catalog.RESOURCES[i][1],Catalog.RESOURCES[i][2]],14);l.size_flags_horizontal=Control.SIZE_EXPAND_FILL;row.add_child(l)
  row.add_child(label("%d units"%int(profile.cargo[i]),16,accent))
 v.add_child(button("DOCK TO SELL CARGO",open_screen.bind("dock"),true))

func upgrade_cost(i: int) -> int: return int(Catalog.UPGRADES[i][2]*pow(1.8,profile.upgrades[i]))
func buy_upgrade(i: int) -> bool:
 if not transaction_allowed(): return false
 var cost=upgrade_cost(i)
 if profile.upgrades[i]>=5 or profile.credits<cost: toast("Insufficient credits or maximum level reached.");return false
 profile.credits-=cost;profile.upgrades[i]+=1;apply_stats();save_now();toast(Catalog.UPGRADES[i][0]+" upgraded.");return true

func upgrade_clicked(i: int):
 buy_upgrade(i);show_upgrades()

func show_upgrades():
 var v=make_panel("SHIP SYSTEMS","12 BRANCHES  /  5 TIERS EACH  /  INSTALLATION REQUIRES DOCKING")
 var list=scroll_box(v)
 for i in range(12):
  var row=HBoxContainer.new();list.add_child(row)
  var l=label("%s   [%d / 5]\n%s"%[Catalog.UPGRADES[i][0],int(profile.upgrades[i]),Catalog.UPGRADES[i][1]],14);l.size_flags_horizontal=Control.SIZE_EXPAND_FILL;row.add_child(l)
  var b=button("MAXED" if profile.upgrades[i]>=5 else "%d CR  +"%upgrade_cost(i),upgrade_clicked.bind(i),true);b.custom_minimum_size.x=155;b.disabled=profile.upgrades[i]>=5 or profile.credits<upgrade_cost(i) or not docked();row.add_child(b)
 v.add_child(label("Available balance  %d CR   /   %s"%[int(profile.credits),"DOCKED" if docked() else "FLIGHT MODE · INSTALLATION LOCKED"],14,accent))

func show_map():
 var v=make_panel("STELLAR CARTOGRAPHY","RESEARCH & JUMP NETWORK  /  UNLOCKED SECTORS REMAIN AVAILABLE")
 var list=scroll_box(v)
 for i in range(5):
  var s=Catalog.SECTORS[i];var row=HBoxContainer.new();list.add_child(row)
  var l=label("0%d   %s\n%s  /  %s"%[i+1,s[0],s[1],s[4]],14,s[5]);l.size_flags_horizontal=Control.SIZE_EXPAND_FILL;row.add_child(l)
  var known=i in profile.sectors
  var b=button("CURRENT" if sector==i else ("JUMP  →" if known else "RESEARCH · %d CR"%s[2]),jump_clicked.bind(i),known)
  b.custom_minimum_size.x=235;b.disabled=sector==i or (not known and (profile.credits<s[2] or not i-1 in profile.sectors));row.add_child(b)
 v.add_child(label("Jumping recalls drones and resets the local field. Cargo is retained.\nResearch sectors in order. Each jump provides a safe arrival corridor.",13,dim))

func jump_clicked(i: int):
 if not i in profile.sectors:
  if not i-1 in profile.sectors or profile.credits<Catalog.SECTORS[i][2]: return
  profile.credits-=Catalog.SECTORS[i][2];profile.sectors.append(i);unlock(4)
  if i==4: unlock(6)
 sector=i;generate_sector(i);player.position=Vector2.ZERO;player.velocity=Vector2.ZERO;camera.reset_smoothing();save_now();resume();toast("Jump complete · "+Catalog.SECTORS[i][0])

func show_shop():
 var v=make_panel("SHIP SHOWROOM","CHASSIS BONUSES STACK WITH INSTALLED SYSTEMS")
 for i in range(3):
  var row=HBoxContainer.new();v.add_child(row)
  var icon=TextureRect.new();icon.texture=textures["ship"+str(i)];icon.custom_minimum_size=Vector2(94,94);icon.expand_mode=TextureRect.EXPAND_IGNORE_SIZE;row.add_child(icon)
  var l=label(Catalog.SHIPS[i][0]+"\n"+Catalog.SHIPS[i][2],16);l.size_flags_horizontal=Control.SIZE_EXPAND_FILL;row.add_child(l)
  var b=button("ACTIVE" if profile.ship==i else ("EQUIP" if i in profile.ships else "%d CR"%Catalog.SHIPS[i][1]),buy_ship.bind(i));b.disabled=profile.ship==i or not docked() or (not i in profile.ships and profile.credits<Catalog.SHIPS[i][1]);row.add_child(b)

func buy_ship(i: int):
 if not transaction_allowed(): return
 if not i in profile.ships:
  if profile.credits<Catalog.SHIPS[i][1]: return
  profile.credits-=Catalog.SHIPS[i][1];profile.ships.append(i)
 profile.ship=i;apply_stats();repair();save_now();show_shop()

func show_drones():
 var v=make_panel("AUTONOMOUS OPERATIONS","VISIBLE MINING UNITS  /  SEARCH → MINE → COLLECT → RETURN")
 v.add_child(label("DEPLOYED  %d / %d    ·    TECHNOLOGY TIER %d"%[int(profile.drone_count),drone_capacity(),int(profile.drone_level)+1],23))
 v.add_child(label("Each drone carries %d units and mines at %.1f damage / sec.\nFull drones return to your ship to unload. Cargo limits still apply.\nA scanner upgrade increases the fleet’s target acquisition range."%[4+2*profile.drone_level,18+profile.drone_level*8],15,dim))
 var cost=int(550*pow(1.5,profile.drone_count))
 var b=button("BUILD MINING DRONE  ·  %d CR"%cost,buy_drone,true);b.disabled=not docked() or profile.drone_count>=drone_capacity() or profile.credits<cost;v.add_child(b)
 var c=int(450*pow(1.9,profile.drone_level));b=button("RESEARCH DRONE SYSTEMS  ·  %d CR"%c,upgrade_drone);b.disabled=not docked() or profile.drone_level>=5 or profile.credits<c;v.add_child(b)
 var list=scroll_box(v);list.custom_minimum_size.y=180
 for i in range(drones.size()): list.add_child(label("DR-%02d    %s    hold %d / %d"%[i+1,drones[i].mode.to_upper(),drones[i].load,4+2*profile.drone_level],15,accent))

func buy_drone():
 var c=int(550*pow(1.5,profile.drone_count))
 if not transaction_allowed() or profile.drone_count>=drone_capacity() or profile.credits<c: return
 profile.credits-=c;profile.drone_count+=1;rebuild_drones();unlock(3);save_now();show_drones()

func upgrade_drone():
 var c=int(450*pow(1.9,profile.drone_level))
 if not transaction_allowed() or profile.drone_level>=5 or profile.credits<c: return
 profile.credits-=c;profile.drone_level+=1;save_now();show_drones()

func show_achievements():
 var v=make_panel("EXPEDITION RECORD","PERSISTENT MILESTONES  /  %d OF 8 COMPLETE"%profile.achievements.size())
 var list=scroll_box(v)
 var conditions=["Collect your first ore","Fill the cargo hold","Sell your first shipment","Deploy a mining drone","Research Crystal Nebula","Defeat a hostile vessel","Reach Void Frontier","Sell 15,000 CR of materials"]
 for i in range(8): list.add_child(label(("[ COMPLETE ]   " if i in profile.achievements else "[ LOCKED ]     ")+Catalog.ACHIEVEMENTS[i]+"\n                       "+conditions[i],16,accent if i in profile.achievements else dim))

func show_settings():
 var v=make_panel("FLIGHT SETTINGS","APPLIED IMMEDIATELY  /  SAVED WITH YOUR EXPEDITION",740)
 v.add_child(label("Master audio",18))
 var slider=HSlider.new();slider.min_value=0;slider.max_value=1;slider.step=.01;slider.value=profile.settings.volume;slider.value_changed.connect(func(value): profile.settings.volume=value);v.add_child(slider)
 var check=CheckButton.new();check.text="Enhanced particles";check.button_pressed=profile.settings.particles;check.toggled.connect(func(value): profile.settings.particles=value);v.add_child(check)
 check=CheckButton.new();check.text="Fullscreen";check.button_pressed=profile.settings.fullscreen;check.toggled.connect(func(value): profile.settings.fullscreen=value;DisplayServer.window_set_mode(DisplayServer.WINDOW_MODE_FULLSCREEN if value else DisplayServer.WINDOW_MODE_WINDOWED));v.add_child(check)
 v.add_child(button("SAVE SETTINGS",save_now,true))
 v.add_child(button("FLIGHT MANUAL",open_screen.bind("help")))

func show_help():
 var v=make_panel("PROSPECTOR FIELD MANUAL","HELIX MINING CO.  /  REVISION 1.0",850)
 v.add_child(label("01   Navigate with WASD. Your ship aims at the cursor.\n02   Hold LEFT MOUSE over a rock within range to mine.\n03   Fly into the ore fragments to collect them.\n04   Follow the amber marker to the refinery. Press E to dock.\n05   Sell cargo, install upgrades and build mining drones.\n06   Hold RIGHT MOUSE or SPACE to fire at hostiles.\n07   Open M to research sectors and jump to new fields.\n08   Progress autosaves every 20 seconds and after purchases.",18))
 v.add_child(label("I  Inventory    U  Upgrades    M  Sector map    O  Drones\nP  Achievements    F5  Save    ESC  Pause / Return\n\nHull failure costs 12% of credits and half of cargo. Ship and\nupgrades are retained. Shield and energy regenerate in flight.\nSector hazards pulse every 12 seconds; leave the red hazard ring.",15,dim))

func capacity() -> int: return 50+int(profile.upgrades[2])*30+(60 if profile.ship==1 else 0)
func max_hull() -> float: return 100+profile.upgrades[5]*35+(80 if profile.ship==2 else 0)
func max_shield() -> float: return 60+profile.upgrades[6]*30
func drone_capacity() -> int: return 1+int(profile.upgrades[9])
func mining_power() -> float: return 29*(1+profile.upgrades[0]*.35)*(1+profile.upgrades[1]*.2)*(1.3 if profile.ship==1 else 1.0)
func scan_range() -> float: return 410+profile.upgrades[8]*160
func apply_stats():
 player.acceleration=430+profile.upgrades[4]*90
 player.top_speed=(290+profile.upgrades[3]*45)*(1.2 if profile.ship==2 else 1.0)
 health=minf(health,max_hull());shield=minf(shield,max_shield())

func unlock(i: int):
 if not i in profile.achievements:
  profile.achievements.append(i);toast("MILESTONE  /  "+Catalog.ACHIEVEMENTS[i])

func toast(text: String):
 if is_instance_valid(notification): notification.text=text
 toast_time=4.5

func save_now():
 if profile.save_data(): toast("Expedition saved.")
 else: toast("Save failed. Check storage permissions.")

func quit_game():
 profile.save_data();get_tree().quit()

func _notification(what):
 if what==NOTIFICATION_WM_CLOSE_REQUEST: profile.save_data()

func _unhandled_key_input(event):
 if not event is InputEventKey or not event.pressed or event.echo: return
 var key=event.physical_keycode
 if key==KEY_ESCAPE:
  if screen=="flight": show_menu()
  else: resume()
 elif key==KEY_F5: save_now()
 elif screen=="flight":
  var keys={KEY_I:"cargo",KEY_U:"upgrades",KEY_M:"map",KEY_O:"drones",KEY_P:"achievements",KEY_E:"dock"}
  if key in keys: open_screen(keys[key])

func generate_sector(index: int):
 rocks.clear();drops.clear();enemies.clear();bullets.clear();effects.clear()
 rng.seed=764812+index*7321
 for i in range(90):
  var pos=Vector2.ZERO
  var radius=rng.randf_range(32,76)
  var valid_pos=false
  for attempt in range(80):
   pos=Vector2(rng.randf_range(-2000,2000),rng.randf_range(-1700,1700))
   if pos.length()<180 or pos.distance_to(station_pos)<330: continue
   valid_pos=true
   for rock in rocks:
    if pos.distance_to(rock.pos)<radius+rock.radius+18: valid_pos=false;break
   if valid_pos: break
  if not valid_pos: continue
  var ids=Catalog.SECTORS[index][3]
  var resource=int(ids[rng.randi_range(0,ids.size()-1)])
  var hp=radius*.8*float(Catalog.RESOURCES[resource][2])
  rocks.append({"pos":pos,"radius":radius,"hp":hp,"max_hp":hp,"resource":resource,"rotation":rng.randf()*TAU,"spin":rng.randf_range(-.13,.13),"variant":i%5,"quantity":rng.randi_range(5,10)})
 # Guaranteed accessible starter targets, outside both player and station spawn clearances.
 for p in [Vector2(250,-70),Vector2(370,130),Vector2(130,270)]:
  var rid=int(Catalog.SECTORS[index][3][0]);var hp=42*float(Catalog.RESOURCES[rid][2])
  rocks.append({"pos":p,"radius":44.0,"hp":hp,"max_hp":hp,"resource":rid,"rotation":0.0,"spin":.06,"variant":0,"quantity":8})
 enemy_timer=9.0;hazard_timer=0;rebuild_drones()

func rebuild_drones():
 # Retain mined cargo during fleet changes.
 for d in drones:
  if d.load>0:
   for i in range(int(d.load)): drops.append({"pos":d.pos+Vector2(rng.randf_range(-20,20),rng.randf_range(-20,20)),"resource":d.resource,"vel":Vector2.ZERO,"age":0.0})
 drones.clear()
 for i in range(int(profile.drone_count)):
  drones.append({"pos":player.position+Vector2(0,55+i*35),"mode":"search","target":-1,"load":0,"resource":0,"beam":false,"angle":0.0})

func _process(delta):
 clock+=delta
 synth_audio()
 mining=false
 if screen=="flight": simulate(minf(delta,.05))
 player.active=screen=="flight"
 for fx in effects:
  fx.life-=delta;fx.pos+=fx.vel*delta;fx.vel*=maxf(0,1-delta*1.5)
 effects=effects.filter(func(f):return f.life>0)
 toast_time-=delta
 if toast_time<=0: notification.text=""
 nav_label.text="%02d  /  %s"%[sector+1,Catalog.SECTORS[sector][0]]
 status_label.text="%s CR   /   LV %02d"%[int(profile.credits),1+int(sqrt(profile.xp/100.0))]
 cargo_label.text="CARGO  %d / %d   ·   DRONES  %d"%[profile.total(),capacity(),drones.size()]
 hull_bar.max_value=max_hull();hull_bar.value=health
 shield_bar.max_value=max_shield();shield_bar.value=shield
 energy_bar.value=energy
 if screen=="flight":
  objective_label.text="E  DOCK AT HELIX REFINERY" if docked() else ("CARGO FULL  /  Return to refinery to sell" if profile.total()>=capacity() else "Extract ore · Trade at refinery · Expand your operation")
 queue_redraw()

func simulate(dt: float):
 energy=minf(100,energy+(14+profile.upgrades[7]*5)*dt)
 shield=minf(max_shield(),shield+5*dt)
 cooldown=maxf(0,cooldown-dt)
 save_timer+=dt
 if save_timer>20: profile.save_data();save_timer=0
 for r in rocks: r.rotation+=r.spin*dt
 target=-1
 var mouse=get_global_mouse_position()
 var best=100000.0
 for i in range(rocks.size()):
  var r=rocks[i]
  if r.hp<=0: continue
  var aim=mouse.distance_to(r.pos)
  if aim<r.radius+28 and player.position.distance_to(r.pos)<scan_range() and aim<best: target=i;best=aim
 if Input.is_mouse_button_pressed(MOUSE_BUTTON_LEFT) and target>=0 and energy>2:
  mining=true;beam_end=rocks[target].pos;energy=maxf(0,energy-24*dt)
  damage_rock(target,mining_power()*dt)
  if rng.randf()<dt*28: burst(beam_end,accent,2,50)
 if (Input.is_mouse_button_pressed(MOUSE_BUTTON_RIGHT) or Input.is_physical_key_pressed(KEY_SPACE)) and cooldown<=0 and energy>=5:
  var direction=Vector2.RIGHT.rotated(player.rotation)
  bullets.append({"pos":player.position+direction*48,"vel":direction*820,"life":1.2,"enemy":false,"damage":18+profile.upgrades[10]*10+(8 if profile.ship==2 else 0)})
  energy-=5;cooldown=.23;burst(player.position+direction*48,Color("ffc38d"),3,80)
 # Physical rock contacts use their procedural collision envelope.
 for r in rocks:
  if r.hp<=0: continue
  var diff=player.position-r.pos;var distance=diff.length();var separation=r.radius*.78+22
  if distance<separation:
   var normal=diff.normalized() if distance>.001 else Vector2.RIGHT
   player.position=r.pos+normal*separation
   if player.velocity.length()>120: hit_player(player.velocity.length()*.018)
   player.velocity=player.velocity.bounce(normal)*.4
 update_drops(dt)
 update_drones(dt)
 update_enemies(dt)
 update_bullets(dt)
 hazard_timer+=dt
 if sector>0 and hazard_timer>12:
  hazard_timer=0
  var hz=hazard_position()
  burst(hz,Catalog.SECTORS[sector][5],35,200)
  if player.position.distance_to(hz)<180: hit_player(12+sector*8);toast("Warning · "+Catalog.SECTORS[sector][4])
 if rocks.filter(func(r):return r.hp>0).size()<10: generate_sector(sector);toast("Survey update · New mineral field detected.")

func hazard_position() -> Vector2: return Vector2(620,-280) if sector<3 else Vector2(340,320)

func damage_rock(index: int,amount: float):
 if index<0 or index>=rocks.size() or rocks[index].hp<=0: return
 var r=rocks[index];r.hp-=amount
 if r.hp<=0:
  profile.mined+=1;profile.xp+=20+sector*12
  burst(r.pos,Catalog.SECTORS[sector][5],24,r.radius*2)
  for i in range(r.quantity):
   drops.append({"pos":r.pos+Vector2(rng.randf_range(-20,20),rng.randf_range(-20,20)),"resource":r.resource,"vel":Vector2.from_angle(rng.randf()*TAU)*rng.randf_range(15,65),"age":0.0})

func collect(resource: int, count=1) -> int:
 var taken=mini(count,capacity()-profile.total())
 if taken<=0: return 0
 profile.cargo[resource]+=taken;profile.xp+=taken*2
 unlock(0)
 if profile.total()>=capacity(): unlock(1)
 return taken

func update_drops(dt: float):
 for d in drops:
  d.pos+=d.vel*dt;d.vel*=maxf(0,1-dt*.8);d.age+=dt
  var dist=d.pos.distance_to(player.position)
  if dist<65+profile.upgrades[11]*90 and profile.total()<capacity(): d.pos=d.pos.move_toward(player.position,(170+profile.upgrades[11]*40)*dt)
  if d.pos.distance_to(player.position)<30 and collect(d.resource)>0:
   d.age=999;burst(d.pos,accent,3,60)
 drops=drops.filter(func(d):return d.age<180)

func update_drones(dt: float):
 for d in drones:
  d.beam=false
  var maxload=4+2*profile.drone_level
  if d.mode=="return":
   d.angle=(player.position-d.pos).angle();d.pos=d.pos.move_toward(player.position,(220+profile.drone_level*25)*dt)
   if d.pos.distance_to(player.position)<45:
    var n=collect(d.resource,int(d.load));d.load-=n
    if d.load<=0: d.mode="search"
   continue
  if d.mode=="search":
   var best=INF;var found=-1
   for i in range(rocks.size()):
    if rocks[i].hp<=0: continue
    var distance=d.pos.distance_to(rocks[i].pos)
    if distance<best and rocks[i].pos.distance_to(player.position)<scan_range()+350: best=distance;found=i
   d.target=found
   if found>=0: d.mode="mine"
   else: d.pos=d.pos.move_toward(player.position+Vector2(70,70),140*dt)
  elif d.mode=="mine":
   if d.target<0 or rocks[d.target].hp<=0:
    d.mode="collect";continue
   var rock=rocks[d.target];d.angle=(rock.pos-d.pos).angle()
   if d.pos.distance_to(rock.pos)>rock.radius+50: d.pos=d.pos.move_toward(rock.pos,190*dt)
   else:
    d.beam=true;damage_rock(d.target,(18+profile.drone_level*8)*dt)
   if d.pos.distance_to(player.position)>scan_range()+600: d.mode="return" if d.load>0 else "search"
  elif d.mode=="collect":
   var nearest=-1;var distance=260.0
   for i in range(drops.size()):
    if drops[i].age>=180: continue
    if d.load>0 and drops[i].resource!=d.resource: continue
    var dd=d.pos.distance_to(drops[i].pos)
    if dd<distance: distance=dd;nearest=i
   if nearest<0 or d.load>=maxload: d.mode="return" if d.load>0 else "search"
   else:
    d.pos=d.pos.move_toward(drops[nearest].pos,180*dt)
    if d.pos.distance_to(drops[nearest].pos)<22:
     d.resource=drops[nearest].resource;d.load+=1;drops[nearest].age=999

func spawn_enemy():
 var kind=sector
 var angle=rng.randf()*TAU
 var pos=player.position+Vector2.from_angle(angle)*900
 enemies.append({"pos":pos,"kind":kind,"hp":45+kind*40,"max_hp":45+kind*40,"cooldown":1.5,"angle":0.0,"phase":rng.randf()*TAU})

func update_enemies(dt: float):
 enemy_timer-=dt
 if enemy_timer<=0:
  if enemies.size()<2+sector: spawn_enemy()
  enemy_timer=24-sector*3
 for e in enemies:
  if e.hp<=0: continue
  var offset=player.position-e.pos
  e.angle=offset.angle()
  var desired=player.position+Vector2.from_angle(clock*.5+e.phase)*(230+e.kind*20)
  if e.pos.distance_to(station_pos)>280 or not docked(): e.pos=e.pos.move_toward(desired,(75+e.kind*18)*dt)
  e.cooldown-=dt
  if e.cooldown<=0 and offset.length()<700 and not docked():
   var aim=offset.normalized()
   if e.kind>=2: aim=(player.position+player.velocity*.25-e.pos).normalized()
   bullets.append({"pos":e.pos+aim*36,"vel":aim*(240+e.kind*45),"life":3.0,"enemy":true,"damage":8+e.kind*5})
   e.cooldown=2.3-e.kind*.22
 enemies=enemies.filter(func(e):return e.hp>0)

func update_bullets(dt: float):
 for b in bullets:
  var previous=b.pos;b.pos+=b.vel*dt;b.life-=dt
  if b.enemy:
   if Geometry2D.get_closest_point_to_segment(player.position,previous,b.pos).distance_to(player.position)<26:
    hit_player(b.damage);b.life=0
  else:
   for e in enemies:
    if e.hp>0 and Geometry2D.get_closest_point_to_segment(e.pos,previous,b.pos).distance_to(e.pos)<34+e.kind*3:
     e.hp-=b.damage;b.life=0;burst(b.pos,Color("ffad81"),7,100)
     if e.hp<=0:
      profile.kills+=1;profile.credits+=90+e.kind*95;profile.xp+=70+e.kind*30;unlock(5);burst(e.pos,Color("ffb773"),35,210)
     break
 bullets=bullets.filter(func(b):return b.life>0)

func hit_player(amount: float):
 var absorbed=minf(shield,amount);shield-=absorbed;health-=amount-absorbed
 burst(player.position,Color("72c9ff") if absorbed>0 else Color("ff986b"),9,120)
 if health<=0:
  burst(player.position,Color("ffc58c"),45,260)
  profile.credits=int(profile.credits*.88)
  for i in range(15): profile.cargo[i]=int(profile.cargo[i]) / 2
  health=max_hull();shield=max_shield();energy=100;player.position=Vector2.ZERO;player.velocity=Vector2.ZERO
  bullets.clear();enemies.clear();rebuild_drones();profile.save_data();toast("Rescue recovery · Lost 12% credits and half your cargo.")

func burst(pos: Vector2,color: Color,count: int,speed: float):
 if not profile.settings.particles: count=mini(3,count)
 for i in range(count):
  if effects.size()>650: break
  effects.append({"pos":pos,"vel":Vector2.from_angle(rng.randf()*TAU)*rng.randf_range(speed*.2,speed),"life":rng.randf_range(.25,.85),"color":color,"size":rng.randf_range(1,4)})

func draw_sprite(tex: Texture2D,pos: Vector2,size: Vector2,angle=0.0,color=Color.WHITE):
 draw_set_transform(pos,angle)
 draw_texture_rect(tex,Rect2(-size*.5,size),false,color)
 draw_set_transform(Vector2.ZERO)

func _draw():
 if player==null or textures.is_empty(): return
 var center=player.position
 draw_texture_rect(textures["bg"+str(sector)],Rect2(center-Vector2(1900,1200),Vector2(3800,2400)),false)
 # Navigational grid is deliberately subdued and remains fixed in world space.
 for x in range(-2400,2600,200): draw_line(Vector2(x,-2300),Vector2(x,2300),Color(.2,.5,.6,.035),1)
 for y in range(-2400,2600,200): draw_line(Vector2(-2400,y),Vector2(2400,y),Color(.2,.5,.6,.035),1)
 if sector>0:
  var hz=hazard_position();var pulse=.12+.12*sin(clock*3)
  draw_arc(hz,180,0,TAU,70,Color(1,.35,.45,pulse+.15),2)
  if hazard_timer>9:
   for i in range(8):
    var a=float(i)*TAU/8+clock*.15
    draw_line(hz+Vector2.from_angle(a)*40,hz+Vector2.from_angle(a+.35)*175,Color(.8,.55,1,.5),2)
  draw_string(mono,hz+Vector2(-85,-198),Catalog.SECTORS[sector][4].to_upper(),HORIZONTAL_ALIGNMENT_LEFT,-1,11,Color("d785a6"))
 if sector==2:
  for p in [Vector2(600,200),Vector2(-750,-400)]: draw_sprite(textures.ship1,p,Vector2(180,180),1.1,Color(.3,.3,.35,.65))
 draw_arc(station_pos,225,0,TAU,100,Color(.95,.65,.3,.12+.04*sin(clock)),1.4)
 draw_sprite(textures.station,station_pos,Vector2(300,300))
 draw_string(mono,station_pos+Vector2(-100,177),"HELIX / ORBITAL REFINERY",HORIZONTAL_ALIGNMENT_LEFT,-1,12,Color("efc38b"))
 draw_string(mono,station_pos+Vector2(-68,197),"E  DOCK & TRADE",HORIZONTAL_ALIGNMENT_LEFT,-1,11,accent)
 for i in range(rocks.size()):
  var r=rocks[i]
  if r.hp<=0: continue
  if r.pos.distance_to(player.position)>1500: continue
  var tex=textures["rock%d_%d"%[sector,r.variant]]
  draw_sprite(tex,r.pos,Vector2.ONE*r.radius*2.4,r.rotation,Color(1,1,1,1))
  if r.hp<r.max_hp:
   var fraction=clampf(r.hp/r.max_hp,0,1)
   for j in range(3):
    var a=r.rotation+j*2.1
    draw_line(r.pos+Vector2.from_angle(a)*r.radius*.2,r.pos+Vector2.from_angle(a+.4)*r.radius*.8,Color(1,.45,.15,(1-fraction)*.8),1.5)
   draw_line(r.pos+Vector2(-25,-r.radius-7),r.pos+Vector2(25,-r.radius-7),Color("233748"),3)
   draw_line(r.pos+Vector2(-25,-r.radius-7),r.pos+Vector2(-25+50*fraction,-r.radius-7),accent,3)
  if target==i and screen=="flight":
   draw_arc(r.pos,r.radius+12,clock,clock+TAU*.8,40,accent,1.5)
   var res=Catalog.RESOURCES[r.resource]
   draw_string(mono,r.pos+Vector2(-40,-r.radius-35),res[0].to_upper(),HORIZONTAL_ALIGNMENT_LEFT,-1,12,accent)
   draw_string(mono,r.pos+Vector2(-40,-r.radius-20),"%d%%  /  %d CR"%[int(maxf(0,r.hp/r.max_hp)*100),res[1]],HORIZONTAL_ALIGNMENT_LEFT,-1,10,dim)
 for d in drops:
  if d.age>=180: continue
  draw_sprite(textures["mineral"+str(d.resource)],d.pos,Vector2(20,20),sin(clock+d.pos.x)*.12)
 if mining:
  var muzzle=player.position+Vector2.RIGHT.rotated(player.rotation)*43
  draw_line(muzzle,beam_end,Color(.2,1,.86,.09),17,true)
  draw_line(muzzle,beam_end,Color(.3,1,.9,.35),7,true)
  draw_line(muzzle,beam_end,Color("c5fff1"),2,true)
 for d in drones:
  if d.beam and d.target>=0:
   draw_line(d.pos,rocks[d.target].pos,Color(1,.74,.3,.15),8,true)
   draw_line(d.pos,rocks[d.target].pos,Color("f8cb7b"),2,true)
  draw_sprite(textures.drone,d.pos,Vector2(38,38),d.angle)
  draw_string(mono,d.pos+Vector2(-15,31),"DR-%02d"%(drones.find(d)+1),HORIZONTAL_ALIGNMENT_LEFT,-1,8,Color("eac78b"))
 for e in enemies:
  draw_sprite(textures["enemy"+str(e.kind)],e.pos,Vector2.ONE*(74+e.kind*7),e.angle)
  draw_line(e.pos+Vector2(-24,-48),e.pos+Vector2(24,-48),Color("512b39"),3)
  draw_line(e.pos+Vector2(-24,-48),e.pos+Vector2(-24+48*e.hp/e.max_hp,-48),Color("ef827e"),3)
  draw_string(mono,e.pos+Vector2(-40,-60),Catalog.ENEMIES[e.kind].to_upper(),HORIZONTAL_ALIGNMENT_LEFT,-1,10,Color("e7a18f"))
 for b in bullets:
  draw_line(b.pos-b.vel.normalized()*18,b.pos,Color("fa7c8e") if b.enemy else Color("ffd291"),3,true)
  draw_line(b.pos-b.vel.normalized()*11,b.pos,Color.WHITE,1,true)
 # Layered animated exhaust; original hull remains detailed at all speeds.
 var forward=Vector2.RIGHT.rotated(player.rotation)
 if player.thrust.length()>.1 and screen=="flight":
  for side in [-1,1]:
   var nozzle=player.position+Vector2(-34,side*18).rotated(player.rotation)
   var length=20+10*sin(clock*53+side)
   draw_line(nozzle-forward*length,nozzle,Color(.15,.8,1,.13),16,true)
   draw_line(nozzle-forward*length,nozzle,Color(.3,.9,1,.7),6,true)
   draw_line(nozzle-forward*length*.65,nozzle,Color("e1fcff"),2,true)
 draw_sprite(textures["ship"+str(int(profile.ship))],player.position,Vector2(90,90),player.rotation)
 if shield>1: draw_arc(player.position,49,player.rotation-.8,player.rotation+.8,28,Color(.3,.8,1,.15+.1*sin(clock*2)),1.5)
 if health<max_hull()*.3:
  draw_arc(player.position,46,clock*2,clock*2+1,16,Color(1,.3,.15,.5),2)
 for fx in effects:
  var c=fx.color;c.a=clampf(fx.life*1.7,0,1)
  draw_line(fx.pos,fx.pos-fx.vel*.025,c,fx.size,true)
 # On-screen bearing to refinery, clamped to a safe HUD inset.
 if player.position.distance_to(station_pos)>560:
  var bearing=(station_pos-player.position).normalized()
  var pos=player.position+bearing*360
  draw_arc(pos,16,0,TAU,24,Color("d6ae76"),1.4)
  draw_line(pos-bearing*8,pos+bearing*8,Color("efd09c"),2)
  draw_string(mono,pos+Vector2(-45,34),"HELIX %d m"%int(player.position.distance_to(station_pos)),HORIZONTAL_ALIGNMENT_LEFT,-1,10,Color("e8c995"))
 # Tactical scanner, world-space placement follows camera and leaves gameplay unobscured.
 var vp=get_viewport_rect().size
 var radar=player.position+Vector2(vp.x*.5-112,-vp.y*.5+218)
 draw_rect(Rect2(radar-Vector2(72,72),Vector2(144,144)),Color(.02,.045,.075,.94),true)
 draw_rect(Rect2(radar-Vector2(72,72),Vector2(144,144)),Color("294253"),false,1)
 draw_line(radar+Vector2(-60,0),radar+Vector2(60,0),Color(.2,.4,.5,.25),1)
 draw_line(radar+Vector2(0,-60),radar+Vector2(0,60),Color(.2,.4,.5,.25),1)
 for r in rocks:
  var offset=(r.pos-player.position)*.04
  if r.hp>0 and absf(offset.x)<65 and absf(offset.y)<65: draw_rect(Rect2(radar+offset-Vector2.ONE,Vector2(2,2)),Color("659b9b"))
 for e in enemies:
  var offset=(e.pos-player.position)*.04
  if absf(offset.x)<65 and absf(offset.y)<65: draw_rect(Rect2(radar+offset-Vector2(2,2),Vector2(4,4)),Color("ef8c8a"))
 draw_rect(Rect2(radar-Vector2(2,2),Vector2(4,4)),accent)
 draw_string(mono,radar+Vector2(-61,88),"LOCAL SURVEY / 3 KM",HORIZONTAL_ALIGNMENT_LEFT,-1,9,dim)
