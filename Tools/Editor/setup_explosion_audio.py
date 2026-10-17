"""Generate an original, bounded explosion sound and import it; no downloads."""
import math, random, struct, wave, hashlib, json
from pathlib import Path
import unreal

root=Path(unreal.Paths.project_dir()).resolve()
path=root/'Content/Ninja/AudioSource/S_Explosion.wav'
rate=44100
rng=random.Random(13116)
samples=[]
low=0.0
for i in range(int(rate*1.15)):
    t=i/rate
    noise=rng.uniform(-1,1)
    low=low*.91+noise*.09
    attack=min(1,t/.003)
    tail=max(0,1-t/1.15)**2
    rumble=math.sin(2*math.pi*(85*t-24*t*t))
    value=attack*tail*(.43*rumble+.48*low+.20*noise*math.exp(-t*15))
    samples.append(int(max(-.95,min(.95,value))*32767))
path.parent.mkdir(parents=True,exist_ok=True)
with wave.open(str(path),'wb') as output:
    output.setnchannels(1); output.setsampwidth(2); output.setframerate(rate)
    output.writeframes(struct.pack('<'+'h'*len(samples),*samples))
task=unreal.AssetImportTask()
for key,value in [('filename',str(path)),('destination_path','/Game/Ninja/Audio'),('automated',True),('replace_existing',True),('save',True)]:
    task.set_editor_property(key,value)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
if not unreal.load_asset('/Game/Ninja/Audio/S_Explosion'):
    raise RuntimeError('Explosion sound import failed')
report={'status':'passed','seconds':len(samples)/rate,'sample_rate':rate,'peak_pcm':max(abs(x) for x in samples),
        'nonzero_samples':sum(x!=0 for x in samples),'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),
        'origin':'Original deterministic noise and sine synthesis within this project; no downloads'}
(root/'doc/validation/explosion-audio.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print(json.dumps(report))
