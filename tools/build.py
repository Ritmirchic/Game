#!/usr/bin/env python3
"""Build ASHES_OF_HUMANITY.rbxl

Assembles:
  * the Luau source tree (src/) into Script/ModuleScript/LocalScript instances
  * the baked world (tools/worldgen.py)
  * lighting / atmosphere / post-processing
into a real Roblox binary place file.
"""
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)

from rbx import binary, db
from rbx.binary import Instance
import worldgen

OUT = os.path.join(ROOT, "ASHES_OF_HUMANITY.rbxl")
SRC = os.path.join(ROOT, "src")

# Enum helpers
M = lambda name: db.enum_value("Material", name)
E = lambda enum, name: db.enum_value(enum, name)

SERVICE_ORDER = [
    "Workspace", "Lighting", "ReplicatedStorage", "ReplicatedFirst",
    "ServerScriptService", "ServerStorage", "StarterGui", "StarterPack",
    "StarterPlayer", "SoundService", "Players", "Teams", "TextChatService",
    "MaterialService",
]


def read_sources(folder, out=None, base=None):
    """Read a directory tree of .lua files into (relative path, kind, source)."""
    if out is None:
        out = []
    if base is None:
        base = folder
    for entry in sorted(os.listdir(folder)):
        full = os.path.join(folder, entry)
        if os.path.isdir(full):
            read_sources(full, out, base)
        elif entry.endswith(".lua"):
            rel = os.path.relpath(full, base)
            kind = "module"
            name = entry
            if name.endswith(".server.lua"):
                kind = "script"
                name = name[: -len(".server.lua")]
            elif name.endswith(".client.lua"):
                kind = "local"
                name = name[: -len(".client.lua")]
            else:
                name = name[: -len(".lua")]
            with open(full, "r", encoding="utf8") as fh:
                out.append((rel, name, kind, fh.read()))
    return out


def add_folder_tree(root_name, files, parent):
    """Build nested folders + scripts from a flat list of relative paths."""
    folders = {}

    def get_folder(parts):
        key = "/".join(parts)
        if key in folders:
            return folders[key]
        if not parts:
            return parent
        f = Instance("Folder", parts[-1])
        get_folder(parts[:-1]).add(f)
        folders[key] = f
        return f

    top = Instance("Folder", root_name)
    parent.add(top)
    folders[root_name] = top
    for rel, name, kind, source in files:
        parts = [root_name] + rel.split(os.sep)[:-1]
        parent_folder = get_folder(parts)
        cls = {"module": "ModuleScript", "script": "Script", "local": "LocalScript"}[kind]
        node = Instance(cls, name)
        node.props["Source"] = source
        parent_folder.add(node)


def build_ui_tree(parent):
    """Static GUI scaffolding that the client UI code populates."""
    return None


def make_services():
    services = {}
    for name in SERVICE_ORDER:
        services[name] = Instance(name, name)
    # StarterPlayer children
    sp = services["StarterPlayer"]
    sp.props.update({
        "CharacterWalkSpeed": 10.0,
        "CharacterJumpPower": 48.0,
        "CharacterUseJumpPower": True,
        "CameraMinZoomDistance": 0.5,
        "CameraMaxZoomDistance": 400.0,
        "EnableMouseLockOption": True,
        "LoadCharacterAppearance": False,
        "CharacterMaxSlopeAngle": 60.0,
    })
    services["Players"].props.update({
        "CharacterAutoLoads": True,
        "RespawnTime": 6.0,
        "MaxPlayersInternal": 24,
    })
    sps = Instance("StarterPlayerScripts", "StarterPlayerScripts")
    scs = Instance("StarterCharacterScripts", "StarterCharacterScripts")
    sp.add(sps, scs)
    services["_StarterPlayerScripts"] = sps
    services["_StarterCharacterScripts"] = scs

    ws = services["Workspace"]
    ws.props.update({
        "StreamingEnabled": True,
        "StreamingMinRadius": 384,
        "StreamingTargetRadius": 1400,
        "StreamingPauseMode": E("StreamingPauseMode", "Default"),
        "Gravity": 196.2,
        "FallHeightEnabled": True,
        "CollisionGroups": "Default",
        "GlobalWind": (4.0, 0.0, -2.0),
    })

    lighting = services["Lighting"]
    lighting.props.update({
        "Technology": E("Technology", "Future"),
        "Ambient": (0.055, 0.06, 0.075),
        "OutdoorAmbient": (0.30, 0.31, 0.34),
        "Brightness": 1.35,
        "EnvironmentDiffuseScale": 0.55,
        "EnvironmentSpecularScale": 0.55,
        "GlobalShadows": True,
        "ShadowSoftness": 0.32,
        "ExposureCompensation": -0.15,
        "ClockTime": 17.4,
        "GeographicLatitude": 51.0,
        "TimeOfDay": "17:24:00",
        "FogColor": (0.30, 0.28, 0.26),
        "FogStart": 140.0,
        "FogEnd": 1250.0,
        "ColorShift_Top": (0.0, 0.0, 0.0),
        "ColorShift_Bottom": (0.0, 0.0, 0.0),
        "Outlines": False,
    })
    lighting.add(
        Instance("Atmosphere", "Atmosphere", Density=0.42, Offset=0.15,
                 Color=(0.62, 0.60, 0.58), Decay=(0.42, 0.40, 0.39), Glare=0.24, Haze=2.2),
        Instance("Clouds", "Clouds", Cover=0.72, Density=0.55, Color=(0.42, 0.42, 0.45)),
        Instance("ColorCorrectionEffect", "Grade", Enabled=True,
                 Brightness=-0.03, Contrast=0.14, Saturation=-0.32, TintColor=(1.0, 0.96, 0.90)),
        Instance("BloomEffect", "Bloom", Enabled=True, Intensity=0.55, Size=28, Threshold=0.92),
        Instance("SunRaysEffect", "SunRays", Enabled=True, Intensity=0.035, Spread=0.55),
        Instance("DepthOfFieldEffect", "DOF", Enabled=True, FarIntensity=0.18, FocusDistance=32.0,
                 InFocusRadius=90.0, NearIntensity=0.05),
    )

    rs = services["ReplicatedStorage"]
    rs.add(Instance("Folder", "Remotes"))

    ss = services["ServerScriptService"]
    storage = services["ServerStorage"]
    storage.add(Instance("Folder", "Templates"), Instance("Folder", "EnemyTemplates"))

    snd = services["SoundService"]
    snd.props.update({"AmbientReverb": E("ReverbType", "Hallway"), "DistanceFactor": 3.33,
                      "DopplerScale": 1.0, "RolloffScale": 1.0})
    return services, None


def main():
    t0 = time.time()
    services, shared = make_services()

    shared_files = read_sources(os.path.join(SRC, "shared"))
    add_folder_tree("Shared", shared_files, services["ReplicatedStorage"])

    server_files = read_sources(os.path.join(SRC, "server"))
    add_folder_tree("Services", [f for f in server_files if f[2] == "module"], services["ServerScriptService"])
    for rel, name, kind, source in server_files:
        if kind == "script":
            node = Instance("Script", name)
            node.props["Source"] = source
            services["ServerScriptService"].add(node)

    client_files = read_sources(os.path.join(SRC, "client"))
    add_folder_tree("Client", client_files, services["_StarterPlayerScripts"])

    world_stats = worldgen.build(services["Workspace"], Instance, M, E)
    # stable container for player-built structures (BuildingService also creates
    # this at runtime if it is missing)
    ws = services["Workspace"]
    world_folder = next((c for c in ws.children if c.props.get("Name") == "World"), None)
    if world_folder is not None and not any(
            c.props.get("Name") == "Structures" for c in ws.children):
        ws.add(Instance("Folder", "Structures"))

    roots = [services[n] for n in SERVICE_ORDER if n in services]
    size, count, classes = binary.write_place(roots, OUT)
    dt = time.time() - t0
    print("built %s" % OUT)
    print("  instances : %d" % count)
    print("  classes   : %d" % classes)
    print("  size      : %.2f MB" % (size / 1e6))
    print("  parts     : %d" % world_stats.get("parts", 0))
    print("  time      : %.1fs" % dt)
    if binary.SKIPPED_PROPERTIES:
        print("  note: unsupported props skipped: %s" % ", ".join(sorted(binary.SKIPPED_PROPERTIES)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
