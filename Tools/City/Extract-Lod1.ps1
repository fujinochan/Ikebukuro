$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$sourceZip = Join-Path $projectRoot '13116_toshima-ku_pref_2025_3dtiles_mvt_1_op.zip'
$destination = Join-Path $projectRoot 'Data\City\ToshimaLOD1'
$prefix = '13116_toshima-ku_pref_2025_citygml_1_op_bldg_3dtiles_13116_toshima-ku_lod1/'
New-Item -ItemType Directory -Path $destination -Force | Out-Null
Add-Type -AssemblyName System.IO.Compression
$archive = [IO.Compression.ZipFile]::OpenRead($sourceZip)
$manifest = @()
try {
    foreach ($entry in $archive.Entries) {
        if (!$entry.FullName.StartsWith($prefix) -or !$entry.Name) { continue }
        $relative = $entry.FullName.Substring($prefix.Length)
        $target = [IO.Path]::GetFullPath((Join-Path $destination $relative))
        if (!$target.StartsWith($destination + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
            throw "Entry escapes destination: $relative"
        }
        New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($target)) -Force | Out-Null
        if (!(Test-Path -LiteralPath $target)) {
            [IO.Compression.ZipFileExtensions]::ExtractToFile($entry, $target, $false)
        }
        if ((Get-Item -LiteralPath $target).Length -ne $entry.Length) {
            throw "Extracted size mismatch: $relative"
        }
        $manifest += [pscustomobject]@{ Path = $relative; Bytes = $entry.Length }
    }
} finally { $archive.Dispose() }
if ($manifest.Count -eq 0) { throw 'No LOD1 entries found' }
$reportDir = Join-Path $projectRoot 'Saved\Validation'
New-Item -ItemType Directory -Path $reportDir -Force | Out-Null
$report = [pscustomobject]@{
    SourceZip = [IO.Path]::GetFileName($sourceZip)
    ArchivePrefix = $prefix
    Destination = 'Data/City/ToshimaLOD1'
    Files = $manifest
    TotalBytes = ($manifest | Measure-Object Bytes -Sum).Sum
}
$report | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $reportDir 'step2-lod1-extraction.json') -Encoding utf8
"LOD1 extracted: $($manifest.Count) files, $($report.TotalBytes) bytes"
