<#
.SYNOPSIS
    Build the headless UI simulator with MSVC, run its script and convert
    every frame to PNG (Windows counterpart of run-ui-simulator.sh).

.DESCRIPTION
    Needs CMake (3.20 or newer, e.g. the system install), Visual Studio
    2022 with C++ tools, Python 3 and the LVGL submodule
    (git submodule update --init).

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File tools\run-ui-simulator.ps1
#>
param(
    [string]$OutDir = '',
    [string]$CMake = 'cmake'
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$build = Join-Path $repo 'build\ui-sim-msvc'
if (-not $OutDir) { $OutDir = Join-Path $repo 'build\ui-sim' }

& $CMake -S (Join-Path $repo 'firmware\simulator') -B $build -G 'Visual Studio 17 2022' -A x64 | Out-Null
if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed' }
& $CMake --build $build --config Release | Out-Null
if ($LASTEXITCODE -ne 0) { throw 'build failed (run the build command above by hand to see the errors)' }

New-Item -ItemType Directory -Force $OutDir | Out-Null
Get-ChildItem $OutDir -Include *.bin, *.png -File -Recurse | Remove-Item -Force
& (Join-Path $build 'Release\ui_simulator.exe') $OutDir
if ($LASTEXITCODE -ne 0) { throw "simulator script failed (exit $LASTEXITCODE)" }

foreach ($dump in Get-ChildItem $OutDir -Filter *.bin) {
    $png = [IO.Path]::ChangeExtension($dump.FullName, '.png')
    python (Join-Path $repo 'tools\fb2png.py') $dump.FullName $png | Out-Null
    Remove-Item $dump.FullName
}
Write-Output "Screenshots in $OutDir"
