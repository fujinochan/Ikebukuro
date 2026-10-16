param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [string]$RunName = 'step4-city-play-area'
)
$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$validation = Join-Path $projectRoot 'Saved\Validation'
$reportPath = Join-Path $validation "$RunName.json"
if (Test-Path -LiteralPath $reportPath) { throw "Report already exists: $reportPath" }
$env:GIT_CONFIG_NOSYSTEM = '1'
$env:GIT_CONFIG_GLOBAL = Join-Path $projectRoot 'Saved\Git\empty-config'
$env:GIT_CONFIG_SYSTEM = $env:GIT_CONFIG_GLOBAL
$env:GIT_ATTR_NOSYSTEM = '1'
$ueArgs = @(
    '"' + (Join-Path $projectRoot 'IkebukuroNinjaCity.uproject') + '"',
    '/Game/Ninja/Maps/L_Ikebukuro', '-game', '-CityAreaCheck',
    "-CityAreaReport=`"$reportPath`"",
    "-CityAreaScreenshot=`"$(Join-Path $validation "$RunName.png")`"",
    '-DisablePlugins=ModelContextProtocol,Fab,EOSShared,EditorTelemetry',
    '-ini:Engine:[HTTP]:bEnableHttp=False', '-NoEpicPortal', '-NoAnalytics',
    '-unattended', '-nop4', '-nosplash', '-NoSound', '-windowed', '-ResX=1280', '-ResY=720',
    '-ExecCmds="t.MaxFPS 60,t.IdleWhenNotForeground 0,r.ScreenPercentage 70,sg.GlobalIlluminationQuality 0,sg.ReflectionQuality 0,sg.ShadowQuality 1"',
    "-abslog=`"$(Join-Path $projectRoot "Saved\Logs\$RunName.log")`""
)
$process = Start-Process -FilePath (Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe') `
    -ArgumentList $ueArgs -WindowStyle Hidden -PassThru
$process.Id | Set-Content -LiteralPath (Join-Path $validation "$RunName.pid")
if (!$process.WaitForExit(180000)) { throw "City verification timed out; inspect process $($process.Id)" }
if (!(Test-Path -LiteralPath $reportPath)) { throw 'City check did not write a report' }
$report = Get-Content -LiteralPath $reportPath -Raw | ConvertFrom-Json
$report | ConvertTo-Json -Depth 5
if ($report.status -ne 'passed') { throw 'City integration verification failed' }
