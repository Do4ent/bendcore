#!/usr/bin/env bash
# Stage 2 Readiness Check
# Verifies all prerequisites for OCCT build and integration
# Usage: ./scripts/check-stage2-readiness.sh

set -euo pipefail

echo "╔════════════════════════════════════════════════════════════╗"
echo "║         BendCore Stage 2 Readiness Check                  ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo ""

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PASS=0
FAIL=0
WARN=0

# Colors
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
RED='\033[0;31m'
BLUE='\033[0;34m'
NC='\033[0m'

check_command() {
  local cmd=$1
  local version_flag=${2:---version}

  if command -v "$cmd" &>/dev/null; then
    if [ "$version_flag" != "none" ]; then
      local version=$("$cmd" "$version_flag" 2>&1 | head -1)
      echo -e "${GREEN}✓${NC} $cmd: $version"
    else
      echo -e "${GREEN}✓${NC} $cmd: installed"
    fi
    ((PASS++))
    return 0
  else
    echo -e "${RED}✗${NC} $cmd: NOT FOUND"
    ((FAIL++))
    return 1
  fi
}

check_file() {
  local path=$1
  local desc=$2

  if [ -f "$path" ]; then
    echo -e "${GREEN}✓${NC} $desc: found"
    ((PASS++))
    return 0
  else
    echo -e "${RED}✗${NC} $desc: NOT FOUND at $path"
    ((FAIL++))
    return 1
  fi
}

check_disk_space() {
  local required_gb=20
  local available_gb=$(df "$ROOT" | tail -1 | awk '{print $4}' | xargs -I {} sh -c "echo \$((\$1/1024/1024))" _ {})

  echo -n "Disk Space: "
  if [ "$available_gb" -ge "$required_gb" ]; then
    echo -e "${GREEN}✓${NC} ${available_gb}GB available (need $required_gb)"
    ((PASS++))
  else
    echo -e "${YELLOW}⚠${NC} ${available_gb}GB available (need $required_gb)"
    ((WARN++))
  fi
}

echo "📋 System Prerequisites"
echo "─────────────────────────────────────────────────────────────"
check_command "cmake" "-version"
check_command "ninja" "-version"
check_command "python3" "-version"
check_command "git" "--version"
check_command "node" "-v"
check_command "npm" "-v"
check_disk_space

echo ""
echo "📦 BendCore Project Structure"
echo "─────────────────────────────────────────────────────────────"
check_file "$ROOT/CMakeLists.txt" "CMakeLists.txt"
check_file "$ROOT/CMakePresets.json" "CMakePresets.json"
check_file "$ROOT/scripts/build_wasm.sh" "WASM build script"
check_file "$ROOT/scripts/setup_wasm.sh" "WASM setup script"
check_file "$ROOT/third_party/build_occt.sh" "OCCT build script"

echo ""
echo "🎯 Stage 1 Verification"
echo "─────────────────────────────────────────────────────────────"

# Check npm test
echo -n "Frontend tests: "
if cd "$ROOT" && npm test -- --testPathPattern=nonexistent --passWithNoTests 2>&1 | grep -q "Test Suites"; then
  echo -e "${GREEN}✓${NC} npm test works"
  ((PASS++))
else
  echo -e "${YELLOW}⚠${NC} npm test might have issues"
  ((WARN++))
fi

# Check npm build
echo -n "Frontend build: "
if cd "$ROOT" && npm run build >/dev/null 2>&1; then
  echo -e "${GREEN}✓${NC} npm run build works"
  ((PASS++))
else
  echo -e "${YELLOW}⚠${NC} npm run build has issues"
  ((WARN++))
fi

echo ""
echo "🔧 Optional: Emscripten SDK"
echo "─────────────────────────────────────────────────────────────"
if [ -f "$HOME/emsdk/emsdk_env.sh" ]; then
  echo -e "${GREEN}✓${NC} emsdk already installed at ~/emsdk"
  ((PASS++))
else
  echo -e "${YELLOW}⚠${NC} emsdk not installed (run: ./scripts/setup_wasm.sh)"
  ((WARN++))
fi

if command -v emcc &>/dev/null; then
  local emcc_ver=$(emcc --version | head -1)
  echo -e "${GREEN}✓${NC} emcc: $emcc_ver"
  ((PASS++))
else
  echo -e "${YELLOW}⚠${NC} emcc: not in PATH (run: source ~/emsdk/emsdk_env.sh)"
  ((WARN++))
fi

echo ""
echo "📚 Documentation"
echo "─────────────────────────────────────────────────────────────"
check_file "$ROOT/IMPLEMENTATION_STATUS.md" "Implementation status"
check_file "$ROOT/WORK_COMPLETED.md" "Work completed summary"
check_file "$ROOT/docs/STAGE_2_3_ROADMAP.md" "Stage 2-3 roadmap"
check_file "$ROOT/docs/WASM_BUILD_GUIDE.md" "WASM build guide"

echo ""
echo "╔════════════════════════════════════════════════════════════╗"
echo "║                     READINESS SUMMARY                      ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo ""

printf "Results: ${GREEN}%d passed${NC} | ${YELLOW}%d warnings${NC} | ${RED}%d failed${NC}\n" "$PASS" "$WARN" "$FAIL"
echo ""

if [ $FAIL -eq 0 ]; then
  if [ $WARN -eq 0 ]; then
    echo -e "${GREEN}✓ READY FOR STAGE 2${NC}"
    echo ""
    echo "Next steps:"
    echo "  1. Review IMPLEMENTATION_STATUS.md"
    echo "  2. Review docs/STAGE_2_3_ROADMAP.md"
    echo "  3. Run: cd third_party && ./build_occt.sh"
    exit 0
  else
    echo -e "${YELLOW}⚠ MOSTLY READY (with warnings)${NC}"
    echo ""
    echo "Warnings can be addressed:"
    echo "  - Emscripten: Run ./scripts/setup_wasm.sh (optional for now)"
    echo "  - Disk space: OCCT build needs 10-15GB disk"
    exit 0
  fi
else
  echo -e "${RED}✗ NOT READY - Missing critical prerequisites${NC}"
  echo ""
  echo "Install missing tools:"
  echo "  • Linux (Ubuntu): sudo apt-get install cmake ninja-build python3"
  echo "  • macOS: brew install cmake ninja python3"
  echo "  • Windows: choco install cmake ninja python"
  exit 1
fi
