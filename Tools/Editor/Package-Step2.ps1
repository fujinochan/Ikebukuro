param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [switch]$ReuseCookedOutput
)
$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$emptyGitConfig = Join-Path $projectRoot 'Saved\Git\empty-config'
$env:GIT_CONFIG_NOSYSTEM = '1'
$env:GIT_CONFIG_GLOBAL = $emptyGitConfig
$env:GIT_CONFIG_SYSTEM = $emptyGitConfig
$env:GIT_ATTR_NOSYSTEM = '1'
$ubaPath = Join-Path $projectRoot 'Saved\UnrealBuildAccelerator'
$archivePath = Join-Path $projectRoot 'SavedBuilds\Step2'
$cookBuildArgs = @('-build', '-cook')
if ($ReuseCookedOutput) { $cookBuildArgs = @('-skipcook') }
& (Join-Path $EngineRoot 'Engine\Build\BatchFiles\RunUAT.bat') BuildCookRun `
    "-project=$(Join-Path $projectRoot 'IkebukuroNinjaCity.uproject')" `
    -noP4 -unattended -utf8output -NoCompileUAT -platform=Win64 -clientconfig=Development `
    @cookBuildArgs -stage -pak -archive "-archivedirectory=$archivePath" `
    -map=/Game/Ninja/Maps/L_CityProof `
    "-UbtArgs=-NoUBA -UBARootDir=`"$ubaPath`"" `
    '-AdditionalCookerOptions=-DisablePlugins=ModelContextProtocol,Fab,EOSShared,EditorTelemetry -ini:Engine:[HTTP]:bEnableHttp=False -NoAnalytics -NoEpicPortal'
if ($LASTEXITCODE -ne 0) { throw "Packaging failed: $LASTEXITCODE" }
$sourceData = Join-Path $projectRoot 'Content\CityData\ToshimaLOD1'
$stagedData = Join-Path $archivePath 'Windows\IkebukuroNinjaCity\Content\CityData\ToshimaLOD1'
foreach ($file in Get-ChildItem -LiteralPath $sourceData -Recurse -File) {
    $relative = $file.FullName.Substring($sourceData.Length + 1)
    $stagedFile = Join-Path $stagedData $relative
    if (!(Test-Path -LiteralPath $stagedFile) -or (Get-Item -LiteralPath $stagedFile).Length -ne $file.Length) {
        throw "City data was not staged correctly: $relative"
    }
}
"City data staging verified: $stagedData"
