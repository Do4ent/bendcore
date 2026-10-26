#pragma once

#include <vector>
#include <cmath>
#include <algorithm>

namespace bendcore {
namespace topology_optimization {

/**
 * Density Filtering for Topology Optimization
 *
 * Implements spatial filtering to prevent checkerboard patterns
 * and improve design manufacturability.
 */
class DensityFilter {
public:
    /**
     * Filter type enumeration
     */
    enum class FilterType {
        LINEAR = 0,           // Linear spatial filter
        HELMHOLTZ = 1,        // Helmholtz differential operator
        MORPHOLOGICAL = 2     // Morphological opening/closing
    };

public:
    /**
     * Linear spatial filtering
     * x_filtered[i] = sum(w[i,j] * x[j]) / sum(w[i,j])
     * where w[i,j] = max(0, R - dist(i,j))
     *
     * @param densities Input density field
     * @param radius Filter radius
     * @param element_positions Element center coordinates (x, y, z)
     * @return Filtered density field
     */
    static std::vector<double> apply_linear_filter(
        const std::vector<double>& densities,
        double radius,
        const std::vector<std::array<double, 3>>& element_positions);

    /**
     * Helmholtz filter using differential operators
     * Solves: (1 + L^2 * denom) * x_filt = x
     * where L is Laplacian operator
     *
     * @param densities Input density field
     * @param radius Filter radius (controls smoothing)
     * @param element_positions Element center coordinates
     * @return Filtered density field
     */
    static std::vector<double> apply_helmholtz_filter(
        const std::vector<double>& densities,
        double radius,
        const std::vector<std::array<double, 3>>& element_positions);

    /**
     * Morphological opening filter
     * Opening = dilation(erosion(x))
     * Removes small features below radius
     *
     * @param densities Input density field
     * @param radius Filter radius
     * @param element_positions Element center coordinates
     * @return Filtered density field
     */
    static std::vector<double> apply_morphological_opening(
        const std::vector<double>& densities,
        double radius,
        const std::vector<std::array<double, 3>>& element_positions);

    /**
     * Morphological closing filter
     * Closing = erosion(dilation(x))
     * Fills small voids below radius
     *
     * @param densities Input density field
     * @param radius Filter radius
     * @param element_positions Element center coordinates
     * @return Filtered density field
     */
    static std::vector<double> apply_morphological_closing(
        const std::vector<double>& densities,
        double radius,
        const std::vector<std::array<double, 3>>& element_positions);

public:
    struct Parameters {
        FilterType filter_type = FilterType::LINEAR;
        double filter_radius = 1.5;         // Spatial filter radius
        bool apply_to_sensitivities = true; // Filter sensitivities as well
        int iterations = 1;                 // Number of filter iterations
    };

    explicit DensityFilter(const Parameters& params = Parameters());

    /**
     * Apply density filter to field
     * @param densities Input density field
     * @param element_positions Element center coordinates
     * @return Filtered density field
     */
    std::vector<double> filter(const std::vector<double>& densities,
                               const std::vector<std::array<double, 3>>& element_positions);

    /**
     * Filter sensitivities to improve convergence
     * @param sensitivities Input sensitivities
     * @param densities Current densities (for filtering context)
     * @param element_positions Element center coordinates
     * @return Filtered sensitivities
     */
    std::vector<double> filter_sensitivities(
        const std::vector<double>& sensitivities,
        const std::vector<double>& densities,
        const std::vector<std::array<double, 3>>& element_positions);

    /**
     * Compute distance-based weight matrix
     * @param element_positions Element center coordinates
     * @param radius Filter radius
     * @return Weight matrix (sparse format as vector of pairs)
     */
    std::vector<std::vector<std::pair<size_t, double>>> compute_weights(
        const std::vector<std::array<double, 3>>& element_positions,
        double radius) const;

private:
    Parameters parameters_;

    /**
     * Apply morphological erosion
     */
    std::vector<double> apply_erosion(
        const std::vector<double>& densities,
        double radius,
        const std::vector<std::array<double, 3>>& element_positions) const;

    /**
     * Apply morphological dilation
     */
    std::vector<double> apply_dilation(
        const std::vector<double>& densities,
        double radius,
        const std::vector<std::array<double, 3>>& element_positions) const;
};

} // namespace topology_optimization
} // namespace bendcore
