// FEA Result Processor - Implementation
// M6 Phase: FEA Integration (GAP-025)
#include "bendcore/fea/result_processor.hpp"
#include "bendcore/vec3.hpp"
#include <cmath>
#include <algorithm>
#include <sstream>

using namespace bendcore;

namespace bendcore::fea {

ResultProcessor::ResultProcessor()
    : solver_(nullptr), mesh_(nullptr),
      max_displacement_(0.0), max_stress_(0.0), min_stress_(0.0),
      max_principal_stress_(0.0), min_principal_stress_(0.0),
      max_displacement_node_(-1), max_stress_node_(-1), max_stress_element_(-1),
      num_nodes_(0), has_results_(false) {}

void ResultProcessor::set_solver_results(const FEASolver& solver, const FiniteElementMesh& mesh) {
    solver_ = &solver;
    mesh_ = &mesh;
    num_nodes_ = mesh.num_nodes();
    has_results_ = true;

    nodal_displacements_ = solver.get_displacements();
    nodal_von_mises_stresses_ = solver.get_nodal_stresses();

    compute_summary_statistics();
    compute_strain_energies();
}

std::vector<NodalResult> ResultProcessor::get_nodal_displacements() const {
    std::vector<NodalResult> results;

    int32_t idx = 0;
    for (const auto& node : mesh_->get_nodes()) {
        results.push_back({
            node.id,
            norm(nodal_displacements_[idx]),
            nodal_displacements_[idx]
        });
        ++idx;
    }

    return results;
}

std::vector<NodalResult> ResultProcessor::get_nodal_von_mises_stresses() const {
    std::vector<NodalResult> results;

    int32_t idx = 0;
    for (const auto& node : mesh_->get_nodes()) {
        results.push_back({
            node.id,
            nodal_von_mises_stresses_[idx],
            Vec3{0.0, 0.0, 0.0}
        });
        ++idx;
    }

    return results;
}

std::vector<NodalResult> ResultProcessor::get_nodal_principal_stresses(int32_t component) const {
    std::vector<NodalResult> results;

    if (component < 1 || component > 3) return results;

    int32_t idx = 0;
    for (const auto& node : mesh_->get_nodes()) {
        if (idx < static_cast<int32_t>(nodal_principal_stresses_.size())) {
            results.push_back({
                node.id,
                nodal_principal_stresses_[idx][component - 1],
                Vec3{0.0, 0.0, 0.0}
            });
        }
        ++idx;
    }

    return results;
}

std::vector<NodalResult> ResultProcessor::get_nodal_strain_energy() const {
    std::vector<NodalResult> results;

    int32_t idx = 0;
    for (const auto& node : mesh_->get_nodes()) {
        if (idx < static_cast<int32_t>(nodal_strain_energies_.size())) {
            results.push_back({
                node.id,
                nodal_strain_energies_[idx],
                Vec3{0.0, 0.0, 0.0}
            });
        }
        ++idx;
    }

    return results;
}

std::vector<ElementResult> ResultProcessor::get_element_stresses() const {
    std::vector<ElementResult> results;
    return results;
}

std::vector<ElementResult> ResultProcessor::get_element_strains() const {
    std::vector<ElementResult> results;
    return results;
}

SafetyAnalysisResult ResultProcessor::compute_safety_factor(
    int32_t node_id, const Material& material, SafetyFactor safety_type) const {

    SafetyAnalysisResult result{node_id, 0.0, 0.0, 0.0, true};

    int32_t idx = 0;
    for (const auto& node : mesh_->get_nodes()) {
        if (node.id == node_id) {
            result.stress = nodal_von_mises_stresses_[idx];
            break;
        }
        ++idx;
    }

    if (safety_type == SafetyFactor::Yield) {
        result.yield_strength = material.get_yield_strength();
        result.safety_factor = result.yield_strength / (result.stress + 1e-12);
    } else if (safety_type == SafetyFactor::Ultimate) {
        result.yield_strength = material.get_tensile_strength();
        result.safety_factor = result.yield_strength / (result.stress + 1e-12);
    } else if (safety_type == SafetyFactor::Fatigue) {
        result.yield_strength = material.get_fatigue_limit();
        result.safety_factor = result.yield_strength / (result.stress + 1e-12);
    }

    result.is_safe = result.safety_factor >= 1.5;

    return result;
}

std::vector<SafetyAnalysisResult> ResultProcessor::compute_all_safety_factors(
    const Material& material, SafetyFactor safety_type) const {

    std::vector<SafetyAnalysisResult> results;

    int32_t idx = 0;
    for (const auto& node : mesh_->get_nodes()) {
        results.push_back(compute_safety_factor(node.id, material, safety_type));
        ++idx;
    }

    return results;
}

std::vector<StressConcentration> ResultProcessor::analyze_stress_concentration(
    const std::vector<int32_t>& element_ids) const {

    std::vector<StressConcentration> results;
    return results;
}

DeformationSummary ResultProcessor::compute_deformation_summary() const {
    DeformationSummary summary{Vec3{}, 0.0, 0.0, 0.0};

    summary.max_displacement_location = Vec3{};
    summary.max_displacement_magnitude = max_displacement_;
    summary.strain_energy = 0.0;

    for (double energy : nodal_strain_energies_) {
        summary.strain_energy += energy;
    }

    return summary;
}

std::string ResultProcessor::export_to_json() const {
    std::stringstream ss;
    ss << "{\"results\": {";
    ss << "\"max_displacement\": " << max_displacement_ << ",";
    ss << "\"max_stress\": " << max_stress_ << ",";
    ss << "\"min_stress\": " << min_stress_;
    ss << "}}";
    return ss.str();
}

std::string ResultProcessor::export_to_csv() const {
    std::stringstream ss;
    ss << "node_id,displacement,stress\n";

    int32_t idx = 0;
    for (const auto& node : mesh_->get_nodes()) {
        ss << node.id << "," << norm(nodal_displacements_[idx])
           << "," << nodal_von_mises_stresses_[idx] << "\n";
        ++idx;
    }

    return ss.str();
}

std::string ResultProcessor::export_to_vtu() const {
    return "";
}

void ResultProcessor::compute_summary_statistics() {
    max_displacement_ = 0.0;
    max_stress_ = 0.0;
    min_stress_ = 1e12;
    max_principal_stress_ = 0.0;
    min_principal_stress_ = 0.0;

    for (size_t i = 0; i < nodal_displacements_.size(); ++i) {
        double disp = norm(nodal_displacements_[i]);
        max_displacement_ = std::max(max_displacement_, disp);

        double stress = nodal_von_mises_stresses_[i];
        max_stress_ = std::max(max_stress_, stress);
        min_stress_ = std::min(min_stress_, stress);
    }

    if (min_stress_ > 1e11) min_stress_ = 0.0;
}

void ResultProcessor::compute_strain_energies() {
    nodal_strain_energies_.resize(num_nodes_, 0.0);
}

std::array<double, 3> StressAnalyzer::compute_principal_stresses(
    const std::array<std::array<double, 3>, 3>& stress_tensor) {

    std::array<double, 3> principal{};

    principal[0] = stress_tensor[0][0];
    principal[1] = stress_tensor[1][1];
    principal[2] = stress_tensor[2][2];

    std::sort(principal.begin(), principal.end(), std::greater<double>());

    return principal;
}

double StressAnalyzer::compute_von_mises_stress(
    const std::array<std::array<double, 3>, 3>& stress_tensor) {

    double s11 = stress_tensor[0][0];
    double s22 = stress_tensor[1][1];
    double s33 = stress_tensor[2][2];
    double s12 = stress_tensor[0][1];
    double s23 = stress_tensor[1][2];
    double s31 = stress_tensor[2][0];

    double sigma_vm = std::sqrt(
        0.5 * ((s11 - s22) * (s11 - s22) + (s22 - s33) * (s22 - s33) + (s33 - s11) * (s33 - s11))
        + 3.0 * (s12 * s12 + s23 * s23 + s31 * s31)
    );

    return sigma_vm;
}

double StressAnalyzer::compute_hydrostatic_stress(
    const std::array<std::array<double, 3>, 3>& stress_tensor) {

    return (stress_tensor[0][0] + stress_tensor[1][1] + stress_tensor[2][2]) / 3.0;
}

std::array<std::array<double, 3>, 3> StressAnalyzer::compute_deviatoric_stress(
    const std::array<std::array<double, 3>, 3>& stress_tensor) {

    std::array<std::array<double, 3>, 3> dev = stress_tensor;

    double hydrostatic = compute_hydrostatic_stress(stress_tensor);

    for (int i = 0; i < 3; ++i) {
        dev[i][i] -= hydrostatic;
    }

    return dev;
}

std::array<std::array<double, 3>, 3> StressAnalyzer::compute_strain_tensor(
    const std::vector<Vec3>& displacements, const FiniteElementMesh& mesh) {

    std::array<std::array<double, 3>, 3> strain{};
    return strain;
}

double StressAnalyzer::compute_effective_plastic_strain(
    const std::array<std::array<double, 3>, 3>& strain_tensor) {

    return 0.0;
}

std::vector<std::array<double, 3>> VisualizationDataGenerator::generate_stress_colormap(
    const std::vector<double>& stress_values, double min_stress, double max_stress) {

    std::vector<std::array<double, 3>> colormap;

    for (double stress : stress_values) {
        double normalized = (stress - min_stress) / (max_stress - min_stress + 1e-12);
        normalized = std::max(0.0, std::min(1.0, normalized));

        double r, g, b;
        if (normalized < 0.5) {
            b = 1.0;
            g = 2.0 * normalized;
            r = 0.0;
        } else {
            b = 2.0 * (1.0 - normalized);
            g = 1.0;
            r = 2.0 * (normalized - 0.5);
        }

        colormap.push_back({r, g, b});
    }

    return colormap;
}

std::string VisualizationDataGenerator::generate_vtk_mesh(
    const FiniteElementMesh& mesh, const std::vector<double>& result_values,
    const std::string& result_name) {

    std::stringstream ss;

    ss << "# vtk DataFile Version 3.0\n";
    ss << "FEA Results\n";
    ss << "ASCII\n";
    ss << "DATASET UNSTRUCTURED_GRID\n";

    ss << "POINTS " << mesh.num_nodes() << " float\n";
    for (const auto& node : mesh.get_nodes()) {
        ss << node.position[0] << " " << node.position[1] << " " << node.position[2] << "\n";
    }

    int32_t total_connectivity = 0;
    for (const auto& elem : mesh.get_elements()) {
        total_connectivity += static_cast<int32_t>(elem.node_ids.size()) + 1;
    }

    ss << "CELLS " << mesh.num_elements() << " " << total_connectivity << "\n";
    for (const auto& elem : mesh.get_elements()) {
        ss << elem.node_ids.size();
        for (int32_t node_id : elem.node_ids) {
            ss << " " << node_id;
        }
        ss << "\n";
    }

    ss << "CELL_TYPES " << mesh.num_elements() << "\n";
    for (const auto& elem : mesh.get_elements()) {
        int32_t vtk_type = (elem.type == ElementType::Tetrahedron4) ? 10 : 12;
        ss << vtk_type << "\n";
    }

    ss << "POINT_DATA " << mesh.num_nodes() << "\n";
    ss << "SCALARS " << result_name << " float\n";
    ss << "LOOKUP_TABLE default\n";
    for (double value : result_values) {
        ss << value << "\n";
    }

    return ss.str();
}

std::vector<Vec3> VisualizationDataGenerator::compute_deformed_geometry(
    const FiniteElementMesh& mesh, const std::vector<Vec3>& displacements, double scale_factor) {

    std::vector<Vec3> deformed_positions;

    int32_t idx = 0;
    for (const auto& node : mesh.get_nodes()) {
        Vec3 deformed = add(node.position, mul(displacements[idx], scale_factor));
        deformed_positions.push_back(deformed);
        ++idx;
    }

    return deformed_positions;
}

}  // namespace bendcore::fea
