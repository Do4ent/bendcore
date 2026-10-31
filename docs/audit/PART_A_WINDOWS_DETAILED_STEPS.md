# Part A: Windows Build Verification - Detailed Actionable Steps

**Status**: Ready for execution after PR #6 merge  
**Timeline**: Days 1-4 (1 week)  
**Owner**: QA Team  
**Environment**: Windows 10/11 machine (local, VM, or CI runner)

---

## A1. Monitor Windows Build Job - STEP BY STEP

### STEP A1.1: Trigger Workflow After PR Merge

**When**: After PR #6 is merged to `main`

**What happens automatically**:
- GitHub Actions workflow "Windows Build & Release" triggers
- Runs on `windows-latest` runner
- Estimated duration: 30-45 minutes

**Manual trigger (if needed)**:
```bash
# If workflow doesn't trigger automatically:
1. Go to: https://github.com/Do4ent/bendcore/actions
2. Click: "Windows Build & Release" workflow on left
3. Click: "Run workflow" dropdown
4. Select branch: main
5. Click: "Run workflow" button
6. Wait for build to start
```

### STEP A1.2: Monitor Build Progress in Real-Time

**Location**: https://github.com/Do4ent/bendcore/actions

**What to watch**:

```
┌─ build-windows job
│
├─ ✓ Setup Node.js (should complete in 1-2 min)
│   └─ Expected: "Setup Node 20.x"
│
├─ ✓ Install dependencies (npm ci) (5-10 min)
│   └─ Expected: "added 970 packages in 27s"
│   └─ Check: No ERROR in output
│
├─ ✓ Run tests (npm test -- --ci --runInBand) (45-60 min)
│   ✓✓ CRITICAL: All tests must PASS
│   └─ Expected output:
│       "Test Suites: 51 passed, 51 total"
│       "Tests: 1217 passed, 1217 total"
│   └─ Check: Exit code 0 (success)
│   └─ FAILURE if: Any test fails (should NOT continue)
│
├─ ✓ Type-check (npm run typecheck) (2-3 min)
│   └─ Expected: No TypeScript errors
│   └─ Includes: main, server, AND api checks
│
├─ ✓ Build (npm run build) (15-20 min)
│   └─ Expected output:
│       "webpack 5.112.2 compiled with 3 warnings"
│       "Generated /dist/web/index.html"
│   └─ Warnings: OK (3 size warnings acceptable)
│
├─ ✓ Electron Pack (npm run dist) (10-15 min)
│   ✓✓ CRITICAL: Look for artifact generation
│   └─ Expected: electron-builder creates .exe and .msi
│   └─ Output: "Building for windows ..."
│   └─ Final: "✓ Building complete"
│
├─ ✓ List artifacts (ls -lh release/) (1 min)
│   ✓✓ CRITICAL: Verify files exist
│   └─ Expected files:
│       -rw-r--r-- ... BendCore-CAD-1.0.0-PRODUCTION-x64.exe (150+ MB)
│       -rw-r--r-- ... BendCore-CAD-1.0.0-PRODUCTION-x64.exe (portable)
│       -rw-r--r-- ... latest-x64.yml
│   └─ FAILURE if: No files found
│
├─ ✓ Generate checksums (sha256sum) (1 min)
│   └─ Expected:
│       xxxxxxxx...  BendCore-CAD-1.0.0-PRODUCTION-x64.exe
│       xxxxxxxx...  BendCore-CAD-1.0.0-PRODUCTION-x64.exe
│
└─ ✓ Upload Artifacts (2 uploads) (2-5 min)
    ├─ artifacts/upload-artifact@v4
    │  └─ windows-builds (should complete)
    │  └─ checksums.txt included
    │
    └─ artifacts/upload-artifact@v4
       └─ windows-checksums (should complete)
```

### STEP A1.3: Check Job Status

**Success Indicators**:
```
✓ All steps completed (no red X marks)
✓ Final status: "Success" (green checkmark)
✓ Duration: ~60 minutes is normal
```

**Failure Indicators**:
```
✗ Any step shows red X
✗ "Run tests" failed → STOP, investigate test failures
✗ "Electron Pack (npm run dist)" failed → electron-builder issue
✗ "List artifacts" shows empty release/ → build didn't create files
✗ Upload failed → artifact storage issue
```

**If failure occurs**:
1. Click failed step to expand
2. Read error message carefully
3. Document error
4. Check logs for root cause
5. File issue or retry if transient

---

## A2. Download and Inspect Artifacts - DETAILED

### STEP A2.1: Access Artifacts Section

**In GitHub Actions page**:
```
1. Scroll down to "Artifacts" section (bottom of job output)
2. Two artifacts should be visible:
   - windows-builds (should be 300-400 MB)
   - windows-checksums (should be < 1 KB)
```

### STEP A2.2: Download windows-builds Artifact

**Steps**:
```
1. Click: "windows-builds" artifact link
2. Browser downloads ZIP file: windows-builds.zip (~300-400 MB)
3. Wait for download to complete
4. Extract ZIP to temp folder:
   - Windows: Right-click → "Extract All"
   - Extract to: C:\temp\bendcore-builds
5. Verify contents:
   ✓ BendCore-CAD-1.0.0-PRODUCTION-x64.exe (NSIS installer, 150+ MB)
   ✓ BendCore-CAD-1.0.0-PRODUCTION-x64.exe (portable, 150+ MB)
   ✓ latest-x64.yml (update manifest, < 1 KB)
   ✓ checksums.txt (SHA256 hashes)
```

### STEP A2.3: Verify File Properties

**For each .exe file**:

```powershell
# In PowerShell:
cd C:\temp\bendcore-builds
ls -la *.exe

# Expected output:
# -a----        10/31/2026   1:00 PM       150000000 BendCore-CAD-1.0.0-PRODUCTION-x64.exe
```

**Checks**:
- [ ] File size > 100 MB (not empty)
- [ ] File date is TODAY (not stale)
- [ ] Not corrupted (try to read first bytes)

```powershell
# Check file is not corrupted (read first 1000 bytes):
[byte[]]$bytes = @()
$stream = [System.IO.File]::OpenRead("BendCore-CAD-1.0.0-PRODUCTION-x64.exe")
$stream.Read($bytes, 0, 1000) | out-null
$stream.Close()
Write-Host "File readable: OK"
```

### STEP A2.4: Verify Checksums

**Steps**:
```
1. Open checksums.txt (ASCII text file)
2. Should contain lines like:
   
   abc123def456...  BendCore-CAD-1.0.0-PRODUCTION-x64.exe
   xyz789abc123...  BendCore-CAD-1.0.0-PRODUCTION-x64.exe

3. Copy SHA256 hash for installer
4. In PowerShell, calculate hash:
```

```powershell
$file = "C:\temp\bendcore-builds\BendCore-CAD-1.0.0-PRODUCTION-x64.exe"
$hash = Get-FileHash $file -Algorithm SHA256
$hash.Hash
# Output should match line in checksums.txt
```

**Result**:
- [ ] Hash matches checksums.txt → file is authentic
- [ ] Hash doesn't match → file corrupted during download

---

## A3. Test NSIS Installer - STEP BY STEP

### Prerequisites Check
- [ ] Windows 10/11 machine available
- [ ] Admin privileges available
- [ ] At least 500 MB free disk space
- [ ] BendCore-CAD-1.0.0-PRODUCTION-x64.exe downloaded

### STEP A3.1: Disable Real-Time Scanning (Optional)

**Why**: Some antivirus software blocks unsigned installers

**Windows Defender**:
```
1. Settings → Virus & threat protection
2. Manage settings
3. Toggle "Real-time protection" OFF
4. Accept warning (temporary)
```

**Other antivirus**: Check software's UI for "exclude program" or "scan pause"

### STEP A3.2: Launch Installer

**Action**:
```
1. Open File Explorer
2. Navigate to: C:\temp\bendcore-builds
3. Double-click: BendCore-CAD-1.0.0-PRODUCTION-x64.exe

Expected: "Windows Defender SmartScreen" warning may appear
(This is normal for unsigned executables)
```

**If SmartScreen blocks**:
```
1. Click: "More info"
2. Click: "Run anyway" (blue button)
3. Wait for installer to launch
```

**If blocked by antivirus**:
```
1. Check antivirus alerts
2. Allow execution (vary by AV software)
3. Retry step 3
```

### STEP A3.3: Accept NSIS Welcome Screen

**Window appears**: "BendCore CAD - Setup Wizard"

**Steps**:
```
✓ Title bar shows "BendCore CAD 1.0.0-PRODUCTION"
✓ Welcome message visible
✓ "Next >" button functional
✓ Click: "Next >" to proceed

Expected: No error dialogs
```

### STEP A3.4: Select Installation Directory

**Window**: "Choose Installation Folder"

**Steps**:
```
✓ Dialog shows folder tree
✓ Default path: C:\Program Files\BendCore CAD
✓ Check: Can browse folders (navigation works)
✓ Options appear:
  - "Install for all users" (radio button)
  - "Install for current user" (radio button)
✓ Select: "Install for all users" (standard)
✓ Click: "Install" button

Expected: No errors, progress bar appears next
```

### STEP A3.5: Monitor Installation Progress

**Window**: "Installing BendCore CAD"

**Progress bar**:
```
✓ Bar moves from 0% to 100%
✓ Status messages appear (e.g., "Installing files...")
✓ Duration: Typically 30-60 seconds
✓ No error dialogs during progress

Expected: Progress completes normally
Failure: Any error message = installer issue
```

### STEP A3.6: Completion Screen

**Window**: "Completing BendCore CAD Setup"

**Checkboxes**:
```
✓ "Create Desktop Shortcut" (checked)
✓ "Create Start Menu Folder" (checked)
✓ "Run BendCore CAD" (can be checked/unchecked)
```

**Steps**:
```
1. Uncheck: "Run BendCore CAD" (for now)
2. Click: "Finish" button
3. Installer closes

Expected: No error messages, installer exits cleanly
```

### STEP A3.7: Verify Installation Files

**Verify folder created**:
```powershell
# In PowerShell:
dir "C:\Program Files\BendCore CAD"

# Expected output:
# Directory: C:\Program Files\BendCore CAD
# 
# Mode  LastWriteTime          Length Name
# ----  ---------------        ------ ----
# d---- 2026-10-31  01:00:00          resources
# d---- 2026-10-31  01:00:00          node_modules
# -a--- 2026-10-31  01:00:00  1500000 BendCore CAD.exe
# ... (other files)
```

**Checks**:
- [ ] resources/ folder exists (contains assets)
- [ ] node_modules/ folder exists (contains dependencies)
- [ ] BendCore CAD.exe exists
- [ ] Total folder size > 200 MB

### STEP A3.8: Verify Desktop Shortcut

**Check Desktop**:
```
✓ Icon labeled "BendCore CAD" visible on Desktop
✓ Right-click → Properties:
  - Target: C:\Program Files\BendCore CAD\BendCore CAD.exe
  - Start in: C:\Program Files\BendCore CAD
  - Shortcut tab shows icon image
```

### STEP A3.9: Verify Start Menu Entry

**Check Start Menu**:
```
1. Click: Windows Start button (bottom-left)
2. Type: "bendcore"
3. Expected: "BendCore CAD" appears in search
4. Right-click → Properties:
  - Validates shortcut exists
```

---

## A4. Test Application Launch - STEP BY STEP

### STEP A4.1: Launch from Desktop Shortcut

**Action**:
```
1. Double-click: Desktop shortcut "BendCore CAD"
2. Observe: Application starts (note timing)
```

**Timing**:
```
Expected: Application appears within 5 seconds
- 0-2 sec: Splash screen or loading indicator
- 2-5 sec: Main window with UI loaded
- > 5 sec: Slower than expected (may indicate issue)
```

### STEP A4.2: Main Window Verification

**Window opens**: "BendCore CAD 1.0.0-PRODUCTION"

**Visual checks**:
```
✓ Title bar shows: "BendCore CAD 1.0.0-PRODUCTION"
✓ Menu bar visible (File, Edit, View, Help, etc.)
✓ Toolbar with icons visible (top area)
✓ Left panel: Feature tree (empty, ready for design)
✓ Center area: 3D viewport (black canvas with grid)
✓ Right panel: Properties panel (empty)
✓ Bottom: Status bar visible
✓ No error dialogs or popups

Expected: All UI elements present and visible
Failure: Missing panels, error messages, or rendering issues
```

### STEP A4.3: Test 3D Viewport Rendering

**Checks**:
```
1. 3D area shows: Black background with grid
2. Try mouse interactions:
   - Middle-click + drag: Should rotate view
   - Scroll wheel: Should zoom in/out
   - Right-click + drag: Should pan
3. Expected: Smooth, responsive interaction
```

**Success criteria**:
```
✓ Viewport responds immediately to mouse input
✓ No lag or frozen UI
✓ View updates smoothly
✓ Zoom works on both scroll directions
```

### STEP A4.4: Test Menu Functionality

**Steps**:
```
1. Click: "File" menu
   Expected: Submenu appears (New, Open, Save, Export, etc.)
   
2. Hover over menu items
   Expected: Tooltips may appear
   
3. Click: "File" → "New"
   Expected: 
   - Dialog appears to create new design
   - Or feature tree updates (ready for input)
   
4. Close dialog (or press Escape)
```

**Failure if**:
```
✗ Menu doesn't open
✗ Menu items are grayed out/disabled
✗ Clicking items causes errors
```

### STEP A4.5: Test No Console Errors

**Open Developer Tools**:
```
1. Press: F12 (or right-click → Inspect)
2. Click: "Console" tab
3. Check for error messages
```

**Expected**:
```
✓ Console is mostly empty or shows only warnings
✓ No ERROR level messages
✓ Warnings (yellow) acceptable (deprecated APIs, etc.)
```

**Failure if**:
```
✗ Red error messages appear
✗ Multiple errors in console
✗ JavaScript errors or crashes
```

### STEP A4.6: Document Application Launch Success

**Screenshot/Notes**:
```
Document:
- Launch time (in seconds)
- 3D viewport rendering (visible/visible-with-lag/black-screen)
- Menu responsiveness (fast/normal/slow)
- Console errors (none/warnings-only/errors-found)
- Overall impression (working/mostly-working/broken)
```

---

## A5. Test Uninstall - STEP BY STEP

### STEP A5.1: Open Programs and Features

**Windows**:
```
1. Press: Windows key
2. Type: "control panel"
3. Click: "Control Panel" application
4. Click: "Programs" → "Programs and Features"
   (Or: Settings → Apps → Apps & features in Windows 11)
```

### STEP A5.2: Find BendCore CAD

**In Programs list**:
```
1. Scroll to find: "BendCore CAD 1.0.0-PRODUCTION"
2. Click: Select it
3. Click: "Uninstall" button (top or right-click menu)
```

### STEP A5.3: Uninstall Process

**NSIS Uninstaller appears**:
```
1. Window title: "BendCore CAD - Uninstall"
2. Asks: "Are you sure?" (typical)
3. Click: "Yes" or "Uninstall" button
4. Progress bar appears
5. Wait for completion (typically 10-30 seconds)

Expected:
✓ No error dialogs
✓ Progress completes normally
✓ Message appears: "Uninstall complete"
✓ Uninstaller closes
```

### STEP A5.4: Verify Files Deleted

**Check installation folder**:
```powershell
# Should NOT exist:
Test-Path "C:\Program Files\BendCore CAD"
# Expected output: False

# If folder still exists:
dir "C:\Program Files\BendCore CAD"
# Should show empty or minimal files (Windows cleanup artifacts)
```

### STEP A5.5: Verify Desktop Shortcut Removed

**Check Desktop**:
```
✓ "BendCore CAD" shortcut should NOT be visible
✓ If present: Right-click → Delete
```

### STEP A5.6: Verify Start Menu Removed

**Check Start Menu**:
```
1. Windows key
2. Type: "bendcore"
3. Expected: Nothing should appear
4. If entry still shows: It's Windows indexing cache (temporary)
```

### STEP A5.7: Verify Registry Cleanup

**PowerShell**:
```powershell
# Check for registry entries:
Get-ChildItem "HKCU:\Software" | Where-Object { $_.Name -like "*BendCore*" }
Get-ChildItem "HKLM:\Software" | Where-Object { $_.Name -like "*BendCore*" }

# Expected: No results (clean uninstall)
```

---

## A6. Test Portable Executable - STEP BY STEP

### Prerequisites
- [ ] BendCore-CAD-1.0.0-PRODUCTION-x64.exe (portable) downloaded
- [ ] Another Windows location (different from Program Files)

### STEP A6.1: Prepare Portable Launch

**Action**:
```
1. Create folder: C:\temp\bendcore-portable
2. Copy: BendCore-CAD-1.0.0-PRODUCTION-x64.exe to this folder
3. This folder contains ONLY the .exe (no other files)
```

### STEP A6.2: Launch Portable

**Action**:
```
1. Navigate to: C:\temp\bendcore-portable
2. Double-click: BendCore-CAD-1.0.0-PRODUCTION-x64.exe
3. Wait for application to start
```

**Expected**:
```
✓ Application launches without installation dialog
✓ No "Welcome" or "Setup Wizard"
✓ Directly shows main application window
✓ Should take 3-5 seconds to start
```

### STEP A6.3: Verify Portable Functionality

**All features should work**:
```
✓ Menu bar functional
✓ File → New works
✓ 3D viewport renders and responds
✓ Property panel updates
✓ Feature tree works
✓ No differences from installed version
```

### STEP A6.4: Check No Dependencies Outside Folder

**Portable should NOT require**:
```
✗ Installed version in Program Files
✗ Windows registry entries
✗ Installed dependencies
✗ System PATH modifications
```

**Test**:
```
1. Uninstall BendCore CAD (see A5)
2. Portable .exe should STILL work
3. If application fails after uninstall → not truly portable
```

### STEP A6.5: Verify No System Artifacts

**After closing portable**:
```
Check these locations for new files:
- C:\Users\[username]\AppData\Local\BendCore (should not exist)
- C:\Users\[username]\AppData\Roaming\BendCore (should not exist)
- HKEY_CURRENT_USER\Software\BendCore (should not exist)
```

**Result**:
```
✓ No artifacts left on system → truly portable
✗ Files/registry entries created → not fully portable
```

### STEP A6.6: Cleanup

**Remove portable test**:
```
1. Close BendCore application
2. Delete: C:\temp\bendcore-portable folder
3. Verify: No files left behind
```

---

## A7. Document Results - TEMPLATES

See: **WINDOWS_BUILD_TEST_RESULTS_TEMPLATE.md** (next document)

---

**All Part A steps complete = Windows Build Verification READY FOR PRODUCTION** ✓
