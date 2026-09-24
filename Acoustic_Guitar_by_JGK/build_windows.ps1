$ErrorActionPreference = 'Stop'

Write-Host "=== Acoustic Guitar by JGK - Windows VST3 Builder ===" -ForegroundColor Cyan

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $root

function Need($cmd, $friendly) {
    if (-not (Get-Command $cmd -ErrorAction SilentlyContinue)) {
        throw "$friendly is required but was not found in PATH."
    }
}

Need cmake "CMake"
Need git "Git"

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vswhere) {
    $vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $vs) { throw "Visual Studio C++ build tools were not found. Install 'Desktop development with C++'." }
}

Write-Host "Configuring..." -ForegroundColor Yellow
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed." }

Write-Host "Building Release VST3..." -ForegroundColor Yellow
cmake --build build --config Release --target AcousticGuitarByJGK_VST3
if ($LASTEXITCODE -ne 0) { throw "Build failed." }

$vst = Join-Path $root "build\AcousticGuitarByJGK_artefacts\Release\VST3\Acoustic Guitar by JGK.vst3"
if (-not (Test-Path $vst)) {
    $vst = Get-ChildItem -Path "$root\build" -Recurse -Filter "Acoustic Guitar by JGK.vst3" -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty FullName
}

if ($vst) {
    Write-Host "" 
    Write-Host "DONE" -ForegroundColor Green
    Write-Host "VST3: $vst" -ForegroundColor Green
    Write-Host "Copy that .vst3 folder to C:\Program Files\Common Files\VST3\ then rescan plugins in FL Studio." -ForegroundColor White
} else {
    Write-Warning "Build completed but the VST3 path was not found automatically. Check the build artefacts folder."
}
