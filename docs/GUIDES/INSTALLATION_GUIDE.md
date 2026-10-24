# BendCore v8.0 Installation Guide

**Version:** v8.0  
**Date:** October 23, 2026  
**Platform:** Windows x64 (10, 11)

---

## Table of Contents

1. [System Requirements](#system-requirements)
2. [Pre-Installation Checklist](#pre-installation-checklist)
3. [Installation Methods](#installation-methods)
4. [Post-Installation Setup](#post-installation-setup)
5. [Verification](#verification)
6. [Troubleshooting](#troubleshooting)
7. [Uninstallation](#uninstallation)

---

## System Requirements

### Minimum Requirements

| Component | Requirement |
|-----------|------------|
| **OS** | Windows 10 64-bit (Build 19041+) |
| **RAM** | 4 GB |
| **Disk Space** | 500 MB (SSD recommended) |
| **Processor** | Intel Core i5 / AMD Ryzen 5 (2.4 GHz+) |
| **GPU** | OpenGL 3.3 compatible |
| **Display** | 1920×1080 or higher |
| **Internet** | Required for first-time activation |

### Recommended Requirements

| Component | Recommendation |
|-----------|----------------|
| **OS** | Windows 11 64-bit (latest updates) |
| **RAM** | 8 GB or more |
| **Disk Space** | 1 GB (SSD for optimal performance) |
| **Processor** | Intel Core i7 / AMD Ryzen 7 (3.0 GHz+) |
| **GPU** | Dedicated NVIDIA/AMD GPU with 2GB+ VRAM |
| **Display** | 4K or ultrawide (3440×1440) |
| **Internet** | Stable connection (for features and updates) |

### Supported Operating Systems

- Windows 10 (21H2 or later)
- Windows 11 (all versions)
- Windows Server 2019+ (for server deployments)

### Hardware Notes

**GPU Support:**
- NVIDIA: GeForce GTX 1050 Ti or better (supports CUDA)
- AMD: Radeon RX 580 or better
- Intel: Iris Xe or Arc (limited support)

**Storage:**
- SSD preferred for faster startup
- Minimum 500 MB free space for installation
- Additional 500 MB for project caching

---

## Pre-Installation Checklist

### Before You Install

- [ ] **Verify system requirements** (see above)
- [ ] **Check available disk space** (at least 1 GB free recommended)
- [ ] **Back up existing BendCore projects** (if upgrading from v7.x)
- [ ] **Note your license key** (if you have one)
- [ ] **Disable antivirus temporarily** (if you encounter issues)
- [ ] **Ensure administrator access** (required for installation)
- [ ] **Close all running applications** (recommended)
- [ ] **Update Windows** to the latest version

### Download Prerequisites

Depending on your installation method, you may need to download:

1. **Installer Package**
   - `BendCore-v8.0-setup.exe` (executable installer, ~300 MB)
   - `BendCore-v8.0.msi` (Windows Installer package, ~300 MB)

2. **Optional Components**
   - Visual C++ Redistributable 2022 (if not already installed)
   - .NET Framework 4.8 (if not already installed)

3. **License File** (if available)
   - License.lic file for offline activation

---

## Installation Methods

### Method 1: Using Setup Executable (Recommended)

#### Step-by-Step Installation

**1. Download the Installer**
   - Visit https://bendcore.io/download
   - Download `BendCore-v8.0-setup.exe`
   - Verify file integrity (SHA256 checksum provided)

**2. Run the Installer**
   ```
   BendCore-v8.0-setup.exe
   ```
   - Right-click → "Run as administrator" (if prompted)
   - Windows may display a security warning - click "Run"

**3. Welcome Screen**
   - Click "Next" to proceed
   - Read and accept the license agreement
   - Click "I Agree" to continue

**4. Installation Type**
   - Select **"Standard Installation"** (recommended for most users)
     - Includes all core components
     - Approx. 500 MB total size
   - Or **"Custom Installation"** for advanced options
     - Choose individual components
     - Specify installation directory

**5. Installation Location**
   - Default: `C:\Program Files\BendCore\v8.0`
   - Change if desired (not recommended unless required)
   - Ensure sufficient disk space in selected drive

**6. Start Menu & Shortcuts**
   - Check "Create Start Menu folder" (recommended)
   - Check "Create desktop shortcut" (optional)
   - Check "Create Quick Launch shortcut" (optional)

**7. Installation Progress**
   - Wait for installation to complete
   - This typically takes 2-5 minutes
   - Progress bar shows file extraction and installation status

**8. Completion**
   - Click "Finish" to complete installation
   - Optionally launch BendCore immediately
   - Check "Launch BendCore v8.0" checkbox to start

#### What Gets Installed

```
C:\Program Files\BendCore\v8.0\
├── bin\
│   ├── BendCore.exe (main application)
│   ├── BendCore.dll (core library)
│   └── *.dll (dependencies)
├── lib\
│   ├── OCCT 7.8 components
│   ├── libdxfrw components
│   └── boost libraries
├── resources\
│   ├── icons\
│   ├── templates\
│   └── documentation\
├── plugins\
│   ├── formula-engine.dll
│   ├── analytics.dll
│   └── import-export.dll
└── config\
    └── default-settings.json
```

---

### Method 2: Using MSI Installer

#### Step-by-Step Installation

**1. Download MSI Package**
   - Visit https://bendcore.io/download
   - Download `BendCore-v8.0.msi`

**2. Run MSI Installer**
   - Double-click `BendCore-v8.0.msi`
   - Or: `msiexec /i BendCore-v8.0.msi` (from command line)

**3. Installation Wizard**
   - Follow same steps as Method 1 (see above)
   - MSI provides same options as setup.exe

**4. Command Line Installation**
   ```batch
   msiexec /i BendCore-v8.0.msi INSTALLDIR="C:\BendCore" /quiet
   ```
   - `/quiet`: Silent installation (no UI)
   - `/log install.log`: Log installation details
   - `INSTALLDIR=`: Specify custom installation path

#### Advantages of MSI Method

- Better integration with Windows deployment tools
- Group Policy deployment support (enterprise)
- Repair/reinstall options
- Uninstall through Control Panel
- Command-line automation support

---

### Method 3: Portable Installation (No Admin Required)

#### Using Portable ZIP Package

**1. Download Portable Package**
   - `BendCore-v8.0-portable.zip` (~350 MB)
   - No installer required

**2. Extract Files**
   - Right-click ZIP file
   - Select "Extract All..."
   - Choose destination folder (e.g., `D:\BendCore`)
   - Click "Extract"

**3. Launch BendCore**
   - Navigate to extracted folder
   - Double-click `BendCore.exe`
   - No installation needed!

#### Advantages

- No administrator privileges required
- Instant deployment
- Portable to USB drives
- Multiple versions can coexist

#### Limitations

- No automatic updates
- No Start Menu integration
- Manual uninstallation
- Larger file size (~350 MB)

---

## Post-Installation Setup

### First Launch Configuration

When you launch BendCore v8.0 for the first time:

**1. Welcome Wizard**
   - Automatically runs on first launch
   - Guides you through initial setup
   - Takes ~2-3 minutes

**2. License Activation**
   
   **Option A: Online Activation (Recommended)**
   ```
   1. Enter your license key
   2. Click "Activate Online"
   3. Internet connection required
   4. Activation completes in seconds
   ```
   
   **Option B: Offline Activation**
   ```
   1. Click "Offline Activation"
   2. Copy your activation code
   3. Visit https://bendcore.io/activate
   4. Paste code and follow instructions
   5. Download license file
   6. Import license file in BendCore
   ```
   
   **Option C: Trial Mode**
   ```
   1. Skip license activation
   2. 30-day trial begins automatically
   3. All features available
   4. No license key required
   ```

**3. Workspace Configuration**
   - Set default project folder
   - Choose: `C:\Users\[YourName]\Documents\BendCore` (recommended)
   - Or specify custom location
   - Click "Create Folder" if needed

**4. Template Library**
   - Select template set to load
   - Options: "Standard", "Advanced", "Manufacturing"
   - Download additional templates if desired
   - Default: "Standard" (recommended)

**5. Performance Settings**
   - Enable hardware acceleration (recommended)
   - Set cache size: 100MB (default) to 500MB (if RAM permits)
   - Enable autosave: Yes (every 5 minutes)
   - Thread count: Auto-detect (recommended)

**6. Theme Selection**
   - Light mode: Classic BendCore appearance
   - Dark mode: Modern dark interface
   - System: Follow Windows theme
   - Choice: Dark mode (recommended for eye comfort)

**7. Completion**
   - Click "Finish" to complete setup
   - BendCore is now ready to use!

---

### Configuration Files

After installation, configuration files are located at:

**Windows Configuration Path:**
```
%APPDATA%\BendCore\v8.0\
├── config\
│   ├── bendcore.config.json (main configuration)
│   ├── user.preferences.json (user settings)
│   ├── licenses.json (license information)
│   └── workspaces.json (saved workspaces)
├── cache\
│   ├── templates.cache (cached templates)
│   ├── geometry.cache (cached geometries)
│   └── search.index (search database)
└── logs\
    ├── bendcore.log (main log file)
    ├── import.log (import operations log)
    └── errors.log (error log)
```

**Quick Access:**
- Press `Ctrl+,` in BendCore to open settings
- Click "Folders" to access configuration directory
- Edit JSON files for advanced configuration

---

## Verification

### Verify Installation Success

**1. Check Installation**
   - Open Control Panel → Programs → Programs and Features
   - Look for "BendCore v8.0" in the list
   - Should show version number: 8.0.20261023

**2. Verify Files**
   ```batch
   dir "C:\Program Files\BendCore\v8.0\bin\"
   ```
   - Should show: BendCore.exe, BendCore.dll, and related files

**3. Launch Application**
   - Double-click BendCore shortcut on desktop
   - Or: Start Menu → BendCore v8.0 → BendCore
   - Should launch successfully within 5-10 seconds

**4. Check Splash Screen**
   - Should show: "BendCore v8.0"
   - Build number: 20261023
   - License status

**5. Verify Components**
   - Help → About BendCore
   - Shows:
     - Version: v8.0
     - Build: 20261023
     - License: (Active/Trial)
     - Installed components list

**6. Test Core Features**
   
   **Formula Engine:**
   ```
   1. File → New Project
   2. Design → Add Formula
   3. Enter: 2 * pi * radius = circumference
   4. Should evaluate without errors
   ```
   
   **Import/Export:**
   ```
   1. File → Import
   2. Select a sample STEP/IGES file
   3. Should display geometry preview
   4. Click "Import" to complete
   ```
   
   **Search & Templates:**
   ```
   1. Library → Templates
   2. Should show 3+ templates loaded
   3. Search field should be functional
   4. Try searching for a template name
   ```

---

## Troubleshooting

### Common Installation Issues

#### Issue: "Administrator privileges required"

**Solution:**
1. Right-click installer file
2. Select "Run as administrator"
3. Click "Yes" in User Account Control dialog
4. Proceed with installation

#### Issue: "Insufficient disk space"

**Solution:**
1. Free up disk space (need 1 GB minimum)
   - Delete temporary files: `Disk Cleanup`
   - Uninstall unnecessary programs
   - Move files to external storage
2. Restart installation
3. Or: Install to different drive (if available)

#### Issue: Antivirus blocking installation

**Solution:**
1. Temporarily disable antivirus
2. Run installer
3. Re-enable antivirus after installation
4. Add BendCore to exclusion list:
   - Windows Defender: Virus & threat protection
   - Settings → Exclusions → Add folders
   - Add: `C:\Program Files\BendCore\`

#### Issue: "Install failed, file not found"

**Solution:**
1. Re-download installer from official source
   - https://bendcore.io/download
2. Verify file integrity (check SHA256)
3. Delete partial installation:
   - `rmdir /s C:\Program Files\BendCore\v8.0`
4. Run installer again

#### Issue: Application crashes on startup

**Solution:**
1. Update GPU drivers:
   - NVIDIA: https://www.nvidia.com/Download/driverDetails.aspx
   - AMD: https://www.amd.com/en/technologies/amd-radeon-software
2. Install Visual C++ Redistributable 2022
   - Download from Microsoft
   - Run installer with admin privileges
3. Disable hardware acceleration:
   - BendCore → Settings → Graphics
   - Uncheck "Enable Hardware Acceleration"
   - Restart BendCore

#### Issue: License activation fails

**Solution:**
1. **Check internet connection**
   - Verify internet is working
   - Try accessing bendcore.io in browser

2. **Check license key**
   - Verify key is entered correctly
   - No extra spaces before/after key
   - Check for typos

3. **Use offline activation**
   - Activation → Offline Activation
   - Copy and process activation code manually
   - Import downloaded license file

4. **Check firewall**
   - Windows Firewall may block connection
   - Add BendCore.exe to firewall exceptions:
     - Settings → Firewall & network protection
     - Allowed apps → Allow another app
     - Browse to: `C:\Program Files\BendCore\v8.0\bin\BendCore.exe`

#### Issue: "OpenGL 3.3 not supported"

**Solution:**
1. Update GPU drivers (latest version)
2. Check GPU capabilities:
   - Download OpenGL Extensions Viewer
   - Verify OpenGL 3.3+ support
3. If unsupported GPU:
   - Contact support@bendcore.io
   - May need GPU upgrade for optimal performance

### Performance Issues

#### Slow startup

**Solution:**
1. Clear cache:
   - Delete: `%APPDATA%\BendCore\v8.0\cache\`
2. Disable startup plugins:
   - Settings → Plugins → Disable unnecessary plugins
3. Check disk space:
   - Ensure 1+ GB free on system drive
4. Check RAM usage:
   - Close other applications
   - Upgrade RAM if below 8GB

#### Sluggish performance during editing

**Solution:**
1. Reduce geometry cache size:
   - Settings → Performance
   - Reduce cache size to 50-100 MB
2. Disable real-time rendering:
   - Settings → Graphics
   - Turn off "Real-time Preview"
3. Upgrade GPU (if possible)
4. Close background applications

---

## Uninstallation

### Method 1: Control Panel

**1. Open Programs and Features**
   - Windows 10/11: Settings → Apps → Apps & features
   - Or: Control Panel → Programs → Programs and Features

**2. Find BendCore**
   - Locate "BendCore v8.0" in the list
   - Click to select it
   - Click "Uninstall" button

**3. Confirm Uninstallation**
   - Click "Yes" in confirmation dialog
   - Uninstall process begins
   - Takes 1-2 minutes

**4. Remove Remaining Files** (optional)
   - Delete: `C:\Program Files\BendCore\` (if not auto-deleted)
   - Delete shortcuts from Desktop and Start Menu
   - Delete: `%APPDATA%\BendCore\` (removes settings/cache)

### Method 2: Direct Installer

**1. Run Installer Again**
   - Run `BendCore-v8.0-setup.exe`
   - Click "Uninstall" button on first screen
   - Follow prompts to remove

### Method 3: Command Line

```batch
REM Using Setup Executable
BendCore-v8.0-setup.exe /uninstall

REM Using MSI
msiexec /x BendCore-v8.0.msi /quiet

REM Using Add/Remove Programs (WMIC)
wmic product where name="BendCore v8.0" call uninstall
```

### Complete Removal

To completely remove all traces of BendCore:

```batch
REM Remove installation directory
rmdir /s /q "C:\Program Files\BendCore"

REM Remove user data and settings
rmdir /s /q "%APPDATA%\BendCore"

REM Remove Start Menu shortcuts
del "%PROGRAMDATA%\Microsoft\Windows\Start Menu\Programs\BendCore*"

REM Remove Desktop shortcuts
del "%USERPROFILE%\Desktop\BendCore.lnk"

REM Remove uninstall registry entries (optional)
reg delete "HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\BendCore v8.0"
```

---

## Upgrade from Previous Versions

### From v7.x to v8.0

**Important: Back up your projects first!**

**Upgrade Steps:**

1. **Back up existing projects**
   ```
   Copy entire folder: C:\Users\[You]\Documents\BendCore
   To backup location: External drive or cloud storage
   ```

2. **Keep v7.x installed** (optional)
   - v8.0 installs alongside v7.x
   - Can coexist without conflicts
   - Or uninstall v7.x first if disk space is limited

3. **Install v8.0**
   - Follow installation steps above
   - Run installer with admin privileges

4. **Migrate Projects**
   - Open BendCore v8.0
   - File → Open → Select v7.x project
   - Auto-migration runs automatically
   - Save as v8.0 project when prompted

5. **Verify Migration**
   - Check all formulas and templates
   - Review design dimensions
   - Test import/export functions
   - Validate calculations and results

**Migration Compatibility:**
- ✓ All project files compatible
- ✓ Formulas automatically converted
- ✓ Templates preserved
- ✓ Custom settings maintained
- ✓ Zero data loss expected

**Breaking Changes in v8.0:**
- Formula API: `LegacyFormulaParser` removed
- Templates: `match()` returns `TemplateMatch` object
- OCCT: New initialization required
- See CHANGELOG.md for details

---

## Network & Firewall

### Firewall Configuration

**Windows Firewall:**
1. Settings → Firewall & network protection
2. Click "Allow an app through firewall"
3. Click "Change settings"
4. Click "Allow another app"
5. Browse to: `C:\Program Files\BendCore\v8.0\bin\BendCore.exe`
6. Click "Add"
7. Check both "Private" and "Public" columns

**Third-Party Firewalls:**
- Consult firewall documentation
- Add BendCore.exe to allowed applications
- Allow outbound HTTPS connections (port 443)

### Network Ports

BendCore requires these outbound connections:
- **HTTPS (443):** License activation, updates, analytics
- **Optional (80):** Documentation downloads

Inbound connections not required.

---

## Support & Resources

### Getting Help

- **Documentation:** https://docs.bendcore.io/v8.0
- **FAQ:** https://bendcore.io/faq
- **Community Forum:** https://forum.bendcore.io
- **GitHub Issues:** https://github.com/Do4ent/bendcore/issues
- **Email Support:** support@bendcore.io

### Reporting Issues

Include in bug reports:
- BendCore version: Help → About
- Windows version: `winver`
- System specs: Processor, RAM, GPU
- Steps to reproduce
- Expected vs actual behavior
- Log file: `%APPDATA%\BendCore\v8.0\logs\bendcore.log`

---

## Next Steps

After successful installation:

1. **Complete Setup Wizard**
   - Configure workspace and preferences
   - Load template library
   - Activate license (if available)

2. **Review Documentation**
   - User Guide: https://docs.bendcore.io/guide
   - Quick Start: https://bendcore.io/quickstart
   - Video Tutorials: https://bendcore.io/tutorials

3. **Create Your First Design**
   - File → New Project
   - Select template or start blank
   - Add formula or import CAD file

4. **Explore Features**
   - Formula Editor: Design → Add Formula
   - Template Library: Library → Templates
   - Import/Export: File → Import/Export
   - Analytics: Help → Analytics Dashboard

5. **Join Community**
   - GitHub Discussions: https://github.com/Do4ent/bendcore/discussions
   - Forum: https://forum.bendcore.io
   - Email: community@bendcore.io

---

## Conclusion

BendCore v8.0 is now installed and ready to use!

**For support and resources:**
- Official Website: https://bendcore.io
- GitHub Repository: https://github.com/Do4ent/bendcore
- Support Email: support@bendcore.io
- Documentation: https://docs.bendcore.io/v8.0

**Welcome to BendCore v8.0!**

---

**Installation Guide Version:** 1.0  
**Last Updated:** October 23, 2026  
**Applies to:** BendCore v8.0 and later
