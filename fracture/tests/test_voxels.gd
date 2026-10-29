extends SceneTree

const WorldScript = preload("res://scripts/voxel_world.gd")
var failures := 0

func check(condition: bool, message: String) -> void:
 if condition: print("PASS: ", message)
 else:
  push_error("FAIL: " + message)
  failures += 1

func _initialize() -> void:
 call_deferred("run")

func run() -> void:
 create_timer(20.0).timeout.connect(func():
  push_error("Voxel test timed out")
  quit(1))
 var world = WorldScript.new()
 root.add_child(world)
 await process_frame
 world.box(Vector3i(-4,0,0),Vector3i(4,5,0),3)
 for c in world.chunk_cells: world.dirty[c] = true
 world.build_all()
 await physics_frame
 await physics_frame
 var space := world.get_world_3d().direct_space_state
 var query := PhysicsRayQueryParameters3D.create(Vector3(0.25,1.25,2),Vector3(0.25,1.25,-2))
 query.collision_mask = 1 # Ignore the secondary debris layer; test terrain collision itself.
 check(not space.intersect_ray(query).is_empty(),"solid wall initially blocks ray")
 var before: int = world.voxels.size()
 var count: int = world.damage(Vector3(0.25,1.25,0.25),0.8,1.6)
 check(count > 0 and world.voxels.size() == before-count,"impact removes individual voxels")
 check(world.dirty.size() <= 2,"edit dirties only local chunks")
 var saved: Array = world.serialize_removed()
 world.build_all()
 await process_frame
 await physics_frame
 await physics_frame
 check(space.intersect_ray(query).is_empty(),"rebuilt collision opens the breach")
 var world2 = WorldScript.new(); root.add_child(world2)
 await process_frame
 world2.box(Vector3i(-4,0,0),Vector3i(4,5,0),3)
 world2.apply_removed(saved)
 check(world2.voxels.size() == world.voxels.size(),"saved edits restore voxel occupancy")
 check(not world2.voxels.has(Vector3i(0,2,0)),"saved breach stays open")
 world.queue_free(); world2.queue_free()
 print("Voxel test failures: ",failures)
 quit(1 if failures else 0)
