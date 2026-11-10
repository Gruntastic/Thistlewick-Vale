"""Create placeholder maps so the project runs before the Environment Artist delivers.

Creates under /Game/MMO/Maps:
  L_Entry       empty front-end map (TVEntryGameMode via World Settings and the L_Entry map prefix in DefaultEngine.ini)
  L_Thistlewick flat 1 km ground, PlayerStart tagged "BindStone" (Lanternrest bind stone), sun, sky, fog,
                nav bounds, and a dungeon entrance portal 30 m east of spawn (for the smoke test)
  L_Barrow      40 m test floor, TargetPoint tagged "TV_DungeonEntrance", exit portal, point lights, nav bounds.
                No directional/sky light: it is streamed into the overworld as a level instance.

Smoke-test content (Tests/Net/README.md), until the Environment Artist places the real thing:
  L_Thistlewick NPC_ELDER_WREN 7 m from spawn (quest Q01), six NODE_GLOWMOSS nodes 10 m apart west of spawn, and a
                MOB_MOSSBACK_SCRAPPER spawn point (3 mobs) 57 m south-west, beyond aggro range of the bind stone.
  L_Barrow      MOB_HOLLOWED_FOOTMAN_DGN and MOB_BARROW_WISP_DGN spawn points (2 each) 29 m from the entrance.

Existing maps are left alone unless --force is given (then they are deleted and rebuilt).

Run (editor Python, after the C++ module is built so TVDungeonPortal / TVEntryGameMode exist):
  UnrealEditor.exe <path>\\ThistlewickVale.uproject -ExecutePythonScript="<path>\\Tools\\ue\\create_maps.py"
or in the editor Output Log (Python mode):
  py "<path>/Tools/ue/create_maps.py" --force

Owner: Network & Dungeon Engineer. Uses only LevelEditorSubsystem / EditorActorSubsystem / EditorAssetLibrary.
Run headless against the 5.8.4 editor (UnrealEditor-Cmd -run=pythonscript) on 2026-11-10.
"""
import sys

import unreal

MAP_DIR = "/Game/MMO/Maps"
CUBE = "/Engine/BasicShapes/Cube.Cube"
FORCE = "--force" in sys.argv

level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def log(msg):
    unreal.log("[create_maps] " + msg)


def load_cpp_class(name):
    cls = unreal.load_class(None, "/Script/ThistlewickVale." + name)
    if cls is None:
        unreal.log_warning("[create_maps] C++ class %s not found (build the editor target first); skipping it" % name)
    return cls


def spawn(cls, location=(0, 0, 0), rotation=(0, 0, 0), label=None):
    actor = actors.spawn_actor_from_class(
        cls, unreal.Vector(*location), unreal.Rotator(roll=rotation[0], pitch=rotation[1], yaw=rotation[2])
    )
    if actor and label:
        actor.set_actor_label(label)
    return actor


def add_floor(size_m, label, z_top=0.0):
    """Cube scaled to size_m x size_m x 1 m whose top surface is at z_top (cube is 100 cm, centered)."""
    floor = spawn(unreal.StaticMeshActor, (0, 0, z_top - 50.0), label=label)
    floor.static_mesh_component.set_static_mesh(unreal.load_asset(CUBE))
    floor.set_actor_scale3d(unreal.Vector(size_m, size_m, 1.0))
    return floor


def add_nav_bounds(size_m, height_m, label):
    nav = spawn(unreal.NavMeshBoundsVolume, (0, 0, 0), label=label)
    # Default brush is 200 cm on a side.
    nav.set_actor_scale3d(unreal.Vector(size_m / 2.0, size_m / 2.0, height_m / 2.0))
    return nav


def new_map(name):
    path = MAP_DIR + "/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        if not FORCE:
            log("%s exists, skipping (use --force to rebuild)" % path)
            return False
        # Rebuild in place: deleting a map package from the pythonscript commandlet leaves the .umap on disk (5.8),
        # after which new_level refuses the path. Clearing the actors also keeps references to the map valid.
        if not level_editor.load_level(path):
            unreal.log_error("[create_maps] could not load " + path)
            return False
        # Keep World Settings and the builder brush (exact class Brush; volumes are Brush subclasses and go).
        doomed = [a for a in actors.get_all_level_actors()
                  if not isinstance(a, unreal.WorldSettings) and a.get_class() != unreal.Brush.static_class()]
        actors.destroy_actors(doomed)
        log("cleared %d actors from %s" % (len(doomed), path))
        return True
    if not level_editor.new_level(path):
        unreal.log_error("[create_maps] could not create " + path)
        return False
    log("created " + path)
    return True


def save():
    level_editor.save_current_level()


# Smoke-test layout, shared with Tests/Net/smoke/*.txt (keep the coordinates in sync).
NPC_WREN_LOCATION = (500, 500, 0)
GLOWMOSS_LOCATIONS = [(-1000 - 1000 * i, 1500, 0) for i in range(6)]
SCRAPPER_SPAWN_LOCATION = (-4000, -4000, 100)


def add_spawn_point(mob_id, count, location, label):
    cls = load_cpp_class("TVSpawnPoint")
    if not cls:
        return None
    point = spawn(cls, location, label=label)
    point.set_editor_property("mob_id", unreal.Name(mob_id))
    point.set_editor_property("count", count)
    point.set_editor_property("respawn_time", 30.0)
    return point


def add_zone_test_content():
    npc_class = load_cpp_class("TVNpc")
    if npc_class:
        npc = spawn(npc_class, NPC_WREN_LOCATION, (0, 0, -135), label="NPC_ElderWren")
        npc.set_editor_property("npc_id", unreal.Name("NPC_ELDER_WREN"))
    node_class = load_cpp_class("TVQuestNode")
    if node_class:
        for index, location in enumerate(GLOWMOSS_LOCATIONS):
            node = spawn(node_class, location, label="Glowmoss_%d" % index)
            node.set_editor_property("node_id", unreal.Name("NODE_GLOWMOSS"))
            node.set_editor_property("kind", unreal.TVQuestNodeKind.COLLECT)
            node.set_editor_property("mesh_id", unreal.Name("ENV_GLOWMOSS_CLUMP"))
    add_spawn_point("MOB_MOSSBACK_SCRAPPER", 3, SCRAPPER_SPAWN_LOCATION, "Spawn_Scrappers")


def build_entry():
    if not new_map("L_Entry"):
        return
    entry_mode = load_cpp_class("TVEntryGameMode")
    if entry_mode:
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        world.get_world_settings().set_editor_property("default_game_mode", entry_mode)
    spawn(unreal.CameraActor, (0, 0, 200), label="EntryCamera")
    save()


def build_thistlewick():
    if not new_map("L_Thistlewick"):
        return
    add_floor(1000.0, "Ground_1km")

    start = spawn(unreal.PlayerStart, (0, 0, 120), label="BindStone_Lanternrest")
    start.set_editor_property("player_start_tag", "BindStone")

    sun = spawn(unreal.DirectionalLight, (0, 0, 1000), (0, -45, 30), label="Sun")
    sun.light_component.set_editor_property("intensity", 8.0)
    sun.light_component.set_editor_property("atmosphere_sun_light", True)

    sky_light = spawn(unreal.SkyLight, (0, 0, 500), label="SkyLight")
    sky_light.light_component.set_editor_property("real_time_capture", True)
    spawn(unreal.SkyAtmosphere, (0, 0, 0), label="SkyAtmosphere")
    spawn(unreal.ExponentialHeightFog, (0, 0, 0), label="HeightFog")
    spawn(unreal.VolumetricCloud, (0, 0, 0), label="Clouds")

    add_nav_bounds(1000.0, 100.0, "NavBounds_Zone")
    add_zone_test_content()

    portal_class = load_cpp_class("TVDungeonPortal")
    if portal_class:
        # Placeholder Barrow entrance near spawn so the smoke test can walk into it; Env Artist moves it to the Ruined Hills.
        portal = spawn(portal_class, (3000, 0, 200), (0, 0, 180), label="BarrowEntrancePortal")
        portal.set_editor_property("is_exit", False)
    save()


def build_barrow():
    if not new_map("L_Barrow"):
        return
    add_floor(40.0, "Barrow_TestFloor")
    # Ceiling-less test room walls are left to the Environment Artist.

    entrance = spawn(unreal.TargetPoint, (0, -1500, 120), (0, 0, 90), label="DungeonEntrance")
    entrance.set_editor_property("tags", [unreal.Name("TV_DungeonEntrance")])

    portal_class = load_cpp_class("TVDungeonPortal")
    if portal_class:
        exit_portal = spawn(portal_class, (0, -1850, 200), (0, 0, 90), label="BarrowExitPortal")
        exit_portal.set_editor_property("is_exit", True)

    for index, (x, y) in enumerate([(-1200, -1200), (1200, -1200), (-1200, 1200), (1200, 1200), (0, 0)]):
        light = spawn(unreal.PointLight, (x, y, 600), label="BarrowLight_%d" % index)
        light.light_component.set_editor_property("intensity", 20000.0)
        light.light_component.set_editor_property("attenuation_radius", 2500.0)
        light.light_component.set_editor_property("light_color", unreal.Color(r=255, g=170, b=90, a=255))

    add_nav_bounds(40.0, 20.0, "NavBounds_Barrow")
    add_spawn_point("MOB_HOLLOWED_FOOTMAN_DGN", 2, (-800, 1400, 100), "Spawn_Footmen")
    add_spawn_point("MOB_BARROW_WISP_DGN", 2, (800, 1400, 100), "Spawn_Wisps")
    save()


def main():
    unreal.EditorAssetLibrary.make_directory(MAP_DIR)
    build_entry()
    build_thistlewick()
    build_barrow()
    # Leave the editor on the zone map.
    if unreal.EditorAssetLibrary.does_asset_exist(MAP_DIR + "/L_Thistlewick"):
        level_editor.load_level(MAP_DIR + "/L_Thistlewick")
    log("done")


main()
