# Windows Build Testing - Execution Guide

**Quick Start Guide for Phase 2: Windows Build Verification**

---

## 🎯 GOAL

Verify that BendCore Windows build (.exe installer and portable) works correctly on actual Windows machine.

**Timeline**: 1-2 weeks  
**Total effort**: 8-10 hours  
**Success criteria**: All 6 sections (A1-A7) PASS

---

## 📋 PRE-EXECUTION CHECKLIST

Before you start, verify you have:

- [ ] PR #6 merged to main
- [ ] Windows 10/11 machine available (local, VM, or CI)
- [ ] Admin privileges on test machine
- [ ] At least 500 MB free disk space
- [ ] Antivirus software (optional, can be disabled)
- [ ] Internet connection (for artifact download)
- [ ] Text editor for documentation

**Start date**: _______________  
**Tester name**: _______________

---

## ⏱️ TIMELINE ESTIMATE

| Section | Task | Duration | Status |
|---------|------|----------|--------|
| A1 | Monitor workflow | 1-2 hours | ☐ |
| A2 | Download artifacts | 30 min | ☐ |
| A3 | Test NSIS installer | 1-2 hours | ☐ |
| A4 | Test app launch | 1 hour | ☐ |
| A5 | Test uninstall | 30 min | ☐ |
| A6 | Test portable .exe | 1 hour | ☐ |
| A7 | Document results | 1-2 hours | ☐ |
| **TOTAL** | | **8-10 hours** | |

---

## 🚀 STEP-BY-STEP EXECUTION

### PHASE 1: PREPARATION (30 minutes)

**Day 1 - Morning**

```
1. [ ] Read: PART_A_WINDOWS_DETAILED_STEPS.md (full procedures)
2. [ ] Read: WINDOWS_BUILD_TEST_RESULTS_TEMPLATE.md (result tracking)
3. [ ] Prepare Windows machine (login, antivirus check)
4. [ ] Create test folder: C:\temp\bendcore-test
5. [ ] Open template file for documentation:
       docs/audit/WINDOWS_BUILD_TEST_RESULTS_[DATE].md
```

---

### PHASE 2: WORKFLOW MONITORING (1-2 hours)

**Day 1 - After PR Merge**

**Section A1: Monitor Windows Build Job**

```
STEP 1: Check GitHub Actions
  URL: https://github.com/Do4ent/bendcore/actions
  
  ✓ Look for "Windows Build & Release" workflow
  ✓ Should show build-windows job in progress or completed
  
STEP 2: If job not started
  ✓ Manually trigger workflow
  ✓ Select main branch
  ✓ Click "Run workflow"

STEP 3: Monitor each step
  Reference: PART_A_WINDOWS_DETAILED_STEPS.md (A1.2)
  
  Watch for:
  ✓ npm ci (install deps) → should show "added 970 packages"
  ✓ npm test (run tests) → should show "Test Suites: 51 passed"
  ✓ npm typecheck → should show no errors (includes API)
  ✓ npm build (webpack) → should show "compiled with 3 warnings"
  ✓ npm dist (electron-builder) → should show "Building complete"
  ✓ Upload artifacts → both should complete

STEP 4: Check final status
  ✓ Job should show green checkmark
  ✓ Duration: approximately 60 minutes
  ✓ No red X marks anywhere

RESULT: Mark A1 as PASS or FAIL in template
```

---

### PHASE 3: ARTIFACT DOWNLOAD & VERIFICATION (30 minutes)

**Day 1 - After Workflow Complete**

**Section A2: Download and Inspect Artifacts**

```
STEP 1: Download artifacts from GitHub Actions
  
  1. GitHub Actions page → build-windows job (completed)
  2. Scroll to "Artifacts" section
  3. Click: "windows-builds" artifact
  4. Browser downloads: windows-builds.zip (~300-400 MB)
  5. Wait for download complete (may take 5-10 min)

STEP 2: Extract and verify
  
  1. Right-click downloaded ZIP
  2. "Extract All" → C:\temp\bendcore-builds
  3. Navigate to: C:\temp\bendcore-builds
  4. Expected files:
     ✓ BendCore-CAD-1.0.0-PRODUCTION-x64.exe (150+ MB, NSIS installer)
     ✓ BendCore-CAD-1.0.0-PRODUCTION-x64.exe (150+ MB, portable)
     ✓ latest-x64.yml (< 1 KB)
     ✓ checksums.txt (< 1 KB)

STEP 3: Verify file integrity
  
  Reference: PART_A_WINDOWS_DETAILED_STEPS.md (A2.3-A2.4)
  
  PowerShell:
  ```powershell
  cd C:\temp\bendcore-builds
  ls -la *.exe
  # Check sizes are > 100 MB
  
  # Verify file is not corrupted:
  $file = "BendCore-CAD-1.0.0-PRODUCTION-x64.exe"
  $hash = Get-FileHash $file -Algorithm SHA256
  $hash.Hash
  # Compare with checksums.txt
  ```

RESULT: Mark A2 as PASS or FAIL in template
```

---

### PHASE 4: NSIS INSTALLER TEST (1-2 hours)

**Day 2 - Morning**

**Section A3: Test NSIS Installer**

```
PRE-TEST: Optional - Disable antivirus (temporarily)
  If prompted by antivirus during installation, disable it
  (or add BendCore to exclusions)

INSTALLATION TEST:

1. [ ] Navigate to: C:\temp\bendcore-builds
2. [ ] Double-click: BendCore-CAD-1.0.0-PRODUCTION-x64.exe
3. [ ] Wait for installation window (should appear in < 5 sec)
4. [ ] Follow installation wizard:
       - Welcome screen → Click "Next >"
       - Directory selection → Select C:\Program Files\BendCore CAD
       - Click "Install"
       - Monitor progress bar (30-60 seconds)
       - Completion screen → Click "Finish"

VERIFICATION:
  [ ] Check: C:\Program Files\BendCore CAD exists
  [ ] Check: Desktop shortcut created (BendCore CAD icon)
  [ ] Check: Start Menu entry created
  
  PowerShell verification:
  ```powershell
  dir "C:\Program Files\BendCore CAD" | measure | Select-Object Count
  # Should show 150+ items
  ```

RESULT: Mark A3 as PASS or FAIL in template
```

---

### PHASE 5: APPLICATION LAUNCH TEST (1 hour)

**Day 2 - After Installation**

**Section A4: Test Application Launch**

```
LAUNCH:

1. [ ] Double-click Desktop shortcut "BendCore CAD"
2. [ ] Note startup time (target: < 5 seconds)
3. [ ] Application window opens

VERIFY UI ELEMENTS:
  [ ] Title bar: "BendCore CAD 1.0.0-PRODUCTION"
  [ ] Menu bar visible (File, Edit, View, Help)
  [ ] Toolbar with icons visible
  [ ] Left panel: Feature tree (empty)
  [ ] Center: 3D viewport (black with grid)
  [ ] Right panel: Properties panel (empty)
  [ ] Bottom: Status bar
  [ ] No error dialogs

TEST 3D VIEWPORT:
  [ ] Middle-click + drag → viewport rotates
  [ ] Scroll wheel → zoom in/out works
  [ ] Right-click + drag → pan works
  [ ] Interactions are smooth (no lag)

TEST MENUS:
  [ ] Click File menu → opens submenu
  [ ] Click File → New → dialog appears (or tree updates)
  [ ] Menu items are not grayed out

CHECK CONSOLE (F12):
  [ ] Press F12 to open Developer Tools
  [ ] Click "Console" tab
  [ ] Should be clean or show only warnings (no errors)
  [ ] No red error messages

RESULT: Mark A4 as PASS or FAIL in template
```

---

### PHASE 6: UNINSTALL TEST (30 minutes)

**Day 3 - Morning**

**Section A5: Test Uninstall**

```
UNINSTALL PROCESS:

1. [ ] Control Panel → Programs → Programs and Features
       (or: Settings → Apps → Apps & features)
2. [ ] Find: "BendCore CAD 1.0.0-PRODUCTION"
3. [ ] Click: Select it
4. [ ] Click: "Uninstall" button
5. [ ] NSIS Uninstaller dialog appears
6. [ ] Click: "Yes" or "Uninstall"
7. [ ] Wait for progress (10-30 seconds)
8. [ ] Should see "Uninstall complete"

VERIFICATION:

PowerShell:
```powershell
Test-Path "C:\Program Files\BendCore CAD"
# Should return: False (folder deleted)

# Check for registry entries:
Get-ChildItem "HKCU:\Software" | Where-Object { $_.Name -like "*BendCore*" }
# Should return nothing

# Check desktop:
Test-Path "C:\Users\$env:USERNAME\Desktop\BendCore CAD.lnk"
# Should return: False (shortcut deleted)
```

RESULT: Mark A5 as PASS or FAIL in template
```

---

### PHASE 7: PORTABLE EXECUTABLE TEST (1 hour)

**Day 3 - Afternoon**

**Section A6: Test Portable**

```
SETUP:

1. [ ] Create folder: C:\temp\bendcore-portable
2. [ ] Copy: BendCore-CAD-1.0.0-PRODUCTION-x64.exe (portable)
       to this folder

LAUNCH TEST:

1. [ ] Double-click: BendCore-CAD-1.0.0-PRODUCTION-x64.exe
2. [ ] Wait for launch (< 5 seconds, no installer dialog)
3. [ ] Should go directly to application window

VERIFY:

All features should work same as installed:
  [ ] Menu bar functional
  [ ] File → New works
  [ ] 3D viewport renders
  [ ] Property panel updates
  [ ] Feature tree works
  [ ] No console errors

TEST INDEPENDENCE:

1. [ ] Close application
2. [ ] Check: No files left in AppData
   ```powershell
   Test-Path "C:\Users\$env:USERNAME\AppData\Local\BendCore"
   # Should return: False (no artifacts)
   ```

CLEANUP:

1. [ ] Delete: C:\temp\bendcore-portable folder
2. [ ] Verify: No leftover files

RESULT: Mark A6 as PASS or FAIL in template
```

---

### PHASE 8: DOCUMENTATION (1-2 hours)

**Day 4 - Morning**

**Section A7: Complete Test Results**

```
DOCUMENT ALL FINDINGS:

Use template: WINDOWS_BUILD_TEST_RESULTS_TEMPLATE.md

1. [ ] Fill in test metadata (date, tester, OS version)
2. [ ] Record workflow status (each step)
3. [ ] Record artifact verification results
4. [ ] Record NSIS installer results
5. [ ] Record application launch metrics (timing, UI check)
6. [ ] Record uninstall verification
7. [ ] Record portable .exe results

CRITICAL SECTIONS:

If any test FAILED:
  [ ] Document issue in detail
  [ ] Note error messages
  [ ] Take screenshots if possible
  [ ] Provide reproduction steps
  [ ] Recommend fix

OVERALL STATUS:

  [ ] All sections PASS → "Windows Build Verified"
  [ ] Any section FAIL → "Windows Build Issue Found"

SIGN-OFF:

  [ ] Tester name and date
  [ ] QA Lead review
  [ ] Release Manager decision

RESULT: Complete template file
```

---

## ✅ SUCCESS CRITERIA

**Windows Build is PASS when:**

- ✓ Workflow job completes successfully
- ✓ All artifacts generated and downloadable
- ✓ NSIS installer completes without errors
- ✓ Application launches and all UI visible
- ✓ 3D viewport renders and responds
- ✓ Menus and controls functional
- ✓ No console errors
- ✓ Uninstall is clean
- ✓ Portable .exe works independently
- ✓ All results documented

---

## ⚠️ COMMON ISSUES & SOLUTIONS

### Issue: Antivirus blocks installer

**Solution**:
```
1. Disable real-time scanning (temporarily)
2. Or add BendCore.exe to antivirus whitelist
3. Retry installation
4. Re-enable antivirus after test
```

### Issue: Installer fails with error

**Solution**:
```
1. Check disk space (need 500 MB+)
2. Close other applications
3. Try running installer as Administrator
4. Document error message
5. Check GitHub Actions logs for build issue
```

### Issue: Application doesn't start

**Solution**:
```
1. Check installation folder exists
2. Try relaunching
3. Check Windows Event Viewer for errors
4. Try portable .exe instead
5. Document issue and symptoms
```

---

## 📞 ESCALATION

If you encounter blockers:

1. Document issue completely:
   - What you were doing
   - Error message (screenshot)
   - Steps to reproduce
   - OS version and machine type

2. Contact:
   - QA Lead: [name]
   - DevOps: [name]
   - Release Manager: [name]

3. File issue in GitHub

---

## 🎯 FINAL CHECKLIST

Before signing off:

- [ ] All sections A1-A6 executed
- [ ] Template completely filled
- [ ] All results documented
- [ ] Screenshots taken (if issues found)
- [ ] Issues categorized (critical vs minor)
- [ ] QA Lead review completed
- [ ] Release Manager approval obtained
- [ ] Template saved with date: WINDOWS_BUILD_TEST_RESULTS_[DATE].md

---

**Execution Guide Completed**: _______________  
**Date**: _______________  
**Notes**: _____________________________________

---

*This guide provides the step-by-step execution path for Phase 2: Windows Build Verification*
