#include "bendcore/simulation/load_case_handler.hpp"
#include <algorithm>
#include <numeric>
#include <cmath>

namespace bendcore::simulation {

bool LoadCaseHandler::addLoadComponent(const LoadComponent& load) noexcept {
    if (load.name.empty()) return false;
    loads_[load.name] = load;
    return true;
}

bool LoadCaseHandler::addBoundaryCondition(const BoundaryCondition& bc) noexcept {
    if (bc.name.empty()) return false;
    boundary_conditions_[bc.name] = bc;
    return true;
}

bool LoadCaseHandler::addLoadCombination(const LoadCombination& combination) noexcept {
    if (combination.name.empty()) return false;
    load_combinations_[combination.name] = combination;
    return true;
}

std::optional<LoadComponent> LoadCaseHandler::getLoadComponent(const std::string& name) const noexcept {
    auto it = loads_.find(name);
    if (it != loads_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::optional<BoundaryCondition> LoadCaseHandler::getBoundaryCondition(const std::string& name) const noexcept {
    auto it = boundary_conditions_.find(name);
    if (it != boundary_conditions_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::optional<LoadCombination> LoadCaseHandler::getLoadCombination(const std::string& name) const noexcept {
    auto it = load_combinations_.find(name);
    if (it != load_combinations_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<LoadComponent> LoadCaseHandler::getAllLoadComponents() const noexcept {
    std::vector<LoadComponent> results;
    for (const auto& [name, load] : loads_) {
        results.push_back(load);
    }
    return results;
}

std::vector<BoundaryCondition> LoadCaseHandler::getAllBoundaryConditions() const noexcept {
    std::vector<BoundaryCondition> results;
    for (const auto& [name, bc] : boundary_conditions_) {
        results.push_back(bc);
    }
    return results;
}

std::vector<LoadCombination> LoadCaseHandler::getAllLoadCombinations() const noexcept {
    std::vector<LoadCombination> results;
    for (const auto& [name, comb] : load_combinations_) {
        results.push_back(comb);
    }
    return results;
}

LoadCaseResult LoadCaseHandler::analyzeLoadCase(const std::vector<std::string>& load_names) noexcept {
    LoadCaseResult result;
    result.load_case_name = "Custom";

    if (load_names.empty()) {
        result.converged = false;
        result.status_message = "No loads specified";
        return result;
    }

    // Validate load case
    if (!validateLoadCase(load_names)) {
        result.converged = false;
        result.status_message = "Load case validation failed";
        return result;
    }

    // Accumulate loads
    std::vector<LoadComponent> combined_loads;
    for (const auto& name : load_names) {
        auto load = getLoadComponent(name);
        if (load) {
            combined_loads.push_back(*load);
        }
    }

    if (combined_loads.empty()) {
        result.converged = false;
        result.status_message = "No valid loads found";
        return result;
    }

    // Calculate total force and moment
    double total_fx = 0.0, total_fy = 0.0, total_fz = 0.0;
    double total_mx = 0.0, total_my = 0.0, total_mz = 0.0;

    for (const auto& load : combined_loads) {
        if (!load.is_moment) {
            // Force load
            switch (load.direction) {
                case LoadDirection::PositiveX: total_fx += load.magnitude; break;
                case LoadDirection::NegativeX: total_fx -= load.magnitude; break;
                case LoadDirection::PositiveY: total_fy += load.magnitude; break;
                case LoadDirection::NegativeY: total_fy -= load.magnitude; break;
                case LoadDirection::PositiveZ: total_fz += load.magnitude; break;
                case LoadDirection::NegativeZ: total_fz -= load.magnitude; break;
            }
        } else {
            // Moment load
            switch (load.direction) {
                case LoadDirection::PositiveX: total_mx += load.magnitude; break;
                case LoadDirection::NegativeX: total_mx -= load.magnitude; break;
                case LoadDirection::PositiveY: total_my += load.magnitude; break;
                case LoadDirection::NegativeY: total_my -= load.magnitude; break;
                case LoadDirection::PositiveZ: total_mz += load.magnitude; break;
                case LoadDirection::NegativeZ: total_mz -= load.magnitude; break;
            }
        }
    }

    // Calculate equivalent stress
    result.max_stress = calculateEquivalentStress(total_fx, total_fy, total_fz);

    // Calculate displacement (simplified)
    result.max_displacement = std::sqrt(total_fx * total_fx + total_fy * total_fy + total_fz * total_fz) / 1e6;

    // Calculate strain energy
    result.strain_energy = 0.5 * result.max_stress * result.max_displacement;

    // Calculate reaction forces
    result.reaction_forces = {-total_fx, -total_fy, -total_fz};
    result.reaction_moments = {-total_mx, -total_my, -total_mz};

    // Check equilibrium
    result.converged = checkStaticEquilibrium(combined_loads);

    if (result.converged) {
        result.status_message = "Load case analyzed successfully";
    } else {
        result.status_message = "Static equilibrium not satisfied";
    }

    return result;
}

std::vector<LoadCaseResult> LoadCaseHandler::analyzeLoadCombination(const std::string& combination_name) noexcept {
    std::vector<LoadCaseResult> results;

    auto combo = getLoadCombination(combination_name);
    if (!combo) {
        LoadCaseResult fail_result;
        fail_result.converged = false;
        fail_result.status_message = "Load combination not found";
        results.push_back(fail_result);
        return results;
    }

    // Analyze each load factor in the combination
    for (const auto& [load_name, factor] : combo->load_factors) {
        auto load = getLoadComponent(load_name);
        if (load) {
            // Create scaled load
            LoadComponent scaled = *load;
            scaled.magnitude *= factor;

            LoadCaseResult result = analyzeLoadCase({load_name});
            result.load_case_name = combination_name + " (" + load_name + ")";
            results.push_back(result);
        }
    }

    return results;
}

std::vector<LoadCaseResult> LoadCaseHandler::analyzeAllLoadCombinations() noexcept {
    std::vector<LoadCaseResult> all_results;

    for (const auto& [name, combo] : load_combinations_) {
        auto results = analyzeLoadCombination(name);
        all_results.insert(all_results.end(), results.begin(), results.end());
    }

    return all_results;
}

bool LoadCaseHandler::removeLoadComponent(const std::string& name) noexcept {
    return loads_.erase(name) > 0;
}

bool LoadCaseHandler::removeBoundaryCondition(const std::string& name) noexcept {
    return boundary_conditions_.erase(name) > 0;
}

bool LoadCaseHandler::removeLoadCombination(const std::string& name) noexcept {
    return load_combinations_.erase(name) > 0;
}

bool LoadCaseHandler::validateLoadCase(const std::vector<std::string>& load_names) const noexcept {
    // Check that all loads exist
    for (const auto& name : load_names) {
        if (loads_.find(name) == loads_.end()) {
            return false;
        }
    }

    // Check that we have boundary conditions
    if (boundary_conditions_.empty()) {
        return false;
    }

    return true;
}

bool LoadCaseHandler::isLoadCaseStaticallyDetermined() const noexcept {
    // System is statically determined if:
    // #DOF_restrained = #DOF_free for 3D (6 DOF per node)
    // or other rules for 2D (3 DOF per node)

    int restrained = countRestrainedDOF();
    return restrained == 6 || restrained == 3;  // Simplified check
}

std::vector<double> LoadCaseHandler::calculateReactionForces(
    const std::vector<std::string>& load_names) const noexcept {
    std::vector<double> reactions;

    double total_fx = 0.0, total_fy = 0.0, total_fz = 0.0;

    for (const auto& name : load_names) {
        auto load = getLoadComponent(name);
        if (load && !load->is_moment) {
            switch (load->direction) {
                case LoadDirection::PositiveX: total_fx += load->magnitude; break;
                case LoadDirection::NegativeX: total_fx -= load->magnitude; break;
                case LoadDirection::PositiveY: total_fy += load->magnitude; break;
                case LoadDirection::NegativeY: total_fy -= load->magnitude; break;
                case LoadDirection::PositiveZ: total_fz += load->magnitude; break;
                case LoadDirection::NegativeZ: total_fz -= load->magnitude; break;
            }
        }
    }

    reactions = {-total_fx, -total_fy, -total_fz};
    return reactions;
}

double LoadCaseHandler::calculateTotalAppliedLoad(const std::vector<std::string>& load_names) const noexcept {
    double total = 0.0;

    for (const auto& name : load_names) {
        auto load = getLoadComponent(name);
        if (load) {
            total += std::abs(load->magnitude);
        }
    }

    return total;
}

LoadCaseResult LoadCaseHandler::getWorstCase(const std::vector<LoadCaseResult>& results) const noexcept {
    if (results.empty()) {
        return LoadCaseResult();
    }

    return *std::max_element(results.begin(), results.end(),
        [](const LoadCaseResult& a, const LoadCaseResult& b) {
            return a.max_stress < b.max_stress;
        });
}

std::vector<std::pair<double, double>> LoadCaseHandler::getStressEnvelope(
    const std::vector<LoadCaseResult>& results) const noexcept {
    std::vector<std::pair<double, double>> envelope;

    if (results.empty()) return envelope;

    double min_stress = results[0].max_stress;
    double max_stress = results[0].max_stress;

    for (const auto& result : results) {
        min_stress = std::min(min_stress, result.max_stress);
        max_stress = std::max(max_stress, result.max_stress);
    }

    envelope.push_back({min_stress, max_stress});
    return envelope;
}

bool LoadCaseHandler::isSystemOverdetermined() const noexcept {
    int dof_restrained = countRestrainedDOF();
    return dof_restrained > 6;  // More than 6 DOF restrained
}

bool LoadCaseHandler::hasValidLoadPath(const std::vector<std::string>& load_names) const noexcept {
    if (load_names.empty() || boundary_conditions_.empty()) {
        return false;
    }

    // Check that there's at least one valid load and boundary condition
    bool has_load = false;
    for (const auto& name : load_names) {
        if (loads_.find(name) != loads_.end()) {
            has_load = true;
            break;
        }
    }

    return has_load;
}

double LoadCaseHandler::calculateEquivalentStress(double fx, double fy, double fz) const noexcept {
    // Von Mises equivalent stress (simplified)
    return std::sqrt(fx * fx + fy * fy + fz * fz) / 1000.0;  // Normalize
}

int LoadCaseHandler::countRestrainedDOF() const noexcept {
    int count = 0;

    for (const auto& [name, bc] : boundary_conditions_) {
        if (bc.restrain_x) count++;
        if (bc.restrain_y) count++;
        if (bc.restrain_z) count++;
        if (bc.restrain_rx) count++;
        if (bc.restrain_ry) count++;
        if (bc.restrain_rz) count++;
    }

    return count;
}

bool LoadCaseHandler::checkStaticEquilibrium(const std::vector<LoadComponent>& loads) const noexcept {
    double sum_fx = 0.0, sum_fy = 0.0, sum_fz = 0.0;
    double sum_mx = 0.0, sum_my = 0.0, sum_mz = 0.0;

    for (const auto& load : loads) {
        if (!load.is_moment) {
            switch (load.direction) {
                case LoadDirection::PositiveX: sum_fx += load.magnitude; break;
                case LoadDirection::NegativeX: sum_fx -= load.magnitude; break;
                case LoadDirection::PositiveY: sum_fy += load.magnitude; break;
                case LoadDirection::NegativeY: sum_fy -= load.magnitude; break;
                case LoadDirection::PositiveZ: sum_fz += load.magnitude; break;
                case LoadDirection::NegativeZ: sum_fz -= load.magnitude; break;
            }
        }
    }

    // Check if forces are nearly balanced (tolerance 1e-6)
    double tolerance = 1e-6;
    return std::abs(sum_fx) < tolerance && std::abs(sum_fy) < tolerance && std::abs(sum_fz) < tolerance;
}

}  // namespace bendcore::simulation
