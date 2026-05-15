param(
  [string]$ToolchainBase = "",
  [string]$WinlibsTag = "16.0.1-snapshot20260222posix-13.0.0-ucrt-r1",
  [string]$WinlibsZipName = "winlibs-x86_64-posix-seh-gcc-16.0.1-snapshot20260222-mingw-w64ucrt-13.0.0-r1.zip",
  [string]$Msys2GmpPkg = "mingw-w64-x86_64-gmp-6.3.0-2-any.pkg.tar.zst"
)

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

if ([string]::IsNullOrWhiteSpace($ToolchainBase)) {
  if (-not [string]::IsNullOrWhiteSpace($env:MINGW64_TOOLCHAIN_BASE)) {
    $ToolchainBase = $env:MINGW64_TOOLCHAIN_BASE
  } else {
    $ToolchainBase = Join-Path $PSScriptRoot "toolchains\winlibs-x64"
  }
}

New-Item -ItemType Directory -Force -Path $ToolchainBase | Out-Null

$zipPath = Join-Path $ToolchainBase "winlibs-x64.zip"
$winlibsUrl = "https://github.com/brechtsanders/winlibs_mingw/releases/download/$WinlibsTag/$WinlibsZipName"

if (!(Test-Path (Join-Path $ToolchainBase "mingw64\bin\gcc.exe"))) {
  Write-Output "downloading winlibs x64..."
  Invoke-WebRequest -Uri $winlibsUrl -OutFile $zipPath
  Expand-Archive -Path $zipPath -DestinationPath $ToolchainBase -Force
}

$gmpPkgPath = Join-Path $ToolchainBase $Msys2GmpPkg
$gmpUrl = "https://mirror.msys2.org/mingw/mingw64/$Msys2GmpPkg"
if (!(Test-Path (Join-Path $ToolchainBase "mingw64\include\gmp.h")) -or
    !(Test-Path (Join-Path $ToolchainBase "mingw64\lib\libgmp.a"))) {
  Write-Output "downloading gmp dev package..."
  Invoke-WebRequest -Uri $gmpUrl -OutFile $gmpPkgPath
  tar -xf $gmpPkgPath -C $ToolchainBase
}

$gcc = Join-Path $ToolchainBase "mingw64\bin\gcc.exe"
$gxx = Join-Path $ToolchainBase "mingw64\bin\g++.exe"
$gmpHeader = Join-Path $ToolchainBase "mingw64\include\gmp.h"
$gmpLib = Join-Path $ToolchainBase "mingw64\lib\libgmp.a"
if (!(Test-Path $gcc) -or !(Test-Path $gxx)) {
  throw "x64 compiler missing under $ToolchainBase\\mingw64\\bin"
}
if (!(Test-Path $gmpHeader) -or !(Test-Path $gmpLib)) {
  throw "gmp development files missing under $ToolchainBase\\mingw64"
}

$machine = (& $gcc -dumpmachine).Trim()
Write-Output "toolchain ready: $gcc"
Write-Output "target triple: $machine"
Write-Output "gmp header: $gmpHeader"
Write-Output "gmp lib: $gmpLib"
