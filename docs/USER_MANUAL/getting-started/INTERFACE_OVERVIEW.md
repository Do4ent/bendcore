# BendCore CAD Interface Overview

**Estimated Time:** 15 minutes  
**Difficulty:** Beginner  
**Prerequisites:** None

## Main Workspace Layout

When you open BendCore, you'll see five main areas:

```
┌─────────────────────────────────────────────────────────────────┐
│  Title Bar        |  File > Edit > View > Tools > Window > Help │
├─────────────────────────────────────────────────────────────────┤
│  🏠  📁  💾  ↩️  ↪️  |  View Controls  |  Search  |  🔍 ⚙️      │ Menu & Toolbar
├──────────────┬─────────────────────────────┬──────────────────┤
│              │                             │                  │
│              │                             │  Properties      │
│   Project    │      3D VIEWPORT            │  Panel           │
│   Tree       │      (Main editing area)    │  • General       │
│              │                             │  • Position      │
│              │                             │  • Material      │
│              │                             │  • Analysis      │
├──────────────┼─────────────────────────────┼──────────────────┤
│              │                             │                  │
│ • Tubes      │  Status Bar:                │  Browser Panel   │
│ • Sketches   │  Ready | Scale: 100% | ... │  • Libraries     │
│ • Analyses   │                             │  • Materials     │
│              │                             │  • Templates     │
└──────────────┴─────────────────────────────┴──────────────────┘
```

## Left Panel: Project Tree

The **Project Tree** shows your design hierarchy:

### Structure Example

```
📁 My_First_Bend (Project)
├── 📋 Sketch_01 (2D sketch)
├── 🔲 Tube_01 (3D tube component)
│   └── 🔶 Bend_01 (Bend feature)
├── 📊 Analysis_01 (FEA analysis)
└── 📄 Drawing_01 (Manufacturing drawing)
```

### Tree Icons

| Icon | Meaning |
|------|---------|
| 📁 | Project/Assembly |
| 📋 | Sketch (2D drawing) |
| 🔲 | Part (3D component) |
| 🔶 | Feature (Bend, fillet, etc.) |
| 📊 | Analysis/Simulation |
| 📄 | Drawing/View |
| 👁️ | Visibility toggle |
| 🔒 | Lock/Unlock |

### Common Tree Actions

| Action | How To Do It |
|--------|-------------|
| Select | Click on item in tree |
| Hide/Show | Click the 👁️ icon |
| Lock/Unlock | Click the 🔒 icon |
| Delete | Right-click > Delete |
| Rename | Right-click > Rename |
| Properties | Right-click > Properties |
| Duplicate | Right-click > Duplicate |

## Center: 3D Viewport

The **3D Viewport** is where you design and view your models.

### Viewport Controls

#### Navigation

| Action | How To Do It |
|--------|-------------|
| **Rotate** | Middle-mouse drag (or Shift + right-click) |
| **Pan** | Right-mouse drag |
| **Zoom In** | Scroll wheel up |
| **Zoom Out** | Scroll wheel down |
| **Fit View** | Press `V` or click Fit button |
| **Zoom Selection** | Press `Shift+Z` or click button |

#### View Presets

Click the view buttons in the toolbar:
- **Front** - View from front
- **Top** - View from above
- **Right** - View from right side
- **Isometric** - 3D perspective view
- **Custom** - Create custom views

#### Display Options

| Option | Icon | Purpose |
|--------|------|---------|
| Wireframe | 🗦 | Show edges only |
| Shaded | ⚪ | Show solid surfaces |
| Hidden Line | ⊞ | Show with transparency |
| Render | 🌟 | Photorealistic rendering |

### Selection in Viewport

| Action | How To Do It |
|--------|-------------|
| **Select** | Click on object |
| **Multi-select** | Hold `Ctrl` and click |
| **Deselect All** | Press `Esc` or click empty space |
| **Select All** | Press `Ctrl+A` |
| **Select Face** | Click on face in object |
| **Select Edge** | Click on edge in object |

## Right Panel: Properties & Browser

### Properties Panel (Upper Right)

Shows properties of the selected object:

#### Tabs

- **General** - Name, type, description
- **Position** - X, Y, Z coordinates
- **Rotation** - Pitch, yaw, roll angles
- **Dimensions** - Width, height, depth
- **Material** - Material type and properties
- **Analysis** - FEA parameters
- **Appearance** - Color, transparency, style

#### Editing Properties

1. Click on a property value
2. Type the new value
3. Press `Enter` to confirm
4. Model updates automatically (parametric)

### Browser Panel (Lower Right)

Access libraries and templates:

#### Tabs

- **Materials Library** - Browse available materials
- **Templates Library** - Pre-built tube templates
- **Components** - Standard parts and assemblies
- **Standards** - Industry standards (ANSI, ISO, etc.)
- **Symbols** - Annotation symbols for drawings

#### Using Libraries

1. Find the item you want in the browser
2. Drag it into the viewport to add it
3. Or right-click on item and select "Insert"

## Top Toolbar

The toolbar contains frequently-used commands:

### Standard Section (Left)
```
[🏠] [📁] [💾] [↩️] [↪️]
```
- New, Open, Save, Undo, Redo

### View Section (Center-Left)
```
[📦] [V] [🔍] [⊞] [⚪] [🗦]
```
- View presets, Fit, Zoom, Display modes

### Edit Section (Center)
```
[✂️] [📋] [🔗] [+] [-] [←→]
```
- Cut, Copy, Paste, Delete, Move, Flip

### Tools Section (Right)
```
[🔷] [🎨] [⚙️] [📊]
```
- Shape tools, Colors, Settings, Analysis

## Status Bar (Bottom)

Shows important information:

```
Ready | Scale: 100% | Snap: ON | Grid: 0.5mm | Selection: Tube_01 | Coordinates: X: 10.5 Y: 20.3 Z: 0
```

| Item | Meaning |
|------|---------|
| Ready | Current application status |
| Scale | Zoom level |
| Snap | Whether snap-to-grid is active |
| Grid | Current grid spacing |
| Selection | What's currently selected |
| Coordinates | Cursor position in 3D space |

## Menu Bar

### File Menu
- **New** - Create new design
- **Open** - Open existing file
- **Save** - Save current work
- **Export** - Export to STEP, IGES, DWG, PDF
- **Import** - Import designs
- **Recent** - Recently opened files
- **Exit** - Close application

### Edit Menu
- **Undo/Redo** - Reverse/repeat actions
- **Cut/Copy/Paste** - Clipboard operations
- **Select All** - Select all objects
- **Delete** - Remove selected
- **Preferences** - User settings

### View Menu
- **View Presets** - Front, top, right, isometric
- **Display** - Wireframe, shaded, render modes
- **Panels** - Show/hide interface panels
- **Grid** - Toggle and configure grid
- **Compass** - Show 3D orientation compass
- **Fullscreen** - Toggle fullscreen mode

### Tools Menu
- **Sketch** - Enter sketch editing mode
- **Create Tube** - Create tube from sketch
- **Analysis** - FEA and simulation tools
- **Assembly** - Multi-part assembly tools
- **Templates** - Template manager
- **Converters** - File format conversion
- **Plugins** - Manage extensions

### Window Menu
- **Arrange** - Tile or cascade windows
- **Workspaces** - Save/load custom layouts
- **Reset Layout** - Return to default layout

### Help Menu
- **Contents** - Full documentation (F1)
- **Tutorials** - Interactive tutorials
- **API Documentation** - For scripting
- **About** - Version and system info
- **Contact Support** - Get help

## Customizing the Interface

### Showing/Hiding Panels

1. Go to **View > Panels**
2. Toggle panels on/off
3. Or right-click on panel title bar to show/hide

### Rearranging Panels

1. Click and hold on panel title bar
2. Drag panel to new location
3. Release to place
4. Double-click title to dock/undock

### Saving Your Layout

1. Arrange panels as desired
2. Go to **Window > Workspaces > Save Current Layout**
3. Name your workspace (e.g., "Sketching Setup")
4. Later, select it from **Window > Workspaces** to restore

### Keyboard Shortcuts

Press `F1` to open Help, then search for "Keyboard Shortcuts" to see full list.

Common shortcuts:
| Shortcut | Action |
|----------|--------|
| `Ctrl+N` | New |
| `Ctrl+O` | Open |
| `Ctrl+S` | Save |
| `Ctrl+Z` | Undo |
| `Ctrl+Y` | Redo |
| `Delete` | Delete selected |
| `Escape` | Deselect all |
| `V` | Fit view |
| `E` | Edit sketch |
| `H` | Hide selected |
| `Shift+H` | Show hidden |

## Interface Tips & Tricks

💡 **Two-Monitor Setup:** Drag a panel to your second monitor for more space

💡 **Collapse Unused Panels:** Click the minus (-) icon on panel title bars to collapse

💡 **Search Functions:** Press `Ctrl+/` to search for commands

💡 **Workspace Presets:**
   - **Sketching:** Large sketch area
   - **Assembly:** Properties and tree panels large
   - **Analysis:** Analysis panel prominent
   - **Drawing:** Full viewport for drawing views

💡 **Quick Access:** Customize your toolbar with Tools > Customize Toolbar

## Dark Mode & Themes

### Enabling Dark Mode

1. Go to **Edit > Preferences**
2. Select **Appearance**
3. Choose **Dark Theme**
4. Click **Apply**

### Available Themes
- Light (default)
- Dark
- High Contrast
- Professional Gray

## Accessibility Features

### High Contrast Mode
- **Edit > Preferences > Accessibility > High Contrast**
- Better for color-blind users

### Large UI Text
- **Edit > Preferences > Accessibility > Large Text**
- Increases all text sizes

### Screen Reader Support
- Full keyboard navigation supported
- Screen reader compatible dialogs
- See Help > Accessibility for details

## Troubleshooting Interface Issues

### Panel Disappeared

1. Go to **View > Panels**
2. Check the panel name
3. Click to show it

### Toolbar Buttons Missing

1. Right-click on toolbar
2. Select **Customize Toolbar**
3. Drag buttons back to toolbar

### Reset to Default Layout

1. Go to **Window > Reset Layout**
2. Click **Yes** to confirm
3. Interface returns to default arrangement

---

**Next:** [Keyboard Shortcuts Reference](KEYBOARD_SHORTCUTS.md)
