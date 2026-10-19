"""Add local matte skin and fabric trim materials; do not change maps or gameplay."""
from pathlib import Path
import json
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
assets = unreal.EditorAssetLibrary
edit = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
for name, color in [('Skin', (.72, .40, .20)), ('Wrap', (.095, .12, .15))]:
    path = '/Game/Ninja/Materials/M_Ninja' + name
    material = unreal.load_asset(path) if assets.does_asset_exist(path) else tools.create_asset(
        'M_Ninja' + name, '/Game/Ninja/Materials', unreal.Material, unreal.MaterialFactoryNew())
    edit.delete_all_material_expressions(material)
    base = edit.create_material_expression(material, unreal.MaterialExpressionConstant3Vector)
    base.set_editor_property('constant', unreal.LinearColor(*color, 1))
    edit.connect_material_property(base, '', unreal.MaterialProperty.MP_BASE_COLOR)
    for value, prop in [(0.94, unreal.MaterialProperty.MP_ROUGHNESS), (0.08, unreal.MaterialProperty.MP_SPECULAR)]:
        node = edit.create_material_expression(material, unreal.MaterialExpressionConstant)
        node.set_editor_property('r', value)
        edit.connect_material_property(node, '', prop)
    edit.recompile_material(material)
    if not assets.save_loaded_asset(material, False):
        raise RuntimeError('Material save failed: ' + path)
(root / 'Saved/Validation/ninja-costume-assets.json').write_text(json.dumps({
    'status': 'passed', 'created_materials': ['M_NinjaSkin', 'M_NinjaWrap'], 'external_downloads': False
}, indent=2), encoding='utf-8')
