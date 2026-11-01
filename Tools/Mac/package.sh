#!/bin/bash
# Build an Apple Silicon Shipping archive. No dependency downloads.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
ENGINE_ROOT="${UE_ENGINE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
OUTPUT="$ROOT/SavedBuilds/Mac/Package"
if [[ $# -ne 0 ]]; then
  echo 'Usage: bash Tools/Mac/package.sh (UE_ENGINE_ROOT selects UE 5.8)' >&2
  exit 2
fi
if [[ ! -f "$ENGINE_ROOT/Engine/Build/BatchFiles/RunUAT.sh" ]]; then
  echo 'Set UE_ENGINE_ROOT to the installed UE 5.8 directory.' >&2
  exit 1
fi
# Fail before a long Cook if Xcode's separately installed Metal component is missing.
xcrun metal --version >/dev/null
if [[ -e "$OUTPUT" ]]; then
  echo 'Package output already exists. Move it within this project before rebuilding.' >&2
  exit 1
fi
mkdir -p "$ROOT/Saved/Git" "$ROOT/Saved/Logs"
touch "$ROOT/Saved/Git/empty-config"
export GIT_CONFIG_NOSYSTEM=1 GIT_ATTR_NOSYSTEM=1
export GIT_CONFIG_GLOBAL="$ROOT/Saved/Git/empty-config"
export GIT_CONFIG_SYSTEM="$GIT_CONFIG_GLOBAL"
export uebp_LogFolder="$ROOT/Saved/Logs/Mac-UAT"
export uebp_FinalLogFolder="$uebp_LogFolder"
export uebp_EngineSavedFolder="$ROOT/Saved/Mac-AutomationTool"
python3 "$ROOT/Tools/setup_project.py" --check
python3 "$ROOT/Tools/Mac/patch_cesium.py"
bash "$ENGINE_ROOT/Engine/Build/BatchFiles/RunUAT.sh" BuildCookRun \
  "-project=$ROOT/IkebukuroNinjaCity.uproject" \
  -noP4 -unattended -utf8output -platform=Mac -architecture=arm64 \
  -clientconfig=Shipping -build -cook -stage -pak -iostore -compressed \
  -nodebuginfo -package -archive "-archivedirectory=$OUTPUT" \
  -map=/Game/Ninja/Maps/L_Ikebukuro \
  '-UbtArgs=-NoUBA' \
  '-AdditionalCookerOptions=-DDC=Step8Offline -DisablePlugins=ModelContextProtocol,Fab,EOSShared,EditorTelemetry,EditorToolset,ToolsetRegistry -ini:Engine:[HTTP]:bEnableHttp=False -NoAnalytics -NoEpicPortal' \
  2>&1 | tee "$ROOT/Saved/Logs/Mac-Package.log"
# Xcode injects get-task-allow for local signing; remove it from the release app.
codesign --force --sign - --timestamp=none \
  --entitlements "$ROOT/Build/Mac/Resources/Ikebukuro.entitlements" \
  "$OUTPUT/IkebukuroNinjaCity-Mac-Shipping.app"
codesign --verify --deep --strict "$OUTPUT/IkebukuroNinjaCity-Mac-Shipping.app"
python3 "$ROOT/Tools/Mac/verify_package.py"
echo 'Shipping archive verified. Follow doc/mac-distribution.md for runtime checks and signing.'
