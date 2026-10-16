"""Bake local collision assets and build the Ikebukuro play map."""
import json
import math
import heapq
from pathlib import Path

import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
CONFIG = json.loads((ROOT / "Tools/City/ikebukuro-area.json").read_text())
MAP = "/Game/Ninja/Maps/L_Ikebukuro"
library = unreal.EditorAssetLibrary
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
library.make_directory("/Game/Ninja/City")
if library.does_asset_exist(MAP):
    if not levels.load_level(MAP):
        raise RuntimeError("Could not load Ikebukuro map")
else:
    if not levels.new_level(MAP):
        raise RuntimeError("Could not create Ikebukuro map")


def find(label):
    return next((a for a in actors.get_all_level_actors() if a.get_actor_label() == label), None)


geo = find("Ikebukuro_FixedOrigin") or actors.spawn_actor_from_class(unreal.CesiumGeoreference, unreal.Vector())
geo.set_actor_label("Ikebukuro_FixedOrigin")
geo.set_origin_longitude_latitude_height(unreal.Vector(*CONFIG["origin_longitude_latitude_height"]))
minimum = CONFIG["play_min_cm"]
maximum = CONFIG["play_max_cm"]
margin = CONFIG["collision_margin_m"] * 100
for layer in ("Buildings", "Roads"):
    relative = f"CityData/Ikebukuro/{layer}/tileset.json"
    tile = find("City_" + layer) or actors.spawn_actor_from_class(unreal.LocalCityTileset, unreal.Vector())
    tile.set_actor_label("City_" + layer)
    tile.set_georeference(geo)
    tile.set_editor_property("dataset_relative_path", relative)
    tile.set_editor_property("create_tile_physics", False)
    tile.set_editor_property("enable_occlusion_culling", False)
    tile.configure_local_source()
    asset_path = f"/Game/Ninja/City/SM_{layer}Collision"
    mesh = unreal.CityGeometryTools.bake_layer(relative, asset_path, geo,
        unreal.Vector2D(minimum[0]-margin, minimum[1]-margin), unreal.Vector2D(maximum[0]+margin, maximum[1]+margin))
    if mesh is None:
        raise RuntimeError("Could not bake " + layer)
    proxy = find("City_" + layer + "Collision") or actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector())
    proxy.set_actor_label("City_" + layer + "Collision")
    proxy.static_mesh_component.set_static_mesh(None)
    proxy.static_mesh_component.set_static_mesh(mesh)
    proxy.static_mesh_component.set_visibility(False)
    proxy.static_mesh_component.set_cast_shadow(False)
    proxy.static_mesh_component.set_collision_profile_name("BlockAll")
    proxy.set_actor_hidden_in_game(True)
for actor in actors.get_all_level_actors():
    if isinstance(actor, unreal.CesiumGeoreference) and actor != geo:
        actors.destroy_actor(actor)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
world.get_world_settings().set_editor_property("enable_world_bounds_checks", False)
world.get_world_settings().set_editor_property("default_game_mode",
    library.load_blueprint_class("/Game/Ninja/Blueprints/BP_NinjaTrainingGameMode"))
if not levels.save_current_level():
    raise RuntimeError("Could not save map")
unreal.log("STEP4_COLLISION_BAKE_PASSED")

# Fill missing terrain with an explicit approximation derived from nearby roads.
# This is gameplay ground, not a surveyed DEM or a replacement for road geometry.
road_samples = json.loads((ROOT / "Saved/Validation/SM_RoadsCollision.json").read_text())["upward_surface_samples"]
roof_samples = json.loads((ROOT / "Saved/Validation/SM_BuildingsCollision.json").read_text())["upward_surface_samples"]
cells = {}
for p in road_samples:
    key = (round(p["x"]/500), round(p["y"]/500))
    if key not in cells or p["z"] < cells[key]["z"]:
        cells[key] = p
samples = list(cells.values())
if not samples:
    raise RuntimeError("No road height samples")


def ground_height(x, y):
    nearest = heapq.nsmallest(6, samples, key=lambda p: (p["x"]-x)**2 + (p["y"]-y)**2)
    weights = [1/max(10000, (p["x"]-x)**2 + (p["y"]-y)**2) for p in nearest]
    return sum(p["z"]*w for p,w in zip(nearest,weights))/sum(weights) - 30


step = CONFIG["ground_grid_m"] * 100
low = [minimum[0]-margin, minimum[1]-margin]
nx = math.ceil((maximum[0]+margin-low[0])/step)
ny = math.ceil((maximum[1]+margin-low[1])/step)
vertices = [unreal.Vector(low[0]+x*step,low[1]+y*step,ground_height(low[0]+x*step,low[1]+y*step))
            for y in range(ny+1) for x in range(nx+1)]
indices = []
for y in range(ny):
    for x in range(nx):
        a = y*(nx+1)+x
        b, c, d = a+1, a+nx+1, a+nx+2
        indices += [a,b,d,a,d,c]
ground = unreal.CityGeometryTools.build_surface_mesh("/Game/Ninja/City/SM_FillGround", vertices, indices)
if ground is None:
    raise RuntimeError("Could not build fill ground")
floor = find("City_FillGround") or actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector())
floor.set_actor_label("City_FillGround")
floor.static_mesh_component.set_static_mesh(None)
floor.static_mesh_component.set_static_mesh(ground)
material_path = "/Game/Ninja/City/M_CityGround"
ground_material = unreal.load_asset(material_path) if library.does_asset_exist(material_path) else None
if ground_material is None:
    ground_material = unreal.AssetToolsHelpers.get_asset_tools().create_asset("M_CityGround", "/Game/Ninja/City", unreal.Material, unreal.MaterialFactoryNew())
unreal.MaterialEditingLibrary.delete_all_material_expressions(ground_material)
texture = unreal.MaterialEditingLibrary.create_material_expression(ground_material, unreal.MaterialExpressionTextureSample)
texture.set_editor_property("texture", unreal.load_asset("/Game/LevelPrototyping/Textures/T_GridChecker_A"))
color = unreal.MaterialEditingLibrary.create_material_expression(ground_material, unreal.MaterialExpressionConstant3Vector)
color.set_editor_property("constant", unreal.LinearColor(0.22,0.25,0.28,1))
multiply = unreal.MaterialEditingLibrary.create_material_expression(ground_material, unreal.MaterialExpressionMultiply)
unreal.MaterialEditingLibrary.connect_material_expressions(texture,"R",multiply,"A")
unreal.MaterialEditingLibrary.connect_material_expressions(color,"",multiply,"B")
unreal.MaterialEditingLibrary.connect_material_property(multiply,"",unreal.MaterialProperty.MP_BASE_COLOR)
ground_material.set_editor_property("two_sided", True)
unreal.MaterialEditingLibrary.recompile_material(ground_material)
library.save_loaded_asset(ground_material)
floor.static_mesh_component.set_material(0, ground_material)
floor.static_mesh_component.set_collision_profile_name("BlockAll")
floor.static_mesh_component.set_reverse_culling(False)
floor.static_mesh_component.set_visibility(True)
floor.static_mesh_component.set_hidden_in_game(False)
floor.set_actor_hidden_in_game(False)


def inside(x, y, inset=500):
    return minimum[0]+inset<x<maximum[0]-inset and minimum[1]+inset<y<maximum[1]-inset


def surface(x, y):
    return unreal.CityGeometryTools.surface_point(world, unreal.Vector2D(x,y))


route = None
for p in sorted(samples,key=lambda p:(p["x"]-12000)**2+(p["y"]+5000)**2)[:120]:
    if not inside(p["x"],p["y"],2000):
        continue
    start = surface(p["x"],p["y"])
    if start.z < -10000 or start.z > ground_height(start.x,start.y)+150:
        continue
    for angle in range(0,360,15):
        dx,dy = math.cos(math.radians(angle))*3000,math.sin(math.radians(angle))*3000
        if not inside(start.x+dx,start.y+dy,1000):
            continue
        end = surface(start.x+dx,start.y+dy)
        if end.z < -10000 or end.z > ground_height(end.x,end.y)+150 or abs(end.z-start.z)>100:
            continue
        if unreal.CityGeometryTools.is_route_clear(world,start,end):
            route = start,end
            break
    if route:
        break
if route is None:
    raise RuntimeError("Could not find a clear 30m street route")
start,end = route
roofs = []
for p in sorted(roof_samples,key=lambda p:-p["area_m2"]):
    if p["area_m2"] < 20 or not inside(p["x"],p["y"],1500):
        continue
    if not 400 < p["z"]-ground_height(p["x"],p["y"]) < 2600:
        continue
    point = surface(p["x"],p["y"])
    if abs(point.z-p["z"])>5:
        continue
    if any((point.x-q.x)**2+(point.y-q.y)**2<3000**2 for q in roofs):
        continue
    roofs.append(point)
    if len(roofs)==3:
        break
if len(roofs)!=3:
    raise RuntimeError("Could not choose three accessible real roofs")

area = find("Ikebukuro_PlayArea") or actors.spawn_actor_from_class(unreal.CityPlayArea, unreal.Vector())
area.set_actor_label("Ikebukuro_PlayArea")
area.set_editor_property("play_minimum", unreal.Vector2D(*minimum))
area.set_editor_property("play_maximum", unreal.Vector2D(*maximum))
area.set_editor_property("safe_street", start)
area.set_editor_property("street_run_end", end)
area.set_editor_property("roof_targets", roofs)
player = find("Ikebukuro_PlayerStart") or actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector())
player.set_actor_label("Ikebukuro_PlayerStart")
player.set_actor_location(start+unreal.Vector(0,0,130),False,False)
yaw = math.degrees(math.atan2(end.y-start.y,end.x-start.x))
player.set_actor_rotation(unreal.Rotator(pitch=0,yaw=yaw,roll=0),False)

cube = unreal.load_asset("/Engine/BasicShapes/Cube")
marker_material = unreal.load_asset("/Game/LevelPrototyping/Materials/MI_PrototypeGrid_TopDark")
for i,point in enumerate(roofs):
    marker = find(f"Roof_{i+1}_Marker") or actors.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector())
    marker.set_actor_label(f"Roof_{i+1}_Marker")
    marker.set_actor_location(point+unreal.Vector(0,0,100),False,False)
    marker.static_mesh_component.set_static_mesh(cube)
    marker.static_mesh_component.set_material(0,marker_material)
    marker.static_mesh_component.set_collision_profile_name("NoCollision")
    marker.set_actor_scale3d(unreal.Vector(0.4,0.4,2))

for i in range(9):
    for edge in range(4):
        x = minimum[0]+(maximum[0]-minimum[0])*i/8 if edge<2 else (minimum[0] if edge==2 else maximum[0])
        y = (minimum[1] if edge==0 else maximum[1]) if edge<2 else minimum[1]+(maximum[1]-minimum[1])*i/8
        marker = find(f"Boundary_{edge}_{i}") or actors.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector())
        marker.set_actor_label(f"Boundary_{edge}_{i}")
        marker.set_actor_location(unreal.Vector(x,y,ground_height(x,y)+100),False,False)
        marker.static_mesh_component.set_static_mesh(cube)
        marker.static_mesh_component.set_material(0,marker_material)
        marker.static_mesh_component.set_collision_profile_name("NoCollision")
        marker.set_actor_scale3d(unreal.Vector(0.3,0.3,2))
if not find("Ikebukuro_Sun"):
    sun = actors.spawn_actor_from_class(unreal.DirectionalLight,unreal.Vector(0,0,50000),unreal.Rotator(pitch=-45,yaw=-35,roll=0))
    sun.set_actor_label("Ikebukuro_Sun")
    actors.spawn_actor_from_class(unreal.SkyAtmosphere,unreal.Vector())
    actors.spawn_actor_from_class(unreal.SkyLight,unreal.Vector(0,0,20000))
for actor in actors.get_all_level_actors():
    if isinstance(actor, unreal.SkyLight):
        actor.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
        actor.light_component.set_real_time_capture(True)
        actor.light_component.set_intensity(1.0)
if not levels.save_current_level():
    raise RuntimeError("Could not save completed map")
report = {"status":"passed","map":MAP,"config":CONFIG,
          "safe_street":[start.x,start.y,start.z],"street_run_end":[end.x,end.y,end.z],
          "roof_targets":[[p.x,p.y,p.z] for p in roofs],
          "ground_vertices":len(vertices),"ground_triangles":len(indices)//3,
          "ground_method":"inverse-distance nearby road heights, 10m grid, 30cm below roads; approximate fill terrain",
          "tile_physics":False,"fixed_collision_assets":["SM_BuildingsCollision","SM_RoadsCollision","SM_FillGround"]}
(ROOT/"Saved/Validation/step4-setup.json").write_text(json.dumps(report,indent=2),encoding="utf-8")
unreal.log("STEP4_MAP_SETUP_PASSED")
