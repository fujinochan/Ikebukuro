import unreal, json
from pathlib import Path
root=Path(unreal.Paths.project_dir()).resolve()
roof=unreal.load_asset('/Game/Ninja/City/SM_BuildingsLOD2Collision')
road=unreal.load_asset('/Game/Ninja/City/SM_RoadsCollision')
points=[(8345,-6637),(3313,-8867),(7168,-19861),(11387,-18008),(20043,7526)]
rows=[]
for x,y in points:
    samples=[]
    for dx in range(-900,901,150):
        for dy in range(-900,901,150):
            p=unreal.CityGeometryTools.surface_on_mesh(roof,unreal.Vector2D(x+dx,y+dy))
            samples.append([x+dx,y+dy,round(p.z,2)])
    rows.append({'point':[x,y],'grid':samples})
roads=[]
for x in range(11000,23001,1500):
    for y in range(-5000,10001,1500):
        r=unreal.CityGeometryTools.surface_on_mesh(road,unreal.Vector2D(x,y))
        b=unreal.CityGeometryTools.surface_on_mesh(roof,unreal.Vector2D(x,y))
        if r.z>-10000 and b.z<r.z+35: roads.append([x,y,r.z])
(root/'Saved/Validation/step8-probe.json').write_text(json.dumps({'roofs':rows,'roads':roads},indent=2))
