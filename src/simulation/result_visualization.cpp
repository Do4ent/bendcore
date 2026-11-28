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
    // matplotlib "viridis" (van der Walt, Smith, 2015): entries 0, 32, ..., 224 and 255 of
    // the 256-entry table in matplotlib/_cm_listed.py, linear interpolation in between.
    struct Sample { double t, r, g, b; };
    static constexpr Sample kSamples[] = {
        {0.0 / 255.0, 0.267004, 0.004874, 0.329415},   {32.0 / 255.0, 0.282623, 0.140926, 0.457517},
        {64.0 / 255.0, 0.229739, 0.322361, 0.545706},  {96.0 / 255.0, 0.163625, 0.471133, 0.558148},
        {128.0 / 255.0, 0.127568, 0.566949, 0.550556}, {160.0 / 255.0, 0.134692, 0.658636, 0.517649},
        {192.0 / 255.0, 0.266941, 0.748751, 0.440573}, {224.0 / 255.0, 0.477504, 0.821444, 0.318195},
        {255.0 / 255.0, 0.993248, 0.906157, 0.143936}};
    constexpr size_t kCount = sizeof(kSamples) / sizeof(kSamples[0]);
    t = std::clamp(t, 0.0, 1.0);
    size_t k = 0;
    while (k + 2 < kCount && t > kSamples[k + 1].t) ++k;
    const Sample& a = kSamples[k];
    const Sample& b = kSamples[k + 1];
    const double f = (t - a.t) / (b.t - a.t);
    return Color(a.r + f * (b.r - a.r), a.g + f * (b.g - a.g), a.b + f * (b.b - a.b));
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
    if (fields_.empty()) {
        return false;
    }
    const auto& points = fields_.front()->getNodePositions();
    const size_t n = points.size();
    for (const auto& field : fields_) {
        const auto& pos = field->getNodePositions();
        if (pos.size() != n) return false;
        for (size_t i = 0; i < n; ++i) {
            if ((pos[i] - points[i]).magnitude() > 1e-12) return false;
        }
        const size_t values = field->getType() == FieldType::SCALAR ? field->getScalarData().size()
                                                                  : field->getVectorData().size();
        if (values != n) return false;
    }

    std::ofstream file(filename);
    if (!file.is_open()) {
        return false;
    }
    file << "# vtk DataFile Version 3.0\n";
    file << "Simulation Results\n";
    file << "ASCII\n";
    file << "DATASET UNSTRUCTURED_GRID\n";
    file << "POINTS " << n << " double\n";
    for (const auto& pt : points) {
        file << pt.x << " " << pt.y << " " << pt.z << "\n";
    }
    // One VTK_VERTEX (type 1) cell per node, so viewers draw the point cloud.
    file << "CELLS " << n << " " << 2 * n << "\n";
    for (size_t i = 0; i < n; ++i) file << "1 " << i << "\n";
    file << "CELL_TYPES " << n << "\n";
    for (size_t i = 0; i < n; ++i) file << "1\n";

    file << "POINT_DATA " << n << "\n";
    for (const auto& field : fields_) {
        if (field->getType() == FieldType::SCALAR) {
            file << "SCALARS " << field->getName() << " double 1\n";
            file << "LOOKUP_TABLE default\n";
            for (double val : field->getScalarData()) file << val << "\n";
        } else {
            file << "VECTORS " << field->getName() << " double\n";
            for (const auto& v : field->getVectorData()) file << v.x << " " << v.y << " " << v.z << "\n";
        }
    }
    return static_cast<bool>(file);
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

}  // namespace BendCore::CadCore::Simulation
