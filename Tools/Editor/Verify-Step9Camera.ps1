param([string]$EngineRoot='C:\Program Files\Epic Games\UE_5.8',[string]$RunName='step9-camera',[switch]$City,[ValidateSet(30,60,120)][int]$FPS=60)
$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$validation=Join-Path $projectRoot 'Saved/Validation'
$report=Join-Path $validation "$RunName.json"
if (Test-Path -LiteralPath $report) { throw "Report exists: $report" }
$env:GIT_CONFIG_NOSYSTEM='1'
$env:GIT_CONFIG_GLOBAL=Join-Path $projectRoot 'Saved/Git/empty-config'
$env:GIT_CONFIG_SYSTEM=$env:GIT_CONFIG_GLOBAL
$env:GIT_ATTR_NOSYSTEM='1'
$map=if ($City) { '/Game/Ninja/Maps/L_Ikebukuro' } else { '/Game/Ninja/Maps/L_NinjaMatch' }
$ueArgs=@(('"'+(Join-Path $projectRoot 'IkebukuroNinjaCity.uproject')+'"'),$map,'-game','-NinjaCameraCheck',"-CameraFPS=$FPS",'-NinjaLegacyTimerCheck',
    "-CameraReport=`"$report`"","-CameraScreenshot=`"$(Join-Path $validation "$RunName.png")`"",
    '-DDC=Step8Offline','-DisablePlugins=ModelContextProtocol,Fab,EOSShared,EditorTelemetry,EditorToolset,ToolsetRegistry',
    '-ini:Engine:[HTTP]:bEnableHttp=False','-NoEpicPortal','-NoAnalytics','-unattended','-nop4','-nosplash','-windowed','-ResX=1920','-ResY=1080',
    '-ExecCmds="t.MaxFPS 60,t.IdleWhenNotForeground 0,r.ScreenPercentage 80,sg.GlobalIlluminationQuality 0,sg.ReflectionQuality 0,sg.ShadowQuality 1"',
    "-abslog=`"$(Join-Path $projectRoot "Saved/Logs/$RunName.log")`""
)
if (!$City) { $ueArgs+='-NullRHI' }


$process=Start-Process -FilePath (Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor.exe') -ArgumentList $ueArgs -WindowStyle Hidden -PassThru
$process.Id | Set-Content (Join-Path $validation "$RunName.pid")
if (!$process.WaitForExit(600000)) { throw "Camera check timed out; inspect process $($process.Id)" }
if (!(Test-Path -LiteralPath $report)) { throw 'Camera report missing' }
$result=Get-Content $report -Raw | ConvertFrom-Json
$result | ConvertTo-Json -Depth 5
if ($result.status -ne 'passed') { throw 'Camera verification failed' }
