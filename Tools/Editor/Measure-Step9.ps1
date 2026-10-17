param([string]$EngineRoot='C:\Program Files\Epic Games\UE_5.8',[string]$RunName='step9-perf')
$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$validation=Join-Path $projectRoot 'Saved/Validation'
$report=Join-Path $validation "$RunName.json"
if (Test-Path -LiteralPath $report) { throw "Report exists: $report" }
$env:GIT_CONFIG_NOSYSTEM='1'
$env:GIT_CONFIG_GLOBAL=Join-Path $projectRoot 'Saved/Git/empty-config'
$env:GIT_CONFIG_SYSTEM=$env:GIT_CONFIG_GLOBAL
$env:GIT_ATTR_NOSYSTEM='1'
$map='/Game/Ninja/Maps/L_Ikebukuro'
$ueArgs=@(('"'+(Join-Path $projectRoot 'IkebukuroNinjaCity.uproject')+'"'),$map,'-game','-NinjaPerformanceCheck','-NinjaLegacyTimerCheck',
    "-PerfReport=`"$report`"","-PerfScreenshot=`"$(Join-Path $validation "$RunName-scene")`"",
    '-DDC=Step8Offline','-DisablePlugins=ModelContextProtocol,Fab,EOSShared,EditorTelemetry,EditorToolset,ToolsetRegistry',
    '-ini:Engine:[HTTP]:bEnableHttp=False','-NoEpicPortal','-NoAnalytics','-unattended','-nop4','-nosplash','-NoSound','-windowed','-ForceRes','-ResX=1920','-ResY=1080',
    '-ExecCmds="t.MaxFPS 0,r.VSync 0,t.IdleWhenNotForeground 0,r.ScreenPercentage 100"',
    "-abslog=`"$(Join-Path $projectRoot "Saved/Logs/$RunName.log")`""
)



$process=Start-Process -FilePath (Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor.exe') -ArgumentList $ueArgs -WindowStyle Hidden -PassThru
$process.Id | Set-Content (Join-Path $validation "$RunName.pid")
if (!$process.WaitForExit(600000)) { throw "Performance measurement timed out; inspect process $($process.Id)" }
if (!(Test-Path -LiteralPath $report)) { throw 'Performance report missing' }
$result=Get-Content $report -Raw | ConvertFrom-Json
$result | ConvertTo-Json -Depth 5
if ($result.status -ne 'passed') { throw 'Performance measurement failed' }
