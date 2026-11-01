# Complete Windows Build Script for BendCore
# Builds frontend, backend, and C++ components on Windows
# Usage: .\scripts\windows-build.ps1 [-BuildType Release|Debug]

param(
    [ValidateSet("Release", "Debug")]
    [string]$BuildType = "Release",
    [switch]$SkipTests,
    [switch]$SkipCpp
)

$ErrorActionPreference = 'Stop'
$WarningPreference = 'Continue'

# Colors
$colors = @{
    Success = 'Green'
    Error = 'Red'
    Warning = 'Yellow'
    Info = 'Cyan'
    Section = 'Magenta'
}

function Write-Section {
    param([string]$Title)
    Write-Host ""
    Write-Host "=== $Title ===" -ForegroundColor $colors.Section
}

function Write-Success {
    param([string]$Message)
    Write-Host "✓ $Message" -ForegroundColor $colors.Success
}

function Write-Error-Custom {
    param([string]$Message)
    Write-Host "✗ $Message" -ForegroundColor $colors.Error
}

function Write-Warning-Custom {
    param([string]$Message)
    Write-Host "⚠ $Message" -ForegroundColor $colors.Warning
}

function Test-CommandExists {
    param([string]$Command)

    try {
        $null = & $Command --version 2>&1
        return $true
    }
    catch {
        return $false
    }
}

# === PREREQUISITES ===
Write-Section "Checking Prerequisites"

$requiredTools = @("cmake", "ninja", "node", "npm", "git", "python")
$missingTools = @()

foreach ($tool in $requiredTools) {
    if (Test-CommandExists $tool) {
        $version = & $tool --version 2>&1 | Select-Object -First 1
        Write-Success "$tool`: $version"
    }
    else {
        Write-Error-Custom "$tool`: NOT FOUND"
        $missingTools += $tool
    }
}

if ($missingTools.Count -gt 0) {
    Write-Host ""
    Write-Error-Custom "Missing tools: $($missingTools -join ', ')"
    Write-Host "Install from:"
    Write-Host "  • CMake: https://cmake.org/download/"
    Write-Host "  • Ninja: https://github.com/ninja-build/ninja/releases"
    Write-Host "  • Python: https://www.python.org/downloads/"
    Write-Host "  • Git: https://git-scm.com/download/win"
    Write-Host "  • Node.js: https://nodejs.org/"
    exit 1
}

# === SETUP ===
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$BuildDir = Join-Path $Root "build-native"
$LogDir = Join-Path $Root "build-logs"

if (-not (Test-Path $LogDir)) {
    New-Item -ItemType Directory -Path $LogDir -Force | Out-Null
}

Write-Section "Build Configuration"
Write-Host "Root: $Root"
Write-Host "Build Type: $BuildType"
Write-Host "Build Dir: $BuildDir"
Write-Host "Log Dir: $LogDir"

# === FRONTEND ===
Write-Section "Frontend Build"

Push-Location $Root

try {
    # Install dependencies
    Write-Host "Installing npm dependencies..." -ForegroundColor $colors.Info
    npm ci --no-audit --no-fund 2>&1 | Tee-Object -FilePath (Join-Path $LogDir "01-npm-install.log")
    if ($LASTEXITCODE -ne 0) { throw "npm ci failed" }
    Write-Success "Dependencies installed"

    # TypeScript check
    Write-Host ""
    Write-Host "Running TypeScript type check..." -ForegroundColor $colors.Info
    npm run typecheck 2>&1 | Tee-Object -FilePath (Join-Path $LogDir "02-typecheck.log")
    if ($LASTEXITCODE -ne 0) { throw "TypeScript check failed" }
    Write-Success "TypeScript check passed"

    # Build frontend
    Write-Host ""
    Write-Host "Building frontend..." -ForegroundColor $colors.Info
    npm run build 2>&1 | Tee-Object -FilePath (Join-Path $LogDir "03-frontend-build.log")
    if ($LASTEXITCODE -ne 0) { throw "Frontend build failed" }
    Write-Success "Frontend built"

    # Tests
    if (-not $SkipTests) {
        Write-Host ""
        Write-Host "Running Jest tests..." -ForegroundColor $colors.Info
        npm test -- --ci --runInBand 2>&1 | Tee-Object -FilePath (Join-Path $LogDir "04-jest-tests.log")
        if ($LASTEXITCODE -ne 0) {
            Write-Warning-Custom "Jest tests failed - continuing"
        }
        else {
            Write-Success "Jest tests passed"
        }
    }
}
catch {
    Write-Error-Custom "Frontend build failed: $_"
    exit 1
}

# === BACKEND ===
Write-Section "Backend Build"

try {
    Push-Location (Join-Path $Root "backend")

    # Install
    Write-Host "Installing backend dependencies..." -ForegroundColor $colors.Info
    npm ci --no-audit --no-fund 2>&1 | Tee-Object -FilePath (Join-Path $LogDir "05-backend-install.log")
    if ($LASTEXITCODE -ne 0) { throw "Backend npm ci failed" }
    Write-Success "Backend dependencies installed"

    # Build
    Write-Host ""
    Write-Host "Building backend..." -ForegroundColor $colors.Info
    npm run build 2>&1 | Tee-Object -FilePath (Join-Path $LogDir "06-backend-build.log")
    if ($LASTEXITCODE -ne 0) { throw "Backend build failed" }
    Write-Success "Backend built"

    # Tests
    if (-not $SkipTests) {
        Write-Host ""
        Write-Host "Running backend tests..." -ForegroundColor $colors.Info
        npm test -- --ci 2>&1 | Tee-Object -FilePath (Join-Path $LogDir "07-backend-tests.log")
        if ($LASTEXITCODE -ne 0) {
            Write-Warning-Custom "Backend tests failed - continuing"
        }
        else {
            Write-Success "Backend tests passed"
        }
    }
}
catch {
    Write-Error-Custom "Backend build failed: $_"
    exit 1
}
finally {
    Pop-Location
}

# === C++ NATIVE ===
if (-not $SkipCpp) {
    Write-Section "C++ Native Build"

    try {
        # Configure CMake
        Write-Host "Configuring CMake..." -ForegroundColor $colors.Info

        if (Test-Path $BuildDir) {
            Remove-Item $BuildDir -Recurse -Force
        }

        $cmakeArgs = @(
            "-B", $BuildDir,
            "-G", "Visual Studio 17 2022",
            "-DCMAKE_BUILD_TYPE=$BuildType",
            "-DCMAKE_CXX_COMPILER=cl.exe",
            "-DCMAKE_C_COMPILER=cl.exe",
            "-DBENDCORE_WITH_OCCT=OFF"
        )

        # Try to find Eigen
        $eigenPaths = @(
            "C:\vcpkg\installed\x64-windows\include",
            "C:\Program Files\Eigen",
            "C:\Program Files (x86)\Eigen"
        )

        foreach ($path in $eigenPaths) {
            if (Test-Path $path) {
                Write-Host "Found Eigen at: $path" -ForegroundColor $colors.Info
                $cmakeArgs += "-DBENDCORE_EIGEN_DIR=$path"
                break
            }
        }

        & cmake @cmakeArgs 2>&1 | Tee-Object -FilePath (Join-Path $LogDir "08-cmake-config.log")
        if ($LASTEXITCODE -ne 0) { throw "CMake configuration failed" }
        Write-Success "CMake configured"

        # Build
        Write-Host ""
        Write-Host "Building C++ components..." -ForegroundColor $colors.Info
        & cmake --build $BuildDir --config $BuildType --parallel 4 2>&1 | Tee-Object -FilePath (Join-Path $LogDir "09-cmake-build.log")
        if ($LASTEXITCODE -ne 0) { throw "C++ build failed" }
        Write-Success "C++ components built"

        # Tests
        if (-not $SkipTests) {
            Write-Host ""
            Write-Host "Running C++ unit tests..." -ForegroundColor $colors.Info
            & ctest --test-dir $BuildDir --config $BuildType --output-on-failure 2>&1 | Tee-Object -FilePath (Join-Path $LogDir "10-cpp-tests.log")

            if ($LASTEXITCODE -eq 0 -or $LASTEXITCODE -eq 2) {
                Write-Success "C++ tests completed (no tests or all passed)"
            }
            else {
                Write-Warning-Custom "C++ tests had failures"
            }
        }
    }
    catch {
        Write-Error-Custom "C++ build failed: $_"
        exit 1
    }
}

# === ELECTRON PACKAGING ===
Write-Section "Electron Packaging"

try {
    Write-Host "Building Electron app..." -ForegroundColor $colors.Info
    $env:CI = "true"
    $env:WINDOWS = "true"

    npm run dist 2>&1 | Tee-Object -FilePath (Join-Path $LogDir "11-electron-build.log")
    if ($LASTEXITCODE -ne 0) {
        Write-Warning-Custom "Electron packaging had issues - continuing"
    }
    else {
        Write-Success "Electron app packaged"
    }
}
catch {
    Write-Error-Custom "Electron build failed: $_"
}

# === VERIFICATION ===
Write-Section "Verification"

$artifactChecks = @(
    "dist/web/index.html",
    "dist/main.js",
    "backend/dist/server.js"
)

$allFound = $true
foreach ($artifact in $artifactChecks) {
    $path = Join-Path $Root $artifact
    if (Test-Path $path) {
        $size = (Get-Item $path).Length / 1MB
        Write-Success "$artifact ($([math]::Round($size, 2)) MB)"
    }
    else {
        Write-Error-Custom "$artifact NOT FOUND"
        $allFound = $false
    }
}

if (Test-Path (Join-Path $Root "release")) {
    Write-Host ""
    Write-Host "Release directory contents:" -ForegroundColor $colors.Info
    Get-ChildItem (Join-Path $Root "release") -Recurse | Select-Object Name, @{N='Size(MB)'; E={[math]::Round($_.Length / 1MB, 2)}} | Format-Table -AutoSize
}

# === SUMMARY ===
Write-Section "Build Complete"

if ($allFound) {
    Write-Success "All artifacts present"
    Write-Host ""
    Write-Host "Next steps:" -ForegroundColor $colors.Info
    Write-Host "  • Test the app: npm start"
    Write-Host "  • Run frontend tests: npm test"
    Write-Host "  • View logs: ls build-logs\"
    Write-Host "  • Package release: npm run dist"
}
else {
    Write-Warning-Custom "Some artifacts missing - check logs in: $LogDir"
}

Write-Host ""
Write-Success "Build completed with Build Type: $BuildType"
Write-Host "Logs saved to: $LogDir"

Pop-Location
