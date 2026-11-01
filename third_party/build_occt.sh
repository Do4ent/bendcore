#!/usr/bin/env bash
# Сборка OpenCASCADE (OCCT) 8.0 для BendCore
# Использование: cd third_party && ./build_occt.sh
# Результат: ./occt-install/ (header files and static libs)
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
# Git tag of Open-Cascade-SAS/OCCT. The code uses 7.8+ APIs (TopTools_ShapeMapHasher).
OCCT_VERSION="${OCCT_VERSION:-8.0.1}"
OCCT_REPO="https://github.com/Open-Cascade-SAS/OCCT.git"
OCCT_BUILD="${ROOT}/occt-build"
OCCT_INSTALL="${ROOT}/occt-install"
OCCT_SOURCE="${ROOT}/occt-${OCCT_VERSION}"

echo "=== BendCore OCCT Build ==="
echo "Version: $OCCT_VERSION"
echo "Build: $OCCT_BUILD"
echo "Install: $OCCT_INSTALL"
echo ""

# Step 1: Check prerequisites
echo "[1/5] Checking prerequisites..."
for cmd in cmake ninja python3 git; do
  if ! command -v "$cmd" &>/dev/null; then
    echo "ERROR: $cmd not found" >&2
    exit 1
  fi
done
echo "  ✓ All tools found"

# Step 2: Fetch OCCT sources (shallow clone of the release tag)
echo "[2/5] Fetching OCCT $OCCT_VERSION..."
if [[ -d "$OCCT_SOURCE/.git" || -f "$OCCT_SOURCE/CMakeLists.txt" ]]; then
  echo "  Using existing source at $OCCT_SOURCE"
else
  git clone --depth 1 --branch "V${OCCT_VERSION}" "$OCCT_REPO" "$OCCT_SOURCE" || {
    echo "ERROR: Failed to clone $OCCT_REPO tag V${OCCT_VERSION}" >&2
    exit 1
  }
fi

# Step 3: Configure CMake
echo "[3/5] Configuring CMake..."
mkdir -p "$OCCT_BUILD"
cd "$OCCT_BUILD"

# Static TK*.a libs (BendCore links lib/libTK*.a directly). Modules:
# FoundationClasses + ModelingData + ModelingAlgorithms + DataExchange
# (STEPControl). No Visualization / ApplicationFramework / Draw.
# Note: OCCT's switch is BUILD_LIBRARY_TYPE, not BUILD_SHARED_LIBS.
LOG_DIR="${ROOT}/occt-logs"
mkdir -p "$LOG_DIR"
cmake "$OCCT_SOURCE" \
  -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$OCCT_INSTALL" \
  -DBUILD_LIBRARY_TYPE=Static \
  -DBUILD_MODULE_FoundationClasses=ON \
  -DBUILD_MODULE_ModelingData=ON \
  -DBUILD_MODULE_ModelingAlgorithms=ON \
  -DBUILD_MODULE_DataExchange=ON \
  -DBUILD_MODULE_Visualization=OFF \
  -DBUILD_MODULE_ApplicationFramework=OFF \
  -DBUILD_MODULE_Draw=OFF \
  -DBUILD_SAMPLES=OFF \
  -DBUILD_Inspector=OFF \
  -DUSE_TBB=OFF -DUSE_FREETYPE=OFF -DUSE_FREEIMAGE=OFF -DUSE_RAPIDJSON=OFF \
  -DUSE_VTK=OFF -DUSE_OPENGL=OFF -DUSE_GLES2=OFF \
  >"$LOG_DIR/configure.log" 2>&1 || {
    echo "ERROR: CMake configuration failed (full log: $LOG_DIR/configure.log)" >&2
    tail -n 50 "$LOG_DIR/configure.log" >&2
    exit 1
  }

echo "  ✓ CMake configured"

# Step 4: Build
echo "[4/5] Building OCCT (this may take 10-20 minutes)..."
if ! ninja -j"${OCCT_JOBS:-$(nproc 2>/dev/null || echo 4)}" >"$LOG_DIR/build.log" 2>&1; then
  echo "ERROR: Build failed (full log: $LOG_DIR/build.log)" >&2
  grep -n -E "error:|FAILED" "$LOG_DIR/build.log" | head -n 30 >&2 || true
  tail -n 30 "$LOG_DIR/build.log" >&2
  exit 1
fi
echo "  ✓ Build complete"

# Step 5: Install
echo "[5/5] Installing OCCT..."
ninja install >"$LOG_DIR/install.log" 2>&1 || {
  echo "ERROR: Installation failed (full log: $LOG_DIR/install.log)" >&2
  tail -n 30 "$LOG_DIR/install.log" >&2
  exit 1
}

# Verify installation
echo ""
echo "=== Verification ==="
if [[ -d "$OCCT_INSTALL/include/opencascade" ]]; then
  HEADER_COUNT=$(find "$OCCT_INSTALL/include/opencascade" -name "*.hxx" | wc -l)
  echo "  ✓ Headers: $HEADER_COUNT files"
else
  echo "  ✗ Headers not found"
  exit 1
fi

if [[ -d "$OCCT_INSTALL/lib" ]]; then
  LIB_COUNT=$(find "$OCCT_INSTALL/lib" -name "libTK*.a" | wc -l)
  echo "  ✓ Libraries: $LIB_COUNT static libs"
else
  echo "  ✗ Libraries not found"
  exit 1
fi

echo ""
echo "✓ OCCT installation successful!"
echo ""
echo "To use with BendCore:"
echo "  cmake --preset host -DBENDCORE_OCCT_DIR=$OCCT_INSTALL -DBENDCORE_WITH_OCCT=ON"
echo "  cmake --build --preset host"
echo ""
echo "Or for WASM (after emsdk setup):"
echo "  export BENDCORE_OCCT_DIR=$OCCT_INSTALL"
echo "  ./scripts/build_wasm.sh"
