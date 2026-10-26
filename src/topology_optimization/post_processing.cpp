#include "bendcore/topology_optimization/post_processing.hpp"
#include <fstream>
#include <cmath>
#include <algorithm>
#include <numeric>

namespace bendcore {
namespace topology_optimization {

std::vector<double> PostProcessor::apply_simple_threshold(
    const std::vector<double>& densities,
    double threshold) {

    std::vector<double> binary(densities.size());

    for (size_t i = 0; i < densities.size(); ++i) {
        binary[i] = (densities[i] > threshold) ? 1.0 : 0.0;
    }

    return binary;
}

std::vector<double> PostProcessor::apply_heaviside_threshold(
    const std::vector<double>& densities,
    double threshold,
    double beta) {

    std::vector<double> binary(densities.size());
    double denom = 1.0 - 2.0 * std::exp(-beta) + std::exp(-2.0 * beta);

    for (size_t i = 0; i < densities.size(); ++i) {
        double x = densities[i];
        double heaviside = (1.0 - std::exp(-beta * x) + std::exp(-beta)) / denom;
        binary[i] = std::max(0.0, std::min(1.0, heaviside));
    }

    return binary;
}

std::vector<double> PostProcessor::morphological_cleanup(
    const std::vector<double>& densities,
    const std::vector<std::array<double, 3>>& element_positions,
    double min_feature_size,
    double threshold) {

    // Threshold first
    auto binary = apply_simple_threshold(densities, threshold);

    // Remove isolated small features using connected component analysis
    std::vector<bool> visited(binary.size(), false);
    std::vector<double> cleaned = binary;

    for (size_t i = 0; i < binary.size(); ++i) {
        if (!visited[i] && binary[i] > 0.5) {
            std::vector<size_t> component;
            std::vector<size_t> queue;

            queue.push_back(i);
            visited[i] = true;

            while (!queue.empty()) {
                size_t current = queue.back();
                queue.pop_back();
                component.push_back(current);

                // Find neighbors within min_feature_size
                for (size_t j = 0; j < binary.size(); ++j) {
                    if (!visited[j] && binary[j] > 0.5) {
                        double dist = 0.0;
                        if (current < element_positions.size() && j < element_positions.size()) {
                            for (int k = 0; k < 3; ++k) {
                                double diff = element_positions[current][k] - element_positions[j][k];
                                dist += diff * diff;
                            }
                            dist = std::sqrt(dist);

                            if (dist < min_feature_size * 2.0) {
                                visited[j] = true;
                                queue.push_back(j);
                            }
                        }
                    }
                }
            }

            // Remove small components
            if (component.size() < 5) {
                for (size_t idx : component) {
                    cleaned[idx] = 0.0;
                }
            }
        }
    }

    return cleaned;
}

PostProcessor::PostProcessor(const Parameters& params)
    : parameters_(params) {}

PostProcessor::ExtractedGeometry PostProcessor::extract_geometry(
    const std::vector<double>& densities,
    const std::vector<Element>& elements) {

    ExtractedGeometry geometry;

    // Threshold the density field
    std::vector<double> binary;
    switch (parameters_.thresholding) {
        case ThresholdingMethod::SIMPLE:
            binary = apply_simple_threshold(densities, parameters_.threshold_value);
            break;
        case ThresholdingMethod::HEAVISIDE:
            binary = apply_heaviside_threshold(densities, parameters_.threshold_value);
            break;
        case ThresholdingMethod::MORPHOLOGICAL:
            binary = morphological_cleanup(densities,
                std::vector<std::array<double, 3>>(elements.size()),
                parameters_.min_feature_size);
            break;
    }

    // Build vertices from element centers
    std::vector<std::array<double, 3>> positions;
    std::vector<size_t> element_mapping;

    for (size_t i = 0; i < elements.size(); ++i) {
        if (binary[i] > 0.5) {
            element_mapping.push_back(positions.size());
            positions.push_back(elements[i].center);
            geometry.volume += 1.0;
        } else {
            element_mapping.push_back(SIZE_MAX);
        }
    }

    geometry.vertices = positions;

    // Generate simple triangular mesh (one triangle per element face)
    for (size_t i = 0; i < elements.size(); ++i) {
        if (binary[i] > 0.5 && element_mapping[i] != SIZE_MAX) {
            size_t idx = element_mapping[i];

            // Create simple triangles from element center
            if (i > 0 && element_mapping[i-1] != SIZE_MAX) {
                size_t prev_idx = element_mapping[i-1];
                geometry.triangles.push_back({idx, prev_idx,
                    (i > 1) ? element_mapping[i-2] : idx});
            }
        }
    }

    return geometry;
}

double PostProcessor::check_manufacturability(
    const std::vector<double>& densities,
    const std::vector<std::array<double, 3>>& element_positions,
    double min_feature_size,
    double max_overhang_angle) {

    double score = 1.0;

    // Check feature size
    for (size_t i = 0; i < densities.size(); ++i) {
        if (densities[i] > 0.5) {
            // Find nearest void
            double min_dist_to_void = min_feature_size * 2.0;

            for (size_t j = 0; j < densities.size(); ++j) {
                if (densities[j] < 0.5) {
                    double dist = 0.0;
                    if (i < element_positions.size() && j < element_positions.size()) {
                        for (int k = 0; k < 3; ++k) {
                            double diff = element_positions[i][k] - element_positions[j][k];
                            dist += diff * diff;
                        }
                        dist = std::sqrt(dist);
                        min_dist_to_void = std::min(min_dist_to_void, dist);
                    }
                }
            }

            if (min_dist_to_void < min_feature_size) {
                score *= 0.8;
            }
        }
    }

    // Check for overhangs (simplified)
    for (size_t i = 0; i < densities.size(); ++i) {
        if (densities[i] > 0.5 && i > 0 && i < element_positions.size()) {
            // Check if element is overhanging (negative Z gradient)
            auto current_z = element_positions[i][2];
            auto prev_z = (i > 0) ? element_positions[i-1][2] : current_z;

            if (current_z < prev_z) {
                score *= 0.9;
            }
        }
    }

    return std::max(0.0, std::min(1.0, score));
}

std::vector<double> PostProcessor::smooth_boundaries(
    const std::vector<double>& densities,
    const std::vector<std::array<double, 3>>& element_positions,
    double smoothing_radius) {

    std::vector<double> smoothed(densities.size());
    std::vector<double> weights(densities.size(), 0.0);

    for (size_t i = 0; i < densities.size(); ++i) {
        smoothed[i] = 0.0;
        weights[i] = 0.0;

        for (size_t j = 0; j < densities.size(); ++j) {
            double dist = 0.0;
            if (i < element_positions.size() && j < element_positions.size()) {
                for (int k = 0; k < 3; ++k) {
                    double diff = element_positions[i][k] - element_positions[j][k];
                    dist += diff * diff;
                }
                dist = std::sqrt(dist);

                if (dist < smoothing_radius) {
                    double w = std::exp(-dist * dist / (smoothing_radius * smoothing_radius));
                    smoothed[i] += w * densities[j];
                    weights[i] += w;
                }
            }
        }

        if (weights[i] > 1e-12) {
            smoothed[i] /= weights[i];
        } else {
            smoothed[i] = densities[i];
        }
    }

    return smoothed;
}

bool PostProcessor::export_stl(const ExtractedGeometry& geometry,
                               const std::string& filename) {

    std::ofstream file(filename);
    if (!file.is_open()) {
        return false;
    }

    file << "solid geometry\n";

    for (const auto& triangle : geometry.triangles) {
        if (triangle[0] < geometry.vertices.size() &&
            triangle[1] < geometry.vertices.size() &&
            triangle[2] < geometry.vertices.size()) {

            // Compute normal (simplified)
            auto v0 = geometry.vertices[triangle[0]];
            auto v1 = geometry.vertices[triangle[1]];
            auto v2 = geometry.vertices[triangle[2]];

            // v1-v0 cross v2-v0
            std::array<double, 3> e1 = {v1[0]-v0[0], v1[1]-v0[1], v1[2]-v0[2]};
            std::array<double, 3> e2 = {v2[0]-v0[0], v2[1]-v0[1], v2[2]-v0[2]};

            std::array<double, 3> normal = {
                e1[1]*e2[2] - e1[2]*e2[1],
                e1[2]*e2[0] - e1[0]*e2[2],
                e1[0]*e2[1] - e1[1]*e2[0]
            };

            double norm = std::sqrt(normal[0]*normal[0] + normal[1]*normal[1] + normal[2]*normal[2]);
            if (norm > 1e-12) {
                normal[0] /= norm;
                normal[1] /= norm;
                normal[2] /= norm;
            }

            file << "  facet normal " << normal[0] << " " << normal[1] << " " << normal[2] << "\n";
            file << "    outer loop\n";
            file << "      vertex " << v0[0] << " " << v0[1] << " " << v0[2] << "\n";
            file << "      vertex " << v1[0] << " " << v1[1] << " " << v1[2] << "\n";
            file << "      vertex " << v2[0] << " " << v2[1] << " " << v2[2] << "\n";
            file << "    endloop\n";
            file << "  endfacet\n";
        }
    }

    file << "endsolid geometry\n";
    file.close();

    return true;
}

bool PostProcessor::export_stl_binary(const ExtractedGeometry& geometry,
                                     const std::string& filename) {

    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    // Header (80 bytes)
    char header[80] = {0};
    std::string title = "TopologyOptimization";
    std::copy(title.begin(), title.end(), header);
    file.write(header, 80);

    // Triangle count
    uint32_t count = static_cast<uint32_t>(geometry.triangles.size());
    file.write(reinterpret_cast<const char*>(&count), sizeof(uint32_t));

    // Triangles
    for (const auto& tri : geometry.triangles) {
        if (tri[0] < geometry.vertices.size() &&
            tri[1] < geometry.vertices.size() &&
            tri[2] < geometry.vertices.size()) {

            auto v0 = geometry.vertices[tri[0]];
            auto v1 = geometry.vertices[tri[1]];
            auto v2 = geometry.vertices[tri[2]];

            // Normal
            std::array<double, 3> e1 = {v1[0]-v0[0], v1[1]-v0[1], v1[2]-v0[2]};
            std::array<double, 3> e2 = {v2[0]-v0[0], v2[1]-v0[1], v2[2]-v0[2]};

            std::array<float, 3> normal = {
                static_cast<float>(e1[1]*e2[2] - e1[2]*e2[1]),
                static_cast<float>(e1[2]*e2[0] - e1[0]*e2[2]),
                static_cast<float>(e1[0]*e2[1] - e1[1]*e2[0])
            };

            file.write(reinterpret_cast<const char*>(normal.data()), sizeof(float) * 3);
            file.write(reinterpret_cast<const char*>(v0.data()), sizeof(float) * 3);
            file.write(reinterpret_cast<const char*>(v1.data()), sizeof(float) * 3);
            file.write(reinterpret_cast<const char*>(v2.data()), sizeof(float) * 3);

            uint16_t attribute = 0;
            file.write(reinterpret_cast<const char*>(&attribute), sizeof(uint16_t));
        }
    }

    file.close();
    return true;
}

} // namespace topology_optimization
} // namespace bendcore
