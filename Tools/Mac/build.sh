#!/bin/bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
ENGINE_ROOT="${UE_ENGINE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
if [[ ! -f "$ENGINE_ROOT/Engine/Build/BatchFiles/Mac/Build.sh" ]]; then
  echo "Set UE_ENGINE_ROOT to your UE 5.8 installation." >&2
  exit 1
fi
mkdir -p "$ROOT/Saved/Git" "$ROOT/Saved/Logs"
touch "$ROOT/Saved/Git/empty-config"
export GIT_CONFIG_NOSYSTEM=1 GIT_ATTR_NOSYSTEM=1
export GIT_CONFIG_GLOBAL="$ROOT/Saved/Git/empty-config"
export GIT_CONFIG_SYSTEM="$GIT_CONFIG_GLOBAL"
bash "$ENGINE_ROOT/Engine/Build/BatchFiles/Mac/Build.sh" \
  IkebukuroNinjaCityEditor Mac Development \
  "-Project=$ROOT/IkebukuroNinjaCity.uproject" -WaitMutex -NoHotReloadFromIDE -NoUBA \
  "-Log=$ROOT/Saved/Logs/Mac-Build.log"
