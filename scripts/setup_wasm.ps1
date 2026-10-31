# Automatic setup of dependencies for WASM build
# Usage: .\scripts\setup_wasm.ps1
# PowerShell version for Windows

param(
    [string]$EmsdkDir = "$env:USERPROFILE\emsdk"
)

Write-Host "=== BendCore WASM Setup (PowerShell) ===" -ForegroundColor Cyan
Write-Host "System: Windows"
Write-Host ""

# Step 1: Check prerequisites
Write-Host "[1/4] Checking prerequisites..." -ForegroundColor Yellow
$missingTools = @()

foreach ($tool in @("cmake", "ninja", "python", "git", "node", "npm")) {
    try {
        $version = switch ($tool) {
            "cmake" { cmake --version 2>&1 | Select-Object -First 1 }
            "ninja" { ninja --version 2>&1 }
            "python" { python --version 2>&1 }
            "git" { git --version 2>&1 }
            "node" { node --version 2>&1 }
            "npm" { npm --version 2>&1 }
        }
        Write-Host "  ✓ $tool`: $version" -ForegroundColor Green
    }
    catch {
        Write-Host "  ✗ $tool`: NOT FOUND" -ForegroundColor Red
        $missingTools += $tool
    }
}

if ($missingTools.Count -gt 0) {
    Write-Host ""
    Write-Host "Missing tools: $($missingTools -join ', ')" -ForegroundColor Red
    Write-Host "Please install them first:" -ForegroundColor Yellow
    Write-Host "  cmake: https://cmake.org/download/" -ForegroundColor Yellow
    Write-Host "  ninja: https://github.com/ninja-build/ninja/releases" -ForegroundColor Yellow
    Write-Host "  python: https://www.python.org/downloads/" -ForegroundColor Yellow
    Write-Host "  git: https://git-scm.com/download/win" -ForegroundColor Yellow
    Write-Host "  node: https://nodejs.org/" -ForegroundColor Yellow
    exit 1
}

# Step 2: Check Node.js
Write-Host ""
Write-Host "[2/4] Checking Node.js..." -ForegroundColor Yellow
try {
    $nodeVer = node --version 2>&1
    Write-Host "  Node.js $nodeVer found ✓" -ForegroundColor Green
}
catch {
    Write-Host "  ERROR: Node.js not found" -ForegroundColor Red
    exit 1
}

# Step 3: Install Emscripten
Write-Host ""
Write-Host "[3/4] Setting up Emscripten..." -ForegroundColor Yellow

if (Test-Path $EmsdkDir) {
    Write-Host "  emsdk already exists at $EmsdkDir" -ForegroundColor Green
}
else {
    Write-Host "  Cloning emsdk from GitHub..." -ForegroundColor Yellow
    Write-Host "  URL: https://github.com/emscripten-core/emsdk.git" -ForegroundColor Gray

    try {
        $ErrorActionPreference = 'Stop'
        git clone https://github.com/emscripten-core/emsdk.git $EmsdkDir 2>&1 | Out-Null
        Write-Host "  ✓ emsdk cloned" -ForegroundColor Green
    }
    catch {
        Write-Host "  ERROR: Failed to clone emsdk" -ForegroundColor Red
        Write-Host "  Make sure you have internet access" -ForegroundColor Yellow
        exit 1
    }
}

Write-Host "  Installing latest emscripten..." -ForegroundColor Yellow
Push-Location $EmsdkDir
try {
    & ".\emsdk.bat" install latest 2>&1 | Out-Null
    & ".\emsdk.bat" activate latest 2>&1 | Out-Null
    Write-Host "  ✓ Emscripten installed" -ForegroundColor Green
}
catch {
    Write-Host "  ERROR: Failed to install emscripten" -ForegroundColor Red
    Pop-Location
    exit 1
}
finally {
    Pop-Location
}

# Step 4: Add to environment
Write-Host ""
Write-Host "[4/4] Setting up environment..." -ForegroundColor Yellow

$emsdkEnv = "$EmsdkDir\emsdk_env.bat"
if (Test-Path $emsdkEnv) {
    Write-Host "  ✓ emsdk_env.bat found" -ForegroundColor Green
}
else {
    Write-Host "  WARNING: emsdk_env.bat not found" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "=== Verification ===" -ForegroundColor Cyan
Write-Host "Node.js: $(node --version)" -ForegroundColor Green
Write-Host "npm: $(npm --version)" -ForegroundColor Green
Write-Host "cmake: $(cmake --version 2>&1 | Select-Object -First 1)" -ForegroundColor Green
Write-Host "ninja: $(ninja --version)" -ForegroundColor Green

# Check emcc
$emsdkBat = "$EmsdkDir\emsdk_env.bat"
if (Test-Path $emsdkBat) {
    Write-Host "emsdk_env.bat: ✓ found" -ForegroundColor Green
}
else {
    Write-Host "emsdk_env.bat: ✗ NOT found" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "✓ Setup complete!" -ForegroundColor Green
Write-Host ""
Write-Host "Next steps:" -ForegroundColor Cyan
Write-Host "  1. In Command Prompt or PowerShell, run emsdk activation:" -ForegroundColor White
Write-Host "     $EmsdkDir\emsdk_env.bat" -ForegroundColor Yellow
Write-Host "  2. Then build WASM:" -ForegroundColor White
Write-Host "     .\scripts\build_wasm.ps1" -ForegroundColor Yellow
Write-Host "  3. Or: cmake --preset wasm && cmake --build --preset wasm" -ForegroundColor Yellow
Write-Host ""
Write-Host "EMSDK location: $EmsdkDir" -ForegroundColor Cyan
