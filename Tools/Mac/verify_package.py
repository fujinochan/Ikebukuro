#!/usr/bin/env python3
"""Check Mac Non-UFS staging against the pinned runtime manifest, offline."""
import argparse
import hashlib
import json
import plistlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def digest(path):
    result = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            result.update(block)
    return result.hexdigest()


def verify(package):
    package = package.resolve()
    if not package.is_relative_to(ROOT) or package == ROOT:
        raise ValueError('Package must be inside this project')
    apps = [p for p in package.rglob('IkebukuroNinjaCity*.app')
            if (p / 'Contents/Info.plist').is_file()]
    if len(apps) != 1:
        raise ValueError(f'Expected one game app, found {len(apps)}')
    app = apps[0]
    info = plistlib.loads((app / 'Contents/Info.plist').read_bytes())
    executable = (app / 'Contents/MacOS' / info['CFBundleExecutable']).resolve()
    if not executable.is_relative_to(package):
        raise ValueError('Executable escapes package')
    if not executable.is_file() or not executable.stat().st_mode & 0o111:
        raise ValueError('Missing executable or lost executable permission')
    manifest = json.loads((ROOT / 'doc/project-runtime-files.json').read_text())
    rows = []
    # UE archives may place the game Content beside the app or under its bundle.
    content_roots = [p.parent for p in package.rglob('Content/Movies/Ikebukuro Ninja City.m4a')]
    if len(content_roots) != 1:
        raise ValueError('Expected exactly one staged game Content/Movies directory')
    content = content_roots[0].parent
    for item in manifest['files']:
        relative = Path(item['path'])
        if relative.parts[0] != 'Content':
            raise ValueError(f'Unexpected runtime manifest entry: {relative}')
        target = (content / Path(*relative.parts[1:])).resolve()
        if not target.is_relative_to(package):
            raise ValueError('Staged data escapes package')
        if not target.is_file() or digest(target) != item['sha256']:
            raise ValueError(f'Missing or altered staged runtime data: {relative}')
        rows.append(str(target.relative_to(package)))
    for original in (ROOT / 'Content/Distribution').rglob('*'):
        if original.is_file():
            relative = original.relative_to(ROOT / 'Content')
            staged = (content / relative).resolve()
            if not staged.is_relative_to(package) or not staged.is_file() or digest(staged) != digest(original):
                raise ValueError(f'Missing or altered attribution/instructions: {relative}')
    if not list(package.rglob('*.utoc')) or not list(package.rglob('*.ucas')):
        raise ValueError('Missing cooked IoStore containers')
    report = {'status': 'passed', 'scope': 'staged data only; runtime and signing not verified',
              'app': str(app.relative_to(ROOT)), 'runtime_files_verified': len(rows)}
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--package', default='SavedBuilds/Mac/Package')
    args = parser.parse_args()
    report = verify(ROOT / args.package)
    output = ROOT / 'Saved/Validation/mac-staging.json'
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n')
    print(json.dumps(report, ensure_ascii=False, indent=2))


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, KeyError) as error:
        raise SystemExit(f'Mac staging verification failed: {error}')
