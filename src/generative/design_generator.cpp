#include "../../include/bendcore/generative/design_generator.hpp"
#include <cmath>
#include <sstream>
#include <algorithm>

namespace bendcore::generative {

// DesignGeometry methods
std::string DesignGeometry::to_string() const {
    std::ostringstream oss;
    oss << "Volume: " << volume << "\n";
    oss << "Surface Area: " << surface_area << "\n";
    oss << "Mass: " << mass;
    return oss.str();
}

// DesignTopology methods
bool DesignTopology::is_valid_dag() const {
    if (features.empty()) return true;

    // Check for cycles using DFS
    std::vector<int> visited(features.size(), 0);

    std::function<bool(size_t)> has_cycle = [&](size_t idx) -> bool {
        visited[idx] = 1;

        if (idx < child_indices.size()) {
            for (int child : child_indices[idx]) {
                if (child >= 0 && child < static_cast<int>(features.size())) {
                    if (visited[child] == 1) return true;  // Back edge
                    if (visited[child] == 0 && has_cycle(child)) return true;
                }
            }
        }

        visited[idx] = 2;
        return false;
    };

    for (size_t i = 0; i < features.size(); ++i) {
        if (visited[i] == 0 && has_cycle(i)) return false;
    }

    return true;
}

std::vector<size_t> DesignTopology::topological_order() const {
    std::vector<size_t> order;
    if (!is_valid_dag()) return order;

    std::vector<int> in_degree(features.size(), 0);
    for (size_t i = 0; i < parent_indices.size(); ++i) {
        if (parent_indices[i] >= 0) {
            in_degree[i]++;
        }
    }

    std::vector<size_t> queue;
    for (size_t i = 0; i < in_degree.size(); ++i) {
        if (in_degree[i] == 0) {
            queue.push_back(i);
        }
    }

    while (!queue.empty()) {
        size_t u = queue.back();
        queue.pop_back();
        order.push_back(u);

        if (u < child_indices.size()) {
            for (int child : child_indices[u]) {
                if (child >= 0) {
                    in_degree[child]--;
                    if (in_degree[child] == 0) {
                        queue.push_back(child);
                    }
                }
            }
        }
    }

    return order;
}

bool DesignTopology::has_cycles() const {
    return !is_valid_dag();
}

// DesignGenerator methods
DesignGenerator::DesignGenerator(const DesignSpace& design_space)
    : design_space_(design_space) {

    // Register default feature types
    register_feature_type(FeatureType::PAD, "Pad");
    register_feature_type(FeatureType::POCKET, "Pocket");
    register_feature_type(FeatureType::HOLE, "Hole");
    register_feature_type(FeatureType::FILLET, "Fillet");
    register_feature_type(FeatureType::CHAMFER, "Chamfer");
}

void DesignGenerator::register_feature_type(FeatureType type, const std::string& template_name) {
    DesignFeature template_feature;
    template_feature.name = template_name;
    template_feature.type = type;
    feature_templates_[type] = template_feature;
}

const DesignFeature& DesignGenerator::get_feature_template(FeatureType type) const {
    if (feature_templates_.find(type) == feature_templates_.end()) {
        static DesignFeature empty;
        return empty;
    }
    return feature_templates_.at(type);
}

GeneratedDesign DesignGenerator::generate_design(const std::vector<double>& design_variables) {
    GeneratedDesign design;
    design.design_variables = design_variables;
    design.topology = generate_topology(design_variables);
    design.geometry = compute_geometry(design.topology);
    return design;
}

DesignTopology DesignGenerator::generate_topology(const std::vector<double>& design_variables) {
    DesignTopology topology;

    // Generate features based on design variables
    size_t num_features = std::min(static_cast<size_t>(5), design_variables.size() / 3);

    for (size_t i = 0; i < num_features && i * 3 < design_variables.size(); ++i) {
        double param1 = design_variables[i * 3];
        double param2 = (i * 3 + 1 < design_variables.size()) ? design_variables[i * 3 + 1] : 0.5;
        double param3 = (i * 3 + 2 < design_variables.size()) ? design_variables[i * 3 + 2] : 0.5;

        FeatureType feature_type = (i % 4 == 0) ? FeatureType::PAD :
                                   (i % 4 == 1) ? FeatureType::POCKET :
                                   (i % 4 == 2) ? FeatureType::FILLET : FeatureType::CHAMFER;

        DesignFeature feature;
        feature.name = "Feature_" + std::to_string(i);
        feature.type = feature_type;
        feature.parameters = {param1, param2, param3};
        feature.parameter_indices = {i * 3, i * 3 + 1, i * 3 + 2};

        topology.features.push_back(feature);
        topology.parent_indices.push_back(i > 0 ? i - 1 : -1);
    }

    // Build child indices
    topology.child_indices.resize(topology.features.size());
    for (size_t i = 0; i < topology.parent_indices.size(); ++i) {
        if (topology.parent_indices[i] >= 0) {
            topology.child_indices[topology.parent_indices[i]].push_back(i);
        }
    }

    return topology;
}

DesignGeometry DesignGenerator::compute_geometry(const DesignTopology& topology) const {
    DesignGeometry geometry;

    if (topology.features.empty()) {
        return geometry;
    }

    // Compute cumulative geometry
    double total_volume = 0.0;
    double total_surface_area = 0.0;

    for (const auto& feature : topology.features) {
        total_volume += estimate_feature_volume(feature);
    }

    // Approximate surface area from volume
    double side_length = std::cbrt(total_volume);
    total_surface_area = 6.0 * side_length * side_length;

    geometry.volume = std::max(total_volume, 1.0);  // Minimum volume
    geometry.surface_area = std::max(total_surface_area, 1.0);
    geometry.mass = geometry.volume * 7.85 / 1000.0;  // Steel density

    // Bounding box
    geometry.bounding_box = {0, side_length, 0, side_length, 0, side_length};

    // Center of mass (approximate)
    geometry.center_of_mass = {side_length / 2.0, side_length / 2.0, side_length / 2.0};

    return geometry;
}

void DesignGenerator::add_feature(const DesignFeature& feature, int parent_index) {
    // Placeholder implementation
}

void DesignGenerator::modify_feature(size_t feature_index, const std::vector<double>& new_parameters) {
    // Placeholder implementation
}

void DesignGenerator::remove_feature(size_t feature_index) {
    // Placeholder implementation
}

void DesignGenerator::reorder_features(const std::vector<size_t>& new_order) {
    // Placeholder implementation
}

bool DesignGenerator::validate_dependencies(const DesignTopology& topology) const {
    return topology.is_valid_dag();
}

std::vector<size_t> DesignGenerator::resolve_dependencies(const DesignTopology& topology) const {
    return topology.topological_order();
}

double DesignGenerator::compute_volume(const DesignTopology& topology) const {
    double volume = 0.0;
    for (const auto& feature : topology.features) {
        volume += estimate_feature_volume(feature);
    }
    return volume;
}

double DesignGenerator::compute_surface_area(const DesignTopology& topology) const {
    double volume = compute_volume(topology);
    double side_length = std::cbrt(volume);
    return 6.0 * side_length * side_length;
}

double DesignGenerator::compute_mass(const DesignTopology& topology, double material_density) const {
    return compute_volume(topology) * material_density;
}

std::vector<double> DesignGenerator::compute_center_of_mass(const DesignTopology& topology) const {
    if (topology.features.empty()) {
        return {0.5, 0.5, 0.5};
    }

    double volume = compute_volume(topology);
    double side_length = std::cbrt(volume);
    return {side_length / 2.0, side_length / 2.0, side_length / 2.0};
}

std::vector<double> DesignGenerator::compute_bounding_box(const DesignTopology& topology) const {
    double volume = compute_volume(topology);
    double side_length = std::cbrt(volume);
    return {0, side_length, 0, side_length, 0, side_length};
}

bool DesignGenerator::is_parameter_used(size_t param_index) const {
    // Placeholder implementation
    return true;
}

std::vector<size_t> DesignGenerator::get_related_features(size_t param_index) const {
    std::vector<size_t> related;
    // Placeholder implementation
    return related;
}

std::vector<double> DesignGenerator::get_parameter_sensitivity(size_t param_index) const {
    std::vector<double> sensitivity;
    // Placeholder implementation
    return sensitivity;
}

bool DesignGenerator::validate_design(const GeneratedDesign& design) const {
    return design.is_complete() && validate_dependencies(design.topology);
}

std::vector<std::string> DesignGenerator::get_validation_errors(const GeneratedDesign& design) const {
    std::vector<std::string> errors;

    if (!design.topology.is_valid_dag()) {
        errors.push_back("Invalid topology: contains cycles");
    }

    if (!design.geometry.is_valid()) {
        errors.push_back("Invalid geometry: volume is zero or negative");
    }

    return errors;
}

DesignFeature DesignGenerator::create_feature_from_variables(FeatureType type,
                                                            const std::vector<double>& variables) const {
    DesignFeature feature;
    feature.type = type;
    feature.parameters = variables;
    return feature;
}

bool DesignGenerator::check_feature_compatibility(const DesignFeature& f1, const DesignFeature& f2) const {
    // Simple compatibility check
    return true;
}

double DesignGenerator::estimate_feature_volume(const DesignFeature& feature) const {
    if (feature.parameters.empty()) return 1.0;

    double param1 = feature.parameters[0];
    double param2 = (feature.parameters.size() > 1) ? feature.parameters[1] : 1.0;
    double param3 = (feature.parameters.size() > 2) ? feature.parameters[2] : 1.0;

    return param1 * param2 * param3 * 100.0;  // Approximate volume
}

} // namespace bendcore::generative
