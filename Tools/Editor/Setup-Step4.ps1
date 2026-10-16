param([string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8')
$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$env:GIT_CONFIG_NOSYSTEM = '1'
$env:GIT_CONFIG_GLOBAL = Join-Path $projectRoot 'Saved\Git\empty-config'
$env:GIT_CONFIG_SYSTEM = $env:GIT_CONFIG_GLOBAL
$env:GIT_ATTR_NOSYSTEM = '1'
& (Join-Path $EngineRoot 'Engine\Binaries\ThirdParty\Python3\Win64\python.exe') (Join-Path $projectRoot 'Tools\City\crop_ikebukuro.py')
if ($LASTEXITCODE -ne 0) { throw 'City crop failed' }
$ueArgs = @(
    (Join-Path $projectRoot 'IkebukuroNinjaCity.uproject'),
    '-run=pythonscript', ('-script=' + (Join-Path $projectRoot 'Tools\Editor\setup_step4.py')),
    '-DisablePlugins=ModelContextProtocol,Fab,EOSShared,EditorTelemetry',
    '-ini:Engine:[HTTP]:bEnableHttp=False', '-NoEpicPortal', '-NoAnalytics', '-unattended', '-nop4', '-NullRHI',
    ('-abslog=' + (Join-Path $projectRoot 'Saved\Logs\Step4-Setup.log'))
)
& (Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe') @ueArgs
if ($LASTEXITCODE -ne 0) { throw 'City map setup failed' }
