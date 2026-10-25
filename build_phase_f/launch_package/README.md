# BendCore CAD - Professional Tube Bending Design System

**Version**: 6.0.0 (Production Launch)
**Build Date**: Oct 25 2026
**Status**: Production Ready

## Overview

BendCore CAD is a professional-grade 3D computer-aided design (CAD) system specialized 
for tube and pipe assembly design. It combines advanced geometric modeling with real-world 
manufacturing constraints to deliver production-ready designs.

## Key Features

- **Parametric Modeling**: Dimension-driven design with automatic updates
- **Pipe Assembly Design**: Specialized tools for tube routing and bending
- **Advanced Geometry**: NURBS curves, B-Rep solids, and surface operations
- **Real-time Collaboration**: Multi-user editing with conflict resolution
- **Manufacturing Integration**: BOM generation, drawing automation, cost estimation
- **FEA Integration**: Stress analysis and deformation prediction
- **Cross-Platform**: Linux, Windows, macOS support

## Installation

See [INSTALL.md](INSTALL.md) for detailed installation instructions.

### Quick Start

```bash
# Clone the repository
git clone https://github.com/Do4ent/bendcore.git
cd bendcore

# Build
mkdir build && cd build
cmake ..
make -j$(nproc)

# Run tests
make test
```

## Documentation

- [API Reference](API.md) - Developer API documentation
- [Build Guide](BUILD.md) - Building from source
- [Testing Guide](TESTING.md) - Running the test suite
- [Performance](PERFORMANCE.md) - Performance benchmarks
- [Security](SECURITY.md) - Security considerations
- [Deployment](DEPLOYMENT.md) - Production deployment

## Requirements

- C++20 compiler (GCC 13.3+, Clang 14+, MSVC 2019+)
- CMake 3.20+
- OpenCASCADE (OCCT) 7.6+
- Node.js 18+ (for UI)

## Features by Phase

### Phase A: Document Model
- Entity ID system with type safety
- CAD document with hierarchy support
- Unified entity access patterns

### Phase B: Commands & Transactions
- Command engine for operations
- Transaction-based undo/redo
- Event bus for observers

### Phase C: Parameters & Dependencies
- Type-safe parameter storage
- Dependency graph tracking
- Incremental rebuild engine

### Phase D: Geometry & Topology
- 3D geometric primitives
- B-Rep topology (vertex, edge, face, shell, solid)
- NURBS curves and surfaces
- Boolean operations

### Phase E: Persistence
- Document serialization
- Multiple file formats
- Import/export capabilities

### Phase F: Production Launch
- Comprehensive test suite (50+ tests)
- Performance profiling and optimization
- Production readiness validation
- Complete documentation

## License

This project is licensed under the [Production License](LICENSE).

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for contribution guidelines.

## Support

For support, please open an issue on GitHub or contact the development team.

---

**BendCore CAD v6.0.0 - Production Ready**
