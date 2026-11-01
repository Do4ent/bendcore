# OCCT Integration Guide

## Overview

BendCore integrates Open Cascade Technology (OCCT) 8.0 for professional CAD file processing and parametric dimension extraction.

## System Requirements

- **Platform**: Linux / macOS (static OCCT via `third_party/build_occt.sh`), Windows x64
- **OCCT Version**: 8.0.1 (default in build_occt.sh, tested on Linux: 84/84 ctest); 7.8+ required (`TopTools_ShapeMapHasher`)
- **OCCT modules**: FoundationClasses, ModelingData, ModelingAlgorithms, DataExchange
- **C++ Standard**: C++20

## Build Configuration

### CMake Setup
```bash
(cd third_party && ./build_occt.sh)   # -> third_party/occt-install (static libTK*.a)
cmake -B build -DBENDCORE_WITH_OCCT=ON -DBENDCORE_OCCT_DIR=$PWD/third_party/occt-install
cmake --build build
```

### vcpkg Setup
```bash
vcpkg install opencascade:x64-windows
```

## Supported STEP Versions

- AP203: Configuration Controlled Design
- AP214: Automotive Design
- AP242: Managed Model Based 3D Engineering

## Performance Characteristics

- **Typical Import Time**: <1 second for 100MB files
- **Memory Usage**: ~200MB base + file-dependent
- **Cache Hit Benefit**: 90% faster re-import
- **Maximum File Size**: 1GB (limited by available RAM)

## Memory Management

- **Cache Limit**: 100MB LRU eviction
- **Shape Data**: Stored in memory cache
- **Cleanup**: Automatic on cache eviction or program exit

## Precision Settings

- **Default Tolerance**: 0.01mm
- **Angle Precision**: 0.1 degrees
- **Cylinder Fit Tolerance**: 0.05mm

## Troubleshooting

### OCCT Version Check Failed
```cpp
auto info = bendcore::occt::get_occt_info();
// Check info.version_major >= 7, info.version_minor >= 7
```

### File Not Loading
- Verify STEP file is not corrupted
- Check file permissions
- Try exporting from source CAD software again

### Low Confidence Results
- Check geometry for non-cylindrical shapes
- Verify wall thickness ratios are realistic
- Complex assemblies may need manual review

