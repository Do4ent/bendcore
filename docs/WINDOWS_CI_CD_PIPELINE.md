# BendCore Windows CI/CD Pipeline

Complete automated build and validation system for Windows using GitHub Actions and local PowerShell scripts.

---

## Overview

The Windows CI/CD pipeline provides:

- **Automated builds** on windows-2022 runner (MSVC, CMake, Ninja)
- **TypeScript strict mode** validation (0 errors enforced)
- **Jest unit tests** (1220+ tests)
- **C++ native components** build and test
- **Electron packaging** (EXE + MSI)
- **Artifact generation** and verification
- **Test coverage** reports
- **Reproducible local builds** via PowerShell scripts

---

## GitHub Actions Pipeline

### Workflow File
**Location:** `.github/workflows/windows-build.yml`

### Trigger Events
```yaml
on:
  push:
    branches: [ main, develop, ccr-* ]
    tags: [ 'v*' ]
  pull_request:
    branches: [ main, develop ]
  workflow_dispatch:
```

### Build Matrix
- **Runner:** `windows-2022`
- **Node versions:** `18.x`, `20.x`
- **Strategy:** `fail-fast: false` (continue on failure)

### Pipeline Stages

#### 1. Environment Setup
```
✓ Checkout code (with submodules)
✓ Node.js cache setup
✓ Python 3.11 installation
✓ CMake 3.28+ installation
✓ Ninja build system
✓ MSVC compiler (via setup-msbuild)
✓ vcpkg package manager
```

#### 2. Frontend Build
```
✓ npm ci (clean install with lock file)
✓ TypeScript strict type check
✓ npm run build
✓ Jest unit tests (--ci --runInBand)
```

**Failure:** Blocks entire pipeline (must pass)

#### 3. Backend Build
```
✓ npm install (backend/)
✓ npm run build
✓ npm test --ci
```

**Failure:** Blocks Electron packaging

#### 4. C++ Native Components
```
✓ CMake configuration (Visual Studio 17 2022 generator)
✓ cmake --build (Release, parallel)
✓ ctest (C++ unit tests)
```

**Failure:** Non-blocking (warns but continues)

#### 5. Electron Packaging
```
✓ npm run dist
✓ Generates: .exe, .msi, update manifests
```

**Output locations:**
- Executables: `release/*.exe`
- Installers: `release/*.msi`
- Update manifests: `release/*.yml`

#### 6. Artifact Verification
```
✓ Verify build outputs exist
✓ Check Windows executables
✓ Verify distribution contents
```

#### 7. Upload & Archive
```
✓ Build logs → artifacts
✓ Executables (.exe/.msi) → artifacts
✓ Web distribution → artifacts
✓ C++ build outputs → artifacts
✓ 30-day retention
```

---

## Local Windows Build

### Prerequisites
Install required tools:

```powershell
# Install via Chocolatey (recommended)
choco install cmake ninja python git nodejs -y

# Or install manually:
# • CMake: https://cmake.org/download/
# • Ninja: https://github.com/ninja-build/ninja/releases
# • Python 3.8+: https://www.python.org/downloads/
# • Git: https://git-scm.com/download/win
# • Node.js 18+: https://nodejs.org/
```

### Run Full Build
```powershell
cd C:\path\to\bendcore
.\scripts\windows-build.ps1 -BuildType Release
```

### Build Variants

**Debug build (slower, better debugging):**
```powershell
.\scripts\windows-build.ps1 -BuildType Debug
```

**Skip C++ compilation (frontend/backend only):**
```powershell
.\scripts\windows-build.ps1 -SkipCpp
```

**Skip all tests (faster, use with caution):**
```powershell
.\scripts\windows-build.ps1 -SkipTests
```

### Build Outputs

```
build-logs/
├── 01-npm-install.log       # Frontend dependencies
├── 02-typecheck.log         # TypeScript check
├── 03-frontend-build.log    # npm run build
├── 04-jest-tests.log        # Jest unit tests
├── 05-backend-install.log   # Backend dependencies
├── 06-backend-build.log     # Backend build
├── 07-backend-tests.log     # Backend tests
├── 08-cmake-config.log      # CMake configuration
├── 09-cmake-build.log       # C++ compilation
├── 10-cpp-tests.log         # C++ unit tests
└── 11-electron-build.log    # Electron packaging

dist/
├── web/                      # React frontend (served)
├── main.js                   # Electron main process
└── index.html

backend/dist/
└── server.js                # Express backend

build-native/                 # CMake output
├── Debug/                    # Object files, etc.
└── CMakeFiles/

release/
├── BendCore-Setup.exe       # NSIS installer
├── BendCore.exe             # Portable executable
└── latest.yml               # Update manifest
```

---

## C++ Integration

### CMake Configuration

Located in: `CMakeLists.txt`

```cmake
# Windows-specific settings
if(MSVC)
  add_compile_options(/fp:precise /fp:contract-)
  set(CMAKE_CXX_COMPILER cl.exe)
  set(CMAKE_C_COMPILER cl.exe)
endif()
```

### Dependency Management

**Eigen (linear algebra library):**
```cmake
find_package(Eigen3 3.3 REQUIRED)
# Or specify manually:
# -DBENDCORE_EIGEN_DIR="C:/vcpkg/installed/x64-windows/include"
```

**OCCT (CAD kernel) - Optional:**
```cmake
# Currently disabled in CI: -DBENDCORE_WITH_OCCT=OFF
# To enable locally:
# -DBENDCORE_WITH_OCCT=ON
# -DBENDCORE_OCCT_DIR="path/to/occt/install"
```

### Build Commands

```powershell
# Configure (generates build files)
cmake -B build-native -G "Visual Studio 17 2022" `
  -DCMAKE_BUILD_TYPE=Release

# Build (compile code)
cmake --build build-native --config Release --parallel 4

# Test (run unit tests)
ctest --test-dir build-native --config Release --output-on-failure
```

---

## TypeScript Requirements

**Strict mode enforced:** All code must compile with zero errors.

```powershell
npm run typecheck    # Runs tsc --noEmit --strict
```

**Common issues:**
- `any` types not allowed
- Null/undefined checks required
- Unused variables must be removed
- Import all dependencies explicitly

---

## Jest Test Requirements

**Minimum:** All tests must pass
**Coverage target:** >80% for changed files

```powershell
npm test -- --ci --runInBand --coverage
```

**Options:**
- `--ci`: Suitable for CI/CD
- `--runInBand`: Sequential execution (more stable)
- `--coverage`: Generate coverage reports

---

## Electron Packaging

### Configuration Files
- `package.json`: `"build"` section (electron-builder)
- `electron-builder.json`: Platform-specific settings

### Windows Output
```
release/
├── BendCore-Setup-1.0.0.exe      # NSIS installer (recommended)
├── BendCore-1.0.0.exe            # Portable executable
├── BendCore-1.0.0-ia32.exe       # 32-bit version (if configured)
└── latest.yml                     # Update manifest
```

### Build Command
```powershell
npm run dist
# Equivalent to: electron-builder --win
```

### Code Signing (Optional)
```powershell
$env:WIN_CSC_KEY_PASSWORD = "your-password"
$env:WIN_CSC_LINK = "path/to/certificate.pfx"
npm run dist
```

---

## Test Coverage

### Frontend Tests (Jest)
```
src/ui/              → ~95% coverage
src/utils/           → ~90% coverage
src/api/             → ~85% coverage
src/components/      → ~80% coverage
```

Run: `npm test -- --coverage`

### Backend Tests
```
backend/src/routes/   → ~85% coverage
backend/src/services/ → ~90% coverage
```

Run: `npm test --prefix backend -- --coverage`

### C++ Tests
```
tests/               → Unit tests (GTest)
```

Run: `ctest --test-dir build-native --config Release`

---

## CI/CD Environment Variables

### GitHub Actions Secrets
```yaml
GH_TOKEN: ${{ secrets.GITHUB_TOKEN }}  # Auto-provided
```

### Build Environment
```
CI=true               # Set by workflow
WINDOWS=true          # Set by workflow
NODE_ENV=production   # For Electron build
```

---

## Troubleshooting

### Issue: "CMake not found"
**Solution:** Install CMake from https://cmake.org/download/

Verify:
```powershell
cmake --version  # Should show v3.28+
```

### Issue: "npm ERR! ERESOLVE unable to resolve dependency tree"
**Solution:** Clear npm cache and reinstall
```powershell
npm cache clean --force
rm -Recurse node_modules, package-lock.json
npm ci
```

### Issue: "TypeScript compilation failed"
**Solution:** Check for `any` types and strict mode violations
```powershell
npm run typecheck  # Show detailed errors
```

### Issue: "Jest tests timeout"
**Solution:** Run sequentially with longer timeout
```powershell
npm test -- --ci --runInBand --testTimeout=10000
```

### Issue: "C++ build fails with 'cl.exe not found'"
**Solution:** Install MSVC or Visual Studio
```powershell
# Via Visual Studio installer: C++ Desktop Development workload
# Or: Visual C++ Build Tools (lightweight)
```

### Issue: "Electron build creates empty .exe"
**Solution:** Verify `dist/main.js` exists and contains valid code
```powershell
ls -la dist/main.js
npm run build  # Rebuild TypeScript first
npm run dist   # Then rebuild Electron
```

---

## GitHub Actions Workflow Tips

### View Logs
1. Go to repository → **Actions** tab
2. Click on workflow run (by branch/tag)
3. Click job (e.g., "build-windows [18.x]")
4. Expand each step to see logs

### Download Artifacts
1. Workflow completed → **Artifacts** section
2. Select artifact to download:
   - `windows-executables-node-20.x` → .exe/.msi files
   - `build-logs-node-20.x` → compilation logs
   - `web-dist-node-20.x` → web distribution

### Cancel Running Build
1. Workflow running → click **X** button
2. Reason: "Dismiss" or "Remove"

### Manual Trigger
```powershell
# Trigger workflow from any branch
gh workflow run windows-build.yml --ref main
```

---

## Performance Optimization

### Cache Strategy
- **npm cache:** Per `package-lock.json` hash
- **vcpkg cache:** Per CMakeLists.txt hash
- **Build artifacts:** Deleted after 30 days

### Parallel Builds
- **Frontend:** Webpack parallel compilation
- **Backend:** Multi-threaded Node.js
- **C++:** `cmake --build --parallel 4`

### Build Time Targets
- **Frontend:** ~2-3 minutes (with cache: ~1 min)
- **Backend:** ~1-2 minutes (with cache: ~30 sec)
- **C++ (first):** ~5-10 minutes
- **C++ (incremental):** ~1-2 minutes
- **Electron:** ~2-3 minutes
- **Total:** ~15-20 minutes (first), ~8-10 minutes (cached)

---

## Release Process

### Automatic Release (on tag)
```powershell
# Tag a commit
git tag -a v1.0.0 -m "Release 1.0.0"
git push origin v1.0.0

# Workflow automatically:
# 1. Builds application
# 2. Creates GitHub release
# 3. Uploads .exe + .msi files
# 4. Generates checksums
```

### Manual Release
```powershell
# If workflow didn't create release automatically:
gh release create v1.0.0 \
  release/BendCore-Setup-1.0.0.exe \
  release/BendCore-1.0.0.exe \
  -n "Release notes here"
```

---

## Best Practices

### Before Pushing
1. Run local build: `.\scripts\windows-build.ps1`
2. Verify tests pass: `npm test`
3. Check TypeScript: `npm run typecheck`
4. Check types in your changes

### Commit Messages
```
Describe the change clearly
- TypeScript: fixes, features, refactoring
- C++: compilation fixes, performance
- Tests: coverage improvements
- CI/CD: workflow updates

Example:
  fix: resolve TypeScript strict mode errors in ImportDialog.tsx
  
  - Added type annotations to props
  - Fixed null/undefined checks
  - All tests passing (1227/1227)
```

### Pull Requests
- Wait for CI/CD to complete (green checkmarks)
- No merging with failed builds
- Review logs if build fails
- Use "Draft" for work-in-progress

---

## Resources

- [GitHub Actions Docs](https://docs.github.com/en/actions)
- [Windows Runners](https://docs.github.com/en/actions/using-github-hosted-runners/about-windows-runners)
- [CMake Documentation](https://cmake.org/cmake/help/latest/)
- [Ninja Build System](https://ninja-build.org/)
- [Electron Builder](https://www.electron.build/)

---

## Support

For issues or questions:
1. Check build logs (GitHub Actions → Artifacts)
2. Run local build with `-BuildType Debug`
3. Review error messages in `build-logs/` directory
4. Check related issues in repository

---

**Last Updated:** 2026-11-01
**Maintained by:** BendCore Development Team
