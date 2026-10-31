# Windows & CAD Testing - Troubleshooting Guide

**What to do when tests fail**

---

## WINDOWS BUILD TROUBLESHOOTING

### Problem: GitHub Workflow Fails

**Symptom**: Red X on build-windows job in GitHub Actions

**Investigation**:
1. Click failed step to expand
2. Read error message carefully
3. Check step that failed (most likely: npm test, npm dist)

**Possible causes & solutions**:

#### npm ci fails (dependency installation)
```
Error: "ERR! peer npm notice"
Cause: Network issue or package not available

Solution:
1. Retry workflow
2. Check npm registry status
3. Manually run: npm ci --no-audit
4. Check package-lock.json for issues
```

#### npm test fails (tests not passing)
```
Error: "Test Suites: X failed, Y passed"
Cause: Tests broken by code changes

Solution:
1. Run locally: npm test -- --ci --runInBand
2. Identify failing test
3. Check recent code changes
4. Fix test or code
5. Re-trigger workflow
```

#### npm dist fails (electron-builder)
```
Error: "electron-builder error" or "No valid code signing"
Cause: 
  - electron-builder not installed
  - NSIS tools missing
  - Path issues

Solution:
1. Check electron-builder in package.json
2. Run locally: npm run dist (on Windows)
3. Check NSIS installer path
4. Verify release/ folder accessible
```

#### Artifacts not uploaded
```
Error: "Failed to upload artifact" or "No artifacts found"
Cause:
  - Files not created by dist
  - Upload service issue
  - Retention policy

Solution:
1. Check "List build artifacts" step output
2. Verify: release/*.exe files exist
3. Check artifact storage quota
4. Retry upload step
```

---

### Problem: Cannot Download Artifacts

**Symptom**: Artifact link doesn't work or download fails

**Solution**:
```
1. GitHub Actions must have completed successfully (green checkmark)
2. Click artifact name → browser downloads ZIP
3. If still fails:
   - Try a different browser
   - Check internet connection
   - Clear browser cache
   - Try incognito mode
   - Retry after 5 minutes
```

---

### Problem: NSIS Installer Won't Launch

**Symptom**: Double-clicking .exe does nothing or error appears

**Possible causes & solutions**:

#### SmartScreen blocks execution
```
Message: "Windows protected your PC"
Cause: Unsigned executable (expected - not yet signed)

Solution:
1. Click "More info"
2. Click "Run anyway" (blue button)
3. Accept UAC prompt
```

#### Antivirus blocks file
```
Symptom: File disappears or blocked message
Cause: Antivirus quarantined executable

Solution:
1. Disable antivirus real-time scanning temporarily
2. Restore file from quarantine
3. Add .exe to antivirus whitelist
4. Retry installation
5. Re-enable antivirus
```

#### File corrupted or incomplete
```
Error: "This is not a valid Windows installer"
Cause: Download corrupted or incomplete

Solution:
1. Delete existing file
2. Re-download from GitHub Actions
3. Verify SHA256 hash matches checksums.txt
4. Try again
```

#### No admin rights
```
Error: "Access denied" during installation
Cause: Installation requires administrator

Solution:
1. Right-click .exe
2. Select "Run as administrator"
3. Accept UAC prompt
4. Retry installation
```

---

### Problem: Application Won't Start After Installation

**Symptom**: Shortcut clicked but nothing happens or error

**Possible causes & solutions**:

#### Installation incomplete
```
Verify installation folder:
  C:\Program Files\BendCore CAD

PowerShell:
  dir "C:\Program Files\BendCore CAD" | measure
  # Should show 150+ items

If empty or missing:
  1. Re-run installer
  2. Choose different path
  3. Ensure 500+ MB free space
```

#### Corrupted node_modules
```
Symptom: "Module not found" or crashes on startup

Solution:
1. Check node_modules folder exists
2. Reinstall with fresh installer
3. Or manually run: npm ci
```

#### Missing dependencies
```
Check for error messages:
  Press F12 → Console tab

If errors about missing modules:
1. Delete: C:\Program Files\BendCore CAD
2. Re-run installer
3. Ensure internet connection during install
```

#### GPU/Driver issues
```
Symptom: Black screen or graphics errors

Solution:
1. Update GPU drivers
2. Check if WebGL works: https://webglreport.com/
3. Try different resolution
4. Try on different machine
```

---

### Problem: Uninstall Doesn't Clean Up

**Symptom**: Files/shortcuts still present after uninstall

**Solution**:
```
Manual cleanup:

1. Delete folder:
   C:\Program Files\BendCore CAD
   
2. Delete shortcuts:
   Desktop: "BendCore CAD" shortcut
   Start Menu: BendCore CAD entry
   
3. Clean registry:
   PowerShell:
   Get-ChildItem "HKCU:\Software" | 
   Where-Object { $_.Name -like "*BendCore*" } |
   Remove-Item
   
4. Delete AppData:
   C:\Users\[username]\AppData\Local\BendCore
   C:\Users\[username]\AppData\Roaming\BendCore
```

---

### Problem: Portable .exe Doesn't Work

**Symptom**: Portable executable fails or doesn't launch

**Solution**:
```
1. Verify file is complete:
   - Size > 100 MB
   - SHA256 hash matches
   
2. Run from different location:
   - Try: C:\Temp\Test\
   - Try: C:\Users\[user]\Downloads\
   
3. Check dependencies:
   - Required: Windows 10/11
   - Required: .NET Framework (usually pre-installed)
   
4. If portable fails but installer works:
   - Issue is with portable executable
   - Use installer version instead
   - Report issue with portable
```

---

## CAD FILE TESTING TROUBLESHOOTING

### Problem: CAD File Won't Import

**Symptom**: File → Open dialog appears but file cannot open

**Cause & Solution**:
```
Possible causes:
1. File format not supported
   → Check file extension (.bdc)
   → Verify JSON structure

2. File corrupted
   → Verify with text editor
   → Check for valid JSON

3. Application crashed during import
   → Check console errors (F12)
   → Check Windows Event Viewer
   → Try smaller file first

4. Missing features in code
   → Check if import handler exists
   → Verify BDC file format supported
```

---

### Problem: Geometry Not Visible

**Symptom**: File imported but 3D viewport empty

**Solution**:
```
1. Check feature tree:
   - Left panel should show segments
   - If empty: import may have failed
   
2. Try "Fit All" command (if available):
   - Menu → View → Fit All
   - Or press Home key
   - Viewport should zoom to geometry
   
3. Check viewport settings:
   - Ensure 3D rendering enabled
   - Check GPU drivers (see Windows section)
   
4. Try different file:
   - Try test_simple_tube.bdc first
   - If that doesn't show, geometry issue
```

---

### Problem: Geometry Looks Wrong

**Symptom**: Geometry visible but dimensions/angles don't look correct

**Investigation**:
```
1. Measure dimensions in application:
   - Use dimension tools (if available)
   - Compare to expected values in test file
   
2. Check calculations:
   - Simple tube: Should be 12" straight line
   - Multi-bend: Should show 2 bends at 90°
   - Parametric: Values should match formulas
   
3. Document discrepancy:
   - Take screenshot
   - Note expected vs actual
   - Provide measurements
   - Report as BLOCKER if critical
```

---

### Problem: Save/Reload Doesn't Preserve Geometry

**Symptom**: After save and reload, design is different

**Solution**:
```
Debug steps:
1. Open file
2. Note initial dimensions/properties
3. Edit minor property (e.g., name)
4. File → Save
5. Close application
6. Reopen application
7. File → Open (same file)
8. Compare dimensions

If changed:
  → Issue: Save/reload broken
  → This is a BLOCKER
  → Do NOT continue other tests
  → Report immediately
```

---

### Problem: Undo/Redo Doesn't Work

**Symptom**: Undo (Ctrl+Z) doesn't revert changes

**Solution**:
```
Test carefully:
1. Open file
2. Make ONE small change
   - Edit property
   - Change dimension
   - Rename segment
3. Ctrl+Z (undo)
   - Should revert that change
   
If undo doesn't work:
  → Feature not implemented or broken
  → Skip undo/redo test
  → Document as missing feature
  
If undo works but redo doesn't:
  → Check: Ctrl+Y or Ctrl+Shift+Z
  → Try Edit menu → Redo
  → May be different shortcut
```

---

### Problem: Parameters Won't Update

**Symptom**: Change parameter value but geometry doesn't update

**Solution**:
```
For parametric test file:
1. Identify parameter: baseLength
2. Find where to change it:
   - Properties panel?
   - Dialog box?
   - Formula bar?
   
3. Change value: 10.0 → 12.0
4. Press Enter or click OK
5. Check:
   - Feature tree updates?
   - Geometry changes?
   - Other dimensions recalculate?
   
If parameters don't update:
  → Parametric features broken
  → Document issue
  → Try with test_simple_tube instead
```

---

### Problem: Application Crashes During Testing

**Symptom**: Application freezes or closes unexpectedly

**Investigation**:
```
1. Check Windows Event Viewer for errors:
   - Event Viewer → Windows Logs → Application
   - Look for BendCore errors
   - Note error code and description
   
2. Check console errors (if app is still running):
   - Press F12
   - Check Console tab
   - Screenshot any errors
   
3. Note what you were doing:
   - Exact steps before crash
   - File being tested
   - What action triggered crash
   
4. Reproduce the crash:
   - Try same steps again
   - Does it always crash at same point?
   - Or was it a one-time issue?
   
5. Report:
   - This is a BLOCKER
   - Include steps to reproduce
   - Include error messages
   - Mark test as UNABLE TO COMPLETE
```

---

## DOCUMENTATION ISSUES

### Problem: Can't Find Test Results Template

**Solution**:
```
Files are located in:
  /docs/audit/

Windows template:
  WINDOWS_BUILD_TEST_RESULTS_TEMPLATE.md

CAD template:
  CAD_FILE_TEST_RESULTS_TEMPLATE.md
  
Make a copy for your test:
  WINDOWS_BUILD_TEST_RESULTS_[DATE].md
  CAD_FILE_TEST_RESULTS_[DATE].md
```

---

### Problem: Don't Know What to Document

**Solution**:
```
Use templates provided:
  - Templates have every section
  - Mark PASS or FAIL for each
  - Add notes in brackets
  - Take screenshots of issues
  - Document timestamps
  - Include error messages
  
Questions? Contact QA Lead
```

---

## ESCALATION CRITERIA

**Mark as BLOCKER when**:
1. Workflow doesn't complete
2. Artifacts can't be downloaded
3. Installer won't run at all
4. Application won't start after install
5. CAD file import completely fails
6. Application crashes during testing
7. Geometry is clearly corrupted
8. Save/reload loses data

**For blockers**:
1. Stop testing further sections
2. Document exactly what failed
3. Take screenshots and error messages
4. Contact QA Lead immediately
5. Do NOT try workarounds

---

**When in doubt, escalate. Better to pause and ask than to document wrong results.**
