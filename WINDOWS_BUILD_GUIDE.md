# Windows Build Guide - BendCore v1.0

**Date:** November 1, 2026  
**Status:** Building on Windows 10/11  
**Support Level:** Community-supported (Docker recommended)

---

## Overview

This guide covers building BendCore v1.0 on Windows 10/11 from source. For most users, **Docker Desktop is recommended** instead (skip to "Option 2: Docker").

### Building Options

1. **Option 1: Docker Desktop (Recommended)** - 10 minutes
2. **Option 2: Native Build on Windows** - 45 minutes, requires MSVC compiler

---

## Option 1: Docker Desktop (Recommended) ✅ EASIEST

### Prerequisites
- Windows 10 21H2 or Windows 11
- 8+ GB RAM
- 50 GB free disk space
- Internet connection

### Step 1: Install Docker Desktop
1. Download from https://www.docker.com/products/docker-desktop
2. Run installer and follow prompts
3. Enable "WSL 2 backend" (Windows Subsystem for Linux 2)
4. Restart computer when prompted

### Step 2: Clone Repository
```bash
# Open PowerShell or Command Prompt
git clone https://github.com/Do4ent/bendcore.git
cd bendcore
```

### Step 3: Build and Run
```bash
# Build Docker images
docker-compose build

# Start services (may take 2-3 minutes on first run)
docker-compose up

# In another terminal, check status
docker-compose ps
```

### Step 4: Access Application
1. Open browser: http://localhost:3000
2. Click "📦 Import STEP/IGES"
3. Select a test tube model
4. Click "⚙️ Analyze (FEA)"

### Troubleshooting Docker

**Error: "Docker daemon is not running"**
- Start Docker Desktop from Windows Start menu
- Wait 30 seconds for Docker service to start

**Error: "WSL 2 installation is incomplete"**
```bash
# In PowerShell (as Administrator)
wsl --install
wsl --set-default-version 2
```

**Error: "Out of disk space"**
```bash
docker system prune -a  # Remove unused images
```

---

## Option 2: Native Windows Build (Advanced)

### Prerequisites
- Windows 10 21H2 or Windows 11
- Visual Studio 2022 Community (free) with C++ tools
- CMake 3.20+ (installer from cmake.org)
- Node.js 18 LTS (nodejs.org)
- Git for Windows (git-scm.com)
- 50 GB free disk space
- 8+ GB RAM

### Step 1: Install Visual Studio 2022 Community

1. Download installer: https://visualstudio.microsoft.com/downloads/
2. Run installer → Click "Community"
3. In workloads, select:
   - ✅ "Desktop development with C++"
   - ✅ "CMake tools for Windows"
4. Click Install (takes 15-30 minutes)

### Step 2: Install Dependencies

Open Command Prompt or PowerShell and run:

```bash
# Install Node.js 18 LTS
# Download from nodejs.org and run installer
# Or use Windows Package Manager:
winget install OpenJS.NodeJS.LTS

# Install CMake
# Download from cmake.org
# Or: winget install Kitware.CMake

# Verify installations
node --version    # Should be v18+
npm --version     # Should be 9+
cmake --version   # Should be 3.20+
```

### Step 3: Install OpenCASCADE

```bash
# Option A: Using vcpkg (Visual C++ package manager)
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg

# Bootstrap vcpkg
.\bootstrap-vcpkg.bat

# Install OpenCASCADE
.\vcpkg install opencascade:x64-windows

# Add to system PATH for CMake to find it
# Set VCPKG_ROOT environment variable to vcpkg directory
```

### Step 4: Clone and Build BendCore

```bash
# Clone repository
git clone https://github.com/Do4ent/bendcore.git
cd bendcore

# Install Node.js dependencies
npm install

# Create build directory
mkdir build
cd build

# Configure with CMake (Point to Visual Studio 2022)
cmake -G "Visual Studio 17 2022" -A x64 ..

# Build
cmake --build . --config Release

# Back to root
cd ..
```

### Step 5: Build Frontend

```bash
# Build React UI
npm run build:web

# Output will be in: ui/dist/web/
```

### Step 6: Run Services

You need to start 3 services in separate terminals:

**Terminal 1 - Node.js API Server:**
```bash
npm start
# Runs on http://localhost:3001
```

**Terminal 2 - C++ CAD Server:**
```bash
cd build/Release
# Find bendcore_server executable
# ./bendcore_server
# Or run from VS: Open bendcore.sln and run bendcore_server

# Should show:
# BendCore Server listening on port 8080
```

**Terminal 3 - Frontend Dev Server:**
```bash
npm run dev:ui
# Runs on http://localhost:3000
```

### Step 7: Access Application
1. Open http://localhost:3000 in browser
2. You should see the import interface

---

## Building Individual Components

### Just C++ Backend
```bash
cd build
cmake --build . --config Release --target bendcore_server
```

### Just Frontend
```bash
npm run build:web
```

### Just Tests
```bash
npm test              # Frontend tests
npm run test:e2e      # E2E tests
cd backend && npm test && cd ..  # C++ tests
```

---

## Environment Variables

Create `.env.local` in project root:

```env
# Frontend
REACT_APP_API_URL=http://localhost:3001
REACT_APP_BENDCORE_SERVER=http://localhost:8080

# Backend
NODE_ENV=development
BENDCORE_SERVER_PORT=8080
BENDCORE_SERVER_HOST=localhost

# C++ Server
OCCT_DIR=C:\vcpkg\packages\opencascade_x64-windows
```

---

## Troubleshooting

### "CMake not found"
```bash
# Add CMake to PATH manually
# Or reinstall with "Add CMake to PATH" option checked
```

### "OpenCASCADE not found"
```bash
# Make sure vcpkg is in VCPKG_ROOT
$env:VCPKG_ROOT="C:\path\to\vcpkg"

# Or set in CMake:
cmake -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake ..
```

### "MSVC compiler not found"
```bash
# Make sure Visual Studio C++ tools are installed
# Or force specific compiler version:
cmake -G "Visual Studio 17 2022" -A x64 ..
```

### "Port 3000/3001/8080 already in use"
```powershell
# Find process using port
netstat -ano | findstr :3001

# Kill process (replace PID)
taskkill /PID [PID] /F
```

### Build takes forever
- Normal first build: 15-30 minutes
- Incremental builds: 1-5 minutes
- Close VS Code or other IDEs to free RAM

---

## Performance Tips

1. **Use Release builds** (not Debug) for better performance
2. **Close unnecessary programs** when building
3. **Use SSD** for faster builds
4. **Use Docker** if available (much faster)
5. **Increase Windows virtual memory** if you get out-of-memory errors

---

## Next Steps

After successful build:

1. **Test the import workflow:**
   - Click "📦 Import STEP/IGES"
   - Select a tube geometry file
   - Click "⚙️ Analyze (FEA)"
   - Click "🎯 Optimize"
   - Click "📋 Export Drawing"

2. **Run automated tests:**
   ```bash
   npm test
   ```

3. **Check the logs:**
   - Frontend: http://localhost:3000/logs
   - Backend: Check console where `npm start` runs
   - C++ Server: Check console where bendcore_server runs

---

## Known Issues

### Windows Defender Slowdown
Windows Defender may slow down builds. Temporarily exclude build directory:
- Settings → Virus & threat protection → Manage settings
- Add exclusion for `bendcore/build/`

### Slow Git Clone
If cloning is slow, try:
```bash
git clone https://github.com/Do4ent/bendcore.git --depth=1
```

### Out of Memory During Build
Set Windows virtual memory to 16+ GB:
- Settings → System → About → Advanced system settings
- Advanced tab → Performance → Virtual Memory

---

## Windows Command Reference

```bash
# Check installed software
wmic product list brief  # List installed programs

# Kill process by port
netstat -ano | findstr :3001
taskkill /PID [PID] /F

# Check environment variables
$env:Path
$env:VCPKG_ROOT

# Set environment variables (temporary)
$env:NODE_ENV="development"

# Set environment variables (permanent)
[Environment]::SetEnvironmentVariable("NODE_ENV", "development", "User")
```

---

## Docker Quick Reference

```bash
# View running containers
docker ps

# View all images
docker images

# Stop container
docker stop bendcore-web-1

# View logs
docker logs bendcore-api-1

# Rebuild images
docker-compose build --no-cache

# Remove everything (warning: destructive)
docker system prune -a
```

---

## Support

For issues:
1. Check Docker Desktop is running
2. Check ports 3000, 3001, 8080 are free
3. Check 50+ GB disk space available
4. Check network connectivity
5. Try Docker (easier than native build)

---

**Last Updated:** November 1, 2026  
**Tested On:** Windows 11 Pro, Docker Desktop 4.20+  
**Status:** Working ✅

For latest updates, see [README.md](README.md) and [ROADMAP.md](ROADMAP.md)
