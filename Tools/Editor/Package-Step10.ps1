param([string]$EngineRoot='C:\Program Files\Epic Games\UE_5.8', [switch]$ReuseCookedOutput)
$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$env:GIT_CONFIG_NOSYSTEM='1'
$env:GIT_CONFIG_GLOBAL=Join-Path $projectRoot 'Saved/Git/empty-config'
$env:GIT_CONFIG_SYSTEM=$env:GIT_CONFIG_GLOBAL
$env:GIT_ATTR_NOSYSTEM='1'
$archive=Join-Path $projectRoot 'SavedBuilds/Step10/Package'
$buildArgs=@('-build','-cook')
if ($ReuseCookedOutput) { $buildArgs=@('-build','-skipcook') }
& (Join-Path $EngineRoot 'Engine/Build/BatchFiles/RunUAT.bat') BuildCookRun `
    "-project=$(Join-Path $projectRoot 'IkebukuroNinjaCity.uproject')" `
    -noP4 -unattended -utf8output -NoCompileUAT -platform=Win64 -clientconfig=Shipping `
    @buildArgs -stage -pak -iostore -compressed -prereqs -nodebuginfo -archive "-archivedirectory=$archive" `
    -map=/Game/Ninja/Maps/L_Ikebukuro `
    "-UbtArgs=-NoUBA -UBARootDir=`"$(Join-Path $projectRoot 'Saved/UnrealBuildAccelerator')`"" `
    '-AdditionalCookerOptions=-DDC=Step8Offline -DisablePlugins=ModelContextProtocol,Fab,EOSShared,EditorTelemetry,EditorToolset,ToolsetRegistry -ini:Engine:[HTTP]:bEnableHttp=False -NoAnalytics -NoEpicPortal' `
    2>&1 | Tee-Object -FilePath (Join-Path $projectRoot 'Saved/Logs/Step10-Package.log')
if ($LASTEXITCODE -ne 0) { throw "Packaging failed: $LASTEXITCODE" }
