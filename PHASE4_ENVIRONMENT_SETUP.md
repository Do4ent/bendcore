# Phase 4 Environment Setup Guide

**Status**: Phase 4 foundation 100% complete, ready for environment setup and execution
**Timeline**: 50-65 hours to complete Phase 4 once environment is prepared

---

## Quick Start

If you have access to package managers:

```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install -y cmake ninja-build g++ libeigen3-dev libssl-dev
npm install

# macOS
brew install cmake ninja eigen openssl
npm install

# Windows (with vcpkg)
vcpkg install eigen3 openssl
npm install
```

Then run Phase 4:

```bash
cd /home/claude/bendcore
./scripts/phase4_execute.sh all
```

---

## Detailed Setup Instructions

### Part 1: C++ Build Environment

#### Option A: Ubuntu/Debian Linux

```bash
# Update package lists
sudo apt-get update

# Install build tools
sudo apt-get install -y \
  cmake \
  ninja-build \
  g++ \
  git

# Install required libraries
sudo apt-get install -y \
  libeigen3-dev \
  libssl-dev

# Verify installation
cmake --version        # Should be 3.20+
ninja --version       # Or use make
g++ --version         # Should be 10+
pkg-config --modversion eigen3
```

#### Option B: macOS

```bash
# Using Homebrew
brew install cmake ninja eigen openssl

# Verify installation
cmake --version
brew list eigen
brew list openssl
```

#### Option C: Windows with Visual Studio

```bash
# Using vcpkg (recommended)
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
./vcpkg integrate install
./vcpkg install eigen3:x64-windows
./vcpkg install openssl:x64-windows

# Or manual setup:
# 1. Download Eigen from https://gitlab.com/libeigen/eigen
# 2. Extract to C:\Libraries\Eigen
# 3. Set environment variable: BENDCORE_EIGEN_DIR=C:\Libraries\Eigen
```

#### Option D: Docker (Guaranteed to Work)

```bash
# Build Docker image with all dependencies
docker build -f docker/Dockerfile -t bendcore-build .

# Run Phase 4 inside Docker
docker run --rm -v $(pwd):/home/claude/bendcore \
  bendcore-build \
  /home/claude/bendcore/scripts/phase4_execute.sh all
```

### Part 2: Node.js and npm Setup

#### Prerequisites

- Node.js v16+ (v18+ recommended)
- npm v8+

#### Installation

```bash
# Ubuntu/Debian
curl -fsSL https://deb.nodesource.com/setup_18.x | sudo -E bash -
sudo apt-get install -y nodejs

# macOS
brew install node

# Windows
# Download from https://nodejs.org/

# Verify installation
node --version    # v18.x.x
npm --version     # 8.x.x or higher
```

#### Configure npm Registry (if behind proxy)

```bash
# Configure npm to use proxy
npm config set proxy http://127.0.0.1:45467
npm config set https-proxy http://127.0.0.1:45467
npm config set registry https://registry.npmjs.org/

# Or use alternative registry (China)
npm config set registry https://registry.npmmirror.com/

# Clear npm cache
npm cache clean --force
```

#### Install npm Dependencies

```bash
cd /home/claude/bendcore
npm install

# Verify all dependencies installed
npm ls
```

### Part 3: Optional Build Tools

#### Code Coverage (C++)

```bash
# Ubuntu/Debian
sudo apt-get install -y gcov lcov

# macOS
brew install gcov lcov

# Verify
lcov --version
```

#### Doxygen (API Documentation)

```bash
# Ubuntu/Debian
sudo apt-get install -y doxygen graphviz

# macOS
brew install doxygen graphviz

# Verify
doxygen --version
```

#### Clang Tools (Optional, for better performance)

```bash
# Ubuntu/Debian
sudo apt-get install -y clang-15 clang-tools-15

# macOS
brew install llvm

# Verify
clang --version
```

### Part 4: Verify Complete Setup

Run the verification script:

```bash
cd /home/claude/bendcore

# Check all dependencies
cmake --version && echo "✓ CMake OK"
ninja --version && echo "✓ Ninja OK" || echo "✓ Make OK"
g++ --version && echo "✓ G++ OK"
node --version && echo "✓ Node.js OK"
npm --version && echo "✓ npm OK"
git --version && echo "✓ git OK"

# Check Eigen
pkg-config --modversion eigen3 && echo "✓ Eigen OK" || \
  echo "⚠ Eigen not found via pkg-config (check BENDCORE_EIGEN_DIR)"

# Check OpenSSL
pkg-config --modversion openssl && echo "✓ OpenSSL OK"
```

---

## Environment Variables

Set these before running Phase 4:

```bash
# Eigen (if not in standard location)
export BENDCORE_EIGEN_DIR=/usr/include/eigen3

# Build type
export CMAKE_BUILD_TYPE=Release

# Number of parallel build jobs
export BUILD_JOBS=4

# Optional: Skip expensive checks
export SKIP_EIGEN=0  # Set to 1 to skip Eigen requirement

# Optional: npm mirror for China
export npm_registry=https://registry.npmmirror.com/
```

Or set in `.env` file and source it:

```bash
# Create .env file
cat > .env << 'EOF'
export BENDCORE_EIGEN_DIR=/usr/include/eigen3
export CMAKE_BUILD_TYPE=Release
export BUILD_JOBS=4
EOF

# Source before building
source .env
```

---

## Troubleshooting

### CMake Error: "Could not find BENDCORE_EIGEN_INCLUDE"

```bash
# Option 1: Install Eigen via package manager
sudo apt-get install libeigen3-dev

# Option 2: Specify Eigen directory
export BENDCORE_EIGEN_DIR=/path/to/eigen
cmake .. -DBENDCORE_EIGEN_DIR=$BENDCORE_EIGEN_DIR

# Option 3: Check where Eigen is installed
pkg-config --cflags --libs eigen3

# Option 4: Manual Eigen download
cd /home/claude/bendcore/third_party
wget https://gitlab.com/libeigen/eigen/-/archive/3.4.0/eigen-3.4.0.zip
unzip eigen-3.4.0.zip
export BENDCORE_EIGEN_DIR=$(pwd)/eigen-3.4.0
```

### npm Error: "network request to registry.npmjs.org failed"

```bash
# Option 1: Configure npm proxy
npm config set proxy http://127.0.0.1:45467
npm config set https-proxy http://127.0.0.1:45467

# Option 2: Use alternative registry
npm config set registry https://registry.npmmirror.com/

# Option 3: Clear cache and retry
npm cache clean --force
npm install

# Option 4: Use offline npm cache
# (if previously installed packages are cached)
npm install --prefer-offline
```

### Build Error: "No such file or directory"

```bash
# Clean build directory and retry
rm -rf build
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j4

# Or use the automated script
./scripts/phase4_execute.sh
```

### CTest: "PASSED: 0, FAILED: 1"

```bash
# Run test with verbose output
ctest -V

# Run specific test
ctest -R test_name -V

# Check test binary exists
ls build/test_*

# Run test directly
./build/test_dwfx_parser_comprehensive
```

### Jest Error: "Cannot find module"

```bash
# Clean install
rm -rf node_modules package-lock.json
npm install

# Verify TypeScript compilation
npm run build

# Check Jest configuration
npm test -- --config jest.config.js
```

---

## Performance Optimization

For faster builds and testing:

```bash
# Use Ninja instead of Make (faster parallel builds)
cmake .. -G Ninja
ninja -j8  # or higher, depending on CPU cores

# Use ccache for faster rebuilds
sudo apt-get install ccache
cmake .. -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache

# Enable Link Time Optimization (LTO)
cmake .. -DCMAKE_CXX_FLAGS="-flto"

# Use multiple test jobs
ctest --parallel 4

# Use multiple jest workers
npm test -- --maxWorkers=4
```

---

## CI/CD Integration

### GitHub Actions Example

```yaml
name: Phase 4 Testing

on: [push, pull_request]

jobs:
  phase4:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      
      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y cmake ninja-build g++ libeigen3-dev libssl-dev
          node --version
          npm --version
      
      - name: Build and test
        run: |
          ./scripts/phase4_execute.sh all
      
      - name: Upload results
        if: always()
        uses: actions/upload-artifact@v3
        with:
          name: phase4-results
          path: phase4_results/
```

### Local Pre-Commit Testing

```bash
# Create .git/hooks/pre-commit
#!/bin/bash
./scripts/phase4_execute.sh ctest || exit 1
npm test || exit 1
```

---

## Support

If you encounter issues:

1. Check that all dependencies are installed: `./scripts/phase4_execute.sh`
2. Review error messages in build logs: `cat build/CMakeOutput.log`
3. Run individual test stages separately
4. Check `/root/.ccr/README.md` for proxy configuration
5. Contact the development team with error logs

---

## Next Steps

Once environment is set up:

```bash
# 1. Run Phase 4 full execution
cd /home/claude/bendcore
./scripts/phase4_execute.sh all

# 2. Monitor progress
tail -f phase4_results/ctest_results_*.txt

# 3. Review results
open phase4_results/coverage_*/index.html
open phase4_results/e2e_report_*/index.html

# 4. Commit results
git add phase4_results/
git commit -m "Phase 4: Test execution results"
```

---

**Estimated Time**: 1-2 hours to set up environment, 49-64 hours for Phase 4 execution

**Target**: November 25, 2026 production launch
