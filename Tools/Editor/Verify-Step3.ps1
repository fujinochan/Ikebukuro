param(
    [ValidateSet(30,60,120)][int]$FPS = 60,
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [string]$RunName = ''
)
$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
if (!$RunName) { $RunName = "step3-movement-${FPS}fps" }
$validation = Join-Path $projectRoot 'Saved\Validation'
New-Item -ItemType Directory -Path $validation -Force | Out-Null
$reportPath = Join-Path $validation "$RunName.json"
if (Test-Path -LiteralPath $reportPath) { throw "Report already exists: $reportPath" }
$env:GIT_CONFIG_NOSYSTEM = '1'
$env:GIT_CONFIG_GLOBAL = Join-Path $projectRoot 'Saved\Git\empty-config'
$env:GIT_CONFIG_SYSTEM = $env:GIT_CONFIG_GLOBAL
$env:GIT_ATTR_NOSYSTEM = '1'
$ueArgs = @(
    '"' + (Join-Path $projectRoot 'IkebukuroNinjaCity.uproject') + '"',
    '/Game/Ninja/Maps/L_NinjaTraining', '-game', '-NinjaMovementCheck', "-NinjaCheckFPS=$FPS",
    "-NinjaCheckReport=`"$reportPath`"",
    "-NinjaCheckScreenshot=`"$(Join-Path $validation "$RunName.png")`"",
    '-DisablePlugins=ModelContextProtocol,Fab,EOSShared,EditorTelemetry',
    '-ini:Engine:[HTTP]:bEnableHttp=False', '-NoEpicPortal', '-NoAnalytics',
    '-unattended', '-nop4', '-nosplash', '-NoSound', '-windowed', '-ResX=1280', '-ResY=720',
    "-abslog=`"$(Join-Path $projectRoot "Saved\Logs\$RunName.log")`""
)
$process = Start-Process -FilePath (Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe') `
    -ArgumentList $ueArgs -WindowStyle Hidden -PassThru
$process.Id | Set-Content -LiteralPath (Join-Path $validation "$RunName.pid")
if (!$process.WaitForExit(210000)) { throw "Verification timed out; inspect process $($process.Id)" }
if (!(Test-Path -LiteralPath $reportPath)) { throw 'Movement check did not write a report' }
$report = Get-Content -LiteralPath $reportPath -Raw | ConvertFrom-Json
$report | ConvertTo-Json -Depth 6
if ($report.status -ne 'passed') { throw 'Movement verification failed' }
