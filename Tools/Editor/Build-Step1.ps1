param([string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8')
$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$projectFile = Join-Path $projectRoot 'IkebukuroNinjaCity.uproject'
$logDir = Join-Path $projectRoot 'Saved\Logs'
New-Item -ItemType Directory -Path $logDir -Force | Out-Null
# Running this script accesses the external UE/compiler/SDK installations.
# Obtain user approval for that access before executing it in an agent session.
& (Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat') `
    IkebukuroNinjaCityEditor Win64 Development "-Project=$projectFile" `
    -WaitMutex -NoHotReloadFromIDE -NoUBA `
    "-UBARootDir=$(Join-Path $projectRoot 'Saved\UnrealBuildAccelerator')" `
    "-Log=$(Join-Path $logDir 'Step1-Build.log')"
if ($LASTEXITCODE -ne 0) { throw "UE build failed: $LASTEXITCODE" }
