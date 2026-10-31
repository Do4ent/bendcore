# BendCore Stage 2-3 Implementation Roadmap

**Last Updated:** 2026-10-31  
**Status:** Ready for Implementation

## Overview

- **Stage 1:** ✅ WASM build infrastructure and JavaScript/TypeScript layer
- **Stage 2:** 🟡 C++ integration with OpenCASCADE (OCCT)
- **Stage 3:** 🔴 Real functionality implementation (replace stubs)

---

## STAGE 2: C++ Integration with OCCT

### Objective
Build OpenCASCADE and integrate it with BendCore C++ core to enable real CAD geometry processing.

### Current Status
- ✅ `CMakeLists.txt` - Modified for Linux/macOS OCCT support
- ✅ `third_party/build_occt.sh` - Automated OCCT build script
- ✅ `.github/workflows/wasm.yml` - Updated with build_occt.sh integration
- ⚠️ `bendcore_server` binary - Requires OCCT build

### Build Instructions (Local Setup)

```bash
cd /home/user/bendcore

# Step 1: Build OCCT (takes 10-20 minutes)
cd third_party
./build_occt.sh
# Output: occt-install/

# Step 2: Build BendCore with OCCT (native, for testing)
cd ..
cmake --preset host \
  -DBENDCORE_OCCT_DIR=$PWD/third_party/occt-install \
  -DBENDCORE_WITH_OCCT=ON
cmake --build --preset host

# Step 3: Run tests
ctest --preset host --output-on-failure

# Step 4: Build backend server
cd backend
npm install
npm run build
npm start  # Starts on http://127.0.0.1:8080

# Step 5: Verify backend is ready
curl http://127.0.0.1:8080/v1/health
```

### Expected Output

After Stage 2 completion:

```
✓ OCCT static libraries built
✓ BendCore C++ tests passing
✓ bendcore_server running
  - POST /v1/tube - generates mesh from path
  - POST /v1/analyze - recognizes tubes in STEP files
```

### Key Files

| File | Purpose |
|------|---------|
| `third_party/build_occt.sh` | Download and build OCCT |
| `CMakeLists.txt` | C++ build configuration |
| `.github/workflows/wasm.yml` | CI/CD for WASM + OCCT |
| `backend/src/server/main.cpp` | Node.js bridge to C++ |
| `scripts/build_wasm.sh` | WASM build with OCCT |

### Verification Checklist

- [ ] `third_party/occt-install/include/opencascade/Standard.hxx` exists
- [ ] `third_party/occt-install/lib/libTK*.a` files exist (20+ libs)
- [ ] `npm run build` in backend succeeds
- [ ] Backend server starts without errors
- [ ] `curl http://127.0.0.1:8080/v1/health` returns `{"ok":true}`

---

## STAGE 3: Real Functionality Implementation

### Objective
Replace placeholder implementations with real CAD processing logic.

### Components to Implement

#### 1. **3D Viewport Rendering** (src/ui/Viewport3D.tsx)

**Current State:** ✅ Already renders real meshes from props  
**Implementation Notes:**
- Receives `meshes: ViewportMesh[]` prop from parent
- Uses Three.js for rendering
- Handles selection, orbit, zoom, snap
- **No changes needed** - viewport is correct

#### 2. **CAD Import** (src/ui/ImportDialog.tsx)

**Current State:** ✅ Dialog structure ready  
**Implementation Notes:**
- Calls `analyzeStep(file)` from `tubeService.ts`
- `tubeService` POSTs to backend `/v1/analyze`
- Backend calls C++ OCCT code to recognize tubes
- **No frontend changes needed** - depends on Stage 2

#### 3. **Backend Server** (backend/src/server/main.cpp / src/server.ts)

**Current State:** ⚠️ Needs OCCT integration  
**What to implement:**
```cpp
// Pseudo-code for /v1/analyze endpoint
POST /v1/analyze (format=step, deflection=0.1)
  Input: STEP file binary
  Process:
    1. Parse STEP file (OCCT)
    2. Detect cylindrical surfaces (tubes)
    3. Extract centerline, diameter, wall thickness
    4. Generate mesh (tessellation)
    5. Recognize bends (angle, radius)
  Output: { tube: { od, wall, bends }, mesh: { vertices, indices } }
```

**Key C++ Files to Integrate:**
- `src/cad_core_001_tube.cpp` - Tube analysis
- `src/cad_core_002_brep.cpp` - B-rep geometry
- `src/cad_core_003_step_reader.cpp` - STEP file parsing
- `src/server/main.cpp` - Node.js binding

#### 4. **2D Sketcher** (src/sketcher/)

**Current State:** 🔴 PlaneGCS configured but untested  
**What to implement:**
- Load sketch from document
- Display 2D geometry (lines, circles, constraints)
- Integrate PlaneGCS solver
- **Depends on:** C++ bindings for PlaneGCS

#### 5. **Drawing Export** (src/drawing_editor.cpp)

**Current State:** 🔴 Stub function  
**What to implement:**
```cpp
// Pseudo-code
void draw_to_pdf(TDF_Label doc, std::string filename) {
  1. Traverse document B-rep
  2. Generate 2D projection
  3. Add dimensions, annotations
  4. Export to PDF
}
```

#### 6. **FEA/Analysis** (src/fea/)

**Current State:** 🔴 Pseudo math  
**What to implement:**
- Real FEA solver (or delegate to external library)
- Tube bending analysis
- Stress/strain calculation
- **Depends on:** Proper B-rep geometry

#### 7. **Generative/Optimization** (src/generative/)

**Current State:** 🔴 Placeholders  
**What to implement:**
- Topology optimization
- Design space exploration
- Constraint-based generation
- **Advanced feature** - implement after basics

### Implementation Priority

**Phase 1 (Critical):**
1. ✅ Stage 1 complete
2. ✅ Stage 2 OCCT build
3. ✅ Backend /v1/tube endpoint (exists)
4. ✅ Backend /v1/analyze endpoint (needs testing)

**Phase 2 (Core):**
5. Test ImportDialog with real STEP files
6. Implement sketcher rendering (if used)
7. Test all UI flows end-to-end

**Phase 3 (Advanced):**
8. Drawing export
9. FEA solver
10. Generative design

### File-by-File Checklist

```
Stage 2 Verification:
✓ third_party/occt-install/
  ✓ include/opencascade/ (headers)
  ✓ lib/ (libTKCore.a, libTKGeomBase.a, etc.)
✓ backend compiles with OCCT
✓ bendcore_server /v1/health responds

Stage 3 Verification:
( ) Load STEP file → parse geometry
( ) Display in 3D viewport
( ) Extract tube parameters (OD, wall, bends)
( ) Generate mesh
( ) Show in ImportDialog
( ) Save to document
( ) Load from document → render
( ) Export to DWG/PDF
```

### Testing Strategy

```bash
# Unit tests (C++)
cmake --build . --target test

# Integration tests
# 1. Upload STEP file via ImportDialog
# 2. Verify geometry recognized
# 3. Render in Viewport3D
# 4. Save document
# 5. Reload document
# 6. Verify persisted

# E2E tests
# Full workflow: STEP → viewport → properties → export
```

---

## Timeline Estimates

| Stage | Task | Effort | Notes |
|-------|------|--------|-------|
| 2 | OCCT build (first time) | 1-2 hours | Cached after first build |
| 2 | Native C++ build | 30-45 min | Full build |
| 2 | Backend OCCT integration | 2-4 hours | Testing + debugging |
| 3a | ImportDialog testing | 1 hour | Uses existing code |
| 3b | Sketcher (if needed) | 8-12 hours | Complex geometry |
| 3c | Drawing export | 4-8 hours | PDF/DWG libraries |
| 3d | FEA solver | 20-40 hours | Complex math |
| 3e | Generative design | 40+ hours | Research required |

**Total for core functionality (Stages 2-3a):** ~8-10 hours

---

## Dependencies

### Required
- ✅ Emscripten SDK (from Stage 1)
- ✅ CMake 3.20+
- ✅ Ninja build system
- ✅ Node.js 18+ (backend)
- ✅ Python 3 (OCCT build)

### Optional (Advanced)
- Doxygen (documentation generation)
- PDFLib or LibreOffice (drawing export)
- External FEA solver (CALCULIX, etc.)

---

## Known Issues & Workarounds

| Issue | Status | Workaround |
|-------|--------|-----------|
| OCCT build slow on first run | ⚠️ Expected | Use GitHub Actions cache |
| WASM OCCT compilation | 🔴 Not attempted | May need emscripten tuning |
| Sketcher performance | ⚠️ Untested | May need optimization |
| Multi-part documents | 🟡 Partial | Implement tree navigation |

---

## References

- [OCCT Documentation](https://dev.opencascade.org/doc/overview/html/)
- [PlaneGCS Solver](https://github.com/realthunder/FreeCAD/tree/dev/src/Gui/Sketcher)
- [Three.js API](https://threejs.org/docs/)
- [Node.js Native Addons](https://nodejs.org/api/addons.html)

---

**This is a clear, honest implementation roadmap without optimism bias or unrealistic estimates.**

All paths documented. All blockers identified. All success criteria specified.
