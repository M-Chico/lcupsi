param(
  [ValidateSet("X86", "X64")]
  [string]$Arch = "X64",
  [int]$Cores = 1,
  [ValidateSet("AUTO", "NONE", "OPENMP", "PTHREAD")]
  [string]$MultiMode = "AUTO",
  [string]$ToolchainRoot = "",
  [string]$RelicRoot = "",
  [string]$TargetDir = "",
  [switch]$CleanConfigure = $true
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($ToolchainRoot)) {
  $ToolchainRoot = $env:MINGW64_ROOT
}
if ([string]::IsNullOrWhiteSpace($ToolchainRoot)) {
  throw "ToolchainRoot is empty. Pass -ToolchainRoot or set MINGW64_ROOT."
}

if ([string]::IsNullOrWhiteSpace($RelicRoot)) {
  throw "RelicRoot is empty. Pass -RelicRoot with the path to relic-main."
}
if ([string]::IsNullOrWhiteSpace($TargetDir)) {
  $TargetDir = Join-Path $RelicRoot "demo\psi-client-server\target"
}

$binDir = Join-Path $ToolchainRoot "bin"
$cc = Join-Path $binDir "gcc.exe"
$cxx = Join-Path $binDir "g++.exe"
if (!(Test-Path $cc) -or !(Test-Path $cxx)) {
  throw "x64 compiler not found under $binDir"
}

$env:Path = "$binDir;$env:Path"
$dumpMachine = (& $cc -dumpmachine).Trim()
if ($Arch -eq "X64" -and $dumpMachine -notmatch "x86_64") {
  throw "selected compiler is not x64: $dumpMachine"
}

$wsize = "32"
if ($Arch -eq "X64") {
  $wsize = "64"
}
$multi = ""
if ($MultiMode -eq "AUTO") {
  if ($Cores -gt 1) {
    $multi = "OPENMP"
  }
} elseif ($MultiMode -eq "OPENMP") {
  $multi = "OPENMP"
} elseif ($MultiMode -eq "PTHREAD") {
  $multi = "PTHREAD"
} else {
  $multi = ""
}

New-Item -ItemType Directory -Force -Path $TargetDir | Out-Null
Push-Location $TargetDir

if ($CleanConfigure) {
  if (Test-Path (Join-Path $TargetDir "CMakeCache.txt")) {
    Remove-Item -Force (Join-Path $TargetDir "CMakeCache.txt")
  }
  if (Test-Path (Join-Path $TargetDir "CMakeFiles")) {
    Remove-Item -Recurse -Force (Join-Path $TargetDir "CMakeFiles")
  }
}

& cmake -G "MinGW Makefiles" `
  -DCMAKE_BUILD_TYPE=Release `
  "-DCMAKE_C_COMPILER=$cc" `
  "-DCMAKE_CXX_COMPILER=$cxx" `
  "-DARCH=$Arch" `
  "-DWSIZE=$wsize" `
  -DARITH=gmp `
  "-DGMP_ROOT=$ToolchainRoot" `
  "-DGMP_INCLUDE_DIR=$ToolchainRoot/include" `
  "-DGMP_LIBRARIES=$ToolchainRoot/lib/libgmp.a" `
  -DFP_PRIME=381 `
  "-DWITH=BN;DV;FP;FPX;EP;EPX;PP;PC;CP;MD" `
  -DSHLIB=off `
  -DSTLIB=on `
  -DSTBIN=on `
  -DCHECK=off `
  -DVERBS=off `
  -DDOCUM=off `
  -DQUIET=on `
  -DTESTS=0 `
  -DBENCH=0 `
  "-DCORES=$Cores" `
  "-DMULTI=$multi" `
  "-DCFLAGS=-O3 -funroll-loops -fomit-frame-pointer -finline-small-functions" `
  $RelicRoot
if ($LASTEXITCODE -ne 0) {
  throw "relic cmake configure failed"
}

& cmake --build . --target relic_s --parallel
if ($LASTEXITCODE -ne 0) {
  throw "relic build failed"
}

Pop-Location

Write-Output "relic rebuilt for UPSI: $TargetDir"
