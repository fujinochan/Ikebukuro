"""Stage the user-provided M4A locally; no conversion or downloads."""
import hashlib, json, shutil, struct
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
source=ROOT/'Ikebukuro Ninja City.m4a'
data=source.read_bytes()
def atoms(begin,end):
    while begin+8<=end:
        size,kind=struct.unpack_from('>I4s',data,begin); header=8
        if size==1: size=struct.unpack_from('>Q',data,begin+8)[0]; header=16
        if size==0: size=end-begin
        if size<header or begin+size>end: raise ValueError('Invalid M4A atom bounds')
        yield kind,begin+header,begin+size
        begin+=size
duration=None
for kind,start,end in atoms(0,len(data)):
    if kind!=b'moov': continue
    for kind,start,end in atoms(start,end):
        if kind!=b'mvhd': continue
        version=data[start]
        scale=struct.unpack_from('>I',data,start+(20 if version==1 else 12))[0]
        length=struct.unpack_from('>Q' if version==1 else '>I',data,start+(24 if version==1 else 16))[0]
        duration=length/scale
if not duration or not 1<duration<3600: raise ValueError('Missing or invalid music duration')
destination=ROOT/'Content/Movies/Ikebukuro Ninja City.m4a'
destination.parent.mkdir(parents=True,exist_ok=True)
shutil.copyfile(source,destination)
sha=hashlib.sha256(data).hexdigest()
assert hashlib.sha256(destination.read_bytes()).hexdigest()==sha
report={'status':'passed','source':source.name,'runtime_file':destination.relative_to(ROOT).as_posix(),
        'bytes':len(data),'sha256':sha,'container_duration_seconds':duration,
        'conversion':False,'downloads':False,'note':'Runtime duration and completion come from UE WMF MediaPlayer.'}
(ROOT/'doc/validation/round-music-source.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps(report,indent=2))
