#pragma once

#include <vector>
#include <string>
#include <memory>
#include <optional>
#include <map>
#include "design_space.hpp"

namespace bendcore::generative {

/**
 * Design Generator Module
 * Parametric design synthesis and feature tree generation
 */

// Feature type in design generation
enum class FeatureType {
    SKETCH,
    PAD,
    POCKET,
    HOLE,
    FILLET,
    CHAMFER,
    BEND,
    PATTERN,
    LOFT,
    SWEEP,
    REVOLVE,
    MIRROR,
    SHELL,
    CUSTOM
};

// Feature definition with parameters
struct DesignFeature {
    std::string name;
    FeatureType type;
    std::vector<double> parameters;  // Feature-specific parameters
    std::vector<size_t> parameter_indices;  // Indices into design space
    std::map<std::string, std::string> properties;

    DesignFeature() = default;
    DesignFeature(const std::string& n, FeatureType t)
        : name(n), type(t) {}

    [[nodiscard]] bool is_valid() const { return !name.empty(); }
};

// Design topology representation
struct DesignTopology {
    std::vector<DesignFeature> features;  // Ordered feature tree
    std::vector<int> parent_indices;      // Feature dependencies
    std::vector<std::vector<int>> child_indices;  // Children for each feature

    [[nodiscard]] bool is_valid_dag() const;
    [[nodiscard]] std::vector<size_t> topological_order() const;
    [[nodiscard]] bool has_cycles() const;
};

// Design geometry properties
struct DesignGeometry {
    double volume = 0.0;
    double surface_area = 0.0;
    double mass = 0.0;
    std::vector<double> bounding_box;  // [min_x, max_x, min_y, max_y, min_z, max_z]
    std::vector<double> center_of_mass;

    [[nodiscard]] bool is_valid() const { return volume > 0.0; }
    [[nodiscard]] std::string to_string() const;
};

// Generated design
struct GeneratedDesign {
    std::vector<double> design_variables;
    DesignTopology topology;
    DesignGeometry geometry;
    std::map<std::string, double> properties;

    [[nodiscard]] bool is_complete() const { return topology.is_valid_dag() && geometry.is_valid(); }
};

// Design generator
class DesignGenerator {
public:
    explicit DesignGenerator(const DesignSpace& design_space);

    // Feature management
    void register_feature_type(FeatureType type, const std::string& template_name);
    [[nodiscard]] const DesignFeature& get_feature_template(FeatureType type) const;

    // Design generation
    [[nodiscard]] GeneratedDesign generate_design(const std::vector<double>& design_variables);
    [[nodiscard]] DesignTopology generate_topology(const std::vector<double>& design_variables);
    [[nodiscard]] DesignGeometry compute_geometry(const DesignTopology& topology) const;

    // Feature operations
    void add_feature(const DesignFeature& feature, int parent_index = -1);
    void modify_feature(size_t feature_index, const std::vector<double>& new_parameters);
    void remove_feature(size_t feature_index);

    // Topology modification
    void reorder_features(const std::vector<size_t>& new_order);
    [[nodiscard]] bool validate_dependencies(const DesignTopology& topology) const;
    [[nodiscard]] std::vector<size_t> resolve_dependencies(const DesignTopology& topology) const;

    // Geometry computation
    [[nodiscard]] double compute_volume(const DesignTopology& topology) const;
    [[nodiscard]] double compute_surface_area(const DesignTopology& topology) const;
    [[nodiscard]] double compute_mass(const DesignTopology& topology, double material_density) const;
    [[nodiscard]] std::vector<double> compute_center_of_mass(const DesignTopology& topology) const;
    [[nodiscard]] std::vector<double> compute_bounding_box(const DesignTopology& topology) const;

    // Variability and parameterization
    [[nodiscard]] bool is_parameter_used(size_t param_index) const;
    [[nodiscard]] std::vector<size_t> get_related_features(size_t param_index) const;
    [[nodiscard]] std::vector<double> get_parameter_sensitivity(size_t param_index) const;

    // Design validation
    [[nodiscard]] bool validate_design(const GeneratedDesign& design) const;
    [[nodiscard]] std::vector<std::string> get_validation_errors(const GeneratedDesign& design) const;

private:
    const DesignSpace& design_space_;
    std::map<FeatureType, DesignFeature> feature_templates_;

    // Internal operations
    [[nodiscard]] DesignFeature create_feature_from_variables(FeatureType type,
                                                             const std::vector<double>& variables) const;
    [[nodiscard]] bool check_feature_compatibility(const DesignFeature& f1, const DesignFeature& f2) const;
    [[nodiscard]] double estimate_feature_volume(const DesignFeature& feature) const;
};

} // namespace bendcore::generative
