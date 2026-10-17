param(
    [ValidateSet('Music','Volley','Match')][string]$Check='Music',
    [string]$RunName='step10-music',
    [switch]$FullTrack,
    [string]$PackagePath='SavedBuilds/Step10/Relocated Windows'
)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$package=[IO.Path]::GetFullPath((Join-Path $root $PackagePath))
if (!$package.StartsWith($root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) { throw 'Package must be inside this project' }
$validation=Join-Path $root 'Saved/Validation'
$report=Join-Path $validation "$RunName.json"
if (Test-Path -LiteralPath $report) { throw "Report already exists: $report" }
$context=Join-Path $root 'SavedBuilds/Step10/RunContext'
New-Item -ItemType Directory -Path $context -Force | Out-Null
$argsList=@("-Ninja${Check}Check", "-${Check}Report=`"$report`"", "-${Check}Screenshot=`"$(Join-Path $validation $RunName)`"",
    '-ini:Engine:[HTTP]:bEnableHttp=False','-NoEpicPortal','-NoAnalytics','-unattended','-nop4','-nosplash',
    '-windowed','-ResX=1280','-ResY=720',"-UserDir=`"$(Join-Path $package 'UserData')`"",
    "-abslog=`"$(Join-Path $root "Saved/Logs/$RunName.log")`"",
    '-ExecCmds="t.MaxFPS 60,t.IdleWhenNotForeground 0"')
if ($Check -ne 'Music') { $argsList+=@('-NinjaLegacyTimerCheck',"-${Check}FPS=60") }
if ($FullTrack) { $argsList+='-MusicFullTrackCheck' }
# Launch the same bootstrap executable players use; do not specify a map or uproject.
$process=Start-Process -FilePath (Join-Path $package 'IkebukuroNinjaCity.exe') -ArgumentList $argsList -WorkingDirectory $context -WindowStyle Hidden -PassThru
$process.Id | Set-Content (Join-Path $validation "$RunName.pid")
$deadline=[DateTime]::UtcNow.AddMinutes(10)
while (!(Test-Path -LiteralPath $report)) {
    if ($process.HasExited -and $process.ExitCode -ne 0) { throw "Packaged process failed with exit code $($process.ExitCode)" }
    if ([DateTime]::UtcNow -gt $deadline) { throw "Timed out; inspect package process started by PID $($process.Id)" }
    Start-Sleep -Seconds 2
}
if (!$process.WaitForExit(10000)) { throw "Report written but packaged process did not exit: $($process.Id)" }
$result=Get-Content -LiteralPath $report -Raw | ConvertFrom-Json
$result | ConvertTo-Json -Depth 6
if ($result.status -ne 'passed') { throw "$Check packaged verification failed" }
if ($Check -eq 'Music') {
    if (!$result.cooked_data -or $result.map -ne 'L_Ikebukuro') { throw 'Unexpected cooked data or startup map' }
    $expectedContent=Join-Path $package 'IkebukuroNinjaCity/Content'
    if ([IO.Path]::GetFullPath($result.content_directory).TrimEnd('/','\') -ne $expectedContent.TrimEnd('/','\')) { throw 'Content resolved outside relocated package' }
    if ($result.local_datasets.Count -ne 2 -or @($result.local_datasets | Where-Object { !$_.exists }).Count -gt 0) { throw 'Local city dataset missing' }
}
