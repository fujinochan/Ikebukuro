param([switch]$Relocate,[string]$RelocationName='Relocated Windows')
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$package=Join-Path $root 'SavedBuilds/Step10/Package/Windows'
if (!(Test-Path -LiteralPath (Join-Path $package 'IkebukuroNinjaCity.exe'))) { throw 'Packaged bootstrap executable missing' }
# Keep the original build symbols under Binaries; exclude their duplicate from the release.
$pdb=Join-Path $package 'IkebukuroNinjaCity/Binaries/Win64/IkebukuroNinjaCity-Win64-Shipping.pdb'
if (Test-Path -LiteralPath $pdb) { Remove-Item -LiteralPath $pdb }
Copy-Item -LiteralPath (Join-Path $root 'Content/Distribution/README-ja.md') -Destination $package
Copy-Item -LiteralPath (Join-Path $root 'Content/Distribution/CREDITS.md') -Destination $package
Copy-Item -LiteralPath (Join-Path $root 'Content/Distribution/Licenses') -Destination $package -Recurse -Force
$rows=@()
foreach ($directory in @('CityData/IkebukuroLOD2/Buildings','CityData/Ikebukuro/Roads','Movies','Distribution')) {
    $source=Join-Path $root "Content/$directory"
    foreach ($file in Get-ChildItem -LiteralPath $source -Recurse -File) {
        $relative=$file.FullName.Substring((Join-Path $root 'Content').Length+1)
        $target=Join-Path $package "IkebukuroNinjaCity/Content/$relative"
        if (!(Test-Path -LiteralPath $target)) { throw "Missing staged file: $relative" }
        $hash=(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
        if ((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ne $hash) { throw "Staged hash mismatch: $relative" }
        $rows+=@{path=$relative; bytes=$file.Length; sha256=$hash}
    }
}
@{status='passed'; files=$rows; scope='SHA-256 equality of every required loose dataset, movie and distribution file'} | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $root 'Saved/Validation/step10-staging.json') -Encoding UTF8
if ($Relocate) {
    if ($RelocationName -notmatch '^[A-Za-z0-9 _-]+$') { throw 'Use a simple folder name for relocation' }
    $destination=Join-Path $root "SavedBuilds/Step10/$RelocationName"
    if (Test-Path -LiteralPath $destination) { throw 'Relocation destination already exists; refusing to overwrite' }
    Copy-Item -LiteralPath $package -Destination $destination -Recurse
}
"Verified $($rows.Count) staged files."
