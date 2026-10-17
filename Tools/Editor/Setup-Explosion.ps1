param([string]$EngineRoot='C:\Program Files\Epic Games\UE_5.8')
$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$env:GIT_CONFIG_NOSYSTEM='1'
$env:GIT_CONFIG_GLOBAL=Join-Path $projectRoot 'Saved\Git\empty-config'
$env:GIT_CONFIG_SYSTEM=$env:GIT_CONFIG_GLOBAL
$env:GIT_ATTR_NOSYSTEM='1'
$ueArgs=@((Join-Path $projectRoot 'IkebukuroNinjaCity.uproject'),'-run=pythonscript',('-script='+(Join-Path $PSScriptRoot 'setup_explosion_audio.py')),
    '-DDC=Step8Offline','-DisablePlugins=ModelContextProtocol,Fab,EOSShared,EditorTelemetry','-ini:Engine:[HTTP]:bEnableHttp=False','-NoEpicPortal','-NoAnalytics','-unattended','-nop4','-NullRHI',
    ('-abslog='+(Join-Path $projectRoot 'Saved\Logs\Explosion-Setup.log')))
& (Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe') @ueArgs
if ($LASTEXITCODE -ne 0) { throw 'Explosion audio setup failed' }
