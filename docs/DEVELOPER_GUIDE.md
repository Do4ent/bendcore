# BendCore Developer Guide

**For Developers Working on Stage 2-3 Implementation**

---

## Quick Start

### Prerequisites Check
```bash
./scripts/check-stage2-readiness.sh
```

This script verifies:
- ✅ CMake, Ninja, Python3, Git, Node.js, npm installed
- ✅ 20+ GB disk space available
- ✅ All required project files present
- ⚠️ Optional: Emscripten SDK

If this passes, you're ready to proceed.

### Development Environment (Without OCCT)

For UI development and testing **without building OCCT**:

```bash
# Terminal 1: Start mock server
./scripts/dev-server.sh

# This starts:
# - Mock BendCore Server on http://localhost:8080
# - Frontend dev server on http://localhost:3000
```

Then open http://localhost:3000 in your browser.

The mock server provides:
- `GET /v1/health` - Health check
- `POST /v1/tube` - Generates tube mesh (mock data)
- `POST /v1/analyze` - Analyzes STEP file (mock data)

**Perfect for:** UI development, ImportDialog testing, Viewport3D testing

---

## Stage 2: C++ Integration (Local Setup)

### Full OCCT Build Setup

Only needed if you want to test real STEP file parsing.

**Estimated Time:** 2-4 hours (first time), 30 minutes (cached)

#### Step 1: Install Emscripten SDK

```bash
./scripts/setup_wasm.sh
source ~/.bashrc
```

Verification:
```bash
emcc --version
```

#### Step 2: Build OCCT

```bash
cd third_party
./build_occt.sh
# Takes 10-20 minutes
# Output: occt-install/
```

Verification:
```bash
ls -la occt-install/include/opencascade/Standard.hxx
ls -la occt-install/lib/libTKCore.a
```

#### Step 3: Build BendCore C++ with OCCT

```bash
cd ..
cmake --preset host \
  -DBENDCORE_OCCT_DIR=$PWD/third_party/occt-install \
  -DBENDCORE_WITH_OCCT=ON
cmake --build --preset host
```

#### Step 4: Build Backend Server

```bash
cd backend
npm install
npm run build
npm start
# Server runs on http://127.0.0.1:8080
```

Verification:
```bash
curl http://127.0.0.1:8080/v1/health
# Expected: {"ok":true}
```

#### Step 5: Test Real ImportDialog

```bash
# In another terminal
cd ..
npm start  # Frontend

# Open http://localhost:3000
# Try importing a real STEP file
```

---

## Testing

### Unit Tests (TypeScript)

```bash
npm test                    # All tests
npm test -- --watch        # Watch mode
npm test -- ImportDialog   # Specific file
```

### Integration Tests (with Mock Server)

```bash
# Make sure mock server is running
./scripts/dev-server.sh &

# Run integration tests
npm test -- ImportDialog.integration
```

### End-to-End Tests (with Real Backend)

```bash
# Prerequisites:
# 1. OCCT built (cd third_party && ./build_occt.sh)
# 2. Backend running (cd backend && npm start)

npm run e2e  # If configured
```

---

## File Structure

### Key Directories

```
bendcore/
├── src/
│   ├── ui/                  # React components
│   │   ├── Viewport3D.tsx       # 3D rendering (Three.js)
│   │   ├── ImportDialog.tsx     # STEP file import dialog
│   │   ├── tubeService.ts       # API client
│   │   └── __tests__/           # Component tests
│   ├── server/              # Node.js backend (if available)
│   └── ...                  # C++ core (CMakeLists.txt builds this)
│
├── scripts/
│   ├── setup_wasm.sh           # Emscripten installation
│   ├── build_wasm.sh           # WASM build
│   ├── mock-server.js          # Mock API for UI testing
│   ├── dev-server.sh           # Start all dev services
│   └── check-stage2-readiness.sh # Verify prerequisites
│
├── third_party/
│   ├── build_occt.sh           # OCCT build script
│   ├── occt-build/             # OCCT build directory (created)
│   └── occt-install/           # OCCT installation (created)
│
├── .github/workflows/
│   ├── build-test.yml          # Frontend CI/CD
│   ├── wasm.yml                # WASM + C++ CI/CD
│   └── ui-integration.yml      # UI integration tests
│
├── docs/
│   ├── IMPLEMENTATION_STATUS.md     # Honest project status
│   ├── STAGE_2_3_ROADMAP.md         # Implementation guide
│   ├── WASM_BUILD_GUIDE.md          # WASM setup details
│   └── DEVELOPER_GUIDE.md           # This file
│
└── ...
```

---

## Architecture Overview

### Layers

```
┌─────────────────────────────────┐
│   React UI (Viewport3D, etc)    │
│   TypeScript/JavaScript         │
├─────────────────────────────────┤
│   Node.js Backend (Express)     │
│   REST API (/v1/tube, analyze)  │
├─────────────────────────────────┤
│   C++ Core (OCCT integration)   │
│   CMake + Ninja build           │
├─────────────────────────────────┤
│   OpenCASCADE (OCCT)            │
│   CAD kernel, STEP parser       │
└─────────────────────────────────┘
```

### Data Flow (Import STEP File)

```
1. User uploads STEP file
   ↓
2. ImportDialog calls tubeService.analyzeStep()
   ↓
3. Frontend POSTs file to /v1/analyze
   ↓
4. Backend receives file, calls C++ function
   ↓
5. C++ (OCCT) parses geometry, recognizes tubes
   ↓
6. Backend returns mesh + tube properties
   ↓
7. Frontend renders in Viewport3D (Three.js)
   ↓
8. User sees 3D model
```

### API Endpoints (Backend)

```
GET  /v1/health
     Response: {"ok": true}

POST /v1/tube
     Query: format=csv|json, od=16, wall=1.5
     Body: CSV or JSON with path coordinates
     Response: {ok, tube: {od, wall, ...}, mesh: {vertices, indices}}

POST /v1/analyze
     Query: format=step, deflection=0.1
     Body: STEP file binary
     Response: {ok, tube: {od, wall, bends, ...}, mesh: {...}}
```

---

## Common Development Tasks

### Add New UI Component

1. Create in `src/ui/MyComponent.tsx`
2. Add test in `src/ui/__tests__/MyComponent.test.tsx`
3. Import and use in parent component
4. Run tests: `npm test -- MyComponent`

### Modify Viewport3D

1. Edit `src/ui/Viewport3D.tsx`
2. Update Three.js rendering logic
3. Test with mock server: `./scripts/dev-server.sh`
4. View in browser: http://localhost:3000

### Test with Real STEP Files

1. Build everything: `cd third_party && ./build_occt.sh`
2. Build C++: `cmake --preset host -DBENDCORE_OCCT_DIR=...`
3. Start backend: `cd backend && npm start`
4. Start frontend: `npm start`
5. Upload STEP file in ImportDialog

### Debug API Calls

1. Open browser DevTools (F12)
2. Go to Network tab
3. Try importing a file
4. Check requests/responses:
   - POST http://localhost:8080/v1/analyze
   - Check response JSON structure
   - Verify mesh data is present

### Check Mock Server Data

```bash
# Test /v1/tube endpoint
echo "od,length" | curl -X POST --data-binary @- \
  "http://localhost:8080/v1/tube?format=csv&od=16&wall=1.5" | jq .

# Test /v1/analyze endpoint
echo "test" | curl -X POST --data-binary @- \
  "http://localhost:8080/v1/analyze?format=step&deflection=0.1" | jq .
```

---

## Troubleshooting

### Port Already in Use

```bash
# Find what's using the port
lsof -i :8080    # For port 8080
lsof -i :3000    # For port 3000

# Kill the process
kill -9 <PID>
```

### Mock Server Not Starting

```bash
# Check logs
tail -f /tmp/bendcore-mock-server.log

# Test manually
node scripts/mock-server.js

# If it fails, check Node.js version
node --version  # Should be 18+
```

### OCCT Build Fails

```bash
# Check prerequisites
./scripts/check-stage2-readiness.sh

# Clean and retry
cd third_party
rm -rf occt-build occt-install
./build_occt.sh

# Check logs
cat occt-build/CMakeFiles/CMakeError.log
```

### Tests Failing

```bash
# Clear cache and reinstall
rm -rf node_modules package-lock.json
npm install

# Run tests with verbose output
npm test -- --verbose

# Debug specific test
npm test -- ImportDialog.integration --no-coverage
```

---

## Performance Tips

### Faster Builds

```bash
# Use parallel build
cmake --build --preset host -j 4

# Or use Ninja directly
ninja -C build-host -j 4
```

### Faster Tests

```bash
# Run only changed tests
npm test -- --onlyChanged

# Run without coverage
npm test -- --no-coverage

# Specific file only
npm test ImportDialog -- --no-coverage
```

### Faster OCCT Build (Cached)

```bash
# GitHub Actions caches OCCT
# Local build is cached in CMakeFiles/

# To clean cache:
rm -rf third_party/occt-build
```

---

## Debugging with DevTools

### Frontend Debugging

1. Chrome DevTools: F12
2. Check Console for errors
3. Check Network for API calls
4. Check Application > LocalStorage for settings

### Backend Debugging (Node.js)

```bash
# Start with debugging
node --inspect backend/src/server/main.js

# Open chrome://inspect in Chrome
```

### C++ Debugging (if using native build)

```bash
# Build with debug symbols
cmake --preset host -DCMAKE_BUILD_TYPE=Debug
cmake --build --preset host

# Debug with gdb
gdb ./build-host/bendcore_app
```

---

## CI/CD Pipelines

### build-test.yml (Frontend)
- Runs on every push/PR to main/develop
- Tests: npm test
- Build: npm run build
- Coverage: uploaded

### wasm.yml (C++/WASM)
- Runs on push to main
- Builds OCCT (cached)
- Builds WASM
- Runs C++ tests

### ui-integration.yml (UI Integration)
- Runs on every push/PR
- Starts mock server
- Runs ImportDialog integration tests
- Tests API endpoints

---

## Release Process

1. Run tests locally: `npm test`
2. Build frontend: `npm run build`
3. Check dist/: `ls -la dist/`
4. Commit changes
5. Create PR with description
6. After merge, monitor CI/CD

---

## Resources

### Documentation
- `IMPLEMENTATION_STATUS.md` - Current project state
- `STAGE_2_3_ROADMAP.md` - Implementation guide
- `WASM_BUILD_GUIDE.md` - WASM details

### External Links
- [OCCT Documentation](https://dev.opencascade.org/doc/overview/html/)
- [Three.js API](https://threejs.org/docs/)
- [CMake Documentation](https://cmake.org/documentation/)

### Scripts
- `check-stage2-readiness.sh` - Verify prerequisites
- `dev-server.sh` - Start all dev services
- `mock-server.js` - Mock API server
- `build_wasm.sh` - Build WASM
- `setup_wasm.sh` - Install Emscripten

---

## Getting Help

1. **Check documentation:** IMPLEMENTATION_STATUS.md, STAGE_2_3_ROADMAP.md
2. **Check logs:** /tmp/bendcore-*.log
3. **Run diagnostics:** ./scripts/check-stage2-readiness.sh
4. **Check tests:** npm test
5. **Review code:** src/ui/*, backend/src/

---

**Last Updated:** 2026-10-31  
**Maintainer:** BendCore Development Team  
**Status:** Active Development
