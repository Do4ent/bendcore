# BendCore v1.0.0 - Quick Start Guide

Welcome to BendCore CAD! This guide will help you get up and running with the professional tube bending design system.

## 📋 Table of Contents

1. [Installation](#installation)
2. [First Launch](#first-launch)
3. [Basic Workflow](#basic-workflow)
4. [Key Features](#key-features)
5. [Keyboard Shortcuts](#keyboard-shortcuts)
6. [Tips & Tricks](#tips--tricks)

---

## Installation

### System Requirements

- **Operating System**: Windows 10+, macOS 10.14+, or Linux (Ubuntu 18.04+)
- **RAM**: 4GB minimum, 8GB recommended
- **Storage**: 2GB available space
- **GPU**: 2GB VRAM (1GB minimum)
- **Node.js**: 18.0+ (for development/building)

### Installation Steps

#### From Release Package

1. Download the latest release from [GitHub Releases](https://github.com/Do4ent/bendcore/releases)
2. Extract the archive to your desired location
3. Run `npm install` to install dependencies
4. Run `npm start` to launch the application

#### From Source

```bash
# Clone the repository
git clone https://github.com/Do4ent/bendcore.git
cd bendcore

# Install dependencies
npm install

# Start development server
npm run dev

# In another terminal, start the application
npm start
```

#### Docker Installation

```bash
# Build Docker image
docker build -t bendcore:latest .

# Run with Docker
docker run -p 3000:3000 bendcore:latest

# Access at http://localhost:3000
```

---

## First Launch

### Initial Setup

When you launch BendCore for the first time:

1. **Welcome Screen**: Choose your interface theme
   - Light (Recommended for daytime use)
   - Dark (Recommended for reduced eye strain)
   - High Contrast (Recommended for accessibility)
   - Colorblind-Friendly (For red/green color blindness)

2. **Unit System Setup**: Select your preferred unit system
   - Metric (mm, kg, °C) - Default
   - Imperial (inches, pounds, °F)
   - Custom (Define your own)

3. **Keyboard Shortcut Profile**: Choose your shortcut style
   - Default (BendCore standard)
   - Vim (For Vim users)
   - Emacs (For Emacs users)

4. **Project Template**: Create your first project
   - Blank project
   - Pre-configured templates (Tube assembly, Welding, etc.)

---

## Basic Workflow

### Creating Your First Design

#### Step 1: Create a New Project

```
File → New Project (Ctrl+N)
```

1. Choose a project name (e.g., "First_Bend")
2. Select save location
3. Choose unit system
4. Click "Create"

#### Step 2: Create a Sketch

```
Toolbar → Sketch (Ctrl+Shift+K)
or Tools → Sketch Editor
```

1. Select a plane (Front, Top, Right, or custom)
2. Click "Create Sketch"
3. Use sketch tools to draw geometry:
   - **Line Tool**: Click to start, click again to end
   - **Circle Tool**: Click center, drag to define radius
   - **Arc Tool**: Click start point, intermediate point, end point
   - **Point Tool**: Click to create point

#### Step 3: Add Constraints

```
In Sketch Editor: Constraint toolbar
```

1. Select two geometry elements
2. Click constraint type:
   - **Coincident**: Two points at same location
   - **Parallel**: Two lines parallel
   - **Perpendicular**: Two lines at 90°
   - **Distance**: Set distance between elements
   - **Angle**: Set angle between lines

3. Enter values in the dimension panel
4. Press Enter to confirm

#### Step 4: Create a Tube

```
Toolbar → Tube (Ctrl+T)
or Tools → Create Tube
```

1. Select the sketch profile
2. Extrude path:
   - **Linear**: Set length (e.g., 100mm)
   - **Along sketch**: Follow path sketch
   - **Through profile**: Use 2D profile

3. Set tube properties:
   - Diameter: 12.7 mm
   - Wall thickness: 1.24 mm
   - Material: Steel (ST37), Stainless (304), etc.

4. Click "Create"

#### Step 5: Add a Bend

```
Toolbar → Bend (or Tools → Bend Operations)
```

1. Select the tube
2. Define bend location:
   - Click on the tube to set bend point
   - Or enter distance along tube

3. Set bend parameters:
   - **Angle**: 90° (default)
   - **Radius**: Calculate from tube diameter
   - **Material**: Select from library
   - **Condition**: Stress-relieved, Normalized, etc.

4. Preview shows deformed geometry
5. Click "Apply Bend"

#### Step 6: Save Project

```
File → Save (Ctrl+S)
or File → Save As (Ctrl+Shift+S)
```

- Choose save location
- Select format (BendCore native, STEP, IGES, SAT, DWG)
- Click "Save"

---

## Key Features

### File Operations (M22)

#### Open Recent Files
```
File → Recent Files
or Ctrl+Alt+R
```
- Quickly access recently edited projects
- Pin favorite projects for quick access

#### Settings
```
Edit → Settings
or File → Preferences
```

Configure:
- **Theme**: Light/Dark/High Contrast/Colorblind
- **Units**: Metric/Imperial/Custom
- **Keyboard Shortcuts**: Customize all shortcuts
- **Performance**: Viewport quality, cache size
- **Cloud Storage**: Enable cloud sync (optional)

### Sketch Editor (M23)

#### 2D Sketch Creation

1. **Geometric Primitives**
   - Points, Lines, Circles, Arcs, Splines
   - Draw by clicking in viewport
   - Hold Shift for additional options

2. **Constraints**
   - Fully constrained sketches (0 degrees of freedom)
   - Visual feedback: Green (fully constrained), Yellow (under-constrained), Red (over-constrained)

3. **Smart Dimensioning**
   - Drag dimension to adjust values
   - Double-click to edit dimension value
   - Expressions supported (=B5+10, =D2*2, etc.)

4. **Layers**
   - Organize geometry by layer
   - Toggle layer visibility
   - Lock/unlock layers for protection

#### Sketch Tools

| Tool | Shortcut | Function |
|------|----------|----------|
| Line | L | Draw straight line |
| Circle | C | Draw circle |
| Arc | A | Draw arc |
| Point | P | Create point |
| Dimension | D | Add dimension |
| Constraint | K | Add constraint |
| Mirror | M | Mirror geometry |
| Rotate | R | Rotate geometry |
| Scale | S | Scale geometry |

### Assembly Workbench (M24)

#### Building Assemblies

1. **Add Parts**
   - File → Import Part
   - Select part from library
   - Position using constraints

2. **Apply Mates**
   ```
   Mates toolbar in Assembly mode
   ```
   - **Coincident**: Align faces or planes
   - **Parallel**: Make surfaces parallel
   - **Distance**: Set gap between parts
   - **Angle**: Rotate parts at specific angle

3. **Analyze Assembly**
   - Constraint validation
   - Collision detection
   - Degree of freedom analysis

4. **Exploded View**
   - View → Exploded View
   - Shows assembly step-by-step separation
   - Useful for assembly instructions

### Manufacturing (M25)

#### Generate Bill of Materials

```
Manufacturing → BOM Generator
```

1. Customize columns (Part #, Description, Qty, Weight, Cost)
2. Export to:
   - Excel (.xlsx)
   - CSV (.csv)
   - PDF (.pdf)

#### Create Manufacturing Drawing

```
Manufacturing → Drawing Generator
```

- Auto-generates multi-view drawings
- Includes dimensions and notes
- Export to PDF or DWG

#### View NC Code

```
Manufacturing → NC Code Viewer
```

- View generated G-code
- Simulate tool path
- Check for collisions
- Edit code if needed

#### Manufacturing Analysis

```
Manufacturing → Analysis
```

- Machine capability checking
- Tool availability verification
- Cost estimation
- Setup time calculation

---

## Keyboard Shortcuts

### File Operations

| Shortcut | Action |
|----------|--------|
| Ctrl+N | New Document |
| Ctrl+O | Open Document |
| Ctrl+S | Save Document |
| Ctrl+Shift+S | Save As |
| Ctrl+Q | Exit Application |

### Editing

| Shortcut | Action |
|----------|--------|
| Ctrl+Z | Undo |
| Ctrl+Y | Redo |
| Ctrl+X | Cut |
| Ctrl+C | Copy |
| Ctrl+V | Paste |
| Del | Delete |

### Sketching

| Shortcut | Action |
|----------|--------|
| Ctrl+Shift+K | New Sketch |
| L | Line Tool |
| C | Circle Tool |
| A | Arc Tool |
| D | Dimension |
| K | Constraint |
| Escape | Exit Sketch |

### Assembly

| Shortcut | Action |
|----------|--------|
| Ctrl+T | New Tube |
| Alt+B | Bend Operation |
| Alt+W | Weld Operation |
| Alt+M | Apply Mate |

### View

| Shortcut | Action |
|----------|--------|
| V | Fit All (Zoom to fit) |
| F3 | Toggle Snap to Grid |
| F1 | Help |
| Ctrl+/ | Command Palette |

### Settings

| Shortcut | Action |
|----------|--------|
| Ctrl+, | Open Settings |
| Ctrl+K, Ctrl+S | Customize Shortcuts |

---

## Tips & Tricks

### 1. Speed Up Sketching

- **Use the Grid**: F3 toggles snap-to-grid for faster geometry alignment
- **Smart Constraints**: Add constraints while drawing for faster work
- **Expression Editor**: Use formulas for dimensions (=diameter*2)

### 2. Efficient Assembly Modeling

- **Copy Parts**: Duplicate parts with Ctrl+D, then modify constraints
- **Templates**: Create reusable assembly templates
- **Visibility Control**: Hide parts while working on specific assembly

### 3. Optimize Tube Operations

- **Bend Library**: Save common bend angles and radii for reuse
- **Material Presets**: Create material presets with company standards
- **Batch Operations**: Apply operations to multiple tubes simultaneously

### 4. Effective Manufacturing Planning

- **BOM Customization**: Customize BOM columns for supplier requirements
- **Cost Tracking**: Enable cost estimation to track project budgets
- **NC Program Review**: Always review generated NC code before machining

### 5. Working with Large Assemblies

- **Use Layers**: Organize parts into logical layers
- **Freeze/Lock**: Lock completed subassemblies while working on others
- **View Filters**: Filter parts by type, material, or status

### 6. Keyboard Navigation

- **Command Palette**: Ctrl+/ opens command search (type action name)
- **Quick Actions**: Right-click for context menu in most panels
- **Keyboard Shortcuts Editor**: Customize all shortcuts to your preference

---

## Common Tasks

### Task: Import CAD File

```
File → Open (Ctrl+O)
Select file → STEP, IGES, SAT, or DWG format
Click "Import"
```

### Task: Export to CAD Format

```
File → Export As
Choose format:
  - STEP (ISO standard, recommended)
  - IGES (legacy format)
  - SAT (ACIS geometry)
  - DWG (2D drawing)
Click "Export"
```

### Task: Calculate Part Weight

1. Create complete design
2. Tools → Properties
3. Material and geometry are calculated automatically
4. View → Properties panel shows weight

### Task: Create Assembly Drawing

```
Manufacturing → Drawing Generator
Select views:
  - Front, Top, Right (standard)
  - Custom views as needed
Click "Generate"
Export as PDF or DWG
```

---

## Getting Help

### Documentation

- **User Guide**: See `/docs/GUIDES/` for detailed documentation
- **Keyboard Shortcuts**: F1 shows all available shortcuts
- **API Reference**: See `/docs/API/` for developer documentation

### Support

- **GitHub Issues**: Report bugs at https://github.com/Do4ent/bendcore/issues
- **Discussions**: Ask questions at https://github.com/Do4ent/bendcore/discussions
- **Documentation**: See `/docs/` for comprehensive guides

### Video Tutorials

- Official tutorials coming soon
- See GitHub Discussions for community guides

---

## Next Steps

1. ✅ Install BendCore
2. ✅ Create your first sketch
3. ✅ Build a simple tube model
4. ✅ Add a bend operation
5. ✅ Generate manufacturing output
6. ✅ Export to CAD format

Then explore:
- Advanced sketching constraints
- Complex assemblies
- Manufacturing analysis
- Custom settings and presets

---

## Troubleshooting

### Application won't start
- Ensure Node.js 18+ is installed: `node --version`
- Delete `node_modules` and run `npm install` again
- Check system requirements (4GB RAM minimum)

### Slow performance
- Reduce viewport quality in Settings
- Clear geometry cache: Settings → Performance → Clear Cache
- Close other applications to free RAM

### File won't open
- Ensure file format is supported (STEP, IGES, SAT, DWG, BendCore)
- Check file is not corrupted
- Try importing with File → Import instead of Open

### Constraint solving issues
- Check for over-constrained sketches (red circle)
- Remove conflicting constraints
- Use Sketch → Diagnostics to identify issues

---

## Performance Tips

- Close unused panels to reduce rendering load
- Use layer visibility to hide parts while working
- Simplify complex sketches with fewer constraints
- Use frozen/locked components for completed assemblies
- Enable performance logging: Settings → Performance → Log

---

## Keyboard Shortcuts Customization

To change keyboard shortcuts:

1. Open Settings: Ctrl+, (comma)
2. Go to "Keyboard" tab
3. Find the command you want to change
4. Click the shortcut column
5. Press your desired key combination
6. Click "Save"

---

## System Information

Check system status:
- **Memory Usage**: View → Status Bar (bottom right)
- **Performance**: View → Performance Monitor
- **Component Count**: View → Status Panel

---

**Happy Designing! 🎉**

For more help, visit the [BendCore Documentation](https://github.com/Do4ent/bendcore/tree/main/docs).
