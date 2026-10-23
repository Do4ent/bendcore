// OCCT Initialization Implementation (Q3 2027, Task 1, Subtask 1)
#include <bendcore/occt_config.hpp>

#include <Standard_Version.hxx>
#include <Message_Algorithm.hxx>
#include <Standard_Failure.hxx>

#include <sstream>
#include <iostream>

namespace bendcore::occt {

// Global OCCT state
static OCCTInfo g_occt_info;

OCCTInfo get_occt_info() {
    return g_occt_info;
}

OCCTErrorCode initialize_occt() {
    // Check if already initialized
    if (g_occt_info.is_initialized) {
        return OCCTErrorCode::SUCCESS;
    }

    try {
        // Get OCCT version information
        g_occt_info.version_major = OCC_VERSION_MAJOR;
        g_occt_info.version_minor = OCC_VERSION_MINOR;
        g_occt_info.version_patch = OCC_VERSION_MAINTENANCE;

        // Validate version is >= 7.7
        if (g_occt_info.version_major < OCCT_VERSION_MAJOR ||
            (g_occt_info.version_major == OCCT_VERSION_MAJOR &&
             g_occt_info.version_minor < OCCT_VERSION_MINOR)) {
            std::ostringstream oss;
            oss << "OCCT version " << g_occt_info.version_major << "."
                << g_occt_info.version_minor << "." << g_occt_info.version_patch
                << " is below required 7.7.0";
            g_occt_info.error_message = oss.str();
            return OCCTErrorCode::INITIALIZATION_FAILED;
        }

        // Setup message reporting for OCCT
        Message_Algorithm msg;

        // Mark as initialized
        g_occt_info.is_initialized = true;
        g_occt_info.library_path = "OCCT 7.7+";

        return OCCTErrorCode::SUCCESS;
    } catch (const Standard_Failure& e) {
        std::ostringstream oss;
        oss << "OCCT initialization failed: " << e.GetMessageString();
        g_occt_info.error_message = oss.str();
        g_occt_info.is_initialized = false;
        return OCCTErrorCode::INITIALIZATION_FAILED;
    } catch (const std::exception& e) {
        std::ostringstream oss;
        oss << "OCCT initialization error: " << e.what();
        g_occt_info.error_message = oss.str();
        g_occt_info.is_initialized = false;
        return OCCTErrorCode::INITIALIZATION_FAILED;
    }
}

void cleanup_occt() {
    // Clear OCCT state
    if (g_occt_info.is_initialized) {
        try {
            // Standard_Failure::Raise() is used for exception handling
            // No explicit cleanup needed for OCCT 7.7+
        } catch (...) {
            // Ignore errors during cleanup
        }
        g_occt_info.is_initialized = false;
    }
}

bool is_occt_initialized() {
    return g_occt_info.is_initialized;
}

std::string occt_error_to_string(OCCTErrorCode code) {
    switch (code) {
        case OCCTErrorCode::SUCCESS:
            return "Success";
        case OCCTErrorCode::FILE_NOT_FOUND:
            return "File not found";
        case OCCTErrorCode::INVALID_FORMAT:
            return "Invalid file format";
        case OCCTErrorCode::INITIALIZATION_FAILED:
            return "OCCT initialization failed";
        case OCCTErrorCode::SHAPE_EXTRACTION_FAILED:
            return "Shape extraction failed";
        case OCCTErrorCode::GEOMETRY_ANALYSIS_FAILED:
            return "Geometry analysis failed";
        case OCCTErrorCode::PARAMETER_EXTRACTION_FAILED:
            return "Parameter extraction failed";
        case OCCTErrorCode::CACHE_ERROR:
            return "Cache error";
        case OCCTErrorCode::PRECISION_ERROR:
            return "Precision error";
        case OCCTErrorCode::UNSUPPORTED_FEATURE:
            return "Unsupported feature";
        case OCCTErrorCode::UNKNOWN_ERROR:
            return "Unknown error";
        default:
            return "Unknown error code";
    }
}

}  // namespace bendcore::occt
