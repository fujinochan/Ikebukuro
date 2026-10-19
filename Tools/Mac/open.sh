#!/bin/bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
ENGINE_ROOT="${UE_ENGINE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
EDITOR="$ENGINE_ROOT/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
if [[ ! -x "$EDITOR" ]]; then
  echo "Set UE_ENGINE_ROOT to your UE 5.8 installation." >&2
  exit 1
fi
mkdir -p "$ROOT/Saved/Logs" "$ROOT/Saved/Git"
touch "$ROOT/Saved/Git/empty-config"
export GIT_CONFIG_NOSYSTEM=1 GIT_ATTR_NOSYSTEM=1
export GIT_CONFIG_GLOBAL="$ROOT/Saved/Git/empty-config"
export GIT_CONFIG_SYSTEM="$GIT_CONFIG_GLOBAL"
exec "$EDITOR" "$ROOT/IkebukuroNinjaCity.uproject" \
  /Game/Ninja/Maps/L_Ikebukuro -DDC=Step8Offline \
  -DisablePlugins=ModelContextProtocol,Fab,EOSShared,EditorTelemetry,EditorToolset,ToolsetRegistry \
  -ini:Engine:[HTTP]:bEnableHttp=False -NoEpicPortal -NoAnalytics \
  "-abslog=$ROOT/Saved/Logs/Mac-Editor.log" "$@"
