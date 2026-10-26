#pragma once

#include <vector>
#include <map>
#include <memory>
#include <string>
#include <array>
#include <cmath>
#include <functional>
#include <stdexcept>

namespace bendcore {

/// Multi-physics coupling framework for BendCore CAD
namespace physics {

// ============================================================================
// Forward Declarations
// ============================================================================
class CouplingIterator;
class DataTransferMap;
class SolverOrchestrator;

// ============================================================================
// Data Structures for Field Data
// ============================================================================

/// Vector field data (displacement, velocity, acceleration)
struct VectorField {
    std::vector<std::array<double, 3>> values;
    std::string name;

    VectorField() = default;
    explicit VectorField(size_t size, const std::string& n = "")
        : values(size, {0.0, 0.0, 0.0}), name(n) {}

    size_t size() const { return values.size(); }

    std::array<double, 3>& operator[](size_t i) { return values[i]; }
    const std::array<double, 3>& operator[](size_t i) const { return values[i]; }
};

/// Scalar field data (temperature, pressure, stress)
struct ScalarField {
    std::vector<double> values;
    std::string name;

    ScalarField() = default;
    explicit ScalarField(size_t size, const std::string& n = "")
        : values(size, 0.0), name(n) {}

    size_t size() const { return values.size(); }

    double& operator[](size_t i) { return values[i]; }
    double operator[](size_t i) const { return values[i]; }
};

/// Tensor field data (stress, strain tensors)
struct TensorField {
    std::vector<std::array<double, 6>> values;  // 6 components: Sxx, Syy, Szz, Sxy, Sxz, Syz
    std::string name;

    TensorField() = default;
    explicit TensorField(size_t size, const std::string& n = "")
        : values(size, {0.0, 0.0, 0.0, 0.0, 0.0, 0.0}), name(n) {}

    size_t size() const { return values.size(); }

    std::array<double, 6>& operator[](size_t i) { return values[i]; }
    const std::array<double, 6>& operator[](size_t i) const { return values[i]; }
};

// ============================================================================
// Fluid-Structure Interaction (FSI)
// ============================================================================

class FluidStructureInteraction {
public:
    struct Config {
        size_t num_nodes = 0;
        size_t num_fluid_dofs = 0;
        size_t num_struct_dofs = 0;
        double fluid_density = 1.225;
        double structure_density = 7850.0;
        double coupling_tolerance = 1e-6;
        int max_iterations = 100;
        double pressure_damping = 0.1;
    };

    explicit FluidStructureInteraction(const Config& cfg) : config_(cfg),
        pressure_load_(cfg.num_struct_dofs),
        displacement_(cfg.num_struct_dofs),
        structure_velocity_(cfg.num_struct_dofs),
        fluid_pressure_(cfg.num_fluid_dofs) {}

    /// Transfer pressure loads from fluid domain to structure
    void transfer_pressure_loads(const ScalarField& fluid_pressure);

    /// Transfer displacement from structure back to fluid mesh
    void transfer_displacement(const VectorField& struct_displacement);

    /// Compute fluid-structure coupling residual
    double compute_coupling_residual() const;

    /// Get current pressure loads on structure
    const VectorField& get_pressure_loads() const { return pressure_load_; }

    /// Get structure displacement
    const VectorField& get_displacement() const { return displacement_; }

    /// Update structure displacement (from structural solver)
    void set_displacement(const VectorField& disp);

    /// Update fluid pressure (from fluid solver)
    void set_fluid_pressure(const ScalarField& pres);

private:
    Config config_;
    VectorField pressure_load_;
    VectorField displacement_;
    VectorField structure_velocity_;
    ScalarField fluid_pressure_;
};

// ============================================================================
// Thermal-Structural Coupling
// ============================================================================

class ThermalStructuralCoupling {
public:
    struct Config {
        size_t num_nodes = 0;
        double reference_temperature = 293.15;  // 20°C in Kelvin
        double thermal_conductivity = 50.0;
        double specific_heat = 500.0;
        double alpha_linear = 12e-6;  // Linear thermal expansion coefficient
        double coupling_tolerance = 1e-6;
        int max_iterations = 100;
    };

    explicit ThermalStructuralCoupling(const Config& cfg) : config_(cfg),
        temperature_(cfg.num_nodes),
        temperature_rate_(cfg.num_nodes),
        thermal_stress_(cfg.num_nodes),
        thermal_strain_(cfg.num_nodes),
        heat_flux_(cfg.num_nodes) {}

    /// Compute thermal strain from temperature field
    void compute_thermal_strain(const ScalarField& temperature);

    /// Compute thermal stress from thermal strain
    void compute_thermal_stress(const VectorField& struct_stiffness);

    /// Transfer heat flux for transient analysis
    void apply_heat_flux(const ScalarField& external_heat_flux, double time_step);

    /// Update temperature field (from thermal solver)
    void set_temperature(const ScalarField& temp);

    /// Get thermal stress
    const VectorField& get_thermal_stress() const { return thermal_stress_; }

    /// Get thermal strain
    const VectorField& get_thermal_strain() const { return thermal_strain_; }

private:
    Config config_;
    ScalarField temperature_;
    ScalarField temperature_rate_;
    VectorField thermal_stress_;
    VectorField thermal_strain_;
    ScalarField heat_flux_;
};

// ============================================================================
// Electromagnetic-Thermal Coupling
// ============================================================================

class ElectromagneticThermalCoupling {
public:
    struct Config {
        size_t num_nodes = 0;
        double electrical_conductivity = 1e7;  // Siemens/meter
        double permeability = 1.0;
        double coupling_tolerance = 1e-6;
        int max_iterations = 100;
    };

    explicit ElectromagneticThermalCoupling(const Config& cfg) : config_(cfg),
        electric_field_(cfg.num_nodes),
        magnetic_field_(cfg.num_nodes),
        current_density_(cfg.num_nodes),
        joule_heat_(cfg.num_nodes),
        electromagnetic_force_(cfg.num_nodes) {}

    /// Compute Joule heating from current density (P = I²R or P = J·E)
    void compute_joule_heating(const VectorField& current_density,
                               const VectorField& electric_field);

    /// Evaluate electromagnetic force from field interaction
    void compute_electromagnetic_force(const VectorField& magnetic_field,
                                       const VectorField& current_density);

    /// Interpolate fields between electromagnetic and thermal domains
    void interpolate_fields(const VectorField& source_field,
                           VectorField& target_field,
                           const std::vector<size_t>& mapping);

    /// Update electric field (from electromagnetic solver)
    void set_electric_field(const VectorField& e_field);

    /// Update magnetic field (from electromagnetic solver)
    void set_magnetic_field(const VectorField& b_field);

    /// Get Joule heat source
    const ScalarField& get_joule_heat() const { return joule_heat_; }

    /// Get electromagnetic force
    const VectorField& get_electromagnetic_force() const { return electromagnetic_force_; }

private:
    Config config_;
    VectorField electric_field_;
    VectorField magnetic_field_;
    VectorField current_density_;
    ScalarField joule_heat_;
    VectorField electromagnetic_force_;
};

// ============================================================================
// Coupling Algorithms
// ============================================================================

enum class CouplingScheme {
    MONOLITHIC,           // Fully coupled solution
    PARTITIONED_LOOSE,    // Weak coupling (non-iterative)
    PARTITIONED_TIGHT,    // Strong coupling (iterative)
    OPERATOR_SPLITTING    // Sequential field solving
};

class CouplingAlgorithm {
public:
    virtual ~CouplingAlgorithm() = default;

    virtual double iterate() = 0;
    virtual bool is_converged() const = 0;
    virtual int get_iteration_count() const = 0;
};

/// Monolithic coupling: All physics solved simultaneously
class MonolithicCoupling : public CouplingAlgorithm {
public:
    struct Config {
        double tolerance = 1e-6;
        int max_iterations = 100;
        double relaxation = 1.0;
    };

    explicit MonolithicCoupling(const Config& cfg) : config_(cfg), iterations_(0) {}

    double iterate() override;
    bool is_converged() const override { return residual_ < config_.tolerance; }
    int get_iteration_count() const override { return iterations_; }

    void set_solve_callback(std::function<VectorField()> callback) {
        solve_callback_ = callback;
    }

private:
    Config config_;
    int iterations_;
    double residual_;
    std::function<VectorField()> solve_callback_;
};

/// Partitioned coupling: Alternating solution of different physics
class PartitionedCoupling : public CouplingAlgorithm {
public:
    struct Config {
        double tolerance = 1e-6;
        int max_iterations = 100;
        double relaxation = 0.5;
        bool use_aitken = true;  // Aitken adaptive relaxation
    };

    explicit PartitionedCoupling(const Config& cfg) : config_(cfg), iterations_(0) {}

    double iterate() override;
    bool is_converged() const override { return residual_ < config_.tolerance; }
    int get_iteration_count() const override { return iterations_; }

    void add_physics_solver(const std::string& name,
                           std::function<VectorField()> solver);

private:
    Config config_;
    int iterations_;
    double residual_;
    std::vector<std::pair<std::string, std::function<VectorField()>>> solvers_;
    VectorField previous_solution_;
    double aitken_parameter_ = 1.0;
};

/// Operator splitting: Sequential solution with field exchange
class OperatorSplitting : public CouplingAlgorithm {
public:
    struct Config {
        double tolerance = 1e-6;
        int max_iterations = 100;
        std::vector<std::string> split_order;
    };

    explicit OperatorSplitting(const Config& cfg) : config_(cfg), iterations_(0) {}

    double iterate() override;
    bool is_converged() const override { return residual_ < config_.tolerance; }
    int get_iteration_count() const override { return iterations_; }

    void add_operator(const std::string& name,
                     std::function<void()> operator_func);

private:
    Config config_;
    int iterations_;
    double residual_;
    std::map<std::string, std::function<void()>> operators_;
};

// ============================================================================
// Data Transfer & Mapping
// ============================================================================

class DataTransferMap {
public:
    /// Types of field mapping
    enum class MappingType {
        NEAREST_NEIGHBOR,
        LINEAR_INTERPOLATION,
        RBFQ,        // Radial basis function quadratic
        CONSERVATION // Conserve total quantity
    };

    struct Config {
        MappingType type = MappingType::LINEAR_INTERPOLATION;
        double tolerance = 1e-10;
        bool check_consistency = true;
    };

    explicit DataTransferMap(const Config& cfg) : config_(cfg) {}

    /// Build mapping between two meshes
    void build_mapping(const std::vector<std::array<double, 3>>& source_nodes,
                      const std::vector<std::array<double, 3>>& target_nodes);

    /// Transfer scalar field from source to target mesh
    void transfer_scalar_field(const ScalarField& source,
                               ScalarField& target);

    /// Transfer vector field from source to target mesh
    void transfer_vector_field(const VectorField& source,
                               VectorField& target);

    /// Transfer tensor field from source to target mesh
    void transfer_tensor_field(const TensorField& source,
                               TensorField& target);

    /// Check consistency of field projection
    double check_consistency();

    /// Get interpolation error estimate
    double get_interpolation_error() const { return interpolation_error_; }

private:
    Config config_;
    std::vector<std::vector<size_t>> source_to_target_;
    std::vector<std::vector<double>> interpolation_weights_;
    double interpolation_error_ = 0.0;
};

// ============================================================================
// Convergence Monitoring
// ============================================================================

struct ConvergenceMetrics {
    double residual = 0.0;
    double energy_error = 0.0;
    double field_change_norm = 0.0;
    int iteration = 0;
    bool converged = false;
    std::vector<double> residual_history;
    std::vector<double> energy_history;
};

class ConvergenceMonitor {
public:
    struct Config {
        double absolute_tolerance = 1e-8;
        double relative_tolerance = 1e-6;
        int max_iterations = 100;
        bool check_energy = true;
    };

    explicit ConvergenceMonitor(const Config& cfg) : config_(cfg) {}

    void record_iteration(double residual, double energy);
    bool is_converged() const;
    const ConvergenceMetrics& get_metrics() const { return metrics_; }

private:
    Config config_;
    ConvergenceMetrics metrics_;
};

// ============================================================================
// Solver Orchestration
// ============================================================================

class SolverOrchestrator {
public:
    struct Config {
        size_t num_nodes = 0;
        double time_step = 0.01;
        int num_time_steps = 100;
        CouplingScheme coupling_scheme = CouplingScheme::PARTITIONED_TIGHT;
        bool enable_adaptive_stepping = true;
    };

    explicit SolverOrchestrator(const Config& cfg) : config_(cfg) {}

    /// Register a physics solver
    void register_solver(const std::string& name,
                        std::function<VectorField()> solver);

    /// Register data transfer between domains
    void register_data_transfer(const std::string& from_physics,
                               const std::string& to_physics,
                               std::shared_ptr<DataTransferMap> transfer);

    /// Execute coupled simulation
    void execute();

    /// Get current time
    double get_current_time() const { return current_time_; }

    /// Get time step
    double get_time_step() const { return config_.time_step; }

    /// Adaptive time stepping
    void adjust_time_step(double reduction_factor);

private:
    Config config_;
    double current_time_ = 0.0;
    std::map<std::string, std::function<VectorField()>> solvers_;
    std::map<std::pair<std::string, std::string>, std::shared_ptr<DataTransferMap>> transfers_;
    std::vector<ConvergenceMetrics> convergence_history_;
};

} // namespace physics
} // namespace bendcore
