"""Three bounded patrol enemies on streets and roofs; no new external assets."""
import json
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve()
lib=unreal.EditorAssetLibrary
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
enemy_class=lib.load_blueprint_class('/Game/Ninja/Blueprints/BP_RedNinja')
records=[]

def find(label):
    return next((a for a in actors.get_all_level_actors() if a.get_actor_label()==label),None)

def area(label,point,extent):
    a=find(label) or actors.spawn_actor_from_class(unreal.NinjaPatrolArea,unreal.Vector(*point))
    a.set_actor_label(label); a.set_actor_location(unreal.Vector(*point),False,False)
    a.set_editor_property('half_extent',unreal.Vector2D(*extent))
    a.set_editor_property('height_tolerance',100)
    return a

def enemy(label,tag,point,domain):
    a=find(label) or actors.spawn_actor_from_class(enemy_class,unreal.Vector(*point))
    a.set_actor_label(label); a.set_actor_location(unreal.Vector(*point),False,False)
    a.set_editor_property('patrol_area',domain); a.set_editor_property('ai_enabled',True)
    a.set_editor_property('tags',[unreal.Name(tag)])
    records.append({'map':ACTIVE_MAP,
                    'label':label,'tag':tag,'spawn':list(point),'area':domain.get_actor_label()})
    return a

def block(label,point,scale):
    a=find(label) or actors.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*point))
    a.set_actor_label(label); a.set_actor_location(unreal.Vector(*point),False,False)
    a.set_actor_scale3d(unreal.Vector(*scale))
    a.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'))
    a.static_mesh_component.set_material(0,unreal.load_asset('/Game/LevelPrototyping/Materials/MI_PrototypeGrid_Gray'))
    a.static_mesh_component.set_collision_profile_name('BlockAll')
    return a

MAP='/Game/Ninja/Maps/L_NinjaEncounter'
ACTIVE_MAP=MAP
if not lib.does_asset_exist(MAP): lib.duplicate_asset('/Game/Ninja/Maps/L_NinjaCombat',MAP)
if not levels.load_level(MAP): raise RuntimeError('Encounter map load failed')
for a in list(actors.get_all_level_actors()):
    if a.get_actor_label().startswith('Practice_RedNinja_') or isinstance(a,unreal.NinjaCombatCheck): actors.destroy_actor(a)
ground=area('Encounter_GroundArea',(1800,800,0),(3000,2000))
roof=area('Encounter_RoofArea',(700,-1800,800),(800,800))
block('Encounter_Wall',(1800,750,150),(.3,10,3))
block('Encounter_Roof',(700,-1800,400),(20,20,8))
enemy('Encounter_GroundA','GroundA',(900,800,110),ground)
enemy('Encounter_GroundB','GroundB',(2700,800,110),ground)
enemy('Encounter_RoofA','RoofA',(700,-1800,910),roof)
if not find('Encounter_Check_OnlyWithCommandFlag'):
    a=actors.spawn_actor_from_class(unreal.NinjaEncounterCheck,unreal.Vector()); a.set_actor_label('Encounter_Check_OnlyWithCommandFlag')
if not levels.save_current_level(): raise RuntimeError('Encounter save failed')

if not levels.load_level('/Game/Ninja/Maps/L_Ikebukuro'): raise RuntimeError('City load failed')
ACTIVE_MAP='/Game/Ninja/Maps/L_Ikebukuro'
config=json.loads((ROOT/'doc/validation/step4-setup.json').read_text(encoding='utf-8'))
start=config['safe_street']; end=config['street_run_end']
center=[(a+b)/2 for a,b in zip(start,end)]
ground=area('City_GroundPatrolArea',center,(1800,600))
roof_point=(8345.25,-6637.38,1641.19)
roof=area('City_RoofPatrolArea',roof_point,(750,750))
enemy('City_RedNinja_Step5','GroundA',(start[0]+2400,start[1],start[2]+110),ground)
enemy('City_RedNinja_GroundB','GroundB',(center[0],center[1],center[2]+110),ground)
enemy('City_RedNinja_RoofA','RoofA',(roof_point[0],roof_point[1],roof_point[2]+110),roof)
if not find('City_Encounter_Check_OnlyWithCommandFlag'):
    a=actors.spawn_actor_from_class(unreal.NinjaEncounterCheck,unreal.Vector()); a.set_actor_label('City_Encounter_Check_OnlyWithCommandFlag')
if not levels.save_current_level(): raise RuntimeError('City save failed')
report={'status':'passed','encounter_map':MAP,'enemies_per_map':3,'enemies':records,
        'navigation':'bounded 1.5m walking grid, fixed WorldStatic collision, 50cm floor checks, 35cm maximum step',
        'external_downloads':False}
(ROOT/'Saved/Validation/step6-setup.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
unreal.log('STEP6_SETUP_PASSED')
