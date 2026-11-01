#!/usr/bin/env python3
"""Apply the pinned Cesium 2.30.0 Clang initialization fix (project copy only)."""
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TARGET = ROOT / 'Plugins/CesiumForUnreal/Source/CesiumEditor/Private/IonQuickAddPanel.cpp'


def main():
    descriptor = ROOT / 'Plugins/CesiumForUnreal/CesiumForUnreal.uplugin'
    if json.loads(descriptor.read_text(encoding='utf-8-sig'))['VersionName'] != '2.30.0':
        raise ValueError('Patch is restricted to Cesium 2.30.0')
    raw = TARGET.read_bytes()
    text = raw.decode('utf-8').replace('\r\n', '\n')
    before = '      .ClientSize(FVector2D(400.0f, 200.0f))\n      .Content()\n      [\n        SNew(SVerticalBox) +'
    after = '      .ClientSize(FVector2D(400.0f, 200.0f));\n  AssetDepotConfirmWindow->SetContent(\n        SNew(SVerticalBox) +'
    tail = '      ];\n  // clang-format on\n  GEditor->EditorAddModalWindow(AssetDepotConfirmWindow);'
    fixed_tail = '      );\n  // clang-format on\n  GEditor->EditorAddModalWindow(AssetDepotConfirmWindow);'
    notice = '// Modified for Ikebukuro on 2026-11-01: initialize modal window before capturing it.\n'
    if after in text and fixed_tail in text:
        if not text.startswith(notice):
            TARGET.write_bytes(notice.encode('utf-8') + raw)
        print('Cesium Mac Clang fix already applied')
        return
    if text.count(before) != 1 or text.count(tail) != 1:
        raise ValueError('Unexpected Cesium source; refusing to patch')
    backup = ROOT / 'Backups/Mac-Cesium-2.30.0/IonQuickAddPanel.cpp'
    backup.parent.mkdir(parents=True, exist_ok=True)
    if backup.exists() and backup.read_bytes() != raw:
        raise ValueError('Existing backup differs; refusing to overwrite')
    backup.write_bytes(raw)
    patched = text.replace(before, after).replace(tail, fixed_tail)
    newline = '\r\n' if b'\r\n' in raw else '\n'
    output = (notice + patched).replace('\n', newline).encode('utf-8')
    TARGET.write_bytes(output)
    print('Applied Cesium Mac Clang fix; original SHA-256: ' + hashlib.sha256(raw).hexdigest())


if __name__ == '__main__':
    main()
