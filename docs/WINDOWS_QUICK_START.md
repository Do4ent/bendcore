# Windows Build Quick Start

Get BendCore building on Windows in 5 minutes.

## 1️⃣ Install Prerequisites

```powershell
# Option A: Chocolatey (recommended)
choco install cmake ninja python nodejs git -y

# Option B: Manual
# • CMake 3.28+: https://cmake.org/download/
# • Ninja: https://github.com/ninja-build/ninja/releases
# • Python 3.8+: https://www.python.org/downloads/
# • Git: https://git-scm.com/download/win
# • Node.js 18+: https://nodejs.org/
```

**Verify installation:**
```powershell
cmake --version      # Should show 3.28+
ninja --version      # Should show 1.11+
node --version       # Should show v18+
python --version     # Should show Python 3.8+
git --version        # Should show git 2.x
```

## 2️⃣ Clone Repository

```powershell
git clone https://github.com/Do4ent/bendcore.git
cd bendcore
```

## 3️⃣ Run Full Build

**One command:**
```powershell
.\scripts\windows-build.ps1
```

**Build Type:**
```powershell
.\scripts\windows-build.ps1 -BuildType Debug      # Slower, better debugging
.\scripts\windows-build.ps1 -BuildType Release    # Faster, optimized
```

**Skip steps:**
```powershell
.\scripts\windows-build.ps1 -SkipTests        # Skip all tests
.\scripts\windows-build.ps1 -SkipCpp          # Skip C++ build
```

## 4️⃣ What Gets Built

```
✓ Frontend (React + TypeScript)
✓ Backend (Express.js)
✓ Tests (Jest + C++ unit tests)
✓ Electron app (.exe installer)
```

**Outputs:**
- `dist/` → Web app (React)
- `backend/dist/` → Backend server
- `release/*.exe` → Windows executable
- `release/*.msi` → Windows installer
- `build-logs/` → Build details

## 5️⃣ Verify Build Success

```powershell
# Check if key files exist
Test-Path dist/web/index.html        # ✓ Should exist
Test-Path dist/main.js               # ✓ Should exist
Test-Path backend/dist/server.js     # ✓ Should exist
Test-Path release/*.exe              # ✓ Should exist
```

## Next Steps

### Run Tests Locally
```powershell
npm test                              # Jest unit tests
npm run typecheck                     # TypeScript check
npm test --prefix backend -- --ci     # Backend tests
```

### Start Dev Server
```powershell
npm start                             # Runs Electron app
```

### Deploy to GitHub

GitHub Actions automatically builds on:
- Push to `main` or `develop`
- Pull requests
- Manual trigger (Actions → Windows Build → Run)

**View builds:**
1. GitHub → **Actions** tab
2. Click **"Windows Build & Validation"**
3. Click run to see logs

---

## Common Issues

| Issue | Solution |
|-------|----------|
| "cmake not found" | Install from https://cmake.org/download/ |
| "TypeScript errors" | Run `npm run typecheck` to see details |
| "Tests fail" | Run `npm test -- --no-coverage` |
| "Permission denied" | Run PowerShell as Administrator |
| "Ninja not found" | Install from https://github.com/ninja-build/ninja/releases |

---

## Full Documentation

→ **[WINDOWS_CI_CD_PIPELINE.md](./WINDOWS_CI_CD_PIPELINE.md)** - Complete pipeline documentation

---

**Estimated times:**
- First build: 10-15 minutes
- Cached build: 5-8 minutes
- Just tests: 2-3 minutes

**Questions?** Check build logs in `build-logs/` directory.
