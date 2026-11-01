# CMake Windows Configuration Guide

How BendCore CMake is configured for Windows builds with MSVC, Ninja, and dependencies.

---

## CMakeLists.txt Windows Support

### MSVC Compiler Settings

```cmake
if(MSVC)
  # Precision floating-point: no fast-math, no expression contraction
  add_compile_options(/fp:precise /fp:contract-)
  
  # Specify compiler (optional, defaults to cl.exe)
  set(CMAKE_CXX_COMPILER cl.exe)
  set(CMAKE_C_COMPILER cl.exe)
endif()
```

**Why:** Ensures deterministic, precise calculations for CAD operations.

### Windows-Specific Optimizations

```cmake
# Reduce binary size (Release builds)
if(MSVC AND CMAKE_BUILD_TYPE STREQUAL "Release")
  string(REGEX REPLACE "/O2" "/O1" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  # Or use /Ox for aggressive optimization
endif()

# Enable SIMD for performance (optional)
if(MSVC)
  # AVX2 support (requires Intel Haswell or newer)
  add_compile_options(/arch:AVX2)
  # Or SSE2 for older CPUs
  # add_compile_options(/arch:SSE2)
endif()
```

---

## Generator Selection

### Visual Studio (Recommended for IDE development)
```powershell
cmake -B build-native -G "Visual Studio 17 2022" `
  -DCMAKE_BUILD_TYPE=Release
```

**Pros:**
- Visual Studio project files (.sln, .vcxproj)
- Integrated debugging
- Incremental builds faster
- Better IDE integration

**Cons:**
- Larger build artifacts
- Slower initial configuration
- More disk space

### Ninja (Recommended for CI/CD)
```powershell
cmake -B build-native -G "Ninja" `
  -DCMAKE_BUILD_TYPE=Release
```

**Pros:**
- Faster build times
- Smaller artifacts
- Better for CI/CD
- Cross-platform consistency

**Cons:**
- Less IDE integration
- No .sln files
- Requires Ninja installation

---

## Dependency Configuration

### Eigen (Linear Algebra)

**Automatic detection:**
```powershell
cmake -B build-native -G "Visual Studio 17 2022" `
  -DCMAKE_BUILD_TYPE=Release
```

CMake searches:
1. System `find_package(Eigen3)`
2. vcpkg: `C:\vcpkg\installed\x64-windows\include`
3. Manual: `third_party/eigen-5.0.0/`
4. Stub: `third_party_stubs/`

**Manual specification (if auto-detection fails):**
```powershell
cmake -B build-native -G "Visual Studio 17 2022" `
  -DCMAKE_BUILD_TYPE=Release `
  -DBENDCORE_EIGEN_DIR="C:/vcpkg/installed/x64-windows/include"
```

**vcpkg installation:**
```powershell
git clone https://github.com/Microsoft/vcpkg.git C:\vcpkg
cd C:\vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg install eigen3:x64-windows
```

### ZLIB (Compression)

**Windows:**
```cmake
find_package(ZLIB REQUIRED)
target_link_libraries(bendcore PRIVATE ZLIB::ZLIB)
```

**Install via vcpkg:**
```powershell
vcpkg install zlib:x64-windows
```

### OpenSSL (Optional)

**Auto-detected, optional dependency:**
```cmake
find_package(OpenSSL QUIET)
if(OPENSSL_FOUND)
  target_link_libraries(bendcore PRIVATE OpenSSL::Crypto)
endif()
```

**Install via vcpkg:**
```powershell
vcpkg install openssl:x64-windows
```

### OCCT (OpenCASCADE - CAD Kernel)

**Currently disabled for Windows CI:** `-DBENDCORE_WITH_OCCT=OFF`

**To enable locally:**
1. Build OCCT (10-20 minutes):
```powershell
cd third_party
.\build_occt.ps1
cd ..
```

2. Configure CMake with OCCT:
```powershell
cmake -B build-native -G "Visual Studio 17 2022" `
  -DCMAKE_BUILD_TYPE=Release `
  -DBENDCORE_WITH_OCCT=ON `
  -DBENDCORE_OCCT_DIR="$pwd\third_party\occt-install"
```

---

## Build Configuration Options

### Common CMake Flags

```powershell
# Build type (Release / Debug)
-DCMAKE_BUILD_TYPE=Release

# Enable/disable OCCT
-DBENDCORE_WITH_OCCT=OFF

# Eigen location
-DBENDCORE_EIGEN_DIR="path/to/eigen"

# Build unverified modules (testing)
-DBENDCORE_BUILD_UNVERIFIED=ON

# Parallel compilation
cmake --build build-native --parallel 4
```

### Debug vs. Release

**Debug (slower, better debugging):**
```powershell
cmake -B build-native -G "Visual Studio 17 2022" `
  -DCMAKE_BUILD_TYPE=Debug
```

- Includes debug symbols
- Disables optimizations
- Slower execution
- Larger binaries
- Better for `gdb`, Visual Studio debugger

**Release (faster, optimized):**
```powershell
cmake -B build-native -G "Visual Studio 17 2022" `
  -DCMAKE_BUILD_TYPE=Release
```

- Optimized code
- Smaller binaries
- Faster execution
- No debug symbols
- Better for production

---

## Complete Windows Build Example

```powershell
# Install dependencies
vcpkg install eigen3:x64-windows zlib:x64-windows

# Configure
cmake -B build-native `
  -G "Visual Studio 17 2022" `
  -DCMAKE_BUILD_TYPE=Release `
  -DBENDCORE_EIGEN_DIR="C:/vcpkg/installed/x64-windows/include" `
  -DBENDCORE_WITH_OCCT=OFF

# Build
cmake --build build-native --config Release --parallel 4

# Test
ctest --test-dir build-native --config Release --output-on-failure

# Output
# build-native/Release/bendcore.lib     (Static library)
# build-native/Release/bendcore_core    (Object files)
```

---

## Troubleshooting

### Issue: "Could not find Eigen3"

**Solution 1: Install via vcpkg**
```powershell
vcpkg install eigen3:x64-windows
# Then cmake will auto-find it
```

**Solution 2: Specify manually**
```powershell
cmake -B build-native ... `
  -DBENDCORE_EIGEN_DIR="C:/vcpkg/installed/x64-windows/include"
```

**Solution 3: Use third_party_stubs (fallback)**
- Eigen stubs are in `third_party_stubs/`
- Uses simplified matrix operations
- Not for production

### Issue: "ZLIB not found" (Windows)

**Solution:**
```powershell
vcpkg install zlib:x64-windows
# Or disable compression:
# CMake will fallback to no-op ZLIB
```

### Issue: "MSVC compiler not found"

**Solution:**
- Install Visual Studio with C++ support
- Or install Visual C++ Build Tools
- Run Visual Studio installer and add "Desktop development with C++"

**Verify:**
```powershell
where cl.exe        # Should show MSVC path
$env:PATH -split ';' | Select-String "MSVC"
```

### Issue: "ninja: command not found"

**Solution:**
```powershell
# Install
choco install ninja

# Or download
# https://github.com/ninja-build/ninja/releases
```

### Issue: "CMake can't find msbuild"

**Solution:**
CMake auto-detects Visual Studio. If not found:
```powershell
# Option 1: Use newer Visual Studio generator
cmake -B build-native -G "Visual Studio 17 2022" ...

# Option 2: Use Ninja instead (doesn't need msbuild)
cmake -B build-native -G "Ninja" ...
```

---

## CI/CD Integration

### GitHub Actions Configuration

**Windows runner setup:**
```yaml
runs-on: windows-2022

steps:
  - name: Install CMake and Ninja
    run: choco install cmake ninja -y

  - name: Configure CMake
    run: |
      cmake -B build-native `
        -G "Visual Studio 17 2022" `
        -DCMAKE_BUILD_TYPE=Release `
        -DBENDCORE_WITH_OCCT=OFF

  - name: Build
    run: |
      cmake --build build-native --config Release --parallel 4
```

**See:** `.github/workflows/windows-build.yml`

---

## Performance Tips

1. **Use Ninja for faster builds** (CI/CD)
   ```powershell
   -G "Ninja"  # ~30% faster than Visual Studio
   ```

2. **Enable parallel compilation**
   ```powershell
   cmake --build build-native --parallel 4
   ```

3. **Use ccache for incremental builds** (optional)
   ```powershell
   # Install: choco install ccache
   cmake -B build-native -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
   ```

4. **Link-time optimization** (slower initial, faster runtime)
   ```cmake
   set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
   ```

5. **Precompiled headers** (faster recompilation)
   ```cmake
   target_precompile_headers(bendcore PRIVATE pch.h)
   ```

---

## Resources

- [CMake Documentation](https://cmake.org/cmake/help/latest/)
- [Visual Studio Generators](https://cmake.org/cmake/help/latest/generator/Visual%20Studio%2017%202022.html)
- [Ninja Build System](https://ninja-build.org/)
- [vcpkg Package Manager](https://vcpkg.io/)
- [Eigen Documentation](https://eigen.tuxfamily.org/)

---

**Last Updated:** 2026-11-01
**For:** CMake 3.28+, Visual Studio 2022, Ninja
