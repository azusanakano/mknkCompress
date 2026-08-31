[CmdletBinding()]
param(
  [ValidateSet('x64','x86','ARM64','All')]
  [string]$Architecture = 'All',
  [ValidateSet('Release','Debug')]
  [string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path $vswhere)) {
  throw 'Visual Studio 2022 Build Tools が見つかりません。C++によるデスクトップ開発を追加してください。'
}
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw 'MSVC C++ ビルドツールが見つかりません。' }
$cmake = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
if (-not (Test-Path $cmake)) { $cmake = (Get-Command cmake.exe -ErrorAction Stop).Source }

$targets = if ($Architecture -eq 'All') { @('x64','x86','ARM64') } else { @($Architecture) }
foreach ($target in $targets) {
  $generatorArch = if ($target -eq 'x86') { 'Win32' } else { $target }
  $build = Join-Path $root "build/$target"
  & $cmake -S $root -B $build -A $generatorArch
  if ($LASTEXITCODE) { throw "CMake configure failed: $target" }
  & $cmake --build $build --config $Configuration --parallel
  if ($LASTEXITCODE) { throw "CMake build failed: $target" }
  $dist = Join-Path $root "dist/$target"
  New-Item -ItemType Directory -Force -Path $dist | Out-Null
  Copy-Item (Join-Path $build "$Configuration/mknkYahooCompress.exe") $dist -Force
  Copy-Item (Join-Path $root 'README.md') $dist -Force
  Copy-Item (Join-Path $root 'LICENSE') $dist -Force
}

Write-Host "Build complete: $root\dist" -ForegroundColor Green
