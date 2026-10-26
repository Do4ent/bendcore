#pragma once

#include <vector>
#include <array>
#include <memory>
#include <string>
#include "../geometry/vector.hpp"

namespace BendCore::CadCore::Simulation {

/**
 * @brief Thermal material properties
 */
struct ThermalMaterialProperties {
    std::string name;
    double thermal_conductivity = 0.0;   // W/(m·K)
    double specific_heat_capacity = 0.0; // J/(kg·K)
    double density = 0.0;                // kg/m^3
    double thermal_expansion = 0.0;      // 1/K
    double emissivity = 0.0;             // 0-1
};

/**
 * @brief Thermal boundary condition types
 */
enum class ThermalBCType {
    TEMPERATURE,      // Fixed temperature (Dirichlet)
    HEAT_FLUX,        // Fixed heat flux (Neumann)
    CONVECTION,       // Convection boundary condition
    RADIATION,        // Radiation boundary condition
    ADIABATIC,        // Insulated (zero flux)
};

/**
 * @brief Thermal boundary condition
 */
struct ThermalBoundaryCondition {
    ThermalBCType type;
    std::string name;
    double temperature = 0.0;         // For TEMPERATURE BC
    double heat_flux = 0.0;           // For HEAT_FLUX BC (W/m^2)
    double convection_coefficient = 0.0;  // For CONVECTION BC (W/(m^2·K))
    double ambient_temperature = 0.0;     // For CONVECTION and RADIATION BCs
    std::vector<size_t> face_indices;
};

/**
 * @brief Heat source/sink definition
 */
struct HeatSource {
    size_t element_id;
    double heat_generation = 0.0;  // W/m^3
};

/**
 * @brief Thermal analysis results
 */
struct ThermalResults {
    size_t node_id;
    Vector3D position;
    double temperature = 0.0;
    Vector3D heat_flux = Vector3D(0, 0, 0);
};

/**
 * @brief Thermal Solver for heat conduction
 */
class ThermalSolver {
public:
    ThermalSolver();
    ~ThermalSolver() = default;

    // Material properties
    void setMaterialProperties(const ThermalMaterialProperties& props);
    [[nodiscard]] const ThermalMaterialProperties& getMaterialProperties() const { return material_; }

    // Transient analysis
    void setTimeStep(double dt);
    void setTotalTime(double total_time);
    void setTransientMode(bool transient);

    // Mesh and coordinates
    void setNumNodes(size_t num_nodes);
    void addNodeCoordinates(size_t node_id, const Vector3D& coords);

    // Boundary conditions
    void addBoundaryCondition(const ThermalBoundaryCondition& bc);
    void setInitialTemperature(double temp);

    // Heat sources
    void addHeatSource(const HeatSource& source);

    // Solver execution
    bool solve();
    bool solveTransient();

    // Results retrieval
    [[nodiscard]] const std::vector<ThermalResults>& getTemperatureField() const { return results_; }
    [[nodiscard]] double getMaxTemperature() const { return max_temperature_; }
    [[nodiscard]] double getMinTemperature() const { return min_temperature_; }
    [[nodiscard]] bool isConverged() const { return converged_; }
    [[nodiscard]] size_t getIterationCount() const { return iteration_count_; }
    [[nodiscard]] double getTimeStep() const { return time_step_; }
    [[nodiscard]] double getElapsedTime() const { return elapsed_time_; }

private:
    // Material properties
    ThermalMaterialProperties material_;

    // Geometry
    std::vector<Vector3D> node_coordinates_;
    size_t num_nodes_ = 0;

    // Boundary conditions and heat sources
    std::vector<ThermalBoundaryCondition> boundary_conditions_;
    std::vector<HeatSource> heat_sources_;

    // Time parameters
    double time_step_ = 1.0;
    double total_time_ = 10.0;
    double elapsed_time_ = 0.0;
    bool transient_mode_ = false;
    double initial_temperature_ = 293.15;  // 20°C

    // Solution fields
    std::vector<ThermalResults> results_;
    std::vector<double> temperature_field_;
    std::vector<double> temperature_field_previous_;

    // Convergence tracking
    bool converged_ = false;
    size_t iteration_count_ = 0;
    double max_temperature_ = 0.0;
    double min_temperature_ = 0.0;
    double convergence_tolerance_ = 1e-6;

    // Internal solver methods
    void initializeTemperatureField();
    void assembleConductivityMatrix();
    void assembleHeatCapacityMatrix();
    void applyBoundaryConditions();
    void solveLinearSystem();
    void updateResults();
    [[nodiscard]] double computeResidual();
};

}  // namespace BendCore::CadCore::Simulation
