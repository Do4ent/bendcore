# Build WebAssembly (Emscripten) version
# Usage: .\scripts\build_wasm.ps1
# PowerShell version for Windows

param(
    [string]$EmsdkDir = "$env:USERPROFILE\emsdk",
    [string]$EigenDir = "C:\Program Files (x86)\Eigen"
)

$ErrorActionPreference = 'Stop'

Write-Host "=== BendCore WASM Build (PowerShell) ===" -ForegroundColor Cyan

$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$BuildDir = Join-Path $Root "build-wasm"
$LogFile = Join-Path $Root "build-wasm.log"

# Check EMSDK
if (-not (Test-Path $EmsdkDir)) {
    Write-Host "ERROR: EMSDK not found at $EmsdkDir" -ForegroundColor Red
    Write-Host ""
    Write-Host "Setup instructions:" -ForegroundColor Yellow
    Write-Host "  1. .\scripts\setup_wasm.ps1" -ForegroundColor Yellow
    Write-Host "  2. Run emsdk activation:" -ForegroundColor Yellow
    Write-Host "     $EmsdkDir\emsdk_env.bat" -ForegroundColor Yellow
    Write-Host "  3. Then retry: .\scripts\build_wasm.ps1" -ForegroundColor Yellow
    exit 1
}

# Activate EMSDK
Write-Host "Activating EMSDK..." -ForegroundColor Yellow
$emsdkEnv = Join-Path $EmsdkDir "emsdk_env.bat"
if (Test-Path $emsdkEnv) {
    & cmd /c "$emsdkEnv" 2>&1 | Out-Null
    Write-Host "  ✓ EMSDK activated" -ForegroundColor Green
}
else {
    Write-Host "  WARNING: emsdk_env.bat not found" -ForegroundColor Yellow
}

# Check tools
Write-Host ""
Write-Host "=== Checking Tools ===" -ForegroundColor Cyan
$tools = @("emcc", "cmake", "ninja", "node")
foreach ($tool in $tools) {
    try {
        $version = & $tool --version 2>&1 | Select-Object -First 1
        Write-Host "  ✓ $tool`: $version" -ForegroundColor Green
    }
    catch {
        Write-Host "  ERROR: $tool not found" -ForegroundColor Red
        exit 1
    }
}

# Check Eigen
Write-Host ""
Write-Host "=== Checking Eigen ===" -ForegroundColor Cyan
if (Test-Path "C:\Program Files\Eigen") {
    $EigenDir = "C:\Program Files\Eigen"
}
elseif (Test-Path "C:\Program Files (x86)\Eigen") {
    $EigenDir = "C:\Program Files (x86)\Eigen"
}

if (Test-Path $EigenDir) {
    Write-Host "  ✓ Eigen found at $EigenDir" -ForegroundColor Green
}
else {
    Write-Host "  WARNING: Eigen not found" -ForegroundColor Yellow
    Write-Host "  Install from: https://eigen.tuxfamily.org/" -ForegroundColor Yellow
}

# Build
Write-Host ""
Write-Host "=== Building WASM ===" -ForegroundColor Cyan

Push-Location $Root

try {
    Write-Host "Configuring CMake..." -ForegroundColor Yellow
    if (Test-Path $BuildDir) {
        Remove-Item $BuildDir -Recurse -Force
    }

    $cmakeArgs = @(
        "-B", $BuildDir,
        "-G", "Ninja",
        "-DCMAKE_BUILD_TYPE=Release",
        "-DCMAKE_TOOLCHAIN_FILE=$EmsdkDir\upstream\emscripten\cmake\Modules\Platform\Emscripten.cmake"
    )

    if (Test-Path $EigenDir) {
        $cmakeArgs += "-DBENDCORE_EIGEN_DIR=$EigenDir"
    }

    & cmake @cmakeArgs 2>&1 | Tee-Object -FilePath $LogFile
    Write-Host "  ✓ CMake configured" -ForegroundColor Green

    Write-Host ""
    Write-Host "Building..." -ForegroundColor Yellow
    & cmake --build $BuildDir --preset wasm 2>&1 | Tee-Object -FilePath $LogFile -Append
    Write-Host "  ✓ Build complete" -ForegroundColor Green

    # Verify output
    Write-Host ""
    Write-Host "=== Verifying Output ===" -ForegroundColor Cyan
    $outDir = Join-Path $BuildDir "web"

    foreach ($file in @("bendcore.js", "bendcore.wasm", "bendcore.mjs", "index.html")) {
        $path = Join-Path $outDir $file
        if (Test-Path $path) {
            $size = (Get-Item $path).Length / 1MB
            Write-Host "  ✓ $file ($([math]::Round($size, 2)) MB)" -ForegroundColor Green
        }
        else {
            Write-Host "  ✗ $file MISSING" -ForegroundColor Red
        }
    }

    Write-Host ""
    Write-Host "✓ WASM Build Successful!" -ForegroundColor Green
    Write-Host "Output: $outDir" -ForegroundColor Cyan
}
catch {
    Write-Host ""
    Write-Host "ERROR: Build failed" -ForegroundColor Red
    Write-Host $_.Exception.Message -ForegroundColor Red
    Write-Host ""
    Write-Host "Check log: $LogFile" -ForegroundColor Yellow
    exit 1
}
finally {
    Pop-Location
}
