"""Author an eight-target route and locally synthesized feedback; no downloads."""
import json, math, wave, struct, heapq
from pathlib import Path
import unreal
ROOT=Path(unreal.Paths.project_dir()).resolve()
lib=unreal.EditorAssetLibrary
tools=unreal.AssetToolsHelpers.get_asset_tools()
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
edit=unreal.MaterialEditingLibrary
MAIN='/Game/Ninja/Maps/L_Ikebukuro'
if not levels.load_level(MAIN): raise RuntimeError('City load failed')

# Re-running this script replaces only Step 8 actors and the original three enemies.
for a in list(actors.get_all_level_actors()):
    if isinstance(a,(unreal.RedNinja,unreal.NinjaPatrolArea)) or a.get_actor_label().startswith(('Route8_','Roof_')):
        actors.destroy_actor(a)

def material(name,color,unlit=False):
    path='/Game/Ninja/Materials/'+name
    m=unreal.load_asset(path) if lib.does_asset_exist(path) else tools.create_asset(name,'/Game/Ninja/Materials',unreal.Material,unreal.MaterialFactoryNew())
    edit.delete_all_material_expressions(m)
    node=edit.create_material_expression(m,unreal.MaterialExpressionVectorParameter)
    node.set_editor_property('parameter_name','Color'); node.set_editor_property('default_value',unreal.LinearColor(*color,1))
    edit.connect_material_property(node,'',unreal.MaterialProperty.MP_BASE_COLOR)
    edit.connect_material_property(node,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    if unlit: m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
    edit.recompile_material(m); lib.save_loaded_asset(m,False)
    return m
cyan=material('M_NinjaStep',(.02,.55,.7))
material('M_NinjaBurst',(.05,2,3),True)
# A restrained Fresnel edge keeps the existing black cloth legible against the city.
for name,color in [('Black',(.015,.12,.18)),('Red',(.3,.018,.006))]:
    m=unreal.load_asset('/Game/Ninja/Materials/M_Ninja'+name)
    edit.delete_all_material_expressions(m)
    base=edit.create_material_expression(m,unreal.MaterialExpressionConstant3Vector)
    base.set_editor_property('constant',unreal.LinearColor(*((.006,.008,.012) if name=='Black' else (.55,.018,.025)),1))
    edit.connect_material_property(base,'',unreal.MaterialProperty.MP_BASE_COLOR)
    rough=edit.create_material_expression(m,unreal.MaterialExpressionConstant); rough.set_editor_property('r',.94)
    edit.connect_material_property(rough,'',unreal.MaterialProperty.MP_ROUGHNESS)
    specular=edit.create_material_expression(m,unreal.MaterialExpressionConstant); specular.set_editor_property('r',.08)
    edit.connect_material_property(specular,'',unreal.MaterialProperty.MP_SPECULAR)
    fresnel=edit.create_material_expression(m,unreal.MaterialExpressionFresnel)
    fresnel.set_editor_property('exponent',3.0); fresnel.set_editor_property('base_reflect_fraction',.02)
    c=edit.create_material_expression(m,unreal.MaterialExpressionConstant3Vector); c.set_editor_property('constant',unreal.LinearColor(*color,1))
    mul=edit.create_material_expression(m,unreal.MaterialExpressionMultiply)
    edit.connect_material_expressions(fresnel,'',mul,'A'); edit.connect_material_expressions(c,'',mul,'B')
    edit.connect_material_property(mul,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    edit.recompile_material(m); lib.save_loaded_asset(m,False)

audio_dir=ROOT/'Content/Ninja/AudioSource'; audio_dir.mkdir(parents=True,exist_ok=True)
sound_specs={'Jump':(440,660,.11),'Triple':(660,1320,.23),'Throw':(1000,230,.09),'Hit':(900,420,.15),'Stomp':(180,70,.19),'Damage':(150,80,.20),'Impact':(650,120,.08)}
sounds=[]
for name,(f0,f1,duration) in sound_specs.items():
    path=audio_dir/('S_'+name+'.wav'); rate=22050; count=int(rate*duration); data=[]; phase=0
    for i in range(count):
        t=i/rate; u=i/count; phase+=2*math.pi*(f0+(f1-f0)*u)/rate
        envelope=min(1,t/.006)*(1-u)**2
        value=.42*envelope*(math.sin(phase)+.18*math.sin(2*phase))
        data.append(struct.pack('<h',int(max(-1,min(1,value))*32767)))
    with wave.open(str(path),'wb') as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(rate); w.writeframes(b''.join(data))
    task=unreal.AssetImportTask(); task.set_editor_property('filename',str(path)); task.set_editor_property('destination_path','/Game/Ninja/Audio')
    task.set_editor_property('automated',True); task.set_editor_property('replace_existing',True); task.set_editor_property('save',True)
    if not lib.does_asset_exist('/Game/Ninja/Audio/S_'+name): tools.import_asset_tasks([task])
    sounds.append({'name':name,'seconds':duration,'samples':count})

meshes={n:unreal.load_asset('/Game/Ninja/City/'+n) for n in ('SM_BuildingsLOD2Collision','SM_RoadsCollision','SM_FillGround')}
cache={}
def z(mesh,x,y):
    key=(mesh,round(x,2),round(y,2))
    if key not in cache: cache[key]=unreal.CityGeometryTools.surface_on_mesh(meshes[mesh],unreal.Vector2D(x,y)).z
    return cache[key]
def building(x,y): return z('SM_BuildingsLOD2Collision',x,y)
def ground(x,y): return max(z('SM_RoadsCollision',x,y),z('SM_FillGround',x,y))
def surface(x,y): return max(ground(x,y),building(x,y))
def block(label,point,scale):
    a=actors.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*point)); a.set_actor_label('Route8_'+label)
    a.set_actor_scale3d(unreal.Vector(*scale)); a.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'))
    a.static_mesh_component.set_material(0,cyan); a.static_mesh_component.set_collision_profile_name('BlockAll')
    return a
def clear_bridge(a,b,height,forbidden=()):
    count=max(1,math.ceil(math.dist(a[:2],b[:2])/120))
    for i in range(count+1):
        u=i/count; px=a[0]+(b[0]-a[0])*u; py=a[1]+(b[1]-a[1])*u
        if any(abs(px-p[0])<350 and abs(py-p[1])<350 for p in forbidden): return False
        if max(building(px+sx,py+sy) for sx,sy in ((0,0),(-180,0),(180,0),(0,-180),(0,180)))>height-20: return False
    return True
def bridge_path(start,end,height,forbidden=()):
    if clear_bridge(start,end,height,forbidden): return [start,end]
    # A small orthogonal grid routes the auxiliary bridge around taller buildings.
    cell=400; goal=(round((end[0]-start[0])/cell),round((end[1]-start[1])/cell))
    def point(node): return [start[0]+node[0]*cell,start[1]+node[1]*cell,height]
    queue=[(0,0,(0,0))]; parents={}; cost={(0,0):0}
    while queue:
        _,g,node=heapq.heappop(queue)
        if g!=cost[node]: continue
        if node==goal and clear_bridge(point(node),end,height,forbidden):
            chain=[end,point(node)]
            while node!=(0,0): node=parents[node]; chain.append(point(node))
            return list(reversed(chain))
        for dx,dy in ((1,0),(-1,0),(0,1),(0,-1)):
            nxt=(node[0]+dx,node[1]+dy)
            if not min(0,goal[0])-5<=nxt[0]<=max(0,goal[0])+5 or not min(0,goal[1])-5<=nxt[1]<=max(0,goal[1])+5: continue
            if g+1>=cost.get(nxt,1e9) or not clear_bridge(point(node),point(nxt),height,forbidden): continue
            cost[nxt]=g+1; parents[nxt]=node
            heapq.heappush(queue,(g+1+abs(nxt[0]-goal[0])+abs(nxt[1]-goal[1]),g+1,nxt))
    raise RuntimeError('No unobstructed bridge route')
def assist(order,target):
    x,y,top=target; choices=[]
    for angle in range(0,360,45):
        dx,dy=math.cos(math.radians(angle)),math.sin(math.radians(angle)); tx,ty=-dy,dx
        for dist in range(750,3001,150):
            cx,cy=x+dx*dist,y+dy*dist; base=ground(cx,cy)
            if base < -10000: continue
            # The four-corner stair and its ground entrance stand outside buildings.
            if any(building(cx+sx,cy+sy)>base+45 for sx in (-400,0,400) for sy in (-700,-400,0,400)): continue
            height=top+70-base
            if height<300 or height>2500: continue
            # Nothing taller than the landing bridge may intersect its 3m width.
            if any(building(x+dx*d+tx*side,y+dy*d+ty*side)>top+35 for d in range(0,dist+1,150) for side in (-160,0,160)): continue
            choices.append((dist,cx,cy,base,dx,dy)); break
    if not choices: raise RuntimeError('No clear stair approach for roof '+str(order))
    dist,cx,cy,base,dx,dy=min(choices)
    # Four-corner stairs avoid placing the second step over the starting jump.
    # Every rise <=2.8m, corner-to-corner travel 4m, clear gap 1.2m.
    n=math.ceil((top+70-base)/280); rise=(top+70-base)/n
    points=[[cx-200,cy-600,ground(cx-200,cy-600)]]
    corners=[(-200,-200),(200,-200),(200,200),(-200,200)]
    for i in range(1,n+1):
        ox,oy=corners[(i-1)%4]
        px,py=cx+ox,cy+oy; pz=base+rise*i
        block(f'{order:02}_Step_{i:02}',(px,py,pz-12),(2.8,2.8,.24))
        points.append([px,py,pz])
    # Broad non-jumping bridge above the measured roof, ending beside its target.
    start=points[-1]; end=[x+dx*400,y+dy*400,top+70]
    # Keep the bridge away from the last lower steps, including jump headroom.
    forbidden=[p for p in points[1:-1] if start[2]-p[2]<650]
    chain=bridge_path(start,end,top+70,forbidden)
    for i,(a,b) in enumerate(zip(chain,chain[1:])):
        distance=math.dist(a[:2],b[:2])
        if distance<1: continue
        bridge=block(f'{order:02}_Bridge_{i:02}',((a[0]+b[0])/2,(a[1]+b[1])/2,top+58),(distance/100+2.8,3,.24))
        bridge.set_actor_rotation(unreal.Rotator(yaw=math.degrees(math.atan2(b[1]-a[1],b[0]-a[0]))),False)
    points.extend(chain[1:])
    points.append([x,y,top])
    return points

# Four street encounters and four measured rooftops; the route can be played in any order.
layout=[
 (1,'GroundB',13418.34,-5055.33,'STREET / F to throw',False),
 (2,'GroundA',14618.34,-5055.33,'STREET / jump and descend to stomp',False),
 (3,'RoofA',8345.25,-6637.38,'LOW ROOF / cyan steps to the roof',True),
 (4,'RoofB',4824.28,-7151.13,'LOW ROOF / follow the cyan bridge west',True),
 (5,'RoofC',7618,-19861,'MID ROOF / triple jump or cyan steps',True),
 (6,'RoofD',11387,-18008,'MID ROOF / return east',True),
 (7,'GroundC',11000,1000,'STREET / head south',False),
 (8,'GroundD',15500,4000,'FINAL / clear the street',False)]
records=[]
enemy_class=lib.load_blueprint_class('/Game/Ninja/Blueprints/BP_RedNinja')
for order,tag,x,y,hint,is_roof in layout:
    top=surface(x,y)
    if top < -10000: raise RuntimeError('Invalid target surface')
    if order in (4,6):
        # Both low roofs share an accessible ground staircase, avoiding a second
        # tower squeezed between buildings on the narrow station-side streets.
        previous=records[-1]; route=[list(p) for p in previous['assist_route']]
        origin=previous['foot']; length=math.dist(origin[:2],(x,y)); ux,uy=(x-origin[0])/length,(y-origin[1])/length
        start=[origin[0]+ux*400,origin[1]+uy*400,origin[2]]; bridge_top=max(start[2],top)+350
        start[2]=bridge_top; end=[x,y,bridge_top]
        chain=bridge_path(start,end,bridge_top)
        for i,(a,b) in enumerate(zip(chain,chain[1:])):
            distance=math.dist(a[:2],b[:2])
            if distance<1: continue
            bridge=block(f'{order:02}_Link_{i:02}',((a[0]+b[0])/2,(a[1]+b[1])/2,bridge_top-12),(distance/100+2.8,3,.24))
            bridge.set_actor_rotation(unreal.Rotator(yaw=math.degrees(math.atan2(b[1]-a[1],b[0]-a[0]))),False)
        route.extend(chain)
        # This target stands on the roof connector, 3.5-3.7m above the city roof.
        # Its AI domain uses the same physical connector surface.
        top=bridge_top
    else:
        route=assist(order,(x,y,top)) if is_roof else []
    # Each patrol domain follows the actual supporting surface, city or connector.
    domain=actors.spawn_actor_from_class(unreal.NinjaPatrolArea,unreal.Vector(x,y,top))
    domain.set_actor_label(f'Route8_{order:02}_Area')
    domain.set_editor_property('half_extent',unreal.Vector2D(750,600))
    enemy=actors.spawn_actor_from_class(enemy_class,unreal.Vector(x,y,top+115))
    enemy.set_actor_label(f'Route8_{order:02}_{tag}'); enemy.set_editor_property('tags',[unreal.Name(tag),unreal.Name(f'Route{order:02}')])
    enemy.set_editor_property('ai_enabled',True); enemy.set_editor_property('patrol_area',domain)
    enemy.set_editor_property('route_order',order); enemy.set_editor_property('route_hint',hint)
    enemy.set_editor_property('assist_route',[unreal.Vector(*p) for p in route])
    enemy.set_editor_property('detection_radius',500 if order<=2 else 900)
    records.append({'order':order,'tag':tag,'foot':[x,y,top],'hint':hint,'assist_route':route})
if not any(isinstance(a,unreal.NinjaRouteCheck) for a in actors.get_all_level_actors()):
    a=actors.spawn_actor_from_class(unreal.NinjaRouteCheck,unreal.Vector()); a.set_actor_label('Route8_Check_OnlyWithCommandFlag')
if not levels.save_current_level(): raise RuntimeError('Step 8 city save failed')
report={'status':'authored','enemy_count':8,'route':records,'sounds':sounds,'baseline_commit':'082f56b','external_downloads':False}
(ROOT/'Saved/Validation/step8-setup.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
unreal.log('STEP8_SETUP_AUTHORED')
