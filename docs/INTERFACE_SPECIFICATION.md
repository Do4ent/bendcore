# BendCore Interface Specification

**Document Status:** Recovered from design sessions (2026-10-18 to 2026-10-22)  
**Last Updated:** 2026-10-26

## Table of Contents

1. [Phase Development Timeline](#phase-development-timeline)
2. [Architecture Overview](#architecture-overview)
3. [Icon Themes](#icon-themes)
4. [Keyboard Shortcuts](#keyboard-shortcuts)
5. [Interface Layout](#interface-layout)
6. [Design Principles](#design-principles)

---

## Phase Development Timeline

### M1-M20 Interface Development (Oct-Nov 2026)

#### **M1-M3: Core UI Foundation**
- **M1:** Command Manager ribbon structure, 3-panel layout
- **M2:** Project tree with context menus, Properties panel
- **M3:** 3D viewport with ViewCube, standard views, display modes

#### **M4-M6: Sketch & Tube Tools**
- **M4:** 2D/3D Sketch editor with dimension/constraint tools
- **M5:** Tube creation wizard, tube properties editor
- **M6:** Bend analysis panel, visualization of constraints

#### **M7-M9: Assembly & Manufacturing**
- **M7:** Assembly panel, constraint editor, component tree
- **M8:** Manufacturing analysis UI, toolpath preview
- **M9:** Bend table editor (L-R-A format), machine profile selector

#### **M10-M12: Analysis & Reporting**
- **M10:** FEA results visualization (stress, deformation)
- **M11:** Report generation UI, drawing export options
- **M12:** Real-time collaboration panel, version control UI

#### **M13-M15: Advanced Features**
- **M13:** Parameter table editor, formula editor
- **M14:** Template manager, library browser
- **M15:** Custom icon themes loader, keyboard shortcut customizer

#### **M16-M18: Polish & Optimization**
- **M16:** Search/command palette (Ctrl+/)
- **M17:** Settings/Preferences dialog (all user options)
- **M18:** Dark mode refinement, accessibility improvements

#### **M19-M20: Production Readiness**
- **M19:** Help system, tooltips, context-sensitive help (F1)
- **M20:** Analytics integration, crash reporting, user feedback widget

#### **M21: UI Complete (Post-Launch)**
- Full UI implementation with all 20 phases integrated
- Real-time monitoring dashboard
- Agent-based parallel development tracking

---

---

## Architecture Overview

BendCore interface is designed after **SolidWorks/Inventor** standard CAD patterns:

### Three-Panel Layout

```
┌─────────────────────────────────────────────────────┐
│  Menu: File | Edit | View | Tools | Window | Help   │
├──────────────┬─────────────────────┬────────────────┤
│              │                     │                │
│  Project     │                     │  Properties    │
│  Tree        │   3D Viewport       │  Panel         │
│  (Left)      │   (Center)          │  (Right)       │
│              │                     │                │
│              │   [ViewCube]        │                │
│              │                     │                │
├──────────────┼─────────────────────┼────────────────┤
│              Status Bar: Ready | Scale: 100% | ...  │
└─────────────────────────────────────────────────────┘
```

### Ribbon Toolbar (CommandManager)

- **Context-sensitive tabs** by document type:
  - Part Modeling
  - Assembly
  - Drawing
  
- **Tab-based organization** with command groups
- **11 command categories** (see Icon Themes section)

---

## Icon Themes

### Three-Layer Architecture

```
Layer 1: Base Icon Set (all commands)
    ↓
Layer 2: Theme Icon Set (overrides/extends layer 1)
    ↓
Layer 3: Color Palette (for light/dark modes)
```

### Color Roles in SVG

Each icon uses placeholders for dynamic coloring:

```xml
<svg>
  <path fill="{{category}}" stroke="{{outline}}"/>
  <circle fill="{{accent}}" opacity="{{background}}"/>
</svg>
```

| Role | Usage |
|------|-------|
| `{{outline}}` | Icon stroke/border |
| `{{category}}` | Main fill color (category-specific) |
| `{{accent}}` | Highlight/emphasis |
| `{{background}}` | Background shape |

### Built-in Themes

#### 1. Цветная (Colored) - Default

11 command categories with distinct colors:

| Category | Light | Dark | Purpose |
|----------|-------|------|---------|
| File | #2563eb | #60a5fa | New, Open, Save |
| Edit | #7c3aed | #a78bfa | Undo, Redo, Cut, Copy, Paste |
| View | #0891b2 | #06b6d4 | View presets, Display modes |
| Sketch | #059669 | #10b981 | Lines, Circles, Dimensions |
| Tube | #dc2626 | #ef4444 | Create tube, Set diameter |
| Bends | #ea580c | #fb923c | Set radius, angles |
| Analysis | #7c3aed | #a78bfa | Check gaps, collisions |
| Output | #0891b2 | #06b6d4 | Export, Print |
| Helpers | #6b7280 | #9ca3af | Measure, Markup |
| Assembly | #059669 | #10b981 | Assembly, Constraints |
| Tools | #6b7280 | #9ca3af | Settings, Templates |

#### 2. Монохромная (Monochrome)

Single-color palette with variation for emphasis:

| Element | Light | Dark |
|---------|-------|------|
| Normal | #1f2937 | #f3f4f6 |
| Emphasis | #374151 | #e5e7eb |
| Disabled | #9ca3af | #6b7280 |

### Theme Package Format

```
theme-package.zip/
├── manifest.json
│   {
│     "name": "Theme Name",
│     "version": "1.0",
│     "description": "Description",
│     "colors": {
│       "light": { "outline": "#...", "category": "#...", ... },
│       "dark": { "outline": "#...", "category": "#...", ... }
│     }
│   }
└── icons/
    ├── command-name-1.svg
    ├── command-name-2.svg
    └── ...
```

### Theme Package Validation

✅ **Allowed:**
- SVG files with color placeholders
- Standard SVG elements (path, circle, rect, etc.)
- Style attributes without external resources
- Max 64KB per SVG file

❌ **Forbidden:**
- Scripts (`<script>`)
- Event handlers (`on*` attributes)
- External links (http://, https://, javascript:)
- `<foreignObject>` elements
- External stylesheets (@import, url())

---

## Keyboard Shortcuts

### Shortcut Sets (Keymaps)

BendCore supports multiple keyboard layouts for users transitioning from other CAD systems:

- **SolidWorks** (default when available)
- **Inventor**
- **NX**
- **Creo**
- **CATIA**
- **Custom/User-defined**

### Normalization Rules

All shortcuts are normalized to a standard format:

```
Input:     "ctrl+shift+z"  →  Output: "Ctrl+Shift+Z"
Input:     "shift+f5"      →  Output: "Shift+F5"
Input:     "alt+m"         →  Output: "Alt+M"
```

Modifiers are ordered: **Ctrl** → **Shift** → **Alt**

### Snap Modes (Привязки)

| Shortcut | Function | Context |
|----------|----------|---------|
| **F3** | Toggle all snaps on/off | Global |
| **Ctrl** | Temporarily disable snaps | While held |
| **Alt+E** | Snap to endpoint | 3D Sketch |
| **Alt+M** | Snap to midpoint | 3D Sketch |
| **Alt+C** | Snap to center | 3D Sketch |
| **Alt+V** | Snap to vertex | 3D Sketch |
| **Alt+T** | Snap to bend tangent | Tube mode |
| **Alt+A** | Snap to axis | 3D Sketch |
| **Alt+O** | Snap to port | Assembly |
| **Alt+I** | Snap to intersection | 3D Sketch |
| **Alt+P** | Snap perpendicular | 3D Sketch |
| **Alt+N** | Snap nearest on object | 3D Sketch |
| **Shift+RMB** | Snap options menu | Any |
| **Tab** | Cycle snap targets | 3D Sketch |

### Shortcut Contexts

Shortcuts are context-sensitive:

- **Global** - Available everywhere
- **3D Path Sketch** - Path editing mode
- **2D Sketch** - Flat sketch editing
- **Tube Mode** - Tube creation/editing
- **Assembly** - Multi-body mode
- **Drawing** - 2D view/annotation

### Customization

Users can:
- Override shortcuts via **Edit → Preferences → Keyboard**
- Load shortcut set from another CAD system
- Export/import custom shortcut profiles
- Reset to defaults per category

### Implementation (keymap.py)

```python
# Load predefined set
keyset = load_keyset("SolidWorks")

# Override specific shortcuts
custom = customize(keyset, {
    "Ctrl+T": "my_command",
    "F7": None,  # unbind F7
})

# Query
command = keyset.command_for("Ctrl+Z")  # → "undo"
```

---

## Interface Layout

### Top Menu Bar

```
File | Edit | View | Tools | Window | Help
```

### Left Panel: Project Tree

**Structure:**
```
📁 Project
├── 📋 Sketch_01 (2D Sketch)
├── 🔲 Tube_01 (3D Component)
│   └── 🔶 Bend_01 (Feature)
├── ⚡ Weld_001 (Feature)
├── 🔗 Assembly_001 (Sub-assembly)
├── 📊 Analysis_01 (FEA Analysis)
└── 📄 Drawing_01 (Technical Drawing)
```

**Icons:**
- 📁 Project/Assembly
- 📋 Sketch (2D)
- 🔲 Part (3D component)
- 🔶 Feature (Bend, weld, cut)
- 📊 Analysis/Simulation
- 📄 Drawing/View
- 👁️ Visibility toggle
- 🔒 Lock/Unlock

**Actions:**
- Left-click: Select
- Right-click: Context menu (Copy, Delete, Rename, Properties)
- Drag-drop: Reorder
- Double-click: Edit/expand

### Center: 3D Viewport

**Features:**
- Three.js WebGL rendering
- ViewCube for orientation
- Real-time model preview
- Display modes (Shaded, Wireframe, Hidden Line, Render)
- Standard views (Front, Top, Right, Isometric, Custom)

**Interaction:**
- **Middle-mouse drag**: Rotate view
- **Right-mouse drag**: Pan
- **Scroll**: Zoom
- **"V" key**: Fit all in view
- **Shift+Z**: Zoom to selection

### Right Panel: Properties

**Structure:**
```
GENERAL
├── Name: [editable]
├── Type: [display]
└── Description: [editable]

GEOMETRY
├── Diameter: 12.7 mm
├── Thickness: 1.24 mm
└── Length: 152.4 mm

MATERIAL
├── Material: Stainless 304
├── Weight: 0.354 kg
└── Cost: $42.50

ANALYSIS
└── Status: ✓ Complete
```

**Updates:**
- Real-time as model changes
- Click to edit values
- Press Enter to confirm
- Live validation (red for errors)

### Status Bar

```
Ready | Scale: 100% | Snap: ON | Grid: 0.5mm | Selection: Tube_01 | Coordinates: X: 10.5 Y: 20.3 Z: 0
```

---

## Design Principles

### 1. CAD-Standard Interface
- Follows SolidWorks, Inventor, NX patterns
- Familiar to engineers from other systems
- Three-panel layout is industry standard
- **Implemented in:** M1-M3 (Core Foundation)

### 2. Parametric Everything
- All dimensions linked to parameters
- Change one value → model updates
- Formulas supported (R = 2·D)
- **Implemented in:** M4-M5 (Sketch & Tube Tools)

### 3. Real Manufacturing Constraints
- Interface reflects actual machine limitations
- Minimum straight segment enforced
- Bend radius validated against equipment
- Spring-back compensation built-in
- **Implemented in:** M8-M9 (Manufacturing Panel)

### 4. Keyboard-Centric
- Every command has shortcut
- Shortcuts match SolidWorks by default
- Users from other CAD can match their habits
- Snap modes are context-sensitive
- **Implemented in:** M15 (Keyboard Customizer), M16 (Command Palette)

### 5. Multiple Language Support
- English and Russian UI
- Localizable strings in separate files
- Keyboard layout awareness
- **Implemented in:** All phases with language-agnostic code

### 6. Light & Dark Modes
- All colors work in both modes
- Automatic contrast adjustment
- High-contrast mode for accessibility
- No hardcoded colors in components
- **Implemented in:** M18 (Polish & Optimization)

### 7. Autonomous Multi-Agent Development
- 20+ parallel agents working on 20 phases simultaneously (M1-M20)
- Each agent handles 4-7 GAP tasks per phase
- Real-time monitoring dashboard (M21)
- Auto-launch next phase upon completion
- **Development approach:** Oct 26-27, 2026

### 8. Transition from Other CAD Systems
- Users from SolidWorks get matching shortcuts
- Users from Inventor, NX, Creo, CATIA get their layouts
- Customizable keyboard maps per user
- Familiar ribbon structure with tube-specific categories
- **Implemented in:** M15 (Customizer), M17 (Settings)

---

## Phase-to-Component Mapping

### UI Components by Phase

| Phase | Component | Purpose | Status |
|-------|-----------|---------|--------|
| M1 | CommandManager (Ribbon) | Organize 300+ commands by category | In Progress |
| M1 | MainWindow Layout | 3-panel container | In Progress |
| M2 | ProjectTree | Hierarchical model view | In Progress |
| M2 | ContextMenu | Right-click actions | In Progress |
| M3 | Viewport3D | WebGL rendering with Three.js | In Progress |
| M3 | ViewCube | Orientation control | In Progress |
| M4 | SketchEditor | 2D/3D constraint sketcher | In Progress |
| M5 | TubeWizard | Step-by-step tube creation | In Progress |
| M6 | BendAnalysisPanel | Constraint violation display | In Progress |
| M7 | AssemblyPanel | Component tree & constraints | In Progress |
| M8 | ManufacturingPanel | Toolpath & analysis results | In Progress |
| M9 | BendTableEditor | L-R-A table (editable) | In Progress |
| M10 | FEAResultsViewer | Stress/deformation visualization | Planned |
| M11 | ReportGenerator | Drawing & export options | Planned |
| M12 | CollaborationPanel | Real-time editing indicators | Planned |
| M13 | ParameterTable | Design variable editor | Planned |
| M14 | LibraryBrowser | Template & material library | Planned |
| M15 | ThemeManager | Load custom icon themes | Planned |
| M15 | ShortcutCustomizer | Remap keyboard shortcuts | Planned |
| M16 | CommandPalette | Ctrl+/ search interface | Planned |
| M17 | PreferencesDialog | All user settings | Planned |
| M18 | DarkModeTheme | Theme refinement & polish | Planned |
| M19 | HelpSystem | Integrated help & tooltips | Planned |
| M20 | AnalyticsWidget | Usage tracking & feedback | Planned |

### React Component Architecture

```
<MainApp>
├── <MenuBar>
├── <CommandManager (Ribbon)>
├── <MainContent>
│   ├── <ProjectTree>      [M2]
│   ├── <Viewport3D>       [M3]
│   └── <RightPanel>
│       ├── <PropertiesPanel>    [M1-M20]
│       ├── <AnalysisPanel>      [M6+]
│       └── <CollaborationPanel> [M12]
├── <StatusBar>
└── <ModalDialogs>
    ├── <TubeWizard>        [M5]
    ├── <PreferencesDialog> [M17]
    └── <HelpDialog>        [M19]
```

### GAP Tasks by Component

Each phase contains 4-7 GAP tasks (Group of Aligned Projects):

**Example - M1 (Core UI Foundation):**
- **GAP-001:** CommandManager ribbon layout (4-7 tasks)
- **GAP-002:** MainWindow container & layout
- **GAP-003:** Menu bar with File/Edit/View/Tools/Window/Help
- **GAP-004:** Integration tests for M1 components
- **GAP-005:** Documentation & keyboard shortcut mapping
- **GAP-006:** Dark mode color palette
- **GAP-007:** Accessibility compliance

**Each GAP task = 1 autonomous agent**

---

## Related Documents

- [INTERFACE_OVERVIEW.md](./USER_MANUAL/getting-started/INTERFACE_OVERVIEW.md) - User guide
- [KEYBOARD_SHORTCUTS.md](./GUIDES/KEYBOARD_SHORTCUTS.md) - Complete shortcut reference
- [COMPONENTS.md](../COMPONENTS.md) - React component library
- [README.md](../README.md) - Project status & phases

---

**Document Version:** 3.0  
**Status:** Updated with M1-M20 phase information  
**Last Updated:** 2026-10-26 18:50 UTC+1  
**Development:** Parallel autonomous agent execution (M1-M20)  
**Next Review:** Phase completion checkpoints

---

## Current Development Status (2026-10-26)

```
Phase Execution Timeline:
M1-M3:   Core Foundation        [████████████░░░░░░░░] 60%
M4-M6:   Sketcher & Tube        [████████░░░░░░░░░░░░] 40%
M7-M9:   Assembly & Mfg         [██░░░░░░░░░░░░░░░░░░] 10%
M10-M12: Analysis & Reporting   [░░░░░░░░░░░░░░░░░░░░] 0%
M13-M15: Advanced Features      [░░░░░░░░░░░░░░░░░░░░] 0%
M16-M18: Polish & Optimization  [░░░░░░░░░░░░░░░░░░░░] 0%
M19-M20: Production Readiness   [░░░░░░░░░░░░░░░░░░░░] 0%
M21:     UI Complete (M1-M20)   [░░░░░░░░░░░░░░░░░░░░] 0%
```

**Agents Active:** 30-50 parallel tasks  
**Test Coverage:** 30+ unit tests per agent  
**Dashboard:** Real-time progress tracking in `agent-dashboard.html`
