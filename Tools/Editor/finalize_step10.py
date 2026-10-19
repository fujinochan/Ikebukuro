"""Create a local release archive and integrity manifest; no network access."""
import hashlib
import json
import argparse
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser()
parser.add_argument('--build-directory', default='SavedBuilds/Step10')
parser.add_argument('--archive-name', default='IkebukuroNinjaCity-1.0.0-Windows.zip')
parser.add_argument('--report-name', default='step10-release')
args = parser.parse_args()
release_root = (ROOT / args.build_directory).resolve()
if not release_root.is_relative_to(ROOT) or Path(args.archive_name).name != args.archive_name or Path(args.report_name).name != args.report_name:
    raise RuntimeError('Outputs must remain inside the project')
PACKAGE = release_root / 'Package/Windows'
ARCHIVE = release_root / args.archive_name
if not (PACKAGE / 'IkebukuroNinjaCity.exe').is_file():
    raise RuntimeError('Package missing')
if ARCHIVE.exists():
    raise RuntimeError('Refusing to replace an existing release archive')

def sha256(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(chunk)
    return digest.hexdigest()

files = sorted(p for p in PACKAGE.rglob('*') if p.is_file() and p.name != 'SHA256-MANIFEST.json')
rows = [{'path': p.relative_to(PACKAGE).as_posix(), 'bytes': p.stat().st_size, 'sha256': sha256(p)} for p in files]
(PACKAGE / 'SHA256-MANIFEST.json').write_text(json.dumps(rows, indent=2, ensure_ascii=False), encoding='utf-8')
with ZipFile(ARCHIVE, 'x', compression=ZIP_DEFLATED, compresslevel=1, allowZip64=True) as out:
    for path in sorted(p for p in PACKAGE.rglob('*') if p.is_file()):
        out.write(path, 'IkebukuroNinjaCity/' + path.relative_to(PACKAGE).as_posix())
with ZipFile(ARCHIVE) as check:
    bad = check.testzip()
    if bad:
        raise RuntimeError('Archive CRC failure: ' + bad)
report = {'status': 'passed', 'files': len(rows), 'package_bytes': sum(row['bytes'] for row in rows),
          'archive': str(ARCHIVE.relative_to(ROOT)), 'archive_bytes': ARCHIVE.stat().st_size,
          'archive_sha256': sha256(ARCHIVE), 'zip_crc_checked': True}
(ROOT / ('Saved/Validation/' + args.report_name + '.json')).write_text(json.dumps(report, indent=2), encoding='utf-8')
print(json.dumps(report, indent=2))
