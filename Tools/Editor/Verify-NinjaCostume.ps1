param([string]$EngineRoot='C:\Program Files\Epic Games\UE_5.8',[string]$RunName='ninja-costume')
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$report=Join-Path $root "Saved/Validation/$RunName.json"
if (Test-Path -LiteralPath $report) { throw 'Choose a new RunName' }
$env:GIT_CONFIG_NOSYSTEM='1'
$env:GIT_CONFIG_GLOBAL=Join-Path $root 'Saved/Git/empty-config'
$env:GIT_CONFIG_SYSTEM=$env:GIT_CONFIG_GLOBAL
$env:GIT_ATTR_NOSYSTEM='1'
$arguments=@(('"'+(Join-Path $root 'IkebukuroNinjaCity.uproject')+'"'),'/Game/Ninja/Maps/L_NinjaCombat','-game','-NinjaAppearanceCheck',
    "-AppearanceReport=`"$report`"","-AppearanceScreenshot=`"$(Join-Path $root "Saved/Validation/$RunName")`"",
    '-DDC=Step8Offline','-DisablePlugins=ModelContextProtocol,Fab,EOSShared,EditorTelemetry,EditorToolset,ToolsetRegistry',
    '-ini:Engine:[HTTP]:bEnableHttp=False','-NoEpicPortal','-NoAnalytics','-unattended','-nop4','-nosplash','-NoSound','-windowed','-ResX=1440','-ResY=1080',
    '-ExecCmds="t.MaxFPS 60,t.IdleWhenNotForeground 0,r.ScreenPercentage 100"',"-abslog=`"$(Join-Path $root "Saved/Logs/$RunName.log")`"")
$process=Start-Process -FilePath (Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor.exe') -ArgumentList $arguments -WindowStyle Hidden -PassThru
$process.Id | Set-Content (Join-Path $root "Saved/Validation/$RunName.pid")
if (!$process.WaitForExit(120000)) { throw "Preview timed out; inspect PID $($process.Id)" }
if (!(Test-Path -LiteralPath $report)) { throw 'Preview report missing' }
$result=Get-Content $report -Raw | ConvertFrom-Json
$result | ConvertTo-Json -Depth 5
if ($result.status -ne 'passed') { throw 'Costume preview failed' }
