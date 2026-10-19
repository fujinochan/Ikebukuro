"""Generate and apply an offline photo-derived world-space ground material."""
import json
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
lib = unreal.EditorAssetLibrary
edit = unreal.MaterialEditingLibrary

def connect(source, output_name, target, input_name):
    if not edit.connect_material_expressions(source, output_name, target, input_name):
        raise RuntimeError(f'Material connection failed: {source.get_class().get_name()} -> {target.get_class().get_name()} {input_name}')

levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
if not levels.load_level('/Game/Ninja/Maps/L_Ikebukuro'):
    raise RuntimeError('City load failed')
current = actors.get_all_level_actors()
geo = next(a for a in current if isinstance(a, unreal.CesiumGeoreference))
floor = next(a for a in current if a.get_actor_label() == 'City_FillGround')
roads = next(a for a in current if isinstance(a, unreal.LocalCityTileset) and '/Roads/' in a.get_editor_property('dataset_relative_path'))

def collision_state():
    result = []
    for actor in current:
        if isinstance(actor, unreal.StaticMeshActor):
            component = actor.static_mesh_component
            mesh = component.get_editor_property('static_mesh')
            result.append((actor.get_actor_label(), mesh.get_path_name() if mesh else '',
                           str(actor.get_actor_transform()), str(component.get_collision_profile_name())))
    return sorted(result)

before = collision_state()
if not unreal.CityGroundColorTools.generate_ground_colors('Downloads/GSI/ground-photo-manifest.json', geo):
    raise RuntimeError('Offline photo color generation failed')
task = unreal.AssetImportTask()
task.set_editor_property('filename', str(root / 'Content/Ninja/City/Textures/T_CityGroundColors.png'))
task.set_editor_property('destination_path', '/Game/Ninja/City/Textures')
task.set_editor_property('destination_name', 'T_CityGroundColors')
task.set_editor_property('automated', True)
task.set_editor_property('replace_existing', True)
task.set_editor_property('save', True)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
texture = unreal.load_asset('/Game/Ninja/City/Textures/T_CityGroundColors')
if not texture:
    raise RuntimeError('Palette texture import failed')
texture.set_editor_property('srgb', True)
texture.set_editor_property('address_x', unreal.TextureAddress.TA_CLAMP)
texture.set_editor_property('address_y', unreal.TextureAddress.TA_CLAMP)
lib.save_loaded_asset(texture, False)

path = '/Game/Ninja/City/M_CityGroundPhotoColors'
material = unreal.load_asset(path) if lib.does_asset_exist(path) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(
    'M_CityGroundPhotoColors', '/Game/Ninja/City', unreal.Material, unreal.MaterialFactoryNew())
edit.delete_all_material_expressions(material)
position = edit.create_material_expression(material, unreal.MaterialExpressionWorldPosition)
xy = edit.create_material_expression(material, unreal.MaterialExpressionComponentMask)
xy.set_editor_property('r', True); xy.set_editor_property('g', True)
xy.set_editor_property('b', False); xy.set_editor_property('a', False)
connect(position, '', xy, '')
manifest = json.loads((root / 'Downloads/GSI/ground-photo-manifest.json').read_text(encoding='utf-8'))
minimum, maximum = manifest['world_min_cm'], manifest['world_max_cm']
origin = edit.create_material_expression(material, unreal.MaterialExpressionConstant2Vector)
origin.set_editor_property('r', minimum[0]); origin.set_editor_property('g', minimum[1])
subtract = edit.create_material_expression(material, unreal.MaterialExpressionSubtract)
connect(xy, '', subtract, 'A'); connect(origin, '', subtract, 'B')
span = edit.create_material_expression(material, unreal.MaterialExpressionConstant2Vector)
span.set_editor_property('r', maximum[0] - minimum[0]); span.set_editor_property('g', maximum[1] - minimum[1])
divide = edit.create_material_expression(material, unreal.MaterialExpressionDivide)
connect(subtract, '', divide, 'A'); connect(span, '', divide, 'B')
sample = edit.create_material_expression(material, unreal.MaterialExpressionTextureSample)
sample.set_editor_property('texture', texture)
connect(divide, '', sample, 'UVs')
edit.connect_material_property(sample, 'RGB', unreal.MaterialProperty.MP_BASE_COLOR)
for value, prop in [(0.97, unreal.MaterialProperty.MP_ROUGHNESS), (0.02, unreal.MaterialProperty.MP_SPECULAR)]:
    node = edit.create_material_expression(material, unreal.MaterialExpressionConstant)
    node.set_editor_property('r', value)
    edit.connect_material_property(node, '', prop)
material.set_editor_property('two_sided', True)
edit.recompile_material(material)
lib.save_loaded_asset(material, False)
floor.static_mesh_component.set_material(0, material)
roads.set_editor_property('material', material)
if collision_state() != before:
    raise RuntimeError('Geometry, transforms or collision settings changed')
if not levels.save_current_level():
    raise RuntimeError('City save failed')
(root / 'Saved/Validation/ground-colors-setup.json').write_text(json.dumps({
    'status': 'passed', 'material': path, 'texture': texture.get_path_name(),
    'floor_actor': floor.get_actor_label(), 'roads_actor': roads.get_actor_label(),
    'world_min_cm': minimum, 'world_max_cm': maximum,
    'static_meshes_transforms_and_collision_unchanged': True,
    'static_mesh_actors_checked': len(before), 'runtime_network_needed': False
}, indent=2), encoding='utf-8')
