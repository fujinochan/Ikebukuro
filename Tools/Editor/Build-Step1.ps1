param([string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8')
$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$projectFile = Join-Path $projectRoot 'IkebukuroNinjaCity.uproject'
$logDir = Join-Path $projectRoot 'Saved\Logs'
New-Item -ItemType Directory -Path $logDir -Force | Out-Null
$gitStateDir = Join-Path $projectRoot 'Saved\Git'
New-Item -ItemType Directory -Path $gitStateDir -Force | Out-Null
$emptyGitConfig = Join-Path $gitStateDir 'empty-config'
if (!(Test-Path -LiteralPath $emptyGitConfig)) { Set-Content -LiteralPath $emptyGitConfig -Value '' -NoNewline }
$env:GIT_CONFIG_NOSYSTEM = '1'
$env:GIT_CONFIG_GLOBAL = $emptyGitConfig
$env:GIT_CONFIG_SYSTEM = $emptyGitConfig
$env:GIT_ATTR_NOSYSTEM = '1'
# Running this script accesses the external UE/compiler/SDK installations.
# Obtain user approval for that access before executing it in an agent session.
& (Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat') `
    IkebukuroNinjaCityEditor Win64 Development "-Project=$projectFile" `
    -WaitMutex -NoHotReloadFromIDE -NoUBA `
    "-UBARootDir=$(Join-Path $projectRoot 'Saved\UnrealBuildAccelerator')" `
    "-Log=$(Join-Path $logDir 'Step1-Build.log')"
if ($LASTEXITCODE -ne 0) { throw "UE build failed: $LASTEXITCODE" }
