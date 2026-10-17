"""Check locally authored PCM files without using an audio library or network."""
import hashlib, json, math, struct, wave
from pathlib import Path
root=Path(__file__).resolve().parents[2]
rows=[]
for path in sorted((root/'Content/Ninja/AudioSource').glob('S_*.wav')):
    with wave.open(str(path),'rb') as wav:
        assert wav.getnchannels()==1 and wav.getsampwidth()==2 and wav.getframerate()==22050
        frames=wav.getnframes(); samples=struct.unpack('<'+'h'*frames,wav.readframes(frames))
        duration=frames/wav.getframerate()
        rms=math.sqrt(sum((s/32768)**2 for s in samples)/frames)
        peak=max(abs(s) for s in samples)/32768
        assert .05<duration<.3 and .02<rms<.4 and .1<peak<.9
        rows.append({'file':path.relative_to(root).as_posix(),'duration':duration,'rms':rms,'peak':peak,'sha256':hashlib.sha256(path.read_bytes()).hexdigest()})
assert len(rows)==7
output=root/'Saved/Validation/step8-audio.json'
output.write_text(json.dumps({'status':'passed','files':rows,'scope':'PCM structure, duration, non-silence and no clipping; not a subjective listening test'},indent=2),encoding='utf-8')
print('STEP8_AUDIO_PASSED',len(rows))
