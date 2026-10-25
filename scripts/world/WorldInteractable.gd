class_name WorldInteractable
extends StaticBody3D
## Reusable interactive 3D object wrapper for Documents, Microcassette Recorders,
## Breakers, Threshold Controls, Secret Latches, Override Keys, and Anomaly Targets.

var interact_id: String = ""
var prompt_text: String = "[E] Interact"
var anomaly_tag_id: int = 0
var on_interact: Callable


func setup_box_collider(col_size: Vector3, col_offset: Vector3 = Vector3.ZERO) -> void:
	set_meta("interactable", self)
	var cs := CollisionShape3D.new()
	var bs := BoxShape3D.new()
	bs.size = col_size
	cs.shape = bs
	cs.position = col_offset
	add_child(cs)


func get_prompt() -> String:
	return prompt_text


func interact(actor: Node3D = null) -> void:
	if on_interact.is_valid():
		on_interact.call(actor)
