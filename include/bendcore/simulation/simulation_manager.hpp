#pragma once

#include <memory>
#include <vector>
#include <string>
#include <map>
#include "cfd_solver.hpp"
#include "structural_solver.hpp"
#include "thermal_solver.hpp"
#include "result_visualization.hpp"
#include "../document/entity_id.hpp"

namespace BendCore::CadCore::Simulation {

/**
 * @brief Simulation type
 */
enum class SimulationType {
    CFD,               // Fluid dynamics
    STRUCTURAL,        // Stress analysis
    THERMAL,           // Heat transfer
    MULTI_PHYSICS,     // Combined analysis
};

/**
 * @brief Coupled physics method
 */
enum class CouplingMethod {
    LOOSE,             // Sequential coupling
    TIGHT,             // Iterative coupling
    MONOLITHIC,        // Fully coupled
};

/**
 * @brief Simulation job configuration
 */
struct SimulationConfig {
    SimulationType type;
    EntityId geometry_id;
    std::string name;
    CouplingMethod coupling = CouplingMethod::LOOSE;
    bool save_intermediate = false;
    size_t max_iterations = 100;
    double convergence_tolerance = 1e-6;
};

/**
 * @brief Simulation progress information
 */
struct ProgressInfo {
    double progress_percent = 0.0;
    size_t current_step = 0;
    size_t total_steps = 0;
    std::string status_message;
    bool is_running = false;
};

/**
 * @brief Result post-processing data
 */
struct PostProcessingResult {
    std::string quantity_name;
    std::vector<double> values;
    double min_value = 0.0;
    double max_value = 0.0;
    double average_value = 0.0;
};

/**
 * @brief Main Simulation Manager
 *
 * Orchestrates multi-physics simulations with:
 * - Solver coordination
 * - Data coupling
 * - Result aggregation
 * - Export capabilities
 */
class SimulationManager {
public:
    SimulationManager();
    ~SimulationManager() = default;

    // Configuration
    void setConfiguration(const SimulationConfig& config);
    [[nodiscard]] const SimulationConfig& getConfiguration() const { return config_; }

    // Solver creation and access
    std::shared_ptr<CFDSolver> createCFDSolver();
    std::shared_ptr<StructuralSolver> createStructuralSolver();
    std::shared_ptr<ThermalSolver> createThermalSolver();

    [[nodiscard]] std::shared_ptr<CFDSolver> getCFDSolver() const { return cfd_solver_; }
    [[nodiscard]] std::shared_ptr<StructuralSolver> getStructuralSolver() const { return structural_solver_; }
    [[nodiscard]] std::shared_ptr<ThermalSolver> getThermalSolver() const { return thermal_solver_; }

    // Simulation execution
    bool runSimulation();
    void stopSimulation();
    bool pauseSimulation();
    bool resumeSimulation();

    // Progress tracking
    [[nodiscard]] const ProgressInfo& getProgress() const { return progress_; }
    [[nodiscard]] bool isRunning() const { return progress_.is_running; }

    // Results access
    [[nodiscard]] const PostProcessingResult& getPostProcessedResult(const std::string& name) const;
    [[nodiscard]] const std::vector<PostProcessingResult>& getAllResults() const { return results_; }

    // Visualization data
    std::shared_ptr<ResultVisualization> getVisualization() { return visualization_; }

    // Export
    bool exportResults(const std::string& filename, const std::string& format);
    bool exportToVTK(const std::string& filename);
    bool exportToCSV(const std::string& filename);

    // Coupling
    bool applyCFDToStructural();
    bool applyThermalToStructural();
    bool applyCFDToThermal();

    // Validation
    [[nodiscard]] bool validateConfiguration() const;
    [[nodiscard]] std::vector<std::string> getValidationErrors() const;

private:
    // Configuration
    SimulationConfig config_;

    // Solvers
    std::shared_ptr<CFDSolver> cfd_solver_;
    std::shared_ptr<StructuralSolver> structural_solver_;
    std::shared_ptr<ThermalSolver> thermal_solver_;

    // Results
    std::vector<PostProcessingResult> results_;
    std::shared_ptr<ResultVisualization> visualization_;

    // Progress tracking
    ProgressInfo progress_;
    bool paused_ = false;

    // Execution control
    bool stop_requested_ = false;

    // Internal methods
    void updateProgress(double percent, const std::string& message);
    void postProcessResults();
    void aggregateResults();
    void setupCoupling();
};

}  // namespace BendCore::CadCore::Simulation
