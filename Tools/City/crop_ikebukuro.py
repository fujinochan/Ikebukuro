"""Select fixed leaf tiles intersecting the area, directly from the original ZIP."""
import copy
import hashlib
import json
import math
import struct
import zipfile
from pathlib import Path
from urllib.parse import unquote, urlparse

ROOT = Path(__file__).resolve().parents[2]
CONFIG = json.loads((Path(__file__).with_name("ikebukuro-area.json")).read_text())
OUT = ROOT / "Content/CityData/Ikebukuro"
lon, lat, height = CONFIG["origin_longitude_latitude_height"]
phi = math.radians(lat)
e2 = 6.69437999014e-3
n = 6378137 / math.sqrt(1 - e2 * math.sin(phi) ** 2)
m = 6378137 * (1-e2) / (1-e2 * math.sin(phi) ** 2) ** 1.5
minx, miny = (x / 100 for x in CONFIG["play_min_cm"])
maxx, maxy = (x / 100 for x in CONFIG["play_max_cm"])
buffer = CONFIG["background_buffer_m"]
region = [math.radians(lon)+(minx-buffer)/((n+height)*math.cos(phi)),
          phi-(maxy+buffer)/(m+height),
          math.radians(lon)+(maxx+buffer)/((n+height)*math.cos(phi)),
          phi-(miny-buffer)/(m+height)]


def intersects(a, b):
    return a[0] <= b[2] and a[2] >= b[0] and a[1] <= b[3] and a[3] >= b[1]


def safe_uri(uri):
    if urlparse(uri).scheme:
        raise ValueError("External URI is not allowed: " + uri)
    path = unquote(uri).replace("\\", "/")
    if path.startswith("/") or ".." in path.split("/"):
        raise ValueError("Unsafe URI: " + uri)
    return path


def validate_embedded_glb(blob):
    if blob[:4] == b"b3dm":
        header = struct.unpack_from("<4s6I", blob)
        if header[1] != 1 or header[2] != len(blob):
            raise ValueError("Invalid b3dm header")
        feature = json.loads(blob[28:28+header[3]])
        if "RTC_CENTER" in feature:
            raise ValueError("Feature-table RTC needs explicit handling in the collision baker")
        blob = blob[28+sum(header[3:]):]
    if blob[:4] != b"glTF":
        raise ValueError("Unsupported tile format")
    length, kind = struct.unpack_from("<II", blob, 12)
    if kind != 0x4E4F534A:
        raise ValueError("Missing GLB JSON")
    model = json.loads(blob[20:20+length])
    for item in model.get("buffers", []) + model.get("images", []):
        if "uri" in item and not item["uri"].startswith("data:"):
            raise ValueError("External GLB dependency needs explicit copying: " + item["uri"])


report = {"config": CONFIG, "background_region_radians": region,
          "background_bounds_degrees": [math.degrees(x) for x in region],
          "play_bounds_degrees": [lon+math.degrees(minx/((n+height)*math.cos(phi))),
                                  lat-math.degrees(maxy/(m+height)),
                                  lon+math.degrees(maxx/((n+height)*math.cos(phi))),
                                  lat-math.degrees(miny/(m+height))], "layers": {}}
with zipfile.ZipFile(ROOT / CONFIG["source_zip"]) as archive:
    for name, prefix in CONFIG["layers"].items():
        source = json.loads(archive.read(prefix + "tileset.json"))
        if source["asset"].get("gltfUpAxis", "Y") != "Y":
            raise ValueError("Non-default up-axis requires explicit handling in the baker")
        leaves, kept = [], []

        def visit(tile):
            if "transform" in tile:
                raise ValueError("Transform composition required; refusing to flatten")
            bounds = tile.get("boundingVolume", {}).get("region")
            if bounds is None:
                raise ValueError("Expected geographic region")
            if not intersects(bounds, region):
                return None
            node = copy.deepcopy(tile)
            node.pop("content", None)
            children = [child for t in tile.get("children", []) if (child := visit(t))]
            if children:
                node["children"] = children
            elif not tile.get("children") and "content" in tile:
                leaf = copy.deepcopy(tile)
                leaf.pop("children", None)
                leaf["geometricError"] = 0
                leaves.append(leaf)
                node = leaf
            else:
                return None
            kept.append(node)
            return node

        hierarchy = visit(source["root"])
        if not leaves:
            raise ValueError("Layer does not cover the requested area: " + name)
        derived = {"asset": source["asset"], "geometricError": 1e9,
                   "root": {"boundingVolume": source["root"]["boundingVolume"],
                            "geometricError": 1e9, "refine": "REPLACE", "children": leaves}}
        folder = OUT / name
        folder.mkdir(parents=True, exist_ok=True)
        manifest = []
        for leaf in leaves:
            content = leaf["content"]
            uri = safe_uri(content.get("uri", content.get("url")))
            blob = archive.read(prefix + uri)
            validate_embedded_glb(blob)
            target = folder / uri
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(blob)
            manifest.append({"uri": uri, "bytes": len(blob), "sha256": hashlib.sha256(blob).hexdigest()})
        (folder / "tileset.json").write_text(json.dumps(derived, indent=2), encoding="utf-8")
        (folder / "tileset-hierarchy.json").write_text(json.dumps({"asset": source["asset"], "geometricError": source.get("geometricError", source["root"]["geometricError"]), "root": hierarchy}, indent=2), encoding="utf-8")
        source_tiles = [i for i in archive.infolist() if i.filename.startswith(prefix) and i.filename.endswith((".b3dm", ".glb"))]
        report["layers"][name] = {"source_tiles": len(source_tiles), "selected_leaf_tiles": len(leaves),
                                  "ancestor_and_leaf_nodes": len(kept), "source_bytes": sum(i.file_size for i in source_tiles),
                                  "selected_bytes": sum(i["bytes"] for i in manifest), "files": manifest}
(OUT / "area.json").write_text(json.dumps(CONFIG, indent=2), encoding="utf-8")
path = ROOT / "Saved/Validation/step4-crop.json"
path.parent.mkdir(parents=True, exist_ok=True)
path.write_text(json.dumps(report, indent=2), encoding="utf-8")
print(json.dumps({k: {a:b for a,b in v.items() if a != "files"} for k,v in report["layers"].items()}, indent=2))
