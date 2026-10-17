"""Enable a five-minute round in the city and a dedicated lightweight check map."""
import json
from pathlib import Path
import unreal
ROOT=Path(unreal.Paths.project_dir()).resolve()
lib=unreal.EditorAssetLibrary
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
PRACTICE='/Game/Ninja/Maps/L_NinjaMatch'
if not lib.does_asset_exist(PRACTICE):
    if not lib.duplicate_asset('/Game/Ninja/Maps/L_NinjaEncounter',PRACTICE): raise RuntimeError('Match practice copy failed')
for path in (PRACTICE,'/Game/Ninja/Maps/L_Ikebukuro'):
    if not levels.load_level(path): raise RuntimeError('Map load failed: '+path)
    current=actors.get_all_level_actors()
    for cls,label in ((unreal.NinjaMatchDirector,'Ninja_Match_Director'),(unreal.NinjaMatchCheck,'Match_Check_OnlyWithCommandFlag')):
        existing=[a for a in current if isinstance(a,cls)]
        if len(existing)>1: raise RuntimeError('Duplicate match actor: '+label)
        if not existing:
            a=actors.spawn_actor_from_class(cls,unreal.Vector()); a.set_actor_label(label)
    if not levels.save_current_level(): raise RuntimeError('Map save failed: '+path)
report={'status':'passed','maps':[PRACTICE,'/Game/Ninja/Maps/L_Ikebukuro'],'round_seconds':300,
    'countdown_seconds':3,'enemies':3,'retry':'reset actors and player in place, keep city tiles loaded',
    'controls':{'enter':'start/retry','p':'pause/resume','escape':'pause/resume in standalone; PIE may reserve Escape'},'external_downloads':False}
(ROOT/'Saved/Validation/step7-setup.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
unreal.log('STEP7_SETUP_PASSED')
