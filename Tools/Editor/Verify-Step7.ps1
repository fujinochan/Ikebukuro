param([string]$EngineRoot='C:\Program Files\Epic Games\UE_5.8',[int]$FPS=60,[string]$RunName='step7-match',[switch]$City,[switch]$FatalFall)
$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$validation=Join-Path $projectRoot 'Saved\Validation'
$report=Join-Path $validation "$RunName.json"
if (Test-Path -LiteralPath $report) { throw "Report exists: $report" }
$env:GIT_CONFIG_NOSYSTEM='1'
$env:GIT_CONFIG_GLOBAL=Join-Path $projectRoot 'Saved\Git\empty-config'
$env:GIT_CONFIG_SYSTEM=$env:GIT_CONFIG_GLOBAL
$env:GIT_ATTR_NOSYSTEM='1'
$map=if ($City) { '/Game/Ninja/Maps/L_Ikebukuro' } else { '/Game/Ninja/Maps/L_NinjaMatch' }
$ueArgs=@(
    ('"'+(Join-Path $projectRoot 'IkebukuroNinjaCity.uproject')+'"'),
    $map,'-game','-NinjaMatchCheck',"-MatchFPS=$FPS",
    "-MatchReport=`"$report`"", "-MatchScreenshot=`"$(Join-Path $validation "$RunName.png")`"",
    '-DisablePlugins=ModelContextProtocol,Fab,EOSShared,EditorTelemetry',
    '-ini:Engine:[HTTP]:bEnableHttp=False','-NoEpicPortal','-NoAnalytics','-unattended','-nop4','-nosplash','-NoSound','-windowed','-ResX=1280','-ResY=720',
    '-ExecCmds="t.MaxFPS 60,t.IdleWhenNotForeground 0,r.ScreenPercentage 80,sg.GlobalIlluminationQuality 0,sg.ReflectionQuality 0,sg.ShadowQuality 1"',
    "-abslog=`"$(Join-Path $projectRoot "Saved\Logs\$RunName.log")`""
)
if ($FatalFall) { $ueArgs+='-MatchFatalFall' }
$process=Start-Process -FilePath (Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe') -ArgumentList $ueArgs -WindowStyle Hidden -PassThru
$process.Id | Set-Content (Join-Path $validation "$RunName.pid")
if (!$process.WaitForExit(150000)) { throw "Match verification timed out; inspect process $($process.Id)" }
if (!(Test-Path -LiteralPath $report)) { throw 'Match report missing' }
$result=Get-Content $report -Raw | ConvertFrom-Json
$result | ConvertTo-Json -Depth 5
if ($result.status -ne 'passed') { throw 'Match verification failed' }
