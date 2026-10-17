"""Create a local release archive and integrity manifest; no network access."""
import hashlib
import json
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED

ROOT = Path(__file__).resolve().parents[2]
PACKAGE = ROOT / 'SavedBuilds/Step10/Package/Windows'
ARCHIVE = ROOT / 'SavedBuilds/Step10/IkebukuroNinjaCity-1.0.0-Windows.zip'
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
(ROOT / 'Saved/Validation/step10-release.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print(json.dumps(report, indent=2))
