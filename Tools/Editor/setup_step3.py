"""Create Ninja input/Blueprints and a separate grey training map."""
import json
from pathlib import Path

import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
library = unreal.EditorAssetLibrary
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def asset(name, source):
    path = "/Game/Ninja/Input/" + name
    existing = unreal.load_asset(path) if library.does_asset_exist(path) else None
    return existing or library.duplicate_asset(source, path)


sprint = asset("IA_Sprint", "/Game/Input/Actions/IA_Jump")
sprint.set_editor_property("value_type", unreal.InputActionValueType.BOOLEAN)
mapping = asset("IMC_NinjaSprint", "/Game/Input/IMC_Default")
mapping.unmap_all()
for name in ("LeftShift", "RightShift"):
    key = unreal.Key()
    key.set_editor_property("key_name", name)
    mapping.map_key(sprint, key)
library.save_loaded_asset(sprint)
library.save_loaded_asset(mapping)

character_path = "/Game/Ninja/Blueprints/BP_NinjaCharacter"
if not library.does_asset_exist(character_path):
    library.duplicate_asset("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter", character_path)
character = unreal.load_asset(character_path)
unreal.BlueprintEditorLibrary.reparent_blueprint(character, unreal.NinjaCharacter)
if not unreal.BlueprintEditorLibrary.compile_blueprint(character):
    raise RuntimeError("Could not compile Ninja Blueprint")
character_class = library.load_blueprint_class(character_path)
defaults = unreal.get_default_object(character_class)
defaults.set_editor_property("sprint_action", sprint)
defaults.set_editor_property("sprint_mapping_context", mapping)
library.save_loaded_asset(character)

mode_path = "/Game/Ninja/Blueprints/BP_NinjaTrainingGameMode"
if not library.does_asset_exist(mode_path):
    library.duplicate_asset("/Game/ThirdPerson/Blueprints/BP_ThirdPersonGameMode", mode_path)
mode = unreal.load_asset(mode_path)
mode_class = library.load_blueprint_class(mode_path)
mode_defaults = unreal.get_default_object(mode_class)
mode_defaults.set_editor_property("default_pawn_class", character_class)
mode_defaults.set_editor_property("hud_class", unreal.NinjaTrainingHUD)
library.save_loaded_asset(mode)

MAP = "/Game/Ninja/Maps/L_NinjaTraining"
if library.does_asset_exist(MAP):
    if not levels.load_level(MAP):
        raise RuntimeError("Could not load training map")
else:
    if not levels.new_level(MAP):
        raise RuntimeError("Could not create training map")

cube = unreal.load_asset("/Engine/BasicShapes/Cube")
material = unreal.load_asset("/Game/LevelPrototyping/Materials/M_PrototypeGrid")


def find(label):
    return next((a for a in actors.get_all_level_actors() if a.get_actor_label() == label), None)


def block(label, position, scale):
    result = find(label) or actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*position))
    result.set_actor_label(label)
    result.set_actor_location(unreal.Vector(*position), False, False)
    result.static_mesh_component.set_static_mesh(cube)
    result.static_mesh_component.set_material(0, material)
    result.static_mesh_component.set_collision_profile_name("BlockAll")
    result.set_actor_scale3d(unreal.Vector(*scale))
    return result


block("Ninja_Ground_200m", (0, 0, -50), (200, 200, 1))
for height, y in ((10, -3000), (20, 0), (30, 3000)):
    block(f"Jump_Building_{height}m", (5000, y, height * 50), (10, 20, height))
    label = f"Building_Label_{height}m"
    text = find(label) or actors.spawn_actor_from_class(
        unreal.TextRenderActor, unreal.Vector(4440, y - 600, 400), unreal.Rotator(pitch=0, yaw=180, roll=0)
    )
    text.set_actor_label(label)
    text.set_actor_location(unreal.Vector(4440, y - 600, 400), False, False)
    text.set_actor_rotation(unreal.Rotator(pitch=0, yaw=180, roll=0), False)
    component = text.get_component_by_class(unreal.TextRenderComponent)
    component.set_text(f"{height} m")
    component.set_world_size(180)

if not find("Training_PlayerStart"):
    start = actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(0, -3000, 120))
    start.set_actor_label("Training_PlayerStart")
if not find("Training_Sun"):
    sun = actors.spawn_actor_from_class(
        unreal.DirectionalLight, unreal.Vector(0, 0, 15000), unreal.Rotator(pitch=-45, yaw=-35, roll=0)
    )
    sun.set_actor_label("Training_Sun")
    actors.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector())
    actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 10000))
if not find("Movement_Check_OnlyWithCommandFlag"):
    checker = actors.spawn_actor_from_class(unreal.NinjaMovementCheck, unreal.Vector())
    checker.set_actor_label("Movement_Check_OnlyWithCommandFlag")
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
world.get_world_settings().set_editor_property("default_game_mode", mode_class)
if not levels.save_current_level():
    raise RuntimeError("Could not save training map")
report = {"status": "passed", "map": MAP, "character": character_path, "mode": mode_path,
          "sprint_keys": ["LeftShift", "RightShift"], "jump_key": "SpaceBar (existing IMC_Default)",
          "building_heights_m": [10, 20, 30], "floor_size_m": 200,
          "timing_window_seconds": [-0.15, 0.15]}
path = ROOT / "Saved/Validation/step3-setup.json"
path.write_text(json.dumps(report, indent=2), encoding="utf-8")
unreal.log("STEP3_SETUP_PASSED")
