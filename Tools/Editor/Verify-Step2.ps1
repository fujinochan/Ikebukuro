param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [string]$GameExecutable = '',
    [string]$ReportName = 'step2-city-editor-runtime'
)
$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$validation = Join-Path $projectRoot 'Saved\Validation'
New-Item -ItemType Directory -Path $validation -Force | Out-Null
$emptyGitConfig = Join-Path $projectRoot 'Saved\Git\empty-config'
$env:GIT_CONFIG_NOSYSTEM = '1'
$env:GIT_CONFIG_GLOBAL = $emptyGitConfig
$env:GIT_CONFIG_SYSTEM = $emptyGitConfig
$env:GIT_ATTR_NOSYSTEM = '1'
$reportPath = Join-Path $validation "$ReportName.json"
if (Test-Path -LiteralPath $reportPath) { throw "Report already exists: $reportPath" }
$ueArgs = @()
if (!$GameExecutable) {
    $GameExecutable = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
    $ueArgs += '"' + (Join-Path $projectRoot 'IkebukuroNinjaCity.uproject') + '"'
    $ueArgs += '/Game/Ninja/Maps/L_CityProof', '-game'
}
$ueArgs += @(
    '-CitySmokeTest', "-CitySmokeReport=`"$reportPath`"",
    "-CitySmokeScreenshot=`"$(Join-Path $validation "$ReportName.png")`"",
    '-DisablePlugins=ModelContextProtocol,Fab,EOSShared,EditorTelemetry',
    '-ini:Engine:[HTTP]:bEnableHttp=False', '-NoEpicPortal', '-NoAnalytics',
    '-unattended', '-nosplash', '-NoSound', '-windowed', '-ResX=1280', '-ResY=720',
    '-ExecCmds="t.MaxFPS 30"',
    "-abslog=`"$(Join-Path $projectRoot "Saved\Logs\$ReportName.log")`""
)
$process = Start-Process -FilePath $GameExecutable -ArgumentList $ueArgs -WindowStyle Hidden -PassThru
$process.Id | Set-Content -LiteralPath (Join-Path $validation "$ReportName.pid")
if (!$process.WaitForExit(240000)) { throw "Verification timed out; inspect process $($process.Id) and logs" }
if (!(Test-Path -LiteralPath $reportPath)) { throw 'Runtime did not write a report' }
$report = Get-Content -LiteralPath $reportPath -Raw | ConvertFrom-Json
$report | ConvertTo-Json -Depth 5
if ($report.status -ne 'passed') { throw 'Runtime verification failed' }
if ([Math]::Abs($report.one_metre_in_unreal_cm - 100) -gt 0.01) { throw 'Unexpected metre/cm conversion' }
