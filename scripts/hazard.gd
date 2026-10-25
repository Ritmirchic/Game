class_name SectorHazard
extends Node2D

signal triggered(hazard: SectorHazard)

var kind := "storm"
var radius := 140.0
var timer := 1.4
var duration := 1.4
var age := 0.0
var hazard_color := Color("789cff")

func _ready() -> void:
	z_index = 12
	if kind == "well":
		radius = 460.0
		hazard_color = Color("a369df")

func _process(delta: float) -> void:
	age += delta
	if kind == "storm":
		timer -= delta
		if timer <= 0.0:
			triggered.emit(self)
			queue_free()
	queue_redraw()

func _draw() -> void:
	if kind == "storm":
		var progress := 1.0 - timer / duration
		var pulse := 0.55 + sin(age * 20.0) * 0.25
		draw_circle(Vector2.ZERO, radius, Color(0.25, 0.38, 0.85, 0.035 + progress * 0.06))
		draw_arc(Vector2.ZERO, radius, 0, TAU, 80, Color(hazard_color, 0.45 + progress * 0.4), 2.0, true)
		draw_arc(Vector2.ZERO, radius * (1.0 - progress), 0, TAU, 64, Color(0.65, 0.78, 1.0, pulse), 2.0, true)
		for i in range(8):
			var angle := float(i) / 8.0 * TAU + age * 0.25
			var p := Vector2.from_angle(angle) * radius
			draw_line(p * 0.88, p, Color(hazard_color, 0.75), 2.0, true)
		draw_line(Vector2(0, -500), Vector2.ZERO, Color(hazard_color, 0.12 + progress * 0.25), 2.0, true)
	elif kind == "well":
		for i in range(6):
			var r := 48.0 + i * 48.0 + sin(age * 1.5 + i) * 8.0
			var start := age * (0.22 + i * 0.05) + i
			draw_arc(Vector2.ZERO, r, start, start + PI * 1.38, 70, Color(hazard_color, 0.09 + i * 0.012), 2.0 + i * 0.25, true)
		draw_circle(Vector2.ZERO, 25.0 + sin(age * 2.0) * 4.0, Color(0.025, 0.01, 0.065, 0.95))
		draw_arc(Vector2.ZERO, 34.0, -age, -age + PI * 1.7, 48, Color(0.55, 0.31, 0.86, 0.5), 3.0, true)
