param(
  [string]$BuildDir = "",
  [ValidateSet("Release", "RelWithDebInfo", "Debug")]
  [string]$BuildType = "Release",
  [string]$Generator = "",
  [string[]]$ExtraCMakeArgs = @()
)

$ErrorActionPreference = "Stop"

$root = $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($BuildDir)) {
  $BuildDir = Join-Path $root "build"
}

$configureArgs = @("-S", $root, "-B", $BuildDir, "-DCMAKE_BUILD_TYPE=$BuildType")
if (-not [string]::IsNullOrWhiteSpace($Generator)) {
  $configureArgs = @("-G", $Generator) + $configureArgs
}
if ($ExtraCMakeArgs.Count -gt 0) {
  $configureArgs += $ExtraCMakeArgs
}

& cmake @configureArgs
if ($LASTEXITCODE -ne 0) {
  throw "cmake configure failed"
}

& cmake --build $BuildDir --config $BuildType --parallel
if ($LASTEXITCODE -ne 0) {
  throw "build failed"
}

& ctest --test-dir $BuildDir -C $BuildType --output-on-failure
if ($LASTEXITCODE -ne 0) {
  throw "test failed"
}
