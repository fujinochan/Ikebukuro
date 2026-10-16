"""Use textured LOD2 and retain only buildings absent from it as a LOD1 fallback."""
import json
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve()
lib=unreal.EditorAssetLibrary
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
MAP='/Game/Ninja/Maps/L_Ikebukuro'
BASELINE='/Game/Ninja/Maps/L_IkebukuroLOD1Comparison'
config=json.loads((ROOT/'Tools/City/ikebukuro-area.json').read_text())

def find(label):
    return next((a for a in actors.get_all_level_actors() if a.get_actor_label()==label),None)

def check_actor():
    if not find('Texture_Check_OnlyWithCommandFlag'):
        a=actors.spawn_actor_from_class(unreal.CityTextureCheck,unreal.Vector())
        a.set_actor_label('Texture_Check_OnlyWithCommandFlag')

if not lib.does_asset_exist(BASELINE):
    if not lib.duplicate_asset(MAP,BASELINE): raise RuntimeError('Baseline map backup failed')
if not levels.load_level(BASELINE): raise RuntimeError('Baseline load failed')
check_actor()
if not levels.save_current_level(): raise RuntimeError('Baseline save failed')

if '-TexturePrepareBaseline' not in unreal.SystemLibrary.get_command_line():
    if not levels.load_level(MAP): raise RuntimeError('City load failed')
    check_actor()
    geo=find('Ikebukuro_FixedOrigin')
    lo=unreal.Vector2D(*(x-config['collision_margin_m']*100 for x in config['play_min_cm']))
    hi=unreal.Vector2D(*(x+config['collision_margin_m']*100 for x in config['play_max_cm']))
    relative='CityData/IkebukuroLOD2/Buildings/tileset.json'
    mesh=unreal.CityGeometryTools.bake_layer(relative,'/Game/Ninja/City/SM_BuildingsLOD2Collision',geo,lo,hi)
    if not mesh: raise RuntimeError('LOD2 collision bake failed')
    bake=json.loads((ROOT/'Saved/Validation/SM_BuildingsLOD2Collision.json').read_text(encoding='utf-8-sig'))
    ids=bake['represented_feature_ids']
    if not ids: raise RuntimeError('No decoded LOD2 building IDs')
    fallback=unreal.CityGeometryTools.bake_layer_without_features('CityData/Ikebukuro/Buildings/tileset.json',
        '/Game/Ninja/City/SM_BuildingsFallback',geo,
        unreal.Vector2D(*(x-config['background_buffer_m']*100 for x in config['play_min_cm'])),
        unreal.Vector2D(*(x+config['background_buffer_m']*100 for x in config['play_max_cm'])),ids)
    fallback_bake=json.loads((ROOT/'Saved/Validation/SM_BuildingsFallback.json').read_text(encoding='utf-8-sig'))
    if not fallback and fallback_bake['saved_triangles']>0: raise RuntimeError('LOD1 fallback bake failed')
    if set(ids)&set(fallback_bake['represented_feature_ids']): raise RuntimeError('LOD1/LOD2 duplicated feature ID')
    # The fallback is both visible and collidable using exactly the same saved mesh.
    # Its name deliberately differs from invisible '*Collision' proxies.
    proxy=find('City_BuildingsCollision')
    proxy.static_mesh_component.set_static_mesh(None)
    proxy.static_mesh_component.set_static_mesh(mesh)
    tile=find('City_Buildings')
    tile.set_editor_property('dataset_relative_path',relative)
    tile.set_editor_property('create_tile_physics',False)
    tile.configure_local_source()
    if fallback:
        name='/Game/Ninja/City/M_CityFallback'
        material=unreal.load_asset(name) or unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            'M_CityFallback','/Game/Ninja/City',unreal.Material,unreal.MaterialFactoryNew())
        edit=unreal.MaterialEditingLibrary
        edit.delete_all_material_expressions(material)
        color=edit.create_material_expression(material,unreal.MaterialExpressionConstant3Vector)
        color.set_editor_property('constant',unreal.LinearColor(.65,.66,.67,1))
        edit.connect_material_property(color,'',unreal.MaterialProperty.MP_BASE_COLOR)
        rough=edit.create_material_expression(material,unreal.MaterialExpressionConstant)
        rough.set_editor_property('r',.9)
        edit.connect_material_property(rough,'',unreal.MaterialProperty.MP_ROUGHNESS)
        edit.recompile_material(material); lib.save_loaded_asset(material)
        a=find('City_BuildingsFallback') or actors.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector())
        a.set_actor_label('City_BuildingsFallback')
        a.static_mesh_component.set_static_mesh(None); a.static_mesh_component.set_static_mesh(fallback)
        a.static_mesh_component.set_material(0,material)
        a.static_mesh_component.set_collision_profile_name('BlockAll')
    elif find('City_BuildingsFallback'):
        actors.destroy_actor(find('City_BuildingsFallback'))
    area=find('Ikebukuro_PlayArea')
    original=json.loads((ROOT/'doc/validation/step4-setup.json').read_text())
    roofs=[]
    for i,p in enumerate(original['roof_targets']):
        point=unreal.CityGeometryTools.surface_on_mesh(mesh,unreal.Vector2D(p[0],p[1]))
        if point.z < -10000: raise RuntimeError('LOD2 does not support a training roof target')
        roofs.append(point)
        marker=find(f'Roof_{i+1}_Marker')
        if marker: marker.set_actor_location(point+unreal.Vector(0,0,100),False,False)
    area.set_editor_property('roof_targets',roofs)
    domain=find('City_RoofPatrolArea')
    center=domain.get_actor_location()
    foot=unreal.CityGeometryTools.surface_on_mesh(mesh,unreal.Vector2D(center.x,center.y))
    if foot.z < -10000: raise RuntimeError('LOD2 roof patrol has no floor')
    domain.set_actor_location(foot,False,False)
    roof_enemy=find('City_RedNinja_RoofA')
    roof_enemy.set_actor_location(foot+unreal.Vector(0,0,110),False,False)
    if not levels.save_current_level(): raise RuntimeError('Textured city save failed')
    report={'status':'passed','map':MAP,'baseline_map':BASELINE,'textured_dataset':relative,
        'lod2_represented_features':len(ids),'lod1_fallback_source_features':len(fallback_bake['represented_feature_ids']),
        'lod1_fallback_used':bool(fallback),
        'duplicate_feature_ids':0,'lod1_removed_triangles':fallback_bake['excluded_triangles'],
        'lod2_collision_triangles':bake['saved_triangles'],'fallback_triangles':fallback_bake['saved_triangles'],
        'roof_targets':[[p.x,p.y,p.z] for p in roofs],
        'roof_enemy_surface':[foot.x,foot.y,foot.z],
        'external_downloads':False}
    (ROOT/'Saved/Validation/step6_5-setup.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
unreal.log('STEP6_5_SETUP_PASSED')
