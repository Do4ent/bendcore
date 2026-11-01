# Phase 4 - Detailed Implementation Schedule (REVISED)
## Строгое выполнение по документации - FEA & API FROZEN

**Начало:** January 15, 2027  
**Завершение:** April 10, 2027  
**Длительность:** 12 недель  
**Status:** ✅ UPDATED - FEA & API components FROZEN (see FROZEN_COMPONENTS.md)

---

## ⚠️ IMPORTANT: FROZEN COMPONENTS

**The following components are LOCKED for modifications:**
- ❌ FEA (src/fea/) - ZERO changes allowed
- ❌ API Routes (backend/src/api/routes/) - ZERO changes allowed

See detailed restrictions in: [FROZEN_COMPONENTS.md](FROZEN_COMPONENTS.md)

---

## WEEK 1-2: FEA SOLVER INTEGRATION

### Week 1 (Jan 15-19)

**Day 1-2: Analysis & Planning**
- Читай: ROADMAP.md Phase 4a, FEATURES.md M17
- Анализ: LinearElasticFEA engine (src/fea/)
- Планирование: Tetrahedralization algorithm
- Создай: Task list для Week 1-2

**Day 3-5: Mesh Tetrahedralization Implementation**

File: `src/geometry/mesh_tetrahedralization.hpp` (NEW)
```cpp
#pragma once

#include <vector>
#include <optional>
#include "bendcore/geometry_kernel.hpp"

namespace bendcore::geometry {

struct Tetrahedron {
    struct Vertex {
        int id;
        Point3D position;
    };
    std::vector<Vertex> vertices;  // 4 vertices
};

class TetrahedralizationEngine {
public:
    struct Parameters {
        double target_aspect_ratio = 1.2;
        double min_edge_length = 0.1;
        int max_iterations = 100;
    };
    
    std::vector<Tetrahedron> tetrahedralize(
        const TriangleMesh& surface_mesh,
        const Parameters& params = {}
    );
    
    bool validate_quality(const std::vector<Tetrahedron>& mesh);
};

}  // namespace bendcore::geometry
```

File: `src/geometry/mesh_tetrahedralization.cpp` (NEW)
```cpp
#include "mesh_tetrahedralization.hpp"

namespace bendcore::geometry {

std::vector<Tetrahedron> TetrahedralizationEngine::tetrahedralize(
    const TriangleMesh& surface_mesh,
    const Parameters& params
) {
    // Implementation: Delaunay-based tetrahedralization
    // 1. Compute Delaunay triangulation in 3D
    // 2. Constrain surface mesh
    // 3. Improve aspect ratio iteratively
    
    std::vector<Tetrahedron> result;
    // ... implementation
    return result;
}

}  // namespace bendcore::geometry
```

File: `src/geometry/__tests__/tetrahedralization.test.ts` (NEW)
```typescript
import { tetrahedralize } from '../mesh_tetrahedralization';

describe('TetrahedralizationEngine', () => {
    it('tetrahedralizes simple cube mesh', () => {
        const mesh = createCubeMesh();
        const tetras = tetrahedralize(mesh);
        expect(tetras.length).toBeGreaterThan(0);
    });
    
    it('validates mesh quality (aspect ratio)', () => {
        // 15+ test cases
    });
});
```

**Deliverables (Day 3-5):**
- ✅ Tetrahedralization algorithm (300-400 LOC)
- ✅ Tests: 15+ cases
- ✅ Code review & merge

---

### Week 2 (Jan 22-26)

**Day 1-3: Boundary Conditions & Loads**

File: `src/fea/boundary_conditions.cpp` (NEW)
```cpp
struct BoundaryCondition {
    enum Type {
        FIXED,        // 6 DOF constrained
        PINNED,       // XY translation fixed
        ROLLER,       // Movement along direction only
        SYMMETRIC,    // Symmetry plane
        PERIODIC      // Periodic boundary
    };
};

struct Load {
    enum Type {
        FORCE,        // F(N)
        PRESSURE,     // P(MPa)
        TEMPERATURE,  // T(°C)
        GRAVITY       // g = 9.81 m/s²
    };
};
```

File: `src/fea/fea_analyzer.cpp` (EXTEND)
```cpp
class FEAAnalyzer {
public:
    struct Result {
        std::vector<double> stress;        // Von Mises stress per node
        std::vector<Vector3> displacement; // Displacement field
        double max_stress;
        double max_displacement;
        int num_elements;
        int num_nodes;
    };
    
    Result analyze(
        const TriangleMesh& mesh,
        const std::vector<BoundaryCondition>& bcs,
        const std::vector<Load>& loads,
        const Material& material
    );
};
```

**Day 4-5: bendcore_server Integration**

File: `backend/src/api/routes/import.ts` (MODIFY)
```typescript
// POST /api/import/analyze-fea-full
router.post('/analyze-fea-full', async (req, res) => {
    const { mesh, boundary_conditions, loads, material } = req.body;
    
    try {
        const bendcoreUrl = `http://localhost:8080/v1/fea/analyze`;
        const response = await fetch(bendcoreUrl, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                mesh,
                boundary_conditions,
                loads,
                material
            })
        });
        
        const result = await response.json();
        res.json({ success: true, data: result });
    } catch (err) {
        res.status(500).json({ success: false, error: err.message });
    }
});
```

**Deliverables (Week 2):**
- ✅ Boundary conditions & loads (250 LOC)
- ✅ FEA analyzer update (300 LOC)
- ✅ API integration (100 LOC)
- ✅ Tests: 20+ BC/load cases
- ✅ Code review & merge

---

**Week 1-2 Summary:**
- ✅ 900+ lines of new C++ code
- ✅ 35+ test cases passing
- ✅ Tetrahedralization + BC/Loads working
- ✅ Ready for Week 3: Visualization

---

## WEEK 3: STRESS VISUALIZATION

**Day 1-2: Heatmap Component**

File: `ui/src/components/FEA/StressHeatmap.tsx` (NEW)
```typescript
interface StressHeatmapProps {
    stressData: number[];           // Von Mises stress per vertex
    meshVertices: Vector3[];
    meshIndices: number[];
    colorScale: 'hot' | 'cool' | 'viridis';
    minStress?: number;
    maxStress?: number;
}

export const StressHeatmap: React.FC<StressHeatmapProps> = ({
    stressData,
    meshVertices,
    meshIndices,
    colorScale,
    minStress,
    maxStress
}) => {
    // Canvas-based rendering
    // Map stress values → colors
    // Display legend with statistics
};
```

**Day 3-5: Integration with ImportDialog**

File: `ui/src/components/Import/ImportDialog.tsx` (MODIFY)
```typescript
// Modify FEA results section to include stress heatmap
// Show stress visualization in 3D viewport
// Add controls for color scale adjustment
```

**Deliverables (Week 3):**
- ✅ Stress heatmap component (300 LOC)
- ✅ Integration with ImportDialog
- ✅ Tests: 10+ visualization cases
- ✅ Performance: <100ms rendering

---

## WEEK 4: PARAMETRIC DESIGN

**Day 1-2: Parameter Engine**

File: `src/parameters/parameter_engine.cpp` (EXTEND)
```cpp
class ParameterStore {
    struct Parameter {
        std::string name;
        double value;
        double min_value, max_value;
        std::string unit;
        bool is_driving;
    };
    
    // Operations
    bool add_parameter(const Parameter& p);
    bool link_to_geometry(
        const std::string& name,
        EntityId entity_id,
        const std::string& property
    );
    bool update_parameter(const std::string& name, double value);
};
```

**Day 3-5: Parameter Slider UI**

File: `ui/src/components/Parametric/ParameterSliderPanel.tsx` (NEW)
```typescript
// Interactive slider for each parameter
// Real-time geometry update
// Input validation

export const ParameterSliderPanel: React.FC = ({ parameters, onUpdate }) => {
    return (
        <div className="parameter-panel">
            {parameters.map(p => (
                <ParameterRow
                    key={p.name}
                    parameter={p}
                    onChange={(value) => onUpdate(p.name, value)}
                />
            ))}
        </div>
    );
};
```

**Deliverables (Week 4):**
- ✅ Parameter engine (400 LOC)
- ✅ Parameter UI (350 LOC)
- ✅ Tests: 30+ parameter cases
- ✅ Real-time updates working

---

## WEEK 5-6: GEOMETRY CREATION

### Week 5: Extrude

File: `src/features/extrude.cpp` (NEW)
```cpp
class ExtrudeFeature {
    struct Parameters {
        Sketch2D* sketch;
        double length;
        bool symmetric = false;
        double taper_angle = 0;
    };
    
    Solid execute(const Parameters& params);
};
```

Tests: `extrude.test.ts`
- Basic extrude
- Symmetric extrude
- Taper angle extrusion
- Multi-profile extrusion

### Week 6: Revolve

File: `src/features/revolve.cpp` (NEW)
```cpp
class RevolveFeature {
    struct Parameters {
        Sketch2D* profile;
        Line3D* axis;
        double angle = 360;
        bool reverse = false;
    };
    
    Solid execute(const Parameters& params);
};
```

Tests: `revolve.test.ts`
- Full revolution (360°)
- Partial angles (90°, 180°, etc.)
- Multiple revolutions
- Reverse direction

**Deliverables (Week 5-6):**
- ✅ Extrude feature (500 LOC)
- ✅ Revolve feature (500 LOC)
- ✅ Tests: 30+ feature cases
- ✅ B-Rep generation working

---

## WEEK 7: BOOLEAN OPERATIONS

File: `src/geometry/boolean_operations.cpp` (NEW)
```cpp
class BooleanOperations {
    enum Operation { UNION, DIFFERENCE, INTERSECTION };
    
    Solid execute(
        const Solid& a,
        const Solid& b,
        Operation op
    );
};
```

Tests: `boolean_operations.test.ts`
- Union with overlap
- Subtraction (cavity)
- Intersection
- Adjacent surfaces
- Edge cases (touching edges)

**Deliverables (Week 7):**
- ✅ Boolean operations (600 LOC)
- ✅ Tests: 25+ cases
- ✅ B-Rep topology working

---

## WEEK 8-9: ASSEMBLY SYSTEM

### Week 8: Constraint Definition

File: `src/assembly/assembly_constraints.cpp` (NEW)
```cpp
enum ConstraintType {
    COINCIDENT,
    PARALLEL,
    PERPENDICULAR,
    DISTANCE,
    ANGLE,
    TANGENT,
    CONCENTRIC,
    FIX
};

class ConstraintResolver {
    bool resolve(
        std::vector<AssemblyConstraint>& constraints,
        const CadDocument& doc
    );
};
```

### Week 9: Assembly UI

File: `ui/src/components/Assembly/AssemblyTree.tsx` (NEW)
File: `ui/src/components/Assembly/ConstraintEditor.tsx` (NEW)

**Deliverables (Week 8-9):**
- ✅ Assembly constraints (700 LOC)
- ✅ Constraint resolver (400 LOC)
- ✅ Assembly UI (500 LOC)
- ✅ Tests: 30+ constraint cases

---

## WEEK 10-12: INTEGRATION & TESTING

### Week 10: Feature Integration
- Integrate all Phase 4 features
- Ensure no conflicts between components
- Performance optimization

### Week 11: Full Test Suite
- All 1220+ Jest tests passing
- 72 C++ tests passing
- 40+ Phase 4-specific tests passing
- Cross-platform builds (Windows/Linux/WASM preview)

### Week 12: Documentation & Polish
- API documentation complete
- User guide for Phase 4 features
- Performance benchmarks documented
- Code cleanup and optimization

---

## ACCEPTANCE CRITERIA

### Code Quality
```
✅ All tests pass (1220+ Jest + 72 C++ + 40+ Phase 4)
✅ Code coverage >85% for new code
✅ Zero compiler warnings (-Wall -Wextra -Werror)
✅ No memory leaks (Valgrind clean)
✅ TypeScript strict mode enabled
```

### Functionality
```
✅ Can extrude and revolve from 2D sketches
✅ Boolean operations (union/subtract/intersect) working
✅ Assembly constraints resolve properly
✅ FEA analysis <2s for 100+ element mesh
✅ Parametric sliders update geometry in <100ms
✅ Stress visualization heatmap renders correctly
```

### Performance
```
✅ FEA tetrahedralization: <1s for typical geometry
✅ Boolean operations: <500ms for typical bodies
✅ Parameter updates: <100ms mesh regeneration
✅ Constraint resolution: <500ms for 10+ constraints
```

### Documentation
```
✅ API docs complete (Doxygen)
✅ User guide written (Phase 4 features)
✅ Architecture diagrams updated
✅ Code comments explain "why" not "what"
```

---

## GO/NO-GO DECISION (April 10, 2027)

### GO Criteria (all must be met)
- ✅ All acceptance criteria met
- ✅ Zero critical bugs
- ✅ Performance targets met
- ✅ Documentation complete
- ✅ Stakeholder sign-off obtained

### NO-GO Actions
- Bug fixes (max 1 week)
- Feature deferral to Phase 5
- Scope reduction if needed
- Re-evaluation for Phase 4.5 patch

---

## TEAM ASSIGNMENTS

| Component | Developer | Weeks |
|-----------|-----------|-------|
| FEA Solver | C++ Lead | 1-4 |
| Tetrahedralization | C++ Lead | 1-2 |
| Stress Visualization | TS Lead | 3 |
| Parameter Engine | C++ Lead | 4 |
| Parameter UI | TS Lead | 4 |
| Extrude/Revolve | C++ Lead | 5-6 |
| Boolean Operations | C++ Lead | 7 |
| Assembly Constraints | C++ Lead | 8 |
| Assembly UI | TS Lead | 8-9 |
| Testing | QA Lead | 10-12 |
| Documentation | Tech Writer | 12 |

---

## SUCCESS METRIC: FEATURE PARITY

By end of Phase 4:
- ✅ 30% feature parity with Autodesk Inventor (for tube design)
- ✅ Can create basic 3D models from scratch
- ✅ Can assemble multiple parts with constraints
- ✅ Can run FEA analysis on designs
- ✅ Can optimize designs parametrically

---

**Document Status:** Approved for Phase 4 Execution  
**Start Date:** January 15, 2027  
**Target Completion:** April 10, 2027
