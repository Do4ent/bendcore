// OCCT Initialization Tests (Q3 2027, Task 1, Subtask 1)
#include <bendcore/occt_config.hpp>

#include <cassert>
#include <iostream>
#include <string>

using namespace bendcore::occt;

int test_occt_version_detected() {
    auto info = get_occt_info();
    // After initialization, version should be detected
    std::cout << "Test 1: OCCT version detected... ";

    // Before initialization, version should be 0
    assert(info.version_major == 0 && info.version_minor == 0);

    // Initialize OCCT
    OCCTErrorCode result = initialize_occt();
    assert(result == OCCTErrorCode::SUCCESS);

    // Now check version
    info = get_occt_info();
    assert(info.version_major >= OCCT_VERSION_MAJOR);
    assert(info.version_minor >= OCCT_VERSION_MINOR || info.version_major > OCCT_VERSION_MAJOR);

    std::cout << "PASS (OCCT v" << info.version_major << "." << info.version_minor << "."
              << info.version_patch << ")" << std::endl;
    return 0;
}

int test_platform_validation() {
    std::cout << "Test 2: Platform validation (Windows x64)... ";

    // This test passes if compilation succeeded (_WIN32 and _WIN64 are checked)
    // The code doesn't compile on non-Windows platforms
    #if defined(_WIN64)
        std::cout << "PASS (Windows x64 detected)" << std::endl;
        return 0;
    #else
        std::cout << "FAIL (not Windows x64)" << std::endl;
        return 1;
    #endif
}

int test_initialization_succeeds() {
    std::cout << "Test 3: Initialization succeeds... ";

    // Clean state
    cleanup_occt();
    assert(!is_occt_initialized());

    // Initialize
    OCCTErrorCode result = initialize_occt();
    assert(result == OCCTErrorCode::SUCCESS);
    assert(is_occt_initialized());

    // Second initialization should also succeed (idempotent)
    result = initialize_occt();
    assert(result == OCCTErrorCode::SUCCESS);
    assert(is_occt_initialized());

    std::cout << "PASS" << std::endl;
    return 0;
}

int test_exception_handling() {
    std::cout << "Test 4: Exception handling works... ";

    // Initialize OCCT (should handle exceptions gracefully)
    OCCTErrorCode result = initialize_occt();

    // Even if there were errors, we should get a valid error code
    assert(result == OCCTErrorCode::SUCCESS ||
           result == OCCTErrorCode::INITIALIZATION_FAILED);

    auto info = get_occt_info();
    // Error message should be set if initialization failed
    if (result != OCCTErrorCode::SUCCESS) {
        assert(!info.error_message.empty());
    }

    std::cout << "PASS" << std::endl;
    return 0;
}

int test_cleanup_called() {
    std::cout << "Test 5: Cleanup called... ";

    // Initialize
    initialize_occt();
    assert(is_occt_initialized());

    // Cleanup
    cleanup_occt();
    assert(!is_occt_initialized());

    // Second cleanup should not crash (safe)
    cleanup_occt();
    assert(!is_occt_initialized());

    std::cout << "PASS" << std::endl;
    return 0;
}

int main() {
    std::cout << "=== OCCT Initialization Tests ===" << std::endl;

    int failed = 0;

    try {
        failed += test_occt_version_detected();
        failed += test_platform_validation();
        failed += test_initialization_succeeds();
        failed += test_exception_handling();
        failed += test_cleanup_called();
    } catch (const std::exception& e) {
        std::cout << "FATAL ERROR: " << e.what() << std::endl;
        failed++;
    } catch (...) {
        std::cout << "FATAL ERROR: Unknown exception" << std::endl;
        failed++;
    }

    std::cout << "\n=== Test Summary ===" << std::endl;
    std::cout << "Total tests: 5" << std::endl;
    std::cout << "Failed: " << failed << std::endl;
    std::cout << "Passed: " << (5 - failed) << std::endl;

    return failed;
}
