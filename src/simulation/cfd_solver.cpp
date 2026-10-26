#include "bendcore/simulation/cfd_solver.hpp"
#include <cmath>
#include <algorithm>
#include <numeric>

namespace BendCore::CadCore::Simulation {

// ==================== CFDMesh Implementation ====================

void CFDMesh::addPoint(const MeshPoint& point) {
    points_.push_back(point);
}

void CFDMesh::addElement(const MeshElement& element) {
    elements_.push_back(element);
}

void CFDMesh::addFace(const MeshFace& face) {
    faces_.push_back(face);
}

void CFDMesh::clear() {
    points_.clear();
    elements_.clear();
    faces_.clear();
}

double CFDMesh::getTotalVolume() const {
    double total = 0.0;
    for (const auto& elem : elements_) {
        total += elem.volume;
    }
    return total;
}

// ==================== CFDSolver Implementation ====================

CFDSolver::CFDSolver()
    : fluid_density_(1.225), fluid_viscosity_(1.81e-5),
      max_iterations_(1000), relaxation_factor_(0.7),
      convergence_tolerance_(1e-6), converged_(false),
      residual_(1.0), iteration_count_(0) {}

void CFDSolver::setTurbulenceModel(TurbulenceModel model) {
    turbulence_model_ = model;
}

void CFDSolver::setFluidDensity(double density) {
    fluid_density_ = density;
}

void CFDSolver::setFluidViscosity(double viscosity) {
    fluid_viscosity_ = viscosity;
}

void CFDSolver::setNumberOfIterations(size_t iterations) {
    max_iterations_ = iterations;
}

void CFDSolver::setRelaxationFactor(double factor) {
    relaxation_factor_ = factor;
}

void CFDSolver::setMesh(const std::shared_ptr<CFDMesh>& mesh) {
    mesh_ = mesh;
}

void CFDSolver::addBoundaryCondition(const BoundaryCondition& bc) {
    boundary_conditions_.push_back(bc);
}

bool CFDSolver::solve() {
    if (!mesh_ || mesh_->getNumPoints() == 0) {
        return false;
    }

    initializeFields();

    iteration_count_ = 0;
    while (iteration_count_ < max_iterations_) {
        computeMomentumEquation();
        computePressureCorrection();
        correctVelocity();

        if (turbulence_model_ != TurbulenceModel::LAMINAR) {
            computeTurbulenceFields();
        }

        computeTemperatureField();

        residual_ = computeResidual();

        if (residual_ < convergence_tolerance_) {
            converged_ = true;
            break;
        }

        iteration_count_++;
    }

    return converged_;
}

void CFDSolver::initializeFields() {
    size_t num_points = mesh_->getNumPoints();

    velocity_field_.clear();
    velocity_field_.resize(num_points);

    pressure_field_.clear();
    pressure_field_.resize(num_points, 101325.0);  // 1 atm

    temperature_field_.clear();
    temperature_field_.resize(num_points, 293.15);  // 20°C

    turbulent_ke_field_.clear();
    turbulent_ke_field_.resize(num_points, 1e-6);

    turbulent_diss_field_.clear();
    turbulent_diss_field_.resize(num_points, 1e-9);

    // Apply inlet boundary conditions
    for (const auto& bc : boundary_conditions_) {
        if (bc.type == BoundaryConditionType::DIRICHLET) {
            for (size_t idx : bc.face_indices) {
                if (idx < velocity_field_.size()) {
                    velocity_field_[idx].velocity_x = bc.velocity_x;
                    velocity_field_[idx].velocity_y = bc.velocity_y;
                    velocity_field_[idx].velocity_z = bc.velocity_z;
                    pressure_field_[idx] = bc.pressure;
                    temperature_field_[idx] = bc.temperature;
                }
            }
        }
    }
}

void CFDSolver::computeMomentumEquation() {
    size_t num_points = mesh_->getNumPoints();

    for (size_t i = 0; i < num_points; i++) {
        // Simplified momentum equation (SIMPLE-like)
        // ∇·(ρu⊗u) = -∇p + μ∇²u + f
        double kinematic_viscosity = fluid_viscosity_ / fluid_density_;

        // Gradient approximation (simplified)
        Vector3D grad_u = Vector3D(0, 0, 0);
        Vector3D grad_p = Vector3D(0, 0, 0);

        // Apply Laplacian-like diffusion
        velocity_field_[i].velocity_x *= (1.0 - relaxation_factor_) +
            relaxation_factor_ * (velocity_field_[i].velocity_x - 0.001 * kinematic_viscosity);
        velocity_field_[i].velocity_y *= (1.0 - relaxation_factor_) +
            relaxation_factor_ * (velocity_field_[i].velocity_y - 0.001 * kinematic_viscosity);
        velocity_field_[i].velocity_z *= (1.0 - relaxation_factor_) +
            relaxation_factor_ * (velocity_field_[i].velocity_z - 0.001 * kinematic_viscosity);
    }
}

void CFDSolver::computePressureCorrection() {
    // Simplified pressure correction (Poisson equation approximation)
    size_t num_points = mesh_->getNumPoints();

    for (size_t i = 0; i < num_points; i++) {
        // Continuity equation: ∇·u = 0 (incompressible)
        // Adjust pressure slightly based on velocity divergence
        double vel_mag = std::sqrt(
            velocity_field_[i].velocity_x * velocity_field_[i].velocity_x +
            velocity_field_[i].velocity_y * velocity_field_[i].velocity_y +
            velocity_field_[i].velocity_z * velocity_field_[i].velocity_z
        );

        // Pressure adjustment
        pressure_field_[i] += 0.5 * fluid_density_ * vel_mag * vel_mag * 0.001;
    }
}

void CFDSolver::correctVelocity() {
    size_t num_points = mesh_->getNumPoints();

    // Velocity correction based on pressure gradient
    for (size_t i = 0; i < num_points; i++) {
        // u_corrected = u_predicted - (1/ρ) * ∇p_correction
        // Simplified: apply small correction
        velocity_field_[i].velocity_x -= 0.0001 * (pressure_field_[i] - 101325.0);
        velocity_field_[i].velocity_y -= 0.0001 * (pressure_field_[i] - 101325.0);
        velocity_field_[i].velocity_z -= 0.0001 * (pressure_field_[i] - 101325.0);
    }
}

void CFDSolver::computeTurbulenceFields() {
    if (turbulence_model_ == TurbulenceModel::LAMINAR) {
        return;
    }

    size_t num_points = mesh_->getNumPoints();

    // k-epsilon turbulence model (simplified)
    for (size_t i = 0; i < num_points; i++) {
        double vel_mag = std::sqrt(
            velocity_field_[i].velocity_x * velocity_field_[i].velocity_x +
            velocity_field_[i].velocity_y * velocity_field_[i].velocity_y +
            velocity_field_[i].velocity_z * velocity_field_[i].velocity_z
        );

        // Production term
        double production = 0.1 * vel_mag * vel_mag;

        // Dissipation term
        double dissipation = 0.01;

        // Transport equations (simplified forward Euler)
        turbulent_ke_field_[i] += (production - dissipation) * 0.01;
        turbulent_diss_field_[i] += (0.1 * production - 0.1 * dissipation) * 0.01;

        // Keep fields positive
        turbulent_ke_field_[i] = std::max(turbulent_ke_field_[i], 1e-10);
        turbulent_diss_field_[i] = std::max(turbulent_diss_field_[i], 1e-12);

        // Update flow field
        velocity_field_[i].turbulent_kinetic_energy = turbulent_ke_field_[i];
        velocity_field_[i].turbulent_dissipation = turbulent_diss_field_[i];
    }
}

void CFDSolver::computeTemperatureField() {
    size_t num_points = mesh_->getNumPoints();

    // Energy equation (simplified)
    const double specific_heat = 1006.0;  // J/(kg·K) for air
    const double thermal_conductivity = 0.026;  // W/(m·K)

    for (size_t i = 0; i < num_points; i++) {
        // Heat transfer due to convection
        double vel_mag = std::sqrt(
            velocity_field_[i].velocity_x * velocity_field_[i].velocity_x +
            velocity_field_[i].velocity_y * velocity_field_[i].velocity_y +
            velocity_field_[i].velocity_z * velocity_field_[i].velocity_z
        );

        // Simplified convective cooling (towards ambient)
        double ambient_temp = 293.15;
        double heat_transfer_coeff = 10.0;  // W/(m^2·K)
        double temp_correction = heat_transfer_coeff * (ambient_temp - temperature_field_[i]) * 0.001;

        temperature_field_[i] += temp_correction;

        velocity_field_[i].temperature = temperature_field_[i];
    }
}

double CFDSolver::computeResidual() {
    double total_residual = 0.0;

    for (const auto& field : velocity_field_) {
        double vel_residual = std::sqrt(
            field.velocity_x * field.velocity_x +
            field.velocity_y * field.velocity_y +
            field.velocity_z * field.velocity_z
        );
        total_residual += vel_residual;
    }

    if (!velocity_field_.empty()) {
        return total_residual / velocity_field_.size();
    }
    return 0.0;
}

double CFDSolver::getMaxVelocity() const {
    double max_vel = 0.0;
    for (const auto& field : velocity_field_) {
        double vel = std::sqrt(
            field.velocity_x * field.velocity_x +
            field.velocity_y * field.velocity_y +
            field.velocity_z * field.velocity_z
        );
        max_vel = std::max(max_vel, vel);
    }
    return max_vel;
}

double CFDSolver::getMaxPressure() const {
    return *std::max_element(pressure_field_.begin(), pressure_field_.end());
}

double CFDSolver::getMaxTemperature() const {
    return *std::max_element(temperature_field_.begin(), temperature_field_.end());
}

double CFDSolver::getMinTemperature() const {
    return *std::min_element(temperature_field_.begin(), temperature_field_.end());
}

}  // namespace BendCore::CadCore::Simulation
