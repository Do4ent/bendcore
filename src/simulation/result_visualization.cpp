#include "bendcore/simulation/result_visualization.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>

namespace BendCore::CadCore::Simulation {

// ==================== ColorMap ====================

ColorMap::ColorMap(ColorMapType type)
    : type_(type), min_value_(0.0), max_value_(1.0) {}

void ColorMap::setRange(double min_val, double max_val) {
    min_value_ = min_val;
    max_value_ = max_val;
}

Color ColorMap::getColor(double normalized_value) const {
    // Clamp to [0, 1]
    double t = std::max(0.0, std::min(1.0, normalized_value));
    return interpolateColor(t);
}

Color ColorMap::interpolateColor(double t) const {
    switch (type_) {
        case ColorMapType::JET:
            return jetColor(t);
        case ColorMapType::VIRIDIS:
            return viridisColor(t);
        case ColorMapType::HOT:
            return hotColor(t);
        case ColorMapType::COOL_WARM:
            return Color(t, 0.5, 1.0 - t);
        case ColorMapType::GRAYSCALE:
            return Color(t, t, t);
        case ColorMapType::RAINBOW:
            if (t < 0.2) return Color(0, 0, 1);  // Blue
            if (t < 0.4) return Color(0, 1, 1);  // Cyan
            if (t < 0.6) return Color(0, 1, 0);  // Green
            if (t < 0.8) return Color(1, 1, 0);  // Yellow
            return Color(1, 0, 0);  // Red
        default:
            return Color(t, t, t);
    }
}

Color ColorMap::jetColor(double t) const {
    // Blue -> Cyan -> Green -> Yellow -> Red
    double r = 0.0, g = 0.0, b = 0.0;

    if (t < 0.125) {
        b = 1.0;
        g = t * 8.0;
    } else if (t < 0.375) {
        b = 1.0 - (t - 0.125) * 4.0;
        g = 1.0;
    } else if (t < 0.625) {
        g = 1.0;
        r = (t - 0.375) * 4.0;
    } else if (t < 0.875) {
        g = 1.0 - (t - 0.625) * 4.0;
        r = 1.0;
    } else {
        r = 1.0;
    }

    return Color(r, g, b);
}

Color ColorMap::viridisColor(double t) const {
    // Perceptually uniform colormap
    // Simplified implementation
    if (t < 0.25) {
        return Color(0.267, 0.004, 0.329);
    } else if (t < 0.5) {
        return Color(0.128, 0.565, 0.510);
    } else if (t < 0.75) {
        return Color(0.993, 0.906, 0.144);
    } else {
        return Color(0.993, 0.906, 0.144);
    }
}

Color ColorMap::hotColor(double t) const {
    // Black -> Red -> Yellow -> White
    if (t < 0.33) {
        return Color(t * 3.0, 0, 0);
    } else if (t < 0.67) {
        return Color(1.0, (t - 0.33) * 3.0, 0);
    } else {
        return Color(1.0, 1.0, (t - 0.67) * 3.0);
    }
}

// ==================== VisualizationField ====================

VisualizationField::VisualizationField(const std::string& name, FieldType type)
    : name_(name), type_(type) {}

void VisualizationField::setScalarData(const std::vector<double>& data) {
    scalar_data_ = data;
}

void VisualizationField::setVectorData(const std::vector<Vector3D>& data) {
    vector_data_ = data;
}

void VisualizationField::setNodePositions(const std::vector<Vector3D>& positions) {
    node_positions_ = positions;
}

// ==================== ResultVisualization ====================

ResultVisualization::ResultVisualization()
    : colormap_(ColorMapType::JET) {}

void ResultVisualization::addField(const std::shared_ptr<VisualizationField>& field) {
    fields_.push_back(field);
}

std::shared_ptr<VisualizationField> ResultVisualization::getField(const std::string& name) const {
    for (const auto& field : fields_) {
        if (field->getName() == name) {
            return field;
        }
    }
    return nullptr;
}

void ResultVisualization::generateScalarFieldColors(const std::string& field_name, ColorMapType colormap_type) {
    auto field = getField(field_name);
    if (!field || field->getType() != FieldType::SCALAR) {
        return;
    }

    scalar_colors_.clear();

    const auto& scalar_data = field->getScalarData();
    const auto& positions = field->getNodePositions();

    if (scalar_data.empty() || positions.empty()) {
        return;
    }

    // Find min and max values
    double min_val = *std::min_element(scalar_data.begin(), scalar_data.end());
    double max_val = *std::max_element(scalar_data.begin(), scalar_data.end());

    ColorMap colormap(colormap_type);
    colormap.setRange(min_val, max_val);

    // Generate colors for each point
    for (size_t i = 0; i < scalar_data.size() && i < positions.size(); i++) {
        double normalized = 0.0;
        if (max_val > min_val) {
            normalized = (scalar_data[i] - min_val) / (max_val - min_val);
        }

        ScalarFieldColor color_data;
        color_data.node_id = i;
        color_data.position = positions[i];
        color_data.value = scalar_data[i];
        color_data.color = colormap.getColor(normalized);

        scalar_colors_.push_back(color_data);
    }
}

void ResultVisualization::generateIsoSurface(const std::string& field_name, double iso_value) {
    auto field = getField(field_name);
    if (!field || field->getType() != FieldType::SCALAR) {
        return;
    }

    IsoSurface surface;
    surface.iso_value = iso_value;

    // Simplified iso-surface extraction
    extractIsoSurfaceTriangles(field_name, iso_value, surface);

    // Color the iso-surface
    ColorMap colormap(colormap_);
    surface.color = colormap.getColor(iso_value);

    iso_surfaces_.push_back(surface);
}

void ResultVisualization::generateVectorGlyphs(const std::string& field_name, size_t skip, double scale) {
    auto field = getField(field_name);
    if (!field || field->getType() != FieldType::VECTOR) {
        return;
    }

    vector_glyphs_.clear();

    const auto& vector_data = field->getVectorData();
    const auto& positions = field->getNodePositions();

    // Generate glyphs at selected points
    for (size_t i = 0; i < vector_data.size() && i < positions.size(); i += skip) {
        VectorGlyph glyph;
        glyph.position = positions[i];
        glyph.direction = vector_data[i];
        glyph.magnitude = glyph.direction.magnitude();

        if (glyph.magnitude > 1e-10) {
            glyph.direction = glyph.direction * (scale / glyph.magnitude);
        }

        // Color based on magnitude
        double normalized_mag = std::min(1.0, glyph.magnitude / 10.0);
        ColorMap colormap(colormap_);
        glyph.color = colormap.getColor(normalized_mag);

        vector_glyphs_.push_back(glyph);
    }
}

void ResultVisualization::setColorMap(const ColorMap& colormap) {
    colormap_ = colormap;
}

void ResultVisualization::addContourLevel(const ContourLevel& level) {
    contour_levels_.push_back(level);
}

bool ResultVisualization::exportToVTK(const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        return false;
    }

    file << "# vtk DataFile Version 3.0\n";
    file << "Simulation Results\n";
    file << "ASCII\n";
    file << "DATASET UNSTRUCTURED_GRID\n";

    // Export points from all fields
    std::vector<Vector3D> all_points;
    for (const auto& field : fields_) {
        const auto& positions = field->getNodePositions();
        all_points.insert(all_points.end(), positions.begin(), positions.end());
    }

    file << "POINTS " << all_points.size() << " float\n";
    for (const auto& pt : all_points) {
        file << pt.x << " " << pt.y << " " << pt.z << "\n";
    }

    // Empty cells for now
    file << "CELLS 0 0\n";
    file << "CELL_TYPES 0\n";

    // Write point data
    file << "POINT_DATA " << all_points.size() << "\n";

    // Export scalar fields
    for (const auto& field : fields_) {
        if (field->getType() == FieldType::SCALAR && !field->getScalarData().empty()) {
            file << "SCALARS " << field->getName() << " float\n";
            file << "LOOKUP_TABLE default\n";
            for (double val : field->getScalarData()) {
                file << val << "\n";
            }
        }
    }

    file.close();
    return true;
}

bool ResultVisualization::exportColormap(const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        return false;
    }

    file << "Colormap Export\n";
    file << "Min: " << colormap_.getMin() << ", Max: " << colormap_.getMax() << "\n";

    // Export color samples
    for (double t = 0.0; t <= 1.0; t += 0.1) {
        Color c = colormap_.getColor(t);
        file << "Value " << t << ": RGB(" << c.r << ", " << c.g << ", " << c.b << ")\n";
    }

    file.close();
    return true;
}

void ResultVisualization::extractIsoSurfaceTriangles(const std::string& field_name, double iso_value, IsoSurface& surface) {
    // Simplified marching cubes-like algorithm
    // In a real implementation, this would use proper marching cubes

    auto field = getField(field_name);
    if (!field) {
        return;
    }

    const auto& scalar_data = field->getScalarData();
    const auto& positions = field->getNodePositions();

    // Create vertices at iso-value crossings
    for (size_t i = 0; i < scalar_data.size(); i++) {
        if (std::abs(scalar_data[i] - iso_value) < 0.01) {
            surface.vertices.push_back(positions[i]);
        }
    }

    // Create simple triangulation
    for (size_t i = 0; i + 2 < surface.vertices.size(); i += 3) {
        surface.triangles.push_back({i, i + 1, i + 2});
    }
}

void ResultVisualization::computeContourColors(const std::shared_ptr<VisualizationField>& field) {
    if (!field || field->getType() != FieldType::SCALAR) {
        return;
    }

    const auto& scalar_data = field->getScalarData();

    for (const auto& level : contour_levels_) {
        // Find points near this contour level
        for (size_t i = 0; i < scalar_data.size(); i++) {
            if (std::abs(scalar_data[i] - level.value) < 0.1) {
                // Point is near this contour level
                // (would update visualization accordingly)
            }
        }
    }
}

}  // namespace BendCore::CadCore::Simulation
