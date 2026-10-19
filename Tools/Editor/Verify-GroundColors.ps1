param([string]$EngineRoot='C:\Program Files\Epic Games\UE_5.8',[string]$RunName='ground-colors',[switch]$Baseline,[string]$PackagePath)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$report=Join-Path $root "Saved/Validation/$RunName.json"
if (Test-Path -LiteralPath $report) { throw 'Choose a new RunName' }
$env:GIT_CONFIG_NOSYSTEM='1'
$env:GIT_CONFIG_GLOBAL=Join-Path $root 'Saved/Git/empty-config'
$env:GIT_CONFIG_SYSTEM=$env:GIT_CONFIG_GLOBAL
$env:GIT_ATTR_NOSYSTEM='1'
$argsList=@('-CityGroundColorCheck',"-GroundReport=`"$report`"","-GroundScreenshot=`"$(Join-Path $root "Saved/Validation/$RunName")`"",
    '-DDC=Step8Offline','-DisablePlugins=ModelContextProtocol,Fab,EOSShared,EditorTelemetry,EditorToolset,ToolsetRegistry',
    '-ini:Engine:[HTTP]:bEnableHttp=False','-NoEpicPortal','-NoAnalytics','-unattended','-nop4','-nosplash','-NoSound','-windowed','-ResX=1280','-ResY=720',
    '-ExecCmds="t.MaxFPS 60,t.IdleWhenNotForeground 0,r.ScreenPercentage 100"',"-abslog=`"$(Join-Path $root "Saved/Logs/$RunName.log")`"")
if ($Baseline) { $argsList+='-GroundBaseline' }
if ($PackagePath) {
    $package=[IO.Path]::GetFullPath((Join-Path $root $PackagePath))
    if (!$package.StartsWith($root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) { throw 'Package must remain inside the project' }
    $exe=Join-Path $package 'IkebukuroNinjaCity.exe'
    $argsList+="-UserDir=`"$(Join-Path $package 'UserData')`""
} else {
    $exe=Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor.exe'
    $argsList=@(('"'+(Join-Path $root 'IkebukuroNinjaCity.uproject')+'"'),'/Game/Ninja/Maps/L_Ikebukuro','-game')+$argsList
}
$process=Start-Process -FilePath $exe -ArgumentList $argsList -WorkingDirectory $root -WindowStyle Hidden -PassThru
$process.Id | Set-Content (Join-Path $root "Saved/Validation/$RunName.pid")
if (!$process.WaitForExit(180000)) { throw "Ground check timed out; inspect PID $($process.Id)" }
if (!(Test-Path -LiteralPath $report)) { throw 'Ground check report missing' }
$result=Get-Content $report -Raw | ConvertFrom-Json
$result | ConvertTo-Json -Depth 5
if ($result.status -ne 'passed') { throw 'Ground color check failed' }
$log=Join-Path $root "Saved/Logs/$RunName.log"
if ((Test-Path -LiteralPath $log) -and (Select-String -LiteralPath $log -Pattern 'Failed to compile Material' -Quiet)) { throw 'A material failed to compile; inspect captured images and log' }
