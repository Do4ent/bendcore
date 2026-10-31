#!/usr/bin/env bash
# Воспроизводимая сборка WebAssembly (Emscripten) и тестов в node.
# Использование: ./scripts/build_wasm.sh
# Или с явной переменной: EMSDK=/path/to/emsdk BENDCORE_EIGEN_DIR=/path/with/Eigen ./scripts/build_wasm.sh
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
LOG_FILE="${ROOT}/build-wasm.log"

# Try to source emsdk_env.sh if EMSDK is not set
if [[ -z "${EMSDK:-}" ]]; then
  if [[ -f "${HOME}/emsdk/emsdk_env.sh" ]]; then
    export EMSDK="${HOME}/emsdk"
    echo "Using default emsdk at $EMSDK"
  fi
fi

# Verify EMSDK is available
if [[ -z "${EMSDK:-}" || ! -f "${EMSDK}/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake" ]]; then
  echo "ERROR: EMSDK not set or invalid" >&2
  echo "" >&2
  echo "Setup instructions:" >&2
  echo "  1. ./scripts/setup_wasm.sh        # Auto-setup emsdk" >&2
  echo "  2. source ~/.bashrc               # Reload environment" >&2
  echo "  3. ./scripts/build_wasm.sh        # Build WASM" >&2
  echo "" >&2
  echo "Or manually:" >&2
  echo "  export EMSDK=/path/to/emsdk" >&2
  echo "  source \$EMSDK/emsdk_env.sh" >&2
  echo "  ./scripts/build_wasm.sh" >&2
  exit 2
fi

# Source emsdk environment
# shellcheck disable=SC1091
source "${EMSDK}/emsdk_env.sh" >/dev/null 2>&1 || { echo "ERROR: Failed to source emsdk_env.sh" >&2; exit 2; }

# Verify required tools
for cmd in emcc cmake ninja node; do
  if ! command -v "$cmd" &>/dev/null; then
    echo "ERROR: $cmd not found in PATH" >&2
    exit 2
  fi
done

cd "$ROOT"

# Print environment info
echo "=== Environment ==="
echo "emcc version: $(emcc --version | head -1)"
echo "cmake version: $(cmake --version | head -1)"
echo "node version: $(node --version)"
echo "ninja version: $(ninja --version)"
echo "EMSDK: $EMSDK"
echo ""

# Set default Eigen path if not provided
if [[ -z "${BENDCORE_EIGEN_DIR:-}" ]]; then
  if [[ -d /usr/include/eigen3 ]]; then
    BENDCORE_EIGEN_DIR=/usr/include/eigen3
    echo "Using default Eigen: $BENDCORE_EIGEN_DIR"
  else
    echo "WARNING: Eigen not found. Continuing anyway..." >&2
  fi
fi

CMAKE_EXTRA=()
if [[ -n "${BENDCORE_EIGEN_DIR:-}" ]]; then
  CMAKE_EXTRA+=("-DBENDCORE_EIGEN_DIR=${BENDCORE_EIGEN_DIR}")
fi

# Build
echo "=== Building WASM ==="
if ! cmake --preset wasm "${CMAKE_EXTRA[@]}"; then
  echo "ERROR: CMake configure failed" >&2
  [[ -f "$LOG_FILE" ]] && tail -n 50 "$LOG_FILE" >&2
  exit 1
fi

if ! cmake --build --preset wasm; then
  echo "ERROR: CMake build failed" >&2
  [[ -f build-wasm/CMakeFiles/CMakeError.log ]] && tail -n 50 build-wasm/CMakeFiles/CMakeError.log >&2
  exit 1
fi

# Run tests
echo "=== Running tests ==="
if ! ctest --preset wasm; then
  echo "ERROR: Tests failed" >&2
  exit 1
fi

# Verify output files
echo "=== Verifying output ==="
OUT="$ROOT/build-wasm/web"
MISSING=0

for f in bendcore.js bendcore.wasm bendcore.mjs index.html; do
  if [[ -s "$OUT/$f" ]]; then
    SIZE=$(stat -c%s "$OUT/$f" 2>/dev/null || stat -f%z "$OUT/$f" 2>/dev/null || echo "?")
    echo "  ✓ $f ($SIZE bytes)"
  else
    echo "  ✗ $f MISSING"
    ((MISSING++))
  fi
done

if [[ $MISSING -gt 0 ]]; then
  echo "ERROR: $MISSING output files missing" >&2
  exit 1
fi

echo ""
echo "✓ Build successful!"
echo "Output: $OUT/"
echo "Ready to deploy or test in browser"
