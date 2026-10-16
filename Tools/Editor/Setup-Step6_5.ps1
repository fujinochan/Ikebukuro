param([string]$EngineRoot='C:\Program Files\Epic Games\UE_5.8',[switch]$BaselineOnly)
$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$env:GIT_CONFIG_NOSYSTEM='1'
$env:GIT_CONFIG_GLOBAL=Join-Path $projectRoot 'Saved\Git\empty-config'
$env:GIT_CONFIG_SYSTEM=$env:GIT_CONFIG_GLOBAL
$env:GIT_ATTR_NOSYSTEM='1'
if (!$BaselineOnly) {
    & (Join-Path $EngineRoot 'Engine\Binaries\ThirdParty\Python3\Win64\python.exe') (Join-Path $projectRoot 'Tools\City\crop_ikebukuro.py') --textured
    if ($LASTEXITCODE -ne 0) { throw 'LOD2 extraction failed' }
}
$ueArgs=@((Join-Path $projectRoot 'IkebukuroNinjaCity.uproject'),'-run=pythonscript',('-script='+(Join-Path $PSScriptRoot 'setup_step6_5.py')),
    '-DisablePlugins=ModelContextProtocol,Fab,EOSShared,EditorTelemetry','-ini:Engine:[HTTP]:bEnableHttp=False','-NoEpicPortal','-NoAnalytics','-unattended','-nop4','-NullRHI',
    ('-abslog='+(Join-Path $projectRoot 'Saved\Logs\Step6_5-Setup.log')))
if ($BaselineOnly) { $ueArgs+='-TexturePrepareBaseline' }
& (Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe') @ueArgs
if ($LASTEXITCODE -ne 0) { throw 'Step 6.5 setup failed' }
