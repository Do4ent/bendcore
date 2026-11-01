#include "bendcore/simulation/thermal_solver.hpp"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <Eigen/Sparse>
#include <Eigen/SparseLU>

namespace BendCore::CadCore::Simulation {

// ==================== ThermalSolver ====================

ThermalSolver::ThermalSolver()
    : num_nodes_(0), time_step_(1.0), total_time_(10.0),
      elapsed_time_(0.0), transient_mode_(false),
      initial_temperature_(293.15), converged_(false),
      iteration_count_(0), max_temperature_(0.0),
      min_temperature_(0.0), convergence_tolerance_(1e-6) {

    // Default material: Steel
    material_.name = "Steel";
    material_.thermal_conductivity = 50.0;      // W/(m·K)
    material_.specific_heat_capacity = 470.0;   // J/(kg·K)
    material_.density = 7850.0;                 // kg/m^3
    material_.thermal_expansion = 1.2e-5;       // 1/K
    material_.emissivity = 0.8;
}

void ThermalSolver::setMaterialProperties(const ThermalMaterialProperties& props) {
    material_ = props;
}

void ThermalSolver::setTimeStep(double dt) {
    time_step_ = dt;
}

void ThermalSolver::setTotalTime(double total_time) {
    total_time_ = total_time;
}

void ThermalSolver::setTransientMode(bool transient) {
    transient_mode_ = transient;
}

void ThermalSolver::setNumNodes(size_t num_nodes) {
    num_nodes_ = num_nodes;
    node_coordinates_.resize(num_nodes);
    temperature_field_.resize(num_nodes);
    temperature_field_previous_.resize(num_nodes);
}

void ThermalSolver::addNodeCoordinates(size_t node_id, const Vector3D& coords) {
    if (node_id < node_coordinates_.size()) {
        node_coordinates_[node_id] = coords;
    }
}

void ThermalSolver::addBoundaryCondition(const ThermalBoundaryCondition& bc) {
    boundary_conditions_.push_back(bc);
}

void ThermalSolver::setInitialTemperature(double temp) {
    initial_temperature_ = temp;
}

void ThermalSolver::addHeatSource(const HeatSource& source) {
    heat_sources_.push_back(source);
}

namespace {

// Nodes closer than this are treated as conduction neighbours (same rule
// the explicit transient update and the heat-flux post-processing use).
constexpr double kNeighbourRadius = 2.0;
constexpr double kStefanBoltzmann = 5.67e-8;  // W/(m^2*K^4)

}  // namespace

bool ThermalSolver::solve() {
    if (transient_mode_) {
        return solveTransient();
    }

    converged_ = false;
    if (num_nodes_ == 0) {
        return false;
    }

    initializeTemperatureField();

    // Steady-state conduction on the node graph:
    //   sum_j k (T_j - T_i) / d_ij^2 + q_i = 0
    // with Dirichlet (TEMPERATURE), Robin (CONVECTION), linearised
    // RADIATION, and HEAT_FLUX / heat sources on the right-hand side.
    // Radiation is non-linear, so the system is re-solved (Picard)
    // until temperatures stop changing.
    const size_t n = num_nodes_;
    const double k = material_.thermal_conductivity;

    std::vector<bool> fixed(n, false);
    std::vector<double> fixed_value(n, 0.0);
    bool has_radiation = false;
    for (const auto& bc : boundary_conditions_) {
        if (bc.type == ThermalBCType::TEMPERATURE) {
            for (size_t idx : bc.face_indices) {
                if (idx < n) { fixed[idx] = true; fixed_value[idx] = bc.temperature; }
            }
        } else if (bc.type == ThermalBCType::RADIATION) {
            has_radiation = true;
        }
    }

    const int max_picard = has_radiation ? 100 : 1;
    iteration_count_ = 0;
    for (int picard = 0; picard < max_picard; ++picard) {
        temperature_field_previous_ = temperature_field_;

        std::vector<Eigen::Triplet<double>> triplets;
        Eigen::VectorXd rhs = Eigen::VectorXd::Zero(static_cast<Eigen::Index>(n));
        std::vector<double> diag(n, 0.0);

        for (size_t i = 0; i < n; ++i) {
            if (fixed[i]) continue;
            for (size_t j = 0; j < n; ++j) {
                if (i == j) continue;
                double dist = (node_coordinates_[j] - node_coordinates_[i]).magnitude();
                if (dist >= kNeighbourRadius || dist <= 1e-10) continue;
                double g = k / (dist * dist);
                diag[i] += g;
                if (fixed[j]) {
                    rhs[i] += g * fixed_value[j];
                } else {
                    triplets.emplace_back(i, j, -g);
                }
            }
        }

        for (const auto& bc : boundary_conditions_) {
            for (size_t idx : bc.face_indices) {
                if (idx >= n || fixed[idx]) continue;
                switch (bc.type) {
                    case ThermalBCType::HEAT_FLUX:
                        rhs[idx] += bc.heat_flux;
                        break;
                    case ThermalBCType::CONVECTION:
                        diag[idx] += bc.convection_coefficient;
                        rhs[idx] += bc.convection_coefficient * bc.ambient_temperature;
                        break;
                    case ThermalBCType::RADIATION: {
                        // q = e*s*(T^4 - Ta^4) = h_r (T - Ta), h_r from last iterate
                        double T = temperature_field_[idx];
                        double Ta = bc.ambient_temperature;
                        double h_r = material_.emissivity * kStefanBoltzmann *
                                     (T * T + Ta * Ta) * (T + Ta);
                        diag[idx] += h_r;
                        rhs[idx] += h_r * Ta;
                        break;
                    }
                    default:
                        break;
                }
            }
        }
        for (const auto& source : heat_sources_) {
            if (source.element_id < n && !fixed[source.element_id]) {
                rhs[source.element_id] += source.heat_generation;
            }
        }

        for (size_t i = 0; i < n; ++i) {
            if (fixed[i]) {
                triplets.emplace_back(i, i, 1.0);
                rhs[i] = fixed_value[i];
            } else {
                triplets.emplace_back(i, i, diag[i]);
            }
        }

        Eigen::SparseMatrix<double> K(static_cast<Eigen::Index>(n), static_cast<Eigen::Index>(n));
        K.setFromTriplets(triplets.begin(), triplets.end());
        Eigen::SparseLU<Eigen::SparseMatrix<double>> lu;
        lu.compute(K);
        if (lu.info() != Eigen::Success) {
            // Singular: no boundary condition anchors the temperature level
            updateResults();
            return false;
        }
        Eigen::VectorXd T = lu.solve(rhs);
        if (lu.info() != Eigen::Success || !T.allFinite()) {
            updateResults();
            return false;
        }
        for (size_t i = 0; i < n; ++i) temperature_field_[i] = T[static_cast<Eigen::Index>(i)];

        ++iteration_count_;
        if (computeResidual() < convergence_tolerance_ || !has_radiation) {
            converged_ = true;
            break;
        }
    }

    updateResults();
    return converged_;
}

bool ThermalSolver::solveTransient() {
    if (num_nodes_ == 0) {
        return false;
    }

    initializeTemperatureField();

    while (elapsed_time_ < total_time_) {
        temperature_field_previous_ = temperature_field_;

        // Implicit Euler time stepping
        assembleConductivityMatrix();
        assembleHeatCapacityMatrix();
        applyBoundaryConditions();
        solveLinearSystem();

        elapsed_time_ += time_step_;
        iteration_count_++;
    }

    converged_ = true;
    updateResults();
    return true;
}

void ThermalSolver::initializeTemperatureField() {
    std::fill(temperature_field_.begin(), temperature_field_.end(), initial_temperature_);
    std::fill(temperature_field_previous_.begin(), temperature_field_previous_.end(), initial_temperature_);

    // Apply Dirichlet boundary conditions
    for (const auto& bc : boundary_conditions_) {
        if (bc.type == ThermalBCType::TEMPERATURE) {
            for (size_t idx : bc.face_indices) {
                if (idx < temperature_field_.size()) {
                    temperature_field_[idx] = bc.temperature;
                    temperature_field_previous_[idx] = bc.temperature;
                }
            }
        }
    }
}

void ThermalSolver::assembleConductivityMatrix() {
    // Simplified: use finite difference method
    // ∇²T = ∂²T/∂x² + ∂²T/∂y² + ∂²T/∂z²

    for (size_t i = 0; i < num_nodes_; i++) {
        if (i < temperature_field_.size()) {
            // Laplacian approximation (simplified)
            double laplacian = 0.0;

            // Find neighboring nodes (simplified: just use distance-based approximation)
            for (size_t j = 0; j < num_nodes_; j++) {
                if (i == j) continue;

                Vector3D diff = node_coordinates_[j] - node_coordinates_[i];
                double dist = diff.magnitude();

                if (dist < 2.0 && dist > 1e-10) {
                    double temp_diff = temperature_field_[j] - temperature_field_[i];
                    laplacian += temp_diff / (dist * dist);
                }
            }

            // Apply thermal diffusivity: ∂T/∂t = α∇²T
            // where α = k / (ρ*c)
            double thermal_diffusivity = material_.thermal_conductivity /
                (material_.density * material_.specific_heat_capacity);

            temperature_field_[i] += thermal_diffusivity * laplacian * 0.01;
        }
    }
}

void ThermalSolver::assembleHeatCapacityMatrix() {
    // For implicit time stepping
    // (ρ*c/Δt)*T_new + k*∇²T_new = (ρ*c/Δt)*T_old
}

void ThermalSolver::applyBoundaryConditions() {
    for (const auto& bc : boundary_conditions_) {
        switch (bc.type) {
            case ThermalBCType::TEMPERATURE:
                // Dirichlet BC
                for (size_t idx : bc.face_indices) {
                    if (idx < temperature_field_.size()) {
                        temperature_field_[idx] = bc.temperature;
                    }
                }
                break;

            case ThermalBCType::HEAT_FLUX:
                // Neumann BC: -k * ∂T/∂n = q
                // Simplified: add heat to boundary nodes
                for (size_t idx : bc.face_indices) {
                    if (idx < temperature_field_.size()) {
                        double heat_input = bc.heat_flux / (material_.density * material_.specific_heat_capacity);
                        temperature_field_[idx] += heat_input * 0.01;
                    }
                }
                break;

            case ThermalBCType::CONVECTION:
                // h*(T_boundary - T_ambient) = -k * ∂T/∂n
                for (size_t idx : bc.face_indices) {
                    if (idx < temperature_field_.size()) {
                        double heat_transfer = bc.convection_coefficient *
                            (temperature_field_[idx] - bc.ambient_temperature);
                        temperature_field_[idx] -= (heat_transfer / (material_.density * material_.specific_heat_capacity)) * 0.01;
                    }
                }
                break;

            case ThermalBCType::RADIATION:
                // q = ε*σ*(T⁴ - T_ambient⁴)
                for (size_t idx : bc.face_indices) {
                    if (idx < temperature_field_.size()) {
                        const double stefan_boltzmann = 5.67e-8;  // W/(m²·K⁴)
                        double T = temperature_field_[idx];
                        double T_amb = bc.ambient_temperature;
                        double heat_transfer = material_.emissivity * stefan_boltzmann *
                            (T * T * T * T - T_amb * T_amb * T_amb * T_amb);
                        temperature_field_[idx] -= (heat_transfer / (material_.density * material_.specific_heat_capacity)) * 0.01;
                    }
                }
                break;

            case ThermalBCType::ADIABATIC:
                // Do nothing (no heat flow)
                break;
        }
    }

    // Apply heat sources
    for (const auto& source : heat_sources_) {
        if (source.element_id < temperature_field_.size()) {
            double heat_input = source.heat_generation / (material_.density * material_.specific_heat_capacity);
            temperature_field_[source.element_id] += heat_input * 0.01;
        }
    }
}

void ThermalSolver::solveLinearSystem() {
    // Already done in assembleConductivityMatrix with forward Euler
    // In a real solver, this would solve the system K*T = F
}

void ThermalSolver::updateResults() {
    results_.clear();

    max_temperature_ = 0.0;
    min_temperature_ = std::numeric_limits<double>::max();

    for (size_t i = 0; i < num_nodes_; i++) {
        if (i < temperature_field_.size()) {
            ThermalResults result;
            result.node_id = i;
            result.position = node_coordinates_[i];
            result.temperature = temperature_field_[i];

            // Simplified heat flux computation: q = -k*∇T
            Vector3D grad_T = Vector3D(0, 0, 0);
            int neighbor_count = 0;

            for (size_t j = 0; j < num_nodes_; j++) {
                if (i == j) continue;

                Vector3D diff = node_coordinates_[j] - node_coordinates_[i];
                double dist = diff.magnitude();

                if (dist < 2.0 && dist > 1e-10) {
                    double temp_diff = temperature_field_[j] - temperature_field_[i];
                    Vector3D normalized_diff = diff * (1.0 / dist);
                    grad_T = grad_T + normalized_diff * (temp_diff / dist);
                    neighbor_count++;
                }
            }

            if (neighbor_count > 0) {
                grad_T = grad_T * (1.0 / neighbor_count);
                result.heat_flux = grad_T * (-material_.thermal_conductivity);
            }

            results_.push_back(result);

            max_temperature_ = std::max(max_temperature_, result.temperature);
            min_temperature_ = std::min(min_temperature_, result.temperature);
        }
    }

    if (min_temperature_ > 1e9) {
        min_temperature_ = initial_temperature_;
    }
}

double ThermalSolver::computeResidual() {
    double residual = 0.0;

    for (size_t i = 0; i < temperature_field_.size(); i++) {
        double diff = std::abs(temperature_field_[i] - temperature_field_previous_[i]);
        residual += diff;
    }

    if (!temperature_field_.empty()) {
        return residual / temperature_field_.size();
    }
    return 0.0;
}

}  // namespace BendCore::CadCore::Simulation
