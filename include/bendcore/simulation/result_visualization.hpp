#pragma once

#include <vector>
#include <array>
#include <string>
#include <memory>
#include "../geometry/vector.hpp"

namespace BendCore::CadCore::Simulation {

/**
 * @brief Visualization field type
 */
enum class FieldType {
    SCALAR,     // Scalar field (temperature, pressure, etc.)
    VECTOR,     // Vector field (velocity, force, etc.)
    TENSOR,     // Tensor field (stress, strain)
};

/**
 * @brief Color mapping strategy
 */
enum class ColorMapType {
    JET,           // Blue to red
    VIRIDIS,       // Perceptually uniform
    HOT,           // Black to white through red
    COOL_WARM,     // Cool to warm
    GRAYSCALE,     // Grayscale
    RAINBOW,       // Full rainbow spectrum
};

/**
 * @brief RGB color
 */
struct Color {
    double r = 0.0;  // 0-1
    double g = 0.0;  // 0-1
    double b = 0.0;  // 0-1

    Color() = default;
    Color(double r, double g, double b) : r(r), g(g), b(b) {}
};

/**
 * @brief Contour level for iso-surface generation
 */
struct ContourLevel {
    double value = 0.0;
    Color color;
    double opacity = 1.0;
};

/**
 * @brief Iso-surface representation
 */
struct IsoSurface {
    double iso_value = 0.0;
    std::vector<Vector3D> vertices;
    std::vector<std::array<size_t, 3>> triangles;  // Triangle indices
    Color color;
};

/**
 * @brief Vector field glyph (arrow)
 */
struct VectorGlyph {
    Vector3D position;
    Vector3D direction;
    double magnitude = 0.0;
    Color color;
};

/**
 * @brief Scalar field coloring data
 */
struct ScalarFieldColor {
    size_t node_id;
    Vector3D position;
    double value = 0.0;
    Color color;
};

/**
 * @brief Color map for scalar field visualization
 */
class ColorMap {
public:
    explicit ColorMap(ColorMapType type = ColorMapType::JET);

    [[nodiscard]] Color getColor(double normalized_value) const;
    void setRange(double min_val, double max_val);
    [[nodiscard]] double getMin() const { return min_value_; }
    [[nodiscard]] double getMax() const { return max_value_; }

private:
    ColorMapType type_;
    double min_value_ = 0.0;
    double max_value_ = 1.0;

    [[nodiscard]] Color interpolateColor(double t) const;
    [[nodiscard]] Color jetColor(double t) const;
    [[nodiscard]] Color viridisColor(double t) const;
    [[nodiscard]] Color hotColor(double t) const;
};

/**
 * @brief Field data for visualization
 */
class VisualizationField {
public:
    VisualizationField(const std::string& name, FieldType type);

    void setScalarData(const std::vector<double>& data);
    void setVectorData(const std::vector<Vector3D>& data);
    void setNodePositions(const std::vector<Vector3D>& positions);

    [[nodiscard]] const std::string& getName() const { return name_; }
    [[nodiscard]] FieldType getType() const { return type_; }
    [[nodiscard]] const std::vector<double>& getScalarData() const { return scalar_data_; }
    [[nodiscard]] const std::vector<Vector3D>& getVectorData() const { return vector_data_; }
    [[nodiscard]] const std::vector<Vector3D>& getNodePositions() const { return node_positions_; }
    [[nodiscard]] size_t getNumPoints() const { return node_positions_.size(); }

private:
    std::string name_;
    FieldType type_;
    std::vector<double> scalar_data_;
    std::vector<Vector3D> vector_data_;
    std::vector<Vector3D> node_positions_;
};

/**
 * @brief Result Visualization system
 *
 * Generates visualization data for:
 * - Scalar field colormapping
 * - Vector field glyphs
 * - Iso-surface extraction
 */
class ResultVisualization {
public:
    ResultVisualization();
    ~ResultVisualization() = default;

    // Field management
    void addField(const std::shared_ptr<VisualizationField>& field);
    [[nodiscard]] std::shared_ptr<VisualizationField> getField(const std::string& name) const;
    [[nodiscard]] const std::vector<std::shared_ptr<VisualizationField>>& getFields() const { return fields_; }

    // Scalar field visualization
    void generateScalarFieldColors(const std::string& field_name, ColorMapType colormap = ColorMapType::JET);
    [[nodiscard]] const std::vector<ScalarFieldColor>& getScalarFieldColors() const { return scalar_colors_; }

    // Iso-surface generation
    void generateIsoSurface(const std::string& field_name, double iso_value);
    [[nodiscard]] const std::vector<IsoSurface>& getIsoSurfaces() const { return iso_surfaces_; }

    // Vector field visualization
    void generateVectorGlyphs(const std::string& field_name, size_t skip = 1, double scale = 1.0);
    [[nodiscard]] const std::vector<VectorGlyph>& getVectorGlyphs() const { return vector_glyphs_; }

    // Color map
    void setColorMap(const ColorMap& colormap);
    [[nodiscard]] const ColorMap& getColorMap() const { return colormap_; }

    // Contour levels
    void addContourLevel(const ContourLevel& level);
    [[nodiscard]] const std::vector<ContourLevel>& getContourLevels() const { return contour_levels_; }

    // Export visualization data
    bool exportToVTK(const std::string& filename);
    bool exportColormap(const std::string& filename);

private:
    std::vector<std::shared_ptr<VisualizationField>> fields_;
    ColorMap colormap_;
    std::vector<ScalarFieldColor> scalar_colors_;
    std::vector<IsoSurface> iso_surfaces_;
    std::vector<VectorGlyph> vector_glyphs_;
    std::vector<ContourLevel> contour_levels_;

    // Internal methods
    void extractIsoSurfaceTriangles(const std::string& field_name, double iso_value, IsoSurface& surface);
    void computeContourColors(const std::shared_ptr<VisualizationField>& field);
};

}  // namespace BendCore::CadCore::Simulation
