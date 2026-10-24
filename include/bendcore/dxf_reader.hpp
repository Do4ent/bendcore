// DXF File Reader (Q3 2027, Task 1, Subtask 6)
// Reads DXF files and extracts 2D geometry elements
#pragma once

#include <string>
#include <vector>
#include <map>

namespace bendcore::dxf {

// 2D point structure
struct Point2D {
    double x = 0.0;
    double y = 0.0;
};

// DXF geometry elements
struct Line {
    Point2D start;
    Point2D end;
    int color = 256;
};

struct Circle {
    Point2D center;
    double radius = 0.0;
    int color = 256;
};

struct Polyline {
    std::vector<Point2D> vertices;
    bool closed = false;
    int color = 256;
};

struct Spline {
    std::vector<Point2D> control_points;
    int degree = 3;
    int color = 256;
};

// Dimension structure (BC-012: Drawing dimensions)
struct Dimension {
    Point2D start_point;          // First definition point
    Point2D end_point;            // Second definition point
    Point2D text_position;        // Text label position
    std::string text_value;       // Dimension text (e.g., "125.5")
    std::string dimension_type;   // "LINEAR", "RADIUS", "DIAMETER", "ANGULAR"
    double measured_value = 0.0;  // Parsed numeric value in mm
    int color = 256;
};

// DXF geometry collection
struct DXFGeometry {
    std::vector<Line> lines;
    std::vector<Circle> circles;
    std::vector<Polyline> polylines;
    std::vector<Spline> splines;
    std::vector<Dimension> dimensions;  // Dimensions extracted from drawing
    std::map<std::string, std::string> layers;
    std::map<std::string, std::string> properties;
    int entity_count = 0;
};

// DXF File Reader
class DXFReader {
public:
    DXFReader();
    ~DXFReader();

    // Read DXF file and extract geometry
    DXFGeometry read(const std::string& file_path);

    // Get entity count
    int get_entity_count() const;

    // Check if file is a valid DXF file
    static bool is_dxf_file(const std::string& file_path);

private:
    DXFGeometry geometry_;
    int entity_count_ = 0;

    // Extract lines from DXF
    void extract_lines(const std::string& file_content);

    // Extract circles from DXF
    void extract_circles(const std::string& file_content);

    // Extract polylines from DXF
    void extract_polylines(const std::string& file_content);

    // Extract splines from DXF
    void extract_splines(const std::string& file_content);

    // Extract dimensions from DXF (BC-012: Drawing dimensions)
    void extract_dimensions(const std::string& file_content);

    // Extract layer information
    void extract_layers(const std::string& file_content);

    // Parse DXF coordinate
    static double parse_coordinate(const std::string& value);

    // Parse dimension text value to numeric value
    static double parse_dimension_value(const std::string& text_value);
};

}  // namespace bendcore::dxf
