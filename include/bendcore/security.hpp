#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <functional>
#include <limits>
#include <regex>
#include <memory>

namespace bendcore::security {

// ============================================================================
// SEC-001: Input Validation & Sanitization
// ============================================================================

class InputValidator {
public:
    // Numeric bounds checking
    static bool validate_radius(double radius) {
        return radius > 0 && !std::isnan(radius) && !std::isinf(radius) &&
               radius <= 1e6;  // Max 1 million mm
    }

    static bool validate_angle(double angle) {
        return angle >= -360.0 && angle <= 360.0 && !std::isnan(angle) &&
               !std::isinf(angle);
    }

    static bool validate_length(double length) {
        return length > 0 && !std::isnan(length) && !std::isinf(length) &&
               length <= 1e7;  // Max 10 million mm
    }

    static bool validate_ratio(double ratio) {
        return ratio > 0 && ratio < 1.0 && !std::isnan(ratio) &&
               !std::isinf(ratio);
    }

    // String validation and sanitization
    static std::string sanitize_filename(const std::string& filename);
    static std::string sanitize_utf8(const std::string& input);
    static bool validate_utf8(const std::string& input);

    // Path validation
    static bool is_safe_path(const std::string& path);
    static std::string normalize_path(const std::string& path);

    // Size bounds
    static bool validate_input_size(size_t size, size_t max_size = 1024 * 1024);  // 1MB default

    // Dimension label validation
    static std::string sanitize_label(const std::string& label);
};

// ============================================================================
// SEC-002: Safe Serialization & Deserialization
// ============================================================================

class SerializationValidator {
public:
    // CRC32 checksum for file integrity
    static uint32_t crc32(const uint8_t* data, size_t len);
    static bool verify_crc32(const uint8_t* data, size_t len, uint32_t expected_crc);

    // Schema validation for JSON/Protobuf
    static bool validate_json_schema(const std::string& json_str);
    static bool validate_protobuf_schema(const uint8_t* data, size_t len);

    // File integrity checks
    static bool validate_file_signature(const std::string& filepath);
    static bool is_valid_project_file(const std::string& filepath);

    // Version compatibility
    static bool check_version_compatibility(uint32_t file_version,
                                           uint32_t current_version);
};

// ============================================================================
// SEC-003: IPC & Network Security
// ============================================================================

class NetworkSecurity {
public:
    // JWT token validation
    struct JWTClaims {
        std::string sub;      // subject
        std::string iss;      // issuer
        int64_t exp;          // expiration time (Unix timestamp)
        int64_t iat;          // issued at
        std::string scope;    // permissions scope
    };

    static bool validate_jwt_token(const std::string& token);
    static bool validate_token_expiry(int64_t exp_timestamp);
    static JWTClaims extract_jwt_claims(const std::string& token);

    // HTTPS/SSL certificate validation
    static bool validate_ssl_certificate(const std::string& cert_path);
    static bool is_https_enabled();

    // API endpoint security
    static bool validate_api_endpoint(const std::string& endpoint);
    static bool rate_limit_check(const std::string& client_id,
                                 int requests_per_minute = 60);

    // Request audit logging
    static void log_api_request(const std::string& method,
                               const std::string& endpoint,
                               const std::string& client_id,
                               const std::string& status);
};

// ============================================================================
// SEC-004: Memory Safety & Leak Prevention
// ============================================================================

class MemorySafety {
public:
    // Memory bounds checking
    static bool safe_memcpy(void* dest, const void* src, size_t count,
                           size_t dest_size);

    // Buffer overflow detection
    static bool validate_buffer_bounds(const void* ptr, size_t offset,
                                       size_t access_size, size_t buffer_size);

    // Stack overflow protection helpers
    static void check_stack_depth(int max_depth = 1000);
    static bool is_stack_safe();

    // Memory leak detection support
    static void enable_leak_detection();
    static void report_memory_stats();
};

// ============================================================================
// SEC-005: Vulnerability Assessment & Patching
// ============================================================================

class VulnerabilityAssessment {
public:
    struct Dependency {
        std::string name;
        std::string version;
        std::string last_update;
        bool has_vulnerabilities;
    };

    struct Vulnerability {
        std::string cve_id;
        std::string description;
        std::string severity;  // CRITICAL, HIGH, MEDIUM, LOW
        std::string patch_version;
    };

    // Dependency tracking
    static std::vector<Dependency> get_dependencies();
    static bool is_dependency_vulnerable(const std::string& name,
                                         const std::string& version);

    // OWASP Top 10 coverage check
    static bool check_owasp_top_10_coverage();

    // CWE Top 25 coverage check
    static bool check_cwe_top_25_coverage();

    // Security patch SLA
    static bool meets_patch_sla(const std::string& severity,
                               int max_hours = 24);
};

// ============================================================================
// SEC-006: Secret Management
// ============================================================================

class SecretManager {
public:
    // Environment-based secret retrieval
    static std::string get_secret(const std::string& secret_name);
    static bool secret_exists(const std::string& secret_name);

    // Git secret detection
    static bool scan_for_secrets_in_code(const std::string& code);
    static std::vector<std::string> detect_hardcoded_credentials(
        const std::string& filepath);

    // Credential rotation
    static void rotate_credentials(const std::string& credential_type,
                                   int rotation_days = 90);

    // Secret audit
    static void audit_secrets(std::function<void(const std::string&)> report_fn);
};

// ============================================================================
// SEC-007: Crash Recovery & Resilience
// ============================================================================

class CrashRecovery {
public:
    struct RecoveryPoint {
        std::string filepath;
        int64_t timestamp;
        uint32_t crc32;
        std::string version;
    };

    // Auto-save mechanism
    static void enable_autosave(int interval_seconds = 300);  // 5 minutes
    static void disable_autosave();
    static bool is_autosave_enabled();

    // Crash detection and recovery
    static bool detect_crash();
    static bool has_recovery_file();
    static std::string get_recovery_filepath();

    // Recovery data validation
    static bool validate_recovery_data(const std::string& filepath);
    static bool load_recovery(const std::string& filepath);

    // Undo/redo history preservation
    static void preserve_undo_history(const std::string& history_file);
    static bool restore_undo_history(const std::string& history_file);
};

// ============================================================================
// SEC-008: Fuzz Testing & Edge Cases
// ============================================================================

class FuzzTesting {
public:
    // Fuzz test generators
    static std::vector<double> generate_random_valid_dimensions(int count = 100);
    static std::vector<double> generate_random_valid_angles(int count = 100);
    static std::vector<std::string> generate_malformed_inputs(int count = 100);

    // Edge case testing
    static bool test_extreme_geometry(int feature_count = 10000);
    static bool test_large_sketch(size_t size_bytes = 1024 * 1024);
    static bool test_numeric_precision();

    // Graceful failure verification
    static bool verify_graceful_failure(std::function<void()> test_fn);

    // Fuzz report
    struct FuzzResult {
        int total_tests;
        int passed_tests;
        int failed_tests;
        std::vector<std::string> failures;
        std::vector<std::string> edge_cases;
    };

    static FuzzResult run_fuzz_suite();
};

// ============================================================================
// Security Utilities
// ============================================================================

class SecurityConfig {
public:
    static void initialize_security();
    static void enable_all_security_checks();
    static void generate_security_report(const std::string& output_file);

    // Global security flags
    static bool is_input_validation_enabled();
    static bool is_memory_checking_enabled();
    static bool is_network_security_enabled();
};

}  // namespace bendcore::security
