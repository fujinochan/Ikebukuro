#!/bin/bash
# Preserve macOS bundle metadata and executable modes using Apple's ditto.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PACKAGE="$ROOT/SavedBuilds/Mac/Package"
MODE="${1:-local}"
if [[ $# -gt 1 || ( "$MODE" != local && "$MODE" != signed ) ]]; then
  echo 'Usage: bash Tools/Mac/archive.sh [local|signed]' >&2
  exit 2
fi
python3 "$ROOT/Tools/Mac/verify_package.py"
APP="$(find "$PACKAGE" -type d -name 'IkebukuroNinjaCity*.app' -prune)"
if [[ "$MODE" == signed ]]; then
  codesign --verify --deep --strict --verbose=2 "$APP"
  spctl --assess --type execute --verbose=2 "$APP"
  xcrun stapler validate "$APP"
fi
ARCHIVE="$ROOT/SavedBuilds/Mac/IkebukuroNinjaCity-1.0.0-Mac-arm64-$MODE.zip"
if [[ -e "$ARCHIVE" ]]; then
  echo 'Archive already exists; move it within this project before creating a new release.' >&2
  exit 1
fi
# Archive the entire UAT output: loose Non-UFS data may live outside the app.
ditto -c -k --sequesterRsrc --keepParent "$PACKAGE" "$ARCHIVE"
python3 -m zipfile -t "$ARCHIVE"
shasum -a 256 "$ARCHIVE" > "$ARCHIVE.sha256"
echo "$ARCHIVE"
