[CmdletBinding()]
param([string]$Version = '3.0.0')
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
& (Join-Path $root 'build-windows.ps1') -Architecture All -Configuration Release
$release = Join-Path $root 'release'
New-Item -ItemType Directory -Force -Path $release | Out-Null
foreach ($arch in @('x64','x86','ARM64')) {
  $zip = Join-Path $release "mknkYahooCompress-$Version-Native-Windows-$arch.zip"
  if (Test-Path $zip) { Remove-Item $zip -Force }
  Compress-Archive -Path (Join-Path $root "dist/$arch/*") -DestinationPath $zip -CompressionLevel Optimal
}
$source = Join-Path $release "mknkYahooCompress-$Version-native-source.zip"
if (Test-Path $source) { Remove-Item $source -Force }
$items = Get-ChildItem $root -Force | Where-Object { $_.Name -notin @('build','dist','release') }
Compress-Archive -Path $items.FullName -DestinationPath $source -CompressionLevel Optimal
Get-ChildItem $release -File | ForEach-Object {
  $hash = (Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
  "$hash  $($_.Name)"
} | Set-Content (Join-Path $release "mknkYahooCompress-$Version-Native-SHA256SUMS.txt") -Encoding ascii
Write-Host "Release complete: $release" -ForegroundColor Green
