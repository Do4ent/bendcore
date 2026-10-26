# Sketcher Tutorial: Fundamentals

**Duration:** 45 minutes  
**Difficulty:** Intermediate  
**Prerequisites:** [Quick Start Guide](../getting-started/QUICK_START.md)

## What is a Sketch?

A sketch is a 2D drawing that serves as the foundation for your 3D tube design. In BendCore, sketches define:
- **Cross-section profiles** for tubes
- **Reference geometry** for assemblies
- **Manufacturing profiles** for NC programs

All tube designs start with a sketch.

## Part 1: Entering Sketch Mode (5 minutes)

### Method 1: New Sketch

1. Open BendCore and create a new design (File > New > Design)
2. In the Model Tree, right-click on your project name
3. Select **New > Sketch**
4. Choose a plane:
   - **XY Plane** - Top view (default)
   - **XZ Plane** - Front view
   - **YZ Plane** - Side view
5. Click **Create Sketch**

### Method 2: Edit Existing Sketch

1. In the Model Tree, find the sketch you want to edit
2. Double-click on it
3. You're now in Sketch Edit mode

## Interface Changes in Sketch Mode

When you enter sketch mode, notice:
- ✅ **2D orthographic view** (top-down)
- ✅ **Sketch toolbar** appears with 2D tools
- ✅ **Status bar** shows "Sketch Edit Mode"
- ✅ **Model Tree** highlights the current sketch
- ✅ **Grid** is visible for reference

## Part 2: Basic Sketching Tools (20 minutes)

### Tool 1: Line Tool

The Line tool creates straight line segments.

**How to use:**
1. Click the **Line** button in the toolbar (or press `L`)
2. Click to place the first point
3. Click to place the second point
4. Continue clicking to add more connected lines
5. Press `Esc` to finish

**Example: Draw a simple L-shape**
1. Start at origin (0, 0)
2. Click at (50, 0)
3. Click at (50, 25)
4. Press `Esc`

### Tool 2: Rectangle Tool

Creates rectangular profiles (very common for tube cross-sections).

**How to use:**
1. Click the **Rectangle** button (or press `R`)
2. Click the first corner
3. Click the opposite corner
4. Rectangle is created

**Example: 50mm × 25mm rectangle**
1. Click at (0, 0)
2. Click at (50, 25)
3. You now have a rectangle

### Tool 3: Circle Tool

Creates circular profiles (for round tubes).

**How to use:**
1. Click the **Circle** button (or press `C`)
2. Click to place the center point
3. Click to set the radius
4. Circle is created

**Shortcut:** Type the radius value while placing to be precise

**Example: 25mm diameter circle**
1. Click at (0, 0) for center
2. Type `12.7` (radius) and press `Enter`

### Tool 4: Polygon Tool

Creates regular polygons (hexagons, octagons, etc.).

**How to use:**
1. Click the **Polygon** button (or press `P`)
2. Select number of sides (3-12)
3. Click center point
4. Click to set radius
5. Polygon is created

### Tool 5: Arc Tool

Creates curved segments.

**How to use:**
1. Click the **Arc** button (or press `A`)
2. Click start point
3. Click end point
4. Click to set the arc's curvature
5. Arc is created

### Tool 6: Fillet/Chamfer

Rounds corners or creates beveled edges.

**To fillet corners:**
1. Click **Fillet** tool
2. Click on the corner you want to round
3. Drag to set radius (or type value)
4. Click **OK**

## Part 3: Constraints (15 minutes)

Constraints control the exact dimensions of your sketch, making it parametric.

### Dimension Constraints

#### Horizontal Distance
1. Click the **Dimension** tool (or press `D`)
2. Click a horizontal line
3. Click where to place the dimension
4. Enter the value (e.g., `50`)
5. Press `Enter`

#### Vertical Distance
1. Click **Dimension**
2. Click a vertical line
3. Place dimension
4. Enter value

#### Length Constraint
1. Click **Dimension**
2. Click any line
3. Place dimension
4. Enter value

#### Angle Constraint
1. Click **Angle Constraint** tool
2. Click first line
3. Click second line
4. Enter angle in degrees

### Geometric Constraints

#### Horizontal/Vertical
Force a line to be perfectly horizontal or vertical:
1. Select the line
2. Click **Make Horizontal** or **Make Vertical**

#### Parallel/Perpendicular
1. Click **Parallel** tool
2. Click first line
3. Click second line
4. Lines are now parallel

#### Equal Length
Make multiple lines the same length:
1. Click **Equal** tool
2. Click first line
3. Click second line
4. Lines now have equal length

#### Coincident
Force two points to be at the same location:
1. Click **Coincident** tool
2. Click first point
3. Click second point

#### Concentric
Make circles/arcs share the same center:
1. Click **Concentric** tool
2. Click first circle
3. Click second circle

### Example: Constrained Rectangle

Create a rectangle with exact dimensions:

1. Draw any rectangle using the Rectangle tool
2. Click **Dimension** tool
3. Click the bottom edge, enter `100` mm - confirms width
4. Click the right edge, enter `50` mm - confirms height
5. Click **Fully Constrained** to verify
6. ✅ Rectangle is now fully defined

## Part 4: Editing & Modifying Sketches (10 minutes)

### Moving Elements

**Drag:**
1. Click on an element (line, circle, etc.)
2. Hold and drag to new position
3. Release

**Precise Move:**
1. Select element
2. Hold `Shift` and drag
3. Or use Properties panel to enter exact coordinates

### Deleting Elements

1. Select element by clicking on it
2. Press `Delete` key
3. Or right-click and select **Delete**

### Editing Dimension Values

1. Double-click on a dimension value
2. Enter new value
3. Press `Enter`
4. Model updates immediately

### Changing Constraint Type

1. Double-click on the constraint
2. Change the value or type
3. Press `Enter`

## Part 5: Construction Geometry (5 minutes)

Construction geometry helps you build sketches but doesn't define the final shape.

### Using Construction Lines

Construction lines are used as guides but don't appear in the final profile.

**To create construction geometry:**
1. Draw a line or circle normally
2. Right-click on it
3. Select **Toggle Construction Geometry**
4. It becomes lighter/dashed (construction)

**Common use cases:**
- Center lines for symmetry
- Reference lines for alignment
- Guide circles for spacing

## Sketch Exercise 1: Create a Circular Tube Cross-Section

**Goal:** Create a constrained circular profile for a tube

**Steps:**
1. Create a new sketch
2. Use Circle tool: `C`, click at origin, type `12.7` (for 25.4mm diameter)
3. Apply dimension constraint: Click **Dimension**, click circle, place dimension
4. Verify it shows `25.4` mm diameter
5. Mark as fully constrained
6. Exit sketch with `Esc`

**Result:** ✅ A perfectly circular tube profile

## Sketch Exercise 2: Create a Rectangular Profile with Fillets

**Goal:** Create a realistic tube profile with rounded corners

**Steps:**
1. Create new sketch
2. Draw rectangle: `R`, click (0,0), click (50,30)
3. Add dimensions: `D` + click bottom edge → `50` mm
4. Add dimensions: `D` + click right edge → `30` mm
5. Apply fillets: Click **Fillet**, click each corner, enter `3` mm radius
6. Add fillet dimensions
7. Verify fully constrained
8. Exit sketch

**Result:** ✅ A 50×30mm rectangle with 3mm rounded corners

## Sketch Exercise 3: Create a Complex Profile

**Goal:** Create a profile with multiple shapes

**Steps:**
1. New sketch
2. Draw outer rectangle: 100mm × 60mm
3. Draw inner circle: 20mm diameter at center
4. Draw two side rectangles for features
5. Apply all necessary dimensions
6. Verify fully constrained
7. Exit sketch

**Result:** ✅ A complex profile ready for extrusion

## Constraint Status Indicators

### Color Coding

| Color | Meaning |
|-------|---------|
| 🟢 Green | Fully constrained (ready to use) |
| 🟡 Yellow | Under-constrained (partial dimensions) |
| 🔴 Red | Over-constrained (conflicting constraints) |
| ⚪ White | Construction geometry |

### Checking Constraint Status

1. View > Status Bar to see constraint count
2. Look at the color of geometry
3. If yellow: Add more constraints before exiting
4. If red: Remove conflicting constraints

## Common Sketching Mistakes

❌ **Mistake 1: Under-constrained sketch**
- **Problem:** Not all dimensions are specified
- **Solution:** Add dimension constraints to all elements

❌ **Mistake 2: Gaps in profile**
- **Problem:** Sketch has open edges
- **Solution:** Ensure all lines connect (use Coincident constraints)

❌ **Mistake 3: Self-intersecting geometry**
- **Problem:** Lines cross where they shouldn't
- **Solution:** Redraw the profile more carefully

❌ **Mistake 4: Forgetting to exit sketch mode**
- **Problem:** Can't create 3D features
- **Solution:** Press `Esc` to exit sketch edit mode

## Helpful Tips

💡 **Use the Grid:** Enable grid to help align geometry
   - View > Grid or press `Shift+G`

💡 **Snap to Grid:** Makes placement easier
   - Press `G` to toggle snap

💡 **Type Instead of Clicking:** For precision, type values
   - Many tools allow direct value entry

💡 **Zoom Close:** Use `Shift+Z` to zoom to your sketch

💡 **Symmetry:** Use symmetric constraints to reduce dimensions
   - Right-click on line > Make Symmetric about axis

💡 **Construction Center Lines:** Always create horizontal/vertical center lines in construction mode

## Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| `L` | Line tool |
| `R` | Rectangle tool |
| `C` | Circle tool |
| `P` | Polygon tool |
| `A` | Arc tool |
| `D` | Dimension tool |
| `Esc` | Exit sketch mode |
| `Ctrl+Z` | Undo |
| `Ctrl+Y` | Redo |
| `Delete` | Delete selected |
| `V` | Fit view |

## Next Steps

1. ✅ Practice basic sketching with different profiles
2. ✅ Master constraint application
3. ✅ Read [Assembly Tutorial](02_ASSEMBLY_DESIGN.md) to build 3D models
4. ✅ Create your [First Tube Bend](../guides/01_FIRST_BEND_DESIGN.md)

---

**Completed:** Sketcher Fundamentals  
**Next:** [Assembly Design Tutorial](02_ASSEMBLY_DESIGN.md)
