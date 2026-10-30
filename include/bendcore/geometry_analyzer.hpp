// Geometric Parameter Extraction (Q3 2027, Task 1, Subtask 3)
// Extracts dimensions (OD, wall thickness, length, bends) from CAD geometry
#pragma once

#include <vector>
#include <string>
#include <cmath>
#include <functional>

namespace bendcore::geometry {

// Признаки трубы, извлечённые из B-rep (заполняет геометрический бэкенд, например OCCT).
struct BRepTubeFeatures {
    bool ok = false;
    std::string error;                              // причина, если ok == false
    double bbox_size[3] = {0, 0, 0};                // габариты по X, Y, Z (мм)
    double outer_radius = 0.0;                      // наибольший радиус цилиндрических/тороидальных граней
    double inner_radius = 0.0;                      // 0 — внутренней поверхности нет (пруток)
    std::vector<double> straight_lengths;           // длины прямых участков по оси (мм)
    std::vector<std::pair<double, double>> bends;   // (угол, °; радиус гиба по оси, мм)
};

// Бэкенд: B-rep в текстовом формате BRepTools (то, что пишет BRepTools::Write) -> признаки трубы.
using GeometryBackend = std::function<BRepTubeFeatures(const std::string& brep_text)>;

// Установить бэкенд для всех GeometryAnalyzer (nullptr — снять). OCCT-сборка ставит его через
// bendcore::occt::install_geometry_backend(). Без бэкенда analyze() возвращает confidence = 0.
void set_geometry_backend(GeometryBackend backend);
bool has_geometry_backend();

// Extracted parameters from geometric analysis
struct ExtractedParams {
    double outer_diameter = 0.0;      // OD (outer diameter) in mm
    double inner_diameter = 0.0;      // ID = OD - 2*wall
    double wall_thickness = 0.0;      // Wall thickness in mm
    double total_length = 0.0;        // Total length in mm
    std::vector<double> bend_angles;  // Bend angles in degrees
    std::vector<double> bend_radii;   // Bend radii in mm
    double bend_arc_length = 0.0;     // Total arc length of bends in mm
    std::string shape_type;           // "TUBE", "PIPE", "BEND", "COMPLEX"
    double confidence = 0.0;          // 0.0-1.0 confidence level
    std::string analysis_notes;       // Additional analysis information

    // Validate extracted parameters
    bool is_valid() const {
        return outer_diameter > 0.001 && wall_thickness >= 0.001 &&
               total_length > 0.1 && confidence >= 0.5;
    }
};

// Tube axis result from PCA
struct TubeAxisResult {
    std::vector<double> axis_direction;  // Normalized 3D direction vector
    std::vector<double> axis_origin;     // Origin point of axis (3D)
    double confidence = 0.0;              // 0.0-1.0 confidence
};

// Geometry analyzer class
class GeometryAnalyzer {
public:
    GeometryAnalyzer();
    ~GeometryAnalyzer();

    // Анализ формы трубы. geometry_data — B-rep в текстовом формате BRepTools.
    // Без геометрического бэкенда или при ошибке разбора: confidence = 0, причина в analysis_notes.
    ExtractedParams analyze(const std::string& geometry_data);

    // Перевод признаков B-rep в параметры трубы (не зависит от бэкенда; открыт для тестов).
    static ExtractedParams params_from_features(const BRepTubeFeatures& features);

    // Fit cylinder to geometry for OD/ID detection
    double fit_cylinder(const std::vector<std::pair<double, double>>& points);

    // Углы гибов (°) по B-rep; пусто, если бэкенда нет или гибов нет.
    std::vector<double> detect_bends(const std::string& geometry_data);

    // Calculate bend angles from curvature data
    std::vector<double> calculate_bend_angles(const std::vector<double>& curvature_points);

    // Find tube axis using PCA (Principal Component Analysis)
    // points: 3D points sampled from tube surface (shape: N x 3)
    TubeAxisResult find_tube_axis_pca(const std::vector<std::vector<double>>& points_3d);

    // Estimate confidence score based on analysis quality
    static double estimate_confidence(const ExtractedParams& params);

private:
    // Tolerance for dimension matching (in mm)
    static constexpr double TOLERANCE_MM = 0.01;

    // Minimum confidence to accept analysis results
    static constexpr double MIN_CONFIDENCE = 0.50;

    // Calculate curvature at points along geometry
    std::vector<double> calculate_curvature(const std::vector<std::pair<double, double>>& points);
};

}  // namespace bendcore::geometry
