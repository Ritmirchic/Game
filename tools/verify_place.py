#!/usr/bin/env python3
"""Static verification of the built place file.

Reads ASHES_OF_HUMANITY.rbxl back through our own parser and asserts that the
expected service tree, scripts and world survive the round trip. Run after
tools/build.py.

    python3 tools/verify_place.py
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from rbx import binary  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PLACE = os.path.join(ROOT, "ASHES_OF_HUMANITY.rbxl")

EXPECTED_SERVICES = [
    "Workspace", "Lighting", "ReplicatedStorage", "ReplicatedFirst",
    "ServerScriptService", "ServerStorage", "StarterGui", "StarterPack",
    "StarterPlayer", "SoundService", "Players", "MaterialService",
]

SERVER_SERVICES = [
    "Remotes", "PlayerDataService", "InventoryService", "SurvivalService",
    "LootService", "InteractionService", "CombatService", "EnemyService",
    "CraftingService", "BuildingService", "QuestService", "PartyService",
    "WeatherService", "WorldEventService", "StoryService", "DirectorService",
    "ConfigService",
]

SHARED_MODULES = [
    "Config", "Util", "ItemDefinitions", "WeaponDefinitions", "EnemyDefinitions",
    "LootTables", "Recipes", "BuildDefinitions", "QuestDefinitions", "StoryData",
    "Remotes", "InventoryModel", "SurvivalMath", "CombatMath", "LootRoller",
    "QuestLogic", "CraftLogic", "BuildLogic", "EnemyAI", "Profile",
]

CLIENT_MODULES = [
    "UITheme", "UIBuilder", "StateStore", "Network", "HUD", "UIManager",
    "InventoryUI", "CraftingUI", "MedicalUI", "MapUI", "JournalUI",
    "BuildingUI", "SettingsUI", "MainMenuUI", "DeathUI",
    "CameraController", "MovementController", "CombatController",
    "InteractionController", "AtmosphereController", "AudioController",
]

REGIONS = ["Suburbs", "Forest", "Industrial", "City", "Military", "Wasteland"]


def main():
    if not os.path.exists(PLACE):
        print("FAIL: place file missing")
        return 1
    roots = binary.read_file(PLACE)
    root = binary.Instance("DataModel")
    root.children = roots
    for child in roots:
        child.parent = root
    failures = []
    checks = 0

    def find(node, name):
        for child in node.children:
            if child.props.get("Name") == name:
                return child
        return None

    def require(condition, message):
        nonlocal checks
        checks += 1
        if not condition:
            failures.append(message)

    for service in EXPECTED_SERVICES:
        require(find(root, service) is not None, "missing service %s" % service)

    ss = find(root, "ServerScriptService")
    require(ss is not None, "no ServerScriptService")
    if ss:
        require(find(ss, "Bootstrap") is not None, "missing Bootstrap server script")
        services = find(ss, "Services")
        require(services is not None, "missing Services folder")
        if services:
            names = {c.props.get("Name") for c in services.children}
            for name in SERVER_SERVICES:
                require(name in names, "missing server service %s" % name)
            require(len(services.children) == len(SERVER_SERVICES),
                    "unexpected server service count %d" % len(services.children))
            for child in services.children:
                src = child.props.get("Source", "")
                label = child.props.get("Name")
                require(bool(src.strip()), "empty source in service %s" % label)
                require("return " in src[-200:], "service %s does not return a module table" % label)

    rs = find(root, "ReplicatedStorage")
    require(rs is not None, "no ReplicatedStorage")
    if rs:
        shared = find(rs, "Shared")
        require(shared is not None, "missing Shared folder")
        if shared:
            names = {c.props.get("Name") for c in shared.children}
            for name in SHARED_MODULES:
                require(name in names, "missing shared module %s" % name)
            for child in shared.children:
                require(bool(child.props.get("Source", "").strip()),
                        "empty source in shared module %s" % child.props.get("Name"))
        remotes = find(rs, "Remotes")
        require(remotes is not None, "missing Remotes folder")
        if remotes:
            # the server populates this folder at Init with RemoteEvents and
            # RemoteFunctions declared in Shared/Remotes; an empty folder in the
            # saved place is expected
            for child in remotes.children:
                require(child.cls in ("RemoteEvent", "RemoteFunction"),
                        "bad remote class %s" % child.cls)

    sp = find(root, "StarterPlayer")
    require(sp is not None, "no StarterPlayer")
    sps = find(sp, "StarterPlayerScripts") if sp else None
    require(sps is not None, "no StarterPlayerScripts")
    if sps:
        client = find(sps, "Client")
        require(client is not None, "missing Client folder")
        if client:
            names = {c.props.get("Name") for c in client.children}
            for name in CLIENT_MODULES:
                require(name in names, "missing client module %s" % name)
            main_script = find(client, "Main")
            require(main_script is not None and main_script.cls == "LocalScript",
                    "Main must be a LocalScript")

    ws = find(root, "Workspace")
    require(ws is not None, "no Workspace")
    if ws:
        require(ws.props.get("StreamingEnabled") is True, "StreamingEnabled must be on")
        world = find(ws, "World")
        require(world is not None, "missing World folder")
        if world:
            regions = find(world, "Regions") or world
            for region in REGIONS:
                require(find(regions, region) is not None, "missing region %s" % region)
            anchor_count = 0
            stack = [world]
            while stack:
                node = stack.pop()
                if node.props.get("Name") == "EnemySpawn":
                    anchor_count += 1
                stack.extend(node.children)
            require(anchor_count >= 30, "too few enemy spawn anchors: %d" % anchor_count)
            print("spawn anchors: %d" % anchor_count)
        structures = find(ws, "Structures")
        require(structures is not None, "missing Structures folder")
        spawn = False
        stack = [ws]
        while stack:
            node = stack.pop()
            if node.cls == "SpawnLocation":
                spawn = True
                break
            stack.extend(node.children)
        require(spawn, "no SpawnLocation in Workspace")

    lighting = find(root, "Lighting")
    require(lighting is not None, "no Lighting")
    if lighting:
        names = {c.cls for c in lighting.children}
        for cls in ("Atmosphere", "ColorCorrectionEffect", "BloomEffect",
                    "DepthOfFieldEffect", "SunRaysEffect"):
            require(cls in names, "missing lighting effect %s" % cls)
        require(lighting.props.get("Technology") is not None, "Lighting.Technology unset")

    # count scripts
    total_scripts = 0
    total_parts = 0
    stack = [root]
    while stack:
        node = stack.pop()
        if node.cls in ("Script", "LocalScript", "ModuleScript"):
            total_scripts += 1
        if node.cls == "Part":
            total_parts += 1
        stack.extend(node.children)

    require(total_scripts >= 60, "too few scripts: %d" % total_scripts)
    require(total_parts >= 10000, "too few parts: %d" % total_parts)

    print("place        : %s" % PLACE)
    print("roots        : %d" % len(root.children))
    print("scripts      : %d" % total_scripts)
    print("parts        : %d" % total_parts)
    print("checks       : %d" % checks)
    if failures:
        print("FAILURES     : %d" % len(failures))
        for failure in failures[:40]:
            print("  - %s" % failure)
        return 1
    print("result       : ALL CHECKS PASSED")
    return 0


if __name__ == "__main__":
    sys.exit(main())
