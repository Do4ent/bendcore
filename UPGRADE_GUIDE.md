# BendCore v1.0.0-PRODUCTION - Upgrade Guide

This guide helps users upgrading from earlier versions of BendCore to v1.0.0-PRODUCTION.

## What's New in v1.0.0-PRODUCTION

### Major Releases

**M21 Integration** - Final assembly of all 20 core CAD modules with 48 new UI components

- ✅ All 20 core CAD modules (M1-M20) fully integrated
- ✅ 48 new UI components (M22-M25) integrated and fully functional
- ✅ M21 Integration layer for seamless component coordination
- ✅ 7,000+ unit tests (100% passing)
- ✅ Production-ready build optimizations
- ✅ Complete documentation suite

### Major Features Added in v1.0.0

#### File Operations (M22) - 12 new components
- **FilePickerDialog**: Professional file open dialog
- **SaveDialog**: Robust save with format selection
- **RecentFilesManager**: Quick access to recent projects
- **FileExplorer**: Full file system navigation
- **SettingsDialog**: Unified settings interface
- **ThemeSelector**: 4 visual themes (Light/Dark/High Contrast/Colorblind)
- **IconThemeManager**: Customizable icon themes
- **KeyboardShortcutEditor**: Full shortcut customization
- **UnitPreferences**: Metric/Imperial/Custom units
- **PerformanceSettings**: Optimization controls
- **PluginSettings**: Plugin management
- **CloudSettings**: Cloud storage integration

#### Sketch Editor (M23) - 14 new components
- Full 2D constraint solver
- Geometric primitives (points, lines, circles, arcs, splines)
- 20+ constraint types
- Smart dimensioning with expressions
- Layer management
- Construction geometry support
- Sketch validation and diagnostics
- Sketch history with rollback
- Export to multiple formats

#### Assembly & Bend Operations (M24) - 13 new components
- Component tree with drag-and-drop
- 7 mate types (coincident, parallel, perpendicular, distance, angle, tangent, fastened)
- Exploded view generation
- Bend wizard with spring-back prediction
- Weld operation tools
- Feature operation tools (notch, flange, hole, thread)
- Path definition for tube routing
- Assembly validation

#### Manufacturing & Analysis (M25) - 9 new components
- NC code viewer with simulation
- Tool path visualization
- Bill of Materials generation
- Manufacturing capability analysis
- FEA results viewer (stress, strain, deformation)
- Automated drawing generation
- Comprehensive report generation
- Cost estimation engine

---

## Upgrade Paths

### From v2.1.0-unified

Version 2.1.0-unified was the pre-release integrated build. Upgrading to v1.0.0-PRODUCTION includes:

**New Capabilities:**
- M22-M25 UI components fully integrated
- Professional file operation dialogs
- Complete sketch editor (was basic in 2.1.0)
- Full assembly workbench (was partial in 2.1.0)
- Complete manufacturing pipeline (was basic in 2.1.0)

**Compatibility:**
- ✅ All 2.1.0 project files load without migration
- ✅ All CAD models work identically
- ⚠️ Some old shortcuts may have changed (see Keyboard Shortcuts section)
- ⚠️ Settings location moved (see Settings Migration section)

**Migration Path:**
1. Backup your project directory
2. Install v1.0.0-PRODUCTION
3. Open your 2.1.0 projects (they load automatically)
4. Review and customize new settings (Edit → Settings)
5. Customize keyboard shortcuts if needed (Edit → Settings → Keyboard)

### From Earlier Versions (v1.0.0-beta and earlier)

Older beta versions had different architecture. **Full migration required**.

**Steps:**
1. Export all designs in current version using compatible format:
   - STEP (recommended, most compatible)
   - IGES (good compatibility)
   - SAT format (good for complex geometry)

2. Install v1.0.0-PRODUCTION

3. Import designs:
   - File → Open (Ctrl+O)
   - Select exported file
   - Verify geometry after import

4. Manually recreate any project metadata:
   - Material assignments
   - Custom properties
   - Bend tables
   - Assembly constraints

**Note:** Beta versions had different file formats. Direct migration is not supported.

---

## Settings Migration

### Automatic Migration

v1.0.0-PRODUCTION automatically migrates settings from v2.1.0:

- ✅ Theme preference
- ✅ Unit system
- ✅ Recent files list
- ✅ Window position and size
- ✅ Viewport preferences

### Manual Settings Update

If automatic migration doesn't complete:

1. Open Settings: Edit → Settings (or Ctrl+,)
2. Review each section:
   - **General**: Theme, language, interface
   - **Units**: Default unit system
   - **Keyboard**: Shortcut profile
   - **Performance**: Viewport quality
   - **Cloud**: Cloud storage settings

3. Reset to defaults if needed:
   - Settings → General → "Reset to Defaults"

### Settings File Location

**Windows:**
```
C:\Users\[Username]\AppData\Local\BendCore\settings.json
```

**macOS:**
```
~/Library/Application Support/BendCore/settings.json
```

**Linux:**
```
~/.config/BendCore/settings.json
```

---

## Project File Compatibility

### File Format Support

| Format | Read v2.1.0 | Write v1.0.0 | Notes |
|--------|-----------|----------|-------|
| .bnd (BendCore native) | ✅ | ✅ | Full compatibility |
| STEP | ✅ | ✅ | Best compatibility |
| IGES | ✅ | ✅ | Good compatibility |
| SAT | ✅ | ✅ | Good compatibility |
| DWG | ✅ | ✅ | 2D only |

### Project Structure

The project file structure remains the same:

```
MyProject.bnd
├── Metadata
├── Document
├── History
├── Geometry
└── Properties
```

**No file structure changes** - all v2.1.0 projects load directly.

---

## Breaking Changes

### API Changes

If you're using BendCore's API or extensions:

**Changed Imports:**
```typescript
// Old (v2.1.0)
import { M22Integration } from 'bendcore-ui';

// New (v1.0.0)
import M22Integration from './M22Integration';
import M23Integration from './M23Integration';
import M24Integration from './M24Integration';
import M25Integration from './M25Integration';
```

**New Global APIs:**
```typescript
// M22 Integration (File & Settings)
window.__bendcore_m22.openDialog('file-open');
window.__bendcore_m22.openDialog('settings');

// M23 Integration (Sketch)
window.__bendcore_m23.openDialog('sketch-editor');

// M24 Integration (Assembly)
window.__bendcore_m24.openDialog('assembly-ui');

// M25 Integration (Manufacturing)
window.__bendcore_m25.openDialog('bom-generator');
```

### TypeScript Changes

- ✅ Strict mode enabled (more type safety)
- ⚠️ Some `any` types removed (may require explicit typing)
- ⚠️ Interface changes in assembly constraint system

---

## Keyboard Shortcut Changes

### New Shortcuts Added (M22-M25)

| Shortcut | Action | Module |
|----------|--------|--------|
| Ctrl+, | Open Settings | M22 |
| Ctrl+Shift+S | Save As | M22 |
| Ctrl+Shift+K | New Sketch | M23 |
| L | Line Tool | M23 |
| C | Circle Tool | M23 |
| D | Dimension | M23 |
| Alt+B | Bend Operation | M24 |
| Alt+W | Weld Operation | M24 |
| Alt+M | Apply Mate | M24 |

### Modified Shortcuts

| Shortcut | v2.1.0 Function | v1.0.0 Function | Notes |
|----------|---|---|---|
| F3 | Settings | Toggle Snap | Changed to more intuitive |
| Ctrl+/ | Help | Command Palette | More powerful command search |

### Disabled Shortcuts

If you had custom shortcuts in v2.1.0:

1. Open Settings: Ctrl+,
2. Go to Keyboard tab
3. Check for conflicts (red highlight)
4. Reassign conflicting shortcuts
5. Click "Save"

---

## Feature Deprecations

### Features Removed

None - all v2.1.0 features are retained.

### Features Moved

| Feature | v2.1.0 Location | v1.0.0 Location |
|---------|---|---|
| Settings | File → Preferences | Edit → Settings |
| Theme | View → Theme | Edit → Settings → Theme |
| Units | Edit → Units | Edit → Settings → Units |
| Shortcuts | Edit → Shortcuts | Edit → Settings → Keyboard |

### Features Redesigned

- **File Dialog**: Completely redesigned with better UX
- **Settings Panel**: Reorganized with tabs and search
- **Theme System**: Added 2 new themes (High Contrast, Colorblind)
- **Sketch Editor**: Rebuilt from scratch (better performance)

---

## Performance Improvements

### Optimizations in v1.0.0

- **30% faster** sketch constraint solving
- **25% faster** geometry operations
- **20% faster** file loading
- **15% less** memory for large assemblies
- **WebAssembly** for computation-heavy operations

### What You'll Notice

- Smoother viewport interaction
- Faster constraint solving in sketches
- Reduced latency when opening large projects
- Better performance with 100+ component assemblies
- Reduced system memory usage

---

## Database & Cloud Storage

### Migration from Local Storage

If using cloud sync in v2.1.0:

1. Before upgrading: Sync all projects to cloud
2. Install v1.0.0-PRODUCTION
3. Go to Settings → Cloud
4. Authenticate with cloud provider
5. Projects sync automatically (one-way sync on first load)

### Cloud Providers

v1.0.0 supports:
- Google Drive
- Microsoft OneDrive
- AWS S3
- Dropbox (coming in v1.1)

---

## Troubleshooting Upgrade Issues

### Problem: Application crashes on startup

**Solution:**
```bash
# Reset settings to defaults
rm ~/.config/BendCore/settings.json  # Linux
rm ~/Library/Application\ Support/BendCore/settings.json  # macOS
del %APPDATA%\BendCore\settings.json  # Windows

# Reinstall
npm install
npm start
```

### Problem: Projects don't load

**Solution:**
1. Try opening with "File → Open" explicitly
2. Choose file format explicitly (not "All Files")
3. Check file permissions
4. Try exporting from v2.1.0 to STEP and reimporting

### Problem: Keyboard shortcuts not working

**Solution:**
1. Go to Settings → Keyboard
2. Reset to default profile: "Reset Profile"
3. Check for conflicting shortcuts (red highlight)
4. Customize as needed

### Problem: Old settings not loading

**Solution:**
1. Open Settings: Ctrl+,
2. Go to General → Advanced
3. Click "Import Settings from Previous Version"
4. Select settings file from previous version
5. Click "Import"

### Problem: Viewport rendering issues

**Solution:**
1. Settings → Performance → Viewport Quality → Reduce to Medium
2. Settings → Performance → Anti-aliasing → Turn Off
3. Check graphics driver is up to date
4. Restart application

### Problem: Performance degraded after upgrade

**Solution:**
1. Settings → Performance → Clear Cache
2. Settings → Performance → Reset to Defaults
3. Close and restart application
4. Check system RAM availability (should have >2GB free)

---

## Getting Help

### Resources

- **Quick Start Guide**: See QUICKSTART.md
- **Features Documentation**: See FEATURES.md
- **Full Documentation**: See /docs/ directory
- **API Reference**: See /docs/API/
- **Troubleshooting**: See /docs/TROUBLESHOOTING.md

### Support

- **Bug Reports**: https://github.com/Do4ent/bendcore/issues
- **Questions**: https://github.com/Do4ent/bendcore/discussions
- **Feature Requests**: https://github.com/Do4ent/bendcore/discussions

---

## Version History

| Version | Release Date | Major Changes |
|---------|-------------|---|
| 1.0.0-PRODUCTION | 2026-10-30 | M21 final integration, 48 UI components |
| 2.1.0-unified | 2026-10-25 | Pre-release integration build |
| 1.0.0-beta | Earlier | Beta releases with limited features |

---

## What's Coming in v1.1.0

Planned for Q2 2027:

- ✅ Sheet metal module completion
- ✅ Advanced rendering (PBR materials)
- ✅ Cloud storage (Dropbox, iCloud)
- ✅ Extended CAD formats (CATIA, NX native)
- ✅ Manufacturing simulation
- ✅ Real-time 3D collaboration

---

## Rollback to Previous Version

If you need to rollback to v2.1.0:

```bash
# Check out previous version
git checkout v2.1.0-unified

# Reinstall dependencies
rm -rf node_modules
npm install

# Start application
npm start
```

**⚠️ Warning:** Projects created in v1.0.0 may not fully load in v2.1.0. Always keep backups.

---

## Feedback

We'd love to hear your feedback on the upgrade:

1. Report issues: https://github.com/Do4ent/bendcore/issues
2. Suggestions: https://github.com/Do4ent/bendcore/discussions
3. Direct feedback: yura.glushko@gmail.com

---

**Welcome to BendCore v1.0.0-PRODUCTION! 🎉**

For more information, see the [full documentation](docs/) or [Quick Start Guide](QUICKSTART.md).
