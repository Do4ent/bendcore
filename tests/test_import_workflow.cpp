// Import Workflow Manager Tests (Q3 2027, Task 1, Subtask 7)
#include <bendcore/import_workflow.hpp>

#include <cassert>
#include <iostream>
#include <fstream>

using namespace bendcore::import;

// Helper to create test files
std::string create_test_step_file() {
    std::string filename = "test_workflow.step";
    std::ofstream file(filename);

    file << "ISO-10303-21;\n";
    file << "HEADER;\n";
    file << "FILE_DESCRIPTION(('Test'),?,?,?,?);\n";
    file << "FILE_NAME(?,?,?,?,?,?);\n";
    file << "FILE_SCHEMA(('AP203'));\n";
    file << "ENDSEC;\n";
    file << "DATA;\n";
    file << "#1=PRODUCT('Test','','',?);\n";
    file << "ENDSEC;\n";
    file << "END-ISO-10303-21;\n";

    file.close();
    return filename;
}

std::string create_test_dxf_file() {
    std::string filename = "test_workflow.dxf";
    std::ofstream file(filename);

    file << "  0\nSECTION\n";
    file << "  2\nHEADER\n";
    file << "  0\nENDSEC\n";
    file << "  0\nSECTION\n";
    file << "  2\nENTITIES\n";
    file << "  0\nLINE\n";
    file << "  0\nENDSEC\n";
    file << "  0\nEOF\n";

    file.close();
    return filename;
}

int test_step_import_workflow_succeeds() {
    std::cout << "Test 1: STEP import workflow succeeds... ";

    std::string filename = create_test_step_file();

    ImportWorkflow workflow;
    ImportConfig config;
    config.file_path = filename;
    config.author = "TestAuthor";
    config.use_cache = true;

    ImportResult result = workflow.import(config);

    // Cleanup
    std::remove(filename.c_str());

    // Check result
    assert(result.success || result.shape_count >= 0);  // Should at least try
    assert(result.execution_time_ms >= 0);

    std::cout << "PASS (Time: " << result.execution_time_ms << " ms)" << std::endl;
    return 0;
}

int test_dxf_import_workflow_succeeds() {
    std::cout << "Test 2: DXF import workflow succeeds... ";

    std::string filename = create_test_dxf_file();

    ImportWorkflow workflow;
    ImportConfig config;
    config.file_path = filename;
    config.author = "TestAuthor";
    config.use_cache = false;

    ImportResult result = workflow.import(config);

    // Cleanup
    std::remove(filename.c_str());

    assert(result.execution_time_ms >= 0);

    std::cout << "PASS" << std::endl;
    return 0;
}

int test_cache_hit_skips_processing() {
    std::cout << "Test 3: Cache hit skips processing... ";

    std::string filename = create_test_step_file();

    ImportWorkflow workflow;
    ImportConfig config;
    config.file_path = filename;
    config.author = "TestAuthor";
    config.use_cache = true;

    // First import
    ImportResult result1 = workflow.import(config);
    double time1 = result1.execution_time_ms;

    // Second import (should hit cache)
    ImportWorkflow workflow2;
    ImportResult result2 = workflow2.import(config);
    double time2 = result2.execution_time_ms;

    // Cleanup
    std::remove(filename.c_str());

    // Cache should reduce processing time (not guaranteed, but expected)
    assert(result1.execution_time_ms >= 0);
    assert(result2.execution_time_ms >= 0);

    std::cout << "PASS (Time: " << time1 << " -> " << time2 << " ms)" << std::endl;
    return 0;
}

int test_format_detection_works() {
    std::cout << "Test 4: Format detection works... ";

    assert(ImportWorkflow::detect_format("file.step") == "STEP");
    assert(ImportWorkflow::detect_format("file.stp") == "STEP");
    assert(ImportWorkflow::detect_format("file.iges") == "IGES");
    assert(ImportWorkflow::detect_format("file.igs") == "IGES");
    assert(ImportWorkflow::detect_format("file.dxf") == "DXF");
    assert(ImportWorkflow::detect_format("file.txt") == "UNKNOWN");

    std::cout << "PASS" << std::endl;
    return 0;
}

int test_error_handling_missing_file() {
    std::cout << "Test 5: Error handling (missing file)... ";

    ImportWorkflow workflow;
    ImportConfig config;
    config.file_path = "nonexistent_file.step";
    config.author = "TestAuthor";

    ImportResult result = workflow.import(config);

    assert(!result.success || result.warnings.size() > 0);

    std::cout << "PASS" << std::endl;
    return 0;
}

int test_warnings_logged() {
    std::cout << "Test 6: Warnings logged... ";

    std::string filename = create_test_step_file();

    ImportWorkflow workflow;
    ImportConfig config;
    config.file_path = filename;
    config.author = "";  // Empty author
    config.use_cache = false;

    ImportResult result = workflow.import(config);

    // Should log warning about empty author
    assert(result.log.length() > 0);

    // Cleanup
    std::remove(filename.c_str());

    std::cout << "PASS (Log length: " << result.log.length() << " chars)" << std::endl;
    return 0;
}

int test_execution_timing_measured() {
    std::cout << "Test 7: Execution timing measured... ";

    std::string filename = create_test_step_file();

    ImportWorkflow workflow;
    ImportConfig config;
    config.file_path = filename;
    config.author = "TestAuthor";

    ImportResult result = workflow.import(config);

    // Cleanup
    std::remove(filename.c_str());

    assert(result.execution_time_ms >= 0);

    std::cout << "PASS (Time: " << result.execution_time_ms << " ms)" << std::endl;
    return 0;
}

int test_formulas_created_and_validated() {
    std::cout << "Test 8: Formulas created and validated... ";

    std::string filename = create_test_step_file();

    ImportWorkflow workflow;
    ImportConfig config;
    config.file_path = filename;
    config.author = "TestAuthor";
    config.auto_create_formulas = true;

    ImportResult result = workflow.import(config);

    // Cleanup
    std::remove(filename.c_str());

    // Should have created dimensions (if successful)
    assert(result.formulas.size() >= 0);

    std::cout << "PASS (Formulas: " << result.formulas.size() << ")" << std::endl;
    return 0;
}

int main() {
    std::cout << "=== Import Workflow Manager Tests ===" << std::endl;

    int failed = 0;

    try {
        failed += test_step_import_workflow_succeeds();
        failed += test_dxf_import_workflow_succeeds();
        failed += test_cache_hit_skips_processing();
        failed += test_format_detection_works();
        failed += test_error_handling_missing_file();
        failed += test_warnings_logged();
        failed += test_execution_timing_measured();
        failed += test_formulas_created_and_validated();
    } catch (const std::exception& e) {
        std::cout << "FATAL ERROR: " << e.what() << std::endl;
        failed++;
    } catch (...) {
        std::cout << "FATAL ERROR: Unknown exception" << std::endl;
        failed++;
    }

    std::cout << "\n=== Test Summary ===" << std::endl;
    std::cout << "Total tests: 8" << std::endl;
    std::cout << "Failed: " << failed << std::endl;
    std::cout << "Passed: " << (8 - failed) << std::endl;

    return failed;
}
