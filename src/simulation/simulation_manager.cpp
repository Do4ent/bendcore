#include "bendcore/simulation/simulation_manager.hpp"
#include <algorithm>
#include <iostream>
#include <fstream>

namespace BendCore::CadCore::Simulation {

// ==================== SimulationManager ====================

SimulationManager::SimulationManager()
    : stop_requested_(false), paused_(false) {

    cfd_solver_ = nullptr;
    structural_solver_ = nullptr;
    thermal_solver_ = nullptr;
    visualization_ = std::make_shared<ResultVisualization>();

    progress_.progress_percent = 0.0;
    progress_.current_step = 0;
    progress_.total_steps = 0;
    progress_.status_message = "Initialized";
    progress_.is_running = false;
}

void SimulationManager::setConfiguration(const SimulationConfig& config) {
    config_ = config;
}

std::shared_ptr<CFDSolver> SimulationManager::createCFDSolver() {
    cfd_solver_ = std::make_shared<CFDSolver>();
    return cfd_solver_;
}

std::shared_ptr<StructuralSolver> SimulationManager::createStructuralSolver() {
    structural_solver_ = std::make_shared<StructuralSolver>();
    return structural_solver_;
}

std::shared_ptr<ThermalSolver> SimulationManager::createThermalSolver() {
    thermal_solver_ = std::make_shared<ThermalSolver>();
    return thermal_solver_;
}

bool SimulationManager::runSimulation() {
    if (!validateConfiguration()) {
        return false;
    }

    progress_.is_running = true;
    stop_requested_ = false;
    updateProgress(0.0, "Starting simulation...");

    switch (config_.type) {
        case SimulationType::CFD: {
            if (!cfd_solver_) {
                updateProgress(0.0, "CFD solver not configured");
                return false;
            }
            updateProgress(10.0, "Running CFD solver...");
            if (!cfd_solver_->solve()) {
                updateProgress(0.0, "CFD solver failed to converge");
                progress_.is_running = false;
                return false;
            }
            updateProgress(100.0, "CFD simulation completed");
            break;
        }

        case SimulationType::STRUCTURAL: {
            if (!structural_solver_) {
                updateProgress(0.0, "Structural solver not configured");
                return false;
            }
            updateProgress(10.0, "Running structural solver...");
            if (!structural_solver_->solve()) {
                updateProgress(0.0, "Structural solver failed");
                progress_.is_running = false;
                return false;
            }
            updateProgress(100.0, "Structural simulation completed");
            break;
        }

        case SimulationType::THERMAL: {
            if (!thermal_solver_) {
                updateProgress(0.0, "Thermal solver not configured");
                return false;
            }
            updateProgress(10.0, "Running thermal solver...");
            if (!thermal_solver_->solve()) {
                updateProgress(0.0, "Thermal solver failed");
                progress_.is_running = false;
                return false;
            }
            updateProgress(100.0, "Thermal simulation completed");
            break;
        }

        case SimulationType::MULTI_PHYSICS: {
            // The solvers share no mesh, so no fields are mapped between them: each
            // configured solver runs on its own input (docs/simulation/SIMULATION.md).

            if (cfd_solver_) {
                updateProgress(15.0, "Running CFD solver...");
                if (!cfd_solver_->solve()) {
                    progress_.is_running = false;
                    return false;
                }
            }

            if (thermal_solver_) {
                updateProgress(50.0, "Running thermal solver...");
                if (!thermal_solver_->solve()) {
                    progress_.is_running = false;
                    return false;
                }
            }

            if (structural_solver_) {
                updateProgress(75.0, "Running structural solver...");
                if (!structural_solver_->solve()) {
                    progress_.is_running = false;
                    return false;
                }
            }

            updateProgress(100.0, "Multi-physics simulation completed");
            break;
        }
    }

    postProcessResults();

    progress_.is_running = false;
    return true;
}

void SimulationManager::stopSimulation() {
    stop_requested_ = true;
    progress_.is_running = false;
}

bool SimulationManager::pauseSimulation() {
    paused_ = true;
    progress_.is_running = false;
    return true;
}

bool SimulationManager::resumeSimulation() {
    paused_ = false;
    progress_.is_running = true;
    return true;
}

const PostProcessingResult& SimulationManager::getPostProcessedResult(const std::string& name) const {
    static PostProcessingResult empty;

    for (const auto& result : results_) {
        if (result.quantity_name == name) {
            return result;
        }
    }
    return empty;
}

bool SimulationManager::exportResults(const std::string& filename, const std::string& format) {
    if (format == "csv" || format == "CSV") {
        return exportToCSV(filename);
    }
    return false;
}

bool SimulationManager::exportToCSV(const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        return false;
    }

    file << "Quantity,Min,Max,Average\n";
    for (const auto& result : results_) {
        file << result.quantity_name << ","
             << result.min_value << ","
             << result.max_value << ","
             << result.average_value << "\n";
    }

    file.close();
    return true;
}

bool SimulationManager::validateConfiguration() const {
    // Check that at least one solver is configured
    bool has_solver = cfd_solver_ != nullptr ||
                      structural_solver_ != nullptr ||
                      thermal_solver_ != nullptr;

    return has_solver;
}

std::vector<std::string> SimulationManager::getValidationErrors() const {
    std::vector<std::string> errors;

    if (!validateConfiguration()) {
        errors.push_back("No solver configured");
    }

    if (config_.geometry_id.isNull()) {
        errors.push_back("Geometry ID not set");
    }

    return errors;
}

void SimulationManager::updateProgress(double percent, const std::string& message) {
    progress_.progress_percent = std::min(100.0, std::max(0.0, percent));
    progress_.status_message = message;
    progress_.current_step++;
}

void SimulationManager::postProcessResults() {
    results_.clear();

    // Extract CFD results
    if (cfd_solver_) {
        PostProcessingResult cfd_result;
        cfd_result.quantity_name = "CFD_MaxVelocity";
        cfd_result.max_value = cfd_solver_->getMaxVelocity();
        cfd_result.min_value = 0.0;
        results_.push_back(cfd_result);

        cfd_result.quantity_name = "CFD_MaxPressure";
        cfd_result.max_value = cfd_solver_->getMaxPressure();
        const auto& pressure = cfd_solver_->getPressureField();
        cfd_result.min_value = pressure.empty() ? 0.0 : *std::min_element(pressure.begin(), pressure.end());
        results_.push_back(cfd_result);

        cfd_result.quantity_name = "CFD_MaxTemperature";
        cfd_result.max_value = cfd_solver_->getMaxTemperature();
        cfd_result.min_value = cfd_solver_->getMinTemperature();
        results_.push_back(cfd_result);
    }

    // Extract structural results
    if (structural_solver_) {
        PostProcessingResult struct_result;
        struct_result.quantity_name = "Structural_MaxStress";
        struct_result.max_value = structural_solver_->getMaxStress();
        struct_result.min_value = 0.0;
        results_.push_back(struct_result);

        struct_result.quantity_name = "Structural_MaxDisplacement";
        struct_result.max_value = structural_solver_->getMaxDisplacement();
        struct_result.min_value = 0.0;
        results_.push_back(struct_result);

        struct_result.quantity_name = "Structural_MinSafetyFactor";
        struct_result.min_value = structural_solver_->getMinSafetyFactor();
        struct_result.max_value = 100.0;
        results_.push_back(struct_result);
    }

    // Extract thermal results
    if (thermal_solver_) {
        PostProcessingResult thermal_result;
        thermal_result.quantity_name = "Thermal_MaxTemperature";
        thermal_result.max_value = thermal_solver_->getMaxTemperature();
        thermal_result.min_value = thermal_solver_->getMinTemperature();
        results_.push_back(thermal_result);
    }
}

}  // namespace BendCore::CadCore::Simulation
