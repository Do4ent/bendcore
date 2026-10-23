// STEP File Reader (Q3 2027, Task 1, Subtask 2)
// Reads STEP files and extracts solid geometry using OCCT
#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <optional>

#if defined(_WIN32) || defined(_WIN64)
  #include <TopoDS_Shape.hxx>
  #include <TDocStd_Document.hxx>
  #include <STEPCAFControl_Reader.hxx>
#endif

namespace bendcore::step {

// Result of STEP file import
struct ImportResult {
    bool success = false;
    std::vector<std::string> shape_ids;      // IDs of extracted shapes
    std::map<std::string, std::string> properties;  // File metadata (name, description, etc.)
    std::string error_message;
    int warning_count = 0;
    int shape_count = 0;
};

// STEP File Reader class
class STEPReader {
public:
    STEPReader();
    ~STEPReader();

    // Read STEP file and extract shapes
    // Returns ImportResult with success status and extracted shapes
    ImportResult read(const std::string& file_path);

    // Get extracted shape by ID (for geometry analysis)
    // Returns empty string if shape not found
    std::string get_shape_data(const std::string& shape_id) const;

    // Get all property keys
    std::vector<std::string> get_property_keys() const;

    // Get property value by key
    std::string get_property(const std::string& key) const;

    // Check if file is a valid STEP/IGES file
    static bool is_supported_format(const std::string& file_path);

private:
    #if defined(_WIN32) || defined(_WIN64)
      Handle(TDocStd_Document) doc_;
      STEPCAFControl_Reader reader_;
      std::map<std::string, std::string> shapes_;  // shape_id -> serialized shape data
    #endif

    std::map<std::string, std::string> properties_;
    int warning_count_ = 0;

    // Extract all shapes from document
    void extract_shapes(const std::string& file_path);

    // Extract metadata from document
    void extract_metadata(const std::string& file_path);

    // Get file extension
    static std::string get_file_extension(const std::string& file_path);

    // Convert UTF-8 path to Windows wide char
    static std::wstring utf8_to_wide(const std::string& utf8_str);
};

}  // namespace bendcore::step
