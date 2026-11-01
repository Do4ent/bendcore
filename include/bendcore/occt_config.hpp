// OCCT Integration Configuration (Q3 2027, Task 1, Subtask 1)
// Open Cascade Technology (OCCT) constants, platform detection, and error codes
#pragma once

#include <string>

namespace bendcore::occt {

// OCCT version requirements
constexpr int OCCT_VERSION_MAJOR = 7;
constexpr int OCCT_VERSION_MINOR = 6;  // 7.6 = Ubuntu 24.04 libocct-*-dev
constexpr int OCCT_VERSION_PATCH = 0;

// Platform validation: Windows, Linux and macOS are supported; 64-bit only.
static_assert(sizeof(void*) == 8, "BendCore OCCT integration requires a 64-bit platform");

// Error codes for OCCT operations
enum class OCCTErrorCode {
    SUCCESS = 0,
    FILE_NOT_FOUND = 1,
    INVALID_FORMAT = 2,
    INITIALIZATION_FAILED = 3,
    SHAPE_EXTRACTION_FAILED = 4,
    GEOMETRY_ANALYSIS_FAILED = 5,
    PARAMETER_EXTRACTION_FAILED = 6,
    CACHE_ERROR = 7,
    PRECISION_ERROR = 8,
    UNSUPPORTED_FEATURE = 9,
    UNKNOWN_ERROR = 99
};

// OCCT library information
struct OCCTInfo {
    int version_major = 0;
    int version_minor = 0;
    int version_patch = 0;
    std::string library_path;
    bool is_initialized = false;
    std::string error_message;
};

// Get OCCT library information
OCCTInfo get_occt_info();

// Initialize OCCT with platform-specific setup
OCCTErrorCode initialize_occt();

// Cleanup OCCT resources
void cleanup_occt();

// Check if OCCT is properly initialized
bool is_occt_initialized();

// Convert error code to human-readable string
std::string occt_error_to_string(OCCTErrorCode code);

}  // namespace bendcore::occt
