class_name JournalPanel
extends PanelContainer
## Inspector Julian Vance's Anomaly Journal, 30-Anomaly Registry,
## Mystery Objectives Tracker, and Archived Documents / Audio Recordings Reader.

signal closed

var _tabs: TabContainer
var _status_label: RichTextLabel
var _anomaly_list_vbox: VBoxContainer
var _archive_list_vbox: VBoxContainer
var _archive_reader: RichTextLabel


func _ready() -> void:
	add_theme_stylebox_override("panel", ThemeHelper.make_panel_style(ThemeHelper.BG_DARK, ThemeHelper.BORDER_AMBER, 6, 2))
	custom_minimum_size = Vector2(920, 560)

	var root_vbox := VBoxContainer.new()
	root_vbox.add_theme_constant_override("separation", 10)
	add_child(root_vbox)

	# Title Row with Custom Journal Icon
	var top := HBoxContainer.new()
	top.add_theme_constant_override("separation", 10)
	root_vbox.add_child(top)

	var ic := TextureRect.new()
	ic.texture = ThemeHelper.make_icon_tex("icon_journal")
	ic.custom_minimum_size = Vector2(28, 28)
	ic.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	ic.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	top.add_child(ic)

	var title := Label.new()
	title.text = "MUNICIPAL INSPECTION DOSSIER  //  KESTREL COURT BUILDING 4"
	title.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	title.add_theme_font_size_override("font_size", 19)
	title.add_theme_color_override("font_color", ThemeHelper.ACCENT_AMBER)
	top.add_child(title)

	var close_btn := Button.new()
	close_btn.text = "CLOSE [J / ESC]"
	ThemeHelper.style_button(close_btn, ThemeHelper.BORDER_AMBER)
	close_btn.custom_minimum_size = Vector2(150, 34)
	close_btn.pressed.connect(func(): closed.emit())
	top.add_child(close_btn)

	_tabs = TabContainer.new()
	_tabs.size_flags_vertical = Control.SIZE_EXPAND_FILL
	root_vbox.add_child(_tabs)

	# TAB 1: Protocol & Investigation Objectives
	var tab1 := MarginContainer.new()
	tab1.name = "Inspection Protocol"
	_status_label = RichTextLabel.new()
	_status_label.bbcode_enabled = true
	_status_label.fit_content = false
	tab1.add_child(_status_label)
	_tabs.add_child(tab1)

	# TAB 2: 30-Anomaly Registry
	var tab2 := ScrollContainer.new()
	tab2.name = "30-Anomaly Registry"
	_anomaly_list_vbox = VBoxContainer.new()
	_anomaly_list_vbox.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_anomaly_list_vbox.add_theme_constant_override("separation", 6)
	tab2.add_child(_anomaly_list_vbox)
	_tabs.add_child(tab2)

	# TAB 3: Documents & Microcassette Logs
	var tab3 := HSplitContainer.new()
	tab3.name = "Documents & Recordings"
	tab3.split_offset = 340
	var left_scroll := ScrollContainer.new()
	left_scroll.custom_minimum_size = Vector2(330, 0)
	_archive_list_vbox = VBoxContainer.new()
	_archive_list_vbox.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_archive_list_vbox.add_theme_constant_override("separation", 6)
	left_scroll.add_child(_archive_list_vbox)
	tab3.add_child(left_scroll)

	_archive_reader = RichTextLabel.new()
	_archive_reader.bbcode_enabled = true
	_archive_reader.text = "Select an archived document or microcassette recording on the left to inspect its contents."
	tab3.add_child(_archive_reader)
	_tabs.add_child(tab3)

	refresh_contents()


func refresh_contents() -> void:
	if _status_label == null:
		return

	var disc_count := GameState.discovered_anomalies.size()
	var doc_count := GameState.collected_documents.size()
	var tape_count := GameState.played_tapes.size()
	var end_count := GameState.unlocked_endings.size()

	_status_label.text = (
		"[b][color=#efb958]SHIFT STATUS & OBSERVATION PROTOCOL[/color][/b]\n\n"
		+ "Verified Clearance Cycles: [b]%d / %d[/b]   |   Insight Score: [b]%d[/b]   |   Scanner Charges: [b]%d / %d[/b]\n" % [
			GameState.verified_cycles, GameState.TARGET_CLEARANCE_CYCLES, GameState.insight_points,
			GameState.scanner_charges, GameState.max_scanner_charges
		]
		+ "Cataloged Anomalies: [b]%d / 30[/b]   |   Documents: [b]%d / 8[/b]   |   Recordings: [b]%d / 4[/b]   |   Endings Reached: [b]%d / 3[/b]\n\n" % [
			disc_count, doc_count, tape_count, end_count
		]
		+ "[b][color=#efb958]HOW TO SURVIVE & ESCAPE:[/color][/b]\n"
		+ "1. Inspect the Main Corridor, Apartments 4A/4B/4C, Cross Hall, Laundry, Storage, Maintenance, Stairwell, Elevator, and Basement.\n"
		+ "2. Aim at suspected distortions and press [b][R][/b] to log with your Field Scanner (recharges +1 on valid hit; penalizes false scans).\n"
		+ "3. Return to the [b]South Threshold Security Desk[/b]:\n"
		+ "   - Pull [color=#54e085][b]GREEN CLEARANCE LEVER[/b][/color] ONLY when 0 anomalies exist on the floor.\n"
		+ "   - Pull [color=#e8544e][b]RED CONTAINMENT LEVER[/b][/color] whenever an anomaly is present.\n\n"
		+ "[b][color=#efb958]ENTITY COUNTERMEASURES:[/color][/b]\n"
		+ "- [b]THE OBSERVER[/b]: Freezes while in direct unobstructed camera view. Keep sightlines on it as you retreat.\n"
		+ "- [b]THE LISTENER[/b]: Blind acoustic hunter. Hold [b][Ctrl][/b] to crouch-walk below 20 dB.\n"
		+ "- [b]THE IMITATOR[/b]: Mimics tall props & warped chimes. Never touch it with [E] — scan it from afar with [b][R][/b].\n\n"
		+ "[b][color=#efb958]CONTAINMENT MYSTERY CHECKLIST:[/color][/b]\n"
		+ "- Maintenance Main Breaker Restored: [b]%s[/b]\n" % ("ONLINE" if GameState.breaker_restored else "OFFLINE")
		+ "- Hidden Architect's Sanctum (Apt 4C): [b]%s[/b]\n" % ("UNLOCKED" if GameState.sanctum_unlocked else "CONCEALED")
		+ "- 1994 Master Override Key: [b]%s[/b]\n" % ("ACQUIRED" if GameState.master_override_key_taken else "MISSING")
		+ "- Basement Boiler Core Purge: [b]%s[/b]\n" % ("PURGED (True Ending Ready)" if GameState.core_purged else "ACTIVE")
	)

	# Rebuild 30-Anomaly Registry
	for c in _anomaly_list_vbox.get_children():
		c.queue_free()

	for entry in GameState.ANOMALY_CATALOG:
		var aid: int = entry["id"]
		var known: bool = GameState.discovered_anomalies.get(aid, false)
		var row := PanelContainer.new()
		row.add_theme_stylebox_override("panel", ThemeHelper.make_panel_style(
			Color(0.11, 0.13, 0.16, 0.9) if known else Color(0.07, 0.08, 0.10, 0.75),
			ThemeHelper.ACCENT_GREEN if known else ThemeHelper.BORDER_SUBTLE, 3, 1
		))
		var hb := HBoxContainer.new()
		hb.add_theme_constant_override("separation", 12)
		row.add_child(hb)

		var cat_icon := TextureRect.new()
		cat_icon.custom_minimum_size = Vector2(26, 26)
		cat_icon.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
		cat_icon.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
		var icon_file := "icon_cat_visual"
		match String(entry["category"]):
			"AUDIO": icon_file = "icon_cat_audio"
			"ENVIRONMENTAL": icon_file = "icon_cat_env"
			"ENTITY": icon_file = "icon_cat_entity"
		cat_icon.texture = ThemeHelper.make_icon_tex(icon_file)
		hb.add_child(cat_icon)

		var txt := Label.new()
		txt.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		txt.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
		if known:
			txt.text = "[%s] %s (%s — %s)\n%s" % [entry["code"], entry["title"], entry["category"], entry["room"], entry["hint"]]
			txt.add_theme_color_override("font_color", ThemeHelper.TEXT_MAIN)
		else:
			txt.text = "[%s] UNVERIFIED %s SIGNATURE  //  Sector: %s" % [entry["code"], entry["category"], entry["room"]]
			txt.add_theme_color_override("font_color", ThemeHelper.TEXT_MUTED)
		hb.add_child(txt)
		_anomaly_list_vbox.add_child(row)

	# Rebuild Archive List (Documents + Tapes)
	for c in _archive_list_vbox.get_children():
		c.queue_free()

	for doc_id in GameState.DOCUMENTS.keys():
		var has_doc: bool = GameState.collected_documents.get(doc_id, false)
		var meta: Dictionary = GameState.DOCUMENTS[doc_id]
		var b := Button.new()
		b.alignment = HORIZONTAL_ALIGNMENT_LEFT
		b.text = (meta["title"] if has_doc else "[UNFOUND DOCUMENT — " + meta["location"] + "]")
		b.disabled = not has_doc
		ThemeHelper.style_button(b, ThemeHelper.BORDER_AMBER)
		b.pressed.connect(func():
			_archive_reader.text = "[b][color=#efb958]%s[/color][/b]\n[i]Location: %s[/i]\n\n%s" % [meta["title"], meta["location"], meta["body"]]
		)
		_archive_list_vbox.add_child(b)

	for tape_id in GameState.TAPE_LOGS.keys():
		var has_tape: bool = GameState.played_tapes.get(tape_id, false)
		var tmeta: Dictionary = GameState.TAPE_LOGS[tape_id]
		var tb := Button.new()
		tb.alignment = HORIZONTAL_ALIGNMENT_LEFT
		tb.icon = ThemeHelper.make_icon_tex("icon_tape")
		tb.expand_icon = true
		tb.text = (tmeta["title"] if has_tape else "[UNPLAYED RECORDING]")
		tb.disabled = not has_tape
		ThemeHelper.style_button(tb, ThemeHelper.ACCENT_CRIMSON)
		tb.pressed.connect(func():
			_archive_reader.text = "[b][color=#efb958]%s[/color][/b]\nSpeaker: %s\n\n%s" % [tmeta["title"], tmeta["speaker"], tmeta["transcript"]]
			AudioDirector.play_tape(tape_id)
		)
		_archive_list_vbox.add_child(tb)
