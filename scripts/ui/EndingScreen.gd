extends Control
## Custom Ending Presentation & Narrative Debrief Screen for Endings A, B, and C.

const ENDING_DATA: Dictionary = {
	"A": {
		"code": "ENDING 01 OF 03 // SURVIVOR ESCAPE",
		"title": "MORNING OUTSIDE KESTREL COURT",
		"color": Color(0.45, 0.90, 0.65),
		"story": "The magnetic crash-bar gives way with a sharp metallic ring.\n\nCold October air floods your lungs as you stumble out onto the wet asphalt of Kestrel Street. Pale grey dawn is breaking above the neighboring apartment blocks. In your hand, the municipal clipboard is blank—its pages damp with ordinary rain.\n\nBehind you, the heavy fire door clicks shut and seals. For one heartbeat, you hear two measured footsteps stop just behind the wired glass.\n\nYou keep walking toward the avenue. You do not look back."
	},
	"B": {
		"code": "ENDING 02 OF 03 // TRUE ATONEMENT",
		"title": "THE SEAL BROKEN",
		"color": Color(0.96, 0.80, 0.36),
		"story": "Deep beneath Basement Level B, the 1994 Master Override Key shears off inside the Resonance Containment Console. The sixty-hertz hum that held Floor 4 suspended for thirty-two years collapses into a low structural sigh.\n\nAs you burst through the North Emergency Exit into the morning mist, the upper windows of Kestrel Court go dark—one apartment at a time. Dr. Koslov's recorder falls silent. Superintendent Hollis's shift is finally over.\n\nThirty years late, Inspector Julian Vance has filed an honest report."
	},
	"C": {
		"code": "ENDING 03 OF 03 // PERCEPTUAL ASSIMILATION",
		"title": "TENANT 405",
		"color": Color(0.92, 0.28, 0.26),
		"story": "You stop running from the echo behind your heels and gaze directly into the threshold.\n\nThe fluorescent hum warms into a familiar lullaby. Your flashlight slips from your fingers, rolling across the burgundy hallway carpet until its beam rests against the coastal lighthouse painting.\n\nDown at the South Security Desk, a fresh clipboard materializes on the walnut blotter. You take your place at the far end of the North Corridor—tall, motionless, waiting patiently for the next inspector to blink."
	},
}


func _ready() -> void:
	if DisplayServer.get_name() != "headless":
		Input.set_mouse_mode(Input.MOUSE_MODE_VISIBLE)

	set_anchors_preset(Control.PRESET_FULL_RECT)
	AudioDirector.start_ambience("res://assets/audio/menu_theme.wav", -6.0)

	var eid := GameState.pending_ending_id if ENDING_DATA.has(GameState.pending_ending_id) else "A"
	var info: Dictionary = ENDING_DATA[eid]

	var bg := ColorRect.new()
	bg.set_anchors_preset(Control.PRESET_FULL_RECT)
	bg.color = Color(0.03, 0.032, 0.042, 1.0)
	add_child(bg)

	var center := CenterContainer.new()
	center.set_anchors_preset(Control.PRESET_FULL_RECT)
	add_child(center)

	var card := PanelContainer.new()
	card.custom_minimum_size = Vector2(780, 500)
	card.add_theme_stylebox_override("panel", ThemeHelper.make_panel_style(ThemeHelper.BG_DARK, info["color"], 6, 2))
	center.add_child(card)

	var vb := VBoxContainer.new()
	vb.add_theme_constant_override("separation", 14)
	card.add_child(vb)

	var code_lbl := Label.new()
	code_lbl.text = info["code"]
	code_lbl.add_theme_font_size_override("font_size", 13)
	code_lbl.add_theme_color_override("font_color", info["color"])
	vb.add_child(code_lbl)

	var title_lbl := Label.new()
	title_lbl.text = info["title"]
	title_lbl.add_theme_font_size_override("font_size", 32)
	title_lbl.add_theme_color_override("font_color", ThemeHelper.TEXT_MAIN)
	vb.add_child(title_lbl)

	vb.add_child(HSeparator.new())

	var body := RichTextLabel.new()
	body.size_flags_vertical = Control.SIZE_EXPAND_FILL
	body.text = info["story"]
	body.add_theme_font_size_override("normal_font_size", 16)
	vb.add_child(body)

	var summary := Label.new()
	summary.text = "SHIFT SUMMARY  //  Cycles Verified: %d   |   Anomalies Logged: %d / 30   |   Insight: %d   |   Endings Unlocked: %d / 3" % [
		GameState.verified_cycles, GameState.discovered_anomalies.size(), GameState.insight_points, GameState.unlocked_endings.size()
	]
	summary.add_theme_font_size_override("font_size", 13)
	summary.add_theme_color_override("font_color", ThemeHelper.ACCENT_AMBER)
	vb.add_child(summary)

	var btn_row := HBoxContainer.new()
	btn_row.alignment = BoxContainer.ALIGNMENT_END
	btn_row.add_theme_constant_override("separation", 14)
	vb.add_child(btn_row)

	var replay_btn := Button.new()
	replay_btn.text = "START NEW INSPECTION"
	ThemeHelper.style_button(replay_btn, info["color"])
	replay_btn.pressed.connect(func():
		GameSession.load_saved_on_entry = false
		get_tree().change_scene_to_file("res://scenes/GameWorld.tscn")
	)
	btn_row.add_child(replay_btn)

	var menu_btn := Button.new()
	menu_btn.text = "RETURN TO MAIN MENU"
	ThemeHelper.style_button(menu_btn)
	menu_btn.pressed.connect(func():
		get_tree().change_scene_to_file("res://scenes/MainMenu.tscn")
	)
	btn_row.add_child(menu_btn)
