# Parasolid Integration Guide

**Status:** Framework Documented  
**Phase:** 29 (Planning) → Phase 30+ (Implementation)  
**Last Updated:** 2026-10-23

---

## Overview

BendCore CAD integrates with Parasolid (Siemens' advanced geometric kernel) for computational geometry operations that complement BendCore's native tube bending analysis. Parasolid provides enterprise-grade 3D modeling, constraint solving, and manufacturing analytics capabilities.

**Integration Scope:**
- Geometric constraint solving
- Surface/solid generation and validation
- Collision detection (complements BendCore's gap checking)
- Design rule enforcement
- Export to STEP, IGES, and other standard CAD formats

---

## Architecture

```
┌─────────────────────────────────────────────────────┐
│              BendCore CAD Application               │
├─────────────────────────────────────────────────────┤
│                                                     │
│  ┌──────────────────────────────────────────┐     │
│  │     BendCore Native Analysis             │     │
│  │  - Gap Detection                         │     │
│  │  - Mass Calculation                      │     │
│  │  - Bend Parameter Validation             │     │
│  │  - Manufacturing Constraints             │     │
│  └────────────┬─────────────────────────────┘     │
│               │                                     │
│  ┌────────────▼──────────────────────────────┐    │
│  │    Parasolid Integration Layer            │    │
│  │  - Command Router                         │    │
│  │  - Data Serialization                     │    │
│  │  - Result Processing                      │    │
│  └────────────┬─────────────────────────────┘    │
│               │                                     │
│  ┌────────────▼──────────────────────────────┐    │
│  │   Parasolid Kernel (C++)                 │    │
│  │  - Geometric Operations                  │    │
│  │  - Constraint Solving                    │    │
│  │  - CAD Export                            │    │
│  └──────────────────────────────────────────┘    │
│                                                     │
└─────────────────────────────────────────────────────┘
```

---

## Capabilities

### 1. Geometric Constraint Solving

**Purpose:** Solve complex geometric relationships and dependencies

**Operations:**
- Planar constraints (perpendicular, parallel, coaxial)
- Distance/tangency constraints
- Symmetry relationships
- Dimensional constraints

**API:**
```cpp
class ConstraintSolver {
  bool SolveConstraints(GeometryModel& model);
  std::vector<ConstraintViolation> ValidateConstraints();
};
```

**Use Cases:**
- Validate bend orientation relationships
- Enforce port alignment constraints
- Solve assembly dependencies
- Position components accurately

### 2. Surface/Solid Generation

**Purpose:** Create and validate tube geometry as solid models

**Operations:**
- Sweep operations (profiles along paths)
- Surface creation from sketches
- Solid validation
- Volume and center-of-mass computation

**API:**
```cpp
class GeometryGenerator {
  Solid CreateTubeSolid(Path& centerline, 
                       double diameter, 
                       double wallThickness);
  bool ValidateSolid(const Solid& solid);
  double ComputeVolume(const Solid& solid);
};
```

**Use Cases:**
- Generate 3D tube geometry from bend parameters
- Validate manufacturability through solid intersection
- Compute accurate mass properties
- Create visualization geometry

### 3. Collision Detection

**Purpose:** Identify interference between tube components

**Operations:**
- Static collision detection
- Interference volume calculation
- Clearance analysis
- Component-to-component checking

**API:**
```cpp
class CollisionDetector {
  bool HasCollision(const Solid& body1, const Solid& body2);
  double GetClearance(const Solid& body1, const Solid& body2);
  std::vector<Interference> FindInterferences(
    const Assembly& assembly);
};
```

**Integration with BendCore:**
- Complements BendCore's gap detection
- Provides fine-grained interference analysis
- Validates bend path feasibility
- Ensures assembly viability

### 4. Design Rule Enforcement

**Purpose:** Validate adherence to manufacturing constraints

**Operations:**
- Thickness checks
- Draft angle validation
- Draft feature detection
- Undercut identification

**API:**
```cpp
class DesignRuleChecker {
  std::vector<Issue> CheckManufacturability(const Solid& solid);
  bool ValidateThickness(const Solid& solid, double minThickness);
};
```

**Use Cases:**
- Validate tube wall thickness consistency
- Check bend draft angles
- Identify sharp edges
- Flag manufacturability issues

### 5. Format Export

**Purpose:** Export tube designs to standard CAD formats

**Supported Formats:**
- STEP (.step, .stp) - ISO 10303
- IGES (.iges, .igs) - ANSI/US PRIP 104
- PARASOLID (.x_t) - Native format
- STL (.stl) - For 3D printing

**API:**
```cpp
class Exporter {
  bool ExportToSTEP(const Solid& solid, 
                   const std::string& filename);
  bool ExportToIGES(const Solid& solid, 
                   const std::string& filename);
  bool ExportToParasolid(const Solid& solid, 
                        const std::string& filename);
  bool ExportToSTL(const Solid& solid, 
                  const std::string& filename);
};
```

**Use Cases:**
- Share designs with manufacturing partners
- Import into FEM analysis tools
- Create documentation visualizations
- Support 3D printing workflows

---

## Integration Points

### 1. Tube Geometry Kernel Integration

**Current State (Phase 29):**
- Tube centerline path calculated in BendCore
- Diameter and wall thickness validated
- Manufacturing constraints checked

**Parasolid Enhancement (Phase 30+):**
- Create solid geometry from tube definition
- Validate solid geometry integrity
- Extract mass properties with higher accuracy
- Enable collision analysis

**Data Flow:**
```
BendCore Tube Definition
    ↓
Centerline Path (spline)
Diameter, Wall Thickness
    ↓
Parasolid SolidCreation
    ↓
Solid Model (STEP/IGES)
    ↓
Collision Detection
Mass Properties
Manufacturing Checks
```

### 2. Bend Analysis Using Parasolid Solvers

**Current State (Phase 29):**
- Bend parameters validated
- Gap detection at discrete points
- Radius adequacy checked

**Parasolid Enhancement (Phase 30+):**
- Surface analysis along entire bend radius
- Continuity and smoothness validation
- Springback prediction using constraint solver
- Finite element mesh generation

**Workflow:**
```
Bend Definition
    ↓
Create Bend Surface in Parasolid
    ↓
Analyze Surface Continuity
Check Radius Adequacy
    ↓
Springback Prediction
Mesh Generation
    ↓
FEM Analysis Input
```

### 3. Manufacturing Constraint Checking

**Current State (Phase 29):**
- Wall thickness limits enforced
- Bend angle constraints validated
- Gap detection at bends

**Parasolid Enhancement (Phase 30+):**
- Collision detection between tube and tooling
- Clamping interference analysis
- Bend path manufacturability
- Tube insertion/removal feasibility

**Rules:**
```
Manufacturing Constraints:
  - Wall thickness: 1-10mm (configurable per material)
  - Bend angle: 0-180°
  - Min bend radius: OD/2
  - Min spacing: 5mm (tool clearance)
  - No interference with fixtures
  - Tube material compatibility
```

---

## Implementation Timeline

### Phase 29 (Current)
- ✓ Architecture documentation
- ✓ API specification
- ✓ Integration points defined
- ✓ Data format specifications
- ✓ Planning complete

### Phase 30 (Planned)
- [ ] Parasolid SDK integration
- [ ] C++ wrapper layer development
- [ ] Solid geometry generation
- [ ] Collision detection implementation
- [ ] STEP/IGES export functionality
- [ ] Integration testing

### Phase 31+ (Future)
- [ ] FEM analysis bridge
- [ ] Advanced manufacturing analytics
- [ ] Design optimization
- [ ] Cloud processing backend
- [ ] Real-time collision feedback in UI

---

## API Reference

### Core Classes

#### GeometryKernel
```cpp
class ParasolidKernel {
  // Initialization
  bool Initialize(const std::string& license_server);
  void Shutdown();

  // Geometry creation
  Solid CreateTubeSolid(const TubeDefinition& tube_def);
  Surface CreateBendSurface(const BendDefinition& bend_def);

  // Validation
  bool ValidateSolid(const Solid& solid);
  std::vector<Issue> CheckDesignRules(const Solid& solid);

  // Collision detection
  bool DetectCollision(const Solid& a, const Solid& b);
  double GetClearance(const Solid& a, const Solid& b);

  // Export
  bool ExportToSTEP(const Solid& solid, const std::string& path);
  bool ExportToIGES(const Solid& solid, const std::string& path);
};
```

#### ConstraintSolver
```cpp
class ConstraintSolver {
  // Add constraints
  void AddDistanceConstraint(Entity& e1, Entity& e2, double distance);
  void AddPerpendicularConstraint(Entity& e1, Entity& e2);
  void AddParallelConstraint(Entity& e1, Entity& e2);

  // Solve
  SolverResult Solve();
  bool IsConstrained(const Entity& e);
};
```

#### ManufacturingAnalyzer
```cpp
class ManufacturingAnalyzer {
  // Analysis
  ManufacturabilityReport Analyze(const Solid& tube);
  
  // Specific checks
  bool CheckWallThickness(const Solid& solid, double min_thickness);
  bool CheckBendRadius(const Solid& solid, double min_radius);
  bool CheckClearance(const Solid& tube, const Solid& fixture);
};
```

---

## Data Formats

### Tube Definition Structure
```json
{
  "tube_id": "tube-001",
  "outer_diameter": 25,
  "wall_thickness": 2,
  "material": "steel",
  "path": {
    "type": "spline",
    "control_points": [
      [0, 0, 0],
      [50, 0, 0],
      [100, 50, 0],
      [150, 50, 0]
    ]
  },
  "bends": [
    {
      "position": 50,
      "angle": 90,
      "radius": 50,
      "plane": "XY"
    }
  ]
}
```

### Solid Export (STEP Format)
```
ISO 10303-21;
HEADER;
FILE_NAME('tube-001.stp',...);
FILE_SCHEMA(('AP203'));
ENDSEC;
DATA;
#1 = PRODUCT_DEFINITION_FORMATION(...);
...
ENDSEC;
END-ISO-10303-21;
```

---

## Performance Considerations

| Operation | Target Time | Notes |
|-----------|------------|-------|
| Solid creation | <500ms | Single tube |
| Collision detection | <1000ms | Two components |
| Export to STEP | <2000ms | Full assembly |
| Constraint solving | <5000ms | Complex assembly |

---

## Error Handling

### Common Issues and Solutions

| Issue | Cause | Solution |
|-------|-------|----------|
| Invalid solid | Degenerate geometry | Validate tube parameters |
| Collision detected | Geometry intersection | Adjust bend path/positions |
| Export failure | Unsupported geometry | Simplify model |
| License error | SDK initialization | Check Parasolid license |

---

## Testing Strategy

### Unit Tests
```cpp
TEST(ParasolidIntegration, CreateSimpleTube) {
  TubeDefinition tube_def = {...};
  ParasolidKernel kernel;
  Solid solid = kernel.CreateTubeSolid(tube_def);
  ASSERT_TRUE(kernel.ValidateSolid(solid));
}

TEST(ParasolidIntegration, CollisionDetection) {
  Solid tube1 = {...};
  Solid tube2 = {...};
  ParasolidKernel kernel;
  ASSERT_FALSE(kernel.DetectCollision(tube1, tube2));
}
```

### Integration Tests
```cpp
TEST(BendCoreParasolid, EndToEnd) {
  // 1. Create tube in BendCore
  // 2. Pass to Parasolid
  // 3. Generate solid
  // 4. Detect collisions
  // 5. Export to STEP
}
```

---

## Security Considerations

- License key management
- Parasolid SDK authentication
- Data validation before kernel operations
- Resource limits (timeout, memory)
- Safe error handling and cleanup

---

## Dependencies

- **Parasolid Kernel:** Version 34.0+
- **OpenCascade (OCCT):** For complementary geometry operations
- **C++ Standard:** C++17 or later
- **Platform Support:**
  - Windows (x64)
  - Linux (x64)
  - macOS (x64)

---

## References

### Documentation
- [Parasolid SDK Reference](https://siemens.com/parasolid)
- [STEP Format Specification](https://www.iso.org/standard/20541.html)
- [IGES Format Specification](https://webstore.ansi.org)

### Related BendCore Modules
- `/bendcore/src/analysis.cpp` - Gap detection
- `/bendcore/src/tube_model.cpp` - Tube definition
- `/bendcore/include/bendcore/analysis.hpp` - Analysis interfaces

---

## Future Enhancements

1. **Procedural Geometry Generation**
   - Parametric tube design
   - Automated bend path optimization
   - Real-time geometry updates

2. **Advanced Manufacturing**
   - CNC toolpath generation
   - Spindle load simulation
   - Cutting fluid flow analysis

3. **Material Science Integration**
   - Material deformation models
   - Springback compensation
   - Residual stress analysis

4. **Cloud Processing**
   - Offload heavy computations
   - Parallel processing
   - Batch operations

---

## Contact & Support

For questions or issues related to Parasolid integration:

1. Check this documentation
2. Review implementation examples in `/bendcore/src`
3. Consult Parasolid SDK documentation
4. File issues in project repository

---

**Status:** Ready for Phase 30+ Implementation  
**Last Verified:** 2026-10-23  
**Next Review:** Phase 30 Kickoff
