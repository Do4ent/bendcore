# Building BendCore with Desktop UI

## Overview

This guide explains how to build the complete BendCore system with the Electron + React desktop UI.

## Prerequisites

### Required

- **CMake** 3.20+
- **C++20 compiler** (MSVC 2022 or GCC 11+)
- **Node.js** 18+ (for UI and native module)
- **npm** or **yarn** (Node.js package manager)

### Optional

- **Eigen3** (for 2D sketch solver)
- **Open CASCADE** (for 3D geometry)
- **Python** 3.8+ (for test data generation)

## Quick Start

### 1. Build the C++ Core Library

```bash
cd bendcore-remote
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
cd ..
```

This produces:
- `build/lib/libbendcore_core.a` (static library)
- Test executables in `build/`

### 2. Build the Native Module

```bash
cd bendcore-ui
npm install
npm run build:native
```

This compiles the Node.js binding and produces:
- `bendcore-ui/build/Release/bendcore.node`

### 3. Build the UI

```bash
npm run build
```

This creates the Electron application with packaged UI.

### 4. Package for Distribution

```bash
npm run pack
```

Creates Windows installer (`.exe`) and portable executable.

## Build Configurations

### Windows x64 (Primary - v1)

```bash
cd bendcore-remote/build
cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

Then build UI:
```bash
cd ../bendcore-ui
npm install
npm run pack
```

### Linux (Development)

```bash
cd bendcore-remote/build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

### macOS (Future - v2)

```bash
cd bendcore-remote/build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(sysctl -n hw.ncpu)
```

## Project Structure

```
bendcore-remote/
├── include/           # C++ headers
├── src/              # C++ source files
├── native/           # Node.js N-API binding
├── bendcore-ui/      # Electron + React UI
├── binding.gyp       # Native module build config
├── CMakeLists.txt    # C++ library build config
└── README.md         # Library documentation
```

## Development Workflow

### Developing C++ Core

1. Edit files in `src/`
2. Rebuild: `cd build && cmake --build .`
3. Test: `ctest`

### Developing Native Module Binding

1. Edit `native/bendcore.cc`
2. Rebuild: `npm run build:native`
3. Test in UI

### Developing UI

1. Start dev server: `cd bendcore-ui && npm run dev`
2. Hot-reload works for React components
3. Restart needed for IPC handler changes

### Integrated Development

For active development on both C++ and UI:

```bash
# Terminal 1: Watch C++ changes
cd bendcore-remote/build
cmake --build . --watch

# Terminal 2: Watch native module and rebuild
cd bendcore-ui
npm run build:native -- --watch

# Terminal 3: Run UI dev server
cd bendcore-ui
npm run dev
```

## Build Outputs

### C++ Library
- **Library**: `build/lib/libbendcore_core.a` (Windows: `.lib`)
- **Headers**: Ready to use in `include/`

### Native Module
- **Module**: `bendcore-ui/build/Release/bendcore.node`
- **Type definitions**: Handled by TypeScript

### UI Application
- **Development**: `npm run dev` - Electron with hot-reload
- **Production**: `npm run pack` - Creates installer/portable exe
- **Output**: `bendcore-ui/dist/` (packaged app resources)

## Environment Variables

### C++ Build
```bash
BENDCORE_EIGEN_DIR    # Path to Eigen3 include directory
CMAKE_BUILD_TYPE      # Release, Debug, RelWithDebInfo
```

### UI Build
```bash
NODE_ENV              # development, production
ELECTRON_BUILDER_...  # electron-builder configuration
```

## Troubleshooting

### CMake Not Found
```bash
# Windows
choco install cmake
# macOS
brew install cmake
# Linux
sudo apt-get install cmake
```

### Node.js Version Issues
```bash
node --version        # Should be 18+
npm install -g npm    # Update npm
```

### Native Module Build Fails
1. Ensure C++ core library is built first
2. Check `binding.gyp` library paths
3. Verify compiler is installed: `cl /version` (Windows)

### UI Won't Launch
1. Check console for missing native module warning
2. Verify `build/Release/bendcore.node` exists
3. Check Electron version compatibility

## Continuous Integration

The project includes GitHub Actions workflows:

- `.github/workflows/build.yml` - Build on Windows
- `.github/workflows/test.yml` - Run tests

View workflows in the repository.

## Performance Tips

### Build Speed
- Use `-j$(nproc)` for parallel builds
- Use incremental builds (rebuilding after small changes)
- Consider ccache (Linux/macOS)

### Development Speed
- Use `npm run dev` for hot-reload UI
- Use native debugger for C++ code
- Use DevTools for React debugging

## Future Considerations

### v1 (Current)
- Windows x64 only
- Desktop application
- DWFx import support

### v2 (Planned)
- macOS and Linux builds
- WebAssembly for browser
- Plugin system

### v3+ (Future)
- Mobile apps (via Capacitor)
- Cloud synchronization
- Advanced CAD features

## References

- [CMake Documentation](https://cmake.org/cmake/help/latest/)
- [Electron Docs](https://www.electronjs.org/docs)
- [Node.js N-API](https://nodejs.org/api/n_api.html)
- [BendCore C++ API](./README.md)
