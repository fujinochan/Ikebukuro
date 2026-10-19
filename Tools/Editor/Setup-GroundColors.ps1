param([string]$EngineRoot='C:\Program Files\Epic Games\UE_5.8')
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$backup=Join-Path $root 'Backups/GroundColors-20261019'
if (!(Test-Path -LiteralPath $backup)) {
    New-Item -Path $backup -ItemType Directory | Out-Null
    Copy-Item -LiteralPath (Join-Path $root 'Content/Ninja/Maps/L_Ikebukuro.umap') -Destination $backup
}
$env:GIT_CONFIG_NOSYSTEM='1'
$env:GIT_CONFIG_GLOBAL=Join-Path $root 'Saved/Git/empty-config'
$env:GIT_CONFIG_SYSTEM=$env:GIT_CONFIG_GLOBAL
$env:GIT_ATTR_NOSYSTEM='1'
& (Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe') `
    (Join-Path $root 'IkebukuroNinjaCity.uproject') -run=pythonscript "-script=$(Join-Path $PSScriptRoot 'setup_ground_colors.py')" `
    -DDC=Step8Offline '-DisablePlugins=ModelContextProtocol,Fab,EOSShared,EditorTelemetry,EditorToolset,ToolsetRegistry' `
    '-ini:Engine:[HTTP]:bEnableHttp=False' -NoEpicPortal -NoAnalytics -unattended -nop4 -NullRHI `
    "-abslog=$(Join-Path $root 'Saved/Logs/GroundColors-Setup.log')"
if ($LASTEXITCODE -ne 0) { throw 'Ground color setup failed' }
