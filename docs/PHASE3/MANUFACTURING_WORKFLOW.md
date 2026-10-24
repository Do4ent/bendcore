# Manufacturing Workflow Guide

## Complete Design-to-Manufacturing Pipeline

BendCore's manufacturing workflow automates the entire process from CAD design to NC programming.

## Workflow Steps

### Step 1: Design Input
- Create or import 3D assembly
- Define tube specifications
- Set material properties
- Specify bending constraints

### Step 2: Routing Optimization
- Automatically plan pipe routes (RRT*)
- Avoid obstacles and interference
- Select optimal fittings
- Verify manufacturability

### Step 3: FEA Validation
- Mesh the geometry
- Apply loads and constraints
- Solve for stresses/strains
- Verify design safety factor

### Step 4: Manufacturing Plan
- Select appropriate machines
- Generate work instructions
- Create NC programs
- Calculate costs and time

### Step 5: Quality Assurance
- Define inspection procedures
- Generate quality checks
- Prepare documentation
- Verify compliance

## Example Workflow

```
Design Assembly
    ↓
Import STEP file (10 sec)
    ↓
Define Tube Specifications
    ↓
Auto Route Pipes (RRT* - 3 sec)
    ↓
Run FEA Analysis (2 sec)
    ↓
Generate NC Programs (1 sec)
    ↓
Create BOM
    ↓
Estimate Costs
    ↓
Export Documents
    ↓
COMPLETE (Total: ~20 seconds)
```

## Machine Selection

### Available Machines

| Machine | Type | Diameter | Cost/hr |
|---------|------|----------|---------|
| AMOB BC8060 | CNC | 3-60mm | $150/hr |
| Hilpert RB67 | Manual | 6-50mm | $120/hr |
| Triton TBM-40 | CNC | 8-40mm | $100/hr |

## G-Code Generation

BendCore generates ISO 286 compliant G-code:

```gcode
G17 G20 G40 G49 G54 G80 G90 G94
M06 T01 (Tool change)
M03 S3000 (Spindle speed)
G00 X0 Y0 Z0 (Position)
G01 Z-30 F100 (Approach)
(Bend 1)
G00 X100 (Move to bend)
G65 P9001 R45 P25 (Bend 45° at radius 25)
G04 P2000 (Dwell 2 sec)
...
M05 (Stop)
M30 (End)
```

## Quality Checks

Each tube includes:
- Visual inspection procedure
- Dimensional verification
- Surface finish check
- Bend accuracy tolerance

## Cost Estimation

Cost = Machine Cost + Material Cost

- Machine: (time in hours) × (cost per hour)
- Material: Volume × Material Density × Unit Price

---

**Document:** MANUFACTURING_WORKFLOW.md
**Version:** 1.0
