#!/usr/bin/env bash
# Builds Open CASCADE (OCCT) 8.0.1 for native bendcore builds: static libTK*.a with only
# the toolkits listed in third_party/occt_toolkits.txt (plus the dependencies OCCT
# resolves itself from EXTERNLIB), the same list build_occt_wasm.sh and the root
# CMakeLists.txt use.
#
# Usage:  third_party/build_occt.sh
# Environment (all optional):
#   OCCT_VERSION   git tag without the leading V   (default: 8.0.1)
#   OCCT_SOURCE    OCCT sources; cloned if absent   (default: third_party/occt-<version>)
#   OCCT_BUILD     OCCT build directory             (default: third_party/occt-build)
#   OCCT_INSTALL   OCCT install prefix              (default: third_party/occt-install)
#   OCCT_JOBS      parallel compile jobs            (default: nproc)
# Details and the toolkit rationale: docs/kernel/OCCT_BUILD.md.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
OCCT_VERSION="${OCCT_VERSION:-8.0.1}"
OCCT_REPO="https://github.com/Open-Cascade-SAS/OCCT.git"
OCCT_SOURCE="${OCCT_SOURCE:-$ROOT/occt-$OCCT_VERSION}"
OCCT_BUILD="${OCCT_BUILD:-$ROOT/occt-build}"
OCCT_INSTALL="${OCCT_INSTALL:-$ROOT/occt-install}"
JOBS="${OCCT_JOBS:-$(nproc 2>/dev/null || echo 4)}"
TOOLKITS_FILE="$ROOT/occt_toolkits.txt"
LOG_DIR="$OCCT_BUILD/logs"

die() { echo "ERROR: $*" >&2; exit 1; }

echo "=== bendcore OCCT build (native) ==="
echo "Version: $OCCT_VERSION"
echo "Source:  $OCCT_SOURCE"
echo "Build:   $OCCT_BUILD"
echo "Install: $OCCT_INSTALL"

echo "[1/5] Checking prerequisites"
for cmd in cmake ninja git; do
  command -v "$cmd" >/dev/null 2>&1 || die "$cmd not found"
done
[[ -f "$TOOLKITS_FILE" ]] || die "$TOOLKITS_FILE not found"
mapfile -t TOOLKITS < <(grep -E '^TK' "$TOOLKITS_FILE" | cut -d' ' -f1)
[[ ${#TOOLKITS[@]} -gt 0 ]] || die "no toolkits in $TOOLKITS_FILE"
TOOLKITS_CMAKE="$(IFS=';'; echo "${TOOLKITS[*]}")"
echo "Toolkits (${#TOOLKITS[@]}): ${TOOLKITS[*]}"

echo "[2/5] Fetching OCCT $OCCT_VERSION"
if [[ -f "$OCCT_SOURCE/CMakeLists.txt" ]]; then
  echo "  using existing sources at $OCCT_SOURCE"
else
  git clone --depth 1 --branch "V${OCCT_VERSION}" "$OCCT_REPO" "$OCCT_SOURCE" \
    || die "failed to clone $OCCT_REPO tag V${OCCT_VERSION}"
fi

echo "[3/5] Configuring OCCT (log: $LOG_DIR/configure.log)"
mkdir -p "$OCCT_BUILD" "$LOG_DIR"
# All modules off; the toolkit list goes through BUILD_ADDITIONAL_TOOLKITS and OCCT adds
# the EXTERNLIB dependencies itself. Note: OCCT's switch is BUILD_LIBRARY_TYPE, not
# BUILD_SHARED_LIBS. No Tcl/Tk (Draw), FreeType, FreeImage, OpenGL/GLES, VTK, TBB,
# RapidJSON, Draco, FFmpeg, OpenVR, Eigen: none of the listed toolkits needs them.
if ! cmake -S "$OCCT_SOURCE" -B "$OCCT_BUILD" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$OCCT_INSTALL" \
    -DBUILD_LIBRARY_TYPE=Static \
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
    -DUSE_TBB=OFF -DUSE_VTK=OFF -DUSE_EIGEN=OFF -DUSE_OPENGL=OFF -DUSE_GLES2=OFF \
    >"$LOG_DIR/configure.log" 2>&1; then
  tail -n 60 "$LOG_DIR/configure.log" >&2
  die "OCCT configuration failed (full log: $LOG_DIR/configure.log)"
fi

echo "[4/5] Building OCCT with $JOBS jobs (log: $LOG_DIR/build.log)"
if ! cmake --build "$OCCT_BUILD" --parallel "$JOBS" >"$LOG_DIR/build.log" 2>&1; then
  grep -n -E "error:|FAILED" "$LOG_DIR/build.log" | head -n 40 >&2 || true
  tail -n 30 "$LOG_DIR/build.log" >&2
  die "OCCT build failed (full log: $LOG_DIR/build.log)"
fi

echo "[5/5] Installing OCCT (log: $LOG_DIR/install.log)"
if ! cmake --install "$OCCT_BUILD" >"$LOG_DIR/install.log" 2>&1; then
  tail -n 40 "$LOG_DIR/install.log" >&2
  die "OCCT install failed (full log: $LOG_DIR/install.log)"
fi

LIB_DIR="$OCCT_INSTALL/lib"
INC_DIR="$OCCT_INSTALL/include/opencascade"
[[ -f "$INC_DIR/Standard_Version.hxx" ]] || die "headers missing in $INC_DIR after the install"
for tk in "${TOOLKITS[@]}"; do
  [[ -f "$LIB_DIR/lib$tk.a" ]] || die "toolkit $tk missing in $LIB_DIR after the install"
done
echo "Installed toolkits:"
total=0
count=0
for f in "$LIB_DIR"/libTK*.a; do
  name="$(basename "$f" .a)"; name="${name#lib}"
  if printf '%s\n' "${TOOLKITS[@]}" | grep -qx "$name"; then tag="listed"; else tag="dependency added by OCCT"; fi
  size=$(stat -c %s "$f")
  total=$((total + size))
  count=$((count + 1))
  printf '  %-14s %10d bytes  (%s)\n' "$name" "$size" "$tag"
done
echo "Total: $count toolkits, $total bytes"
echo "Done. Configure bendcore with -DBENDCORE_WITH_OCCT=ON -DBENDCORE_OCCT_DIR=$OCCT_INSTALL"
