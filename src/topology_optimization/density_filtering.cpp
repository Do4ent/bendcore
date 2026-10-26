#include "bendcore/topology_optimization/density_filtering.hpp"
#include <cmath>
#include <algorithm>
#include <numeric>

namespace bendcore {
namespace topology_optimization {

std::vector<double> DensityFilter::apply_linear_filter(
    const std::vector<double>& densities,
    double radius,
    const std::vector<std::array<double, 3>>& element_positions) {

    size_t n = densities.size();
    std::vector<double> filtered(n, 0.0);
    std::vector<double> weights(n, 0.0);

    for (size_t i = 0; i < n; ++i) {
        for (size_t j = 0; j < n; ++j) {
            // Compute Euclidean distance
            double dist = 0.0;
            if (i < element_positions.size() && j < element_positions.size()) {
                for (int k = 0; k < 3; ++k) {
                    double diff = element_positions[i][k] - element_positions[j][k];
                    dist += diff * diff;
                }
                dist = std::sqrt(dist);
            }

            // Linear weight function
            double w = std::max(0.0, radius - dist);
            filtered[i] += w * densities[j];
            weights[i] += w;
        }

        if (weights[i] > 1e-12) {
            filtered[i] /= weights[i];
        } else {
            filtered[i] = densities[i];
        }
    }

    return filtered;
}

std::vector<double> DensityFilter::apply_helmholtz_filter(
    const std::vector<double>& densities,
    double radius,
    const std::vector<std::array<double, 3>>& element_positions) {

    // Simplified Helmholtz filter using iterative approach
    std::vector<double> filtered = densities;
    double alpha = radius * radius;

    // Iterate to solve Helmholtz equation
    for (int iter = 0; iter < 3; ++iter) {
        std::vector<double> temp = filtered;

        for (size_t i = 0; i < filtered.size(); ++i) {
            double laplacian = 0.0;
            int count = 0;

            // Approximate Laplacian using neighbors
            for (size_t j = 0; j < filtered.size(); ++j) {
                if (i == j) continue;

                double dist = 0.0;
                if (i < element_positions.size() && j < element_positions.size()) {
                    for (int k = 0; k < 3; ++k) {
                        double diff = element_positions[i][k] - element_positions[j][k];
                        dist += diff * diff;
                    }
                    dist = std::sqrt(dist);

                    if (dist < radius) {
                        double weight = std::exp(-dist * dist / alpha);
                        laplacian += weight * (filtered[j] - filtered[i]) / (dist + 1e-6);
                        count++;
                    }
                }
            }

            if (count > 0) {
                temp[i] = (densities[i] + alpha * laplacian) / (1.0 + alpha);
            }
        }

        filtered = temp;
    }

    return filtered;
}

std::vector<double> DensityFilter::apply_morphological_opening(
    const std::vector<double>& densities,
    double radius,
    const std::vector<std::array<double, 3>>& element_positions) {

    // Opening = dilation(erosion(x))
    auto eroded = apply_erosion(densities, radius, element_positions);
    return apply_dilation(eroded, radius, element_positions);
}

std::vector<double> DensityFilter::apply_morphological_closing(
    const std::vector<double>& densities,
    double radius,
    const std::vector<std::array<double, 3>>& element_positions) {

    // Closing = erosion(dilation(x))
    auto dilated = apply_dilation(densities, radius, element_positions);
    return apply_erosion(dilated, radius, element_positions);
}

DensityFilter::DensityFilter(const Parameters& params) : parameters_(params) {}

std::vector<double> DensityFilter::filter(
    const std::vector<double>& densities,
    const std::vector<std::array<double, 3>>& element_positions) {

    std::vector<double> filtered = densities;

    for (int iter = 0; iter < parameters_.iterations; ++iter) {
        switch (parameters_.filter_type) {
            case FilterType::LINEAR:
                filtered = apply_linear_filter(filtered, parameters_.filter_radius, element_positions);
                break;
            case FilterType::HELMHOLTZ:
                filtered = apply_helmholtz_filter(filtered, parameters_.filter_radius, element_positions);
                break;
            case FilterType::MORPHOLOGICAL:
                filtered = apply_morphological_closing(filtered, parameters_.filter_radius, element_positions);
                break;
        }
    }

    return filtered;
}

std::vector<double> DensityFilter::filter_sensitivities(
    const std::vector<double>& sensitivities,
    const std::vector<double>& densities,
    const std::vector<std::array<double, 3>>& element_positions) {

    if (!parameters_.apply_to_sensitivities) {
        return sensitivities;
    }

    // Chain rule: filtered sensitivity
    auto filtered = sensitivities;
    std::vector<double> density_weights(sensitivities.size(), 1.0);

    // Weight by density magnitude to emphasize regions with material
    for (size_t i = 0; i < sensitivities.size(); ++i) {
        density_weights[i] = densities[i] * (1.0 - densities[i]) + 0.1;  // Avoid zero weights
        filtered[i] *= density_weights[i];
    }

    return filtered;
}

std::vector<std::vector<std::pair<size_t, double>>> DensityFilter::compute_weights(
    const std::vector<std::array<double, 3>>& element_positions,
    double radius) const {

    size_t n = element_positions.size();
    std::vector<std::vector<std::pair<size_t, double>>> weights(n);

    for (size_t i = 0; i < n; ++i) {
        for (size_t j = 0; j < n; ++j) {
            double dist = 0.0;
            for (int k = 0; k < 3; ++k) {
                double diff = element_positions[i][k] - element_positions[j][k];
                dist += diff * diff;
            }
            dist = std::sqrt(dist);

            if (dist < radius) {
                double w = radius - dist;
                weights[i].push_back({j, w});
            }
        }
    }

    return weights;
}

std::vector<double> DensityFilter::apply_erosion(
    const std::vector<double>& densities,
    double radius,
    const std::vector<std::array<double, 3>>& element_positions) const {

    size_t n = densities.size();
    std::vector<double> eroded(n);

    for (size_t i = 0; i < n; ++i) {
        double min_density = 1.0;

        for (size_t j = 0; j < n; ++j) {
            double dist = 0.0;
            if (i < element_positions.size() && j < element_positions.size()) {
                for (int k = 0; k < 3; ++k) {
                    double diff = element_positions[i][k] - element_positions[j][k];
                    dist += diff * diff;
                }
                dist = std::sqrt(dist);

                if (dist < radius) {
                    min_density = std::min(min_density, densities[j]);
                }
            }
        }

        eroded[i] = min_density;
    }

    return eroded;
}

std::vector<double> DensityFilter::apply_dilation(
    const std::vector<double>& densities,
    double radius,
    const std::vector<std::array<double, 3>>& element_positions) const {

    size_t n = densities.size();
    std::vector<double> dilated(n);

    for (size_t i = 0; i < n; ++i) {
        double max_density = 0.0;

        for (size_t j = 0; j < n; ++j) {
            double dist = 0.0;
            if (i < element_positions.size() && j < element_positions.size()) {
                for (int k = 0; k < 3; ++k) {
                    double diff = element_positions[i][k] - element_positions[j][k];
                    dist += diff * diff;
                }
                dist = std::sqrt(dist);

                if (dist < radius) {
                    max_density = std::max(max_density, densities[j]);
                }
            }
        }

        dilated[i] = max_density;
    }

    return dilated;
}

} // namespace topology_optimization
} // namespace bendcore
