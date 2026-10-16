"""Validate the local LOD1 tileset using only Python's standard library."""
import json
import math
import struct
from pathlib import Path
from urllib.parse import unquote, urlparse


ROOT = Path(__file__).resolve().parents[2]
DATA = ROOT / "Data/City/ToshimaLOD1"
REPORT = ROOT / "Saved/Validation/step2-lod1-inspection.json"


def inspect():
    tileset = json.loads((DATA / "tileset.json").read_text(encoding="utf-8"))
    references = []
    regions = []

    def visit(tile, depth=0):
        region = tile.get("boundingVolume", {}).get("region")
        if region:
            regions.append({"depth": depth, "region": region})
        content = tile.get("content", {})
        uri = content.get("uri", content.get("url"))
        if uri:
            if urlparse(uri).scheme:
                raise ValueError("Unexpected external reference: " + uri)
            path = (DATA / unquote(uri)).resolve()
            if not path.is_relative_to(DATA.resolve()):
                raise ValueError("Reference escapes local dataset: " + uri)
            if not path.is_file():
                raise FileNotFoundError(uri)
            with path.open("rb") as stream:
                header = stream.read(28)
                magic, version, length, ft_json, ft_bin, bt_json, bt_bin = struct.unpack("<4s6I", header)
                if magic != b"b3dm" or version != 1 or length != path.stat().st_size:
                    raise ValueError("Invalid b3dm header: " + uri)
                feature_table = json.loads(stream.read(ft_json).decode("utf-8").rstrip("\x00 \t\r\n"))
                stream.seek(28 + ft_json + ft_bin + bt_json + bt_bin)
                if stream.read(4) != b"glTF":
                    raise ValueError("Missing embedded GLB: " + uri)
            references.append({"uri": uri, "bytes": length, "depth": depth,
                               "features": feature_table.get("BATCH_LENGTH"),
                               "rtc_center": feature_table.get("RTC_CENTER")})
        for child in tile.get("children", []):
            visit(child, depth + 1)

    visit(tileset["root"])
    region = tileset["root"]["boundingVolume"]["region"]
    report = {
        "status": "passed", "tiles_version": tileset["asset"]["version"],
        "root_refine": tileset["root"].get("refine"),
        "bounds_degrees": {"west": math.degrees(region[0]), "south": math.degrees(region[1]),
                           "east": math.degrees(region[2]), "north": math.degrees(region[3])},
        "height_range_m": region[4:6], "root_transform": tileset["root"].get("transform"),
        "content_count": len(references), "region_count": len(regions),
        "references": references,
    }
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(json.dumps({k: v for k, v in report.items() if k != "references"}, indent=2))


if __name__ == "__main__":
    inspect()
