// DXF File Reader Implementation (Q3 2027, Task 1, Subtask 6)
#include <bendcore/dxf_reader.hpp>

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace bendcore::dxf {

DXFReader::DXFReader() : entity_count_(0) {
}

DXFReader::~DXFReader() {
}

bool DXFReader::is_dxf_file(const std::string& file_path) {
    // Check file extension
    size_t dot_pos = file_path.rfind('.');
    if (dot_pos == std::string::npos) {
        return false;
    }

    std::string ext = file_path.substr(dot_pos + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    return ext == "dxf";
}

double DXFReader::parse_coordinate(const std::string& value) {
    try {
        return std::stod(value);
    } catch (...) {
        return 0.0;
    }
}

DXFGeometry DXFReader::read(const std::string& file_path) {
    geometry_ = DXFGeometry();
    entity_count_ = 0;

    // Check if file is DXF
    if (!is_dxf_file(file_path)) {
        geometry_.properties["error"] = "Not a DXF file";
        return geometry_;
    }

    // Read file
    std::ifstream file(file_path);
    if (!file.is_open()) {
        geometry_.properties["error"] = "Failed to open file";
        return geometry_;
    }

    // Read entire file into string
    std::string file_content((std::istreambuf_iterator<char>(file)),
                             std::istreambuf_iterator<char>());
    file.close();

    // Validate DXF header
    if (file_content.find("DXF") == std::string::npos) {
        geometry_.properties["error"] = "Not a valid DXF format";
        return geometry_;
    }

    try {
        // Extract geometry elements
        extract_lines(file_content);
        extract_circles(file_content);
        extract_polylines(file_content);
        extract_splines(file_content);
        extract_dimensions(file_content);  // BC-012: Extract dimensions
        extract_layers(file_content);

        // Set entity count
        geometry_.entity_count = (int)(geometry_.lines.size() +
                                      geometry_.circles.size() +
                                      geometry_.polylines.size() +
                                      geometry_.splines.size() +
                                      geometry_.dimensions.size());

        geometry_.properties["file_path"] = file_path;
        geometry_.properties["entity_count"] = std::to_string(geometry_.entity_count);
        geometry_.properties["dimensions_found"] = std::to_string(geometry_.dimensions.size());

    } catch (const std::exception& e) {
        geometry_.properties["error"] = std::string("Parse error: ") + e.what();
    }

    return geometry_;
}

void DXFReader::extract_lines(const std::string& file_content) {
    // Simplified DXF line extraction
    // In production, would use a proper DXF parser

    size_t pos = 0;
    while ((pos = file_content.find("LINE", pos)) != std::string::npos) {
        // Simple placeholder
        Line line;
        line.start = {0.0, 0.0};
        line.end = {100.0, 0.0};
        geometry_.lines.push_back(line);

        pos += 4;
        if (geometry_.lines.size() >= 100) break;  // Limit
    }
}

void DXFReader::extract_circles(const std::string& file_content) {
    // Simplified DXF circle extraction
    size_t pos = 0;
    while ((pos = file_content.find("CIRCLE", pos)) != std::string::npos) {
        // Simple placeholder
        Circle circle;
        circle.center = {0.0, 0.0};
        circle.radius = 25.0;
        geometry_.circles.push_back(circle);

        pos += 6;
        if (geometry_.circles.size() >= 100) break;  // Limit
    }
}

void DXFReader::extract_polylines(const std::string& file_content) {
    // Simplified DXF polyline extraction
    size_t pos = 0;
    while ((pos = file_content.find("POLYLINE", pos)) != std::string::npos) {
        // Simple placeholder
        Polyline polyline;
        polyline.vertices.push_back({0.0, 0.0});
        polyline.vertices.push_back({100.0, 0.0});
        polyline.vertices.push_back({100.0, 100.0});
        geometry_.polylines.push_back(polyline);

        pos += 8;
        if (geometry_.polylines.size() >= 100) break;  // Limit
    }
}

void DXFReader::extract_splines(const std::string& file_content) {
    // Simplified DXF spline extraction
    size_t pos = 0;
    while ((pos = file_content.find("SPLINE", pos)) != std::string::npos) {
        // Simple placeholder
        Spline spline;
        spline.control_points.push_back({0.0, 0.0});
        spline.control_points.push_back({50.0, 50.0});
        spline.control_points.push_back({100.0, 0.0});
        geometry_.splines.push_back(spline);

        pos += 6;
        if (geometry_.splines.size() >= 100) break;  // Limit
    }
}

void DXFReader::extract_dimensions(const std::string& file_content) {
    // BC-012: Extract dimensions from DXF (DIMENSION entities)
    // DIMENSION in DXF format has coordinate and text information

    size_t pos = 0;
    while ((pos = file_content.find("DIMENSION", pos)) != std::string::npos) {
        Dimension dim;

        // Look for coordinate data following the DIMENSION keyword
        // In DXF: 10 = X, 20 = Y (start point), 11 = X, 21 = Y (end point)
        size_t block_end = file_content.find("ENDDIM", pos);
        if (block_end == std::string::npos) {
            block_end = file_content.find("  0", pos + 9);
        }
        if (block_end == std::string::npos) {
            block_end = pos + 200;  // Look ahead max 200 chars
        }

        std::string block = file_content.substr(pos, block_end - pos);

        // Extract dimension text value
        size_t text_pos = block.find("  1");
        if (text_pos != std::string::npos) {
            size_t newline = block.find('\n', text_pos);
            if (newline != std::string::npos) {
                dim.text_value = block.substr(text_pos + 3, newline - text_pos - 3);
                // Trim whitespace
                dim.text_value.erase(0, dim.text_value.find_first_not_of(" \n\r\t"));
                dim.text_value.erase(dim.text_value.find_last_not_of(" \n\r\t") + 1);

                // Parse numeric value from text
                dim.measured_value = parse_dimension_value(dim.text_value);
            }
        }

        // Determine dimension type
        if (block.find("DIAMETER") != std::string::npos ||
            dim.text_value.find("Ø") != std::string::npos ||
            dim.text_value.find("∅") != std::string::npos) {
            dim.dimension_type = "DIAMETER";
        } else if (block.find("RADIUS") != std::string::npos ||
                   dim.text_value.find("R") == 0) {
            dim.dimension_type = "RADIUS";
        } else if (block.find("ANGULAR") != std::string::npos) {
            dim.dimension_type = "ANGULAR";
        } else {
            dim.dimension_type = "LINEAR";
        }

        // Set placeholder coordinates (proper parsing requires full DXF parsing)
        dim.start_point = {0.0, 0.0};
        dim.end_point = {100.0, 0.0};
        dim.text_position = {50.0, -10.0};

        geometry_.dimensions.push_back(dim);

        pos += 9;
        if (geometry_.dimensions.size() >= 100) break;  // Limit
    }
}

double DXFReader::parse_dimension_value(const std::string& text_value) {
    // Parse numeric value from dimension text
    // Handles formats like "125.5", "Ø25", "R10", "90°", etc.

    std::string cleaned = text_value;

    // Remove common prefixes/symbols
    size_t start = 0;
    for (size_t i = 0; i < cleaned.length(); i++) {
        char c = cleaned[i];
        if (isdigit(c) || c == '.' || c == '-' || c == '+') {
            start = i;
            break;
        }
    }

    if (start >= cleaned.length()) return 0.0;

    // Extract digits and decimal point
    std::string num_str;
    for (size_t i = start; i < cleaned.length(); i++) {
        char c = cleaned[i];
        if (isdigit(c) || c == '.' || c == '-' || c == '+') {
            num_str += c;
        } else if (c == ',' && i + 1 < cleaned.length() && isdigit(cleaned[i + 1])) {
            // Handle comma as decimal separator (European format)
            num_str += '.';
        } else {
            break;
        }
    }

    try {
        return std::stod(num_str);
    } catch (...) {
        return 0.0;
    }
}

void DXFReader::extract_layers(const std::string& file_content) {
    // Extract layer information
    // Simplified version

    size_t pos = 0;
    int layer_count = 0;
    while ((pos = file_content.find("LAYER", pos)) != std::string::npos) {
        std::string layer_name = "Layer" + std::to_string(layer_count);
        geometry_.layers[layer_name] = "0";  // Default color

        pos += 5;
        layer_count++;
        if (layer_count >= 50) break;  // Limit
    }
}

int DXFReader::get_entity_count() const {
    return entity_count_;
}

}  // namespace bendcore::dxf
