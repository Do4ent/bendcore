// Mesh Quality Analysis System
// M7 Phase: Mesh Optimization (GAP-028)
#pragma once

#include "bendcore/fea/mesh_generator.hpp"
#include <vector>
#include <array>
#include <cstdint>
#include <limits>

namespace bendcore::mesh_optimization {

// Quality metrics for individual elements
struct ElementQualityMetrics {
    int32_t element_id;
    double aspect_ratio;           // Edge length ratio max/min
    double skewness;               // 0 (perfect) to 1 (degenerate)
    double distortion;             // Shape distortion metric
    double jacobian_determinant;   // Jacobian at element center
    double min_jacobian;           // Minimum Jacobian in element
    double max_jacobian;           // Maximum Jacobian in element
    std::vector<double> dihedral_angles;  // Angles between faces
    double min_dihedral_angle;
    double max_dihedral_angle;
    bool is_inverted;              // Negative Jacobian indicates inversion
};

// Aggregate quality statistics for entire mesh
struct MeshQualityStatistics {
    int32_t total_elements;
    int32_t total_nodes;

    // Aspect ratio statistics
    double aspect_ratio_min;
    double aspect_ratio_max;
    double aspect_ratio_mean;
    double aspect_ratio_median;

    // Skewness statistics
    double skewness_min;
    double skewness_max;
    double skewness_mean;
    double skewness_median;

    // Jacobian statistics
    double jacobian_min;
    double jacobian_max;
    double jacobian_mean;
    double jacobian_median;

    // Dihedral angle statistics
    double dihedral_angle_min;
    double dihedral_angle_max;
    double dihedral_angle_mean;

    // Element validity
    int32_t num_inverted_elements;
    int32_t num_highly_skewed_elements;  // skewness > 0.8
    int32_t num_poor_aspect_ratio;      // aspect_ratio > 10

    // Overall quality score (0-1, 1=perfect)
    double overall_quality_score;
};

// Element connectivity validation result
struct ConnectivityValidation {
    bool is_valid;
    std::vector<int32_t> duplicate_elements;
    std::vector<int32_t> orphaned_nodes;
    std::vector<int32_t> non_manifold_nodes;  // Nodes with improper connectivity
    std::vector<std::pair<int32_t, int32_t>> conflicting_elements;  // Overlapping elements
};

// Quality analyzer for mesh assessment
class QualityAnalyzer {
public:
    QualityAnalyzer() = default;
    ~QualityAnalyzer() = default;

    // Compute quality metrics for a single element
    ElementQualityMetrics analyze_element(
        const fea::FiniteElementMesh& mesh,
        const fea::Element& element);

    // Compute quality for all elements
    std::vector<ElementQualityMetrics> analyze_all_elements(
        const fea::FiniteElementMesh& mesh);

    // Compute aggregate statistics
    MeshQualityStatistics compute_statistics(
        const std::vector<ElementQualityMetrics>& metrics);

    // Validate element connectivity
    ConnectivityValidation validate_connectivity(
        const fea::FiniteElementMesh& mesh);

    // Check if mesh is valid for FEA analysis
    bool is_mesh_valid_for_analysis(
        const fea::FiniteElementMesh& mesh,
        double min_jacobian_threshold = 0.1,
        double max_aspect_ratio_threshold = 100.0);

    // Compute aspect ratio for element
    static double compute_aspect_ratio(
        const fea::FiniteElementMesh& mesh,
        const fea::Element& element);

    // Compute skewness (0=perfect, 1=degenerate)
    static double compute_skewness(
        const fea::FiniteElementMesh& mesh,
        const fea::Element& element);

    // Compute distortion metric
    static double compute_distortion(
        const fea::FiniteElementMesh& mesh,
        const fea::Element& element);

    // Compute Jacobian determinant at point
    static double compute_jacobian_determinant(
        const fea::FiniteElementMesh& mesh,
        const fea::Element& element,
        double xi, double eta, double zeta);

    // Compute dihedral angles for tetrahedron
    static std::vector<double> compute_dihedral_angles(
        const fea::FiniteElementMesh& mesh,
        const fea::Element& element);

    // Get element edge lengths
    static std::array<double, 3> get_edge_length_statistics(
        const fea::FiniteElementMesh& mesh,
        const fea::Element& element);

private:
    // Helper methods for specific element types
    double compute_tet_aspect_ratio(
        const fea::FiniteElementMesh& mesh,
        const fea::Element& element);

    double compute_hex_aspect_ratio(
        const fea::FiniteElementMesh& mesh,
        const fea::Element& element);

    double compute_tet_skewness(
        const fea::FiniteElementMesh& mesh,
        const fea::Element& element);

    double compute_hex_skewness(
        const fea::FiniteElementMesh& mesh,
        const fea::Element& element);
};

}  // namespace bendcore::mesh_optimization
