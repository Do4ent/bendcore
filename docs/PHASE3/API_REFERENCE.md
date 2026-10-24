# BendCore API Reference

## Core Modules (222 Functions)

### Routing Engine (45 functions)

#### Path Planning
- `findOptimalRoute()` - RRT* path planning
- `findNearestNode()` - KD-tree nearest neighbor
- `findNeighbors()` - Radius-based neighbor search
- `extractPath()` - Convert tree to path
- `smoothPath()` - Catmull-Rom smoothing
- `insertFittings()` - Select fittings by angle
- `isCollisionFree()` - Check path validity
- `checkPointObstacleCollision()` - Point-obstacle test
- `sampleRandomPoint()` - Goal-biased sampling

#### Parameters
- `setMaxIterations()` - RRT iterations (default 5000)
- `setRewireRadius()` - Neighbor search radius
- `setCollisionMargin()` - Safety margin (default 5cm)

### FEA Engine (67 functions)

#### Solver
- `solve()` - Main FEA solver
- `computeTetrahedronStiffness()` - Element K matrix
- `computeMaterialMatrix()` - D matrix
- `computeStrain()` - Strain calculation
- `computeStress()` - Stress calculation
- `computeVonMisesStress()` - VM invariant

#### Materials (3 predefined)
- `getSteel()` - E=200 GPa, ν=0.3
- `getAluminum()` - E=70 GPa, ν=0.33
- `getTitanium()` - E=103 GPa, ν=0.34

### Collaboration (50 functions)

#### Server Management
- `start()` - Start WebSocket server
- `stop()` - Graceful shutdown
- `handleNewConnection()` - New client handler
- `handleMessage()` - Message routing

#### Operations
- `handleOperation()` - Process design changes
- `transformOperation()` - OT conflict resolution
- `isCauslyConcurrent()` - Vector clock comparison
- `resolveConflict()` - Conflict resolution logic

#### Presence
- `broadcastPresence()` - Send user list
- `getPresence()` - Get active users
- `handleCursorMove()` - Cursor sync

### Manufacturing (60 functions)

#### Planning
- `generatePlan()` - Complete plan generation
- `generateNCProgram()` - G-code generation
- `selectBendingMachines()` - Machine selection
- `generateBendSequence()` - Sequence planning
- `estimateTime()` - Time estimation
- `estimateCost()` - Cost calculation

#### Quality
- `generateQualityChecks()` - QA procedures
- `generateSafetyNotes()` - Safety guidelines
- `generateMaterialList()` - BOM generation

## Example Usage

```cpp
// Routing
AdvancedRoutingEngine router(constraints);
Path path = router.findOptimalRoute(start, end, obstacles, constraints);

// FEA
LinearElasticFEA fea;
FEAResult result = fea.solve(nodes, elements, bcs, loads);
double max_stress = result.max_stress;

// Manufacturing
ManufacturingPlanner planner;
ManufacturingPlan plan = planner.generatePlan(tubes, constraints);
```

---

**Document:** API_REFERENCE.md
**Version:** 1.0
