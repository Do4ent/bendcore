# BendCore Testing - Quick Reference Card

**Print this and keep at your desk during testing**

---

## 🔗 KEY LINKS

| Resource | Link |
|----------|------|
| PR #6 | https://github.com/Do4ent/bendcore/pull/6 |
| GitHub Actions | https://github.com/Do4ent/bendcore/actions |
| Windows Workflow | Windows Build & Release workflow |
| Detailed Steps | `/docs/audit/PART_A_WINDOWS_DETAILED_STEPS.md` |
| Result Template | `/docs/audit/WINDOWS_BUILD_TEST_RESULTS_TEMPLATE.md` |
| CAD Tests | `/tests/cad-files/` (test_simple_tube.bdc, etc.) |

---

## ⏱️ TIMELINE OVERVIEW

```
Phase 2: Windows Build (1-2 weeks)
├─ A1-A2: Workflow & Artifacts (2 hours)
├─ A3: NSIS Installer (1-2 hours)
├─ A4: App Launch (1 hour)
├─ A5: Uninstall (30 min)
├─ A6: Portable .exe (1 hour)
└─ A7: Documentation (1-2 hours)
   TOTAL: 8-10 hours

Phase 3: CAD Testing (2-3 weeks)
├─ B1: Prepare test files
├─ B2-B8: Run test scenarios
└─ B9: Document results
   TOTAL: 4-6 hours per file
```

---

## 🎯 SECTION A: WINDOWS BUILD

### A1 - Monitor Workflow (30 min - 1 hour)

**Checklist**:
- [ ] PR #6 merged to main
- [ ] Workflow triggered automatically
- [ ] Monitor each build step
- [ ] All steps pass ✓
- [ ] Final status: GREEN ✓

**Key indicators**:
- npm ci: "added 970 packages"
- npm test: "Test Suites: 51 passed, 1217 tests"
- npm typecheck: No errors
- npm build: "compiled with 3 warnings"
- npm dist: "Building complete"

---

### A2 - Artifacts (30 min)

**Download**:
1. GitHub Actions → build-windows job
2. Artifacts section → windows-builds.zip
3. Extract to C:\temp\bendcore-builds

**Verify files exist**:
- [ ] BendCore-CAD-1.0.0-PRODUCTION-x64.exe (NSIS, 150+ MB)
- [ ] BendCore-CAD-1.0.0-PRODUCTION-x64.exe (portable, 150+ MB)
- [ ] latest-x64.yml
- [ ] checksums.txt

**Check integrity**:
```powershell
cd C:\temp\bendcore-builds
ls -la *.exe  # Verify sizes > 100 MB

# Verify hash:
$hash = Get-FileHash "BendCore*.exe" -Algorithm SHA256
$hash.Hash  # Compare with checksums.txt
```

---

### A3 - NSIS Installer (1-2 hours)

**Steps**:
1. Double-click installer
2. Accept SmartScreen warning (if appears)
3. Welcome → Click "Next >"
4. Select directory: C:\Program Files\BendCore CAD
5. Click "Install"
6. Wait for completion
7. Click "Finish"

**Verify installation**:
- [ ] Folder C:\Program Files\BendCore CAD created
- [ ] Desktop shortcut created
- [ ] Start Menu entry created
- [ ] Files exist (150+ items)

---

### A4 - App Launch (1 hour)

**Launch**:
1. Double-click Desktop shortcut
2. Note start time (target: < 5 sec)

**Verify UI**:
- [ ] Title bar: "BendCore CAD 1.0.0-PRODUCTION"
- [ ] Menu bar (File, Edit, View, Help)
- [ ] Toolbar visible
- [ ] Feature tree (left)
- [ ] 3D viewport (center, black with grid)
- [ ] Properties panel (right)
- [ ] Status bar (bottom)

**Test interactions**:
- [ ] Middle-drag: Viewport rotates
- [ ] Scroll: Zoom works
- [ ] Right-drag: Pan works
- [ ] Responsive: No lag

**Check console** (F12):
- [ ] No RED errors
- [ ] Warnings OK

---

### A5 - Uninstall (30 min)

**Uninstall**:
1. Control Panel → Programs
2. Find "BendCore CAD"
3. Click "Uninstall"
4. Confirm

**Verify cleanup**:
```powershell
Test-Path "C:\Program Files\BendCore CAD"     # Should be: False
Test-Path "C:\Users\$env:USERNAME\Desktop\BendCore CAD.lnk"  # False
Get-ChildItem "HKCU:\Software" | grep BendCore  # Should be empty
```

---

### A6 - Portable .exe (1 hour)

**Setup**:
1. Create: C:\temp\bendcore-portable
2. Copy: portable BendCore-CAD-1.0.0-x64.exe
3. Double-click to launch

**Verify**:
- [ ] No installer dialog
- [ ] App launches directly
- [ ] All features work
- [ ] No AppData artifacts

---

### A7 - Documentation (1-2 hours)

**Complete template**:
- [ ] Test metadata (date, tester, OS)
- [ ] A1-A6 results (PASS/FAIL)
- [ ] Issues found (if any)
- [ ] Screenshots (for issues)
- [ ] Sign-off (tester + QA)

**Save as**: WINDOWS_BUILD_TEST_RESULTS_[DATE].md

---

## 🎯 SECTION B: CAD TESTING

### Test Files Location

```
/tests/cad-files/
├─ test_simple_tube.bdc
├─ test_multi_bend.bdc
└─ test_complex_parametric.bdc
```

### B2-B4: Import & Geometry

**Simple Tube**:
- [ ] File imports (no errors)
- [ ] Length: 12.00"
- [ ] Weight: ~1.456 lbs
- [ ] Save/reload preserves

**Multi-Bend**:
- [ ] 5 segments visible
- [ ] 2 bends at 90°
- [ ] Radius: 0.5" each
- [ ] Continuity OK
- [ ] Length: 30.566" ± 0.05"

**Parametric**:
- [ ] Parameters visible
- [ ] Formulas load
- [ ] Change baseLength → total length updates
- [ ] Change bendMultiplier → recalculates

### B5: Undo/Redo

- [ ] Edit property
- [ ] Undo (Ctrl+Z) → Reverts ✓
- [ ] Redo (Ctrl+Y) → Reapplies ✓
- [ ] No geometry corruption

### B6: Constraints

- [ ] Change parameter
- [ ] All dependent values update
- [ ] Calculations correct
- [ ] Geometry updates

### B9: Documentation

**Complete CAD template**:
- [ ] File metadata
- [ ] Each test results (PASS/FAIL)
- [ ] Geometry findings
- [ ] Performance metrics
- [ ] Issues/blockers
- [ ] Sign-off

---

## ⚠️ COMMON ISSUES

| Issue | Solution |
|-------|----------|
| Antivirus blocks installer | Disable real-time scanning temporarily |
| "No space on device" | Need 500+ MB free disk |
| App doesn't start | Check installation folder exists, try as Admin |
| Console shows errors | Document error, check logs |
| Geometry looks wrong | Take screenshot, note discrepancy |
| Performance slow | Note timings, check system load |

---

## 📞 ESCALATION CONTACTS

| Role | Contact |
|------|---------|
| QA Lead | ___________________ |
| DevOps | ___________________ |
| Release Manager | ___________________ |
| CAD Engineer | ___________________ |

---

## ✅ SUCCESS DEFINITION

**Windows Build PASS** when:
- ✓ Workflow completes
- ✓ Artifacts generated
- ✓ Installer works
- ✓ App launches clean
- ✓ No console errors
- ✓ Uninstall clean
- ✓ Portable independent
- ✓ All documented

**CAD Testing PASS** when:
- ✓ All files import
- ✓ Geometry valid
- ✓ Save/reload works
- ✓ Undo/redo works
- ✓ Parameters update
- ✓ Performance OK
- ✓ No corruption
- ✓ All documented

---

## 📋 QUICK COMMANDS

### PowerShell

```powershell
# Check folder exists
Test-Path "C:\Program Files\BendCore CAD"

# Verify file hash
$hash = Get-FileHash "file.exe" -Algorithm SHA256
$hash.Hash

# List files
dir "C:\Program Files\BendCore CAD"

# Check registry
Get-ChildItem "HKCU:\Software" | Where-Object { $_.Name -like "*BendCore*" }
```

### File Locations

```
Installation: C:\Program Files\BendCore CAD
Portable: C:\temp\bendcore-portable\BendCore-CAD-1.0.0-x64.exe
Downloads: C:\temp\bendcore-builds\
Test results: docs/audit/WINDOWS_BUILD_TEST_RESULTS_[DATE].md
Test files: tests/cad-files/
```

---

## 📈 PROGRESS TRACKING

### Windows Phase (A1-A7)

Days to completion: ________

Current status:
- A1: ☐ TODO ☐ IN PROGRESS ☐ DONE
- A2: ☐ TODO ☐ IN PROGRESS ☐ DONE
- A3: ☐ TODO ☐ IN PROGRESS ☐ DONE
- A4: ☐ TODO ☐ IN PROGRESS ☐ DONE
- A5: ☐ TODO ☐ IN PROGRESS ☐ DONE
- A6: ☐ TODO ☐ IN PROGRESS ☐ DONE
- A7: ☐ TODO ☐ IN PROGRESS ☐ DONE

### CAD Phase (B1-B9)

Days to completion: ________

Current status:
- B1: ☐ TODO ☐ IN PROGRESS ☐ DONE
- B2-B4: ☐ TODO ☐ IN PROGRESS ☐ DONE
- B5-B6: ☐ TODO ☐ IN PROGRESS ☐ DONE
- B7-B8: ☐ TODO ☐ IN PROGRESS ☐ DONE
- B9: ☐ TODO ☐ IN PROGRESS ☐ DONE

---

**Print & Keep Handy During Testing** 🎯
