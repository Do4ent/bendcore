// Load Case Handler - Complex Boundary Conditions and Multiple Load Cases
// M12 Phase: Load Case Handling (GAP-053)
#pragma once

#include <vector>
#include <map>
#include <string>
#include <optional>
#include <memory>
#include <cmath>

namespace bendcore::simulation {

// Load direction enumeration
enum class LoadDirection {
    PositiveX, NegativeX,
    PositiveY, NegativeY,
    PositiveZ, NegativeZ
};

// Boundary condition types
enum class BoundaryConditionType {
    Fixed,                  // All DOF restrained
    PinnedSupport,          // Rotation allowed
    RollerSupport,          // One direction free
    FreeEdge,               // No restraint
    Symmetry,               // Symmetric plane
    AntiSymmetry            // Anti-symmetric plane
};

// Single load component
struct LoadComponent {
    std::string name;
    double magnitude{0.0};              // N or N·m
    LoadDirection direction;
    double application_point_x{0.0};    // m
    double application_point_y{0.0};    // m
    double application_point_z{0.0};    // m
    bool is_moment{false};              // Force or moment
    double ramp_time{0.0};              // Seconds (0 = instant)
};

// Boundary condition definition
struct BoundaryCondition {
    std::string name;
    BoundaryConditionType type;
    double location_x{0.0};             // m
    double location_y{0.0};             // m
    double location_z{0.0};             // m
    bool restrain_x{false};
    bool restrain_y{false};
    bool restrain_z{false};
    bool restrain_rx{false};            // Rotational
    bool restrain_ry{false};
    bool restrain_rz{false};
};

// Load combination rule
struct LoadCombination {
    std::string name;
    std::vector<std::pair<std::string, double>> load_factors;  // {load_name, factor}
    double safety_factor{1.0};
    std::string description;
};

// Analysis result for a single load case
struct LoadCaseResult {
    std::string load_case_name;
    double max_stress{0.0};             // Pa
    double max_displacement{0.0};       // m
    double strain_energy{0.0};          // J
    std::vector<double> reaction_forces; // N
    std::vector<double> reaction_moments;// N·m
    bool converged{true};
    std::string status_message;
};

// Main load case handler
class LoadCaseHandler {
public:
    LoadCaseHandler() = default;

    // Add load components
    bool addLoadComponent(const LoadComponent& load) noexcept;

    // Add boundary conditions
    bool addBoundaryCondition(const BoundaryCondition& bc) noexcept;

    // Create load combinations
    bool addLoadCombination(const LoadCombination& combination) noexcept;

    // Get load/BC/combination
    [[nodiscard]] std::optional<LoadComponent> getLoadComponent(const std::string& name) const noexcept;

    [[nodiscard]] std::optional<BoundaryCondition> getBoundaryCondition(const std::string& name) const noexcept;

    [[nodiscard]] std::optional<LoadCombination> getLoadCombination(const std::string& name) const noexcept;

    // Get all loads/BCs/combinations
    [[nodiscard]] std::vector<LoadComponent> getAllLoadComponents() const noexcept;

    [[nodiscard]] std::vector<BoundaryCondition> getAllBoundaryConditions() const noexcept;

    [[nodiscard]] std::vector<LoadCombination> getAllLoadCombinations() const noexcept;

    // Analysis methods
    [[nodiscard]] LoadCaseResult analyzeLoadCase(const std::vector<std::string>& load_names) noexcept;

    [[nodiscard]] std::vector<LoadCaseResult> analyzeLoadCombination(const std::string& combination_name) noexcept;

    [[nodiscard]] std::vector<LoadCaseResult> analyzeAllLoadCombinations() noexcept;

    // Remove elements
    bool removeLoadComponent(const std::string& name) noexcept;

    bool removeBoundaryCondition(const std::string& name) noexcept;

    bool removeLoadCombination(const std::string& name) noexcept;

    // Clear database
    void clear() noexcept {
        loads_.clear();
        boundary_conditions_.clear();
        load_combinations_.clear();
    }

    // Counts
    [[nodiscard]] size_t getLoadCount() const noexcept { return loads_.size(); }
    [[nodiscard]] size_t getBCCount() const noexcept { return boundary_conditions_.size(); }
    [[nodiscard]] size_t getCombinationCount() const noexcept { return load_combinations_.size(); }

    // Validation
    [[nodiscard]] bool validateLoadCase(const std::vector<std::string>& load_names) const noexcept;

    [[nodiscard]] bool isLoadCaseStaticallyDetermined() const noexcept;

    // Calculate reaction forces for a load case
    [[nodiscard]] std::vector<double> calculateReactionForces(
        const std::vector<std::string>& load_names) const noexcept;

    // Calculate total applied load
    [[nodiscard]] double calculateTotalAppliedLoad(const std::vector<std::string>& load_names) const noexcept;

    // Get worst case (max stress)
    [[nodiscard]] LoadCaseResult getWorstCase(const std::vector<LoadCaseResult>& results) const noexcept;

    // Envelope analysis (min/max for each node)
    [[nodiscard]] std::vector<std::pair<double, double>> getStressEnvelope(
        const std::vector<LoadCaseResult>& results) const noexcept;

    // Determine if system is overdetermined
    [[nodiscard]] bool isSystemOverdetermined() const noexcept;

    // Check for load path issues
    [[nodiscard]] bool hasValidLoadPath(const std::vector<std::string>& load_names) const noexcept;

private:
    std::map<std::string, LoadComponent> loads_;
    std::map<std::string, BoundaryCondition> boundary_conditions_;
    std::map<std::string, LoadCombination> load_combinations_;

    // Helper functions
    [[nodiscard]] double calculateEquivalentStress(double fx, double fy, double fz) const noexcept;

    [[nodiscard]] double calculateMomentMagnitude(double mx, double my, double mz) const noexcept {
        return std::sqrt(mx * mx + my * my + mz * mz);
    }

    [[nodiscard]] int countRestrainedDOF() const noexcept;

    [[nodiscard]] bool checkStaticEquilibrium(const std::vector<LoadComponent>& loads) const noexcept;
};

}  // namespace bendcore::simulation
