#pragma once

#include <vector>
#include <cmath>
#include <array>

namespace bendcore {
namespace topology_optimization {

/**
 * Post-Processing for Topology Optimization
 *
 * - Density thresholding for binary designs
 * - Geometry extraction from density field
 * - CAD model generation
 * - Manufacturability assessment
 */
class PostProcessor {
public:
    /**
     * Threshold type enumeration
     */
    enum class ThresholdingMethod {
        SIMPLE = 0,           // Hard threshold at value
        HEAVISIDE = 1,        // Smooth Heaviside function
        MORPHOLOGICAL = 2     // Morphological cleaning
    };

public:
    /**
     * Simple density thresholding
     * Returns binary field where x_binary = (x > threshold) ? 1 : 0
     *
     * @param densities Input density field
     * @param threshold Threshold value (typically 0.5)
     * @return Binary density field
     */
    static std::vector<double> apply_simple_threshold(
        const std::vector<double>& densities,
        double threshold = 0.5);

    /**
     * Smooth Heaviside thresholding
     * x_binary = (1 - exp(-beta * x) + exp(-beta)) / (1 - 2 * exp(-beta) + exp(-beta))
     * Smoother transition at threshold
     *
     * @param densities Input density field
     * @param threshold Threshold value
     * @param beta Heaviside parameter (larger = sharper)
     * @return Thresholded density field
     */
    static std::vector<double> apply_heaviside_threshold(
        const std::vector<double>& densities,
        double threshold = 0.5,
        double beta = 10.0);

    /**
     * Morphological cleanup
     * Removes small isolated features and fills small voids
     *
     * @param densities Input density field
     * @param element_positions Element center coordinates
     * @param min_feature_size Minimum feature size to keep
     * @param threshold Density threshold for clustering
     * @return Cleaned binary field
     */
    static std::vector<double> morphological_cleanup(
        const std::vector<double>& densities,
        const std::vector<std::array<double, 3>>& element_positions,
        double min_feature_size = 1.0,
        double threshold = 0.5);

public:
    /**
     * Element geometry information
     */
    struct Element {
        size_t id;
        std::array<double, 3> center;      // Element center
        std::array<double, 3> dimensions;  // Element dimensions
        double density = 0.0;               // Density value
        bool material = false;              // True if solid
    };

    /**
     * Extracted geometry information
     */
    struct ExtractedGeometry {
        std::vector<std::array<double, 3>> vertices;
        std::vector<std::array<size_t, 3>> triangles;
        std::vector<std::array<size_t, 4>> tetrahedra;
        double volume = 0.0;
        double surface_area = 0.0;
    };

    struct Parameters {
        ThresholdingMethod thresholding = ThresholdingMethod::SIMPLE;
        double threshold_value = 0.5;
        double min_feature_size = 1.0;
        bool remove_isolated_features = true;
        bool fill_small_voids = true;
        bool smooth_boundaries = true;
    };

    explicit PostProcessor(const Parameters& params = Parameters());

    /**
     * Extract geometry from density field
     * Builds mesh representation of solid domain
     *
     * @param densities Optimized density field
     * @param elements Element definitions
     * @return Extracted geometry
     */
    ExtractedGeometry extract_geometry(
        const std::vector<double>& densities,
        const std::vector<Element>& elements);

    /**
     * Check manufacturability of design
     * Evaluates feature sizes, angles, overhangs
     *
     * @param densities Optimized density field
     * @param element_positions Element centers
     * @param min_feature_size Minimum feature size
     * @param max_overhang_angle Maximum overhang angle (degrees)
     * @return Manufacturability score (0-1, where 1 is fully manufacturable)
     */
    double check_manufacturability(
        const std::vector<double>& densities,
        const std::vector<std::array<double, 3>>& element_positions,
        double min_feature_size = 1.0,
        double max_overhang_angle = 45.0);

    /**
     * Smooth boundaries of design
     * Applies smoothing filter to final geometry
     *
     * @param densities Input densities
     * @param element_positions Element positions
     * @param smoothing_radius Smoothing kernel radius
     * @return Smoothed density field
     */
    std::vector<double> smooth_boundaries(
        const std::vector<double>& densities,
        const std::vector<std::array<double, 3>>& element_positions,
        double smoothing_radius = 1.5);

    /**
     * Export to STL format (ASCII)
     * @param geometry Extracted geometry
     * @param filename Output filename
     * @return True if successful
     */
    static bool export_stl(const ExtractedGeometry& geometry,
                          const std::string& filename);

    /**
     * Export to STL format (binary)
     * @param geometry Extracted geometry
     * @param filename Output filename
     * @return True if successful
     */
    static bool export_stl_binary(const ExtractedGeometry& geometry,
                                  const std::string& filename);

private:
    Parameters parameters_;

    /**
     * Marching cubes algorithm for geometry extraction
     */
    ExtractedGeometry marching_cubes(
        const std::vector<double>& densities,
        const std::vector<Element>& elements);

    /**
     * Compute surface normals for geometry
     */
    std::vector<std::array<double, 3>> compute_normals(
        const std::vector<std::array<double, 3>>& vertices,
        const std::vector<std::array<size_t, 3>>& triangles);
};

} // namespace topology_optimization
} // namespace bendcore
