// STEP File Reader (Q3 2027, Task 1, Subtask 2)
// Reads STEP files and extracts solid geometry using OCCT
#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <optional>


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

    // Get extracted shape by ID: OCCT B-Rep text (BRepTools::Write format),
    // readable with BRepTools::Read. Returns empty string if not found.
    std::string get_shape_data(const std::string& shape_id) const;

    // Get all property keys
    std::vector<std::string> get_property_keys() const;

    // Get property value by key
    std::string get_property(const std::string& key) const;

    // Check if file is a valid STEP/IGES file
    static bool is_supported_format(const std::string& file_path);

private:
    std::map<std::string, std::string> shapes_;  // shape_id -> B-Rep text
    std::map<std::string, std::string> properties_;
    int warning_count_ = 0;

    // Get file extension (lower case, without the dot)
    static std::string get_file_extension(const std::string& file_path);
};

}  // namespace bendcore::step
