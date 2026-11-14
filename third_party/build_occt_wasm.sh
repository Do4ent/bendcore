#!/usr/bin/env bash
# Builds Open CASCADE 8.0.1 for WebAssembly (Emscripten) with only the toolkits listed
# in third_party/occt_toolkits.txt (plus the dependencies OCCT resolves itself), then
# builds and runs the size/function probe tests/kernel/wasm/occt_size_probe.cpp under
# node and reports the size of the resulting .wasm (raw and gzip -9).
#
# Usage:  third_party/build_occt_wasm.sh
# Environment (all optional):
#   OCCT_SOURCE        OCCT 8.0.1 sources        (default: third_party/occt-8.0.1)
#   OCCT_WASM_BUILD    OCCT build directory      (default: third_party/occt-build-wasm)
#   OCCT_WASM_INSTALL  OCCT install prefix       (default: third_party/occt-install-wasm)
#   EMSDK              emsdk root                (default: /root/emsdk); emsdk 6.0.12 is pinned in CI
#   OCCT_JOBS          parallel compile jobs     (default: nproc)
#   OCCT_CXX_STANDARD  C++ standard of the OCCT build: C++20 (default) or C++17 (OCCT's default).
#                  With C++20 every `stream >> char_array` in OCCT's readers is bounded by the
#                  array size (P0487); with C++17 a long word in a corrupted BRep file overflows
#                  the 255-byte keyword buffers of BinTools/TopTools/BRepTools (stack smashing).
#   OCCT_RELEASE_CHECKS 1 = keep OCCT's argument/index checks (*_Raise_if macros) in the
#                      Release build: BUILD_RELEASE_DISABLE_EXCEPTIONS=OFF, no -DNo_Exception;
#                      0 = OCCT's own default (checks compiled out). Default: 1.
#   SKIP_OCCT_BUILD=1  reuse an existing install and only build/run the probe
# Details, measured sizes and known issues: docs/kernel/OCCT_BUILD.md.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
REPO="$(cd "$ROOT/.." && pwd)"
OCCT_SOURCE="${OCCT_SOURCE:-$ROOT/occt-8.0.1}"
OCCT_WASM_BUILD="${OCCT_WASM_BUILD:-$ROOT/occt-build-wasm}"
OCCT_WASM_INSTALL="${OCCT_WASM_INSTALL:-$ROOT/occt-install-wasm}"
EMSDK="${EMSDK:-/root/emsdk}"
JOBS="${OCCT_JOBS:-$(nproc 2>/dev/null || echo 4)}"
RELEASE_CHECKS="${OCCT_RELEASE_CHECKS:-1}"
CXX_STANDARD="${OCCT_CXX_STANDARD:-C++20}"
TOOLKITS_FILE="$ROOT/occt_toolkits.txt"
LOG_DIR="$OCCT_WASM_BUILD/logs"

die() { echo "ERROR: $*" >&2; exit 1; }

case "$RELEASE_CHECKS" in
  1) DISABLE_EXCEPTIONS=OFF ;;
  0) DISABLE_EXCEPTIONS=ON ;;
  *) die "OCCT_RELEASE_CHECKS must be 0 or 1, got '$RELEASE_CHECKS'" ;;
esac
case "$CXX_STANDARD" in
  C++17|C++20|C++23) ;;
  *) die "OCCT_CXX_STANDARD must be C++17, C++20 or C++23, got '$CXX_STANDARD'" ;;
esac

# --- Toolchain -------------------------------------------------------------------
if ! command -v emcmake >/dev/null 2>&1; then
  [[ -f "$EMSDK/emsdk_env.sh" ]] || die "emsdk not found at $EMSDK (set EMSDK)"
  # shellcheck disable=SC1091
  source "$EMSDK/emsdk_env.sh" >/dev/null 2>&1
fi
for cmd in emcmake em++ cmake ninja node gzip; do
  command -v "$cmd" >/dev/null 2>&1 || die "$cmd not found"
done
echo "emcc: $(em++ --version | head -n 1)"
echo "node: $(node --version)"

[[ -f "$OCCT_SOURCE/CMakeLists.txt" ]] || die "OCCT sources not found at $OCCT_SOURCE (set OCCT_SOURCE)"
[[ -f "$TOOLKITS_FILE" ]] || die "$TOOLKITS_FILE not found"

mapfile -t TOOLKITS < <(grep -E '^TK' "$TOOLKITS_FILE" | cut -d' ' -f1)
[[ ${#TOOLKITS[@]} -gt 0 ]] || die "no toolkits in $TOOLKITS_FILE"
TOOLKITS_CMAKE="$(IFS=';'; echo "${TOOLKITS[*]}")"
echo "Toolkits (${#TOOLKITS[@]}): ${TOOLKITS[*]}"
echo "Release checks: $RELEASE_CHECKS (BUILD_RELEASE_DISABLE_EXCEPTIONS=$DISABLE_EXCEPTIONS), $CXX_STANDARD"

# Exceptions: OCCT reports errors by throwing Standard_Failure; the bendcore WASM
# build uses Emscripten's JS-based exceptions (-fexceptions, root CMakeLists.txt),
# and both sides of a link must use the same exception model.
EH_FLAGS="-fexceptions"

# --- OCCT ------------------------------------------------------------------------
if [[ "${SKIP_OCCT_BUILD:-0}" != "1" ]]; then
  mkdir -p "$OCCT_WASM_BUILD" "$LOG_DIR"
  echo "[1/3] Configuring OCCT for Emscripten (log: $LOG_DIR/configure.log)"
  # All modules off; the toolkit list goes through BUILD_ADDITIONAL_TOOLKITS and OCCT
  # adds their EXTERNLIB dependencies itself. No Tcl/Tk (Draw), FreeType, FreeImage,
  # OpenGL/GLES, VTK, TBB, RapidJSON, Draco: none of the listed toolkits needs them.
  if ! emcmake cmake -S "$OCCT_SOURCE" -B "$OCCT_WASM_BUILD" -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX="$OCCT_WASM_INSTALL" \
      -DCMAKE_C_FLAGS="$EH_FLAGS" -DCMAKE_CXX_FLAGS="$EH_FLAGS" \
      -DBUILD_LIBRARY_TYPE=Static \
      -DBUILD_RELEASE_DISABLE_EXCEPTIONS="$DISABLE_EXCEPTIONS" \
      -DBUILD_CPP_STANDARD="$CXX_STANDARD" \
      -DBUILD_MODULE_FoundationClasses=OFF \
      -DBUILD_MODULE_ModelingData=OFF \
      -DBUILD_MODULE_ModelingAlgorithms=OFF \
      -DBUILD_MODULE_Visualization=OFF \
      -DBUILD_MODULE_ApplicationFramework=OFF \
      -DBUILD_MODULE_DataExchange=OFF \
      -DBUILD_MODULE_Draw=OFF \
      -DBUILD_ADDITIONAL_TOOLKITS="$TOOLKITS_CMAKE" \
      -DBUILD_SAMPLES_QT=OFF -DBUILD_Inspector=OFF -DBUILD_GTEST=OFF \
      -DBUILD_DOC_Overview=OFF -DBUILD_DOC_RefMan=OFF \
      -DUSE_TK=OFF -DUSE_TCL=OFF -DUSE_FREETYPE=OFF -DUSE_FREEIMAGE=OFF \
      -DUSE_FFMPEG=OFF -DUSE_OPENVR=OFF -DUSE_RAPIDJSON=OFF -DUSE_DRACO=OFF \
      -DUSE_TBB=OFF -DUSE_VTK=OFF -DUSE_EIGEN=OFF \
      >"$LOG_DIR/configure.log" 2>&1; then
    tail -n 60 "$LOG_DIR/configure.log" >&2
    die "OCCT configuration failed (full log: $LOG_DIR/configure.log)"
  fi

  # The mode must have reached the compiler (BUILD_RELEASE_DISABLE_EXCEPTIONS is a cached option).
  if grep -q -- '-DNo_Exception' "$OCCT_WASM_BUILD/build.ninja"; then have_no_exc=1; else have_no_exc=0; fi
  if [[ "$RELEASE_CHECKS" == "1" && "$have_no_exc" == "1" ]]; then
    die "OCCT_RELEASE_CHECKS=1 but -DNo_Exception is in $OCCT_WASM_BUILD/build.ninja (stale cache?)"
  fi
  if [[ "$RELEASE_CHECKS" == "0" && "$have_no_exc" == "0" ]]; then
    die "OCCT_RELEASE_CHECKS=0 but -DNo_Exception is not in $OCCT_WASM_BUILD/build.ninja"
  fi

  echo "[2/3] Building OCCT with $JOBS jobs (log: $LOG_DIR/build.log)"
  if ! cmake --build "$OCCT_WASM_BUILD" --parallel "$JOBS" >"$LOG_DIR/build.log" 2>&1; then
    grep -n -E "error:|FAILED" "$LOG_DIR/build.log" | head -n 40 >&2 || true
    die "OCCT build failed (full log: $LOG_DIR/build.log)"
  fi
  if ! cmake --install "$OCCT_WASM_BUILD" >"$LOG_DIR/install.log" 2>&1; then
    tail -n 40 "$LOG_DIR/install.log" >&2
    die "OCCT install failed (full log: $LOG_DIR/install.log)"
  fi
  printf 'OCCT 8.0.1 wasm release_checks=%s cxx=%s\n' "$RELEASE_CHECKS" "$CXX_STANDARD" >"$OCCT_WASM_INSTALL/bendcore_occt_build.txt"
fi

LIB_DIR="$OCCT_WASM_INSTALL/lib"
INC_DIR="$OCCT_WASM_INSTALL/include/opencascade"
LIBS=()
for tk in "${TOOLKITS[@]}"; do
  [[ -f "$LIB_DIR/lib$tk.a" ]] || die "toolkit $tk missing in $LIB_DIR after the build"
  LIBS+=("$LIB_DIR/lib$tk.a")
done
echo "Installed toolkits:"
for f in "$LIB_DIR"/libTK*.a; do
  name="$(basename "$f" .a)"; name="${name#lib}"
  if printf '%s\n' "${TOOLKITS[@]}" | grep -qx "$name"; then tag="listed"; else tag="dependency added by OCCT"; fi
  printf '  %-14s %10d bytes  (%s)\n' "$name" "$(stat -c %s "$f")" "$tag"
done

# --- Probe -----------------------------------------------------------------------
PROBE_SRC="$REPO/tests/kernel/wasm/occt_size_probe.cpp"
PROBE_DIR="$OCCT_WASM_BUILD/probe"
mkdir -p "$PROBE_DIR"
echo "[3/3] Building and running $PROBE_SRC"
printf '%-6s %12s %12s\n' "opt" "wasm bytes" "gzip -9"
for opt in O2 Oz; do
  out="$PROBE_DIR/occt_size_probe_$opt.js"
  # The probe links only the listed toolkits: a symbol that needs an unlisted one
  # fails here, exactly as it would in the bendcore WASM kernel.
  if ! em++ -std=c++20 "-$opt" $EH_FLAGS -I"$INC_DIR" "$PROBE_SRC" -o "$out" \
      -Wl,--start-group "${LIBS[@]}" -Wl,--end-group \
      -sENVIRONMENT=node -sALLOW_MEMORY_GROWTH=1 -sEXIT_RUNTIME=1 \
      >"$PROBE_DIR/link_$opt.log" 2>&1; then
    cat "$PROBE_DIR/link_$opt.log" >&2
    die "probe link failed (-$opt)"
  fi
  wasm="${out%.js}.wasm"
  raw=$(stat -c %s "$wasm")
  gz=$(gzip -9 -c "$wasm" | wc -c)
  printf '%-6s %12d %12d\n' "-$opt" "$raw" "$gz"
done | tee "$PROBE_DIR/sizes.txt"

echo "--- node $PROBE_DIR/occt_size_probe_O2.js"
node "$PROBE_DIR/occt_size_probe_O2.js" | tee "$PROBE_DIR/run_O2.txt"
echo "--- node $PROBE_DIR/occt_size_probe_Oz.js"
node "$PROBE_DIR/occt_size_probe_Oz.js" | tee "$PROBE_DIR/run_Oz.txt"
grep -qx OK "$PROBE_DIR/run_O2.txt" || die "probe (-O2) did not report OK"
grep -qx OK "$PROBE_DIR/run_Oz.txt" || die "probe (-Oz) did not report OK"
echo "Done. OCCT for WASM: $OCCT_WASM_INSTALL; probe and sizes: $PROBE_DIR"
