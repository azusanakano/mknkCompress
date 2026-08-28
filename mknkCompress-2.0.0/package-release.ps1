[CmdletBinding()]
param([string]$Version = '2.0.0')
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
& (Join-Path $root 'build-windows.ps1') -Architecture All -Configuration Release
$release = Join-Path $root 'release'
if (Test-Path $release) { Remove-Item $release -Recurse -Force }
New-Item -ItemType Directory -Force -Path $release | Out-Null

function Get-PeMachine([string]$Path) {
  $stream = [System.IO.File]::OpenRead($Path)
  try {
    $reader = [System.IO.BinaryReader]::new($stream)
    $stream.Position = 0x3c
    $peOffset = $reader.ReadInt32()
    $stream.Position = $peOffset
    if ($reader.ReadUInt32() -ne 0x00004550) { throw "Invalid PE signature: $Path" }
    return $reader.ReadUInt16()
  }
  finally { $stream.Dispose() }
}

$machineNames = @{
  0x014c = 'x86'
  0x8664 = 'x64'
  0xaa64 = 'ARM64'
}
$expectedMachines = @{
  x64 = 0x8664
  x86 = 0x014c
  ARM64 = 0xaa64
}

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vsPath = & $vswhere -latest -products * -property installationPath
$vsVersion = & $vswhere -latest -products * -property installationVersion
$cmakeVersion = (& cmake --version | Select-Object -First 1)
$os = Get-CimInstance Win32_OperatingSystem
$reportLines = [System.Collections.Generic.List[string]]::new()
$reportLines.Add('mknkCompress 2.0.0 Windows MSVC build report')
$reportLines.Add("BuiltAtUtc: $([DateTime]::UtcNow.ToString('yyyy-MM-ddTHH:mm:ssZ'))")
$reportLines.Add("RunnerOS: $($env:ImageOS)")
$reportLines.Add("RunnerImageVersion: $($env:ImageVersion)")
$reportLines.Add("Windows: $($os.Caption) $($os.Version) build $($os.BuildNumber)")
$reportLines.Add("VisualStudio: $vsVersion")
$reportLines.Add("VisualStudioPath: $vsPath")
$reportLines.Add("CMake: $cmakeVersion")
$reportLines.Add("GitHubRunId: $($env:GITHUB_RUN_ID)")
$reportLines.Add("GitCommit: $($env:GITHUB_SHA)")
$reportLines.Add('Configuration: Release; MSVC static runtime (/MT); C++20; CFG; ASLR; DEP')
$reportLines.Add('CodeSigning: unsigned')
$reportLines.Add('')
$reportLines.Add('PE verification:')

foreach ($arch in @('x64','x86','ARM64')) {
  $exe = Join-Path $root "dist/$arch/mknkCompress.exe"
  $machine = Get-PeMachine $exe
  if ($machine -ne $expectedMachines[$arch]) {
    throw "PE machine mismatch for $arch`: expected 0x$($expectedMachines[$arch].ToString('x4')), got 0x$($machine.ToString('x4'))"
  }
  $signature = Get-AuthenticodeSignature $exe
  $fileVersion = (Get-Item $exe).VersionInfo.FileVersion
  $reportLines.Add("- $arch`: machine=0x$($machine.ToString('x4')) ($($machineNames[$machine])); fileVersion=$fileVersion; signature=$($signature.Status); size=$((Get-Item $exe).Length)")
}

$reportLines.Add('')
$reportLines.Add('Startup smoke test:')
foreach ($arch in @('x64','x86')) {
  $exe = Join-Path $root "dist/$arch/mknkCompress.exe"
  $timer = [System.Diagnostics.Stopwatch]::StartNew()
  $process = Start-Process -FilePath $exe -PassThru
  try {
    while (-not $process.HasExited -and $process.MainWindowHandle -eq 0 -and $timer.ElapsedMilliseconds -lt 10000) {
      Start-Sleep -Milliseconds 50
      $process.Refresh()
    }
    if ($process.HasExited) { throw "$arch executable exited during startup with code $($process.ExitCode)" }
    if ($process.MainWindowHandle -eq 0) { throw "$arch executable did not create its main window within 10 seconds" }
    $reportLines.Add("- $arch`: main window created in $($timer.ElapsedMilliseconds) ms")
  }
  finally {
    if (-not $process.HasExited) { Stop-Process -Id $process.Id -Force }
    $process.Dispose()
  }
}

$reportPath = Join-Path $release "mknkCompress-$Version-Native-MSVC-BUILD-REPORT.txt"
$reportLines | Set-Content $reportPath -Encoding utf8

foreach ($arch in @('x64','x86','ARM64')) {
  Copy-Item $reportPath (Join-Path $root "dist/$arch/WINDOWS-MSVC-BUILD-REPORT.txt") -Force
  Copy-Item (Join-Path $root 'DISTRIBUTION-README.txt') (Join-Path $root "dist/$arch/") -Force
  $zip = Join-Path $release "mknkCompress-$Version-Native-MSVC-Windows-$arch.zip"
  if (Test-Path $zip) { Remove-Item $zip -Force }
  Compress-Archive -Path (Join-Path $root "dist/$arch/*") -DestinationPath $zip -CompressionLevel Optimal
}

$allZip = Join-Path $release "mknkCompress-$Version-Native-MSVC-Windows-All-Architectures.zip"
Compress-Archive -Path (Join-Path $root 'dist/*') -DestinationPath $allZip -CompressionLevel Optimal

$source = Join-Path $release "mknkCompress-$Version-native-source.zip"
if (Test-Path $source) { Remove-Item $source -Force }
$items = Get-ChildItem $root -Force | Where-Object { $_.Name -notin @('.git','build','dist','release') }
Compress-Archive -Path $items.FullName -DestinationPath $source -CompressionLevel Optimal

$checksumPath = Join-Path $release "mknkCompress-$Version-Native-MSVC-SHA256SUMS.txt"
Get-ChildItem $release -File | Where-Object { $_.FullName -ne $checksumPath } | Sort-Object Name | ForEach-Object {
  $hash = (Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
  "$hash  $($_.Name)"
} | Set-Content $checksumPath -Encoding ascii
Write-Host "Release complete: $release" -ForegroundColor Green
