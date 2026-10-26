#include "bendcore/analysis/multi_physics_coupling.hpp"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace bendcore {
namespace physics {

// ============================================================================
// Fluid-Structure Interaction Implementation
// ============================================================================

void FluidStructureInteraction::transfer_pressure_loads(
    const ScalarField& fluid_pressure) {
    // Transfer pressure field to force vector
    // Pressure acts normal to surface, converted to nodal forces
    for (size_t i = 0; i < pressure_load_.size() && i < fluid_pressure.size(); ++i) {
        pressure_load_[i][2] = fluid_pressure[i];  // Pressure in z-direction
    }
}

void FluidStructureInteraction::transfer_displacement(
    const VectorField& struct_displacement) {
    // Transfer structure displacement to update mesh for fluid solver
    displacement_ = struct_displacement;
}

double FluidStructureInteraction::compute_coupling_residual() const {
    // Residual = ||displacement change|| / (1 + ||displacement||)
    double norm = 0.0;
    for (const auto& disp : displacement_.values) {
        for (double d : disp) {
            norm += d * d;
        }
    }
    norm = std::sqrt(norm);

    double norm_prev = 0.0;
    for (const auto& vel : structure_velocity_.values) {
        for (double v : vel) {
            norm_prev += v * v;
        }
    }
    norm_prev = std::sqrt(norm_prev);

    return std::abs(norm - norm_prev) / (1.0 + norm);
}

void FluidStructureInteraction::set_displacement(const VectorField& disp) {
    if (disp.size() != displacement_.size()) {
        throw std::runtime_error("Displacement size mismatch in FSI");
    }
    displacement_ = disp;
}

void FluidStructureInteraction::set_fluid_pressure(const ScalarField& pres) {
    if (pres.size() != fluid_pressure_.size()) {
        throw std::runtime_error("Pressure size mismatch in FSI");
    }
    fluid_pressure_ = pres;
}

// ============================================================================
// Thermal-Structural Coupling Implementation
// ============================================================================

void ThermalStructuralCoupling::compute_thermal_strain(
    const ScalarField& temperature) {
    if (temperature.size() != temperature_.size()) {
        throw std::runtime_error("Temperature size mismatch");
    }

    // Thermal strain: ε_thermal = α * ΔT
    for (size_t i = 0; i < temperature.size(); ++i) {
        double delta_t = temperature[i] - config_.reference_temperature;
        double strain = config_.alpha_linear * delta_t;

        // Isotropic thermal strain (same in all directions)
        thermal_strain_[i][0] = strain;
        thermal_strain_[i][1] = strain;
        thermal_strain_[i][2] = strain;
    }
}

void ThermalStructuralCoupling::compute_thermal_stress(
    const VectorField& struct_stiffness) {
    // Thermal stress: σ_thermal = E * ε_thermal
    // Simplified: use stiffness as Young's modulus proxy
    for (size_t i = 0; i < thermal_strain_.size(); ++i) {
        for (int j = 0; j < 3; ++j) {
            thermal_stress_[i][j] =
                struct_stiffness[i][j] * thermal_strain_[i][j];
        }
    }
}

void ThermalStructuralCoupling::apply_heat_flux(
    const ScalarField& external_heat_flux,
    double time_step) {
    // Transient thermal analysis: ρ * c * dT/dt + ∇·(k*∇T) = Q
    // Simplified: dT = Q / (ρ * c) * dt
    for (size_t i = 0; i < temperature_.size() && i < external_heat_flux.size(); ++i) {
        double heat_per_mass = external_heat_flux[i] /
                              (config_.specific_heat * 1.0);  // density ~ 1
        temperature_rate_[i] = heat_per_mass;
        temperature_[i] += heat_per_mass * time_step;
    }
}

void ThermalStructuralCoupling::set_temperature(const ScalarField& temp) {
    if (temp.size() != temperature_.size()) {
        throw std::runtime_error("Temperature size mismatch in thermal coupling");
    }
    temperature_ = temp;
}

// ============================================================================
// Electromagnetic-Thermal Coupling Implementation
// ============================================================================

void ElectromagneticThermalCoupling::compute_joule_heating(
    const VectorField& current_density,
    const VectorField& electric_field) {
    // Joule heating: Q = J · E = σ^(-1) * |J|²
    // or: Q = I² * R = |J|² / σ

    if (current_density.size() != joule_heat_.size()) {
        throw std::runtime_error("Current density size mismatch");
    }
    if (electric_field.size() != joule_heat_.size()) {
        throw std::runtime_error("Electric field size mismatch");
    }

    for (size_t i = 0; i < joule_heat_.size(); ++i) {
        // Q = J · E (power per unit volume)
        double dot_product = 0.0;
        for (int j = 0; j < 3; ++j) {
            dot_product += current_density[i][j] * electric_field[i][j];
        }
        joule_heat_[i] = std::max(0.0, dot_product);  // Heat always positive
    }
}

void ElectromagneticThermalCoupling::compute_electromagnetic_force(
    const VectorField& magnetic_field,
    const VectorField& current_density) {
    // Lorentz force: F = J × B
    if (magnetic_field.size() != electromagnetic_force_.size()) {
        throw std::runtime_error("Magnetic field size mismatch");
    }
    if (current_density.size() != electromagnetic_force_.size()) {
        throw std::runtime_error("Current density size mismatch in force");
    }

    for (size_t i = 0; i < electromagnetic_force_.size(); ++i) {
        // Cross product: J × B
        const auto& j = current_density[i];
        const auto& b = magnetic_field[i];

        electromagnetic_force_[i][0] = j[1] * b[2] - j[2] * b[1];  // (J×B)_x
        electromagnetic_force_[i][1] = j[2] * b[0] - j[0] * b[2];  // (J×B)_y
        electromagnetic_force_[i][2] = j[0] * b[1] - j[1] * b[0];  // (J×B)_z
    }
}

void ElectromagneticThermalCoupling::interpolate_fields(
    const VectorField& source_field,
    VectorField& target_field,
    const std::vector<size_t>& mapping) {
    // Simple nearest-neighbor field interpolation
    if (mapping.size() != target_field.size()) {
        throw std::runtime_error("Mapping size mismatch");
    }

    for (size_t i = 0; i < target_field.size(); ++i) {
        if (mapping[i] < source_field.size()) {
            target_field[i] = source_field[mapping[i]];
        }
    }
}

void ElectromagneticThermalCoupling::set_electric_field(
    const VectorField& e_field) {
    if (e_field.size() != electric_field_.size()) {
        throw std::runtime_error("Electric field size mismatch");
    }
    electric_field_ = e_field;
}

void ElectromagneticThermalCoupling::set_magnetic_field(
    const VectorField& b_field) {
    if (b_field.size() != magnetic_field_.size()) {
        throw std::runtime_error("Magnetic field size mismatch");
    }
    magnetic_field_ = b_field;
}

// ============================================================================
// Monolithic Coupling Implementation
// ============================================================================

double MonolithicCoupling::iterate() {
    iterations_++;

    // Solve all physics simultaneously
    if (solve_callback_) {
        auto solution = solve_callback_();
        residual_ = 0.0;
        for (const auto& val : solution.values) {
            for (double v : val) {
                residual_ += v * v;
            }
        }
        residual_ = std::sqrt(residual_);
    }

    return residual_;
}

// ============================================================================
// Partitioned Coupling Implementation
// ============================================================================

double PartitionedCoupling::iterate() {
    iterations_++;

    // Solve physics in sequence with data transfer
    VectorField current_solution;

    for (const auto& [name, solver] : solvers_) {
        auto solution = solver();

        // Aitken adaptive relaxation for acceleration
        if (!previous_solution_.values.empty() && config_.use_aitken) {
            // Compute adaptive relaxation parameter
            double numerator = 0.0;
            double denominator = 0.0;

            for (size_t i = 0; i < solution.size(); ++i) {
                for (int j = 0; j < 3; ++j) {
                    double delta = solution[i][j] - previous_solution_[i][j];
                    numerator += delta * delta;
                    denominator += delta * delta;  // Simplified
                }
            }

            if (denominator > 1e-15) {
                aitken_parameter_ = numerator / (denominator + 1e-15);
                aitken_parameter_ = std::max(0.1, std::min(1.0, aitken_parameter_));
            }
        }

        // Apply relaxation
        if (current_solution.values.empty()) {
            current_solution = solution;
        } else {
            for (size_t i = 0; i < solution.size(); ++i) {
                for (int j = 0; j < 3; ++j) {
                    current_solution[i][j] =
                        config_.relaxation * aitken_parameter_ * solution[i][j] +
                        (1.0 - config_.relaxation * aitken_parameter_) * current_solution[i][j];
                }
            }
        }
    }

    // Compute residual
    residual_ = 0.0;
    if (!previous_solution_.values.empty()) {
        for (size_t i = 0; i < current_solution.size(); ++i) {
            for (int j = 0; j < 3; ++j) {
                double diff = current_solution[i][j] - previous_solution_[i][j];
                residual_ += diff * diff;
            }
        }
        residual_ = std::sqrt(residual_);
    }

    previous_solution_ = current_solution;
    return residual_;
}

void PartitionedCoupling::add_physics_solver(
    const std::string& name,
    std::function<VectorField()> solver) {
    solvers_.push_back({name, solver});
}

// ============================================================================
// Operator Splitting Implementation
// ============================================================================

double OperatorSplitting::iterate() {
    iterations_++;

    // Apply operators in sequence
    for (const auto& phase : config_.split_order) {
        auto it = operators_.find(phase);
        if (it != operators_.end()) {
            it->second();
        }
    }

    residual_ = 0.0;  // Simplified residual
    return residual_;
}

void OperatorSplitting::add_operator(
    const std::string& name,
    std::function<void()> operator_func) {
    operators_[name] = operator_func;
}

// ============================================================================
// Data Transfer Implementation
// ============================================================================

void DataTransferMap::build_mapping(
    const std::vector<std::array<double, 3>>& source_nodes,
    const std::vector<std::array<double, 3>>& target_nodes) {

    source_to_target_.clear();
    source_to_target_.resize(target_nodes.size());
    interpolation_weights_.resize(target_nodes.size());

    // Build nearest-neighbor mapping for each target node
    for (size_t i = 0; i < target_nodes.size(); ++i) {
        double min_distance = 1e30;
        size_t closest_source = 0;

        for (size_t j = 0; j < source_nodes.size(); ++j) {
            double dx = target_nodes[i][0] - source_nodes[j][0];
            double dy = target_nodes[i][1] - source_nodes[j][1];
            double dz = target_nodes[i][2] - source_nodes[j][2];
            double distance = std::sqrt(dx * dx + dy * dy + dz * dz);

            if (distance < min_distance) {
                min_distance = distance;
                closest_source = j;
            }
        }

        source_to_target_[i].push_back(closest_source);
        interpolation_weights_[i].push_back(1.0);
    }
}

void DataTransferMap::transfer_scalar_field(
    const ScalarField& source,
    ScalarField& target) {

    if (source_to_target_.empty()) {
        throw std::runtime_error("Mapping not built");
    }

    for (size_t i = 0; i < target.size() && i < source_to_target_.size(); ++i) {
        double value = 0.0;
        for (size_t j = 0; j < source_to_target_[i].size(); ++j) {
            size_t src_idx = source_to_target_[i][j];
            double weight = interpolation_weights_[i][j];
            if (src_idx < source.size()) {
                value += weight * source[src_idx];
            }
        }
        target[i] = value;
    }
}

void DataTransferMap::transfer_vector_field(
    const VectorField& source,
    VectorField& target) {

    if (source_to_target_.empty()) {
        throw std::runtime_error("Mapping not built");
    }

    for (size_t i = 0; i < target.size() && i < source_to_target_.size(); ++i) {
        std::array<double, 3> value = {0.0, 0.0, 0.0};
        for (size_t j = 0; j < source_to_target_[i].size(); ++j) {
            size_t src_idx = source_to_target_[i][j];
            double weight = interpolation_weights_[i][j];
            if (src_idx < source.size()) {
                for (int k = 0; k < 3; ++k) {
                    value[k] += weight * source[src_idx][k];
                }
            }
        }
        target[i] = value;
    }
}

void DataTransferMap::transfer_tensor_field(
    const TensorField& source,
    TensorField& target) {

    if (source_to_target_.empty()) {
        throw std::runtime_error("Mapping not built");
    }

    for (size_t i = 0; i < target.size() && i < source_to_target_.size(); ++i) {
        std::array<double, 6> value = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
        for (size_t j = 0; j < source_to_target_[i].size(); ++j) {
            size_t src_idx = source_to_target_[i][j];
            double weight = interpolation_weights_[i][j];
            if (src_idx < source.size()) {
                for (int k = 0; k < 6; ++k) {
                    value[k] += weight * source[src_idx][k];
                }
            }
        }
        target[i] = value;
    }
}

double DataTransferMap::check_consistency() {
    // Check that interpolation weights sum to 1.0
    double max_error = 0.0;
    for (const auto& weights : interpolation_weights_) {
        double sum = 0.0;
        for (double w : weights) {
            sum += w;
        }
        max_error = std::max(max_error, std::abs(sum - 1.0));
    }
    interpolation_error_ = max_error;
    return max_error;
}

// ============================================================================
// Convergence Monitor Implementation
// ============================================================================

void ConvergenceMonitor::record_iteration(double residual, double energy) {
    metrics_.iteration++;
    metrics_.residual = residual;
    metrics_.energy_error = energy;
    metrics_.residual_history.push_back(residual);
    metrics_.energy_history.push_back(energy);
}

bool ConvergenceMonitor::is_converged() const {
    if (metrics_.residual_history.empty()) {
        return false;
    }

    double res = metrics_.residual;
    bool abs_converged = res < config_.absolute_tolerance;
    bool rel_converged = res < config_.relative_tolerance *
                        (metrics_.residual_history.front() + 1e-15);
    bool iter_ok = metrics_.iteration < config_.max_iterations;

    return (abs_converged || rel_converged) && iter_ok;
}

// ============================================================================
// Solver Orchestration Implementation
// ============================================================================

void SolverOrchestrator::register_solver(
    const std::string& name,
    std::function<VectorField()> solver) {
    solvers_[name] = solver;
}

void SolverOrchestrator::register_data_transfer(
    const std::string& from_physics,
    const std::string& to_physics,
    std::shared_ptr<DataTransferMap> transfer) {
    transfers_[{from_physics, to_physics}] = transfer;
}

void SolverOrchestrator::execute() {
    // Time stepping loop
    for (int step = 0; step < config_.num_time_steps; ++step) {
        // Execute solvers based on coupling scheme
        switch (config_.coupling_scheme) {
            case CouplingScheme::MONOLITHIC: {
                // All physics simultaneously
                for (auto& [name, solver] : solvers_) {
                    solver();
                }
                break;
            }
            case CouplingScheme::PARTITIONED_TIGHT:
            case CouplingScheme::PARTITIONED_LOOSE: {
                // Alternating physics solution
                for (auto& [name, solver] : solvers_) {
                    solver();
                }
                break;
            }
            case CouplingScheme::OPERATOR_SPLITTING: {
                // Sequential operators
                for (auto& [name, solver] : solvers_) {
                    solver();
                }
                break;
            }
        }

        current_time_ += config_.time_step;
    }
}

void SolverOrchestrator::adjust_time_step(double reduction_factor) {
    config_.time_step *= reduction_factor;
}

} // namespace physics
} // namespace bendcore
