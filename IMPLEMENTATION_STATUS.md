# BendCore Implementation Status

**Last Updated:** 2026-11-01  
**Status:** Native C++ + OCCT working; see limitations

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

### Stage 2a: Native C++ build — ✅ (2026-11-01)

The native build never completed before: `third_party/eigen-5.0.0` is a 24 KB
stub, not Eigen, and with no GTest every GoogleTest suite was silently skipped
(they had never compiled).

```bash
sudo apt install libeigen3-dev libgtest-dev
cmake --preset host && cmake --build build -j
ctest --test-dir build -j4                    # 74/74 PASS
```

CI: `.github/workflows/native-cpp.yml` — Release (gcc) and RelWithDebInfo with
ASan/UBSan; both 74/74 locally, sanitizers clean.

### Stage 2b: OCCT integration — ✅ (2026-11-01)

```bash
(cd third_party && ./build_occt.sh)           # OCCT 8.0.1, static, ~1-2 h
cmake -S . -B build-occt -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DBENDCORE_WITH_OCCT=ON -DBENDCORE_OCCT_DIR=$PWD/third_party/occt-install
cmake --build build-occt -j && ctest --test-dir build-occt   # 84/84 PASS
build-occt/bendcore_server 8080               # UI default service address
```

Verified over HTTP: `/v1/health`; `/v1/tube` (body + mesh); `/v1/analyze` on a
generated two-bend STEP tube returns OD 20, wall 2, bends 90°/R50 and 45°/R60,
axis length 345.664 mm (analytic 345.664), mesh 2392 vertices; junk → 400.
The UI (`ImportDialog` → `tubeService.analyzeStep`) reads exactly these fields.

`e2e_import` runs the whole import pipeline: tube → STEP → `STEPReader` (XCAF)
→ B-Rep analysis → parametric formulas → `ImportWorkflow` cache miss/hit.

Real defects found and fixed along the way:

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

| `ImportWorkflow` | analysed the shape *ID* instead of its B-Rep; cached zero dimensions on failed analysis |
| `STEPReader` | Windows-only, and even there stored `"placeholder_shape_data"`; now XCAF on all platforms |
| `bendcore::cache::GeometryCache` | header without implementation; now LRU, 100 MB, thread-safe |
| `CFDSolver` | placeholder pseudo-physics that diverged; now steady upwind/graph solver + k-ε + PPE (limits below) |
| document serialisers | misaligned `uint32/64` stores (UB, UBSan) |
| `build_occt.sh` / CI | 404 URL, shared libs instead of static, no DataExchange, wrong install path |

## Known Limitations (honest)

| Area | State |
|---|---|
| **Excluded tests** | **56 test targets** are skipped via `BENDCORE_UNVERIFIED_TARGETS` (do not compile / no implementation). "74/74" is 74 of 130. |
| `src/security/encryption_support.cpp` (GAP-084) | **Fake**: `hashSHA256` returns `"sha256_"+data` (passwords in plaintext), random bytes are zeros, `encryptFile` / TLS validation return `true`. Not linked anywhere — must not be used as is. |
| `CFDSolver` | no pressure–velocity coupling (continuity not enforced); pressure only approximate on point clouds; SA / k-ω SST fall back to k-ε |
| WASM | build scripts exist, not run in this environment (needs emsdk) |
| DXF reader, parametric builder | compile and pass their tests; tests are shallow |

## Next Steps (Priority Order)

1. Triage the 56 excluded test targets; remove or rewrite the fake security module
2. Run the WASM build with emsdk (`scripts/build_wasm.sh`)
3. Pressure–velocity coupling (SIMPLE/PISO on a proper FV mesh) if CFD results are needed quantitatively
4. Strengthen shallow tests (DXF, parametric builder, STEP reader with real files)

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
