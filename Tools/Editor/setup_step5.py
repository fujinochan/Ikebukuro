"""Create local Ninja appearances, throw input, combat arena and one city target."""
import json
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
lib = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
lib.make_directory('/Game/Ninja/Materials')
for name, color, metal in [
    ('Black',(0.006,0.008,0.012),0), ('Red',(0.55,0.018,0.025),0),
    ('Mask',(0.006,0.007,0.009),0), ('Eyes',(0.9,0.65,0.14),0), ('Steel',(0.4,0.45,0.52),0.8)]:
    path = '/Game/Ninja/Materials/M_Ninja'+name
    mat = unreal.load_asset(path) if lib.does_asset_exist(path) else tools.create_asset('M_Ninja'+name,'/Game/Ninja/Materials',unreal.Material,unreal.MaterialFactoryNew())
    edit = unreal.MaterialEditingLibrary
    edit.set_material_usage(mat,unreal.MaterialUsage.MATUSAGE_SKELETAL_MESH)
    edit.delete_all_material_expressions(mat)
    value = edit.create_material_expression(mat,unreal.MaterialExpressionConstant3Vector)
    value.set_editor_property('constant',unreal.LinearColor(*color,1))
    edit.connect_material_property(value,'',unreal.MaterialProperty.MP_BASE_COLOR)
    for number, prop in [(0.35 if metal else 0.94,unreal.MaterialProperty.MP_ROUGHNESS),(metal,unreal.MaterialProperty.MP_METALLIC),(0.5 if metal else 0.08,unreal.MaterialProperty.MP_SPECULAR)]:
        node = edit.create_material_expression(mat,unreal.MaterialExpressionConstant)
        node.set_editor_property('r',number)
        edit.connect_material_property(node,'',prop)
    edit.recompile_material(mat)
    lib.save_loaded_asset(mat,False)

throw_path='/Game/Ninja/Input/IA_Throw'
throw = unreal.load_asset(throw_path) if lib.does_asset_exist(throw_path) else lib.duplicate_asset('/Game/Input/Actions/IA_Jump',throw_path)
throw.set_editor_property('value_type',unreal.InputActionValueType.BOOLEAN)
mapping=unreal.load_asset('/Game/Ninja/Input/IMC_NinjaSprint')
mapping.unmap_all()
for name in ('LeftShift','RightShift'):
    key=unreal.Key(); key.set_editor_property('key_name',name)
    mapping.map_key(unreal.load_asset('/Game/Ninja/Input/IA_Sprint'),key)
for name in ('F','LeftMouseButton'):
    key=unreal.Key(); key.set_editor_property('key_name',name)
    mapping.map_key(throw,key)
lib.save_loaded_asset(throw,False); lib.save_loaded_asset(mapping,False)
player_bp=unreal.load_asset('/Game/Ninja/Blueprints/BP_NinjaCharacter')
player_class=lib.load_blueprint_class('/Game/Ninja/Blueprints/BP_NinjaCharacter')
player=unreal.get_default_object(player_class)
player.set_editor_property('throw_action',throw)
if not unreal.BlueprintEditorLibrary.compile_blueprint(player_bp): raise RuntimeError('Player Blueprint compile failed')
unreal.get_default_object(lib.load_blueprint_class('/Game/Ninja/Blueprints/BP_NinjaCharacter')).set_editor_property('throw_action',throw)
lib.save_loaded_asset(player_bp,False)

enemy_path='/Game/Ninja/Blueprints/BP_RedNinja'
if not lib.does_asset_exist(enemy_path):
    factory=unreal.BlueprintFactory(); factory.set_editor_property('parent_class',unreal.RedNinja)
    tools.create_asset('BP_RedNinja','/Game/Ninja/Blueprints',unreal.Blueprint,factory)
enemy_bp=unreal.load_asset(enemy_path)
enemy_class=lib.load_blueprint_class(enemy_path)
enemy=unreal.get_default_object(enemy_class)
source_mesh=player.get_component_by_class(unreal.SkeletalMeshComponent)
enemy_mesh=enemy.get_component_by_class(unreal.SkeletalMeshComponent)
enemy_mesh.set_skeletal_mesh_asset(source_mesh.get_skeletal_mesh_asset())
enemy_mesh.set_anim_instance_class(source_mesh.get_editor_property('anim_class'))
enemy_mesh.set_relative_transform(source_mesh.get_relative_transform(),False,False)
enemy_mesh.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
if not unreal.BlueprintEditorLibrary.compile_blueprint(enemy_bp): raise RuntimeError('Enemy Blueprint compile failed')
lib.save_loaded_asset(enemy_bp,False)

def find(label):
    return next((a for a in actors.get_all_level_actors() if a.get_actor_label()==label),None)

def target(label,point):
    actor=find(label) or actors.spawn_actor_from_class(enemy_class,point)
    actor.set_actor_label(label); actor.set_actor_location(point,False,False)
    actor.set_actor_rotation(unreal.Rotator(pitch=0,yaw=180,roll=0),False)
    return actor

MAP='/Game/Ninja/Maps/L_NinjaCombat'
if lib.does_asset_exist(MAP): levels.load_level(MAP)
else: levels.new_level(MAP)
floor=find('Combat_Ground') or actors.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(0,0,-50))
floor.set_actor_label('Combat_Ground')
floor.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'))
floor.static_mesh_component.set_material(0,unreal.load_asset('/Game/LevelPrototyping/Materials/MI_PrototypeGrid_Gray'))
floor.static_mesh_component.set_collision_profile_name('BlockAll')
floor.set_actor_scale3d(unreal.Vector(200,200,1))
if not find('Combat_PlayerStart'):
    actor=actors.spawn_actor_from_class(unreal.PlayerStart,unreal.Vector(0,0,110)); actor.set_actor_label('Combat_PlayerStart')
for i,p in enumerate(((1100,0,92),(1700,600,92),(2300,-600,92))): target('Practice_RedNinja_'+str(i+1),unreal.Vector(*p))
if not find('Combat_Sun'):
    sun=actors.spawn_actor_from_class(unreal.DirectionalLight,unreal.Vector(0,0,15000),unreal.Rotator(pitch=-45,yaw=-35,roll=0)); sun.set_actor_label('Combat_Sun')
    actors.spawn_actor_from_class(unreal.SkyAtmosphere,unreal.Vector())
    sky=actors.spawn_actor_from_class(unreal.SkyLight,unreal.Vector(0,0,10000))
    sky.light_component.set_mobility(unreal.ComponentMobility.MOVABLE); sky.light_component.set_real_time_capture(True)
if not find('Combat_Check_OnlyWithCommandFlag'):
    checker=actors.spawn_actor_from_class(unreal.NinjaCombatCheck,unreal.Vector()); checker.set_actor_label('Combat_Check_OnlyWithCommandFlag')
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
world.get_world_settings().set_editor_property('default_game_mode',lib.load_blueprint_class('/Game/Ninja/Blueprints/BP_NinjaTrainingGameMode'))
if not levels.save_current_level(): raise RuntimeError('Combat map save failed')

levels.load_level('/Game/Ninja/Maps/L_Ikebukuro')
area=find('Ikebukuro_PlayArea')
start=area.get_editor_property('safe_street'); end=area.get_editor_property('street_run_end')
direction=end-start
point=start+direction*(2400/(direction.x**2+direction.y**2+direction.z**2)**0.5)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
# Step 4 verified this complete 30m street route. Let CharacterMovement settle
# the target onto the saved collision at runtime; editor commandlet loading can
# still be compiling these meshes, so it is not a reliable placement trace.
point=point+unreal.Vector(0,0,110)
target('City_RedNinja_Step5',point)
if not find('City_Combat_Check_OnlyWithCommandFlag'):
    checker=actors.spawn_actor_from_class(unreal.NinjaCombatCheck,unreal.Vector()); checker.set_actor_label('City_Combat_Check_OnlyWithCommandFlag')
if not levels.save_current_level(): raise RuntimeError('City map save failed')
report={'status':'passed','combat_map':MAP,'city_enemy':[point.x,point.y,point.z],
        'throw_keys':['F','LeftMouseButton'],'practice_enemies':3,'city_enemies':1,'external_downloads':False}
(ROOT/'Saved/Validation/step5-setup.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
unreal.log('STEP5_SETUP_PASSED')
