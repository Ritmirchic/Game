"""Procedural construction of the Ashes of Humanity world.

The world is baked into the place file (no runtime generation needed):
  * 6 connected regions with distinct architecture and palette
  * traversable interiors with furniture, loot containers and story props
  * interaction anchors consumed by the server's runtime services
"""
import math
import random

import world_kit as wk
from world_kit import Kit, offset, lerp, cf

WORLD = 3800.0
BASE_Y = 0.0

REGIONS = {
    "Suburbs": dict(center=(0, 0), color=(92, 96, 74), mat="Ground"),
    "Forest": dict(center=(-1150, 120), color=(58, 68, 48), mat="Ground"),
    "Industrial": dict(center=(1050, -180), color=(104, 98, 88), mat="Slate"),
    "City": dict(center=(-120, -1180), color=(96, 96, 94), mat="Concrete"),
    "Military": dict(center=(1150, 1050), color=(86, 88, 70), mat="Ground"),
    "Wasteland": dict(center=(-1200, -1150), color=(122, 88, 66), mat="Sand"),
}


class World:
    def __init__(self, parent, Instance, M, E):
        self.workspace = parent
        self.Instance = Instance
        self.M = M
        self.E = E
        self.kit = Kit(Instance, M, E, random.Random(20260101))
        self.rng = self.kit.rng
        self.world = self.kit.model(parent, "World")
        self.regions = self.kit.model(self.world, "Regions")
        self.roads = self.kit.model(self.world, "RoadNetwork")
        self.landmarks = self.kit.model(self.world, "Landmarks")
        self.points = self.kit.model(self.world, "SpawnLocations")
        self.enemy_dirs = self.kit.model(self.world, "EnemySpawns")

    # ------------------------------------------------------------- region api
    def region(self, name):
        info = REGIONS[name]
        m = self.kit.model(self.regions, name)
        return m, info

    def anchor(self, parent, kind, pos, table=None, extra=None, name=None):
        return self.kit.anchor(parent, name or (kind + "Anchor"), pos, kind, table, extra)

    def enemy_anchor(self, parent, pos, tier=1, region="Suburbs"):
        a = self.kit.anchor(parent, "EnemySpawn", pos, "EnemySpawn", None,
                             {"Tier": tier, "Region": region})
        return a

    def spawn_anchor(self, parent, pos):
        self.kit.anchor(parent, "PlayerSpawn", pos, "PlayerSpawn")


# --------------------------------------------------------------------- ground

def build_ground(world):
    kit = world.kit
    rng = world.rng
    ground = kit.model(world.world, "Ground")
    tile = 640.0
    n = 6
    for ix in range(-n // 2, n // 2):
        for iz in range(-n // 2, n // 2):
            cx = ix * tile + tile / 2
            cz = iz * tile + tile / 2
            # nearest region decides the material
            best, best_d = None, 1e12
            for name, info in REGIONS.items():
                d = (info["center"][0] - cx) ** 2 + (info["center"][1] - cz) ** 2
                if d < best_d:
                    best, best_d = name, d
            info = REGIONS[best]
            jitter = rng.randint(-6, 6)
            color = tuple(max(0, min(255, c + jitter)) for c in info["color"])
            kit.part(ground, "GroundTile", (tile + 1, 8, tile + 1),
                     (cx, BASE_Y - 4, cz), mat=info["mat"], color=color, shadow=False,
                     query=True)

    # gentle terrain relief: shallow, wide, walkable undulations
    for i in range(70):
        ang = rng.uniform(0, math.tau)
        dist = rng.uniform(200, WORLD * 0.47)
        pos = (math.cos(ang) * dist, BASE_Y, math.sin(ang) * dist)
        r = rng.uniform(50, 150)
        h = rng.uniform(1.6, 5.0)
        kit.part(ground, "Mound", (r, h, r * rng.uniform(0.7, 1.3)),
                 (pos[0], BASE_Y - h * 0.55, pos[2]),
                 yaw=rng.uniform(0, 360), mat="Ground", color=(78, 82, 62),
                 shape="ball", collidable=True, shadow=False, query=True)


def build_roads(world):
    kit = world.kit
    rng = world.rng
    asphalt = (54, 54, 56)
    centers = [
        ((0, 0), (-1150, 120), 66),
        ((0, 0), (1050, -180), 72),
        ((0, 0), (-120, -1180), 68),
        ((1050, -180), (1150, 1050), 60),
        ((-120, -1180), (-1200, -1150), 58),
        ((-1150, 120), (-1200, -1150), 54),
    ]
    for a, b, width in centers:
        dx, dz = b[0] - a[0], b[1] - a[1]
        length = math.hypot(dx, dz)
        yaw = math.degrees(math.atan2(-dz, dx))
        segments = max(1, int(length / 90))
        for i in range(segments):
            t = (i + 0.5) / segments
            px = lerp(a[0], b[0], t)
            pz = lerp(a[1], b[1], t)
            kit.part(world.roads, "Road", (length / segments + 2, 1.2, width),
                     (px, BASE_Y + 0.35, pz), yaw=yaw, mat="Asphalt", color=asphalt,
                     shadow=False)
            if i % 3 == 0:
                # faded center line
                kit.part(world.roads, "RoadLine", (length / segments * 0.5, 0.1, 0.7),
                         (px, BASE_Y + 0.98, pz), yaw=yaw, mat="Concrete",
                         color=(146, 140, 104), collidable=False, shadow=False)
            if rng.random() < 0.35:
                ang = math.radians(yaw)
                sx = px + math.cos(ang) * rng.uniform(-20, 20) + math.sin(ang) * rng.uniform(-width / 2, width / 2)
                sz = pz - math.sin(ang) * rng.uniform(-20, 20) + math.cos(ang) * rng.uniform(-width / 2, width / 2)
                kit.car_wreck(world.roads, (sx, BASE_Y + 0.9, sz), yaw=rng.uniform(0, 360),
                              burnt=rng.random() < 0.3, flipped=rng.random() < 0.12)
        # roadside lamps
        lamps = max(2, int(length / 150))
        for i in range(lamps):
            t = (i + 0.5) / lamps
            px = lerp(a[0], b[0], t) + rng.uniform(-12, 12)
            pz = lerp(a[1], b[1], t) + rng.uniform(-12, 12)
            kit.streetlamp(world.roads, (px, BASE_Y, pz), yaw=yaw + 90, on=rng.random() < 0.18)


# ------------------------------------------------------------------- suburbs

def build_suburb_house(world, parent, pos, yaw, rng, two_floor=False, burnt=False):
    kit = world.kit
    w, d, h = 34.0, 30.0, 12.0
    m = kit.model(parent, "House")
    wall_col = (118, 108, 96) if not burnt else (58, 54, 50)
    trim = (86, 78, 68) if not burnt else (44, 42, 40)
    kit.floor(m, pos, w, d, yaw=yaw, mat="Concrete", color=(96, 92, 86))
    # exterior walls with openings
    walls = [
        ((0, 0, -d / 2), 0, [(w / 2 - 5, 6, 0, 8), (4, 7, 4, 9)]),
        ((0, 0, d / 2), 180, [(w / 2 - 8, 6, 0, 8), (3, 6, 5, 9)]),
        ((-w / 2, 0, 0), 90, [(d / 2 - 6, 5, 5, 9)]),
        ((w / 2, 0, 0), -90, [(d / 2 - 6, 5, 5, 9), (5, 4, 5, 9)]),
    ]
    for (lx, ly, lz), wy, openings in walls:
        p = offset(pos, yaw, right=lx, forward=-lz)
        kit.wall_open(m, p, yaw + wy, w if wy in (0, 180) else d, h, 0.9,
                      "Brick" if rng.random() < 0.5 else "Concrete", wall_col, openings)
    # door
    door_pos = offset(pos, yaw, right=w / 2 - 5, forward=-d / 2)
    kit.part(m, "DoorFrame", (6.4, 8.4, 0.5), (door_pos[0], BASE_Y + 4.2, door_pos[2]),
             yaw=yaw, mat="Wood", color=trim)
    world.kit.part(m, "Door", (5.8, 8.0, 0.4), (door_pos[0], BASE_Y + 4.0, door_pos[2]),
                   yaw=yaw, mat="Wood", color=(74, 58, 44), collidable=True,
                   query=True).props["CanCollide"] = True
    world.anchor(m, "Door", (door_pos[0], BASE_Y + 4.0, door_pos[2]), name="DoorAnchor")
    # interior partition
    kit.wall_open(m, offset(pos, yaw, forward=-3), yaw + 90, d * 0.55, h, 0.6,
                  "Plaster", (128, 122, 112), [(d * 0.18, 5, 0, 8)])
    # ceiling
    kit.floor(m, (pos[0], BASE_Y + h, pos[2]), w, d, yaw=yaw, mat="WoodPlanks",
              color=(92, 78, 62), thickness=0.8)
    if not burnt:
        kit.roof_gable(m, (pos[0], BASE_Y + h + 0.8, pos[2]), w, d, yaw=yaw,
                       mat="WoodPlanks", color=(72, 54, 44), rise=7.0)
    else:
        kit.roof_flat(m, (pos[0], BASE_Y + h, pos[2]), w * 0.7, d * 0.7, yaw=yaw,
                      mat="Concrete", color=(52, 50, 48), parapet=False)
        for i in range(10):
            ang = rng.uniform(0, math.tau)
            kit.part(m, "CollapsedBeam", (0.8, rng.uniform(8, 18), 0.8),
                     (pos[0] + math.cos(ang) * rng.uniform(0, 12), BASE_Y + h - 2,
                      pos[2] + math.sin(ang) * rng.uniform(0, 12)),
                     yaw=rng.uniform(0, 360), pitch=rng.uniform(30, 80),
                     mat="Wood", color=(50, 44, 38), collidable=False)

    # furniture / loot
    interior = [
        lambda: (kit.table(m, offset(pos, yaw, right=-8, forward=6, up=0.0), yaw,
                           mat="Wood", color=(92, 74, 58)),
                 kit.chair(m, offset(pos, yaw, right=-8, forward=8.6), yaw + 180),
                 kit.chair(m, offset(pos, yaw, right=-8, forward=3.4), yaw)),
        lambda: (kit.sofa(m, offset(pos, yaw, right=8, forward=6), yaw + 180),
                 kit.counter(m, offset(pos, yaw, right=8, forward=-8), yaw, 8.0)),
        lambda: (kit.bed(m, offset(pos, yaw, right=8, forward=-8), yaw),
                 kit.shelf(m, offset(pos, yaw, right=14.6, forward=-8), yaw - 90),
                 kit.cabinet(m, offset(pos, yaw, right=14.6, forward=-2), yaw - 90)),
        lambda: (kit.fridge(m, offset(pos, yaw, right=-14, forward=-10), yaw + 90),
                 kit.counter(m, offset(pos, yaw, right=-14, forward=-2), yaw + 90, 10.0),
                 kit.sink(m, offset(pos, yaw, right=-13.4, forward=-2, up=3.2), yaw)),
        lambda: (kit.desk(m, offset(pos, yaw, right=6, forward=10), yaw),
                 kit.cabinet(m, offset(pos, yaw, right=12, forward=10), yaw)),
    ]
    rng.shuffle(interior)
    for fn in interior[: rng.randint(2, 4)]:
        fn()

    # loot containers
    tables = ["residential", "residential", "kitchen", "medical", "tools"]
    for i in range(rng.randint(3, 5)):
        ang = rng.uniform(0, math.tau)
        rad = rng.uniform(4, 12)
        lx = pos[0] + math.cos(ang) * rad
        lz = pos[2] + math.sin(ang) * rad
        world.anchor(m, "Loot", (lx, BASE_Y + 1.2, lz), table=rng.choice(tables),
                     name="LootAnchor")
    kit.lights(m, "CeilingLamp", (pos[0], BASE_Y + h - 2.5, pos[2]),
               (255, 226, 180), 0.6 if not burnt else 0.0, 22)
    if two_floor:
        stair_pos = offset(pos, yaw, right=-12, forward=-2)
        kit.stairs(m, stair_pos, yaw + 180, width=5.0, steps=9, rise=1.35, run=1.6,
                   mat="Wood", color=(92, 78, 62))
        kit.floor(m, (pos[0], BASE_Y + 12.0, pos[2]), w - 2, d - 2, yaw=yaw,
                  mat="WoodPlanks", color=(88, 74, 60), thickness=0.7)
        kit.bed(m, offset(pos, yaw, right=8, forward=-6, up=12.0), yaw)
        kit.shelf(m, offset(pos, yaw, right=14.0, forward=-6, up=12.0), yaw - 90)
        world.anchor(m, "Loot", (offset(pos, yaw, right=4, forward=-6, up=12.4)),
                     table="residential", name="LootAnchor")
    # exterior dressing
    for i in range(rng.randint(2, 5)):
        ang = rng.uniform(0, math.tau)
        rad = rng.uniform(20, 30)
        kit.bush(m, (pos[0] + math.cos(ang) * rad, BASE_Y, pos[2] + math.sin(ang) * rad),
                 scale=rng.uniform(0.7, 1.3))
    if rng.random() < 0.6:
        kit.fence(m, offset(pos, yaw, right=-w / 2 - 6, forward=d / 2 + 4),
                  offset(pos, yaw, right=w / 2 + 6, forward=d / 2 + 4))
    world.enemy_anchor(m, (offset(pos, yaw, forward=-d / 2 - 7, right=rng.uniform(-8, 8))),
                       tier=1, region="Suburbs")
    return m


def build_suburbs(world):
    kit = world.kit
    rng = world.rng
    reg, info = world.region("Suburbs")
    c = info["center"]
    # street grid
    for row in range(3):
        z = c[1] - 260 + row * 260
        kit.part(reg, "Street", (760, 1.0, 44), (c[0], BASE_Y + 0.3, z),
                 mat="Asphalt", color=(56, 56, 58), shadow=False)
        for i in range(6):
            kit.part(reg, "RoadLine", (16, 0.1, 0.6),
                     (c[0] - 330 + i * 130, BASE_Y + 0.85, z), mat="Concrete",
                     color=(150, 144, 108), collidable=False, shadow=False)
        for i in range(4):
            x = c[0] - 300 + i * 200
            world.kit.streetlamp(reg, (x, BASE_Y, z + 26), yaw=180,
                                 on=rng.random() < 0.22)
            kit.car_wreck(reg, (x + rng.uniform(-30, 30), BASE_Y + 0.6, z + rng.uniform(-14, 14)),
                          yaw=rng.uniform(0, 360), burnt=rng.random() < 0.3)
    for col in range(3):
        x = c[0] - 260 + col * 260
        kit.part(reg, "Street", (44, 1.0, 760), (x, BASE_Y + 0.3, c[1]),
                 mat="Asphalt", color=(56, 56, 58), shadow=False)

    # houses along the blocks
    houses = []
    for bx in range(-2, 3):
        for bz in range(-2, 3):
            if abs(bx) < 1 and abs(bz) < 1:
                continue
            hx = c[0] + bx * 260 + rng.uniform(-24, 24)
            hz = c[1] + bz * 260 + rng.uniform(-24, 24)
            yaw = rng.choice([0, 90, 180, 270])
            two = rng.random() < 0.32
            burnt = rng.random() < 0.22
            houses.append(build_suburb_house(world, reg, (hx, BASE_Y, hz), yaw, rng,
                                             two_floor=two, burnt=burnt))
    # corner shop
    shop_pos = (c[0] + 300, BASE_Y, c[1] + 300)
    m = kit.model(reg, "CornerShop")
    kit.floor(m, shop_pos, 56, 40, mat="Concrete", color=(104, 100, 94))
    kit.wall_open(m, offset(shop_pos, 0, forward=-20), 0, 56, 16, 0.9, "Concrete",
                  (110, 104, 94), [(22, 12, 0, 10)])
    kit.wall_open(m, offset(shop_pos, 0, forward=20), 180, 56, 16, 0.9, "Concrete",
                  (110, 104, 94), [(10, 8, 6, 12)])
    kit.wall_open(m, offset(shop_pos, 0, right=28), 90, 40, 16, 0.9, "Concrete",
                  (110, 104, 94), [(10, 10, 6, 12)])
    kit.wall_open(m, offset(shop_pos, 0, right=-28), -90, 40, 16, 0.9, "Concrete",
                  (110, 104, 94), [(10, 8, 6, 12)])
    kit.floor(m, (shop_pos[0], BASE_Y + 16, shop_pos[2]), 58, 42, mat="Concrete",
              color=(92, 90, 86), thickness=0.8)
    for i in range(4):
        x = shop_pos[0] - 18 + i * 12
        kit.shelf(m, (x, BASE_Y, shop_pos[2] + 6), 180, w=6.0, h=8.0)
        kit.crate(m, (x, BASE_Y, shop_pos[2] - 8), yaw=rng.uniform(0, 30), size=3.0)
    kit.counter(m, (shop_pos[0] + 18, BASE_Y, shop_pos[2] - 12), 90, 14.0)
    for i in range(4):
        world.anchor(m, "Loot", (shop_pos[0] - 18 + i * 12, BASE_Y + 2, shop_pos[2] + 6),
                     table="food", name="LootAnchor")
    kit.lights(m, "ShopLight", (shop_pos[0], BASE_Y + 13, shop_pos[2]), (240, 232, 200), 1.0, 34)
    world.enemy_anchor(m, (shop_pos[0] + 20, BASE_Y, shop_pos[2] - 24), 1, "Suburbs")

    # fuel station
    fuel = (c[0] - 320, BASE_Y, c[1] - 320)
    m = kit.model(reg, "FuelStation")
    kit.floor(m, fuel, 70, 54, mat="Concrete", color=(100, 98, 94))
    for i in range(2):
        px = fuel[0] - 14 + i * 28
        kit.part(m, "PumpIsland", (16, 2, 12), (px, BASE_Y + 1, fuel[2]),
                 mat="Concrete", color=(118, 116, 110))
        for s in (-1, 1):
            kit.part(m, "Pump", (3.0, 8.0, 2.2),
                     (px + s * 5, BASE_Y + 5, fuel[2]), mat="Metal", color=(120, 62, 54))
        kit.part(m, "Canopy", (66, 1.2, 44), (fuel[0], BASE_Y + 22, fuel[2]),
                 mat="CorrodedMetal", color=(74, 72, 66))
        for s in (-1, 1):
            kit.part(m, "CanopyColumn", (1.6, 22, 1.6),
                     (fuel[0] + s * 28, BASE_Y + 11, fuel[2] + s * 16),
                     mat="Concrete", color=(112, 108, 100))
        for i in range(3):
            world.anchor(m, "Loot", (px + rng.uniform(-6, 6), BASE_Y + 2, fuel[2] + rng.uniform(-8, 8)),
                         table="fuel", name="LootAnchor")
        kit.car_wreck(m, (fuel[0] + 20, BASE_Y + 0.6, fuel[2] + 18), yaw=30, burnt=True)
        world.enemy_anchor(m, (fuel[0] - 24, BASE_Y, fuel[2] + 18), 1, "Suburbs")

    # emergency shelter entrance (player start)
    build_shelter(world, reg, (c[0] + 40, BASE_Y, c[1] + 40))
    return reg


# -------------------------------------------------------------------- shelter

def build_shelter(world, parent, pos):
    kit = world.kit
    rng = world.rng
    m = kit.model(parent, "EmergencyShelter")
    # surface hatch building
    kit.floor(m, pos, 26, 22, mat="Concrete", color=(86, 84, 80))
    kit.wall_open(m, offset(pos, 0, forward=-11), 0, 26, 10, 0.8, "Concrete",
                  (92, 90, 86), [(9, 7, 0, 7.5)])
    kit.wall_open(m, offset(pos, 0, forward=11), 180, 26, 10, 0.8, "Concrete",
                  (92, 90, 86), [])
    kit.wall_open(m, offset(pos, 0, right=13), 90, 22, 10, 0.8, "Concrete",
                  (92, 90, 86), [(8, 6, 4, 8)])
    kit.wall_open(m, offset(pos, 0, right=-13), -90, 22, 10, 0.8, "Concrete",
                  (92, 90, 86), [])
    kit.roof_flat(m, (pos[0], BASE_Y + 10, pos[2]), 27, 23, mat="Concrete",
                  color=(78, 76, 72), height=0.9)
    kit.lights(m, "ShelterLamp", (pos[0], BASE_Y + 8, pos[2]), (255, 110, 70), 0.6, 16)

    # staircase shaft down
    shaft = (pos[0] + 6, BASE_Y, pos[2])
    for i in range(9):
        kit.part(m, "ShaftStep", (9, 1.6, 4.0),
                 (shaft[0], BASE_Y - 1.6 * (i + 1) + 0.8, shaft[2] - 2.0 - i * 3.6),
                 mat="Concrete", color=(72, 70, 66))
    kit.wall(m, (shaft[0] - 5.5, BASE_Y - 8, shaft[2] - 16), 0, 22, 18, 0.8,
             "Concrete", (70, 68, 64), sill=-18)
    kit.wall(m, (shaft[0] + 5.5, BASE_Y - 8, shaft[2] - 16), 0, 22, 18, 0.8,
             "Concrete", (70, 68, 64), sill=-18)
    # main bunker room
    bunker_y = BASE_Y - 20
    bunker = (pos[0], bunker_y, pos[2] - 34)
    kit.floor(m, bunker, 44, 34, mat="Concrete", color=(64, 64, 62), thickness=1.2)
    for (dx, dz, yaw, length) in ((0, -17, 0, 44), (0, 17, 180, 44),
                                  (-22, 0, 90, 34), (22, 0, -90, 34)):
        kit.wall_open(m, (bunker[0] + dx, bunker_y, bunker[2] + dz), yaw, length, 14, 1.0,
                      "Concrete", (72, 72, 70), [] if dz != 17 else [(16, 8, 0, 9)])
    kit.floor(m, (bunker[0], bunker_y + 14, bunker[2]), 45, 35, mat="Concrete",
              color=(58, 58, 56), thickness=0.9)
    # interior: bunks, radio, crates, dead terminal
    kit.bed(m, (bunker[0] - 14, bunker_y, bunker[2] - 10), 90, color=(72, 74, 68))
    kit.bed(m, (bunker[0] - 14, bunker_y, bunker[2] - 2), 90, color=(72, 74, 68))
    kit.table(m, (bunker[0] + 10, bunker_y, bunker[2] - 8), 0, w=8, d=4)
    kit.chair(m, (bunker[0] + 10, bunker_y, bunker[2] - 4), 0)
    kit.shelf(m, (bunker[0] + 18, bunker_y, bunker[2] - 2), -90, w=8, h=8)
    kit.crate(m, (bunker[0] - 18, bunker_y, bunker[2] + 10), size=3.4)
    kit.crate(m, (bunker[0] - 14, bunker_y + 3.4, bunker[2] + 10), size=2.8)
    kit.generator(m, (bunker[0] + 16, bunker_y, bunker[2] + 10), yaw=-90)
    # radio desk (story anchor)
    radio_pos = (bunker[0] + 10, bunker_y + 1.2, bunker[2] - 8)
    kit.part(m, "Radio", (3.2, 2.0, 2.4), (radio_pos[0], bunker_y + 3.2, radio_pos[2]),
             mat="Metal", color=(52, 54, 50))
    kit.part(m, "RadioDial", (1.2, 0.8, 0.2),
             (radio_pos[0], bunker_y + 3.4, radio_pos[2] - 1.3), mat="Glass",
             color=(126, 178, 140), collidable=False, shadow=False)
    world.anchor(m, "Radio", radio_pos)
    kit.lights(m, "BunkerLamp", (bunker[0], bunker_y + 12, bunker[2]), (255, 132, 78), 1.4, 34,
               shadows=True)
    kit.lights(m, "BunkerLamp2", (bunker[0] + 16, bunker_y + 12, bunker[2] + 12),
               (255, 96, 60), 0.7, 20)
    # cot for saving/sleeping
    world.anchor(m, "Bed", (bunker[0] - 14, bunker_y + 2.4, bunker[2] - 10), name="BedAnchor")
    for i in range(6):
        world.anchor(m, "Loot", (bunker[0] + rng.uniform(-18, 18), bunker_y + 1.4,
                                 bunker[2] + rng.uniform(-14, 14)),
                     table=rng.choice(["shelter", "medical", "tools"]), name="LootAnchor")
    world.anchor(m, "Workbench", (bunker[0] + 18, bunker_y + 2.4, bunker[2] - 2),
                 name="WorkbenchAnchor")

    # player spawn (face the room)
    spawn_pos = (bunker[0] - 4, bunker_y + 3.5, bunker[2] + 8)
    sp = world.Instance("SpawnLocation", "PlayerSpawn")
    sp.props.update({
        "Size": (10.0, 1.0, 10.0),
        "CFrame": cf(spawn_pos, 0),
        "Color": (120, 120, 116),
        "Material": world.M("Concrete"),
        "Anchored": True,
        "CanCollide": True,
        "Transparency": 1.0,
        "Neutral": True,
        "Duration": 0,
        "TopSurface": 0, "BottomSurface": 0, "LeftSurface": 0, "RightSurface": 0,
        "FrontSurface": 0, "BackSurface": 0,
    })
    sp.add(world.Instance("Decal", "Decal"))
    world.points.add(sp)
    world.spawn_anchor(m, spawn_pos)
    return m


# -------------------------------------------------------------------- forest

def build_forest(world):
    kit = world.kit
    rng = world.rng
    reg, info = world.region("Forest")
    c = info["center"]
    # dense woods
    trees = 0
    for i in range(760):
        ang = rng.uniform(0, math.tau)
        rad = rng.uniform(30, 480)
        x = c[0] + math.cos(ang) * rad
        z = c[1] + math.sin(ang) * rad
        dead = rng.random() < 0.3
        kit.tree(reg, (x, BASE_Y, z), scale=rng.uniform(0.7, 1.35), dead=dead)
        trees += 1
    for i in range(180):
        ang = rng.uniform(0, math.tau)
        rad = rng.uniform(20, 480)
        kit.bush(reg, (c[0] + math.cos(ang) * rad, BASE_Y, c[1] + math.sin(ang) * rad),
                 scale=rng.uniform(0.6, 1.6))
    for i in range(120):
        ang = rng.uniform(0, math.tau)
        rad = rng.uniform(20, 480)
        kit.rock(reg, (c[0] + math.cos(ang) * rad, BASE_Y, c[1] + math.sin(ang) * rad),
                 scale=rng.uniform(0.5, 1.4))
    for i in range(40):
        kit.grass_tufts(reg, (c[0] + rng.uniform(-450, 450), BASE_Y, c[1] + rng.uniform(-450, 450)),
                        radius=rng.uniform(6, 20), count=rng.randint(8, 18))

    # logging cabins
    for i in range(4):
        ang = rng.uniform(0, math.tau)
        rad = rng.uniform(150, 380)
        pos = (c[0] + math.cos(ang) * rad, BASE_Y, c[1] + math.sin(ang) * rad)
        build_cabin(world, reg, pos, rng.uniform(0, 360), rng)

    # watchtower camp
    camp = (c[0] + 190, BASE_Y, c[1] - 170)
    m = kit.model(reg, "RangerCamp")
    kit.watchtower(m, camp, yaw=25)
    kit.tent(m, (camp[0] + 26, BASE_Y, camp[2] + 8), yaw=15)
    kit.campfire(m, (camp[0] + 8, BASE_Y, camp[2] + 22), lit=True)
    world.anchor(m, "Campfire", (camp[0] + 8, BASE_Y + 1.2, camp[2] + 22), name="CampfireAnchor")
    world.anchor(m, "Loot", (camp[0] + 26, BASE_Y + 1.2, camp[2] + 8), table="camp",
                 name="LootAnchor")
    world.anchor(m, "Loot", (camp[0] - 4, BASE_Y + 1.2, camp[2] + 26), table="tools",
                 name="LootAnchor")
    world.enemy_anchor(m, (camp[0] + 40, BASE_Y, camp[2] + 30), 2, "Forest")

    # cave / tunnel entrance
    cave = (c[0] - 300, BASE_Y, c[1] + 250)
    m = kit.model(reg, "BlackwoodCave")
    kit.rock(m, cave, scale=3.4)
    kit.part(m, "CaveMouth", (26, 22, 30), (cave[0], BASE_Y + 8, cave[2]),
             mat="Rock", color=(52, 48, 44))
    tunnel_y = BASE_Y + 1
    for i in range(14):
        z = cave[2] - 8 - i * 16
        for s in (-1, 1):
            kit.part(m, "TunnelWall", (4, 24, 16),
                     (cave[0] + s * 13, tunnel_y + 10, z), mat="Rock",
                     color=(46, 42, 39), shadow=False)
        kit.part(m, "TunnelCeiling", (30, 4, 16), (cave[0], tunnel_y + 22, z),
                 mat="Rock", color=(42, 39, 36), shadow=False)
        kit.part(m, "TunnelFloor", (28, 2, 16), (cave[0], tunnel_y - 1, z),
                 mat="Mud", color=(52, 44, 34), shadow=False)
        if i % 3 == 0:
            kit.lights(m, "CaveGlow", (cave[0] + rng.uniform(-8, 8), tunnel_y + 6, z),
                       (120, 190, 150), 0.35, 12)
        if i % 4 == 1:
            world.enemy_anchor(m, (cave[0], tunnel_y, z), 3, "Forest")
    # lab door at tunnel end (quest)
    end_y = tunnel_y
    end = (cave[0], end_y, cave[2] - 8 - 13 * 16 - 10)
    kit.part(m, "BlastDoor", (18, 16, 1.6), (end[0], end_y + 8, end[2]),
             mat="DiamondPlate", color=(64, 66, 60))
    kit.part(m, "BlastDoorMark", (10, 3, 0.3), (end[0], end_y + 10, end[2] + 1.0),
             mat="Concrete", color=(132, 118, 68), collidable=False, shadow=False)
    world.anchor(m, "LabDoor", (end[0], end_y + 8, end[2] + 2.0), name="LabDoorAnchor")
    for i in range(3):
        world.anchor(m, "Loot", (end[0] + rng.uniform(-8, 8), end_y + 2,
                                 end[2] + rng.uniform(6, 40)), table="camp", name="LootAnchor")

    # hidden survivor shelter
    surv = (c[0] - 120, BASE_Y, c[1] - 300)
    m = kit.model(reg, "HiddenShelter")
    kit.floor(m, surv, 22, 18, mat="WoodPlanks", color=(74, 62, 50))
    kit.wall_open(m, offset(surv, 0, forward=-9), 0, 22, 10, 0.7, "WoodPlanks",
                  (72, 60, 48), [(8, 6, 0, 7)])
    kit.wall_open(m, offset(surv, 0, forward=9), 180, 22, 10, 0.7, "WoodPlanks",
                  (72, 60, 48), [(6, 6, 4, 8)])
    kit.wall_open(m, offset(surv, 0, right=11), 90, 18, 10, 0.7, "WoodPlanks",
                  (72, 60, 48), [])
    kit.wall_open(m, offset(surv, 0, right=-11), -90, 18, 10, 0.7, "WoodPlanks",
                  (72, 60, 48), [])
    kit.roof_gable(m, (surv[0], BASE_Y + 10, surv[2]), 22, 18, yaw=0,
                   mat="WoodPlanks", color=(64, 52, 42), rise=5.0)
    kit.bed(m, offset(surv, 0, right=-6, forward=-4), 0, color=(70, 70, 62))
    kit.table(m, offset(surv, 0, right=4, forward=3), 0, w=5, d=3)
    kit.shelf(m, offset(surv, 0, right=9, forward=0), -90, w=7, h=7)
    world.anchor(m, "Bed", offset(surv, 0, right=-6, forward=-4, up=2.4), name="BedAnchor")
    world.anchor(m, "Loot", offset(surv, 0, right=4, forward=3, up=1.4), table="camp",
                 name="LootAnchor")
    world.anchor(m, "Document", offset(surv, 0, right=9, forward=0, up=1.4),
                 extra={"DocId": "doc_shelter_letter"}, name="DocumentAnchor")
    kit.lights(m, "ShelterCandle", offset(surv, 0, up=7), (255, 168, 96), 0.5, 14)
    return reg


def build_cabin(world, parent, pos, yaw, rng):
    kit = world.kit
    m = kit.model(parent, "Cabin")
    kit.floor(m, pos, 24, 20, mat="WoodPlanks", color=(76, 64, 52))
    kit.wall_open(m, offset(pos, yaw, forward=-10), yaw, 24, 11, 0.8, "WoodPlanks",
                  (80, 66, 52), [(8, 6, 0, 7)])
    kit.wall_open(m, offset(pos, yaw, forward=10), yaw + 180, 24, 11, 0.8, "WoodPlanks",
                  (80, 66, 52), [(6, 5, 4, 8)])
    kit.wall_open(m, offset(pos, yaw, right=12), yaw + 90, 20, 11, 0.8, "WoodPlanks",
                  (80, 66, 52), [])
    kit.wall_open(m, offset(pos, yaw, right=-12), yaw - 90, 20, 11, 0.8, "WoodPlanks",
                  (80, 66, 52), [])
    kit.roof_gable(m, (pos[0], BASE_Y + 11, pos[2]), 24, 20, yaw=yaw,
                   mat="WoodPlanks", color=(62, 50, 40), rise=6.0)
    kit.table(m, offset(pos, yaw, right=-5, forward=3), yaw, w=5, d=3)
    kit.chair(m, offset(pos, yaw, right=-5, forward=5.6), yaw + 180)
    kit.shelf(m, offset(pos, yaw, right=9, forward=-4), yaw - 90, w=6, h=7)
    kit.crate(m, offset(pos, yaw, right=8, forward=6), yaw=yaw + 20, size=3.0)
    kit.campfire(m, offset(pos, yaw, right=-8, forward=-6), lit=rng.random() < 0.3)
    world.anchor(m, "Loot", offset(pos, yaw, right=9, forward=-4, up=1.4), table="camp",
                 name="LootAnchor")
    world.anchor(m, "Loot", offset(pos, yaw, right=8, forward=6, up=1.4), table="camp",
                 name="LootAnchor")
    world.anchor(m, "Campfire", offset(pos, yaw, right=-8, forward=-6, up=1.2),
                 name="CampfireAnchor")
    world.enemy_anchor(m, offset(pos, yaw, forward=-16), 2, "Forest")
    kit.lights(m, "CabinLamp", offset(pos, yaw, up=8), (255, 190, 130),
               0.5 if rng.random() < 0.4 else 0.0, 16)


# ---------------------------------------------------------------- industrial

def build_warehouse(world, parent, pos, yaw, rng, size=(110, 130, 34)):
    kit = world.kit
    w, d, h = size
    m = kit.model(parent, "Warehouse")
    kit.floor(m, pos, w, d, yaw=yaw, mat="Concrete", color=(96, 94, 88), thickness=1.4)
    # shell
    kit.wall_open(m, offset(pos, yaw, forward=-d / 2), yaw, w, h, 1.2, "Concrete",
                  (104, 100, 92), [(20, 26, 0, 22)])
    kit.wall_open(m, offset(pos, yaw, forward=d / 2), yaw + 180, w, h, 1.2, "Concrete",
                  (104, 100, 92), [(w / 2 - 8, 8, 14, 20)])
    kit.wall_open(m, offset(pos, yaw, right=w / 2), yaw + 90, d, h, 1.2, "Concrete",
                  (104, 100, 92), [(d / 2 - 10, 10, 12, 20)])
    kit.wall_open(m, offset(pos, yaw, right=-w / 2), yaw - 90, d, h, 1.2, "Concrete",
                  (104, 100, 92), [])
    kit.roof_flat(m, (pos[0], BASE_Y + h, pos[2]), w, d, yaw=yaw, mat="CorrodedMetal",
                  color=(78, 76, 70), parapet=False)
    # roof trusses
    r = math.radians(yaw)
    for i in range(6):
        along = -d / 2 + (i + 0.5) * (d / 6)
        px = pos[0] - math.sin(r) * along
        pz = pos[2] - math.cos(r) * along
        kit.part(m, "Truss", (w - 4, 1.4, 1.0), (px, BASE_Y + h - 2, pz), yaw=yaw,
                 mat="Metal", color=(72, 70, 66), collidable=False)
        for s in (-1, 1):
            kit.part(m, "TrussBrace", (1.0, 6.0, 1.0),
                     (px + math.cos(r) * s * (w / 2 - 8), BASE_Y + h - 5, pz - math.sin(r) * s * (w / 2 - 8)),
                     yaw=yaw, pitch=35 * s, mat="Metal", color=(72, 70, 66),
                     collidable=False)
    # racks + pallets + crates
    for row in range(3):
        for col in range(4):
            rx = pos[0] + math.cos(r) * (-w / 2 + 18 + row * 34) - math.sin(r) * (-d / 2 + 24 + col * 26)
            rz = pos[2] - math.sin(r) * (-w / 2 + 18 + row * 34) - math.cos(r) * (-d / 2 + 24 + col * 26)
            kit.part(m, "RackUpright", (1.0, 16, 1.0), (rx, BASE_Y + 8, rz),
                     yaw=yaw, mat="DiamondPlate", color=(104, 96, 78))
            for lvl in range(3):
                kit.part(m, "RackShelf", (8.0, 0.4, 14.0),
                         (rx, BASE_Y + 2 + lvl * 5, rz), yaw=yaw,
                         mat="DiamondPlate", color=(96, 90, 74))
                if rng.random() < 0.7:
                    kit.crate(m, (rx, BASE_Y + 2.4 + lvl * 5, rz), yaw=yaw + rng.uniform(-10, 10),
                              size=2.6)
            if rng.random() < 0.5:
                world.anchor(m, "Loot", (rx, BASE_Y + 3.0, rz),
                             table=rng.choice(["industrial", "tools"]), name="LootAnchor")
    for i in range(14):
        kit.crate_stack(m, (pos[0] + rng.uniform(-w / 2 + 8, w / 2 - 8),
                            BASE_Y, pos[2] + rng.uniform(-d / 2 + 8, d / 2 - 8)),
                        yaw=rng.uniform(0, 360), count=rng.randint(2, 5))
    for i in range(4):
        kit.barrel(m, (pos[0] + rng.uniform(-w / 2 + 6, w / 2 - 6), BASE_Y,
                       pos[2] + rng.uniform(-d / 2 + 6, d / 2 - 6)),
                   toxic=rng.random() < 0.25)
    kit.generator(m, (pos[0] + w / 2 - 12, BASE_Y, pos[2] - d / 2 + 14), yaw=yaw)
    world.anchor(m, "Workbench", (pos[0] + w / 2 - 12, BASE_Y + 2.4, pos[2] - d / 2 + 14),
                 name="WorkbenchAnchor")
    # catwalk
    kit.part(m, "Catwalk", (w - 10, 0.8, 6.0),
             (pos[0], BASE_Y + h - 8, pos[2] - d / 2 + 8), yaw=yaw,
             mat="DiamondPlate", color=(88, 84, 74))
    kit.railing(m, (pos[0] - w / 2 + 5, BASE_Y + h - 8, pos[2] - d / 2 + 5),
                (pos[0] + w / 2 - 5, BASE_Y + h - 8, pos[2] - d / 2 + 5))
    kit.stairs(m, (pos[0] + w / 2 - 16, BASE_Y, pos[2] - d / 2 + 12), yaw + 180,
               width=5, steps=10, rise=2.6, run=1.8)
    # lighting
    for i in range(3):
        kit.lights(m, "WarehouseLight",
                   (pos[0] + rng.uniform(-w * 0.3, w * 0.3), BASE_Y + h - 3,
                    pos[2] + rng.uniform(-d * 0.3, d * 0.3)),
                   (250, 236, 200), 1.1 if rng.random() < 0.6 else 0.0, 48)
    for i in range(3):
        world.enemy_anchor(m, (pos[0] + rng.uniform(-w / 3, w / 3), BASE_Y,
                               pos[2] + rng.uniform(-d / 3, d / 3)), 2, "Industrial")
    world.anchor(m, "Loot", (pos[0], BASE_Y + 1.4, pos[2] + d / 2 - 12), table="industrial",
                 name="LootAnchor")
    return m


def build_factory(world, parent, pos, yaw, rng):
    kit = world.kit
    w, d, h = 130.0, 100.0, 46.0
    m = kit.model(parent, "Factory")
    kit.floor(m, pos, w, d, yaw=yaw, mat="DiamondPlate", color=(92, 90, 84), thickness=1.6)
    kit.wall_open(m, offset(pos, yaw, forward=-d / 2), yaw, w, h, 1.4, "Concrete",
                  (96, 92, 84), [(30, 30, 0, 30)])
    kit.wall_open(m, offset(pos, yaw, forward=d / 2), yaw + 180, w, h, 1.4, "Concrete",
                  (96, 92, 84), [(24, 20, 20, 34)])
    kit.wall_open(m, offset(pos, yaw, right=w / 2), yaw + 90, d, h, 1.4, "Concrete",
                  (96, 92, 84), [(d / 2 - 12, 14, 16, 38)])
    kit.wall_open(m, offset(pos, yaw, right=-w / 2), yaw - 90, d, h, 1.4, "Concrete",
                  (96, 92, 84), [(20, 16, 24, 40)])
    kit.roof_flat(m, (pos[0], BASE_Y + h, pos[2]), w, d, yaw=yaw, mat="CorrodedMetal",
                  color=(70, 68, 62), parapet=True)
    # machinery clusters
    for i in range(5):
        mx = pos[0] + math.cos(math.radians(yaw)) * (-w / 2 + 20 + i * 24) - math.sin(math.radians(yaw)) * rng.uniform(-30, 30)
        mz = pos[2] - math.sin(math.radians(yaw)) * (-w / 2 + 20 + i * 24) - math.cos(math.radians(yaw)) * rng.uniform(-30, 30)
        kit.part(m, "MachineBase", (14, 8, 14), (mx, BASE_Y + 4, mz), yaw=yaw,
                 mat="CorrodedMetal", color=(84, 80, 70))
        kit.part(m, "MachinePiston", (5, 12, 5), (mx, BASE_Y + 12, mz), yaw=yaw,
                 mat="Metal", color=(110, 108, 98), shape="cylinder")
        kit.part(m, "MachineHousing", (10, 6, 10), (mx, BASE_Y + 17, mz), yaw=yaw,
                 mat="DiamondPlate", color=(78, 76, 66))
        for a in (-1, 1):
            kit.pipe(m, (mx + a * 8, BASE_Y + 6, mz), (mx + a * 8, BASE_Y + h - 6, mz),
                     radius=1.1 * 0.5, color=(96, 72, 52))
        if rng.random() < 0.6:
            world.anchor(m, "Loot", (mx, BASE_Y + 1.6, mz + 9), table="industrial",
                         name="LootAnchor")
    # catwalks and gantries
    for lvl in (14, 28):
        kit.part(m, "Gantry", (w - 12, 0.9, 8), (pos[0], BASE_Y + lvl, pos[2] - d / 2 + 14),
                 yaw=yaw, mat="DiamondPlate", color=(84, 82, 72))
        kit.part(m, "Gantry2", (w - 12, 0.9, 8), (pos[0], BASE_Y + lvl, pos[2] + d / 2 - 14),
                 yaw=yaw, mat="DiamondPlate", color=(84, 82, 72))
    kit.stairs(m, (pos[0] - w / 2 + 10, BASE_Y, pos[2] - d / 2 + 22), yaw + 180,
               width=6, steps=14, rise=2.0, run=1.6)
    # conveyor
    kit.part(m, "Conveyor", (w - 30, 1.2, 6), (pos[0], BASE_Y + 6, pos[2] + 10), yaw=yaw,
             mat="DiamondPlate", color=(92, 90, 80))
    for i in range(6):
        kit.part(m, "ConveyorRoller", (2.0, 2.0, 6.2),
                 (pos[0] - (w - 36) / 2 + i * ((w - 36) / 5), BASE_Y + 6.6, pos[2] + 10),
                 yaw=yaw, mat="Metal", color=(116, 114, 104), shape="cylinder",
                 collidable=False)
    # toxic vats
    for i in range(3):
        vx = pos[0] + 40 + i * 14
        vz = pos[2] - 30
        kit.part(m, "Vat", (10, 18, 10), (vx, BASE_Y + 9, vz), mat="CorrodedMetal",
                 color=(88, 94, 70), shape="cylinder")
        kit.part(m, "VatGlow", (8.6, 0.6, 8.6), (vx, BASE_Y + 17.5, vz),
                 mat="Plastic", color=(120, 156, 60), shape="cylinder",
                 collidable=False, shadow=False)
        kit.lights(m, "VatLight", (vx, BASE_Y + 20, vz), (140, 210, 90), 0.9, 22)
        world.anchor(m, "Loot", (vx - 7, BASE_Y + 1.6, vz), table="chemical",
                     name="LootAnchor")
    for i in range(4):
        kit.lights(m, "FactoryLight",
                   (pos[0] + rng.uniform(-w / 2.4, w / 2.4), BASE_Y + h - 4,
                    pos[2] + rng.uniform(-d / 2.4, d / 2.4)),
                   (240, 232, 208), 1.3 if rng.random() < 0.5 else 0.0, 60)
    for i in range(3):
        world.enemy_anchor(m, (pos[0] + rng.uniform(-w / 3, w / 3), BASE_Y,
                               pos[2] + rng.uniform(-d / 3, d / 3)), 3, "Industrial")
    return m


def build_industrial(world):
    kit = world.kit
    rng = world.rng
    reg, info = world.region("Industrial")
    c = info["center"]
    build_warehouse(world, reg, (c[0] - 160, BASE_Y, c[1] + 120), 0, rng)
    build_warehouse(world, reg, (c[0] + 150, BASE_Y, c[1] + 90), 90, rng, size=(96, 120, 30))
    build_factory(world, reg, (c[0], BASE_Y, c[1] - 170), 0, rng)

    # cooling towers
    for i in range(2):
        cx = c[0] - 40 + i * 90
        cz = c[1] + 300
        kit.part(reg, "CoolingBase", (54, 8, 54), (cx, BASE_Y + 4, cz),
                 mat="Concrete", color=(112, 108, 100), shape="cylinder")
        kit.part(reg, "CoolingTower", (44, 90, 44), (cx, BASE_Y + 49, cz),
                 mat="Concrete", color=(116, 112, 104), shape="cylinder")
        kit.part(reg, "CoolingTop", (48, 6, 48), (cx, BASE_Y + 96, cz),
                 mat="Concrete", color=(120, 116, 108), shape="cylinder")
        kit.part(reg, "SteamColumn", (30, 40, 30), (cx, BASE_Y + 118, cz),
                 mat="Plastic", color=(150, 150, 148), transparency=0.82,
                 shape="ball", collidable=False, shadow=False)
        kit.lights(reg, "TowerGlow", (cx, BASE_Y + 30, cz), (180, 190, 170), 0.4, 40)
        world.enemy_anchor(reg, (cx + 40, BASE_Y, cz), 2, "Industrial")

    # storage tanks + pipe runs
    for i in range(5):
        tx = c[0] + 240 + (i % 2) * 48
        tz = c[1] - 60 + i * 46
        kit.part(reg, "StorageTank", (34, 30, 34), (tx, BASE_Y + 15, tz),
                 mat="CorrodedMetal", color=(104, 96, 78), shape="cylinder")
        kit.part(reg, "TankCap", (35, 2, 35), (tx, BASE_Y + 31, tz),
                 mat="CorrodedMetal", color=(90, 84, 70), shape="cylinder",
                 collidable=False)
        kit.pipe(reg, (tx - 20, BASE_Y + 6, tz), (c[0] + 120, BASE_Y + 6, tz - 40), radius=1.4)
        if i % 2 == 0:
            world.anchor(reg, "Loot", (tx, BASE_Y + 1.6, tz + 20), table="chemical",
                         name="LootAnchor")
    # chimney stacks
    for i in range(3):
        kit.part(reg, "Chimney", (14, 120, 14), (c[0] - 260 + i * 34, BASE_Y + 60, c[1] - 300),
                 mat="Brick", color=(88, 62, 52), shape="cylinder")

    # yard dressing
    for i in range(26):
        ang = rng.uniform(0, math.tau)
        rad = rng.uniform(60, 420)
        pos = (c[0] + math.cos(ang) * rad, BASE_Y, c[1] + math.sin(ang) * rad)
        roll = rng.random()
        if roll < 0.34:
            kit.crate_stack(reg, pos, yaw=rng.uniform(0, 360), count=rng.randint(2, 5))
        elif roll < 0.6:
            kit.barrel(reg, pos, toxic=rng.random() < 0.3)
        elif roll < 0.8:
            kit.rock(reg, pos, scale=rng.uniform(0.6, 1.1), mat="Slate", color=(96, 92, 86))
        else:
            kit.car_wreck(reg, pos, yaw=rng.uniform(0, 360), burnt=rng.random() < 0.4)
    for i in range(8):
        kit.streetlamp(reg, (c[0] + rng.uniform(-380, 380), BASE_Y, c[1] + rng.uniform(-380, 380)),
                       yaw=rng.uniform(0, 360), on=rng.random() < 0.3)
    for i in range(30):
        world.enemy_anchor(reg, (c[0] + rng.uniform(-400, 400), BASE_Y + 2,
                               c[1] + rng.uniform(-400, 400)), 3, "Industrial")
    return reg


# ---------------------------------------------------------------------- city

def build_apartment(world, parent, pos, yaw, rng, floors=5, w=54.0, d=40.0):
    kit = world.kit
    m = kit.model(parent, "Apartment")
    fh = 14.0
    kit.floor(m, pos, w, d, yaw=yaw, mat="Concrete", color=(94, 92, 88), thickness=1.2)
    for f in range(floors):
        y = BASE_Y + f * fh
        if f > 0:
            kit.floor(m, (pos[0], y, pos[2]), w, d, yaw=yaw, mat="Concrete",
                      color=(88, 86, 82), thickness=1.0)
        openings_long = [(4 + i * 12, 5, 4.5, 10.0) for i in range(int(w / 12) - 1)]
        openings_short = [(4 + i * 11, 5, 4.5, 10.0) for i in range(int(d / 11) - 1)]
        kit.wall_open(m, offset((pos[0], y, pos[2]), yaw, forward=-d / 2), yaw, w, fh, 1.0,
                      "Concrete", (110, 106, 98), openings_long + [(w / 2 - 6, 7, 0, 9)])
        kit.wall_open(m, offset((pos[0], y, pos[2]), yaw, forward=d / 2), yaw + 180, w, fh, 1.0,
                      "Concrete", (110, 106, 98), openings_long)
        kit.wall_open(m, offset((pos[0], y, pos[2]), yaw, right=w / 2), yaw + 90, d, fh, 1.0,
                      "Concrete", (110, 106, 98), openings_short)
        kit.wall_open(m, offset((pos[0], y, pos[2]), yaw, right=-w / 2), yaw - 90, d, fh, 1.0,
                      "Concrete", (110, 106, 98), openings_short)
        # interior corridor wall + doors
        kit.wall_open(m, offset((pos[0], y, pos[2]), yaw, forward=3), yaw, w - 3, fh, 0.6,
                      "Plaster", (120, 116, 108), [(8 + i * 11, 4, 0, 8.5) for i in range(4)])
        # units
        for i in range(3):
            ux = -w / 2 + 9 + i * 17
            ux_world = offset((pos[0], y, pos[2]), yaw, right=ux, forward=-8)
            roll = rng.random()
            if roll < 0.3:
                kit.bed(m, (ux_world[0], y, ux_world[2]), yaw)
                kit.shelf(m, offset(ux_world, yaw, right=6), yaw - 90, w=5, h=6)
            elif roll < 0.6:
                kit.sofa(m, (ux_world[0], y, ux_world[2]), yaw + 180)
                kit.table(m, offset(ux_world, yaw, forward=4), yaw, w=4, d=2.5)
            elif roll < 0.8:
                kit.counter(m, (ux_world[0], y, ux_world[2]), yaw, 7.0)
                kit.fridge(m, offset(ux_world, yaw, right=5), yaw)
            else:
                kit.rubble(m, (ux_world[0], y, ux_world[2]), radius=4.0, count=6)
            if rng.random() < 0.7:
                world.anchor(m, "Loot", (ux_world[0], y + 1.4, ux_world[2]),
                             table=rng.choice(["residential", "medical", "tools"]),
                             name="LootAnchor")
            if rng.random() < 0.25:
                world.enemy_anchor(m, (ux_world[0], y, ux_world[2]), 2, "City")
        # balcony
        kit.part(m, "Balcony", (w * 0.55, 0.9, 7.0),
                 offset((pos[0], y + 0.4, pos[2]), yaw, forward=-d / 2 - 3.2), yaw=yaw,
                 mat="Concrete", color=(100, 96, 90))
        kit.railing(m, offset((pos[0], y, pos[2]), yaw, right=-w * 0.27, forward=-d / 2 - 6),
                    offset((pos[0], y, pos[2]), yaw, right=w * 0.27, forward=-d / 2 - 6),
                    height=3.2)
        if rng.random() < 0.4:
            kit.lights(m, "HallLight", offset((pos[0], y, pos[2]), yaw, forward=3, up=11),
                       (250, 226, 180), 0.5, 22)
    kit.roof_flat(m, (pos[0], BASE_Y + floors * fh, pos[2]), w, d, yaw=yaw,
                  mat="Concrete", color=(84, 82, 78), height=1.0)
    # stair core
    for f in range(floors - 1):
        kit.stairs(m, offset((pos[0], BASE_Y + f * fh, pos[2]), yaw, right=w / 2 - 8, forward=0),
                   yaw + 90, width=5.0, steps=9, rise=fh / 9, run=1.5, mat="Concrete",
                   color=(96, 94, 90))
    # roof access + antenna
    kit.part(m, "RoofHatch", (6, 3, 6), (pos[0] + 10, BASE_Y + floors * fh + 2.5, pos[2]),
             mat="Metal", color=(78, 76, 70))
    kit.part(m, "Antenna", (0.6, 34, 0.6), (pos[0] - 8, BASE_Y + floors * fh + 18, pos[2] - 6),
             mat="Metal", color=(90, 88, 82))
    for i in range(4):
        world.anchor(m, "Loot", (pos[0] + rng.uniform(-w / 2.4, w / 2.4),
                                 BASE_Y + rng.randint(1, floors) * fh + 1.4,
                                 pos[2] + rng.uniform(-d / 2.4, d / 2.4)),
                     table="residential", name="LootAnchor")
    return m


def build_hospital(world, parent, pos, yaw, rng):
    kit = world.kit
    m = kit.model(parent, "Hospital")
    w, d, fh, floors = 110.0, 84.0, 16.0, 3
    kit.floor(m, pos, w, d, yaw=yaw, mat="CeramicTiles", color=(126, 128, 122), thickness=1.4)
    for f in range(floors):
        y = BASE_Y + f * fh
        if f > 0:
            kit.floor(m, (pos[0], y, pos[2]), w, d, yaw=yaw, mat="CeramicTiles",
                      color=(120, 122, 116), thickness=1.0)
        kit.wall_open(m, offset((pos[0], y, pos[2]), yaw, forward=-d / 2), yaw, w, fh, 1.0,
                      "Concrete", (132, 130, 122),
                      [(8 + i * 9, 5, 5, 11) for i in range(11)] + [(w / 2 - 10, 12, 0, 10)])
        kit.wall_open(m, offset((pos[0], y, pos[2]), yaw, forward=d / 2), yaw + 180, w, fh, 1.0,
                      "Concrete", (132, 130, 122), [(8 + i * 9, 5, 5, 11) for i in range(11)])
        kit.wall_open(m, offset((pos[0], y, pos[2]), yaw, right=w / 2), yaw + 90, d, fh, 1.0,
                      "Concrete", (132, 130, 122), [(10 + i * 9, 5, 5, 11) for i in range(7)])
        kit.wall_open(m, offset((pos[0], y, pos[2]), yaw, right=-w / 2), yaw - 90, d, fh, 1.0,
                      "Concrete", (132, 130, 122), [(10 + i * 9, 5, 5, 11) for i in range(7)])
        # central corridor walls
        for s in (-1, 1):
            kit.wall_open(m, offset((pos[0], y, pos[2]), yaw, right=s * 7), yaw, w - 8, fh, 0.6,
                          "CeramicTiles", (142, 140, 132),
                          [(10 + i * 12, 4.5, 0, 9) for i in range(7)])
        # patient rooms
        for side in (-1, 1):
            for i in range(4):
                rx = -w / 2 + 14 + i * 24
                rp = offset((pos[0], y, pos[2]), yaw, right=rx, forward=side * 22)
                kit.medical_bed(m, (rp[0], y, rp[2]), yaw + (0 if side > 0 else 180))
                kit.cabinet(m, offset((rp[0], y, rp[2]), yaw, right=8), yaw, w=3, h=5, d=2)
                if rng.random() < 0.4:
                    kit.hospital_curtain(m, offset((rp[0], y, rp[2]), yaw, right=-6), yaw)
                if rng.random() < 0.75:
                    world.anchor(m, "Loot", (rp[0], y + 1.4, rp[2]),
                                 table="medical", name="LootAnchor")
                if rng.random() < 0.2:
                    kit.rubble(m, (rp[0], y, rp[2]), radius=5, count=8)
        for i in range(2):
            kit.lights(m, "HospitalLight",
                       offset((pos[0], y, pos[2]), yaw, right=-w / 4 + i * w / 2, up=fh - 3),
                       (218, 232, 228), 0.9 if rng.random() < 0.55 else 0.0, 34,
                       shadows=False)
    kit.roof_flat(m, (pos[0], BASE_Y + floors * fh, pos[2]), w, d, yaw=yaw,
                  mat="Concrete", color=(96, 94, 90))
    # lobby / reception
    kit.counter(m, offset((pos[0], BASE_Y, pos[2]), yaw, forward=-d / 2 + 10, right=-10), yaw, 18.0)
    kit.sofa(m, offset((pos[0], BASE_Y, pos[2]), yaw, forward=-d / 2 + 18, right=14), yaw + 180)
    kit.counter(m, offset((pos[0], BASE_Y, pos[2]), yaw, right=w / 2 - 12, forward=d / 2 - 16),
                yaw + 90, 16.0)
    # emergency lighting + gurney
    kit.lights(m, "ERLight", offset((pos[0], BASE_Y, pos[2]), yaw, forward=-d / 2 + 8),
               (170, 210, 255), 1.0, 30)
    for i in range(4):
        world.anchor(m, "Loot", (pos[0] + rng.uniform(-w / 2.6, w / 2.6), BASE_Y + 1.4,
                                 pos[2] + rng.uniform(-d / 2.6, d / 2.6)),
                     table="medical", name="LootAnchor")
    for i in range(3):
        world.enemy_anchor(m, (pos[0] + rng.uniform(-w / 3, w / 3), BASE_Y,
                               pos[2] + rng.uniform(-d / 3, d / 3)), 3, "City")
    world.anchor(m, "Document", offset((pos[0], BASE_Y, pos[2]), yaw, forward=-d / 2 + 10,
                                       right=-10, up=3.6),
                 extra={"DocId": "doc_hospital_log"}, name="DocumentAnchor")
    world.anchor(m, "QuestItem", offset((pos[0], BASE_Y, pos[2]), yaw, right=w / 2 - 12,
                                        forward=d / 2 - 16, up=3.4),
                 extra={"ItemId": "ampoule_mil"}, name="QuestItemAnchor")
    return m


def build_police(world, parent, pos, yaw, rng):
    kit = world.kit
    m = kit.model(parent, "PoliceStation")
    w, d, h = 70.0, 56.0, 18.0
    kit.floor(m, pos, w, d, yaw=yaw, mat="Concrete", color=(96, 96, 92))
    kit.wall_open(m, offset(pos, yaw, forward=-d / 2), yaw, w, h, 1.0, "Concrete",
                  (108, 104, 98), [(w / 2 - 8, 12, 0, 12), (10, 6, 6, 12)])
    kit.wall_open(m, offset(pos, yaw, forward=d / 2), yaw + 180, w, h, 1.0, "Concrete",
                  (108, 104, 98), [(10, 6, 6, 12)])
    kit.wall_open(m, offset(pos, yaw, right=w / 2), yaw + 90, d, h, 1.0, "Concrete",
                  (108, 104, 98), [])
    kit.wall_open(m, offset(pos, yaw, right=-w / 2), yaw - 90, d, h, 1.0, "Concrete",
                  (108, 104, 98), [(14, 6, 6, 12)])
    kit.wall_open(m, offset(pos, yaw, forward=4), yaw + 90, d - 6, h, 0.6, "Plaster",
                  (116, 112, 104), [(10, 4, 0, 9), (24, 4, 0, 9)])
    kit.floor(m, (pos[0], BASE_Y + h, pos[2]), w, d, yaw=yaw, mat="Concrete",
              color=(88, 86, 82), thickness=1.0)
    # holding cells with bars
    for i in range(2):
        cx = pos[0] - 18 + i * 18
        cz = pos[2] + 14
        for b in range(7):
            kit.part(m, "CellBar", (0.35, 14, 0.35), (cx - 4.5 + b * 1.5, BASE_Y + 7, cz + 8),
                     mat="Metal", color=(84, 84, 80), collidable=False)
        kit.bed(m, (cx, BASE_Y, cz + 3), 0, color=(70, 70, 64))
        world.anchor(m, "Loot", (cx, BASE_Y + 1.4, cz), table="police", name="LootAnchor")
    kit.desk(m, offset(pos, yaw, forward=-12, right=-8), yaw, 10.0)
    kit.locker(m, offset(pos, yaw, right=w / 2 - 3, forward=-10), yaw - 90, color=(56, 72, 92))
    kit.locker(m, offset(pos, yaw, right=w / 2 - 3, forward=-6), yaw - 90, color=(56, 72, 92))
    # armory
    kit.part(m, "ArmoryCabinet", (10, 12, 3),
             offset(pos, yaw, right=-w / 2 + 8, forward=6), yaw=yaw,
             mat="DiamondPlate", color=(74, 78, 68))
    world.anchor(m, "Loot", offset(pos, yaw, right=-w / 2 + 8, forward=8), table="police",
                 name="LootAnchor")
    world.anchor(m, "Loot", offset(pos, yaw, right=-w / 2 + 8, forward=10), table="weapons",
                 name="LootAnchor")
    kit.lights(m, "PoliceLight", offset(pos, yaw, up=h - 3), (240, 238, 220), 0.9, 34)
    world.anchor(m, "Document", offset(pos, yaw, forward=-12, right=-8, up=3.4),
                 extra={"DocId": "doc_police_report"}, name="DocumentAnchor")
    world.enemy_anchor(m, offset(pos, yaw, forward=-d / 2 - 10), 3, "City")
    return m


def build_supermarket(world, parent, pos, yaw, rng):
    kit = world.kit
    m = kit.model(parent, "Supermarket")
    w, d, h = 96.0, 70.0, 20.0
    kit.floor(m, pos, w, d, yaw=yaw, mat="CeramicTiles", color=(124, 124, 118))
    kit.wall_open(m, offset(pos, yaw, forward=-d / 2), yaw, w, h, 1.0, "Concrete",
                  (118, 114, 106), [(w / 2 - 16, 24, 0, 14)])
    kit.wall_open(m, offset(pos, yaw, forward=d / 2), yaw + 180, w, h, 1.0, "Concrete",
                  (118, 114, 106), [])
    kit.wall_open(m, offset(pos, yaw, right=w / 2), yaw + 90, d, h, 1.0, "Concrete",
                  (118, 114, 106), [(16, 10, 8, 14)])
    kit.wall_open(m, offset(pos, yaw, right=-w / 2), yaw - 90, d, h, 1.0, "Concrete",
                  (118, 114, 106), [])
    kit.floor(m, (pos[0], BASE_Y + h, pos[2]), w, d, yaw=yaw, mat="Concrete",
              color=(96, 94, 90), thickness=0.8)
    for row in range(4):
        for col in range(3):
            sx = pos[0] - 30 + row * 20
            sz = pos[2] - 20 + col * 16
            kit.shelf(m, (sx, BASE_Y, sz), 0, w=16, h=9, d=3.2)
            if rng.random() < 0.8:
                world.anchor(m, "Loot", (sx, BASE_Y + 2.2, sz + 1.6), table="food",
                             name="LootAnchor")
    for i in range(4):
        kit.part(m, "Checkout", (7, 3.4, 3), (pos[0] - 24 + i * 16, BASE_Y + 1.7, pos[2] + 24),
                 mat="Metal", color=(112, 108, 96))
        world.anchor(m, "Loot", (pos[0] - 24 + i * 16, BASE_Y + 1.8, pos[2] + 24),
                     table="food", name="LootAnchor")
    for i in range(6):
        kit.lights(m, "StoreLight",
                   (pos[0] - 30 + i * 12, BASE_Y + h - 4, pos[2]),
                   (236, 240, 232), 0.8 if rng.random() < 0.5 else 0.0, 34)
    kit.crate_stack(m, (pos[0] + 34, BASE_Y, pos[2] - 24), count=4)
    for i in range(2):
        world.enemy_anchor(m, (pos[0] + rng.uniform(-40, 40), BASE_Y,
                               pos[2] + rng.uniform(-30, 30)), 2, "City")
    return m


def build_parking(world, parent, pos, yaw, rng):
    kit = world.kit
    m = kit.model(parent, "ParkingGarage")
    w, d = 100.0, 80.0
    for lvl in range(2):
        y = BASE_Y + lvl * 14
        kit.floor(m, (pos[0], y, pos[2]), w, d, yaw=yaw, mat="Concrete",
                  color=(86, 84, 80), thickness=1.2)
        for s in (-1, 1):
            kit.part(m, "GarageWall", (w, 5, 1.0),
                     offset((pos[0], y + 2.5, pos[2]), yaw, forward=s * (d / 2 - 0.5)),
                     yaw=yaw, mat="Concrete", color=(80, 78, 74))
            kit.part(m, "GarageWall", (1.0, 5, d),
                     offset((pos[0], y + 2.5, pos[2]), yaw, right=s * (w / 2 - 0.5)),
                     yaw=yaw, mat="Concrete", color=(80, 78, 74))
        for i in range(4):
            for s in (-1, 1):
                kit.part(m, "Column", (4, 13, 4),
                         offset((pos[0], y, pos[2]), yaw, right=-w / 2 + 12 + i * 25,
                                forward=s * (d / 2 - 12)), yaw=yaw, mat="Concrete",
                         color=(92, 90, 86))
        for i in range(3):
            kit.car_wreck(m, offset((pos[0], y, pos[2]), yaw,
                                    right=rng.uniform(-w / 3, w / 3),
                                    forward=rng.uniform(-d / 3, d / 3)),
                          yaw=rng.uniform(0, 360), burnt=rng.random() < 0.5)
        for i in range(2):
            world.anchor(m, "Loot", offset((pos[0], y, pos[2]), yaw,
                                           right=rng.uniform(-w / 3, w / 3),
                                           forward=rng.uniform(-d / 3, d / 3)),
                         table="tools", name="LootAnchor")
        kit.lights(m, "GarageLight", offset((pos[0], y + 12, pos[2]), yaw),
                   (200, 220, 210), 0.7 if rng.random() < 0.4 else 0.0, 30)
        world.enemy_anchor(m, offset((pos[0], y, pos[2]), yaw), 2, "City")
    kit.stairs(m, offset((pos[0], BASE_Y, pos[2]), yaw, right=w / 2 - 12), yaw + 90,
               width=6, steps=8, rise=1.75, run=1.8)
    return m


def build_subway(world, parent, pos, yaw, rng):
    kit = world.kit
    m = kit.model(parent, "Subway")
    # entrance kiosk
    kit.part(m, "SubwayCanopy", (26, 1.2, 18), (pos[0], BASE_Y + 12, pos[2]),
             mat="Concrete", color=(96, 92, 86))
    for s in (-1, 1):
        kit.part(m, "SubwayColumn", (1.6, 12, 1.6),
                 (pos[0] + s * 11, BASE_Y + 6, pos[2] - 7), mat="Concrete",
                 color=(104, 100, 94))
    kit.part(m, "SubwaySign", (12, 4, 0.6), (pos[0], BASE_Y + 14, pos[2] - 8),
             mat="Metal", color=(46, 66, 58), collidable=False)
    # stairs down
    kit.stairs(m, (pos[0], BASE_Y, pos[2] + 11), yaw, width=14, steps=12, rise=1.9, run=2.0,
               mat="CeramicTiles", color=(110, 110, 104))
    ty = BASE_Y - 23
    for i in range(9):
        z = pos[2] + 22 + i * 30
        for s in (-1, 1):
            kit.part(m, "TunnelWall", (3, 22, 30), (pos[0] + s * 18, ty + 10, z),
                     mat="CeramicTiles", color=(96, 98, 92), shadow=False)
            kit.part(m, "TunnelTiles", (0.4, 8, 30), (pos[0] + s * 16.5, ty + 6, z),
                     mat="CeramicTiles", color=(120, 124, 112), shadow=False)
        kit.part(m, "TunnelCeiling", (40, 3, 30), (pos[0], ty + 22, z),
                 mat="Concrete", color=(74, 72, 68), shadow=False)
        kit.part(m, "TunnelFloor", (36, 2, 30), (pos[0], ty - 1, z),
                 mat="Concrete", color=(78, 76, 72), shadow=False)
        if i % 2 == 0:
            kit.part(m, "Rail", (0.8, 0.8, 30), (pos[0] + 9, ty + 0.5, z),
                     mat="Metal", color=(96, 92, 84), collidable=False)
            kit.lights(m, "TunnelLight", (pos[0], ty + 18, z), (208, 214, 200),
                       0.55 if rng.random() < 0.7 else 0.0, 26)
        if i % 3 == 1:
            kit.car_wreck(m, (pos[0] - 6, ty, z), yaw=rng.uniform(0, 360), burnt=True)
        if i % 3 == 2:
            world.anchor(m, "Loot", (pos[0] + rng.uniform(-10, 10), ty + 1.4, z),
                         table="subway", name="LootAnchor")
        if i >= 4 and i % 3 == 0:
            world.enemy_anchor(m, (pos[0], ty, z), 4, "City")
    world.anchor(m, "Workbench", (pos[0] + 12, ty + 1.4, pos[2] + 24), name="WorkbenchAnchor")
    return m


def build_city(world):
    kit = world.kit
    rng = world.rng
    reg, info = world.region("City")
    c = info["center"]
    # street grid
    for i in range(-2, 3):
        kit.part(reg, "CityStreet", (900, 1.0, 50), (c[0], BASE_Y + 0.3, c[1] + i * 180),
                 mat="Asphalt", color=(52, 52, 54), shadow=False)
        kit.part(reg, "CityStreet", (50, 1.0, 900), (c[0] + i * 180, BASE_Y + 0.32, c[1]),
                 mat="Asphalt", color=(52, 52, 54), shadow=False)
    # blocks
    lots = [(c[0] - 270, c[1] - 270), (c[0] + 30, c[1] - 270), (c[0] - 270, c[1] + 30),
            (c[0] + 30, c[1] + 30), (c[0] - 270, c[1] + 330), (c[0] + 30, c[1] + 330)]
    build_apartment(world, reg, (lots[0][0], BASE_Y, lots[0][1]), 0, rng, floors=6)
    build_apartment(world, reg, (lots[1][0], BASE_Y, lots[1][1]), 90, rng, floors=4,
                    w=46, d=38)
    build_apartment(world, reg, (lots[4][0], BASE_Y, lots[4][1]), 180, rng, floors=5,
                    w=50, d=42)
    build_hospital(world, reg, (lots[2][0] + 40, BASE_Y, lots[2][1] - 10), 0, rng)
    build_police(world, reg, (lots[3][0] + 50, BASE_Y, lots[3][1] - 60), 0, rng)
    build_supermarket(world, reg, (lots[5][0] + 60, BASE_Y, lots[5][1] + 40), 90, rng)
    build_parking(world, reg, (c[0] + 290, BASE_Y, c[1] + 300), 0, rng)
    build_subway(world, reg, (c[0] - 60, BASE_Y, c[1] - 300), 0, rng)

    # street dressing
    for i in range(90):
        ang = rng.uniform(0, math.tau)
        rad = rng.uniform(40, 430)
        pos = (c[0] + math.cos(ang) * rad, BASE_Y, c[1] + math.sin(ang) * rad)
        roll = rng.random()
        if roll < 0.4:
            kit.car_wreck(reg, pos, yaw=rng.uniform(0, 360), burnt=rng.random() < 0.4,
                          flipped=rng.random() < 0.1)
        elif roll < 0.6:
            kit.rubble(reg, pos, radius=rng.uniform(4, 14), count=rng.randint(8, 22))
        elif roll < 0.75:
            kit.streetlamp(reg, pos, yaw=rng.uniform(0, 360), on=rng.random() < 0.25)
        elif roll < 0.88:
            kit.bus_wreck(reg, pos, yaw=rng.uniform(0, 360))
        else:
            kit.barrel(reg, pos, toxic=rng.random() < 0.2)
    for i in range(40):
        world.enemy_anchor(reg, (c[0] + rng.uniform(-430, 430), BASE_Y + 2,
                               c[1] + rng.uniform(-430, 430)), 3, "City")
    # rooftop radio relay (main quest landmark)
    relay = (c[0] + 120, BASE_Y, c[1] + 120)
    m = kit.model(reg, "RadioRelay")
    kit.floor(m, relay, 30, 30, mat="Concrete", color=(88, 86, 82))
    for s in (-1, 1):
        for t in (-1, 1):
            kit.part(m, "RelayLeg", (1.6, 70, 1.6),
                     (relay[0] + s * 10, BASE_Y + 35, relay[2] + t * 10),
                     mat="Metal", color=(96, 94, 88))
    kit.part(m, "RelayPlatform", (26, 1.2, 26), (relay[0], BASE_Y + 70, relay[2]),
             mat="DiamondPlate", color=(90, 88, 80))
    kit.part(m, "Dish", (16, 1.0, 16), (relay[0], BASE_Y + 78, relay[2]),
             mat="Metal", color=(150, 150, 142), shape="ball", pitch=35)
    kit.part(m, "RelayMast", (1.2, 30, 1.2), (relay[0], BASE_Y + 88, relay[2]),
             mat="Metal", color=(120, 118, 110))
    kit.stairs(m, (relay[0] + 12, BASE_Y, relay[2] + 12), -90, width=5, steps=12,
               rise=5.8, run=1.6)
    world.anchor(m, "RadioTower", (relay[0], BASE_Y + 72, relay[2]), name="RadioTowerAnchor")
    world.anchor(m, "Loot", (relay[0] - 6, BASE_Y + 72, relay[2] + 6), table="electronics",
                 name="LootAnchor")
    world.enemy_anchor(m, (relay[0], BASE_Y, relay[2] + 20), 4, "City")
    return reg


# ------------------------------------------------------------------ military

def build_bunker(world, parent, pos, yaw, rng, lab=False):
    kit = world.kit
    m = kit.model(parent, "Bunker")
    depth = BASE_Y - 26
    w, d = 46.0, 40.0
    # surface entrance
    kit.part(m, "BunkerHousing", (30, 12, 24), (pos[0], BASE_Y + 6, pos[2]),
             mat="Concrete", color=(92, 92, 86))
    kit.part(m, "BunkerBlastDoor", (10, 10, 1.4),
             offset(pos, yaw, forward=-12.2, up=5.2), yaw=yaw,
             mat="DiamondPlate", color=(72, 76, 66))
    world.anchor(m, "Door", offset(pos, yaw, forward=-13, up=5.2), name="DoorAnchor")
    # ramp down
    for i in range(12):
        kit.part(m, "RampSegment", (12, 2.0, 5.0),
                 offset(pos, yaw, forward=4 + i * 4.6, up=-2 - i * 2.1),
                 yaw=yaw, pitch=-24, mat="Concrete", color=(78, 78, 74), shadow=False)
    # underground rooms
    for i in range(3):
        rz = pos[2] + 60 + i * 46
        kit.floor(m, (pos[0], depth - 1, rz), w, d, mat="Concrete", color=(70, 70, 68),
                  thickness=1.2)
        for (dx, dz, wy, length) in ((0, -d / 2, 0, w), (0, d / 2, 180, w),
                                     (-w / 2, 0, 90, d), (w / 2, 0, -90, d)):
            kit.wall_open(m, (pos[0] + dx, depth, rz + dz), wy, length, 14, 1.0,
                          "Concrete", (78, 78, 76),
                          [(length / 2 - 5, 6, 0, 9)] if dz == -d / 2 and i > 0 else [])
        kit.floor(m, (pos[0], depth + 13, rz), w, d, mat="Concrete", color=(64, 64, 62),
                  thickness=0.8)
        kit.lights(m, "BunkerLight", (pos[0], depth + 11, rz), (236, 244, 255), 0.8, 30)
        # furniture
        for k in range(3):
            kit.locker(m, (pos[0] - 16 + k * 4, depth, rz - 14), 0,
                       color=(62, 70, 58) if lab else (58, 68, 62))
        if lab:
            for k in range(3):
                kit.part(m, "LabBench", (18, 3, 6),
                         (pos[0] - 8 + k * 12, depth + 1.5, rz + 10),
                         mat="CeramicTiles", color=(112, 116, 108))
                kit.part(m, "LabScreen", (5, 3.4, 0.3),
                         (pos[0] - 8 + k * 12, depth + 4.6, rz + 7), mat="Glass",
                         color=(96, 168, 140), collidable=False, shadow=False)
                kit.lights(m, "LabGlow", (pos[0] - 8 + k * 12, depth + 5, rz + 8),
                           (140, 210, 180), 0.5, 14)
            world.anchor(m, "Document", (pos[0] + 10, depth + 1.4, rz + 10),
                         extra={"DocId": "doc_lab_terminal"}, name="DocumentAnchor")
            world.anchor(m, "QuestItem", (pos[0] - 8, depth + 4.0, rz + 10),
                         extra={"ItemId": "lab_keycard"}, name="QuestItemAnchor")
        for k in range(3):
            world.anchor(m, "Loot", (pos[0] + rng.uniform(-16, 16), depth + 1.4,
                                     rz + rng.uniform(-14, 14)),
                         table="military_high" if lab else "military", name="LootAnchor")
        for k in range(2):
            world.enemy_anchor(m, (pos[0] + rng.uniform(-14, 14), depth,
                                   rz + rng.uniform(-14, 14)), 5 if lab else 4, "Military")
    world.anchor(m, "Workbench", (pos[0] - 18, depth + 1.4, pos[2] + 60), name="WorkbenchAnchor")
    return m


def build_military(world):
    kit = world.kit
    rng = world.rng
    reg, info = world.region("Military")
    c = info["center"]
    # perimeter fence
    half = 420.0
    corners = [(c[0] - half, c[1] - half), (c[0] + half, c[1] - half),
               (c[0] + half, c[1] + half), (c[0] - half, c[1] + half)]
    for i in range(4):
        a, b = corners[i], corners[(i + 1) % 4]
        seg = 24
        for s in range(seg):
            p0 = (lerp(a[0], b[0], s / seg), BASE_Y, lerp(a[1], b[1], s / seg))
            p1 = (lerp(a[0], b[0], (s + 1) / seg), BASE_Y, lerp(a[1], b[1], (s + 1) / seg))
            kit.fence(reg, p0, p1, height=8.0, mat="Metal", color=(76, 78, 72),
                      spacing=12.0)
    # checkpoints
    for i in range(2):
        cp = (c[0] + (i * 2 - 1) * 120, BASE_Y, c[1] - half + 20)
        m = kit.model(reg, "Checkpoint")
        kit.part(m, "GuardBooth", (12, 10, 10), cp, mat="Concrete", color=(96, 96, 88))
        kit.part(m, "BoothWindow", (10, 3, 0.3),
                 (cp[0], cp[1] + 6, cp[2] - 5.2), mat="Glass", color=(80, 96, 96),
                 transparency=0.5, collidable=False, shadow=False)
        kit.sandbags(m, (cp[0] - 14, BASE_Y, cp[2] - 12), 0, 12.0)
        kit.sandbags(m, (cp[0] + 14, BASE_Y, cp[2] - 12), 0, 12.0)
        kit.traffic_barrier(m, (cp[0], BASE_Y, cp[2] - 24), 0)
        world.anchor(m, "Loot", (cp[0], BASE_Y + 1.4, cp[2]), table="military",
                     name="LootAnchor")
        world.anchor(m, "Loot", (cp[0] + 14, BASE_Y + 1.4, cp[2] - 10), table="weapons",
                     name="LootAnchor")
        world.enemy_anchor(m, (cp[0] + 20, BASE_Y, cp[2] - 20), 4, "Military")
    # watchtowers
    for i in range(4):
        ang = i * 90 + 45
        tw = (c[0] + math.cos(math.radians(ang)) * 330, BASE_Y,
              c[1] + math.sin(math.radians(ang)) * 330)
        kit.watchtower(reg, tw, yaw=ang + 180, height=38)
        world.anchor(reg, "Loot", (tw[0], BASE_Y + 38.8, tw[2]), table="military",
                     name="LootAnchor")
        world.enemy_anchor(reg, (tw[0], BASE_Y + 38.8, tw[2]), 4, "Military")
    # bunkers
    build_bunker(world, reg, (c[0] - 160, BASE_Y, c[1] - 40), 0, rng)
    build_bunker(world, reg, (c[0] + 120, BASE_Y, c[1] + 60), 90, rng, lab=True)
    build_bunker(world, reg, (c[0] + 30, BASE_Y, c[1] - 220), 0, rng)
    # motor pool
    pool = (c[0] + 40, BASE_Y, c[1] + 240)
    m = kit.model(reg, "MotorPool")
    kit.part(m, "PoolCanopy", (80, 1.0, 60), (pool[0], BASE_Y + 26, pool[2]),
             mat="CorrodedMetal", color=(74, 76, 68))
    for i in range(4):
        kit.part(m, "PoolColumn", (2.0, 26, 2.0),
                 (pool[0] - 34 + i * 22, BASE_Y + 13, pool[2] - 26), mat="Concrete",
                 color=(94, 94, 86))
        kit.part(m, "PoolColumn", (2.0, 26, 2.0),
                 (pool[0] - 34 + i * 22, BASE_Y + 13, pool[2] + 26), mat="Concrete",
                 color=(94, 94, 86))
    for i in range(4):
        kit.armored_vehicle(m, (pool[0] - 24 + i * 18, BASE_Y, pool[2] + rng.uniform(-14, 14)),
                            yaw=rng.uniform(0, 360))
    for i in range(4):
        world.anchor(m, "Loot", (pool[0] - 24 + i * 18, BASE_Y + 1.4, pool[2] + 18),
                     table="military_high" if i % 2 else "military", name="LootAnchor")
    for i in range(2):
        world.enemy_anchor(m, (pool[0] + rng.uniform(-30, 30), BASE_Y,
                               pool[2] + rng.uniform(-20, 20)), 5, "Military")
    # yard clutter
    for i in range(50):
        ang = rng.uniform(0, math.tau)
        rad = rng.uniform(30, 400)
        pos = (c[0] + math.cos(ang) * rad, BASE_Y, c[1] + math.sin(ang) * rad)
        roll = rng.random()
        if roll < 0.3:
            kit.crate_stack(reg, pos, count=rng.randint(2, 4))
        elif roll < 0.5:
            kit.traffic_barrier(reg, pos, yaw=rng.uniform(0, 360))
        elif roll < 0.7:
            kit.tent(reg, pos, yaw=rng.uniform(0, 360))
        elif roll < 0.85:
            kit.barrel(reg, pos)
        else:
            kit.rock(reg, pos, scale=1.0, mat="Slate", color=(96, 92, 84))
    for i in range(30):
        world.enemy_anchor(reg, (c[0] + rng.uniform(-400, 400), BASE_Y + 2,
                               c[1] + rng.uniform(-400, 400)), 5, "Military")
    return reg


# ---------------------------------------------------------------- wasteland

def build_wasteland(world):
    kit = world.kit
    rng = world.rng
    reg, info = world.region("Wasteland")
    c = info["center"]
    # craters
    for i in range(18):
        ang = rng.uniform(0, math.tau)
        rad = rng.uniform(40, 430)
        cp = (c[0] + math.cos(ang) * rad, BASE_Y, c[1] + math.sin(ang) * rad)
        r = rng.uniform(18, 60)
        kit.part(reg, "Crater", (r * 2, 6, r * 2), (cp[0], BASE_Y - 2.6, cp[2]),
                 mat="Basalt", color=(62, 52, 46), shape="cylinder", shadow=False)
        kit.part(reg, "CraterRim", (r * 2.2, 1.6, r * 2.2), (cp[0], BASE_Y + 0.4, cp[2]),
                 mat="Sand", color=(118, 92, 72), shape="cylinder", shadow=False)
        if rng.random() < 0.4:
            kit.part(reg, "GlowSeam", (r * 1.2, 0.3, r * 0.3),
                     (cp[0], BASE_Y - 0.1, cp[2]), yaw=rng.uniform(0, 360),
                     mat="CrackedLava", color=(126, 62, 26), collidable=False,
                     shadow=False)
            kit.lights(reg, "CraterGlow", (cp[0], BASE_Y + 3, cp[2]), (255, 120, 60),
                       0.5, 26)
    # dead forest
    for i in range(220):
        ang = rng.uniform(0, math.tau)
        rad = rng.uniform(20, 460)
        kit.tree(reg, (c[0] + math.cos(ang) * rad, BASE_Y, c[1] + math.sin(ang) * rad),
                 scale=rng.uniform(0.6, 1.2), dead=True)
    for i in range(80):
        ang = rng.uniform(0, math.tau)
        rad = rng.uniform(20, 460)
        kit.rock(reg, (c[0] + math.cos(ang) * rad, BASE_Y, c[1] + math.sin(ang) * rad),
                 scale=rng.uniform(0.7, 1.8), mat="Basalt", color=(66, 58, 52))
    # toxic lake
    lake = (c[0] + 240, BASE_Y, c[1] + 220)
    kit.part(reg, "ToxicLake", (280, 2, 220), (lake[0], BASE_Y - 3.2, lake[2]),
             mat="Glass", color=(88, 120, 46), transparency=0.35, collidable=False,
             shadow=False, reflectance=0.35)
    for i in range(10):
        kit.barrel(reg, (lake[0] + rng.uniform(-100, 100), BASE_Y,
                         lake[2] + rng.uniform(-70, 70)), toxic=True)
    kit.lights(reg, "LakeGlow", (lake[0], BASE_Y + 6, lake[2]), (150, 190, 80), 0.7, 70)
    # reactor ruin (endgame landmark)
    ruin = (c[0] - 260, BASE_Y, c[1] - 260)
    m = kit.model(world.landmarks, "ReactorRuin")
    kit.floor(m, ruin, 120, 120, mat="Basalt", color=(74, 64, 58))
    for i in range(8):
        ang = i * 45
        kit.part(m, "RuinPillar", (8, rng.uniform(24, 70), 8),
                 (ruin[0] + math.cos(math.radians(ang)) * 44, BASE_Y + 20,
                  ruin[2] + math.sin(math.radians(ang)) * 44),
                 yaw=ang, mat="Concrete", color=(88, 80, 70))
    kit.part(m, "ReactorCore", (34, 44, 34), (ruin[0], BASE_Y + 22, ruin[2]),
             mat="CorrodedMetal", color=(78, 74, 66), shape="cylinder")
    kit.part(m, "CoreGlow", (30, 8, 30), (ruin[0], BASE_Y + 46, ruin[2]),
             mat="CrackedLava", color=(150, 78, 30), shape="cylinder",
             collidable=False, shadow=False)
    kit.lights(m, "CoreLight", (ruin[0], BASE_Y + 30, ruin[2]), (255, 138, 62), 2.4, 90,
               shadows=True)
    kit.part(m, "CoreMast", (2.0, 90, 2.0), (ruin[0], BASE_Y + 90, ruin[2]),
             mat="Metal", color=(96, 92, 84))
    kit.part(m, "CoreBeacon", (5, 5, 5), (ruin[0], BASE_Y + 136, ruin[2]),
             mat="Neon" if kit._has_mat("Neon") else "Plastic", color=(190, 70, 50),
             shape="ball", collidable=False, shadow=False)
    world.anchor(m, "QuestItem", (ruin[0], BASE_Y + 2.0, ruin[2] + 20),
                 extra={"ItemId": "core_sample"}, name="QuestItemAnchor")
    world.anchor(m, "Document", (ruin[0] + 20, BASE_Y + 2.0, ruin[2] - 14),
                 extra={"DocId": "doc_reactor_final"}, name="DocumentAnchor")
    for i in range(6):
        world.anchor(m, "Loot", (ruin[0] + rng.uniform(-50, 50), BASE_Y + 1.4,
                                 ruin[2] + rng.uniform(-50, 50)),
                     table="rare", name="LootAnchor")
    for i in range(4):
        world.enemy_anchor(m, (ruin[0] + rng.uniform(-60, 60), BASE_Y,
                               ruin[2] + rng.uniform(-60, 60)), 6, "Wasteland")
    # mutant nest
    for nest in range(3):
        ang = rng.uniform(0, math.tau)
        rad = rng.uniform(120, 380)
        np_ = (c[0] + math.cos(ang) * rad, BASE_Y, c[1] + math.sin(ang) * rad)
        m = kit.model(reg, "MutantNest")
        for i in range(9):
            a2 = rng.uniform(0, math.tau)
            d2 = rng.uniform(2, 16)
            kit.part(m, "NestMass", (rng.uniform(3, 9), rng.uniform(2, 6), rng.uniform(3, 9)),
                     (np_[0] + math.cos(a2) * d2, BASE_Y + rng.uniform(0, 3),
                      np_[2] + math.sin(a2) * d2),
                     yaw=rng.uniform(0, 360), mat="Slate", color=(58, 44, 40),
                     shape="ball")
        kit.part(m, "NestCore", (10, 4, 10), (np_[0], BASE_Y + 1.5, np_[2]),
                 mat="Fabric", color=(64, 48, 42), shape="ball", collidable=False)
        kit.lights(m, "NestGlow", (np_[0], BASE_Y + 3, np_[2]), (190, 90, 70), 0.35, 16)
        world.anchor(m, "Loot", (np_[0], BASE_Y + 1.6, np_[2] + 6), table="rare",
                     name="LootAnchor")
        for i in range(2):
            world.enemy_anchor(m, (np_[0] + rng.uniform(-12, 12), BASE_Y,
                                   np_[2] + rng.uniform(-12, 12)), 6, "Wasteland")
    # radiation storm shelter / field lab
    lab = (c[0] + 60, BASE_Y, c[1] - 380)
    m = kit.model(reg, "FieldLab")
    kit.floor(m, lab, 44, 34, mat="Concrete", color=(92, 86, 78))
    kit.wall_open(m, offset(lab, 0, forward=-17), 0, 44, 12, 0.8, "Concrete",
                  (98, 92, 84), [(10, 8, 0, 9)])
    kit.wall_open(m, offset(lab, 0, forward=17), 180, 44, 12, 0.8, "Concrete",
                  (98, 92, 84), [])
    kit.wall_open(m, offset(lab, 0, right=22), 90, 34, 12, 0.8, "Concrete",
                  (98, 92, 84), [(10, 8, 4, 10)])
    kit.wall_open(m, offset(lab, 0, right=-22), -90, 34, 12, 0.8, "Concrete",
                  (98, 92, 84), [])
    kit.roof_flat(m, (lab[0], BASE_Y + 12, lab[2]), 45, 35, mat="CorrodedMetal",
                  color=(80, 74, 66), height=0.8, parapet=False)
    for i in range(3):
        kit.part(m, "LabBench", (14, 3, 5), (lab[0] - 10 + i * 10, BASE_Y + 1.5, lab[2] + 8),
                 mat="CeramicTiles", color=(108, 112, 104))
        kit.lights(m, "LabWorkLight", (lab[0] - 10 + i * 10, BASE_Y + 6, lab[2] + 8),
                   (200, 240, 220), 0.7, 18)
        world.anchor(m, "Loot", (lab[0] - 10 + i * 10, BASE_Y + 1.8, lab[2] + 8),
                     table="rare", name="LootAnchor")
    world.anchor(m, "Workbench", (lab[0] + 14, BASE_Y + 1.4, lab[2] - 8), name="WorkbenchAnchor")
    world.anchor(m, "Document", (lab[0] - 14, BASE_Y + 1.6, lab[2] - 8),
                 extra={"DocId": "doc_field_lab"}, name="DocumentAnchor")
    world.enemy_anchor(m, (lab[0] + 24, BASE_Y, lab[2] - 20), 6, "Wasteland")

    # ambient dressing
    for i in range(60):
        ang = rng.uniform(0, math.tau)
        rad = rng.uniform(30, 450)
        pos = (c[0] + math.cos(ang) * rad, BASE_Y, c[1] + math.sin(ang) * rad)
        if rng.random() < 0.5:
            kit.car_wreck(reg, pos, yaw=rng.uniform(0, 360), burnt=True)
        else:
            kit.barrel(reg, pos, toxic=True)
    for i in range(40):
        world.enemy_anchor(reg, (c[0] + rng.uniform(-440, 440), BASE_Y + 2,
                               c[1] + rng.uniform(-440, 440)), 6, "Wasteland")
    return reg


# -------------------------------------------------------------------- build

def build(workspace, Instance, M, E):
    world = World(workspace, Instance, M, E)
    build_ground(world)
    build_roads(world)
    build_suburbs(world)
    build_forest(world)
    build_industrial(world)
    build_city(world)
    build_military(world)
    build_wasteland(world)

    # distant silhouettes so the horizon never feels empty
    kit = world.kit
    for i in range(26):
        ang = world.rng.uniform(0, math.tau)
        rad = world.rng.uniform(1350, 1700)
        x = math.cos(ang) * rad
        z = math.sin(ang) * rad
        h = world.rng.uniform(60, 200)
        kit.part(world.landmarks, "DistantTower", (world.rng.uniform(30, 70), h, world.rng.uniform(30, 70)),
                 (x, h / 2, z), yaw=world.rng.uniform(0, 360), mat="Concrete",
                 color=(58, 58, 56), shadow=False)
    return {"parts": kit.parts, "models": kit.models}
