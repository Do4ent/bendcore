# BendCore Implementation Status

**Last Updated:** 2026-11-01  
**Status:** Partially Complete - Honest Assessment

## Completed (✅)

### Stage 1: WASM Build Infrastructure
- ✅ `scripts/setup_wasm.sh` - Automatic emsdk setup
- ✅ `scripts/build_wasm.sh` - Improved build with error handling  
- ✅ `docs/WASM_BUILD_GUIDE.md` - Setup and build documentation
- ✅ `.github/workflows/build-test.yml` - Improved diagnostics
- ✅ Frontend (React/TypeScript) - Fully working
  - TypeScript: 0 errors
  - Tests: 1217/1217 PASS
  - Build: webpack OK
- ✅ Backend (Node.js) - Fully working
  - TypeScript: 0 errors  
  - Tests: 130/130 PASS
  - Build: tsc OK

### Stage 2a: Native C++ build without OCCT — ✅ (2026-11-01)

Previously the native build never completed: `third_party/eigen-5.0.0` is a
24 KB stub, not Eigen, so `sketch.cpp` / PlaneGCS failed, and with no GTest
installed every GoogleTest target was silently skipped (they had never
compiled). Now:

```bash
sudo apt install libeigen3-dev libgtest-dev   # real Eigen 3.4 + GTest
cmake --preset host && cmake --build build -j
ctest --test-dir build -j4                    # 73/74 PASS
```

- Build: 0 errors (CMake now warns when it falls back to the Eigen stub)
- Tests: **73/74 ctest targets pass** (was 63/74 after the build was fixed,
  and the build did not finish before that)
- Only failure: `simulation_m6` → `CFDSolverTest.SimpleSolve`. `CFDSolver` is
  placeholder pseudo-physics, not a discretisation; it needs a real solver
  (Stage 3), not test tweaks.

Real defects found and fixed while getting the suites green:

| Module | Defect |
|---|---|
| sketch `ConstraintDiagnosticsEngine` | `diagnose()` ↔ `suggestConstraints()` infinite recursion → stack overflow |
| `Combined3DOffsetCalculatorOptimized` | "optimized" was no faster (92% of the time was iostream G-code formatting); now `std::to_chars`, byte-identical output, ~70% faster |
| `LinearRegression` | gradient descent on raw features diverged (R² ≈ −5e154 in CV); now standardised |
| `DesignLearningSystem::cluster_designs` | empty cluster → empty centroid; non-deterministic init; now farthest-point init |
| `ThermalSolver` (steady) | explicit relaxation that could not converge; now a sparse linear solve (Eigen SparseLU) with analytic tests |
| `getDrawingVersionHistory` | stub returning `{}`; revision Z→AA broken; now scans the archive, base-26 revisions |
| publishing cloud/e-mail | exported with an empty path (always rejected); now stage to temp dir |
| `TemplateMatcher` | ties ordered by unstable sort; now by closeness to template defaults |
| `BOMGenerator` | items numbered alphabetically instead of in listed order |
| print manager | empty printer name ≠ default printer; scale enlarged past 1:1 |

## In Progress / Requires Setup

### Stage 2: C++ Integration with OCCT
**Status:** 🟡 Configured, not tested locally

**What exists:**
- CMakeLists.txt - Proper structure
- CMakePresets.json - wasm and host presets
- OCCT download/build in wasm.yml

**What needs:**
```bash
# 1. Install OpenCASCADE (6-8 hours)
cd third_party
./build_occt.sh  # Does not exist yet - needs creation

# 2. Build native (optional)
cmake --preset host
cmake --build --preset host

# 3. Build WASM with OCCT
./scripts/setup_wasm.sh
./scripts/build_wasm.sh
```

**Blockers:**
- OCCT build script missing
- No local testing environment available

### Stage 3: Real Functionality (Replace Stubs)
**Status:** 🔴 Not Started

**Stubs to Replace:**
1. `src/ui/Viewport3D.tsx` - Demo cube → Real B-Rep model
2. `src/ui/ImportDialog.tsx` - Mock groups → Real STEP/DWF parsing
3. `src/drawing_editor.cpp` - Stub → Real 2D drawing gen
4. `src/fea/` - Pseudo math → Real FEA solver
5. `src/generative/` - Stubs → Real topology optimization

**Dependencies:**
- OCCT must be fully integrated (Stage 2)
- C++ bindings to WASM (emscripten)
- WebAssembly infrastructure working (Stage 1)

## Current Limitations

| Component | Status | Issue |
|-----------|--------|-------|
| JavaScript/TypeScript | ✅ Working | None |
| Node.js Backend | ✅ Working | None |
| WASM/C++ | 🟡 Ready to setup | Requires emscripten SDK |
| OCCT Integration | 🟡 Configured | Not built locally |
| 3D Viewport | ❌ Placeholder | Shows demo cube |
| CAD Import | ❌ Mock data | Uses mock geometry |
| Sketcher | ⚠️ Configured | PlaneGCS linked but not verified |
| Drawing Export | ❌ Stub | Returns placeholder |

## Quick Start for Developer

```bash
# Stage 1: Get JS/TS working (already done)
cd /home/user/bendcore
npm test              # Should show 1217 PASS
npm run build         # Should succeed

# Stage 2: Setup WASM (3-4 hours)
./scripts/setup_wasm.sh
source ~/.bashrc
./scripts/build_wasm.sh

# Stage 3: Implement real features
# (Requires WASM to work first)
# See individual C++ files for placeholders marked with TODO
```

## Architecture Notes

**Current:**
- React UI + Node.js API (100% working)
- WASM/C++ layer (infrastructure ready)
- Placeholder implementations (many stubs)

**After Completion:**
- React UI ← Node.js API ← WASM ← C++
- Real 3D geometry from OCCT
- Real CAD file parsing
- Real FEA calculations
- Real drawing generation

## Known Issues

1. **3D Viewport** - Shows demo cube, not actual model
2. **Import Dialog** - Uses hardcoded mock data
3. **Sketcher** - Configured but untested
4. **C++ Builds** - Require emscripten and OCCT setup
5. **OCCT Integration** - Partial, needs completion

## Next Steps (Priority Order)

0. **Add a native C++ CI job** (build + ctest). None exists today — only
   WASM/OCCT — which is how the broken GTest suites went unnoticed.
   It would be red until `CFDSolver` is real.
1. **Get WASM working locally** (use setup_wasm.sh)
2. **Build C++ with OCCT** (Stage 2)
3. **Replace placeholder 3D viewport** with real model
4. **Implement real CAD import** (STEP/DWF)
5. **Add real FEA solver**
6. **Complete sketcher integration**

## Files to Review

```
Critical:
- docs/WASM_BUILD_GUIDE.md
- docs/IMPLEMENTATION_STATUS.md (this file)
- scripts/setup_wasm.sh
- scripts/build_wasm.sh

Infrastructure:
- CMakeLists.txt
- CMakePresets.json
- .github/workflows/wasm.yml

To Replace (stubs):
- src/ui/Viewport3D.tsx:28 (demo cube)
- src/ui/ImportDialog.tsx:50 (mock groups)
- src/drawing_editor.cpp:200+ (stub functions)
- src/fea/*.cpp (pseudo-code)
- src/generative/*.cpp (placeholders)
```

---

**This is an honest assessment, not a marketing document.**

Completed work is solid. Remaining work is substantial but clear.
