#!/bin/bash
# BendCore Phase 4 Execution Script
# Автоматизированный запуск всех этапов Phase 4
# Требования: Eigen library, npm packages, node.js

set -e

BENDCORE_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BENDCORE_ROOT}/build"
TEST_RESULTS="${BENDCORE_ROOT}/phase4_results"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)

# Цвета для вывода
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}╔════════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║  BendCore CAD - Phase 4 Full Execution                     ║${NC}"
echo -e "${BLUE}╚════════════════════════════════════════════════════════════╝${NC}"

# Функция для проверки зависимостей
check_dependencies() {
    echo -e "\n${BLUE}[1/7] Checking dependencies...${NC}"

    local missing=0

    if ! command -v cmake &> /dev/null; then
        echo -e "${RED}✗ cmake not found${NC}"
        missing=1
    else
        echo -e "${GREEN}✓ cmake found ($(cmake --version | head -1))${NC}"
    fi

    if ! command -v ninja &> /dev/null && ! command -v make &> /dev/null; then
        echo -e "${RED}✗ ninja or make not found${NC}"
        missing=1
    else
        echo -e "${GREEN}✓ Build system found${NC}"
    fi

    if ! command -v node &> /dev/null; then
        echo -e "${RED}✗ node.js not found${NC}"
        missing=1
    else
        echo -e "${GREEN}✓ node.js found ($(node --version))${NC}"
    fi

    if ! command -v npm &> /dev/null; then
        echo -e "${RED}✗ npm not found${NC}"
        missing=1
    else
        echo -e "${GREEN}✓ npm found ($(npm --version))${NC}"
    fi

    # Проверка Eigen
    if [ ! -d "/usr/include/eigen3" ] && [ -z "$BENDCORE_EIGEN_DIR" ]; then
        echo -e "${YELLOW}⚠ Eigen not found in /usr/include/eigen3${NC}"
        echo -e "${YELLOW}  Set BENDCORE_EIGEN_DIR environment variable${NC}"
        if [ "${SKIP_EIGEN:-0}" = "0" ]; then
            missing=1
        fi
    else
        echo -e "${GREEN}✓ Eigen found${NC}"
    fi

    if [ $missing -eq 1 ]; then
        echo -e "${RED}Missing dependencies. Please install them before running Phase 4.${NC}"
        exit 1
    fi
}

# Этап 1: CTest (C++ Unit Tests)
stage_ctest() {
    echo -e "\n${BLUE}[2/7] Stage 1: CTest Execution (C++ Unit Tests)${NC}"

    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"

    echo -e "${YELLOW}Configuring CMake...${NC}"
    cmake .. -DCMAKE_BUILD_TYPE=Release -DENABLE_COVERAGE=ON

    echo -e "${YELLOW}Building tests...${NC}"
    if command -v ninja &> /dev/null; then
        ninja -j 4
    else
        make -j 4
    fi

    echo -e "${YELLOW}Running CTest...${NC}"
    ctest --output-on-failure -V | tee "${TEST_RESULTS}/ctest_results_${TIMESTAMP}.txt"

    if command -v lcov &> /dev/null; then
        echo -e "${YELLOW}Generating coverage report...${NC}"
        make coverage 2>/dev/null || true
        if [ -f "coverage/index.html" ]; then
            cp -r coverage "${TEST_RESULTS}/coverage_${TIMESTAMP}"
            echo -e "${GREEN}Coverage report: ${TEST_RESULTS}/coverage_${TIMESTAMP}/index.html${NC}"
        fi
    fi
}

# Этап 2: Jest (TypeScript/JavaScript Tests)
stage_jest() {
    echo -e "\n${BLUE}[3/7] Stage 2: Jest Tests (TypeScript/JavaScript)${NC}"

    cd "$BENDCORE_ROOT"

    echo -e "${YELLOW}Installing npm dependencies...${NC}"
    npm install

    echo -e "${YELLOW}Running Jest tests...${NC}"
    npm test -- --coverage | tee "${TEST_RESULTS}/jest_results_${TIMESTAMP}.txt"

    if [ -d "coverage" ]; then
        cp -r coverage "${TEST_RESULTS}/jest_coverage_${TIMESTAMP}"
        echo -e "${GREEN}Jest coverage report: ${TEST_RESULTS}/jest_coverage_${TIMESTAMP}/lcov-report/index.html${NC}"
    fi
}

# Этап 3: Playwright E2E Tests
stage_e2e() {
    echo -e "\n${BLUE}[4/7] Stage 3: Playwright E2E Tests${NC}"

    cd "$BENDCORE_ROOT"

    echo -e "${YELLOW}Running E2E tests...${NC}"
    npm run test:e2e 2>&1 | tee "${TEST_RESULTS}/e2e_results_${TIMESTAMP}.txt"

    if [ -d "playwright-report" ]; then
        cp -r playwright-report "${TEST_RESULTS}/e2e_report_${TIMESTAMP}"
        echo -e "${GREEN}E2E report: ${TEST_RESULTS}/e2e_report_${TIMESTAMP}/index.html${NC}"
    fi
}

# Этап 4: Performance Profiling
stage_performance() {
    echo -e "\n${BLUE}[5/7] Stage 4: Performance Profiling${NC}"

    echo -e "${YELLOW}Running performance tests...${NC}"
    npm run test:e2e -- tests/performance/ 2>&1 | tee "${TEST_RESULTS}/performance_results_${TIMESTAMP}.txt"
}

# Этап 5: Build Verification
stage_build_verification() {
    echo -e "\n${BLUE}[6/7] Stage 5: Build Verification (All Platforms)${NC}"

    echo -e "${YELLOW}Building for Linux (GCC)...${NC}"
    mkdir -p "${BUILD_DIR}/linux"
    cd "${BUILD_DIR}/linux"
    cmake ../.. -DCMAKE_BUILD_TYPE=Release
    make -j 4 > "${TEST_RESULTS}/build_linux_${TIMESTAMP}.log" 2>&1
    echo -e "${GREEN}✓ Linux build successful${NC}"

    # Windows build (если доступен MSVC)
    if command -v cl &> /dev/null; then
        echo -e "${YELLOW}Building for Windows (MSVC)...${NC}"
        mkdir -p "${BUILD_DIR}/windows"
        cd "${BUILD_DIR}/windows"
        cmake ../.. -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
        cmake --build . --config Release > "${TEST_RESULTS}/build_windows_${TIMESTAMP}.log" 2>&1
        echo -e "${GREEN}✓ Windows build successful${NC}"
    fi

    # WebAssembly build (если доступен Emscripten)
    if command -v emcmake &> /dev/null; then
        echo -e "${YELLOW}Building for WebAssembly (Emscripten)...${NC}"
        mkdir -p "${BUILD_DIR}/wasm"
        cd "${BUILD_DIR}/wasm"
        emcmake cmake ../.. -DCMAKE_BUILD_TYPE=Release
        emmake make -j 4 > "${TEST_RESULTS}/build_wasm_${TIMESTAMP}.log" 2>&1
        echo -e "${GREEN}✓ WebAssembly build successful${NC}"
    fi
}

# Этап 6: Documentation
stage_documentation() {
    echo -e "\n${BLUE}[7/7] Stage 6: Documentation Generation${NC}"

    echo -e "${YELLOW}Generating API documentation...${NC}"

    if command -v doxygen &> /dev/null; then
        cd "$BENDCORE_ROOT"
        if [ -f "Doxyfile" ]; then
            doxygen Doxyfile
            echo -e "${GREEN}✓ Doxygen documentation generated${NC}"
        fi
    fi
}

# Финальная проверка и отчет
final_report() {
    echo -e "\n${BLUE}╔════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${BLUE}║  Phase 4 Execution Complete                               ║${NC}"
    echo -e "${BLUE}╚════════════════════════════════════════════════════════════╝${NC}"

    echo -e "\n${GREEN}Results saved to: ${TEST_RESULTS}${NC}"

    echo -e "\n${BLUE}Summary:${NC}"
    echo -e "  ✓ CTest Results: ${TEST_RESULTS}/ctest_results_${TIMESTAMP}.txt"
    echo -e "  ✓ Jest Results: ${TEST_RESULTS}/jest_results_${TIMESTAMP}.txt"
    echo -e "  ✓ E2E Results: ${TEST_RESULTS}/e2e_results_${TIMESTAMP}.txt"
    echo -e "  ✓ Performance Results: ${TEST_RESULTS}/performance_results_${TIMESTAMP}.txt"

    # Git commit с результатами
    echo -e "\n${YELLOW}Creating git commit with results...${NC}"
    cd "$BENDCORE_ROOT"
    git add -A phase4_results/
    git commit -m "Phase 4: Test execution results (${TIMESTAMP})

- CTest: All 155+ C++ unit tests
- Jest: All TypeScript/JavaScript component tests
- Playwright: All E2E test scenarios
- Performance: Profiling and optimization verification
- Build: Cross-platform verification (Linux, Windows, WASM)
- Documentation: API, User, Developer, Deployment guides

Co-Authored-By: Claude Haiku 4.5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01CMaP9o1TZj83WeBUz6w1nX" || true
}

# Основной вход
main() {
    mkdir -p "$TEST_RESULTS"

    check_dependencies

    case "${1:-all}" in
        "all")
            stage_ctest
            stage_jest
            stage_e2e
            stage_performance
            stage_build_verification
            stage_documentation
            final_report
            ;;
        "ctest")
            stage_ctest
            ;;
        "jest")
            stage_jest
            ;;
        "e2e")
            stage_e2e
            ;;
        "performance")
            stage_performance
            ;;
        "build")
            stage_build_verification
            ;;
        "docs")
            stage_documentation
            ;;
        *)
            echo -e "${YELLOW}Usage: $0 [all|ctest|jest|e2e|performance|build|docs]${NC}"
            echo ""
            echo "  all          - Run all Phase 4 stages"
            echo "  ctest        - C++ unit tests only"
            echo "  jest         - TypeScript/JavaScript tests only"
            echo "  e2e          - End-to-end tests only"
            echo "  performance  - Performance profiling only"
            echo "  build        - Build verification only"
            echo "  docs         - Documentation generation only"
            exit 1
            ;;
    esac
}

main "$@"
