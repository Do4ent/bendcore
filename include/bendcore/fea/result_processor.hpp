// FEA Result Processor - Post-processing and Analysis
// M6 Phase: FEA Integration (GAP-025)
#pragma once

#include "bendcore/fea/fea_solver.hpp"
#include "bendcore/fea/mesh_generator.hpp"
#include "bendcore/vec3.hpp"
#include <vector>
#include <array>
#include <string>
#include <cstdint>
#include <memory>

namespace bendcore::fea {

// Safety factor types
enum class SafetyFactor {
    Yield,         // Based on yield strength
    Ultimate,      // Based on ultimate strength
    Fatigue,       // Based on fatigue limit
    Custom         // User-defined
};

// Result type enumerations
enum class ResultType {
    Displacement,
    VonMisesStress,
    PrincipalStress1,
    PrincipalStress2,
    PrincipalStress3,
    ShearStress,
    StrainEnergy,
    SafetyFactor
};

// Nodal result value
struct NodalResult {
    int32_t node_id;
    double value;
    Vec3 vector_value;  // For vector results like displacement
};

// Element result value
struct ElementResult {
    int32_t element_id;
    double value;
    std::array<double, 8> nodal_values;  // Results at element nodes
};

// Safety analysis result
struct SafetyAnalysisResult {
    int32_t node_id;
    double stress;
    double yield_strength;
    double safety_factor;
    bool is_safe;
};

// Stress concentration result
struct StressConcentration {
    int32_t element_id;
    double stress_concentration_factor;
    double nominal_stress;
    double concentrated_stress;
};

// Deformation summary
struct DeformationSummary {
    Vec3 max_displacement_location;
    double max_displacement_magnitude;
    double total_volume_change;
    double strain_energy;
};

// Result processor for FEA analysis
class ResultProcessor {
public:
    ResultProcessor();

    // Set analysis results
    void set_solver_results(const FEASolver& solver, const FiniteElementMesh& mesh);

    // Get nodal results
    std::vector<NodalResult> get_nodal_displacements() const;
    std::vector<NodalResult> get_nodal_von_mises_stresses() const;
    std::vector<NodalResult> get_nodal_principal_stresses(int32_t component) const;  // 1, 2, or 3
    std::vector<NodalResult> get_nodal_strain_energy() const;

    // Get element results
    std::vector<ElementResult> get_element_stresses() const;
    std::vector<ElementResult> get_element_strains() const;

    // Maximum and minimum values
    double get_max_displacement() const { return max_displacement_; }
    double get_max_stress() const { return max_stress_; }
    double get_min_stress() const { return min_stress_; }
    double get_max_principal_stress() const { return max_principal_stress_; }
    double get_min_principal_stress() const { return min_principal_stress_; }

    // Location of extrema
    int32_t get_max_displacement_node() const { return max_displacement_node_; }
    int32_t get_max_stress_node() const { return max_stress_node_; }
    int32_t get_max_stress_element() const { return max_stress_element_; }

    // Safety analysis
    SafetyAnalysisResult compute_safety_factor(
        int32_t node_id,
        const Material& material,
        SafetyFactor safety_type = SafetyFactor::Yield) const;

    std::vector<SafetyAnalysisResult> compute_all_safety_factors(
        const Material& material,
        SafetyFactor safety_type = SafetyFactor::Yield) const;

    // Stress concentration analysis
    std::vector<StressConcentration> analyze_stress_concentration(
        const std::vector<int32_t>& element_ids) const;

    // Deformation summary
    DeformationSummary compute_deformation_summary() const;

    // Export results
    std::string export_to_json() const;
    std::string export_to_csv() const;
    std::string export_to_vtu() const;

    // Query results
    bool has_results() const { return has_results_; }
    int32_t get_num_nodes_with_results() const { return num_nodes_; }

private:
    const FEASolver* solver_;
    const FiniteElementMesh* mesh_;

    // Cached results
    std::vector<Vec3> nodal_displacements_;
    std::vector<double> nodal_von_mises_stresses_;
    std::vector<std::array<double, 3>> nodal_principal_stresses_;
    std::vector<double> nodal_strain_energies_;

    // Summary statistics
    double max_displacement_;
    double max_stress_;
    double min_stress_;
    double max_principal_stress_;
    double min_principal_stress_;
    int32_t max_displacement_node_;
    int32_t max_stress_node_;
    int32_t max_stress_element_;
    int32_t num_nodes_;

    bool has_results_;

    // Helper functions
    void compute_summary_statistics();
    void compute_strain_energies();
};

// Stress analysis utilities
class StressAnalyzer {
public:
    // Compute principal stresses from stress tensor
    static std::array<double, 3> compute_principal_stresses(
        const std::array<std::array<double, 3>, 3>& stress_tensor);

    // Compute von Mises stress
    static double compute_von_mises_stress(
        const std::array<std::array<double, 3>, 3>& stress_tensor);

    // Compute hydrostatic stress
    static double compute_hydrostatic_stress(
        const std::array<std::array<double, 3>, 3>& stress_tensor);

    // Compute deviatoric stress
    static std::array<std::array<double, 3>, 3> compute_deviatoric_stress(
        const std::array<std::array<double, 3>, 3>& stress_tensor);

    // Compute strain from displacement field
    static std::array<std::array<double, 3>, 3> compute_strain_tensor(
        const std::vector<Vec3>& displacements,
        const FiniteElementMesh& mesh);

    // Compute effective plastic strain
    static double compute_effective_plastic_strain(
        const std::array<std::array<double, 3>, 3>& strain_tensor);
};

// Visualization data generator
class VisualizationDataGenerator {
public:
    // Generate color map for stress visualization
    static std::vector<std::array<double, 3>> generate_stress_colormap(
        const std::vector<double>& stress_values,
        double min_stress, double max_stress);

    // Generate mesh for visualization
    static std::string generate_vtk_mesh(
        const FiniteElementMesh& mesh,
        const std::vector<double>& result_values,
        const std::string& result_name);

    // Generate deformed mesh geometry
    static std::vector<Vec3> compute_deformed_geometry(
        const FiniteElementMesh& mesh,
        const std::vector<Vec3>& displacements,
        double scale_factor = 1.0);
};

}  // namespace bendcore::fea
