# Windows Build & CAD Testing Checklist

**Created**: October 31, 2026  
**Purpose**: Actionable steps to verify Windows build and CAD file handling  
**Priority**: P1 (Required before production release)

---

## PART A: Windows Build Verification (Phase 2)

### Prerequisites
- [ ] PR #6 merged to main
- [ ] GitHub Actions workflow triggered on Windows runner
- [ ] Build completes successfully

### A1. Monitor Windows Build Job

**Location**: https://github.com/Do4ent/bendcore/actions

**Steps**:
1. [ ] After merge, trigger Windows "Build & Release" workflow
   - Branch: main
   - Wait for `build-windows` job to start
   
2. [ ] Monitor job progress
   - Check: `npm ci --no-audit --no-fund` → success
   - Check: `npm test -- --ci --runInBand` → all tests pass
   - Check: `npm run typecheck` → main, server, api pass
   - Check: `npm run build` → webpack succeeds
   - Check: `npm run dist` → electron-builder succeeds
   
3. [ ] Verify artifacts uploaded
   - Check artifact: `windows-builds`
     - Should contain: *.exe, *.msi, *.yml files
     - Should NOT be empty
   - Check artifact: `windows-checksums`
     - Should contain: checksums.txt
     - Should have SHA256 hashes for .exe and .msi

### A2. Download and Inspect Artifacts

**From GitHub Actions**:

1. [ ] Download `windows-builds` artifact
   ```
   Filename patterns to expect:
   - BendCore-CAD-1.0.0-PRODUCTION-x64.exe (NSIS installer)
   - BendCore-CAD-1.0.0-PRODUCTION-x64.exe (portable)
   - latest-x64.yml (update manifest)
   ```

2. [ ] Download `windows-checksums` artifact
   ```
   Contents should show:
   sha256(installer.exe) = ...
   sha256(portable.exe) = ...
   ```

3. [ ] Verify file properties
   - [ ] NSIS installer exists and > 100 MB
   - [ ] Portable executable exists and > 100 MB
   - [ ] File signatures verified (not corrupted)

### A3. Test NSIS Installer on Windows Machine

**Environment**: Windows 10/11 (Local, VM, or CI Windows runner)

**Prerequisites**:
- Downloaded `BendCore-CAD-1.0.0-PRODUCTION-x64.exe` (NSIS installer)
- Admin privileges
- Clean system (or VM snapshot)

**Installation Test**:

```
Step 1: Launch installer
  [ ] Double-click downloaded .exe file
  [ ] Installer window appears without errors
  [ ] No SmartScreen warnings (expected - unsigned)

Step 2: Select installation directory
  [ ] "Choose Installation Folder" dialog appears
  [ ] Can browse and select custom directory
  [ ] "Program Files\BendCore CAD" shown as default

Step 3: Complete installation
  [ ] Installer progress bar shows
  [ ] All components install successfully
  [ ] No error messages during installation
  [ ] Installation completes in < 2 minutes

Step 4: Verify installation artifacts
  [ ] Shortcut created on Desktop (BendCore CAD)
  [ ] Shortcut created in Start Menu
  [ ] Files exist in Program Files\BendCore CAD:
      - Folder: resources
      - Folder: node_modules
      - Files: *.exe, config files
```

**Success Criteria**: All checks pass, installation silent and clean

### A4. Test Application Launch

**Prerequisites**: NSIS installer successfully installed

**Application Launch Test**:

```
Step 1: Launch from Desktop shortcut
  [ ] Double-click Desktop shortcut
  [ ] Application starts (< 5 seconds)
  [ ] Main window opens
  [ ] Title bar shows "BendCore CAD"
  [ ] No error popups

Step 2: Verify 3D viewport
  [ ] 3D rendering visible (black canvas with grid)
  [ ] Viewport responds to mouse (can orbit/pan)
  [ ] Menu bar fully functional
  [ ] Toolbar visible

Step 3: Test basic functionality
  [ ] Can access File → New
  [ ] Can create new design (no errors)
  [ ] Can see feature tree on left
  [ ] Can see property panel on right
  [ ] Status bar at bottom shows status

Step 4: Verify no console errors
  [ ] Dev Tools (F12) → Console shows no errors
  [ ] Only warnings acceptable (deprecated APIs, etc)
```

**Success Criteria**: Application launches cleanly, all UI visible, no errors

### A5. Test Uninstall

**Prerequisites**: Application successfully tested

**Uninstall Test**:

```
Step 1: Uninstall via Control Panel
  [ ] Control Panel → Programs → Programs and Features
  [ ] Find "BendCore CAD" in list
  [ ] Click Uninstall
  
Step 2: Complete uninstall
  [ ] Uninstaller dialog appears
  [ ] Uninstall completes successfully
  [ ] No errors during uninstall

Step 3: Verify cleanup
  [ ] Program Files\BendCore CAD deleted
  [ ] Desktop shortcut removed
  [ ] Start Menu shortcut removed
  [ ] No registry entries for BendCore CAD remain
  [ ] Application cannot be launched (shortcut not found)
```

**Success Criteria**: Complete removal, no orphaned files

### A6. Test Portable Executable

**Prerequisites**: Download portable .exe from artifacts

**Portable Execution**:

```
Step 1: Extract/run portable
  [ ] Copy BendCore-CAD-1.0.0-PRODUCTION-x64.exe to any directory
  [ ] Double-click to run
  [ ] Application launches without installation
  
Step 2: Verify portable operation
  [ ] Application runs fully
  [ ] All features functional (same as installed version)
  [ ] No dependencies on Program Files
  
Step 3: Cleanup
  [ ] Delete .exe file
  [ ] No artifacts left on system
```

**Success Criteria**: Portable executable works independently

### A7. Document Results

**Create file**: `docs/audit/WINDOWS_BUILD_VERIFICATION_RESULTS.md`

**Contents**:
```markdown
# Windows Build Verification - Test Results

Date: [date]
Tester: [name]
Windows Version: [e.g., Windows 10 22H2]
Test Environment: [Local/VM/CI]

## Artifact Check
- [ ] NSIS installer exists
- [ ] Portable .exe exists
- [ ] File sizes reasonable (>100 MB)
- [ ] Checksums verified

## NSIS Installer Test
- [ ] Installation successful
- [ ] Shortcuts created
- [ ] No errors during install
- [ ] Uninstall clean

## Application Launch Test
- [ ] Application starts < 5 seconds
- [ ] 3D viewport renders
- [ ] Menu/toolbar functional
- [ ] No console errors

## Portable .exe Test
- [ ] Launches without installation
- [ ] All features work
- [ ] Leaves no artifacts

## Overall Status
PASS / FAIL with notes

## Blockers/Issues
[Document any failures]
```

---

## PART B: CAD File Testing (Phase 3)

### Prerequisites
- [ ] E2E tests fixed and running
- [ ] Real CAD test files available
- [ ] Test scenarios documented

### B1. Prepare Test CAD Files

**Three scenarios required**:

**File 1: Simple Tube** (`test_simple_tube.bdc`)
- [ ] Single straight tube, 12" length, 1" diameter
- [ ] Material: Steel
- [ ] Expected properties:
  - Centerline length: 12"
  - Weight: calculated
  - No bends
  
**File 2: Multi-Bend Assembly** (`test_multi_bend.bdc`)
- [ ] 3+ segments with bends
- [ ] Different materials
- [ ] Expected properties:
  - Total centerline length: sum of segments
  - Bend radii preserved
  - All angles calculated correctly
  
**File 3: Complex Parametric** (`test_complex_parametric.bdc`)
- [ ] Design with formulas
- [ ] Parametric relationships
- [ ] Expected behavior:
  - Dimensions update on parameter change
  - Undo/redo maintains consistency
  - Tree updates correctly

### B2. Test Scenario: File Import

**Steps for each test file**:

```
Test: Import CAD file
File: [test_simple_tube.bdc]

1. [ ] File → Open [filename]
2. [ ] Dialog shows file selector
3. [ ] File selected and opened
4. [ ] No import errors
5. [ ] Design appears in 3D viewport
6. [ ] Feature tree shows all components
7. [ ] Properties panel shows correct values
8. [ ] Geometry matches expected result

Document: Import successful / failed, any errors, visual inspection
```

### B3. Test Scenario: Geometry Validation

**Verify geometric correctness**:

```
Test: Validate geometry after import
File: [test_simple_tube.bdc]

Checks:
[ ] Centerline continuity (no breaks)
[ ] Bend angles match specifications
[ ] Bend radii within tolerance
[ ] Material properties preserved
[ ] Coordinate system correct
[ ] No degenerate geometry

Tools: 
  - Visual inspection in 3D viewport
  - Dimension tools (if available)
  - Property panel values
```

### B4. Test Scenario: Edit and Save/Reload

```
Test: Modify design and verify persistence
File: [test_multi_bend.bdc]

Steps:
1. [ ] Load file
2. [ ] Edit property (e.g., diameter)
3. [ ] Observe tree updates
4. [ ] Observe 3D viewport updates
5. [ ] Save file (File → Save)
6. [ ] Close design
7. [ ] Re-open same file
8. [ ] Verify changes persisted
9. [ ] Dimensions match modified values
```

**Success Criteria**: Changes persist across save/reload cycle

### B5. Test Scenario: Undo/Redo Consistency

```
Test: Undo/redo on real CAD file
File: [test_complex_parametric.bdc]

Steps:
1. [ ] Load file
2. [ ] Make 5+ modifications:
        - Edit parameter
        - Change property
        - Delete feature
        - Add feature
        - Modify dimension
3. [ ] Use Undo 5 times
        [ ] Each undo reverses change correctly
        [ ] Tree updates
        [ ] Viewport updates
4. [ ] Use Redo 5 times
        [ ] Changes reappear in correct order
        [ ] Final state matches step 2
```

**Success Criteria**: All undo/redo operations work correctly

### B6. Test Scenario: Constraint Propagation

```
Test: Parametric updates (if design uses formulas)
File: [test_complex_parametric.bdc]

Steps:
1. [ ] Identify formula-driven dimension
2. [ ] Identify dependent dimensions
3. [ ] Change parameter value
4. [ ] Observe all dependent values update
5. [ ] Verify calculations correct
6. [ ] Check 3D geometry reflects changes

Tools: Property panel, dimension display
```

**Success Criteria**: Formulas propagate correctly, geometry updates

### B7. Test Scenario: Export (if supported)

```
Test: Export to other formats
File: [test_simple_tube.bdc]

If export supported:
1. [ ] File → Export → [format]
2. [ ] Select output file location
3. [ ] File exports successfully
4. [ ] Output file created with expected size
5. [ ] Can re-import exported file
6. [ ] Re-imported geometry matches original

Note: Only if export is documented feature
```

### B8. Performance Test (Large Design)

```
Test: Performance on large design
File: [test_large_design.bdc] (if available)

Measure:
[ ] Load time (goal: < 5 seconds for 10K features)
[ ] Memory usage (goal: < 1 GB)
[ ] UI responsiveness during load
[ ] Viewport responsiveness when zoomed
[ ] Feature tree rendering complete

Document:
- Load time (seconds)
- Memory usage (MB)
- Any UI lag/delays
- Performance acceptable: Yes/No
```

### B9. Document CAD Test Results

**Create file**: `docs/audit/CAD_FILE_TEST_RESULTS.md`

**Contents**:

```markdown
# CAD File Testing - Results

Date: [date]
Tester: [name]
Build Version: [version]

## Test Files Used
- [ ] test_simple_tube.bdc
- [ ] test_multi_bend.bdc
- [ ] test_complex_parametric.bdc

## Test Results Summary

### Test 1: Import
File: test_simple_tube.bdc
Status: PASS / FAIL
Issues: [none or description]

### Test 2: Geometry Validation
File: test_simple_tube.bdc
Centerline: OK / Issue
Bends: OK / Issue
Status: PASS / FAIL

### Test 3: Edit/Save/Reload
File: test_multi_bend.bdc
Status: PASS / FAIL
Issues: [none or description]

### Test 4: Undo/Redo
File: test_complex_parametric.bdc
Status: PASS / FAIL
Issues: [none or description]

### Test 5: Constraints
File: test_complex_parametric.bdc
Formula updates: OK / Issue
Status: PASS / FAIL

### Test 6: Performance (if tested)
File: test_large_design.bdc
Load time: [X] seconds
Memory: [Y] MB
Status: PASS / FAIL

## Overall CAD Testing Status
PASS (All scenarios work correctly)
FAIL (Issues found - see details)

## Blockers/Known Issues
[Document any failures or limitations]

## Recommended Actions
[Next steps if issues found]
```

---

## EXECUTION TIMELINE

| Step | Task | Timeline | Owner | Status |
|------|------|----------|-------|--------|
| A1 | Monitor Windows build | Day 1 | CI/DevOps | ⏳ Waiting |
| A2 | Download artifacts | Day 1 | QA | ⏳ Waiting |
| A3 | Test NSIS installer | Day 2-3 | QA | ⏳ Waiting |
| A4 | Test application | Day 2-3 | QA | ⏳ Waiting |
| A5 | Test uninstall | Day 3 | QA | ⏳ Waiting |
| A6 | Test portable .exe | Day 3 | QA | ⏳ Waiting |
| A7 | Document results | Day 4 | QA | ⏳ Waiting |
| B1 | Prepare CAD files | Day 4-5 | CAD Eng | ⏳ Waiting |
| B2-B8 | CAD test scenarios | Day 5-7 | QA + CAD Eng | ⏳ Waiting |
| B9 | Document results | Day 8 | QA | ⏳ Waiting |

---

## SUCCESS CRITERIA

### Windows Build: PASS when
- ✓ Artifacts generated (NSIS + portable)
- ✓ NSIS installer completes successfully
- ✓ Application launches and runs
- ✓ Uninstall is clean
- ✓ Portable .exe works independently
- ✓ No SmartScreen warnings (expected pre-signing)
- ✓ All functionality accessible

### CAD Testing: PASS when
- ✓ All 3 test files import successfully
- ✓ Geometry validated (continuity, angles, radii)
- ✓ Save/reload cycle preserves design
- ✓ Undo/redo works correctly
- ✓ Parametric formulas propagate
- ✓ Performance acceptable (< 5s for 10K features)
- ✓ No geometry corruption on any operation

### Overall: READY FOR PRODUCTION when
- ✓ Windows tests: PASS
- ✓ CAD tests: PASS
- ✓ No blockers remaining
- ✓ Documentation complete
- ✓ Team sign-off

---

## NOTES

- This checklist is **NOT** a substitute for comprehensive testing
- Real customer CAD files should also be tested when available
- Performance baselines should be established before optimization
- All test results must be documented for audit trail
- Any failures require root cause analysis and fix before release

---

**Created by**: Claude Code (Independent Audit)  
**Version**: 1.0  
**Last Updated**: October 31, 2026
