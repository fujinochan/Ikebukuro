"""Plan a small, project-local GSI photo download for the existing play area."""
import json
import math
from pathlib import Path

root = Path(__file__).resolve().parents[2]
config = json.loads((root / 'Tools/City/ikebukuro-area.json').read_text())
lon, lat, height = config['origin_longitude_latitude_height']
margin = config['background_buffer_m'] * 100
minimum = [v - margin for v in config['play_min_cm']]
maximum = [v + margin for v in config['play_max_cm']]
zoom = 17

def tile_xy(longitude, latitude):
    phi = math.radians(latitude)
    return ((longitude + 180) / 360 * 2**zoom,
            (1 - math.asinh(math.tan(phi)) / math.pi) / 2 * 2**zoom)

corners = [tile_xy(lon + x / 100 / (111320 * math.cos(math.radians(lat))),
                   lat - y / 100 / 111320)
           for x in (minimum[0], maximum[0]) for y in (minimum[1], maximum[1])]
tiles = []
for x in range(math.floor(min(c[0] for c in corners)), math.floor(max(c[0] for c in corners)) + 1):
    for y in range(math.floor(min(c[1] for c in corners)), math.floor(max(c[1] for c in corners)) + 1):
        tiles.append({'x': x, 'y': y,
                      'url': f'https://cyberjapandata.gsi.go.jp/xyz/seamlessphoto/{zoom}/{x}/{y}.jpg',
                      'file': f'Downloads/GSI/seamlessphoto-z{zoom}/{x}_{y}.jpg'})
output = root / 'Downloads/GSI'
output.mkdir(parents=True, exist_ok=True)
(output / 'ground-photo-manifest.json').write_text(json.dumps({
    'layer': 'seamlessphoto', 'zoom': zoom, 'acquired_date': '2026-10-19',
    'source': '国土地理院 地理院タイル 全国最新写真（シームレス）',
    'source_url': 'https://maps.gsi.go.jp/development/ichiran.html#seamlessphoto',
    'world_min_cm': minimum, 'world_max_cm': maximum,
    'origin_longitude_latitude_height': [lon, lat, height], 'tiles': tiles,
    'note': 'Download date is not the photography date. Final sampling uses the saved Cesium georeference.'
}, ensure_ascii=False, indent=2), encoding='utf-8')
print(f'Planned {len(tiles)} tiles at zoom {zoom}; bounds {minimum} to {maximum} cm')
