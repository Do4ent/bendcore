# BendCore CAD Development Guide

**Document Version:** 2.1.0  
**Last Updated:** October 26, 2026  
**Status:** Production Ready

## Table of Contents

1. [Development Environment Setup](#development-environment-setup)
2. [Build Instructions](#build-instructions)
3. [Project Structure](#project-structure)
4. [Coding Standards](#coding-standards)
5. [Testing Procedures](#testing-procedures)
6. [Contribution Guidelines](#contribution-guidelines)
7. [Performance Best Practices](#performance-best-practices)
8. [Debugging Guide](#debugging-guide)

---

## Development Environment Setup

### Prerequisites

- **Compiler**: GCC 11+ or Clang 13+ (C++20 support required)
- **CMake**: 3.20 or higher
- **Git**: 2.40 or higher
- **Python**: 3.8+ (for build scripts)
- **Node.js**: 18+ (for web components)

### Linux/macOS Setup

```bash
# Clone repository
git clone https://github.com/Do4ent/bendcore.git
cd bendcore

# Create build directory
mkdir -p build_dev
cd build_dev

# Configure with development flags
cmake .. \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_COMPILER=g++ \
    -DCMAKE_C_COMPILER=gcc \
    -DENABLE_TESTING=ON \
    -DENABLE_DOCUMENTATION=ON

# Build
cmake --build . -j$(nproc)

# Run tests
ctest --output-on-failure
```

### Windows Setup (MSVC)

```batch
# Clone repository
git clone https://github.com/Do4ent/bendcore.git
cd bendcore

# Create build directory
mkdir build_dev
cd build_dev

# Configure
cmake .. -G "Visual Studio 17 2022" ^
    -DCMAKE_BUILD_TYPE=Debug ^
    -DENABLE_TESTING=ON ^
    -DENABLE_DOCUMENTATION=ON

# Build
cmake --build . --config Debug

# Run tests
ctest -C Debug --output-on-failure
```

### WebAssembly Build

```bash
# Install Emscripten SDK
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh

# Build WebAssembly
cd /path/to/bendcore
./emscripten_build.sh
```

---

## Build Instructions

### Standard Build

```bash
cd bendcore
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j4
```

### Build Options

```bash
# Enable documentation generation (requires Doxygen)
cmake .. -DENABLE_DOCUMENTATION=ON

# Enable testing
cmake .. -DENABLE_TESTING=ON

# Enable performance profiling
cmake .. -DENABLE_PROFILING=ON

# Enable thread sanitizer
cmake .. -DENABLE_TSAN=ON

# Enable address sanitizer
cmake .. -DENABLE_ASAN=ON

# Build specific target
cmake --build . --target bendcore_core
cmake --build . --target test_bend_calculator

# Verbose output
cmake --build . --verbose
```

### Documentation Generation

```bash
# Generate documentation with Doxygen
doxygen Doxyfile

# Output location: docs/generated/html/index.html
# Open in browser
open docs/generated/html/index.html  # macOS
xdg-open docs/generated/html/index.html  # Linux
start docs\generated\html\index.html  # Windows
```

### Installation

```bash
# Install to system directories
cmake --install . --prefix /usr/local

# Install to custom location
cmake --install . --prefix /opt/bendcore
```

---

## Project Structure

```
bendcore/
├── include/
│   └── bendcore/
│       ├── bend_calculator.h      # Core bending API
│       ├── database_manager.h     # Material/machine databases
│       ├── geometry/              # Geometry operations
│       ├── manufacturing.h        # G-code generation
│       ├── materials/             # Material properties
│       ├── optimization/          # Performance optimization
│       ├── parallel/              # Parallel execution
│       └── ...
├── src/
│   ├── bend_calculator.cpp
│   ├── database_manager.cpp
│   ├── geometry/
│   ├── manufacturing.cpp
│   ├── materials/
│   ├── optimization/
│   ├── server/                    # C++ backend server
│   │   ├── main.cpp
│   │   └── tube_service.cpp
│   └── ...
├── tests/
│   ├── unit/
│   │   ├── test_bend_calculator.cpp
│   │   ├── test_database.cpp
│   │   ├── test_geometry.cpp
│   │   └── ...
│   ├── integration/
│   │   └── test_e2e_workflows.cpp
│   └── ...
├── docs/
│   ├── API_REFERENCE.md           # API documentation (this file)
│   ├── ARCHITECTURE.md            # System architecture
│   ├── DEVELOPMENT_GUIDE.md       # (this file)
│   ├── CONFIGURATION.md           # Configuration guide
│   ├── CODING_STANDARDS.md        # Code style guidelines
│   └── generated/                 # Doxygen output
├── ui/                            # React frontend
│   └── src/
│       ├── components/
│       ├── pages/
│       └── App.tsx
├── backend/                       # Node.js backend
│   ├── src/
│   │   ├── api/
│   │   ├── services/
│   │   └── server.js
│   └── ...
├── CMakeLists.txt
├── Doxyfile                       # Doxygen configuration
└── README.md
```

---

## Coding Standards

### C++ Code Style

#### General Rules

- **Language Standard**: C++20
- **Indentation**: 4 spaces (no tabs)
- **Line Length**: Maximum 120 characters
- **Naming Conventions**:
  - Classes: `PascalCase`
  - Functions: `camelCase()`
  - Variables: `snake_case`
  - Constants: `SCREAMING_SNAKE_CASE`
  - Private members: `member_name_`
  - Template parameters: `T`, `U`, `Container`, etc.

#### Example

```cpp
namespace bendcore {

// Class definition
class BendCalculator {
public:
    // Public interface
    explicit BendCalculator(const Config& config = {});
    
    BendResult calculateBend(
        double tubeOD,
        double wallThickness,
        double bendDiameter,
        double bendAngle,
        double bendRadius
    );
    
    // Constants
    static constexpr double MIN_BEND_RADIUS = 1.0;
    static constexpr double MAX_BEND_ANGLE = 360.0;

private:
    // Private members
    Config config_;
    std::vector<CachedBend> bend_cache_;
    
    // Private methods
    double calculateSpringback(
        const Material& material,
        double bendRadius
    );
};

}  // namespace bendcore
```

#### Documentation Comments

All public API elements must have documentation comments:

```cpp
/**
 * Calculate bend parameters for a single bend operation.
 * 
 * This function performs detailed bend analysis including springback
 * prediction and force calculation using material properties.
 * 
 * @param tubeOD Outer diameter of tube in mm, must be > 0
 * @param tubeWall Wall thickness in mm, must be > 0 and < tubeOD/2
 * @param bendDiameter Bend diameter (centerline) in mm, must be > tubeOD
 * @param bendAngle Target bend angle in degrees, valid range [0, 360]
 * @param bendRadius Bend radius in mm, must be >= MIN_BEND_RADIUS
 * 
 * @return BendResult structure containing:
 *         - bendAllowance: Calculated allowance in mm
 *         - springbackAngle: Predicted springback in degrees
 *         - requiredForce: Maximum required force in Newtons
 *         - centerline: 3D points of tube centerline
 *         - isValid: Whether result is valid for manufacturing
 * 
 * @throws std::invalid_argument if any parameter is out of valid range
 * @throws std::runtime_error if calculation fails internally
 * 
 * @note Springback is automatically compensated if compensation table exists
 * @see validateBendParameters() for pre-calculation validation
 * @since v2.0.0
 */
BendResult calculateBend(
    double tubeOD,
    double tubeWall,
    double bendDiameter,
    double bendAngle,
    double bendRadius
);
```

#### Memory Management

Use RAII principles and smart pointers:

```cpp
// Good: RAII
class GeometryCache {
public:
    void addMesh(const std::string& key, std::unique_ptr<Mesh> mesh) {
        cache_[key] = std::move(mesh);
    }
    
private:
    std::map<std::string, std::unique_ptr<Mesh>> cache_;
};

// Avoid: Raw pointers for ownership
class BadExample {
public:
    ~BadExample() {
        delete mesh_;  // error-prone
    }
private:
    Mesh* mesh_;  // Don't do this
};
```

#### Error Handling

Use exceptions for error conditions:

```cpp
// Good: Clear error handling
try {
    auto result = calculator.calculateBend(od, wall, dia, angle, radius);
    return result;
} catch (const std::invalid_argument& e) {
    logger.error("Invalid parameter: " + std::string(e.what()));
    throw;
} catch (const std::exception& e) {
    logger.error("Unexpected error: " + std::string(e.what()));
    throw;
}

// Avoid: Silent failures
double result = -1.0;
if (!calculator.calculateBend(od, wall, dia, angle, radius, result)) {
    // What does false mean? Not obvious.
}
```

### Commit Message Format

Follow conventional commits:

```
<type>(<scope>): <subject>

<body>

<footer>
```

Examples:
```
feat(bend-calculator): add springback compensation

The calculator now includes automatic springback compensation
based on material properties and bend parameters.

Closes #1234

docs(api): update BendCalculator API reference

Add missing parameter documentation and examples.
```

---

## Testing Procedures

### Unit Testing

```bash
# Run all tests
ctest

# Run specific test
ctest --output-on-failure -R test_bend_calculator

# Run with verbose output
ctest --verbose

# Show failed tests details
ctest --output-on-failure

# Run tests in parallel (4 threads)
ctest -j4
```

### Writing Unit Tests

Use Google Test framework:

```cpp
#include <gtest/gtest.h>
#include <bendcore/bend_calculator.h>

using namespace bendcore;

class BendCalculatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        calculator_ = std::make_unique<BendCalculator>();
    }
    
    std::unique_ptr<BendCalculator> calculator_;
};

TEST_F(BendCalculatorTest, CalculateBend_ValidInput_ReturnsValidResult) {
    auto result = calculator_->calculateBend(16, 1.5, 30, 90, 25);
    
    ASSERT_TRUE(result.isValid);
    EXPECT_GT(result.bendAllowance, 0.0);
    EXPECT_LT(result.springbackAngle, 5.0);
}

TEST_F(BendCalculatorTest, CalculateBend_InvalidDiameter_ThrowsException) {
    EXPECT_THROW(
        calculator_->calculateBend(-16, 1.5, 30, 90, 25),
        std::invalid_argument
    );
}

TEST_F(BendCalculatorTest, CalculateBend_VariousAngles_ProducesConsistentResults) {
    for (int angle = 10; angle <= 90; angle += 10) {
        auto result = calculator_->calculateBend(16, 1.5, 30, angle, 25);
        EXPECT_TRUE(result.isValid) << "Failed for angle: " << angle;
    }
}
```

### Integration Testing

```cpp
#include <gtest/gtest.h>
#include <bendcore/bend_calculator.h>
#include <bendcore/manufacturing.h>
#include <bendcore/database_manager.h>

TEST(IntegrationTest, EndToEndDesignToGCode) {
    // 1. Create design
    BendCalculator calc;
    auto bend1 = calc.calculateBend(16, 1.5, 30, 90, 25);
    auto bend2 = calc.calculateBend(16, 1.5, 40, 45, 20);
    
    // 2. Get manufacturing info
    auto& machineDb = MachineDatabase::getInstance();
    auto machines = machineDb.getMachinesByType("bender");
    
    auto& matDb = MaterialDatabase::getInstance();
    auto materials = matDb.getMaterialsByType("steel");
    
    // 3. Generate G-code
    ManufacturingProcessor processor;
    TubeDesign design;
    design.bends = {bend1, bend2};
    
    auto gcode = processor.generateGCode(design, machines[0], materials[0]);
    
    // Verify results
    EXPECT_GT(gcode.instructions.size(), 0);
    EXPECT_GT(gcode.totalLength, 0);
}
```

### Performance Testing

```cpp
TEST(PerformanceTest, BendCalculation_1000Iterations) {
    BendCalculator calc;
    PerformanceMonitor monitor;
    
    monitor.startMeasurement("bend_calculation");
    
    for (int i = 0; i < 1000; ++i) {
        auto result = calc.calculateBend(16, 1.5, 30, 90, 25);
        ASSERT_TRUE(result.isValid);
    }
    
    auto elapsed = monitor.stopMeasurement("bend_calculation");
    
    // Should complete in reasonable time (< 100ms for 1000 iterations)
    EXPECT_LT(elapsed, 100.0);
}
```

---

## Contribution Guidelines

### Before Contributing

1. Fork the repository on GitHub
2. Create a feature branch: `git checkout -b feature/your-feature-name`
3. Ensure development environment is set up correctly
4. Review [CODING_STANDARDS.md](./CODING_STANDARDS.md)

### Making Changes

1. Write tests first (TDD approach)
2. Implement feature/fix
3. Ensure all tests pass
4. Update documentation
5. Check code coverage

### Submitting Pull Request

1. Push to your feature branch
2. Create Pull Request with clear description
3. Reference related issues using `#ISSUE_NUMBER`
4. Wait for CI/CD checks to pass
5. Address review comments
6. Squash commits if requested

### Code Review Checklist

- [ ] Code follows style guidelines
- [ ] All tests pass locally
- [ ] New tests added for new functionality
- [ ] Documentation updated
- [ ] No performance regressions
- [ ] Error handling is appropriate
- [ ] Memory management is correct (RAII)
- [ ] Thread-safety considered where applicable

---

## Performance Best Practices

### Optimization Guidelines

#### 1. Profile Before Optimizing

```cpp
#include <bendcore/optimization/performance_monitor.h>

PerformanceMonitor monitor;

monitor.startMeasurement("critical_section");
// ... code to optimize
auto elapsed = monitor.stopMeasurement("critical_section");

auto stats = monitor.getStats("critical_section");
std::cout << stats.averageTime << "ms average\n";
std::cout << stats.maxTime << "ms maximum\n";
```

#### 2. Use Parallel Processing

```cpp
#include <bendcore/parallel/parallel_executor.h>

std::vector<TubeDesign> designs = /* ... */;

auto results = bendcore::parallel::parallelMap(
    designs,
    [](const TubeDesign& design) {
        return calculateBend(design);
    },
    4  // Use 4 threads
);
```

#### 3. Cache Results

```cpp
class OptimizedCalculator {
private:
    std::map<std::string, BendResult> cache_;
    
    std::string makeCacheKey(double od, double wall, double dia, double angle) {
        return std::to_string(od) + "_" + 
               std::to_string(wall) + "_" + 
               std::to_string(dia) + "_" + 
               std::to_string(angle);
    }
    
public:
    BendResult calculateBend(double od, double wall, double dia, double angle) {
        auto key = makeCacheKey(od, wall, dia, angle);
        
        auto it = cache_.find(key);
        if (it != cache_.end()) {
            return it->second;  // Cache hit
        }
        
        auto result = performCalculation(od, wall, dia, angle);
        cache_[key] = result;
        return result;
    }
};
```

#### 4. Avoid Unnecessary Copies

```cpp
// Good: Pass by reference for large objects
void process(const std::vector<Point3D>& points) {
    // Don't copy the vector
}

// Good: Move semantics
void setPoints(std::vector<Point3D>&& points) {
    points_ = std::move(points);  // Efficient transfer
}

// Avoid: Unnecessary copy
void process(std::vector<Point3D> points) {
    // points is copied!
}
```

---

## Debugging Guide

### GDB Debugging

```bash
# Build with debug symbols
cmake .. -DCMAKE_BUILD_TYPE=Debug

# Run with GDB
gdb ./build/bin/bendcore_app

# In GDB:
(gdb) run
(gdb) break BendCalculator::calculateBend
(gdb) continue
(gdb) print result
(gdb) backtrace
(gdb) step
(gdb) next
```

### Address Sanitizer

```bash
# Build with ASan
cmake .. -DENABLE_ASAN=ON
cmake --build .

# Run (memory errors will be reported)
./build/bin/test_runner
```

### Thread Sanitizer

```bash
# Build with TSan
cmake .. -DENABLE_TSAN=ON
cmake --build .

# Run (race conditions will be reported)
./build/bin/test_runner
```

### Valgrind

```bash
# Check for memory leaks
valgrind --leak-check=full ./build/bin/test_runner

# With detailed output
valgrind --leak-check=full --show-leak-kinds=all ./build/bin/test_runner
```

### Debug Logging

```cpp
#include <iostream>
#include <spdlog/spdlog.h>

auto logger = spdlog::get("bendcore");

// Log levels
logger->trace("Trace message");
logger->debug("Debug message");
logger->info("Info message");
logger->warn("Warning message");
logger->error("Error message");
logger->critical("Critical message");

// Formatted logging
logger->info("Calculated bend: {:.2f}° springback", springback);
logger->debug("Cache hit: {} items", cache_.size());
```

---

**Document Version History:**
- v2.1.0 - October 26, 2026 - Production release
- v2.0.0 - October 1, 2026 - Complete development guide
- v1.0.0 - September 1, 2026 - Initial version

---

## Quick Reference

### Build Commands
```bash
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j4
ctest --output-on-failure
```

### Documentation
```bash
doxygen Doxyfile
open docs/generated/html/index.html
```

### Common Issues
- Missing dependencies: `sudo apt-get install libcgal-dev libeigen3-dev`
- CMake version: `cmake --version` must be 3.20+
- C++ standard: Check compiler supports C++20

For more help, see [FAQ](./FAQ.md) and [TROUBLESHOOTING](./TROUBLESHOOTING.md).
