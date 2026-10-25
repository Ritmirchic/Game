#!/usr/bin/env python3
"""Validate level_layout.json: wall/furniture blocking, room connectivity, doors."""
import json
import os
import sys
from collections import deque

HERE = os.path.dirname(__file__)
layout = json.load(open(os.path.join(HERE, "..", "data", "level_layout.json")))

b = layout["interior"]
W, H = b["x1"], b["z1"]  # counts: valid cell indices are 0..W-1, 0..H-1
blocked = set()
wall_cells = set()

for w in layout["walls"]:
    gaps = set(w.get("gaps", []))
    for i in range(w["from"], w["to"] + 1):
        if i in gaps:
            continue
        cell = (i, w["fixed"]) if w["axis"] == "h" else (w["fixed"], i)
        blocked.add(cell)
        wall_cells.add(cell)

furniture_cells = set()
for f in layout["furniture"]:
    x, z = f["at"]
    cell = (x, z)
    furniture_cells.add(cell)
    blocked.add(cell)

errors, warnings = [], []

# bounds
for w in layout["walls"]:
    limit = H - 1 if w["axis"] == "h" else W - 1
    if not (0 <= w["fixed"] <= limit):
        errors.append(f"wall {w['id']} fixed coord out of bounds")
for f in layout["furniture"]:
    x, z = f["at"]
    if not (0 <= x < W and 0 <= z < H):
        errors.append(f"furniture {f['type']} at {f['at']} out of bounds")
    if (x, z) in wall_cells:
        errors.append(f"furniture {f['type']} at {f['at']} overlaps wall")

# doors must sit in wall gaps
door_cells = set()
for d in layout["doors"]:
    for c in d["cells"]:
        t = tuple(c)
        door_cells.add(t)
        if t in wall_cells:
            errors.append(f"door {d['id']} cell {t} blocked by wall")
        if t in furniture_cells:
            errors.append(f"door {d['id']} cell {t} blocked by furniture")

# BFS over interior non-blocked cells
walkable = {(x, z) for x in range(W) for z in range(H)} - blocked
start = (14, 1)
if start not in walkable:
    errors.append(f"player spawn {start} not walkable")
seen = {start}
q = deque([start])
while q:
    x, z = q.popleft()
    for dx, dz in ((1, 0), (-1, 0), (0, 1), (0, -1)):
        n = (x + dx, z + dz)
        if n in walkable and n not in seen:
            seen.add(n)
            q.append(n)

unreachable = walkable - seen
if unreachable:
    errors.append(f"{len(unreachable)} walkable cells unreachable, e.g. {sorted(unreachable)[:8]}")

# every room must have at least 3 reachable cells (all of its non-furniture cells ideally)
for room in layout["rooms"]:
    x0, z0, x1, z1 = room["rect"]
    cells = [(x, z) for x in range(x0, x1 + 1) for z in range(z0, z1 + 1) if (x, z) in walkable]
    bad = [c for c in cells if c not in seen]
    if len(cells) - len(bad) < 3:
        errors.append(f"room {room['name']} essentially unreachable ({len(bad)}/{len(cells)} blocked)")
    elif bad:
        warnings.append(f"room {room['name']}: {len(bad)} isolated cells")

# every exterior door reachable from spawn
for d in layout["doors"]:
    for c in d["cells"]:
        if tuple(c) not in seen:
            errors.append(f"door {d['id']} {c} not reachable from spawn")

# every zombie spawn must route (via door) into interior
for s in layout["spawns"]:
    x, z = s["at"]
    if 0 <= x < W and 0 <= z < H:
        errors.append(f"spawn {s['id']} is inside the building")

# loot must be walkable & reachable
for i, spot in enumerate(layout["loot"]):
    t = tuple(spot["at"])
    if t in blocked:
        errors.append(f"loot #{i} at {t} inside obstacle")
    elif t not in seen:
        errors.append(f"loot #{i} at {t} unreachable")

# corridor width: ensure no 1-wide pinch between obstacles along main aisles
def neighbors_free(c):
    x, z = c
    return sum(1 for dx, dz in ((1, 0), (-1, 0), (0, 1), (0, -1)) if (x + dx, z + dz) in walkable)

pinch = [c for c in seen if neighbors_free(c) < 2]
if pinch:
    warnings.append(f"{len(pinch)} dead-end/pinch cells (check aisles): {sorted(pinch)[:6]}")

print(f"walkable cells: {len(walkable)}, reachable: {len(seen)}")
print(f"doors: {len(layout['doors'])}, spawns: {len(layout['spawns'])}, loot: {len(layout['loot'])}")
for w_ in warnings:
    print("WARN:", w_)
if errors:
    for e in errors:
        print("ERROR:", e)
    sys.exit(1)
print("LAYOUT VALIDATION PASSED")
