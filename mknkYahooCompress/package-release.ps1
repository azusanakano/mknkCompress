[CmdletBinding()]
param([string]$Version = '3.0.4')
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
& (Join-Path $root 'build-windows.ps1') -Architecture All -Configuration Release
$release = Join-Path $root 'release'
$stage = Join-Path $release 'stage'
New-Item -ItemType Directory -Force -Path $release | Out-Null
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Force -Path $stage | Out-Null
$documents = @('DISTRIBUTION-README.txt','LICENSE','BUILD-REPORT.txt')
foreach ($arch in @('x64','x86','ARM64')) {
  $zip = Join-Path $release "mknkYahooCompress-$Version-Native-Windows-$arch.zip"
  $archStage = Join-Path $stage $arch
  New-Item -ItemType Directory -Force -Path $archStage | Out-Null
  Copy-Item (Join-Path $root "dist/$arch/mknkYahooCompress.exe") $archStage -Force
  foreach ($document in $documents) { Copy-Item (Join-Path $root $document) $archStage -Force }
  if (Test-Path $zip) { Remove-Item $zip -Force }
  Compress-Archive -Path (Join-Path $archStage '*') -DestinationPath $zip -CompressionLevel Optimal
}
$allZip = Join-Path $release "mknkYahooCompress-$Version-Native-Windows-All-Architectures.zip"
$allStage = Join-Path $stage 'all'
foreach ($arch in @('x64','x86','ARM64')) {
  $archStage = Join-Path $allStage $arch
  New-Item -ItemType Directory -Force -Path $archStage | Out-Null
  Copy-Item (Join-Path $root "dist/$arch/mknkYahooCompress.exe") $archStage -Force
}
$docsStage = Join-Path $allStage 'docs'
New-Item -ItemType Directory -Force -Path $docsStage | Out-Null
Copy-Item (Join-Path $root 'README.md') $docsStage -Force
foreach ($document in $documents) { Copy-Item (Join-Path $root $document) $docsStage -Force }
if (Test-Path $allZip) { Remove-Item $allZip -Force }
Compress-Archive -Path (Join-Path $allStage '*') -DestinationPath $allZip -CompressionLevel Optimal
$source = Join-Path $release "mknkYahooCompress-$Version-native-source.zip"
if (Test-Path $source) { Remove-Item $source -Force }
$items = Get-ChildItem $root -Force | Where-Object { $_.Name -notin @('.git','build','dist','release','releases','github-ready') }
Compress-Archive -Path $items.FullName -DestinationPath $source -CompressionLevel Optimal
$checksums = Join-Path $release "mknkYahooCompress-$Version-Native-SHA256SUMS.txt"
if (Test-Path $checksums) { Remove-Item $checksums -Force }
Get-ChildItem $release -File -Filter '*.zip' | Sort-Object Name | ForEach-Object {
  $hash = (Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
  "$hash  $($_.Name)"
} | Set-Content $checksums -Encoding ascii
Remove-Item $stage -Recurse -Force
Write-Host "Release complete: $release" -ForegroundColor Green
