// STEP File Reader Implementation (Q3 2027, Task 1, Subtask 2)
#include <bendcore/step_reader.hpp>
#include <bendcore/occt_config.hpp>

#include <algorithm>
#include <sstream>
#include <fstream>
#include <cctype>

#if defined(_WIN32) || defined(_WIN64)
  #include <windows.h>
  #include <STEPCAFControl_Reader.hxx>
  #include <TDocStd_Document.hxx>
  #include <Handle_TDocStd_Document.hxx>
  #include <Standard_Failure.hxx>
  #include <TopExp_Explorer.hxx>
  #include <TopoDS_Solid.hxx>
  #include <XCAFDoc_ShapeTool.hxx>
  #include <XCAFDoc_DocumentTool.hxx>
  #include <TDataStd_Name.hxx>
#endif

namespace bendcore::step {

STEPReader::STEPReader() {
    #if defined(_WIN32) || defined(_WIN64)
      // OCCT will be initialized on first use
    #endif
}

STEPReader::~STEPReader() {
    // Cleanup handled by OCCT
}

std::string STEPReader::get_file_extension(const std::string& file_path) {
    size_t dot_pos = file_path.rfind('.');
    if (dot_pos == std::string::npos) {
        return "";
    }
    std::string ext = file_path.substr(dot_pos + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return ext;
}

bool STEPReader::is_supported_format(const std::string& file_path) {
    std::string ext = get_file_extension(file_path);
    return ext == "step" || ext == "stp" || ext == "iges" || ext == "igs";
}

#if defined(_WIN32) || defined(_WIN64)
std::wstring STEPReader::utf8_to_wide(const std::string& utf8_str) {
    if (utf8_str.empty()) {
        return std::wstring();
    }

    // Calculate required size
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, utf8_str.c_str(),
                                          (int)utf8_str.length(), NULL, 0);
    if (size_needed == 0) {
        return std::wstring();
    }

    // Convert UTF-8 to wide
    std::wstring wide_str(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8_str.c_str(),
                        (int)utf8_str.length(), &wide_str[0], size_needed);
    return wide_str;
}
#else
std::wstring STEPReader::utf8_to_wide(const std::string& utf8_str) {
    // Non-Windows stub
    std::wstring result(utf8_str.begin(), utf8_str.end());
    return result;
}
#endif

ImportResult STEPReader::read(const std::string& file_path) {
    ImportResult result;
    result.success = false;

    // Check file format
    if (!is_supported_format(file_path)) {
        result.error_message = "Unsupported file format (expected .step, .stp, .iges, .igs)";
        return result;
    }

    // Check if file exists
    std::ifstream file_check(file_path);
    if (!file_check.good()) {
        result.error_message = "File not found: " + file_path;
        return result;
    }
    file_check.close();

#if defined(_WIN32) || defined(_WIN64)
    try {
        // Initialize OCCT if needed
        occt::initialize_occt();

        // Convert path to wide char for Windows API
        std::wstring wide_path = utf8_to_wide(file_path);

        // Convert wide to OCCT-compatible string
        std::string occt_path(file_path.begin(), file_path.end());

        // Load STEP file
        STEPCAFControl_Reader reader;
        int status = reader.ReadFile(occt_path.c_str());

        if (status != IFSelect_RetDone) {
            result.error_message = "Failed to read STEP file: " + occt::occt_error_to_string(
                occt::OCCTErrorCode::INVALID_FORMAT);
            return result;
        }

        // Create document
        Handle(TDocStd_Document) doc = new TDocStd_Document("STEP");

        // Transfer data
        if (!reader.Transfer(doc)) {
            result.error_message = "Failed to transfer STEP data to document";
            return result;
        }

        // Extract shapes
        extract_shapes(file_path);
        extract_metadata(file_path);

        // Build result
        result.success = true;
        result.shape_count = (int)shapes_.size();
        result.warning_count = warning_count_;

        // Convert shape map keys to result
        for (const auto& [shape_id, shape_data] : shapes_) {
            result.shape_ids.push_back(shape_id);
        }

        // Add file properties
        result.properties = properties_;

    } catch (const Standard_Failure& e) {
        result.error_message = "OCCT error: ";
        result.error_message += (e.GetMessageString() ? std::string(e.GetMessageString()) : "Unknown error");
        return result;
    } catch (const std::exception& e) {
        result.error_message = std::string("Error: ") + e.what();
        return result;
    } catch (...) {
        result.error_message = "Unknown error reading STEP file";
        return result;
    }
#else
    result.error_message = "STEP file reading requires Windows platform with OCCT library";
    return result;
#endif
}

void STEPReader::extract_shapes(const std::string& file_path) {
    // Placeholder for shape extraction
    // In production, this would traverse the OCCT document and extract all solids
    shapes_["shape_0"] = "placeholder_shape_data";
    properties_["file_name"] = file_path;
}

void STEPReader::extract_metadata(const std::string& file_path) {
    // Placeholder for metadata extraction
    properties_["file_path"] = file_path;
    properties_["file_extension"] = get_file_extension(file_path);
}

std::string STEPReader::get_shape_data(const std::string& shape_id) const {
    auto it = shapes_.find(shape_id);
    if (it != shapes_.end()) {
        return it->second;
    }
    return "";
}

std::vector<std::string> STEPReader::get_property_keys() const {
    std::vector<std::string> keys;
    for (const auto& [key, value] : properties_) {
        keys.push_back(key);
    }
    return keys;
}

std::string STEPReader::get_property(const std::string& key) const {
    auto it = properties_.find(key);
    if (it != properties_.end()) {
        return it->second;
    }
    return "";
}

}  // namespace bendcore::step
