param(
  [ValidateSet("quick", "target")]
  [string]$Profile = "quick",
  [int]$Trials = 1,
  [ValidateSet(0,1)]
  [int]$RobustMode = 0,
  [int]$BucketThreads = 1,
  [int]$MaxPoints = 0,
  [ValidateSet("Release", "RelWithDebInfo", "Debug")]
  [string]$BuildType = "Release",
  [switch]$ParallelPoints = $false,
  [switch]$UseX64Toolchain = $false,
  [string]$ToolchainRoot = "",
  [string]$BuildDir = "",
  [string]$OutputDir = "",
  [string]$Generator = "",
  [string]$RelicMainDir = "",
  [string]$RelicTargetDir = "",
  [string]$RelicStaticLib = "",
  [string]$GmpIncludeDir = "",
  [string]$GmpLibrary = ""
)

$ErrorActionPreference = "Stop"

$root = $PSScriptRoot
$cc = ""
$cxx = ""
if ($UseX64Toolchain) {
  if ([string]::IsNullOrWhiteSpace($ToolchainRoot)) {
    $ToolchainRoot = $env:MINGW64_ROOT
  }
  if ([string]::IsNullOrWhiteSpace($ToolchainRoot)) {
    throw "UseX64Toolchain is set, but ToolchainRoot is empty. Pass -ToolchainRoot or set MINGW64_ROOT."
  }
  $binDir = Join-Path $ToolchainRoot "bin"
  $cc = Join-Path $binDir "gcc.exe"
  $cxx = Join-Path $binDir "g++.exe"
  if (!(Test-Path $cc) -or !(Test-Path $cxx)) {
    throw "x64 toolchain not found: $binDir"
  }
  $env:Path = "$binDir;$env:Path"
  $mach = (& $cc -dumpmachine).Trim()
  if ($mach -notmatch "x86_64") {
    throw "selected compiler is not x64: $mach"
  }
}

if ([string]::IsNullOrWhiteSpace($BuildDir)) {
  if ($UseX64Toolchain) {
    $BuildDir = Join-Path $root "build_x64"
  } else {
    $BuildDir = Join-Path $root "build"
  }
}
if ([string]::IsNullOrWhiteSpace($OutputDir)) {
  $OutputDir = Join-Path $root "experiment_results"
}

$outDir = $OutputDir
$outCsv = Join-Path $outDir "upsi_4_3_2_summary.csv"

New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

$configureArgs = @("-S", $root, "-B", $BuildDir, "-DCMAKE_BUILD_TYPE=$BuildType")
if (-not [string]::IsNullOrWhiteSpace($Generator)) {
  $configureArgs = @("-G", $Generator) + $configureArgs
}

if ($UseX64Toolchain) {
  $configureArgs += @("-DCMAKE_C_COMPILER=$cc", "-DCMAKE_CXX_COMPILER=$cxx")
}
if (-not [string]::IsNullOrWhiteSpace($RelicMainDir)) {
  $configureArgs += "-DUPSI_RELIC_MAIN_DIR=$RelicMainDir"
}
if (-not [string]::IsNullOrWhiteSpace($RelicTargetDir)) {
  $configureArgs += "-DUPSI_RELIC_TARGET_DIR=$RelicTargetDir"
}
if (-not [string]::IsNullOrWhiteSpace($RelicStaticLib)) {
  $configureArgs += "-DUPSI_RELIC_STATIC_LIB=$RelicStaticLib"
}
if (-not [string]::IsNullOrWhiteSpace($GmpIncludeDir)) {
  $configureArgs += "-DUPSI_GMP_INCLUDE_DIR=$GmpIncludeDir"
}
if (-not [string]::IsNullOrWhiteSpace($GmpLibrary)) {
  $configureArgs += "-DUPSI_GMP_LIBRARY=$GmpLibrary"
}

& cmake @configureArgs
if ($LASTEXITCODE -ne 0) {
  throw "cmake configure failed"
}

& cmake --build $BuildDir --config $BuildType --target upsi_exp_4_3_2_bench --parallel
if ($LASTEXITCODE -ne 0) {
  throw "build benchmark failed"
}

$benchCandidates = @(
  (Join-Path $BuildDir "upsi_exp_4_3_2_bench"),
  (Join-Path (Join-Path $BuildDir $BuildType) "upsi_exp_4_3_2_bench")
)
$benchExe = ""
foreach ($candidate in $benchCandidates) {
  if (Test-Path $candidate) {
    $benchExe = $candidate
    break
  }
  if (Test-Path "$candidate.exe") {
    $benchExe = "$candidate.exe"
    break
  }
}
if ([string]::IsNullOrWhiteSpace($benchExe)) {
  throw "benchmark executable not found under $BuildDir"
}

if (-not $ParallelPoints) {
  if ($RobustMode -ne 0) {
    & $benchExe $outCsv $Profile $Trials 1 $MaxPoints 0 0 $BucketThreads
  } else {
    & $benchExe $outCsv $Profile $Trials 0 $MaxPoints 0 0 $BucketThreads
  }
  if ($LASTEXITCODE -ne 0) {
    throw "benchmark run failed"
  }
} else {
  $allPoints = @()
  if ($Profile -eq "quick") {
    $allPoints = @(128, 256, 512, 1024)
  } else {
    $allPoints = @(16384, 65536, 262144)
  }

  if ($MaxPoints -gt 0) {
    $take = [Math]::Min($MaxPoints, $allPoints.Count)
    if ($take -le 0) {
      throw "invalid MaxPoints"
    }
    $allPoints = $allPoints[0..($take - 1)]
  }

  $robustArg = if ($RobustMode -ne 0) { "1" } else { "0" }
  $jobs = @()
  foreach ($n in $allPoints) {
    $pointCsv = Join-Path $outDir ("upsi_4_3_2_n{0}.csv" -f $n)
    $args = @($pointCsv, $Profile, $Trials, $robustArg, 1, $n, 0, $BucketThreads)
    $p = Start-Process -FilePath $benchExe -ArgumentList $args -PassThru
    $jobs += [PSCustomObject]@{ N = $n; Csv = $pointCsv; Proc = $p }
  }

  foreach ($j in $jobs) {
    $j.Proc.WaitForExit()
    if ($j.Proc.ExitCode -ne 0) {
      throw ("benchmark run failed for n={0} (exit={1})" -f $j.N, $j.Proc.ExitCode)
    }
  }

  $merged = @()
  $headerWritten = $false
  foreach ($j in ($jobs | Sort-Object N)) {
    if (-not (Test-Path $j.Csv)) {
      throw ("missing result file for n={0}: {1}" -f $j.N, $j.Csv)
    }
    $lines = Get-Content $j.Csv
    if ($lines.Count -eq 0) {
      throw ("empty result file for n={0}: {1}" -f $j.N, $j.Csv)
    }
    if (-not $headerWritten) {
      $merged += $lines[0]
      $headerWritten = $true
    }
    if ($lines.Count -gt 1) {
      $merged += $lines[1..($lines.Count - 1)]
    }
  }
  Set-Content -Path $outCsv -Value $merged -Encoding UTF8
}

Write-Output "benchmark result: $outCsv"
