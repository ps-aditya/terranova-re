# Build the C tool on Windows with MinGW-w64.
#   .\scripts\build.ps1
#   .\scripts\build.ps1 -Toolchain D:\mingw64\bin
param(
    [string]$Toolchain = $env:MINGW_BIN
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

if ($Toolchain -and (Test-Path (Join-Path $Toolchain "gcc.exe"))) {
    $env:PATH = "$Toolchain;$env:PATH"
}
if (-not (Get-Command gcc -ErrorAction SilentlyContinue)) {
    throw "gcc not found. Install MinGW-w64 or pass -Toolchain <dir-with-gcc.exe>."
}

gcc -std=c11 -O2 -Wall -Wextra -Wpedantic -o lgres.exe src/lg_res.c src/lg_bitmap.c src/lg_scene.c src/lgres_cli.c
if ($LASTEXITCODE -ne 0) { throw "build failed" }
Write-Host "built $root\lgres.exe"
