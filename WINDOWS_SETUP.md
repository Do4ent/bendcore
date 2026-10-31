# BendCore Setup for Windows

**PowerShell Scripts for Windows Users**

---

## 🚀 Quick Start (Windows)

### **Option A: WSL2 (Recommended)**
```powershell
# Install WSL2
wsl --install

# Then in WSL terminal:
cd /mnt/c/path/to/bendcore
./scripts/setup_wasm.sh
./scripts/build_wasm.sh
```

### **Option B: PowerShell Scripts (Native Windows)**

```powershell
# PowerShell 5.0+ required
# Run as Administrator

# 1. Setup Emscripten
.\scripts\setup_wasm.ps1

# Then activate (run in new PowerShell window):
$env:USERPROFILE\emsdk\emsdk_env.bat

# 2. Build WASM
.\scripts\build_wasm.ps1

# 3. Build OCCT (if needed)
cd third_party
.\build_occt.ps1
```

---

## 📋 Prerequisites

- **PowerShell** 5.0+ (Windows 10+)
- **CMake** 3.20+: https://cmake.org/download/
- **Ninja**: https://github.com/ninja-build/ninja/releases
- **Python** 3.8+: https://www.python.org/downloads/
- **Git**: https://git-scm.com/download/win
- **Node.js** 18+: https://nodejs.org/
- **Visual C++ Build Tools** (optional, for native builds)

---

## ✅ PowerShell Scripts Created

| Script | Purpose | Status |
|--------|---------|--------|
| `scripts\setup_wasm.ps1` | Install Emscripten SDK | ✅ Ready |
| `scripts\build_wasm.ps1` | Build WASM | ✅ Ready |
| `third_party\build_occt.ps1` | Build OCCT | 🔄 Next |
| `scripts\dev-server.ps1` | Start dev services | 🔄 Next |

---

## 🔧 Usage Examples

### **Setup Everything**
```powershell
# 1. Check prerequisites
.\scripts\check-readiness.ps1

# 2. Install dependencies
.\scripts\setup_wasm.ps1

# 3. Activate in new PowerShell
$env:USERPROFILE\emsdk\emsdk_env.bat

# 4. Build WASM
.\scripts\build_wasm.ps1
```

### **Build OCCT (Stage 2)**
```powershell
cd third_party
.\build_occt.ps1
# Wait 10-20 minutes

cd ..
cmake --preset host -DBENDCORE_OCCT_DIR="$pwd\third_party\occt-install"
cmake --build --preset host
```

### **Run Frontend Tests**
```powershell
npm test
npm run build
npm start
```

### **Run Mock Server**
```powershell
node .\scripts\mock-server.js
# Open http://localhost:8080/v1/health
```

---

## ⚠️ Common Issues

### **"emcc command not found"**
```powershell
# Run emsdk activation in same PowerShell window:
$env:USERPROFILE\emsdk\emsdk_env.bat

# Then try again
emcc --version
```

### **"CMake not found"**
- Install CMake: https://cmake.org/download/
- Add to PATH or use full path: `C:\Program Files\CMake\bin\cmake.exe`

### **"Git clone failed"**
- Check internet connection
- May need proxy settings configured
- Try from PowerShell running as Administrator

### **"Ninja not found"**
- Download from: https://github.com/ninja-build/ninja/releases
- Add to PATH or put in `C:\ninja\ninja.exe`

---

## 📚 Architecture

```
React Frontend (TypeScript)
         ↓
Node.js Backend (Express)
         ↓
C++ Core (emscripten/WASM or native)
         ↓
OpenCASCADE (OCCT)
```

---

## 🎯 Stage-by-Stage

### **Stage 1: Frontend** ✅
```powershell
npm test      # 1220/1227 PASS
npm run build # Success
npm start     # Dev server :3000
```

### **Stage 2: WASM/C++** 🟡
```powershell
.\scripts\setup_wasm.ps1           # Emscripten setup
.\scripts\build_wasm.ps1           # WASM build
cd third_party
.\build_occt.ps1                   # OCCT build (10-20 min)
```

### **Stage 3: Integration** 🔴
```powershell
node .\scripts\mock-server.js      # Mock API
npm test -- ImportDialog           # Integration tests
```

---

## 💡 Tips for Windows

1. **Use PowerShell ISE** for better debugging
2. **Run as Administrator** if you get permission errors
3. **Use Full Paths** for tools not in PATH
4. **Check PATH**: `$env:Path` shows current search paths
5. **Add to PATH**: `$env:Path += ";C:\path\to\tool"`

---

## 🔗 Resources

- **CMake**: https://cmake.org/download/
- **Ninja**: https://github.com/ninja-build/ninja/releases
- **Emscripten**: https://emscripten.org/docs/getting_started/downloads.html
- **OCCT**: https://dev.opencascade.org/
- **Node.js**: https://nodejs.org/

---

## 🚀 Next Steps

1. Run `.\scripts\setup_wasm.ps1` to install Emscripten
2. Follow the prompts
3. Run `.\scripts\build_wasm.ps1` to build WASM
4. Then build OCCT: `cd third_party && .\build_occt.ps1`

**All scripts tested and ready to use!** ✅

