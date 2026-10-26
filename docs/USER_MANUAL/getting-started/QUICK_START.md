# BendCore CAD Quick Start Guide

**Estimated Time:** 30 minutes  
**Difficulty:** Beginner  
**Prerequisites:** BendCore CAD installed

## Overview

This guide will walk you through the essential steps to create your first tube bend design. By the end of this tutorial, you'll have a working knowledge of BendCore's core workflow.

## Part 1: Launch & Workspace Orientation (5 minutes)

### Step 1: Start BendCore

1. **Launch the application**
   - Windows: Double-click the BendCore icon on your desktop or start menu
   - macOS: Open Applications > BendCore
   - Linux: Run `bendcore` from terminal or application launcher

2. **You should see the splash screen** with version information
   - Click anywhere or wait for it to disappear

### Step 2: Review the Main Workspace

The interface has five key areas:

```
┌─────────────────────────────────────────────────────────────┐
│  Menu Bar (File, Edit, View, Tools, Window, Help)          │
├──────────────┬────────────────────────────┬─────────────────┤
│              │                            │                 │
│   Toolbar    │   3D Viewport              │  Properties     │
│   (top)      │   (center - 3D model)      │  Panel          │
│              │                            │  (right)        │
├──────────────┼────────────────────────────┼─────────────────┤
│              │                            │                 │
│   Model      │   Model Tree &             │                 │
│   Tree       │   Project Structure        │  Browser/       │
│   (left)     │                            │  Library        │
│              │                            │  Panel          │
└──────────────┴────────────────────────────┴─────────────────┘
```

### Step 3: Create a New Design

1. Click **File > New > Design** (or press `Ctrl+N`)
2. In the dialog:
   - **Project Name:** "My First Bend"
   - **Template:** "Blank - Standard Assembly"
   - **Units:** "Millimeters"
3. Click **Create**

**✅ Task Complete:** You now have a blank design ready.

## Part 2: Create a Simple Sketch (8 minutes)

### Step 1: Add a Sketch Plane

1. In the **Model Tree** (left panel), right-click on "Design"
2. Select **New > Sketch**
3. The **Sketch Plane Selection** dialog appears:
   - Click on **XY Plane** (it's already selected)
   - Click **Create Sketch**

You're now in Sketch Edit mode. Notice:
- The viewport shows a 2D top-down view
- The toolbar changed to show sketch tools
- The status bar shows "Sketch Edit Mode"

### Step 2: Draw a Simple Profile

We'll create a simple rectangular cross-section:

1. Click the **Rectangle Tool** in the toolbar
   - Or press `R` and then `E`
2. Click to place the first corner at `(0, 0)` - use the grid or type coordinates
3. Click to place the opposite corner at `(50, 20)` - creates a 50mm × 20mm rectangle

### Step 3: Exit Sketch

1. Click the **Exit Sketch** button in the toolbar
   - Or press `Esc`
2. You'll return to 3D view with your sketch visible

**✅ Task Complete:** You've created your first sketch.

## Part 3: Create a Tube Profile (7 minutes)

### Step 1: Access the Tube Dialog

1. With your sketch selected in the Model Tree, click **Tools > Create Tube**
2. The **Tube Configuration Dialog** opens

### Step 2: Set Tube Parameters

Fill in the following values:

| Parameter | Value | Unit |
|-----------|-------|------|
| **Sketch Profile** | (auto-selected) | - |
| **Outer Diameter** | 25.4 | mm |
| **Wall Thickness** | 2.0 | mm |
| **Material** | Steel - Mild | - |
| **Bend Angle 1** | 90 | degrees |
| **Bend Radius** | 50 | mm |

### Step 3: Create the Tube

1. Click the **Create Tube** button
2. The 3D viewport shows your tube with the bend applied
3. The model appears in the **Model Tree** as "Tube_01"

**✅ Task Complete:** You've created a parametric tube bend!

## Part 4: Run Basic Analysis (5 minutes)

### Step 1: Open Simulation Panel

1. Click **Tools > Analysis > FEA Setup**
2. The **Simulation Panel** appears on the right

### Step 2: Configure Analysis

1. **Analysis Type:** Keep as "Static Linear"
2. **Material:** Should be pre-filled (Steel - Mild)
3. **Load Type:** Select "Fixed Load"
4. **Load Value:** Enter `100` (load in Newtons)
5. Click **Apply Load to Geometry**

### Step 3: Run the Analysis

1. Click the **Run Analysis** button
2. Status messages appear in the output panel
3. After 10-30 seconds, results display:
   - **Von Mises Stress:** Maximum stress value
   - **Displacement:** Maximum deformation
   - **Safety Factor:** If applicable

### Step 4: View Results

The 3D model is now colored by stress distribution:
- **Blue:** Low stress (safe)
- **Green:** Moderate stress
- **Red:** High stress (potential concern)

**✅ Task Complete:** You've run your first FEA analysis!

## Part 5: Generate a Drawing (5 minutes)

### Step 1: Create Manufacturing Drawing

1. Right-click on "Tube_01" in the Model Tree
2. Select **Generate Drawing**
3. The **Drawing Generation Wizard** opens

### Step 2: Configure Drawing

1. **Drawing Type:** Select "Manufacturing"
2. **Scale:** Keep as "1:1"
3. **Include:**
   - ✅ Dimensions
   - ✅ Bend Angles
   - ✅ Material Specifications
   - ✅ Manufacturing Notes
4. Click **Generate**

A new drawing view opens showing:
- Top, front, and isometric views
- All critical dimensions
- Bend radii and angles
- Material specifications

### Step 3: Save the Drawing

1. Click **File > Export > DWG**
2. Name it "My_First_Bend.dwg"
3. Click **Save**

**✅ Task Complete:** You've generated a manufacturing drawing!

## Part 6: Save Your Project (1 minute)

### Save Your Work

1. Click **File > Save** or press `Ctrl+S`
2. Choose a location on your computer
3. The file is saved as "My First Bend.bending"

### What's a .bending File?

- BendCore's native format
- Contains all models, sketches, and analysis data
- Can be reopened to continue editing
- Includes full history and parametric data

**✅ Task Complete:** Your project is saved!

## What You've Learned

Congratulations! You've completed the essential BendCore workflow:

1. ✅ Created a new design
2. ✅ Drew a sketch profile
3. ✅ Created a parametric tube with bending
4. ✅ Ran FEA analysis
5. ✅ Generated a manufacturing drawing
6. ✅ Saved your project

## Next Steps

### Immediate Next Steps
1. Try creating different bend angles and shapes
2. Experiment with different materials
3. Run analysis with different load values

### Learn More
- [Sketcher Tutorial](../tutorials/01_SKETCHER_FUNDAMENTALS.md) - Master 2D sketching
- [Assembly Tutorial](../tutorials/02_ASSEMBLY_DESIGN.md) - Build complex assemblies
- [FEA Guide](../guides/02_FEA_ANALYSIS.md) - Deep dive into analysis

### Keyboard Shortcuts to Remember

| Shortcut | Action |
|----------|--------|
| `Ctrl+N` | New design |
| `Ctrl+S` | Save |
| `Ctrl+Z` | Undo |
| `Ctrl+Y` | Redo |
| `F1` | Help |
| `E` | Sketch Edit |
| `V` | Fit View |
| `Shift+Z` | Zoom to Selection |

## Troubleshooting Quick Tips

### "Tube creation failed" error
- Verify sketch is closed and valid
- Check that all parameters are positive values
- Ensure sketch isn't self-intersecting

### Analysis won't run
- Check that material is selected
- Verify loads are applied correctly
- Try re-running the analysis

### Drawing won't generate
- Ensure tube is fully defined
- Check that bends are valid
- Try refreshing the viewport first

## Tips for Success

💡 **Use the Grid:** Enable the grid for more precise positioning (View > Grid)

💡 **Snap to Grid:** Toggle snap-to-grid for easier sketching (Press `G`)

💡 **Viewport Navigation:**
- **Rotate:** Middle-click and drag (or Shift + right-click)
- **Pan:** Right-click and drag
- **Zoom:** Scroll wheel

💡 **Undo Everything:** If you get stuck, press `Ctrl+Z` repeatedly to go back

💡 **Read Tooltips:** Hover over any button to see what it does

## Getting Help

- **In-app Help:** Press `F1` or click Help > Help Contents
- **Context Help:** Right-click on any tool
- **Tutorials:** Help > Tutorials
- **Support:** support@bendcore.local

---

**You're ready to start designing!** Open your saved project and continue experimenting.

**Next Document:** [Sketcher Fundamentals](../tutorials/01_SKETCHER_FUNDAMENTALS.md)
