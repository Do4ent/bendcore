# FEA Simulation Guide

## Getting Started with BendCore FEA

The FEA module provides professional-grade structural analysis capabilities.

## Basic Workflow

### 1. Geometry Preparation
- Import STEP/IGES geometry
- Clean up geometry
- Define material regions
- Set up coordinate system

### 2. Meshing
- Automatic tetrahedral mesh generation
- Element size control
- Mesh quality verification
- Refinement in critical areas

### 3. Material Definition
- Select material from library
  - Steel: E=200 GPa, ν=0.3
  - Aluminum: E=70 GPa, ν=0.33
  - Titanium: E=103 GPa, ν=0.34
- Or define custom material

### 4. Boundary Conditions
- Fixed supports (zero displacement)
- Pinned supports (specified displacement)
- Roller supports (constrained in one direction)
- Symmetry conditions

### 5. Load Application
- Point loads
- Distributed loads
- Pressure loads
- Thermal loads

### 6. Solution
- Sparse matrix solver (Eigen SparseLU)
- Automatic convergence check
- Solution time: < 2 sec per 10,000 elements

### 7. Post-Processing
- Displacement visualization
- Stress contour plots
- Strain field display
- Factor of safety calculation

## Solver Method

BendCore uses **Linear Elastic FEA**:

**K × u = F**

Where:
- K = Global stiffness matrix (sparse)
- u = Nodal displacements
- F = Load vector

Solution via: **Sparse LU Decomposition**

## Material Properties

### Steel (Default)
- Young's Modulus: 200 GPa
- Poisson's Ratio: 0.3
- Yield Strength: 250 MPa
- Density: 7,850 kg/m³

### Aluminum
- Young's Modulus: 70 GPa
- Poisson's Ratio: 0.33
- Yield Strength: 50 MPa
- Density: 2,700 kg/m³

### Titanium
- Young's Modulus: 103 GPa
- Poisson's Ratio: 0.34
- Yield Strength: 930 MPa
- Density: 4,506 kg/m³

## Stress Calculations

### Von Mises Stress
$$\sigma_{VM} = \sqrt{\frac{1}{2}[(\sigma_{xx}-\sigma_{yy})^2 + (\sigma_{yy}-\sigma_{zz})^2 + (\sigma_{zz}-\sigma_{xx})^2 + 6(\tau_{xy}^2 + \tau_{xz}^2 + \tau_{yz}^2)]}$$

### Safety Factor
$$SF = \frac{\sigma_{yield}}{\sigma_{VM,max}}$$

## Performance Tips

1. Use coarse mesh initially
2. Refine near stress concentrations
3. Utilize symmetry (reduce model size 50%)
4. Check mesh quality (aspect ratio < 10)
5. Verify boundary conditions

---

**Document:** FEA_SIMULATION_GUIDE.md
**Version:** 1.0
