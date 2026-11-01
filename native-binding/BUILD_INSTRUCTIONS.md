# OCCT Node.js Binding - Build Instructions

## Requirements

### Windows (Recommended)
- Visual Studio 2019 or 2022 with C++ workload
- Python 3.8+ (for node-gyp)
- Node.js 18+ (with npm)
- CMake 3.20+ (for OCCT)
- OpenCASCADE 7.7.0 or later

### Linux
- GCC 8+ or Clang 10+
- Python 3.8+
- Node.js 18+
- CMake 3.20+
- OpenCASCADE 7.7.0

### macOS
- Xcode Command Line Tools
- Python 3.8+
- Node.js 18+
- CMake 3.20+
- OpenCASCADE 7.7.0

## Step 1: Install OpenCASCADE

### Windows
```powershell
# Option A: Download pre-built binaries
# From https://dev.opencascade.org/release
# Extract to C:\occt

# Option B: Build from source
cd C:\projects
git clone https://github.com/Open-Cascade-SAS/OCCT.git occt-src
cd occt-src
mkdir build
cd build
cmake .. -G "Visual Studio 16 2019" -DCMAKE_INSTALL_PREFIX="C:\occt"
cmake --build . --config Release
cmake --install .
```

### Linux
```bash
# Install dependencies
sudo apt-get install -y \
  build-essential \
  cmake \
  libxmu-dev \
  libxi-dev \
  libgl1-mesa-dev \
  libxt-dev

# Build OCCT
cd ~/projects
git clone https://github.com/Open-Cascade-SAS/OCCT.git occt-src
cd occt-src
mkdir build
cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local/occt
make -j4
sudo make install
```

## Step 2: Set OCCT_PATH Environment Variable

### Windows (PowerShell)
```powershell
$env:OCCT_PATH = "C:\occt"
[Environment]::SetEnvironmentVariable("OCCT_PATH", "C:\occt", "User")
```

### Windows (Command Prompt)
```cmd
set OCCT_PATH=C:\occt
setx OCCT_PATH C:\occt
```

### Linux/macOS
```bash
export OCCT_PATH=/usr/local/occt
echo 'export OCCT_PATH=/usr/local/occt' >> ~/.bashrc
source ~/.bashrc
```

## Step 3: Install Node.js Dependencies

```bash
cd /home/user/bendcore/native-binding
npm install
```

## Step 4: Build Native Binding

### Windows
```powershell
cd C:\path\to\bendcore\native-binding
npm run build:windows
```

### Linux/macOS
```bash
cd /path/to/bendcore/native-binding
npm run build
```

### Debug Build
```bash
npm run build:debug
```

## Step 5: Test the Build

```bash
npm run test:native
```

Expected output:
```
Testing OCCT binding...
✓ Module loaded
✓ Shape creation working
✓ STEP file reading working
✓ Geometry analysis working
```

## Common Issues

### Issue: "OCCT_PATH not found"
**Solution:** Set the environment variable correctly
```powershell
# Verify OCCT installation
Test-Path "C:\occt\lib\TKernel.lib"  # Should return True
```

### Issue: "node-gyp not found"
**Solution:** Install globally
```bash
npm install -g node-gyp
```

### Issue: "Python not found"
**Solution:** Install Python 3.8+
- Windows: https://www.python.org/downloads/
- Linux: `sudo apt-get install python3`
- macOS: `brew install python3`

### Issue: "Visual Studio compiler not found"
**Solution:** Install C++ build tools
- Download: https://visualstudio.microsoft.com/downloads/
- Select "Desktop development with C++"
- Or install Visual C++ Build Tools separately

## Build Structure

```
native-binding/
├── src/
│   ├── addon.cc          # Main module initialization
│   ├── shape_wrapper.cc  # Shape wrapper class
│   ├── step_reader.cc    # STEP file reading
│   └── geometry_kernel.cc # Geometry operations
├── include/
│   └── (header files)
├── build/                # Build output (created after build)
│   └── Release/
│       └── occt_binding.node  # Compiled binary
├── lib/
│   └── (TypeScript definitions)
├── tests/
│   └── test-binding.js   # Native binding tests
├── binding.gyp          # node-gyp configuration
└── package.json         # NPM package config
```

## Integration with BendCore

Once built, the binding will be available at:
```
build/Release/occt_binding.node  (Windows/Linux)
build/Debug/occt_binding.node    (Debug)
```

To use in Node.js:
```javascript
const occt = require('./native-binding/build/Release/occt_binding');

// Create a box
const box = occt.kernel.makeBox(100, 100, 100);
console.log(box.volume);  // 1000000

// Read STEP file
const geometry = occt.readStepFile('model.step');
console.log(geometry.boundingBox);

// Analyze geometry
const analysis = occt.analyzeGeometry(geometry);
console.log(analysis.tubeInfo);
```

## Next Steps

1. **After successful build:**
   - Update `backend/src/occt/index.ts` to load the binding
   - Create REST API endpoints that use the binding
   - Connect to frontend

2. **Additional features:**
   - Implement IGES support
   - Add SAT format support
   - Create mesh generation
   - Add collision detection

3. **Performance optimization:**
   - Profile bottlenecks
   - Implement caching
   - Optimize memory usage

## Troubleshooting

For detailed error messages, run:
```bash
node-gyp rebuild --verbose
```

Check build log:
```bash
# Windows
type build\gyp-mac-tool
# Linux/macOS
cat build/config.gypi
```

## Support

For build issues:
1. Check OCCT installation: `$OCCT_PATH/lib` should contain `.lib` files (Windows)
2. Verify Node.js version: `node --version` (should be 18+)
3. Check Python: `python --version` (should be 3.8+)
4. Check Visual Studio: Run `cl.exe` in command line

## Performance Notes

- First build takes 2-5 minutes (OCCT compilation)
- Incremental builds: 30-60 seconds
- Binding size: ~2-5 MB
- Memory usage: Depends on geometry complexity

## License

MIT License - See LICENSE file in root of BendCore project
