// P1-02: Security & Reliability - Comprehensive Security Testing
// Tests for all 8 security items: input validation, serialization, network security,
// memory safety, vulnerability assessment, secret management, crash recovery, fuzz testing

#include "bendcore/security.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <chrono>
#include <vector>

using namespace bendcore::security;

static int failures = 0;
static int test_count = 0;

static void check(bool ok, const char* what) {
    test_count++;
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    } else {
        std::printf("PASS: %s\n", what);
    }
}

static void print_header(const char* section) {
    std::printf("\n=== %s ===\n", section);
}

// ============================================================================
// SEC-001: Input Validation Tests
// ============================================================================

static void test_input_validation() {
    print_header("SEC-001: Input Validation & Sanitization");

    // Numeric bounds checking
    check(InputValidator::validate_radius(1.0), "validate_radius(1.0)");
    check(InputValidator::validate_radius(100.0), "validate_radius(100.0)");
    check(InputValidator::validate_radius(1e6), "validate_radius(1e6)");
    check(!InputValidator::validate_radius(-1.0), "!validate_radius(-1.0)");
    check(!InputValidator::validate_radius(0.0), "!validate_radius(0.0)");

    // Angle validation
    check(InputValidator::validate_angle(0.0), "validate_angle(0.0)");
    check(InputValidator::validate_angle(90.0), "validate_angle(90.0)");
    check(InputValidator::validate_angle(-180.0), "validate_angle(-180.0)");
    check(InputValidator::validate_angle(360.0), "validate_angle(360.0)");
    check(!InputValidator::validate_angle(361.0), "!validate_angle(361.0)");

    // Length validation
    check(InputValidator::validate_length(1.0), "validate_length(1.0)");
    check(InputValidator::validate_length(1000.0), "validate_length(1000.0)");
    check(!InputValidator::validate_length(0.0), "!validate_length(0.0)");
    check(!InputValidator::validate_length(-1.0), "!validate_length(-1.0)");

    // Ratio validation
    check(InputValidator::validate_ratio(0.5), "validate_ratio(0.5)");
    check(InputValidator::validate_ratio(0.001), "validate_ratio(0.001)");
    check(!InputValidator::validate_ratio(0.0), "!validate_ratio(0.0)");
    check(!InputValidator::validate_ratio(1.0), "!validate_ratio(1.0)");

    // String sanitization
    std::string sanitized = InputValidator::sanitize_filename("test_file.txt");
    check(sanitized == "test_file.txt", "sanitize_filename(test_file.txt)");

    sanitized = InputValidator::sanitize_filename("test file!@#");
    check(sanitized == "testfile", "sanitize_filename removes invalid chars");

    // UTF-8 validation
    check(InputValidator::validate_utf8("Hello"), "validate_utf8(ASCII)");
    check(InputValidator::validate_utf8("Привет"), "validate_utf8(Cyrillic)");
    check(!InputValidator::validate_utf8("\xFF\xFE"), "!validate_utf8(invalid)");

    // Input size validation
    check(InputValidator::validate_input_size(100), "validate_input_size(100)");
    check(InputValidator::validate_input_size(1024 * 1024),
          "validate_input_size(1MB)");
    check(!InputValidator::validate_input_size(0), "!validate_input_size(0)");
    check(!InputValidator::validate_input_size(1024 * 1024 + 1),
          "!validate_input_size(>1MB)");

    // Label sanitization
    sanitized = InputValidator::sanitize_label("Part_001");
    check(sanitized == "Part_001", "sanitize_label(Part_001)");

    sanitized = InputValidator::sanitize_label("My Design!@#");
    check(sanitized == "My Design", "sanitize_label removes special chars");

    std::string long_label(300, 'A');
    sanitized = InputValidator::sanitize_label(long_label);
    check(sanitized.length() <= 255, "sanitize_label truncates long strings");
}

// ============================================================================
// SEC-002: Serialization Tests
// ============================================================================

static void test_serialization() {
    print_header("SEC-002: Safe Serialization & Deserialization");

    // CRC32 computation and verification
    const uint8_t data[] = "Hello World";
    uint32_t crc = SerializationValidator::crc32(data, 11);
    check(crc != 0, "crc32 computation");

    uint32_t crc2 = SerializationValidator::crc32(data, 11);
    check(crc == crc2, "crc32 consistency");

    check(SerializationValidator::verify_crc32(data, 11, crc),
          "verify_crc32(correct)");
    check(!SerializationValidator::verify_crc32(data, 11, crc + 1),
          "!verify_crc32(incorrect)");

    // JSON schema validation
    check(SerializationValidator::validate_json_schema("{}"),
          "validate_json_schema({})");
    check(SerializationValidator::validate_json_schema("[]"),
          "validate_json_schema([])");
    check(SerializationValidator::validate_json_schema(R"({"key": "value"})"),
          "validate_json_schema(complex)");

    check(!SerializationValidator::validate_json_schema("{"),
          "!validate_json_schema(unbalanced)");
    check(!SerializationValidator::validate_json_schema("{{{{"),
          "!validate_json_schema(malformed)");

    // Protobuf schema validation
    const uint8_t valid_pb[] = {0x08, 0x01};  // Field 1, value 1
    check(SerializationValidator::validate_protobuf_schema(valid_pb, 2),
          "validate_protobuf_schema(valid)");

    const uint8_t invalid_wire[] = {0x3F};  // Wire type 7 (invalid)
    check(!SerializationValidator::validate_protobuf_schema(invalid_wire, 1),
          "!validate_protobuf_schema(invalid wire type)");

    // Version compatibility
    uint32_t v1_0_0 = (1 << 16) | 0;
    uint32_t v1_5_0 = (1 << 16) | 5;
    uint32_t v2_0_0 = (2 << 16) | 0;

    check(SerializationValidator::check_version_compatibility(v1_0_0, v1_5_0),
          "check_version_compatibility(1.0.0 with 1.5.0)");
    check(!SerializationValidator::check_version_compatibility(v2_0_0, v1_0_0),
          "!check_version_compatibility(major version mismatch)");
}

// ============================================================================
// SEC-003: Network Security Tests
// ============================================================================

static void test_network_security() {
    print_header("SEC-003: IPC & Network Security");

    // JWT token validation
    check(NetworkSecurity::validate_jwt_token(
              "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9."
              "eyJzdWIiOiIxMjM0NTY3ODkwIn0."
              "dozjgNryP4J3jVmNHl0w5N_XgL0n3I9PlFUP0THsR8U"),
          "validate_jwt_token(valid)");

    check(!NetworkSecurity::validate_jwt_token("invalid"),
          "!validate_jwt_token(invalid format)");
    check(!NetworkSecurity::validate_jwt_token("only.two"),
          "!validate_jwt_token(only two parts)");

    // Token expiry validation
    auto now = std::chrono::system_clock::now();
    int64_t current_time =
        std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch())
            .count();

    check(NetworkSecurity::validate_token_expiry(current_time + 3600),
          "validate_token_expiry(future)");
    check(!NetworkSecurity::validate_token_expiry(current_time - 3600),
          "!validate_token_expiry(past)");

    // Rate limiting
    bool rate_ok = NetworkSecurity::rate_limit_check("test_client_1", 60);
    check(rate_ok, "rate_limit_check(first request)");

    // API endpoint validation
    check(NetworkSecurity::validate_api_endpoint("/api/v1/bend"),
          "validate_api_endpoint(/api/v1/bend)");
    check(NetworkSecurity::validate_api_endpoint("/api/v1/sketch"),
          "validate_api_endpoint(/api/v1/sketch)");
    check(!NetworkSecurity::validate_api_endpoint("/invalid/endpoint"),
          "!validate_api_endpoint(invalid)");

    // Audit logging (should not crash)
    NetworkSecurity::log_api_request("POST", "/api/v1/bend", "client_1",
                                      "200 OK");
    check(true, "log_api_request executes");
}

// ============================================================================
// SEC-004: Memory Safety Tests
// ============================================================================

static void test_memory_safety() {
    print_header("SEC-004: Memory Safety & Leak Prevention");

    // Safe memcpy
    uint8_t dest[10] = {0};
    const uint8_t src[5] = {1, 2, 3, 4, 5};

    check(MemorySafety::safe_memcpy(dest, src, 5, 10),
          "safe_memcpy(valid)");
    check(dest[0] == 1, "safe_memcpy copies first byte");
    check(dest[4] == 5, "safe_memcpy copies last byte");

    check(!MemorySafety::safe_memcpy(dest, src, 15, 10),
          "!safe_memcpy(overflow)");
    check(!MemorySafety::safe_memcpy(nullptr, src, 5, 10),
          "!safe_memcpy(null dest)");
    check(!MemorySafety::safe_memcpy(dest, nullptr, 5, 10),
          "!safe_memcpy(null src)");

    // Buffer bounds checking
    uint8_t buffer[100] = {0};

    check(MemorySafety::validate_buffer_bounds(buffer, 0, 10, 100),
          "validate_buffer_bounds(valid access)");
    check(MemorySafety::validate_buffer_bounds(buffer, 50, 50, 100),
          "validate_buffer_bounds(valid at end)");

    check(!MemorySafety::validate_buffer_bounds(buffer, 0, 101, 100),
          "!validate_buffer_bounds(overflow)");
    check(!MemorySafety::validate_buffer_bounds(buffer, 90, 20, 100),
          "!validate_buffer_bounds(out of bounds)");
    check(!MemorySafety::validate_buffer_bounds(nullptr, 0, 10, 100),
          "!validate_buffer_bounds(null ptr)");

    // Stack safety
    check(MemorySafety::is_stack_safe(), "is_stack_safe");

    // Memory reporting (should not crash)
    MemorySafety::enable_leak_detection();
    check(true, "enable_leak_detection");
}

// ============================================================================
// SEC-005: Vulnerability Assessment Tests
// ============================================================================

static void test_vulnerability_assessment() {
    print_header("SEC-005: Vulnerability Assessment & Patching");

    auto deps = VulnerabilityAssessment::get_dependencies();
    check(deps.size() > 0, "get_dependencies returns list");

    bool has_zlib = false;
    for (const auto& dep : deps) {
        if (dep.name == "zlib") {
            has_zlib = true;
            check(!dep.has_vulnerabilities, "zlib has no known vulnerabilities");
        }
    }
    check(has_zlib, "zlib is in dependencies list");

    check(VulnerabilityAssessment::check_owasp_top_10_coverage(),
          "check_owasp_top_10_coverage");
    check(VulnerabilityAssessment::check_cwe_top_25_coverage(),
          "check_cwe_top_25_coverage");

    check(VulnerabilityAssessment::meets_patch_sla("CRITICAL", 24),
          "meets_patch_sla(CRITICAL)");
}

// ============================================================================
// SEC-006: Secret Management Tests
// ============================================================================

static void test_secret_management() {
    print_header("SEC-006: Secret Management");

    // Secret scanning
    std::string clean_code = "int main() { std::cout << \"Hello\"; }";
    check(SecretManager::scan_for_secrets_in_code(clean_code),
          "scan_for_secrets(clean code)");

    std::string with_secret =
        R"(const char* api_key = "abcdef1234567890abcdef1234567890")";
    check(!SecretManager::scan_for_secrets_in_code(with_secret),
          "!scan_for_secrets(with api_key)");

    // Secret existence check
    bool secret_exists = SecretManager::secret_exists("NONEXISTENT");
    check(!secret_exists, "secret_exists returns false for nonexistent");

    // Audit (should not crash)
    SecretManager::audit_secrets([](const std::string& msg) {
        // Receive audit messages
    });
    check(true, "audit_secrets completes");
}

// ============================================================================
// SEC-007: Crash Recovery Tests
// ============================================================================

static void test_crash_recovery() {
    print_header("SEC-007: Crash Recovery & Resilience");

    // Autosave control
    CrashRecovery::enable_autosave(300);
    check(CrashRecovery::is_autosave_enabled(), "enable_autosave");

    CrashRecovery::disable_autosave();
    check(!CrashRecovery::is_autosave_enabled(), "disable_autosave");

    // Recovery file path
    std::string path = CrashRecovery::get_recovery_filepath();
    check(!path.empty(), "get_recovery_filepath returns non-empty");
    check(path.find("recovery") != std::string::npos,
          "recovery filename contains 'recovery'");

    // Crash detection (no recovery file should exist initially)
    check(!CrashRecovery::has_recovery_file(), "!has_recovery_file");

    // Undo/redo preservation (should not crash)
    CrashRecovery::preserve_undo_history("test_history");
    CrashRecovery::restore_undo_history("test_history");
    check(true, "undo/redo preservation completes");
}

// ============================================================================
// SEC-008: Fuzz Testing Tests
// ============================================================================

static void test_fuzz_testing() {
    print_header("SEC-008: Fuzz Testing & Edge Cases");

    // Generate random dimensions
    auto dims = FuzzTesting::generate_random_valid_dimensions(100);
    check(dims.size() == 100, "generate_random_valid_dimensions(100)");

    int valid_dims = 0;
    for (auto d : dims) {
        if (InputValidator::validate_radius(d)) {
            valid_dims++;
        }
    }
    check(valid_dims == 100, "all generated dimensions are valid");

    // Generate random angles
    auto angles = FuzzTesting::generate_random_valid_angles(100);
    check(angles.size() == 100, "generate_random_valid_angles(100)");

    int valid_angles = 0;
    for (auto a : angles) {
        if (InputValidator::validate_angle(a)) {
            valid_angles++;
        }
    }
    check(valid_angles == 100, "all generated angles are valid");

    // Malformed inputs
    auto malformed = FuzzTesting::generate_malformed_inputs(7);
    check(malformed.size() == 7, "generate_malformed_inputs(7)");

    // Extreme geometry
    check(FuzzTesting::test_extreme_geometry(1000),
          "test_extreme_geometry(1000)");

    // Large sketch
    check(FuzzTesting::test_large_sketch(100 * 1024),
          "test_large_sketch(100KB)");

    // Numeric precision
    check(FuzzTesting::test_numeric_precision(), "test_numeric_precision");

    // Full fuzz suite
    auto result = FuzzTesting::run_fuzz_suite();
    check(result.total_tests > 0, "run_fuzz_suite returns results");
    check(result.passed_tests >= 5,
          "run_fuzz_suite: at least 5 of 6 tests pass");

    std::printf("  Fuzz Suite: %d/%d tests passed\n", result.passed_tests,
                result.total_tests);
}

// ============================================================================
// Integration Tests
// ============================================================================

static void test_integration() {
    print_header("Integration Tests");

    // Initialization
    SecurityConfig::initialize_security();
    check(true, "SecurityConfig::initialize_security");

    check(SecurityConfig::is_input_validation_enabled(),
          "input_validation_enabled");
    check(SecurityConfig::is_memory_checking_enabled(),
          "memory_checking_enabled");
    check(SecurityConfig::is_network_security_enabled(),
          "network_security_enabled");

    // Combined validation chain
    std::string filename = "test_design!@#.bcad";
    std::string sanitized = InputValidator::sanitize_filename(filename);
    check(!sanitized.empty(), "filename sanitization chain");
    check(InputValidator::validate_input_size(sanitized.length()),
          "sanitized filename passes size check");

    // Serialization chain
    std::string json = R"({"radius": 10.0, "angle": 45.0})";
    check(SerializationValidator::validate_json_schema(json),
          "JSON schema validation");

    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(json.c_str());
    uint32_t crc = SerializationValidator::crc32(bytes, json.length());
    check(SerializationValidator::verify_crc32(bytes, json.length(), crc),
          "CRC32 round-trip");
}

// ============================================================================
// Performance Baseline Tests
// ============================================================================

static void test_performance() {
    print_header("Performance Baseline Tests");

    // Validation performance
    auto start = std::chrono::high_resolution_clock::now();

    int valid_count = 0;
    for (int i = 0; i < 10000; ++i) {
        if (InputValidator::validate_radius(i * 0.1 + 1.0)) {
            valid_count++;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    check(valid_count > 9000, "validate 10,000 inputs");
    check(duration.count() < 100, "validation completes in <100ms");

    std::printf("  Validation: %d valid inputs in %ldms\n", valid_count,
                duration.count());

    // CRC32 performance
    std::vector<uint8_t> data(1024 * 100);  // 100KB
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] = i % 256;
    }

    start = std::chrono::high_resolution_clock::now();
    uint32_t crc = SerializationValidator::crc32(data.data(), data.size());
    end = std::chrono::high_resolution_clock::now();

    duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    check(crc != 0, "crc32 computation");
    check(duration.count() < 50, "CRC32 completes in <50ms for 100KB");

    std::printf("  CRC32: computed in %ldms for 100KB\n", duration.count());
}

// ============================================================================
// Main Test Execution
// ============================================================================

int main() {
    std::printf("\n");
    std::printf("=========================================================\n");
    std::printf("       BendCore P1-02: Security & Reliability Tests\n");
    std::printf("         8 Security Items, 80+ Comprehensive Tests\n");
    std::printf("=========================================================\n");

    test_input_validation();
    test_serialization();
    test_network_security();
    test_memory_safety();
    test_vulnerability_assessment();
    test_secret_management();
    test_crash_recovery();
    test_fuzz_testing();
    test_integration();
    test_performance();

    std::printf("\n");
    std::printf("=========================================================\n");
    std::printf("                    TEST RESULTS\n");
    std::printf("=========================================================\n");
    std::printf("Total tests: %d\n", test_count);
    std::printf("Passed: %d\n", test_count - failures);
    std::printf("Failed: %d\n", failures);

    if (failures == 0) {
        std::printf("\nSUCCESS: All tests passed!\n");
        std::printf("=========================================================\n\n");
        return 0;
    } else {
        std::printf("\nFAILURE: %d test(s) failed\n", failures);
        std::printf("=========================================================\n\n");
        return 1;
    }
}
