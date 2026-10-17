"""Generate an original, bounded explosion sound and import it; no downloads."""
import math, random, struct, wave, hashlib, json
from pathlib import Path
import unreal

root=Path(unreal.Paths.project_dir()).resolve()
path=root/'Content/Ninja/AudioSource/S_Explosion.wav'
rate=44100
rng=random.Random(13116)
duration=3.45
playback_gain=1.6
values=[]
low=0.0
for i in range(int(rate*duration)):
    t=i/rate
    noise=rng.uniform(-1,1)
    low=low*.91+noise*.09
    attack=min(1,t/.003)
    tail=max(0,1-t/duration)**1.25
    rumble=math.sin(2*math.pi*(62*t-5*t*t))
    value=attack*tail*(.7*rumble+.65*low+.25*noise*math.exp(-t*8))
    # Four excessive secondary booms, followed by a long rumbling tail.
    for delay,strength in [(0,1),(.18,.75),(.42,.6),(.8,.45),(1.2,.25)]:
        u=t-delay
        if u>=0:
            value+=strength*min(1,u/.004)*math.exp(-u*4)*(math.sin(2*math.pi*(95*u-20*u*u))+.5*noise)
    values.append(value*min(1,(duration-t)/.04))
# Leave headroom for the deliberately increased per-voice playback gain.
normalizer=.6/max(abs(x) for x in values)
samples=[round(x*normalizer*32767) for x in values]
assert max(abs(x) for x in samples)*playback_gain < 32767
path.parent.mkdir(parents=True,exist_ok=True)
with wave.open(str(path),'wb') as output:
    output.setnchannels(1); output.setsampwidth(2); output.setframerate(rate)
    output.writeframes(struct.pack('<'+'h'*len(samples),*samples))
task=unreal.AssetImportTask()
for key,value in [('filename',str(path)),('destination_path','/Game/Ninja/Audio'),('automated',True),('replace_existing',True),('save',True)]:
    task.set_editor_property(key,value)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
asset=unreal.load_asset('/Game/Ninja/Audio/S_Explosion')
if not asset:
    raise RuntimeError('Explosion sound import failed')
imported_duration=asset.get_editor_property('duration')
assert abs(imported_duration-duration)<.01, imported_duration
report={'status':'passed','seconds':len(samples)/rate,'sample_rate':rate,'peak_pcm':max(abs(x) for x in samples),
        'imported_asset_duration_seconds':imported_duration,
        'playback_gain':playback_gain,'single_voice_peak_after_gain':max(abs(x) for x in samples)/32767*playback_gain,
        'rms_pcm':math.sqrt(sum(x*x for x in samples)/len(samples)),
        'nonzero_samples':sum(x!=0 for x in samples),'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),
        'origin':'Original deterministic noise and sine synthesis within this project; no downloads'}
(root/'doc/validation/explosion-audio.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print(json.dumps(report))
