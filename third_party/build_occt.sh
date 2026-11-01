#!/usr/bin/env bash
# Сборка OpenCASCADE (OCCT) 7.7+ для BendCore
# Использование: cd third_party && ./build_occt.sh
# Результат: ./occt-install/ (header files and static libs)
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
OCCT_VERSION="${OCCT_VERSION:-7.7.0}"
# Теги OCCT имеют вид V7_7_0 (не V7.7.0); архив тега распаковывается в OCCT-7_7_0/
OCCT_TAG="V${OCCT_VERSION//./_}"
OCCT_URL="https://github.com/Open-Cascade-SAS/OCCT/archive/refs/tags/${OCCT_TAG}.tar.gz"
OCCT_BUILD="${ROOT}/occt-build"
OCCT_INSTALL="${ROOT}/occt-install"
OCCT_SOURCE="${ROOT}/OCCT-${OCCT_VERSION//./_}"
OCCT_LOG="${ROOT}/occt-build.log"

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

# Step 2: Download OCCT if not present
echo "[2/5] Downloading OCCT $OCCT_VERSION..."
if [[ -d "$OCCT_SOURCE" ]]; then
  echo "  Using existing source at $OCCT_SOURCE"
else
  if [[ ! -f "${ROOT}/occt-${OCCT_VERSION}-source.tar.gz" ]]; then
    echo "  Downloading from $OCCT_URL..."
    cd "$ROOT"
    curl -fsSL -o "occt-${OCCT_VERSION}-source.tar.gz" "$OCCT_URL" || {
      echo "ERROR: Failed to download OCCT" >&2
      exit 1
    }
  fi

  echo "  Extracting..."
  tar -xzf "occt-${OCCT_VERSION}-source.tar.gz"
fi

# Step 3: Configure CMake
echo "[3/5] Configuring CMake..."
mkdir -p "$OCCT_BUILD"
cd "$OCCT_BUILD"

cmake "$OCCT_SOURCE" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$OCCT_INSTALL" \
  -DCMAKE_CXX_STANDARD=20 \
  -DCMAKE_CXX_STANDARD_REQUIRED=ON \
  -DCMAKE_CXX_EXTENSIONS=OFF \
  -GNinja \
  -DBUILD_SHARED_LIBS=OFF \
  -DBUILD_TESTING=OFF \
  -DBUILD_MODULE_FoundationClasses=ON \
  -DBUILD_MODULE_ModelingData=ON \
  -DBUILD_MODULE_ModelingAlgorithms=ON \
  -DBUILD_MODULE_DataExchange=ON \
  -DBUILD_MODULE_Visualization=OFF \
  -DBUILD_MODULE_ApplicationFramework=ON \
  -DBUILD_MODULE_Draw=OFF \
  -DUSE_FREETYPE=OFF \
  -DUSE_RAPIDJSON=OFF \
  -DUSE_OPENCL=OFF \
  -DUSE_OPENVINO=OFF \
  -DUSE_VTK=OFF \
  >"$OCCT_LOG" 2>&1 || {
    echo "ERROR: CMake configuration failed (full log: $OCCT_LOG)" >&2
    tail -n 50 "$OCCT_LOG" >&2
    exit 1
  }

echo "  ✓ CMake configured"

# Step 4: Build
echo "[4/5] Building OCCT (this may take 10-20 minutes)..."
if ! ninja >>"$OCCT_LOG" 2>&1; then
  echo "ERROR: Build failed (full log: $OCCT_LOG)" >&2
  echo "Last 50 lines of build output:" >&2
  tail -n 50 "$OCCT_LOG" >&2
  exit 1
fi
echo "  ✓ Build complete"

# Step 5: Install
echo "[5/5] Installing OCCT..."
ninja install >>"$OCCT_LOG" 2>&1 || {
  echo "ERROR: Installation failed" >&2
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
echo "Alternative without building (Ubuntu 24.04 ships OCCT 7.6):"
echo "  sudo apt install libocct-data-exchange-dev libocct-modeling-algorithms-dev libtbb-dev libeigen3-dev"
echo "  cmake --preset host -DBENDCORE_WITH_OCCT=ON"
echo ""
echo "To use this build with BendCore:"
echo "  cmake --preset host -DBENDCORE_OCCT_DIR=$OCCT_INSTALL -DBENDCORE_WITH_OCCT=ON"
echo "  cmake --build --preset host"
echo ""
echo "Or for WASM (after emsdk setup):"
echo "  export BENDCORE_OCCT_DIR=$OCCT_INSTALL"
echo "  ./scripts/build_wasm.sh"
