#include "bendcore/simulation/thermal_solver.hpp"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <limits>
#include <utility>
#include <Eigen/Dense>
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

// Conduction neighbours of node i: nodes within kNeighbourFactor times the
// distance from i to its nearest node (union over both ends). On a regular
// grid with spacing h this keeps exactly the face neighbours, so the graph
// operator sum_j k (T_j - T_i) / d_ij^2 is the standard 3/5/7-point Laplacian.
constexpr double kNeighbourFactor = 1.2;
constexpr double kStefanBoltzmann = 5.67e-8;  // W/(m^2*K^4)

}  // namespace

void ThermalSolver::buildNeighbours() {
    const size_t n = num_nodes_;
    std::vector<std::vector<size_t>> adj(n);
    for (size_t i = 0; i < n; ++i) {
        double h = std::numeric_limits<double>::max();
        for (size_t j = 0; j < n; ++j) {
            if (j == i) continue;
            const double d = (node_coordinates_[j] - node_coordinates_[i]).magnitude();
            if (d > 1e-12) h = std::min(h, d);
        }
        for (size_t j = 0; j < n; ++j) {
            if (j == i) continue;
            const double d = (node_coordinates_[j] - node_coordinates_[i]).magnitude();
            if (d > 1e-12 && d <= kNeighbourFactor * h) {
                adj[i].push_back(j);
                adj[j].push_back(i);
            }
        }
    }
    neighbours_.assign(n, {});
    for (size_t i = 0; i < n; ++i) {
        std::sort(adj[i].begin(), adj[i].end());
        adj[i].erase(std::unique(adj[i].begin(), adj[i].end()), adj[i].end());
        for (size_t j : adj[i]) {
            const double d = (node_coordinates_[j] - node_coordinates_[i]).magnitude();
            neighbours_[i].push_back({j, d});
        }
    }
}

bool ThermalSolver::solveLinearStep(double capacity_over_dt, const std::vector<double>& t_old) {
    // Per unit volume, for every node without a fixed temperature:
    //   C/dt (T_i - T_i_old) = sum_j k (T_j - T_i) / d_ij^2
    //                          + q_flux + q_source - h (T_i - T_a) - h_r (T_i - T_a)
    // C/dt = 0 gives the steady equation. Radiation is linearised around the
    // current iterate: h_r = e*s*(T^2 + T_a^2)(T + T_a).
    const size_t n = num_nodes_;
    const double k = material_.thermal_conductivity;

    std::vector<bool> fixed(n, false);
    std::vector<double> fixed_value(n, 0.0);
    for (const auto& bc : boundary_conditions_) {
        if (bc.type != ThermalBCType::TEMPERATURE) continue;
        for (size_t idx : bc.face_indices) {
            if (idx < n) { fixed[idx] = true; fixed_value[idx] = bc.temperature; }
        }
    }

    std::vector<Eigen::Triplet<double>> triplets;
    Eigen::VectorXd rhs = Eigen::VectorXd::Zero(static_cast<Eigen::Index>(n));
    std::vector<double> diag(n, 0.0);
    for (size_t i = 0; i < n; ++i) {
        if (fixed[i]) continue;
        diag[i] += capacity_over_dt;
        rhs[static_cast<Eigen::Index>(i)] += capacity_over_dt * t_old[i];
        for (const auto& [j, dist] : neighbours_[i]) {
            const double g = k / (dist * dist);
            diag[i] += g;
            if (fixed[j]) {
                rhs[static_cast<Eigen::Index>(i)] += g * fixed_value[j];
            } else {
                triplets.emplace_back(i, j, -g);
            }
        }
    }
    for (const auto& bc : boundary_conditions_) {
        for (size_t idx : bc.face_indices) {
            if (idx >= n || fixed[idx]) continue;
            const auto r = static_cast<Eigen::Index>(idx);
            switch (bc.type) {
                case ThermalBCType::HEAT_FLUX:
                    rhs[r] += bc.heat_flux;
                    break;
                case ThermalBCType::CONVECTION:
                    diag[idx] += bc.convection_coefficient;
                    rhs[r] += bc.convection_coefficient * bc.ambient_temperature;
                    break;
                case ThermalBCType::RADIATION: {
                    const double T = temperature_field_[idx];
                    const double Ta = bc.ambient_temperature;
                    const double h_r = material_.emissivity * kStefanBoltzmann *
                                       (T * T + Ta * Ta) * (T + Ta);
                    diag[idx] += h_r;
                    rhs[r] += h_r * Ta;
                    break;
                }
                default:
                    break;
            }
        }
    }
    for (const auto& source : heat_sources_) {
        if (source.element_id < n && !fixed[source.element_id]) {
            rhs[static_cast<Eigen::Index>(source.element_id)] += source.heat_generation;
        }
    }
    for (size_t i = 0; i < n; ++i) {
        if (fixed[i]) {
            triplets.emplace_back(i, i, 1.0);
            rhs[static_cast<Eigen::Index>(i)] = fixed_value[i];
        } else {
            triplets.emplace_back(i, i, diag[i]);
        }
    }

    Eigen::SparseMatrix<double> K(static_cast<Eigen::Index>(n), static_cast<Eigen::Index>(n));
    K.setFromTriplets(triplets.begin(), triplets.end());
    Eigen::SparseLU<Eigen::SparseMatrix<double>> lu;
    lu.compute(K);
    if (lu.info() != Eigen::Success) {
        return false;  // singular: nothing anchors the temperature level
    }
    Eigen::VectorXd T = lu.solve(rhs);
    if (lu.info() != Eigen::Success || !T.allFinite()) {
        return false;
    }
    for (size_t i = 0; i < n; ++i) temperature_field_[i] = T[static_cast<Eigen::Index>(i)];
    return true;
}

bool ThermalSolver::solve() {
    if (transient_mode_) {
        return solveTransient();
    }

    converged_ = false;
    if (num_nodes_ == 0) {
        return false;
    }
    initializeTemperatureField();
    buildNeighbours();

    bool has_radiation = false;
    for (const auto& bc : boundary_conditions_) {
        has_radiation = has_radiation || bc.type == ThermalBCType::RADIATION;
    }
    // Radiation makes the problem non-linear: Picard iterations on h_r.
    const int max_picard = has_radiation ? 100 : 1;
    iteration_count_ = 0;
    for (int picard = 0; picard < max_picard; ++picard) {
        temperature_field_previous_ = temperature_field_;
        if (!solveLinearStep(0.0, temperature_field_previous_)) {
            updateResults();
            return false;
        }
        ++iteration_count_;
        if (!has_radiation || computeResidual() < convergence_tolerance_) {
            converged_ = true;
            break;
        }
    }

    updateResults();
    return converged_;
}

bool ThermalSolver::solveTransient() {
    converged_ = false;
    elapsed_time_ = 0.0;
    iteration_count_ = 0;
    if (num_nodes_ == 0 || !(time_step_ > 0.0) || total_time_ < 0.0) {
        return false;
    }
    initializeTemperatureField();
    buildNeighbours();

    // Backward (implicit) Euler: unconditionally stable, first order in dt.
    // Radiation is linearised at the start of each step (no inner iterations).
    const double capacity = material_.density * material_.specific_heat_capacity;
    if (!(capacity > 0.0)) {
        return false;
    }
    const auto steps = static_cast<size_t>(std::ceil(total_time_ / time_step_ - 1e-9));
    for (size_t step = 0; step < steps; ++step) {
        const double dt = std::min(time_step_, total_time_ - elapsed_time_);
        temperature_field_previous_ = temperature_field_;
        if (!solveLinearStep(capacity / dt, temperature_field_previous_)) {
            updateResults();
            return false;
        }
        elapsed_time_ += dt;
        ++iteration_count_;
    }

    converged_ = true;
    updateResults();
    return true;
}

void ThermalSolver::initializeTemperatureField() {
    temperature_field_.assign(num_nodes_, initial_temperature_);
    temperature_field_previous_.assign(num_nodes_, initial_temperature_);

    // Dirichlet boundary conditions
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

void ThermalSolver::updateResults() {
    results_.clear();

    max_temperature_ = 0.0;
    min_temperature_ = std::numeric_limits<double>::max();

    for (size_t i = 0; i < num_nodes_ && i < temperature_field_.size(); i++) {
        ThermalResults result;
        result.node_id = i;
        result.position = node_coordinates_[i];
        result.temperature = temperature_field_[i];

        // Fourier's law q = -k grad T with a weighted least-squares nodal gradient
        // (weights 1/d^2); rank-deficient neighbourhoods (1-D / 2-D clouds) are
        // handled by a complete orthogonal decomposition.
        if (i < neighbours_.size() && !neighbours_[i].empty()) {
            Eigen::Matrix3d A = Eigen::Matrix3d::Zero();
            Eigen::Vector3d b = Eigen::Vector3d::Zero();
            for (const auto& [j, dist] : neighbours_[i]) {
                const Vector3D d = node_coordinates_[j] - node_coordinates_[i];
                const Eigen::Vector3d dx(d.x, d.y, d.z);
                const double w = 1.0 / (dist * dist);
                A += w * dx * dx.transpose();
                b += w * dx * (temperature_field_[j] - temperature_field_[i]);
            }
            const Eigen::Vector3d g = A.completeOrthogonalDecomposition().solve(b);
            const double k = material_.thermal_conductivity;
            result.heat_flux = Vector3D(-k * g.x(), -k * g.y(), -k * g.z());
        }

        results_.push_back(result);

        max_temperature_ = std::max(max_temperature_, result.temperature);
        min_temperature_ = std::min(min_temperature_, result.temperature);
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
