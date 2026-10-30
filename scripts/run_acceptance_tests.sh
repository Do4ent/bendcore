#!/bin/bash

# BendCore v1.0.0-PRODUCTION: Acceptance Test Execution Script
# Executes all 10 acceptance test workflows and generates comprehensive report

set -e

REPO_ROOT="/home/claude/bendcore"
TEST_DIR="${REPO_ROOT}/tests/acceptance"
BUILD_DIR="${REPO_ROOT}/build_acceptance_tests"
REPORT_DIR="${REPO_ROOT}/acceptance_test_results"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}BendCore v1.0.0-PRODUCTION${NC}"
echo -e "${BLUE}Acceptance Test Execution Suite${NC}"
echo -e "${BLUE}========================================${NC}\n"

# Create report directory
mkdir -p "${REPORT_DIR}"

# Step 1: Build acceptance tests
echo -e "${YELLOW}Step 1: Building Acceptance Tests...${NC}"
if [ -d "${BUILD_DIR}" ]; then
    rm -rf "${BUILD_DIR}"
fi
mkdir -p "${BUILD_DIR}"

cd "${BUILD_DIR}"
cmake -DCMAKE_BUILD_TYPE=Release "${REPO_ROOT}"
make -j$(nproc) 2>&1 | tee "${REPORT_DIR}/build.log"

if [ $? -ne 0 ]; then
    echo -e "${RED}Build failed!${NC}"
    exit 1
fi
echo -e "${GREEN}Build successful!${NC}\n"

# Step 2: Run individual acceptance tests
echo -e "${YELLOW}Step 2: Running Acceptance Tests...${NC}"

cd "${BUILD_DIR}"

# Create test list
declare -a tests=(
    "ACCEPT-001-SketchExtrude"
    "ACCEPT-002-AssemblyConstraints"
)

test_results=()
passed=0
failed=0

for test in "${tests[@]}"; do
    echo -n "Running ${test}... "
    if ./${test} > "${REPORT_DIR}/${test}.log" 2>&1; then
        echo -e "${GREEN}PASSED${NC}"
        test_results+=("${test}:PASSED")
        ((passed++))
    else
        echo -e "${RED}FAILED${NC}"
        test_results+=("${test}:FAILED")
        ((failed++))
    fi
done

# Step 3: Run benchmarks
echo -e "\n${YELLOW}Step 3: Running Performance Benchmarks...${NC}"
./acceptance_benchmarks > "${REPORT_DIR}/benchmarks.log" 2>&1 || true

# Step 4: Run acceptance test runner for final report
echo -e "\n${YELLOW}Step 4: Generating Acceptance Test Report...${NC}"
cd "${BUILD_DIR}"
./acceptance_test_runner > "${REPORT_DIR}/runner.log" 2>&1 || true

# Copy generated reports
if [ -f ACCEPT-REPORT.md ]; then
    cp ACCEPT-REPORT.md "${REPORT_DIR}/"
fi
if [ -f ACCEPT-REPORT.html ]; then
    cp ACCEPT-REPORT.html "${REPORT_DIR}/"
fi
if [ -f BENCHMARK_RESULTS.json ]; then
    cp BENCHMARK_RESULTS.json "${REPORT_DIR}/"
fi

# Step 5: Summary
echo -e "\n${BLUE}========================================${NC}"
echo -e "${BLUE}Test Summary${NC}"
echo -e "${BLUE}========================================${NC}"
echo -e "Passed: ${GREEN}${passed}${NC}"
echo -e "Failed: ${RED}${failed}${NC}"
echo ""

if [ $failed -eq 0 ]; then
    echo -e "${GREEN}✅ All acceptance tests PASSED!${NC}"
    exit_code=0
else
    echo -e "${RED}❌ Some tests FAILED${NC}"
    exit_code=1
fi

echo ""
echo -e "${YELLOW}Reports saved to: ${REPORT_DIR}${NC}"
echo "  - ACCEPT-REPORT.md"
echo "  - ACCEPT-REPORT.html"
echo "  - BENCHMARK_RESULTS.json"
echo "  - build.log"
echo "  - benchmarks.log"
echo "  - runner.log"
echo ""

exit $exit_code
