#!/usr/bin/env python3
"""Restore the pinned Cesium release and verify committed runtime data (stdlib only)."""
import argparse
import hashlib
import json
import shutil
import stat
import sys
import urllib.request
import zipfile
from pathlib import Path, PurePosixPath

ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def inside(relative):
    path = (ROOT / relative).resolve()
    if ROOT not in path.parents:
        raise ValueError(f'Path escapes project: {relative}')
    return path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--download', action='store_true', help='Acquire the approved official release if the ZIP is absent')
    parser.add_argument('--check', action='store_true', help='Verify only; do not download or extract')
    args = parser.parse_args()
    manifest = json.loads((ROOT / 'doc/project-runtime-files.json').read_text(encoding='utf-8'))
    for item in manifest['files']:
        path = inside(item['path'])
        if not path.is_file() or digest(path) != item['sha256']:
            raise ValueError(f'Missing or altered runtime file: {item["path"]}')
    print(f'Runtime data verified: {len(manifest["files"])} files')
    pin = json.loads((ROOT / 'doc/third-party/cesium-plugin.json').read_text(encoding='utf-8-sig'))
    archive = inside(pin['archive'])
    destination = inside(pin['install_directory'])
    if not destination.exists() and not args.check:
        if not archive.is_file():
            if not args.download:
                raise ValueError('Cesium ZIP is absent. Copy it into Downloads/ or explicitly pass --download.')
            archive.parent.mkdir(parents=True, exist_ok=True)
            partial = archive.with_suffix('.zip.part')
            print(f'Downloading {pin["source"]} ({pin["archive_bytes"]} bytes)', flush=True)
            with urllib.request.urlopen(pin['source'], timeout=120) as response, partial.open('wb') as out:
                shutil.copyfileobj(response, out)
            if partial.stat().st_size != pin['archive_bytes'] or digest(partial) != pin['sha256']:
                raise ValueError('Downloaded Cesium ZIP failed SHA-256 verification; partial retained for inspection')
            partial.rename(archive)
        if archive.stat().st_size != pin['archive_bytes'] or digest(archive) != pin['sha256']:
            raise ValueError('Cesium ZIP failed SHA-256 verification')
        with zipfile.ZipFile(archive) as release:
            # Validate every member before creating any plugin files.
            for member in release.infolist():
                parts = PurePosixPath(member.filename).parts
                if not parts or parts[0] != 'CesiumForUnreal' or '..' in parts or '\\' in member.filename:
                    raise ValueError('Unexpected archive path')
                target = inside('Plugins/' + member.filename)
                if destination != target and destination not in target.parents:
                    raise ValueError('Archive path escapes plugin directory')
                if stat.S_ISLNK(member.external_attr >> 16):
                    raise ValueError('Unexpected symlink in archive')
            destination.parent.mkdir(parents=True, exist_ok=True)
            release.extractall(destination.parent)
        print('Cesium extracted; existing installations are never overwritten.')
    descriptor = destination / 'CesiumForUnreal.uplugin'
    installed = json.loads(descriptor.read_text(encoding='utf-8-sig'))
    if installed['VersionName'] != pin['version'] or installed['EngineVersion'] != '5.8.0':
        raise ValueError('Installed Cesium does not match UE 5.8 / v2.30.0')
    platform = 'Mac' if sys.platform == 'darwin' else 'Win64' if sys.platform == 'win32' else 'Linux'
    binary = {'Mac': 'libUnrealEditor-CesiumRuntime.dylib', 'Win64': 'UnrealEditor-CesiumRuntime.dll', 'Linux': 'libUnrealEditor-CesiumRuntime.so'}[platform]
    if not (destination / 'Binaries' / platform / binary).is_file():
        raise ValueError(f'Cesium editor binary is missing for {platform}')
    print(f'Cesium {pin["version"]} ready for {platform}. Open IkebukuroNinjaCity.uproject with UE 5.8.')


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, KeyError, zipfile.BadZipFile) as error:
        raise SystemExit(f'Setup failed: {error}')
