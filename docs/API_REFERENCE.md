# BendCore CAD API Reference

**Document Version:** 2.1.0  
**Last Updated:** October 26, 2026  
**Status:** Production Ready

## Table of Contents

1. [Core APIs](#core-apis)
2. [Database APIs](#database-apis)
3. [Geometry APIs](#geometry-apis)
4. [Manufacturing APIs](#manufacturing-apis)
5. [Material APIs](#material-apis)
6. [Optimization APIs](#optimization-apis)
7. [Error Handling](#error-handling)
8. [Code Examples](#code-examples)

---

## Core APIs

### BendCalculator API

Core component for calculating tube bending parameters and bend allowance.

#### Header
```cpp
#include <bendcore/bend_calculator.h>
```

#### Key Classes

**class `BendCalculator`**

Core bend calculation engine supporting Cartesian, cylindrical, and multi-axis bending.

##### Constructor
```cpp
BendCalculator();
BendCalculator(const Config& config);
```

##### Public Methods

```cpp
/**
 * Calculate bend parameters for a single bend operation
 * @param tubeOD Outer diameter in mm
 * @param tubeWall Wall thickness in mm
 * @param bendDiameter Bend diameter (centerline) in mm
 * @param bendAngle Target bend angle in degrees
 * @param bendRadius Bend radius in mm
 * @return BendResult containing all bend parameters
 * @throws std::invalid_argument if parameters invalid
 */
BendResult calculateBend(
    double tubeOD,
    double tubeWall,
    double bendDiameter,
    double bendAngle,
    double bendRadius
);

/**
 * Calculate bend allowance using various methods
 * @param bendRadius Radius of the bent line in mm
 * @param bendAngle Bend angle in degrees
 * @param thickness Material thickness in mm
 * @param method Calculation method (EMPIRICAL, DIN, ISO, USER_DEFINED)
 * @return Bend allowance in mm
 */
double calculateBendAllowance(
    double bendRadius,
    double bendAngle,
    double thickness,
    BendAllowanceMethod method = BendAllowanceMethod::EMPIRICAL
);

/**
 * Calculate multi-bend assembly
 * @param bendSequence Vector of individual bend specifications
 * @return AssemblyResult with total length and all bend parameters
 */
AssemblyResult calculateAssembly(
    const std::vector<BendSpecification>& bendSequence
);

/**
 * Validate if bending is possible with current tool setup
 * @param tubeOD Tube outer diameter in mm
 * @param bendRadius Target bend radius in mm
 * @return Validation result with error messages if invalid
 */
ValidationResult validateBendParameters(
    double tubeOD,
    double bendRadius
);
```

#### Data Structures

```cpp
struct BendResult {
    double bendAllowance;           // mm
    double springbackAngle;         // degrees
    double requiredForce;           // N
    double bendTime;                // seconds
    std::vector<Point3D> centerline; // 3D centerline points
    bool isValid;
    std::string validationMessage;
};

struct BendSpecification {
    double bendAngle;               // degrees
    double bendRadius;              // mm
    Vector3D bendPlane;             // normal to bend plane
    double straightLength;          // mm before bend
};

struct AssemblyResult {
    double totalUnbentLength;       // mm
    double totalBentLength;         // mm
    std::vector<BendResult> bends;
    bool isComplete;
};
```

#### Error Handling

```cpp
try {
    BendCalculator calc;
    auto result = calc.calculateBend(16, 1.5, 30, 90, 25);
} catch (const std::invalid_argument& e) {
    // Handle invalid parameter
    std::cerr << "Invalid parameter: " << e.what() << std::endl;
} catch (const std::runtime_error& e) {
    // Handle calculation error
    std::cerr << "Calculation failed: " << e.what() << std::endl;
}
```

---

### TransactionManager API

Manages undo/redo functionality and atomic operations.

#### Header
```cpp
#include <bendcore/transaction.h>
```

#### Key Classes

**class `TransactionManager`**

Implements Command pattern for reversible operations.

##### Methods

```cpp
/**
 * Begin a new transaction
 * @param description Human-readable description of the transaction
 * @return Transaction ID for tracking
 */
TransactionId beginTransaction(const std::string& description);

/**
 * Commit current transaction
 * @throws std::logic_error if no active transaction
 */
void commitTransaction();

/**
 * Rollback current transaction
 * @throws std::logic_error if no active transaction
 */
void rollbackTransaction();

/**
 * Undo last transaction
 * @return true if undo successful, false if at beginning
 */
bool undo();

/**
 * Redo last undone transaction
 * @return true if redo successful, false if at end
 */
bool redo();

/**
 * Get current transaction depth
 */
int getTransactionDepth() const;

/**
 * Get transaction history
 */
std::vector<TransactionRecord> getHistory() const;
```

---

## Database APIs

### MaterialDatabase API

Access material properties and specifications.

#### Header
```cpp
#include <bendcore/database_manager.h>
```

#### Key Classes

**class `MaterialDatabase`**

Singleton managing material library.

##### Public Methods

```cpp
/**
 * Get singleton instance
 */
static MaterialDatabase& getInstance();

/**
 * Add new material to database
 * @param material Material specification
 * @throws std::invalid_argument if material invalid
 */
void addMaterial(const Material& material);

/**
 * Retrieve material by ID
 * @param id Material identifier
 * @return Optional material, empty if not found
 */
std::optional<Material> getMaterial(const std::string& id);

/**
 * Get all materials
 */
std::vector<Material> getAllMaterials();

/**
 * Filter materials by type
 * @param type Material type (steel, aluminum, copper, titanium, composite)
 */
std::vector<Material> getMaterialsByType(const std::string& type);

/**
 * Find materials in stock
 */
std::vector<Material> getMaterialsInStock();

/**
 * Query materials by property range
 * @param property Property name
 * @param minValue Minimum value
 * @param maxValue Maximum value
 */
std::vector<Material> queryByProperty(
    const std::string& property,
    double minValue,
    double maxValue
);
```

#### Material Structure

```cpp
struct Material {
    std::string id;                 // unique identifier
    std::string name;               // display name
    std::string type;               // steel, aluminum, copper, titanium, composite
    double density;                 // g/cm³
    double youngModulus;            // GPa
    double yieldStrength;           // MPa
    double ultimateStrength;        // MPa
    double elongationAtBreak;       // %
    double pricePerKg;              // currency
    std::string supplier;
    bool inStock;
    std::map<std::string, double> customProperties;
};
```

---

### MachineDatabase API

Access machine capabilities and specifications.

#### Header
```cpp
#include <bendcore/database_manager.h>
```

#### Key Classes

**class `MachineDatabase`**

Manages machine tool library and capabilities.

##### Public Methods

```cpp
/**
 * Get singleton instance
 */
static MachineDatabase& getInstance();

/**
 * Add machine to database
 */
void addMachine(const Machine& machine);

/**
 * Get machine by ID
 */
std::optional<Machine> getMachine(const std::string& id);

/**
 * Find machines capable of specific bend
 * @param tubeOD Tube outer diameter in mm
 * @param bendRadius Required bend radius in mm
 * @return List of capable machines
 */
std::vector<Machine> findCapableMachines(
    double tubeOD,
    double bendRadius
);

/**
 * Get all machines of specific type
 * @param machineType Type filter (bender, cutter, welder, etc.)
 */
std::vector<Machine> getMachinesByType(const std::string& machineType);
```

---

## Geometry APIs

### Bezier Curve API

Parametric curve representation and operations.

#### Header
```cpp
#include <bendcore/geometry/bezier_curve.h>
```

#### Key Classes

**class `BezierCurve`**

Represents Bezier curves with arbitrary degree.

##### Constructor

```cpp
/**
 * Create Bezier curve from control points
 * @param controlPoints Control points (minimum 2)
 * @throws std::invalid_argument if fewer than 2 control points
 */
explicit BezierCurve(const std::vector<Point3D>& controlPoints);
```

##### Public Methods

```cpp
/**
 * Evaluate curve at parameter t ∈ [0,1]
 * @param t Parameter value
 * @return Point on curve
 */
Point3D evaluate(double t) const;

/**
 * Evaluate curve derivative
 * @param t Parameter value
 * @return First derivative vector
 */
Vector3D evaluateDerivative(double t) const;

/**
 * Get curve length using numerical integration
 * @param tolerance Integration tolerance
 * @return Curve length in mm
 */
double getLength(double tolerance = 1e-6) const;

/**
 * Subdivide curve at parameter value
 * @param t Division parameter
 * @return Pair of resulting Bezier curves
 */
std::pair<BezierCurve, BezierCurve> subdivide(double t) const;

/**
 * Degree elevation
 * @return New Bezier curve with degree increased by 1
 */
BezierCurve elevateDegree() const;

/**
 * Get control points
 */
const std::vector<Point3D>& getControlPoints() const;

/**
 * Get curve degree
 */
int getDegree() const;
```

---

### Mesh Generator API

Generate surface meshes for visualization and analysis.

#### Header
```cpp
#include <bendcore/geometry/mesh_generator.h>
```

#### Key Classes

**class `MeshGenerator`**

Creates triangle meshes from parametric surfaces.

##### Public Methods

```cpp
/**
 * Generate mesh from parametric surface
 * @param surface Surface definition
 * @param uDivisions Number of divisions in u parameter
 * @param vDivisions Number of divisions in v parameter
 * @return Triangle mesh
 */
Mesh generateFromSurface(
    const ParametricSurface& surface,
    int uDivisions,
    int vDivisions
);

/**
 * Generate mesh with adaptive refinement
 * @param surface Surface definition
 * @param tolerance Target mesh tolerance in mm
 * @param maxDivisions Maximum divisions in any direction
 * @return Triangle mesh
 */
Mesh generateAdaptive(
    const ParametricSurface& surface,
    double tolerance,
    int maxDivisions = 1024
);

/**
 * Smooth mesh using Laplacian smoothing
 * @param mesh Input mesh
 * @param iterations Number of smoothing iterations
 * @param factor Smoothing factor ∈ (0,1)
 * @return Smoothed mesh
 */
Mesh smooth(
    const Mesh& mesh,
    int iterations,
    double factor = 0.5
);

/**
 * Compute mesh normals
 * @param mesh Input mesh
 * @return Mesh with vertex normals computed
 */
Mesh computeNormals(const Mesh& mesh);
```

---

## Manufacturing APIs

### ManufacturingProcessor API

G-code generation and manufacturing analysis.

#### Header
```cpp
#include <bendcore/manufacturing.h>
```

#### Key Classes

**class `ManufacturingProcessor`**

Converts design to manufacturing instructions.

##### Public Methods

```cpp
/**
 * Generate G-code for CNC bending machine
 * @param design Design specification
 * @param machine Target machine
 * @param material Material specification
 * @return GCode object with tool paths
 * @throws std::runtime_error if generation fails
 */
GCode generateGCode(
    const TubeDesign& design,
    const Machine& machine,
    const Material& material
);

/**
 * Generate bill of materials
 * @param design Assembly design
 * @return BOM with all components and quantities
 */
BillOfMaterials generateBOM(const AssemblyDesign& design);

/**
 * Estimate manufacturing time
 * @param gcode G-code to analyze
 * @param feedRate Feed rate in mm/min
 * @return Estimated time in minutes
 */
double estimateManufacturingTime(
    const GCode& gcode,
    double feedRate
);

/**
 * Validate manufacturing feasibility
 * @param design Design to validate
 * @param machine Target machine
 * @return Validation result with issues if any
 */
ManufacturingValidation validate(
    const TubeDesign& design,
    const Machine& machine
);
```

#### Data Structures

```cpp
struct GCode {
    std::vector<std::string> instructions;  // G-code lines
    double totalLength;                     // mm
    double estimatedTime;                   // seconds
    std::map<std::string, double> metadata;
};

struct BillOfMaterials {
    std::string assemblyName;
    std::vector<BOMItem> items;
    double totalWeight;                     // kg
    double estimatedCost;                   // currency
};

struct BOMItem {
    std::string partId;
    std::string partName;
    std::string material;
    double length;                          // mm
    double quantity;
    double weight;                          // kg
    double cost;                            // currency
    std::string vendor;
};
```

---

## Material APIs

### Material Properties API

Access and query material properties.

#### Header
```cpp
#include <bendcore/materials/material_library.h>
```

#### Key Functions

```cpp
/**
 * Get material property value
 * @param materialId Material identifier
 * @param propertyName Property name
 * @return Property value, 0 if not found
 */
double getMaterialProperty(
    const std::string& materialId,
    const std::string& propertyName
);

/**
 * Calculate stress in bent tube
 * @param material Material specification
 * @param bendRadius Bend radius in mm
 * @param tubeOD Tube outer diameter in mm
 * @param tubeWall Wall thickness in mm
 * @return Maximum stress in MPa
 */
double calculateBendingStress(
    const Material& material,
    double bendRadius,
    double tubeOD,
    double tubeWall
);

/**
 * Check if material can be bent without fracture
 * @param material Material specification
 * @param bendRadius Target bend radius in mm
 * @param tubeOD Tube outer diameter in mm
 * @return true if safe to bend
 */
bool isBendSafe(
    const Material& material,
    double bendRadius,
    double tubeOD
);

/**
 * Get minimum bend radius for material
 * @param material Material specification
 * @param tubeOD Tube outer diameter in mm
 * @return Minimum bend radius in mm
 */
double getMinimumBendRadius(
    const Material& material,
    double tubeOD
);
```

---

## Optimization APIs

### Performance Optimization API

Memory and computation optimization.

#### Header
```cpp
#include <bendcore/optimization/performance_monitor.h>
```

#### Key Classes

**class `PerformanceMonitor`**

Tracks and analyzes performance metrics.

##### Public Methods

```cpp
/**
 * Start performance measurement
 * @param label Measurement label
 */
void startMeasurement(const std::string& label);

/**
 * Stop measurement and record results
 * @param label Label to match with startMeasurement
 * @return Elapsed time in milliseconds
 */
double stopMeasurement(const std::string& label);

/**
 * Get performance statistics
 * @param label Label to query
 * @return Performance statistics
 */
PerformanceStats getStats(const std::string& label) const;

/**
 * Generate performance report
 */
std::string generateReport() const;

/**
 * Clear all measurements
 */
void reset();
```

#### Parallel Execution

**class `ParallelExecutor`**

Execute tasks in parallel with thread pool.

```cpp
#include <bendcore/parallel/parallel_executor.h>

/**
 * Execute function in parallel on multiple items
 * @param items Items to process
 * @param processor Function to apply
 * @param numThreads Number of threads to use
 * @return Vector of results in same order as input
 */
template <typename T, typename F>
std::vector<typename std::invoke_result<F, T>::type> 
parallelMap(
    const std::vector<T>& items,
    F processor,
    int numThreads = 0  // 0 = auto-detect
);

/**
 * Execute reduction operation in parallel
 * @param items Items to reduce
 * @param identity Initial value
 * @param reducer Binary reduction function
 * @param numThreads Number of threads
 * @return Reduced value
 */
template <typename T, typename F>
T parallelReduce(
    const std::vector<T>& items,
    const T& identity,
    F reducer,
    int numThreads = 0
);
```

---

## Error Handling

### Exception Hierarchy

```cpp
namespace bendcore {
    class BendCoreException : public std::runtime_error { /* ... */ };
    
    class InvalidParameterException : public BendCoreException { /* ... */ };
    class CalculationException : public BendCoreException { /* ... */ };
    class FileException : public BendCoreException { /* ... */ };
    class DatabaseException : public BendCoreException { /* ... */ };
    class ManufacturingException : public BendCoreException { /* ... */ };
}
```

### Best Practices

```cpp
// Always use try-catch for API calls
try {
    auto result = calculator.calculateBend(od, wall, dia, angle, radius);
} catch (const bendcore::InvalidParameterException& e) {
    // Handle invalid input
} catch (const bendcore::CalculationException& e) {
    // Handle calculation error
} catch (const bendcore::BendCoreException& e) {
    // Handle other BendCore errors
}
```

---

## Code Examples

### Example 1: Simple Bend Calculation

```cpp
#include <bendcore/bend_calculator.h>
#include <iostream>

using namespace bendcore;

int main() {
    BendCalculator calc;
    
    try {
        // Calculate a 90-degree bend
        auto result = calc.calculateBend(
            16.0,  // tube OD in mm
            1.5,   // wall thickness in mm
            30.0,  // bend diameter in mm
            90.0,  // bend angle in degrees
            25.0   // bend radius in mm
        );
        
        std::cout << "Bend Allowance: " << result.bendAllowance << " mm\n";
        std::cout << "Springback: " << result.springbackAngle << " degrees\n";
        std::cout << "Required Force: " << result.requiredForce << " N\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
```

### Example 2: Multi-Bend Assembly

```cpp
#include <bendcore/bend_calculator.h>
#include <vector>

int main() {
    BendCalculator calc;
    
    // Create sequence of bends
    std::vector<BendSpecification> bends;
    
    bends.push_back({90.0, 25.0, {0,0,1}, 50.0});  // 90° bend
    bends.push_back({45.0, 20.0, {1,0,0}, 100.0}); // 45° bend
    bends.push_back({90.0, 30.0, {0,1,0}, 75.0});  // 90° bend
    
    auto assembly = calc.calculateAssembly(bends);
    
    std::cout << "Total unbent length: " << assembly.totalUnbentLength << " mm\n";
    std::cout << "Total bent length: " << assembly.totalBentLength << " mm\n";
    std::cout << "Number of bends: " << assembly.bends.size() << "\n";
    
    return 0;
}
```

### Example 3: Material Selection

```cpp
#include <bendcore/database_manager.h>
#include <iostream>

int main() {
    auto& matDb = bendcore::database::MaterialDatabase::getInstance();
    
    // Find all steel materials in stock
    auto steels = matDb.getMaterialsByType("steel");
    
    for (const auto& material : steels) {
        if (material.inStock) {
            std::cout << material.name << ": "
                      << material.yieldStrength << " MPa, "
                      << material.pricePerKg << " per kg\n";
        }
    }
    
    return 0;
}
```

### Example 4: Manufacturing

```cpp
#include <bendcore/manufacturing.h>
#include <iostream>

int main() {
    ManufacturingProcessor processor;
    
    TubeDesign design;  // ... populate design
    Machine machine;    // ... get machine from database
    Material material;  // ... select material
    
    // Generate G-code
    auto gcode = processor.generateGCode(design, machine, material);
    
    // Generate BOM
    AssemblyDesign assembly;  // ... populate assembly
    auto bom = processor.generateBOM(assembly);
    
    std::cout << "Generated " << gcode.instructions.size() << " G-code lines\n";
    std::cout << "Estimated time: " << processor.estimateManufacturingTime(gcode, 500.0) << " minutes\n";
    std::cout << "BOM items: " << bom.items.size() << "\n";
    std::cout << "Estimated cost: " << bom.estimatedCost << "\n";
    
    return 0;
}
```

---

## Performance Considerations

- **Memory**: BendCore uses efficient data structures with lazy evaluation where possible
- **Parallelization**: Multi-bend calculations can be parallelized using `ParallelExecutor`
- **Caching**: Frequently accessed geometries are cached for performance
- **Precision**: All calculations use double-precision floating-point (64-bit)

## Thread Safety

- All database classes are thread-safe using internal synchronization
- `TransactionManager` supports nested transactions
- Geometry operations are generally thread-safe unless noted otherwise

---

**Document Version History:**
- v2.1.0 - October 26, 2026 - Production release
- v2.0.0 - October 1, 2026 - Complete API reference
- v1.0.0 - September 1, 2026 - Initial version
