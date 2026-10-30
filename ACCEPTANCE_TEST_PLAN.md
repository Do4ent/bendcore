# BendCore v1.0.0-PRODUCTION: Acceptance Test Plan

**Version:** 1.0  
**Date:** 2026-10-30  
**Status:** ACTIVE EXECUTION  
**Test Coverage:** 10 Critical User Workflows

---

## Executive Summary

This document defines the acceptance test strategy for BendCore v1.0.0-PRODUCTION, covering 10 critical integration test workflows that verify the system's readiness for production launch (March 2027).

**Objectives:**
- Verify all critical CAD workflows execute correctly
- Ensure data integrity and persistence
- Validate undo/redo functionality
- Confirm performance targets are met
- Identify any blocking issues before production launch

**Success Criteria:**
- ✅ All 10 workflows pass 100%
- ✅ Performance targets met (FPS, load time, export time)
- ✅ Zero data corruption issues
- ✅ Zero crashes or stability issues
- ✅ All features work as documented

---

## 10 Acceptance Test Workflows

### ACCEPT-001: Sketch → Extrude → Hole → Fillet Workflow

**Objective:** Verify basic parametric modeling workflow

**Steps:**
1. Create 2D sketch: rectangle (100x50mm), circle (10mm diameter)
2. Extrude sketch to 3D (Pad feature, 20mm height)
3. Add hole using Circle + Pocket (5mm diameter)
4. Apply Fillet to edges (2mm radius)
5. Verify geometry is correct
6. Test undo/redo works
7. Save/reopen model and verify preservation

**Success Criteria:**
- Geometry volumes match expected values (within 1% tolerance)
- Undo/redo maintains model consistency
- Model preserves all features after save/reopen
- Total workflow time < 30 seconds

**Expected Metrics:**
- Rectangle volume: 5,000 mm³
- With pad (20mm): 100,000 mm³
- With hole (-5mm diameter): 99,100 mm³ (approx)
- With fillet: geometry refined, volume slightly reduced

---

### ACCEPT-002: Assembly Constraint Workflow

**Objective:** Verify assembly constraint system

**Steps:**
1. Create 2-part assembly: body + mounting plate
2. Apply Coincident mate: surfaces flush
3. Apply Distance mate: 10mm offset
4. Verify no collisions (AssemblyValidation)
5. Save and reopen assembly

**Success Criteria:**
- Constraints are satisfied
- No false collision positives
- Visualization updates correctly
- Assembly saved/reopened correctly

---

### ACCEPT-003: Assembly Structural Analysis

**Objective:** Verify collision detection and assembly validation

**Steps:**
1. Create 5-part assembly with multiple constraint types
2. Run collision detection (AssemblyValidation)
3. Identify interference zones
4. Verify no false positives
5. Measure performance

**Success Criteria:**
- Collision detection accurate (<2 seconds for 5-part assembly)
- No false positives
- Visualization highlights collisions correctly

---

### ACCEPT-004: Sheet Metal Unfolding

**Objective:** Verify sheet metal feature and unfolding

**Steps:**
1. Create bent sheet metal: 90° bend, 2mm radius, 1mm thickness
2. Unfold to 2D flat pattern
3. Verify dimensions match original geometry
4. Verify flat pattern can be re-folded
5. Check precision error <0.1%

**Success Criteria:**
- Flat pattern dimensions correct
- Re-fold matches original geometry
- Precision error <0.1%

---

### ACCEPT-005: STEP Import/Export Round-Trip

**Objective:** Verify CAD interoperability

**Steps:**
1. Import STEP file (50+ parts SolidWorks assembly)
2. Verify geometry integrity (edge/face count, volume)
3. Modify model (add feature, change dimension)
4. Export to STEP
5. Re-import and compare with modified model
6. Verify no data loss

**Success Criteria:**
- Geometry preserved during import/export
- Feature tree preserved
- Dimensions maintained
- Volume calculation correct

---

### ACCEPT-006: DWG/DXF Import & Drawing Generation

**Objective:** Verify 2D import and technical drawing generation

**Steps:**
1. Import 2D DWG drawing (architectural layout)
2. Convert to sketch, create 3D extrusion
3. Generate technical drawing: Top, Front, Right, Isometric views
4. Export to PDF
5. Compare with reference

**Success Criteria:**
- DWG import successful
- Views are correctly oriented
- Dimensions are readable
- PDF quality acceptable

---

### ACCEPT-007: DWFx Markup & Collaboration

**Objective:** Verify markup and annotation system

**Steps:**
1. Generate DWFx from 3D model
2. Add markup: comments, dimensions, revision clouds
3. Verify markup persists on reload
4. Export annotated DWFx
5. Verify file size reasonable

**Success Criteria:**
- Markup saved and persists
- DWFx file format valid
- File size <50MB for typical model

---

### ACCEPT-008: Undo/Redo & Command History

**Objective:** Verify undo/redo system robustness

**Steps:**
1. Perform 20-step workflow: sketches, extrusions, mates, modifications
2. Undo 10 steps, verify model state
3. Redo 5 steps, verify consistency
4. Create branch in history (edit earlier step)
5. Verify no data corruption

**Success Criteria:**
- Undo/redo <200ms per operation
- History tree accurate
- No data corruption
- Branching creates alternative paths

---

### ACCEPT-009: Save/Reopen Cycle & Data Persistence

**Objective:** Verify file persistence and data integrity

**Steps:**
1. Create complex assembly: 100 features, 20 sketches, 15 parts, 40 constraints
2. Add custom properties and manufacturing notes
3. Save to .bcad
4. Reopen in new session
5. Verify all data intact

**Success Criteria:**
- All features/parameters/properties preserved
- No data loss
- File size <50MB
- Load time <5 seconds

---

### ACCEPT-010: Large Assembly Performance & Stability

**Objective:** Verify scalability and performance

**Steps:**
1. Create assembly: 200+ parts, 500+ features, 200+ constraints
2. Rotate viewport: verify 60 FPS
3. Edit dimension: trigger recompute, verify <2s rebuild
4. Export to STEP: verify <5s export time
5. Verify no crashes, memory <1GB

**Success Criteria:**
- Viewport FPS ≥ 60
- Rebuild time <2 seconds
- Export time <5 seconds
- Memory usage <1GB
- No crashes

---

## Test Environment

**Hardware:**
- CPU: Multi-core x86-64
- RAM: 4GB minimum (8GB recommended)
- Disk: 100MB free space for test data

**Software:**
- OS: Linux/Windows/macOS
- CMake 3.20+
- C++20 compiler (GCC, Clang, MSVC)
- Google Test framework

**Dependencies:**
- bendcore_core library
- Eigen (linear algebra)
- CGAL (computational geometry)
- OpenSSL (hashing)
- zlib (compression)

---

## Test Data

Test data includes:
- Sample CAD files (STEP, DWG, DXF)
- Assembly fixture files
- Performance test models
- Reference geometry files

All test data located in `/home/claude/bendcore/tests/samples/`

---

## Test Execution

### Quick Run
```bash
cd /home/claude/bendcore
./scripts/run_acceptance_tests.sh
```

### Individual Tests
```bash
cd /home/claude/bendcore/build_acceptance_tests
./ACCEPT-001-SketchExtrude
./ACCEPT-002-AssemblyConstraints
./ACCEPT-008-UndoRedo
```

### Benchmarks
```bash
./acceptance_benchmarks
```

---

## Reporting

Reports generated:
- `ACCEPT-REPORT.md` - Markdown report with detailed results
- `ACCEPT-REPORT.html` - HTML report for viewing
- `BENCHMARK_RESULTS.json` - Performance metrics
- Build logs and individual test logs

---

## Sign-Off Criteria

**PRODUCTION READY:** ✅ All 10 workflows pass
- Pass rate ≥ 99.5%
- All performance targets met
- Zero data corruption issues
- Zero crashes or stability issues

**REQUIRES FIXES:** ❌ Any workflow fails
- Identify root cause
- Implement fix
- Re-run acceptance tests

---

## Timeline

- **Phase Start:** 2026-10-30
- **Test Execution:** 2026-10-30 to 2026-11-03 (14 hours)
- **Issue Resolution:** 2026-11-03 to 2026-11-10
- **Final Sign-Off:** 2026-11-10
- **Target Launch:** 2026-11-26

---

## Automation

All tests are automated and can be run without manual intervention. CI/CD pipeline triggers acceptance tests on every commit.

---

## Notes

- All timings are measured with Release build (-O3 optimizations)
- Tests use deterministic seed for reproducibility
- Memory profiling uses Valgrind on Linux
- Performance targets are conservative (20-30% safety margin)

