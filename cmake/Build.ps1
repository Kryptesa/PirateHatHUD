param(
  [ValidateSet('Debug', 'Release', 'RelWithDebInfo')]
  [string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$buildRoot = Join-Path $projectRoot 'build/ninja'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) {
  throw 'Visual Studio Installer/vswhere.exe is required.'
}
$vsRoot = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsRoot) {
  throw 'Install Visual Studio Build Tools with Desktop development with C++.'
}
$cmakeRoot = Join-Path $vsRoot 'Common7/IDE/CommonExtensions/Microsoft/CMake'
$cmake = Join-Path $cmakeRoot 'CMake/bin/cmake.exe'
$ctest = Join-Path $cmakeRoot 'CMake/bin/ctest.exe'
$cpack = Join-Path $cmakeRoot 'CMake/bin/cpack.exe'
$ninja = Join-Path $cmakeRoot 'Ninja/ninja.exe'
foreach ($tool in @($cmake, $ctest, $cpack, $ninja)) {
  if (-not (Test-Path -LiteralPath $tool)) {
    throw "Missing $tool. Install the Visual Studio C++ CMake tools component."
  }
}

# Import the x64 toolchain for this process only; restore the caller's environment afterwards.
$savedEnvironment = @{}
Get-ChildItem Env: | ForEach-Object { $savedEnvironment[$_.Name] = $_.Value }
try {
  $devCmd = Join-Path $vsRoot 'Common7/Tools/VsDevCmd.bat'
  $toolchainEnvironment = & $env:ComSpec /d /c "call `"$devCmd`" -no_logo -arch=x64 -host_arch=x64 >nul && set"
  if ($LASTEXITCODE -ne 0) {
    throw 'Failed to initialize the MSVC x64 environment.'
  }
  foreach ($line in $toolchainEnvironment) {
    if ($line -match '^([^=]+)=(.*)$') {
      [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], 'Process')
    }
  }

  $configureArguments = @('-S', $projectRoot, '-B', $buildRoot, '-G', 'Ninja Multi-Config',
    "-DCMAKE_MAKE_PROGRAM=$ninja")
  # Reuse downloaded sources, but keep all Ninja-generated files separate from MSBuild.
  foreach ($dependency in @('safetyhook', 'imgui', 'zydis')) {
    $source = Join-Path $projectRoot "build/_deps/$dependency-src"
    $marker = if ($dependency -eq 'imgui') { 'imgui.h' } else { 'CMakeLists.txt' }
    if (Test-Path -LiteralPath (Join-Path $source $marker)) {
      $configureArguments += "-DFETCHCONTENT_SOURCE_DIR_$($dependency.ToUpperInvariant())=$source"
    }
  }
  & $cmake @configureArguments
  if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }
  & $cmake --build $buildRoot --config $Configuration
  if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }
  & $cmake --build $buildRoot --config $Configuration --target architecture-check
  if ($LASTEXITCODE -ne 0) { throw 'Architecture check failed.' }
  & $ctest --test-dir $buildRoot -C $Configuration --output-on-failure
  if ($LASTEXITCODE -ne 0) { throw 'Tests failed.' }
  $packageArguments = @('--config', (Join-Path $buildRoot 'CPackConfig.cmake'),
    '-C', $Configuration)
  if ($Configuration -ne 'Release') {
    $packageArguments += @('-B', (Join-Path $projectRoot "dist/$Configuration"))
  }
  & $cpack @packageArguments
  if ($LASTEXITCODE -ne 0) { throw 'Packaging failed.' }
} finally {
  Get-ChildItem Env: | Where-Object { -not $savedEnvironment.ContainsKey($_.Name) } |
    ForEach-Object { [Environment]::SetEnvironmentVariable($_.Name, $null, 'Process') }
  foreach ($entry in $savedEnvironment.GetEnumerator()) {
    [Environment]::SetEnvironmentVariable($entry.Key, $entry.Value, 'Process')
  }
}
