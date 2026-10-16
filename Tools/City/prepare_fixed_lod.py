"""Make a fixed finest-level proof tileset without modifying source tile meshes."""
import copy
import json
import shutil
from pathlib import Path

root = Path(__file__).resolve().parents[2]
data = root / "Data/City/ToshimaLOD1"
source = json.loads((data / "tileset.json").read_text(encoding="utf-8"))
leaves = []


def visit(tile):
    if "transform" in tile:
        raise ValueError("Flattening requires an explicit transform composition; refusing this dataset")
    children = tile.get("children", [])
    if children:
        for child in children:
            visit(child)
    elif "content" in tile:
        leaf = copy.deepcopy(tile)
        leaf.pop("children", None)
        leaf["geometricError"] = 0
        leaves.append(leaf)


visit(source["root"])
derived = copy.deepcopy(source)
derived["geometricError"] = 1e9
derived["root"].pop("content", None)
derived["root"]["geometricError"] = 1e9
derived["root"]["refine"] = "REPLACE"
derived["root"]["children"] = leaves
(data / "tileset-fixed.json").write_text(json.dumps(derived, indent=2), encoding="utf-8")
destination = root / "Content/CityData/ToshimaLOD1"
shutil.copytree(data, destination, dirs_exist_ok=True)
report = {"source": "tileset.json", "derived": "tileset-fixed.json", "leaf_tiles": len(leaves),
          "method": "flatten transform-free leaf hierarchy, force refinement at root, leaf geometricError=0",
          "purpose": "fixed LOD collision proof, not the final spatial crop or performance configuration"}
(root / "Saved/Validation/step2-fixed-lod.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
print(json.dumps(report, indent=2))
