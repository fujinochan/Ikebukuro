"""Create a local 3D Tiles proof map, without online terrain or ion assets."""
import json
from pathlib import Path

import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
MAP = "/Game/Ninja/Maps/L_CityProof"
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
existing = unreal.EditorAssetLibrary.does_asset_exist(MAP)
if existing:
    if not levels.load_level(MAP):
        raise RuntimeError("Could not load existing proof map")
else:
    if not levels.new_level(MAP):
        raise RuntimeError("Could not create proof map")

geo = next((a for a in actors.get_all_level_actors() if isinstance(a, unreal.CesiumGeoreference)), None)
geo = geo or actors.spawn_actor_from_class(unreal.CesiumGeoreference, unreal.Vector())
geo.set_actor_label("Ikebukuro_FixedOrigin")
# Approximate station area; the final play boundary is a later design step.
geo.set_origin_longitude_latitude_height(unreal.Vector(139.7109, 35.7295, 60.0))
city = next((a for a in actors.get_all_level_actors() if isinstance(a, unreal.LocalCityTileset)), None)
city = city or actors.spawn_actor_from_class(unreal.LocalCityTileset, unreal.Vector())
city.set_actor_label("Toshima_Local_LOD1")
city.set_georeference(geo)
city.set_editor_property("dataset_relative_path", "CityData/ToshimaLOD1/tileset-fixed.json")
city.set_editor_property("enable_occlusion_culling", False)
city.configure_local_source()
for actor in actors.get_all_level_actors():
    if isinstance(actor, unreal.CesiumGeoreference) and actor != geo:
        actors.destroy_actor(actor)

cube = unreal.load_asset("/Engine/BasicShapes/Cube")
material = unreal.load_asset("/Game/LevelPrototyping/Materials/M_PrototypeGrid")


def block(name, location, scale):
    found = next((a for a in actors.get_all_level_actors() if a.get_actor_label() == name), None)
    if found:
        return found
    actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*location))
    actor.set_actor_label(name)
    actor.static_mesh_component.set_static_mesh(cube)
    actor.static_mesh_component.set_material(0, material)
    actor.static_mesh_component.set_collision_profile_name("BlockAll")
    actor.set_actor_scale3d(unreal.Vector(*scale))
    return actor


block("Temporary_Ground_Not_Surveyed", (0, 0, -1050), (1600, 1600, 1))
block("Observation_Deck_20m", (0, 20000, 8950), (20, 20, 1))
block("Calibration_Cube_1m", (500, 20500, 9050), (1, 1, 1))
block("Calibration_Beam_10m", (-500, 20500, 9050), (10, 1, 1))
start = next((a for a in actors.get_all_level_actors() if isinstance(a, unreal.PlayerStart)), None)
start = start or actors.spawn_actor_from_class(
    unreal.PlayerStart, unreal.Vector(0, 20000, 9220), unreal.Rotator(0, -90, 0)
)
start.set_actor_label("Observation_PlayerStart")
if not existing:
    actors.spawn_actor_from_class(
        unreal.DirectionalLight, unreal.Vector(0, 0, 60000), unreal.Rotator(-50, -35, 0)
    )
    actors.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector())
    actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 60000))
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
world.get_world_settings().set_editor_property(
    "default_game_mode",
    unreal.load_class(None, "/Game/ThirdPerson/Blueprints/BP_ThirdPersonGameMode.BP_ThirdPersonGameMode_C")
)
world.get_world_settings().set_editor_property("enable_world_bounds_checks", False)
if not levels.save_current_level():
    raise RuntimeError("Could not save proof map")
origin = unreal.Vector(139.7109, 35.7295, 60)
base = geo.transform_longitude_latitude_height_position_to_unreal(origin)
up = geo.transform_longitude_latitude_height_position_to_unreal(origin + unreal.Vector(0, 0, 1))
east = geo.transform_longitude_latitude_height_position_to_unreal(origin + unreal.Vector(0.00001, 0, 0))
north = geo.transform_longitude_latitude_height_position_to_unreal(origin + unreal.Vector(0, 0.00001, 0))
report = {"status": "passed", "map": MAP, "origin": [139.7109, 35.7295, 60],
          "one_metre_up_cm": [up.x - base.x, up.y - base.y, up.z - base.z],
          "east_0_00001_degrees_cm": [east.x-base.x, east.y-base.y, east.z-base.z],
          "north_0_00001_degrees_cm": [north.x-base.x, north.y-base.y, north.z-base.z],
          "local_source": city.get_editor_property("url"), "actors": [a.get_actor_label() for a in actors.get_all_level_actors()]}
path = ROOT / "Saved/Validation/step2-map-setup.json"
path.parent.mkdir(parents=True, exist_ok=True)
path.write_text(json.dumps(report, indent=2), encoding="utf-8")
unreal.log("STEP2_MAP_SETUP_PASSED")
