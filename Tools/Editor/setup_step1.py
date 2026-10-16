"""Run using UE's PythonScript commandlet; create the Step 1 test map once."""
import json
from pathlib import Path

import unreal


ROOT = Path(unreal.Paths.project_dir()).resolve()
MAP = "/Game/Ninja/Maps/L_TestArena"
REPORT = ROOT / "Saved/Validation/step1-map-setup.json"


def main():
    for directory in ("Blueprints", "Input", "Maps", "Materials", "UI"):
        unreal.EditorAssetLibrary.make_directory("/Game/Ninja/" + directory)

    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if unreal.EditorAssetLibrary.does_asset_exist(MAP):
        raise RuntimeError("Test map already exists; refusing to overwrite: " + MAP)
    if not levels.new_level(MAP):
        raise RuntimeError("Could not create test map")

    cube = unreal.load_asset("/Engine/BasicShapes/Cube")
    material = unreal.load_asset("/Game/LevelPrototyping/Materials/M_PrototypeGrid")
    if cube is None or material is None:
        raise RuntimeError("Missing installed cube or project prototype material")

    def block(label, position, scale):
        actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*position))
        actor.set_actor_label(label)
        component = actor.static_mesh_component
        component.set_static_mesh(cube)
        component.set_material(0, material)
        component.set_collision_profile_name("BlockAll")
        actor.set_actor_scale3d(unreal.Vector(*scale))
        return actor

    # UE cube has 100 cm sides; the ground's top is Z=0.
    block("Arena_Ground_100m", (0, 0, -50), (100, 100, 1))
    block("Step_50cm", (800, -600, 25), (3, 3, 0.5))
    block("Step_100cm", (1200, -600, 50), (3, 3, 1))
    block("Step_200cm", (1600, -600, 100), (3, 3, 2))
    block("Future_Jump_Target_10m", (2500, 1500, 500), (10, 10, 10))

    start = actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(0, 0, 120))
    start.set_actor_label("Ninja_Test_PlayerStart")
    light = actors.spawn_actor_from_class(
        unreal.DirectionalLight, unreal.Vector(0, 0, 1500), unreal.Rotator(-45, -30, 0)
    )
    light.set_actor_label("Arena_Sun")
    actors.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0))
    actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 1000))

    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    mode = unreal.load_class(None, "/Game/ThirdPerson/Blueprints/BP_ThirdPersonGameMode.BP_ThirdPersonGameMode_C")
    if mode is None:
        raise RuntimeError("Third Person GameMode is missing")
    world.get_world_settings().set_editor_property("default_game_mode", mode)
    if not levels.save_current_level():
        raise RuntimeError("Could not save test map")

    report = {
        "status": "passed",
        "map": MAP,
        "game_mode": mode.get_path_name(),
        "actors": sorted(a.get_actor_label() for a in actors.get_all_level_actors()),
        "ground_size_m": 100,
        "ground_top_z_cm": 0,
    }
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("STEP1_MAP_SETUP_PASSED " + str(REPORT))


main()
