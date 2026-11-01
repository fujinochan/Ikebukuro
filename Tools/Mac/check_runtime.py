#!/usr/bin/env python3
"""Run the existing full-track music/city lifecycle check in a packaged Mac app."""
import argparse
import json
import plistlib
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--app', required=True, help='Path to the packaged app inside this project')
    parser.add_argument('--report-only', action='store_true', help='Validate the existing report without launching')
    parser.add_argument('--check', choices=('Music', 'Volley', 'Match'), default='Music')
    args = parser.parse_args()
    app = (ROOT / args.app).resolve()
    if not app.is_relative_to(ROOT) or app.suffix != '.app':
        raise ValueError('App must be inside this project')
    info = plistlib.loads((app / 'Contents/Info.plist').read_bytes())
    executable = (app / 'Contents/MacOS' / info['CFBundleExecutable']).resolve()
    if not executable.is_relative_to(app):
        raise ValueError('Executable escapes app')
    folder = ROOT / 'Saved/Validation'
    folder.mkdir(parents=True, exist_ok=True)
    report = folder / f'mac-runtime-{args.check.lower()}.json'
    if not args.report_only:
        if report.exists():
            raise ValueError('Runtime report exists; move it within the project before rerunning')
        log = folder / f'mac-runtime-{args.check.lower()}-process.log'
        flags = ['-MusicFullTrackCheck'] if args.check == 'Music' else ['-NinjaLegacyTimerCheck', f'-{args.check}FPS=60']
        with log.open('w') as stream:
            subprocess.run(['open', '-W', '-n', str(app), '--args', f'-Ninja{args.check}Check', *flags,
                            f'-{args.check}Report={report}', f'-{args.check}Screenshot={folder / ("mac-runtime-" + args.check.lower() + ".png")}', '-unattended', '-NoEpicPortal', '-NoAnalytics',
                            '-ini:Engine:[HTTP]:bEnableHttp=False', '-windowed', '-ResX=1280', '-ResY=720',
                            f'-abslog={folder / ("mac-runtime-" + args.check.lower() + ".log")}'],
                           cwd=folder, stdout=stream, stderr=subprocess.STDOUT, timeout=660, check=True)
    result = json.loads(report.read_bytes())
    if result.get('status') != 'passed':
        raise ValueError(f'Packaged runtime check failed: {result}')
    if args.check == 'Music':
        # The successful lifecycle suite closes its player before writing the report.
        # A live player at the end would contradict its teardown checks.
        if result.get('media_player') not in ('None', 'AvfMedia'):
            raise ValueError('Unexpected media backend at the end of the Mac lifecycle check')
        if not result.get('cooked_data') or result.get('map') != 'L_Ikebukuro' or not result.get('full_track'):
            raise ValueError('Unexpected cooked map or incomplete full-track test')
        if not Path(result['content_directory']).resolve().is_relative_to(app):
            raise ValueError('Runtime Content resolved outside the relocated app')
        datasets = result.get('local_datasets', [])
        if len(datasets) < 2 or not all(row.get('exists') for row in datasets):
            raise ValueError('Missing local city datasets')
    print(json.dumps(result, ensure_ascii=False, indent=2))


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, KeyError, subprocess.SubprocessError) as error:
        raise SystemExit(f'Mac runtime verification failed: {error}')
