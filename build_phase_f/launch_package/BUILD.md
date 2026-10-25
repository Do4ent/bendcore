# Building from Source

## Prerequisites

- GCC 13.3+ or Clang 14+ or MSVC 2019+
- CMake 3.20 or later
- OpenCASCADE 7.6 or later
- Standard C++20 library

## Build Steps

1. Clone the repository:
   ```bash
   git clone https://github.com/Do4ent/bendcore.git
   cd bendcore
   ```

2. Create build directory:
   ```bash
   mkdir build && cd build
   ```

3. Configure:
   ```bash
   cmake -DCMAKE_BUILD_TYPE=Release \
         -DCMAKE_CXX_FLAGS="-Wall -Wextra -Werror" \
         ..
   ```

4. Build:
   ```bash
   make -j$(nproc)  # or make -j$(sysctl -n hw.ncpu) on macOS
   ```

5. Run tests:
   ```bash
   ctest --output-on-failure
   ```

6. Install:
   ```bash
   sudo make install
   ```

## Build Options

- `-DCMAKE_BUILD_TYPE=Release` - Release build with optimizations
- `-DCMAKE_BUILD_TYPE=Debug` - Debug build with symbols
- `-DBUILD_TESTING=ON` - Build with tests (default)
- `-DBUILD_SHARED_LIBS=ON` - Build shared libraries

