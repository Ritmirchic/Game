"""Geometry toolkit used by the world generator.

Everything is built from plain Parts (no asset ids required), so the produced
place file is fully self contained and opens in Studio without publishing
anything. Material variety + lighting carry the visual quality.
"""
import math
import random

PART_SHAPE = {"ball": 0, "block": 1, "cylinder": 2, "wedge": 3, "corner": 4}


def cf(pos, yaw=0.0, pitch=0.0, roll=0.0):
    """Build a CFrame tuple (position, 3x3 row-major rotation)."""
    cy, sy = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    cp, sp = math.cos(math.radians(pitch)), math.sin(math.radians(pitch))
    cr, sr = math.cos(math.radians(roll)), math.sin(math.radians(roll))
    # R = Ry * Rx * Rz  (yaw, then pitch, then roll)
    ry = ((cy, 0.0, sy), (0.0, 1.0, 0.0), (-sy, 0.0, cy))
    rx = ((1.0, 0.0, 0.0), (0.0, cp, -sp), (0.0, sp, cp))
    rz = ((cr, -sr, 0.0), (sr, cr, 0.0), (0.0, 0.0, 1.0))

    def mul(a, b):
        return tuple(tuple(sum(a[i][k] * b[k][j] for k in range(3)) for j in range(3))
                     for i in range(3))

    return (tuple(float(v) for v in pos), mul(ry, mul(rx, rz)))


def offset(pos, yaw, forward=0.0, right=0.0, up=0.0):
    """Offset a position using a yaw angle (degrees). Forward = -Z of the object."""
    r = math.radians(yaw)
    fx, fz = math.sin(r), math.cos(r)
    # forward vector for yaw-rotated CFrame looking down -Z:
    dx = -math.sin(r) * forward + math.cos(r) * right
    dz = -math.cos(r) * forward - math.sin(r) * right
    return (pos[0] + dx, pos[1] + up, pos[2] + dz)


def lerp(a, b, t):
    return a + (b - a) * t


class Kit:
    def __init__(self, Instance, material, enum, rng):
        self.Instance = Instance
        self.M = material
        self.E = enum
        self.rng = rng
        self.parts = 0
        self.models = 0
        self._colors = {}

    # ------------------------------------------------------------- core
    def model(self, parent, name, pos=None):
        m = self.Instance("Model", name)
        if pos is not None:
            m.props["WorldPivotData"] = cf(pos)
        parent.add(m)
        self.models += 1
        return m

    def part(self, parent, name, size, pos, yaw=0.0, pitch=0.0, roll=0.0,
             mat="Concrete", color=(140, 138, 132), transparency=0.0, collidable=True,
             shadow=True, shape="block", reflectance=0.0, query=True, touch=True,
             massless=False, cf_override=None):
        p = self.Instance("Part", name)
        if isinstance(color, int):
            color = (color, color, color)
        p.props.update({
            "Size": (float(size[0]), float(size[1]), float(size[2])),
            "CFrame": cf_override if cf_override is not None else cf(pos, yaw, pitch, roll),
            "Color": (int(color[0]), int(color[1]), int(color[2])),
            "Material": self.M(mat),
            "Anchored": True,
            "CanCollide": bool(collidable),
            "CanQuery": bool(query),
            "CanTouch": bool(touch),
            "CastShadow": bool(shadow),
            "Transparency": float(transparency),
            "Reflectance": float(reflectance),
            "TopSurface": 0, "BottomSurface": 0, "LeftSurface": 0,
            "RightSurface": 0, "FrontSurface": 0, "BackSurface": 0,
            "Massless": bool(massless),
        })
        if shape != "block":
            p.props["Shape"] = PART_SHAPE[shape]
        parent.add(p)
        self.parts += 1
        return p

    def lights(self, parent, name, pos, color=(255, 214, 160), brightness=1.2,
               rng=26.0, shadows=False):
        pl = self.Instance("PointLight", name)
        pl.props.update({
            "Color": (color[0] / 255.0, color[1] / 255.0, color[2] / 255.0),
            "Brightness": float(brightness),
            "Range": float(rng),
            "Shadows": bool(shadows),
            "Enabled": True,
        })
        parent.add(pl)
        return pl

    def anchor(self, parent, name, pos, kind, table=None, extra=None):
        """Invisible interaction anchor picked up by the server decor service."""
        a = self.part(parent, name, (0.6, 0.6, 0.6), pos, mat="Plastic",
                      color=(0, 0, 0), transparency=1.0, collidable=False,
                      shadow=False, query=False, touch=False)
        s = self.Instance("StringValue", "Kind")
        s.props["Value"] = kind
        a.add(s)
        if table:
            t = self.Instance("StringValue", "LootTable")
            t.props["Value"] = table
            a.add(t)
        if extra:
            for k, v in extra.items():
                node = self.Instance("StringValue", k)
                node.props["Value"] = str(v)
                a.add(node)
        return a

    def decal_bytes(self, *args, **kwargs):  # textures need published ids; unused
        return None

    # --------------------------------------------------------- architecture
    def wall(self, parent, pos, yaw, length, height=12.0, thickness=0.9,
             mat="Brick", color=(120, 112, 102), sill=0.0):
        return self.part(parent, "Wall", (length, height, thickness),
                         (pos[0], pos[1] + height / 2 + sill, pos[2]), yaw=yaw,
                         mat=mat, color=color)

    def wall_open(self, parent, pos, yaw, length, height=12.0, thickness=0.9,
                  mat="Brick", color=(120, 112, 102), openings=None):
        """Wall with rectangular openings (doors/windows) given as
        list of (distance_from_start, width, bottom, top)."""
        openings = sorted(openings or [], key=lambda o: o[0])
        cursor = 0.0
        made = []
        for (d, w, b, t) in openings:
            if d > cursor:
                seg = d - cursor
                cx = -length / 2 + cursor + seg / 2
                made.append(self._wall_piece(parent, pos, yaw, cx, seg, height, thickness, mat, color))
            # sill below opening + lintel above
            if b > 0:
                cx = -length / 2 + d + w / 2
                made.append(self._wall_piece(parent, pos, yaw, cx, w, b, thickness, mat, color,
                                             y0=0.0))
            if t < height:
                cx = -length / 2 + d + w / 2
                made.append(self._wall_piece(parent, pos, yaw, cx, w, height - t, thickness,
                                             mat, color, y0=t))
            cursor = d + w
        if cursor < length:
            cx = -length / 2 + cursor + (length - cursor) / 2
            made.append(self._wall_piece(parent, pos, yaw, cx, length - cursor, height,
                                         thickness, mat, color))
        return made

    def _wall_piece(self, parent, pos, yaw, local_x, w, h, thickness, mat, color, y0=0.0):
        r = math.radians(yaw)
        # local +X axis for a yaw rotation
        px = pos[0] + math.cos(r) * local_x
        pz = pos[2] - math.sin(r) * local_x
        return self.part(parent, "Wall", (w, h, thickness),
                         (px, pos[1] + y0 + h / 2, pz), yaw=yaw, mat=mat, color=color)

    def floor(self, parent, pos, w, d, yaw=0.0, mat="Concrete", color=(120, 118, 112),
              thickness=1.0):
        return self.part(parent, "Floor", (w, thickness, d), (pos[0], pos[1] - thickness / 2, pos[2]),
                         yaw=yaw, mat=mat, color=color)

    def roof_flat(self, parent, pos, w, d, yaw=0.0, mat="Concrete", color=(92, 90, 88),
                  height=0.8, parapet=True):
        self.part(parent, "Roof", (w, height, d), (pos[0], pos[1] + height / 2, pos[2]),
                  yaw=yaw, mat=mat, color=color)
        if parapet:
            ph = 3.0
            for sx, sz, lw, ld in ((0, -d / 2 + 0.5, w, 1.0), (0, d / 2 - 0.5, w, 1.0),
                                   (-w / 2 + 0.5, 0, 1.0, d), (w / 2 - 0.5, 0, 1.0, d)):
                r = math.radians(yaw)
                px = pos[0] + math.cos(r) * sx - math.sin(r) * sz
                pz = pos[2] - math.sin(r) * sx - math.cos(r) * sz
                self.part(parent, "Parapet", (lw, ph, ld), (px, pos[1] + height + ph / 2, pz),
                          yaw=yaw, mat=mat, color=color)

    def roof_gable(self, parent, pos, w, d, yaw=0.0, mat="WoodPlanks", color=(70, 62, 54),
                   rise=6.0, overhang=1.2):
        """Two slanted slabs forming a gable roof (ridge along X)."""
        half = d / 2 + overhang
        angle = math.degrees(math.atan2(rise, half))
        slab = math.hypot(half, rise)
        for sign in (-1, 1):
            r = math.radians(yaw)
            # local offset along Z for each side
            sz = sign * half / 2
            px = pos[0] - math.sin(r) * sz
            pz = pos[2] - math.cos(r) * sz
            self.part(parent, "RoofSlope", (w + overhang * 2, 0.7, slab),
                      (px, pos[1] + rise / 2, pz), yaw=yaw, pitch=sign * angle,
                      mat=mat, color=color)
        # gable end triangles (approximate with thin boxes)
        for sign in (-1, 1):
            r = math.radians(yaw)
            sx = sign * (w / 2 - 0.2)
            px = pos[0] + math.cos(r) * sx
            pz = pos[2] - math.sin(r) * sx
            steps = 4
            for i in range(steps):
                ww = (w / 2) * 0.0
                pass

    def stairs(self, parent, pos, yaw, width=4.0, steps=10, rise=0.9, run=1.4,
               mat="Concrete", color=(110, 108, 104), railing=True):
        r = math.radians(yaw)
        for i in range(steps):
            h = rise * (i + 1)
            along = run * (i + 0.5)
            px = pos[0] - math.sin(r) * along
            pz = pos[2] - math.cos(r) * along
            self.part(parent, "Step", (width, h, run), (px, pos[1] + h / 2, pz),
                      yaw=yaw, mat=mat, color=color)
        if railing:
            for side in (-1, 1):
                for i in range(steps):
                    h = rise * (i + 1)
                    along = run * (i + 0.5)
                    px = pos[0] - math.sin(r) * along + math.cos(r) * side * (width / 2 - 0.2)
                    pz = pos[2] - math.cos(r) * along - math.sin(r) * side * (width / 2 - 0.2)
                    self.part(parent, "Post", (0.25, 3.4, 0.25), (px, pos[1] + h + 1.7, pz),
                              yaw=yaw, mat="Metal", color=(80, 78, 74), collidable=False)
        return rise * steps

    def railing(self, parent, a, b, height=3.2, mat="Metal", color=(80, 78, 74),
                spacing=6.0):
        dx, dz = b[0] - a[0], b[2] - a[2]
        length = math.hypot(dx, dz)
        yaw = math.degrees(math.atan2(-dz, dx))
        n = max(1, int(length / spacing))
        for i in range(n + 1):
            t = i / n
            self.part(parent, "RailPost", (0.22, height, 0.22),
                      (lerp(a[0], b[0], t), a[1] + height / 2, lerp(a[2], b[2], t)),
                      yaw=yaw, mat=mat, color=color, collidable=False)
        mid = ((a[0] + b[0]) / 2, a[1] + height - 0.2, (a[2] + b[2]) / 2)
        self.part(parent, "Rail", (length, 0.22, 0.22), mid, yaw=yaw, mat=mat,
                  color=color, collidable=False)

    def window(self, parent, pos, yaw, w=4.0, h=4.0, lit=False, broken=0.0):
        """Window: frame plus translucent glass. `broken` removes panes."""
        r = math.radians(yaw)
        self.part(parent, "WindowFrame", (w + 0.5, h + 0.5, 0.35), pos, yaw=yaw,
                  mat="Metal", color=(58, 54, 50), collidable=False)
        if self.rng.random() > broken:
            self.part(parent, "Glass", (w, h, 0.12), pos, yaw=yaw, mat="Glass",
                      color=(120, 132, 138), transparency=0.55, collidable=False,
                      shadow=False, reflectance=0.2)
        else:
            # jagged remaining shards
            for i in range(3):
                sw = w * self.rng.uniform(0.15, 0.3)
                sx = (i - 1) * w * 0.3
                px = pos[0] + math.cos(r) * sx
                pz = pos[2] - math.sin(r) * sx
                self.part(parent, "GlassShard", (sw, h * self.rng.uniform(0.3, 0.7), 0.1),
                          (px, pos[1] + h * 0.2, pz), yaw=yaw, mat="Glass",
                          color=(110, 122, 128), transparency=0.5, collidable=False,
                          shadow=False)

    def rubble(self, parent, pos, radius=6.0, count=14, mat="Concrete",
               color=(100, 96, 90)):
        for i in range(count):
            ang = self.rng.uniform(0, math.pi * 2)
            d = self.rng.uniform(0, radius)
            s = self.rng.uniform(0.4, 2.4)
            self.part(parent, "Debris", (s, s * self.rng.uniform(0.3, 0.8), s * self.rng.uniform(0.5, 1.5)),
                      (pos[0] + math.cos(ang) * d, pos[1] + s * 0.2, pos[2] + math.sin(ang) * d),
                      yaw=self.rng.uniform(0, 180), pitch=self.rng.uniform(-20, 20),
                      mat=mat, color=color, collidable=False, shadow=False)

    def grass_tufts(self, parent, pos, radius=14.0, count=18, color=(70, 88, 52)):
        for i in range(count):
            ang = self.rng.uniform(0, math.pi * 2)
            d = self.rng.uniform(0, radius)
            h = self.rng.uniform(0.8, 2.4)
            self.part(parent, "Grass", (self.rng.uniform(0.6, 1.6), h, 0.15),
                      (pos[0] + math.cos(ang) * d, pos[1] + h / 2, pos[2] + math.sin(ang) * d),
                      yaw=self.rng.uniform(0, 180), mat="Grass", color=color,
                      collidable=False, shadow=False, query=False, touch=False)

    # ------------------------------------------------------------- furniture
    def bed(self, parent, pos, yaw, color=(86, 74, 66)):
        frame = self.part(parent, "BedFrame", (6.4, 1.1, 3.4), (pos[0], pos[1] + 0.55, pos[2]),
                          yaw=yaw, mat="Wood", color=(64, 54, 46))
        self.part(parent, "Mattress", (6.2, 0.9, 3.2), (pos[0], pos[1] + 1.4, pos[2]),
                  yaw=yaw, mat="Fabric", color=color)
        r = math.radians(yaw)
        px = pos[0] - math.sin(r) * -1.2
        pz = pos[2] - math.cos(r) * -1.2
        self.part(parent, "Blanket", (4.4, 0.5, 3.25), (px, pos[1] + 1.75, pz), yaw=yaw,
                  mat="Fabric", color=(58, 58, 52))
        px2 = pos[0] - math.sin(r) * 2.4
        pz2 = pos[2] - math.cos(r) * 2.4
        self.part(parent, "Pillow", (1.8, 0.6, 2.4), (px2, pos[1] + 1.75, pz2), yaw=yaw,
                  mat="Fabric", color=(178, 172, 158))
        return frame

    def table(self, parent, pos, yaw, w=5.0, d=3.4, h=2.6, mat="Wood", color=(92, 74, 58)):
        self.part(parent, "TableTop", (w, 0.35, d), (pos[0], pos[1] + h, pos[2]), yaw=yaw,
                  mat=mat, color=color)
        r = math.radians(yaw)
        for sx in (-1, 1):
            for sz in (-1, 1):
                lx, lz = sx * (w / 2 - 0.4), sz * (d / 2 - 0.4)
                px = pos[0] + math.cos(r) * lx - math.sin(r) * lz
                pz = pos[2] - math.sin(r) * lx - math.cos(r) * lz
                self.part(parent, "Leg", (0.3, h, 0.3), (px, pos[1] + h / 2, pz), yaw=yaw,
                          mat=mat, color=color, collidable=False)

    def chair(self, parent, pos, yaw, color=(74, 62, 50)):
        self.part(parent, "Seat", (1.9, 0.25, 1.9), (pos[0], pos[1] + 1.7, pos[2]), yaw=yaw,
                  mat="Wood", color=color)
        self.part(parent, "Back", (1.9, 1.9, 0.2),
                  (offset(pos, yaw, forward=-0.85, up=2.7)), yaw=yaw, mat="Wood", color=color)
        r = math.radians(yaw)
        for sx in (-1, 1):
            for sz in (-1, 1):
                lx, lz = sx * 0.75, sz * 0.75
                px = pos[0] + math.cos(r) * lx - math.sin(r) * lz
                pz = pos[2] - math.sin(r) * lx - math.cos(r) * lz
                self.part(parent, "Leg", (0.18, 1.7, 0.18), (px, pos[1] + 0.85, pz),
                          yaw=yaw, mat="Wood", color=color, collidable=False)

    def sofa(self, parent, pos, yaw, color=(56, 54, 48)):
        self.part(parent, "SofaBase", (7.0, 1.6, 3.2), (pos[0], pos[1] + 0.8, pos[2]), yaw=yaw,
                  mat="Fabric", color=color)
        self.part(parent, "SofaBack", (7.0, 2.4, 0.7),
                  (offset(pos, yaw, forward=-1.25, up=2.0)), yaw=yaw, mat="Fabric", color=color)
        for sign in (-1, 1):
            self.part(parent, "Arm", (0.7, 1.3, 3.2),
                      (offset(pos, yaw, right=sign * 3.15, up=1.3)), yaw=yaw,
                      mat="Fabric", color=color)

    def shelf(self, parent, pos, yaw, w=5.0, h=7.0, d=1.6, color=(78, 66, 54)):
        r = math.radians(yaw)
        self.part(parent, "ShelfBack", (w, h, 0.2), (pos[0], pos[1] + h / 2, pos[2]), yaw=yaw,
                  mat="Wood", color=color)
        for i in range(4):
            y = pos[1] + 0.4 + i * (h / 4)
            self.part(parent, "ShelfBoard", (w, 0.18, d),
                      (offset(pos, yaw, forward=d / 2, up=y - pos[1])), yaw=yaw,
                      mat="Wood", color=color, collidable=False)

    def cabinet(self, parent, pos, yaw, w=3.0, h=6.0, d=2.0, color=(96, 96, 92)):
        self.part(parent, "Cabinet", (w, h, d), (pos[0], pos[1] + h / 2, pos[2]), yaw=yaw,
                  mat="Metal", color=color)

    def fridge(self, parent, pos, yaw, color=(188, 188, 182), open_door=0.0):
        self.part(parent, "Fridge", (3.2, 7.0, 2.8), (pos[0], pos[1] + 3.5, pos[2]), yaw=yaw,
                  mat="Metal", color=color)
        self.part(parent, "FridgeDoor", (0.25, 6.6, 2.7),
                  (offset(pos, yaw, forward=1.45, up=3.5)), yaw=yaw + open_door,
                  mat="Metal", color=(170, 170, 164), collidable=False)

    def counter(self, parent, pos, yaw, w=8.0, color=(120, 116, 108)):
        self.part(parent, "CounterBody", (w, 3.0, 2.6), (pos[0], pos[1] + 1.5, pos[2]),
                  yaw=yaw, mat="Wood", color=(82, 70, 58))
        self.part(parent, "CounterTop", (w + 0.3, 0.25, 2.9),
                  (pos[0], pos[1] + 3.1, pos[2]), yaw=yaw, mat="CeramicTiles", color=color)

    def sink(self, parent, pos, yaw):
        self.part(parent, "Sink", (2.6, 0.5, 1.9), (pos[0], pos[1], pos[2]), yaw=yaw,
                  mat="CeramicTiles", color=(196, 196, 190), collidable=False)
        self.part(parent, "Faucet", (0.25, 1.4, 0.25),
                  (offset(pos, yaw, right=-0.8, up=0.8)), yaw=yaw, mat="Metal",
                  color=(150, 148, 140), collidable=False)

    def toilet(self, parent, pos, yaw):
        self.part(parent, "ToiletBase", (1.8, 1.5, 2.2), (pos[0], pos[1] + 0.75, pos[2]),
                  yaw=yaw, mat="CeramicTiles", color=(206, 202, 194))
        self.part(parent, "ToiletTank", (1.8, 1.6, 0.8),
                  (offset(pos, yaw, forward=-0.8, up=2.2)), yaw=yaw,
                  mat="CeramicTiles", color=(206, 202, 194))

    def desk(self, parent, pos, yaw, w=6.0):
        self.part(parent, "DeskTop", (w, 0.3, 3.0), (pos[0], pos[1] + 2.6, pos[2]), yaw=yaw,
                  mat="Wood", color=(84, 74, 62))
        self.part(parent, "DeskDrawer", (w * 0.4, 2.2, 2.6),
                  (offset(pos, yaw, right=w * 0.25, up=1.1)), yaw=yaw,
                  mat="Metal", color=(100, 100, 96))

    def locker(self, parent, pos, yaw, color=(74, 96, 88)):
        self.part(parent, "Locker", (2.4, 7.0, 2.0), (pos[0], pos[1] + 3.5, pos[2]), yaw=yaw,
                  mat="Metal", color=color)
        for i in range(2):
            self.part(parent, "LockerDoor", (2.2, 3.2, 0.2),
                      (offset(pos, yaw, forward=1.05, up=1.9 + i * 3.5)), yaw=yaw,
                      mat="Metal", color=(color[0] + 14, color[1] + 14, color[2] + 12),
                      collidable=False)

    def crate(self, parent, pos, yaw=0.0, size=3.0, color=(112, 92, 64)):
        self.part(parent, "Crate", (size, size, size), (pos[0], pos[1] + size / 2, pos[2]),
                  yaw=yaw, mat="Wood", color=color)
        self.part(parent, "CrateRailA", (size * 1.02, 0.3, 0.3),
                  (pos[0], pos[1] + size * 0.72, pos[2] + size / 2 * 0.98), yaw=yaw,
                  mat="Wood", color=(88, 72, 50), collidable=False)

    def barrel(self, parent, pos, color=(96, 88, 70), toxic=False):
        self.part(parent, "Barrel", (2.6, 4.0, 2.6), (pos[0], pos[1] + 2.0, pos[2]),
                  mat="CorrodedMetal", color=color, shape="cylinder")
        self.part(parent, "BarrelRing", (2.75, 0.25, 2.75),
                  (pos[0], pos[1] + 2.9, pos[2]), mat="CorrodedMetal",
                  color=(120, 90, 60), shape="cylinder", collidable=False)
        if toxic:
            self.part(parent, "ToxicSludge", (2.3, 0.2, 2.3), (pos[0], pos[1] + 4.05, pos[2]),
                      mat="Plastic", color=(94, 128, 52), shape="cylinder",
                      collidable=False, shadow=False)

    def generator(self, parent, pos, yaw=0.0):
        self.part(parent, "GeneratorBody", (5.0, 3.4, 3.0), (pos[0], pos[1] + 1.7, pos[2]),
                  yaw=yaw, mat="DiamondPlate", color=(96, 92, 80))
        self.part(parent, "GeneratorTank", (2.0, 2.6, 2.0),
                  (offset(pos, yaw, right=2.6, up=1.9)), yaw=yaw,
                  mat="CorrodedMetal", color=(120, 88, 64), shape="cylinder")
        self.part(parent, "GeneratorPipe", (0.6, 3.2, 0.6),
                  (offset(pos, yaw, right=-1.8, up=4.0)), yaw=yaw,
                  mat="CorrodedMetal", color=(96, 74, 52), shape="cylinder",
                  collidable=False)
        self.lights(parent, "GeneratorLamp", (offset(pos, yaw, up=4.2)), (255, 120, 60), 0.9, 14)

    def streetlamp(self, parent, pos, yaw=0.0, color=(255, 196, 132), on=False, height=22.0):
        self.part(parent, "LampPost", (0.55, height, 0.55), (pos[0], pos[1] + height / 2, pos[2]),
                  yaw=yaw, mat="CorrodedMetal", color=(66, 64, 60))
        head = offset(pos, yaw, forward=1.6, up=height)
        self.part(parent, "LampArm", (0.4, 0.4, 3.4),
                  (offset(pos, yaw, forward=0.8, up=height - 0.2)), yaw=yaw,
                  mat="CorrodedMetal", color=(64, 62, 58), collidable=False)
        self.part(parent, "LampHead", (1.4, 0.8, 2.0), head, yaw=yaw,
                  mat="Metal", color=(70, 68, 62), collidable=False)
        self.part(parent, "LampGlass", (1.1, 0.25, 1.6),
                  (head[0], head[1] - 0.45, head[2]), yaw=yaw,
                  mat="Glass", color=color if on else (96, 94, 88),
                  transparency=0.2, collidable=False, shadow=False)
        if on:
            self.lights(parent, "LampLight", (head[0], head[1] - 1.2, head[2]), color, 1.6, 26)

    # ---------------------------------------------------------------- nature
    def tree(self, parent, pos, scale=1.0, dead=False, rng=None):
        rng = rng or self.rng
        h = rng.uniform(26, 46) * scale
        trunk_w = rng.uniform(1.6, 2.6) * scale
        yaw = rng.uniform(0, 360)
        bark = (58, 50, 42) if not dead else (74, 68, 60)
        self.part(parent, "Trunk", (trunk_w, h, trunk_w),
                  (pos[0], pos[1] + h / 2, pos[2]), yaw=yaw,
                  mat="Wood", color=bark, shape="cylinder", collidable=True)
        if not dead:
            layers = rng.randint(2, 4)
            for i in range(layers):
                t = (i + 1) / (layers + 1)
                r = (1.0 - t * 0.62) * h * 0.42
                self.part(parent, "Canopy", (r * 2.1, h * 0.3, r * 2.1),
                          (pos[0], pos[1] + h * (0.55 + t * 0.42), pos[2]), yaw=yaw,
                          mat="LeafyGrass", color=(38 + i * 6, 56 + i * 4, 34),
                          shape="ball", collidable=False, shadow=True)
        else:
            for i in range(3):
                ang = rng.uniform(0, 360)
                pitch = rng.uniform(30, 60)
                ln = rng.uniform(4, 9) * scale
                bx = pos[0] + math.cos(math.radians(ang)) * ln * 0.4
                bz = pos[2] + math.sin(math.radians(ang)) * ln * 0.4
                self.part(parent, "DeadBranch", (0.5 * scale, ln, 0.5 * scale),
                          (bx, pos[1] + h * rng.uniform(0.55, 0.9), bz),
                          yaw=ang, pitch=pitch, mat="Wood", color=bark,
                          shape="cylinder", collidable=False)

    def bush(self, parent, pos, scale=1.0):
        r = self.rng.uniform(2.4, 4.6) * scale
        self.part(parent, "Bush", (r, r * 0.8, r), (pos[0], pos[1] + r * 0.35, pos[2]),
                  yaw=self.rng.uniform(0, 360), mat="LeafyGrass",
                  color=(44, 58, 36), shape="ball", collidable=False)

    def rock(self, parent, pos, scale=1.0, mat="Rock", color=(104, 100, 94)):
        n = self.rng.randint(1, 3)
        for i in range(n):
            s = self.rng.uniform(2.0, 6.0) * scale
            self.part(parent, "Rock", (s, s * self.rng.uniform(0.5, 0.9), s * self.rng.uniform(0.7, 1.3)),
                      (pos[0] + self.rng.uniform(-2, 2) * scale, pos[1] + s * 0.15,
                       pos[2] + self.rng.uniform(-2, 2) * scale),
                      yaw=self.rng.uniform(0, 360), pitch=self.rng.uniform(-20, 20),
                      mat=mat, color=color, collidable=True)

    def stump(self, parent, pos, scale=1.0):
        s = self.rng.uniform(1.6, 2.6) * scale
        self.part(parent, "Stump", (s, 1.6, s), (pos[0], pos[1] + 0.8, pos[2]),
                  mat="Wood", color=(70, 58, 46), shape="cylinder")

    # ---------------------------------------------------------------- vehicles
    def car_wreck(self, parent, pos, yaw=0.0, color=None, burnt=False, flipped=False):
        palette = [(96, 92, 84), (74, 84, 88), (88, 74, 66), (62, 66, 62), (110, 100, 82)]
        color = color or self.rng.choice(palette)
        if burnt:
            color = (44, 42, 40)
        mat = "CorrodedMetal" if not burnt else "CorrodedMetal"
        roll = 180 if flipped else 0
        up = 2.2 if not flipped else 3.4
        self.part(parent, "CarBody", (7.0, 2.2, 15.5),
                  (pos[0], pos[1] + up, pos[2]), yaw=yaw, roll=roll, mat=mat, color=color)
        self.part(parent, "CarCabin", (6.2, 2.0, 6.4),
                  (offset(pos, yaw, forward=-0.6, up=up + 1.9)), yaw=yaw, roll=roll,
                  mat=mat, color=(color[0] - 10, color[1] - 10, color[2] - 10))
        for sx in (-1, 1):
            for fz in (-1, 1):
                self.part(parent, "Wheel", (2.4, 1.4, 2.4),
                          (offset(pos, yaw, forward=fz * 5.2, right=sx * 3.6, up=1.4)),
                          yaw=yaw, mat="Rubber" if self._has_mat("Rubber") else "Plastic",
                          color=(34, 32, 30), shape="cylinder", collidable=False)
        self.part(parent, "Windshield", (5.6, 1.7, 0.3),
                  (offset(pos, yaw, forward=2.0, up=up + 1.7)), yaw=yaw, roll=roll,
                  mat="Glass", color=(70, 78, 82), transparency=0.6, collidable=False,
                  shadow=False)
        if not burnt and self.rng.random() < 0.6:
            self.part(parent, "RustPatch", (7.2, 0.5, 4.0),
                      (offset(pos, yaw, forward=4.0, up=up + 0.2)), yaw=yaw,
                      mat="CorrodedMetal", color=(112, 66, 40), collidable=False)

    def _has_mat(self, name):
        try:
            self.M(name)
            return True
        except Exception:
            return False

    def bus_wreck(self, parent, pos, yaw=0.0):
        self.part(parent, "BusBody", (10.0, 11.0, 34.0), (pos[0], pos[1] + 5.5, pos[2]),
                  yaw=yaw, mat="CorrodedMetal", color=(104, 96, 78))
        self.part(parent, "BusRoof", (10.2, 0.8, 34.2), (pos[0], pos[1] + 11.2, pos[2]),
                  yaw=yaw, mat="CorrodedMetal", color=(88, 82, 68))
        for i in range(5):
            for side in (-1, 1):
                self.part(parent, "BusWindow", (0.2, 3.4, 4.6),
                          (offset(pos, yaw, forward=-12 + i * 6, right=side * 5.05, up=8.0)),
                          yaw=yaw, mat="Glass", color=(60, 68, 70), transparency=0.65,
                          collidable=False, shadow=False)

    def armored_vehicle(self, parent, pos, yaw=0.0):
        self.part(parent, "APCBody", (11.0, 9.0, 24.0), (pos[0], pos[1] + 4.5, pos[2]),
                  yaw=yaw, mat="DiamondPlate", color=(74, 80, 62))
        self.part(parent, "APCTurret", (7.0, 3.4, 7.0),
                  (offset(pos, yaw, up=10.0)), yaw=yaw, mat="DiamondPlate", color=(68, 74, 58))
        self.part(parent, "APCBarrel", (1.0, 1.0, 12.0),
                  (offset(pos, yaw, forward=9.0, up=10.4)), yaw=yaw,
                  mat="Metal", color=(56, 58, 50), shape="cylinder", collidable=False)
        for sx in (-1, 1):
            for fz in (-1, 1):
                self.part(parent, "APCWheel", (3.8, 2.0, 3.8),
                          (offset(pos, yaw, forward=fz * 8.0, right=sx * 5.2, up=2.0)),
                          yaw=yaw, mat="Plastic", color=(30, 30, 28), shape="cylinder",
                          collidable=False)

    # ---------------------------------------------------------------- misc props
    def fence(self, parent, a, b, height=6.0, mat="Wood", color=(84, 70, 54), spacing=8.0):
        dx, dz = b[0] - a[0], b[2] - a[2]
        length = math.hypot(dx, dz)
        if length < 0.1:
            return
        yaw = math.degrees(math.atan2(-dz, dx))
        n = max(1, int(length / spacing))
        for i in range(n + 1):
            t = i / n
            self.part(parent, "FencePost", (0.45, height, 0.45),
                      (lerp(a[0], b[0], t), a[1] + height / 2, lerp(a[2], b[2], t)),
                      yaw=yaw, mat=mat, color=color, collidable=False)
        # horizontal boards
        for yy in (height * 0.35, height * 0.8):
            self.part(parent, "FenceBoard", (length, 0.6, 0.28),
                      ((a[0] + b[0]) / 2, a[1] + yy, (a[2] + b[2]) / 2), yaw=yaw,
                      mat=mat, color=color)

    def sandbags(self, parent, pos, yaw, length=10.0):
        r = math.radians(yaw)
        n = int(length / 2.0)
        for i in range(n):
            for layer in range(2):
                lx = -length / 2 + 1.0 + i * 2.0
                px = pos[0] + math.cos(r) * lx
                pz = pos[2] - math.sin(r) * lx
                self.part(parent, "Sandbag", (1.9, 0.9, 1.3),
                          (px, pos[1] + 0.45 + layer * 0.85, pz), yaw=yaw,
                          mat="Fabric", color=(108, 100, 78), collidable=True)

    def traffic_barrier(self, parent, pos, yaw, color=(178, 172, 154)):
        self.part(parent, "BarrierWall", (9.0, 3.2, 0.6), (pos[0], pos[1] + 2.6, pos[2]),
                  yaw=yaw, mat="Concrete", color=color)
        for s in (-1, 1):
            self.part(parent, "BarrierFoot", (2.0, 1.0, 2.6),
                      (offset(pos, yaw, right=s * 3.4, up=0.5)), yaw=yaw,
                      mat="Concrete", color=color)

    def power_pole(self, parent, pos, height=34.0):
        self.part(parent, "PowerPole", (0.8, height, 0.8), (pos[0], pos[1] + height / 2, pos[2]),
                  mat="Wood", color=(66, 54, 44), shape="cylinder")
        self.part(parent, "Crossbar", (10.0, 0.5, 0.5),
                  (pos[0], pos[1] + height - 1.6, pos[2]), mat="Wood", color=(66, 54, 44),
                  collidable=False)
        for s in (-1, 1):
            self.part(parent, "Insulator", (0.5, 1.0, 0.5),
                      (pos[0] + s * 4.0, pos[1] + height - 0.8, pos[2]),
                      mat="CeramicTiles", color=(120, 116, 104), collidable=False)

    def wire(self, parent, a, b):
        """Approximate a catenary with a couple of thin parts."""
        seg = 3
        for i in range(seg):
            t0, t1 = i / seg, (i + 1) / seg
            p0 = (lerp(a[0], b[0], t0), lerp(a[1], b[1], t0) - math.sin(t0 * math.pi) * 2.0, lerp(a[2], b[2], t0))
            p1 = (lerp(a[0], b[0], t1), lerp(a[1], b[1], t1) - math.sin(t1 * math.pi) * 2.0, lerp(a[2], b[2], t1))
            mx = ((p0[0] + p1[0]) / 2, (p0[1] + p1[1]) / 2, (p0[2] + p1[2]) / 2)
            dx, dy, dz = p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2]
            length = math.sqrt(dx * dx + dy * dy + dz * dz)
            yaw = math.degrees(math.atan2(-dz, dx))
            pitch = math.degrees(math.atan2(dy, math.hypot(dx, dz)) or 0.0)
            self.part(parent, "Wire", (length, 0.12, 0.12), mx, yaw=yaw, pitch=pitch,
                      mat="Metal", color=(34, 32, 30), collidable=False, shadow=False)

    def pipe(self, parent, a, b, radius=1.2, color=(96, 82, 62)):
        dx, dy, dz = b[0] - a[0], b[1] - a[1], b[2] - a[2]
        length = math.sqrt(dx * dx + dy * dy + dz * dz)
        yaw = math.degrees(math.atan2(-dz, dx))
        pitch = math.degrees(math.atan2(dy, math.hypot(dx, dz)))
        self.part(parent, "Pipe", (radius * 2, length, radius * 2),
                  ((a[0] + b[0]) / 2, (a[1] + b[1]) / 2, (a[2] + b[2]) / 2),
                  yaw=yaw, pitch=pitch - 90, mat="CorrodedMetal", color=color,
                  shape="cylinder", collidable=False)

    def crate_stack(self, parent, pos, yaw=0.0, count=4):
        for i in range(count):
            ang = self.rng.uniform(0, 360)
            s = self.rng.uniform(2.4, 3.6)
            off = self.rng.uniform(0, 3.0)
            self.crate(parent, (pos[0] + math.cos(math.radians(ang)) * off,
                                pos[1] + (i // 2) * 3.2,
                                pos[2] + math.sin(math.radians(ang)) * off),
                       yaw=yaw + self.rng.uniform(-15, 15), size=s)

    def medical_bed(self, parent, pos, yaw):
        self.part(parent, "MedBedFrame", (3.6, 0.5, 7.6), (pos[0], pos[1] + 2.9, pos[2]),
                  yaw=yaw, mat="Metal", color=(150, 152, 148))
        self.part(parent, "MedBedMattress", (3.3, 0.5, 7.0), (pos[0], pos[1] + 3.3, pos[2]),
                  yaw=yaw, mat="Fabric", color=(176, 176, 168))
        for sx in (-1, 1):
            for fz in (-1, 1):
                self.part(parent, "MedBedLeg", (0.22, 2.7, 0.22),
                          (offset(pos, yaw, forward=fz * 3.2, right=sx * 1.5, up=1.35)),
                          yaw=yaw, mat="Metal", color=(130, 132, 128), collidable=False)
        self.part(parent, "IVStand", (0.25, 8.0, 0.25),
                  (offset(pos, yaw, right=2.6, up=4.0)), yaw=yaw, mat="Metal",
                  color=(160, 160, 154), collidable=False)

    def hospital_curtain(self, parent, pos, yaw):
        self.part(parent, "Curtain", (0.15, 8.0, 10.0), (pos[0], pos[1] + 4.0, pos[2]),
                  yaw=yaw, mat="Fabric", color=(112, 122, 118), transparency=0.25,
                  collidable=False, shadow=False)

    def campfire(self, parent, pos, lit=False):
        self.part(parent, "FireRing", (4.2, 0.7, 4.2), (pos[0], pos[1] + 0.35, pos[2]),
                  mat="Rock", color=(88, 84, 78), shape="cylinder", collidable=False)
        for i in range(5):
            ang = math.radians(i * 72)
            self.part(parent, "FireLog", (0.7, 4.0, 0.7),
                      (pos[0] + math.cos(ang) * 0.9, pos[1] + 1.1, pos[2] + math.sin(ang) * 0.9),
                      yaw=math.degrees(ang), pitch=52, mat="Wood", color=(58, 44, 34),
                      collidable=False)
        self.part(parent, "Embers", (2.2, 0.2, 2.2), (pos[0], pos[1] + 0.4, pos[2]),
                  mat="CrackedLava", color=(120, 52, 24), collidable=False, shadow=False,
                  transparency=0.2 if not lit else 0.0)
        if lit:
            self.lights(parent, "FireLight", (pos[0], pos[1] + 2.4, pos[2]),
                        (255, 150, 70), 2.2, 30, shadows=True)

    def tent(self, parent, pos, yaw, color=(78, 82, 62)):
        self.part(parent, "TentFloor", (10.0, 0.3, 12.0), (pos[0], pos[1] + 0.2, pos[2]),
                  yaw=yaw, mat="Fabric", color=color, collidable=False)
        for side in (-1, 1):
            self.part(parent, "TentRoof", (10.5, 0.3, 7.0),
                      (offset(pos, yaw, right=side * 2.4, up=3.6)), yaw=yaw,
                      roll=side * 55, mat="Fabric", color=color)
        self.part(parent, "TentBack", (10.0, 5.0, 0.3),
                  (offset(pos, yaw, forward=-5.6, up=2.5)), yaw=yaw,
                  mat="Fabric", color=color)

    def watchtower(self, parent, pos, yaw=0.0, height=34.0):
        r = math.radians(yaw)
        for sx in (-1, 1):
            for sz in (-1, 1):
                lx, lz = sx * 4.5, sz * 4.5
                px = pos[0] + math.cos(r) * lx - math.sin(r) * lz
                pz = pos[2] - math.sin(r) * lx - math.cos(r) * lz
                self.part(parent, "TowerLeg", (0.8, height, 0.8),
                          (px, pos[1] + height / 2, pz), yaw=yaw,
                          mat="Wood", color=(72, 60, 48))
        deck_y = pos[1] + height
        self.part(parent, "TowerDeck", (11.0, 0.8, 11.0), (pos[0], deck_y, pos[2]),
                  yaw=yaw, mat="WoodPlanks", color=(78, 66, 52))
        self.part(parent, "TowerRoof", (12.0, 0.6, 12.0), (pos[0], deck_y + 9.0, pos[2]),
                  yaw=yaw, mat="CorrodedMetal", color=(70, 68, 62))
        for sx in (-1, 1):
            self.part(parent, "TowerWall", (11.0, 6.0, 0.3),
                      (offset((pos[0], deck_y, pos[2]), yaw, forward=sx * 5.4, up=3.0)),
                      yaw=yaw, mat="WoodPlanks", color=(80, 68, 54))
        for sz in (-1, 1):
            self.part(parent, "TowerWall", (0.3, 6.0, 11.0),
                      (offset((pos[0], deck_y, pos[2]), yaw, right=sz * 5.4, up=3.0)),
                      yaw=yaw, mat="WoodPlanks", color=(80, 68, 54))
        # ladder
        for i in range(int(height / 4)):
            self.part(parent, "LadderRung", (2.4, 0.25, 0.25),
                      (offset(pos, yaw, forward=5.0, up=2.5 + i * 4.0)), yaw=yaw,
                      mat="Metal", color=(90, 88, 82), collidable=False)
