#pragma once

#include <vector>
#include <cmath>
#include <memory>

namespace bendcore {
namespace topology_optimization {

/**
 * Constraint Handling for Topology Optimization
 *
 * Manages various constraints:
 * - Volume constraints
 * - Stress constraints
 * - Symmetry constraints
 * - Manufacturing constraints
 */
class ConstraintHandler {
public:
    /**
     * Constraint type enumeration
     */
    enum class ConstraintType {
        VOLUME = 0,
        STRESS = 1,
        SYMMETRY = 2,
        MANUFACTURING = 3,
        DISPLACEMENT = 4,
        FREQUENCY = 5
    };

public:
    /**
     * Check and compute volume constraint
     * g = (total_volume / target_volume) - 1 <= 0
     *
     * @param densities Current density field
     * @param element_volumes Element volumes
     * @param target_volume Target total volume
     * @return Constraint value (should be <= 0 for satisfaction)
     */
    static double compute_volume_constraint(
        const std::vector<double>& densities,
        const std::vector<double>& element_volumes,
        double target_volume);

    /**
     * Compute stress constraint
     * g = (max_stress / stress_limit) - 1 <= 0
     *
     * @param stresses Von Mises stress values
     * @param stress_limit Maximum allowable stress
     * @return Constraint value
     */
    static double compute_stress_constraint(
        const std::vector<double>& stresses,
        double stress_limit);

    /**
     * Enforce symmetry constraint on densities
     * Mirrors densities across symmetry plane
     *
     * @param densities Current densities
     * @param symmetry_pairs Element index pairs (original, mirrored)
     * @return Symmetry-constrained densities
     */
    static std::vector<double> apply_symmetry_constraint(
        const std::vector<double>& densities,
        const std::vector<std::pair<size_t, size_t>>& symmetry_pairs);

    /**
     * Manufacturing constraint: minimum feature size
     * Removes or constrains small isolated features
     *
     * @param densities Current densities
     * @param element_positions Element center coordinates
     * @param min_feature_size Minimum feature size
     * @param threshold Density threshold for feature detection
     * @return Processed densities
     */
    static std::vector<double> apply_manufacturing_constraint(
        const std::vector<double>& densities,
        const std::vector<std::array<double, 3>>& element_positions,
        double min_feature_size,
        double threshold = 0.5);

    /**
     * Displacement constraint
     * g = (max_displacement / displacement_limit) - 1 <= 0
     *
     * @param displacements Nodal displacements
     * @param displacement_limit Maximum allowable displacement
     * @return Constraint value
     */
    static double compute_displacement_constraint(
        const std::vector<double>& displacements,
        double displacement_limit);

    /**
     * Frequency constraint
     * g = (target_frequency / fundamental_frequency) - 1 <= 0
     *
     * @param fundamental_frequency Computed fundamental frequency
     * @param target_frequency Target frequency
     * @return Constraint value
     */
    static double compute_frequency_constraint(
        double fundamental_frequency,
        double target_frequency);

public:
    struct Constraint {
        ConstraintType type;
        double lower_bound = -1e6;
        double upper_bound = 0.0;  // Typically <= 0 for satisfaction
        double weight = 1.0;        // Constraint weighting
        bool active = true;

        Constraint(ConstraintType t, double ub = 0.0, double w = 1.0)
            : type(t), upper_bound(ub), weight(w) {}
    };

    struct Parameters {
        double volume_fraction = 0.5;        // Target volume fraction
        double stress_limit = 1e6;           // Maximum allowable stress
        double min_feature_size = 1.0;       // Manufacturing minimum feature size
        double constraint_tolerance = 1e-3;  // Constraint satisfaction tolerance
        bool use_penalty_method = false;     // Use penalty method vs barrier
        double penalty_parameter = 1.0;      // Penalty parameter for penalty method
    };

    explicit ConstraintHandler(const Parameters& params = Parameters());

    /**
     * Add constraint
     * @param constraint Constraint definition
     */
    void add_constraint(const Constraint& constraint);

    /**
     * Check all constraints
     * @param densities Current densities
     * @param element_volumes Element volumes
     * @param stresses Stress values
     * @param element_positions Element coordinates
     * @return Vector of constraint values (should all be <= upper_bound)
     */
    std::vector<double> evaluate_constraints(
        const std::vector<double>& densities,
        const std::vector<double>& element_volumes,
        const std::vector<double>& stresses,
        const std::vector<std::array<double, 3>>& element_positions);

    /**
     * Check if all constraints are satisfied
     * @param constraint_values Evaluated constraint values
     * @return True if all constraints satisfied
     */
    bool all_constraints_satisfied(const std::vector<double>& constraint_values) const;

    /**
     * Compute penalty for constraint violation (for penalty method)
     * @param constraint_values Evaluated constraint values
     * @return Total penalty
     */
    double compute_penalty(const std::vector<double>& constraint_values) const;

    /**
     * Get active constraints
     * @return Vector of active constraint indices
     */
    std::vector<size_t> get_active_constraints() const;

    /**
     * Set symmetry pairs for symmetry constraint
     * @param pairs Index pairs (original, mirrored)
     */
    void set_symmetry_pairs(const std::vector<std::pair<size_t, size_t>>& pairs);

private:
    Parameters parameters_;
    std::vector<Constraint> constraints_;
    std::vector<std::pair<size_t, size_t>> symmetry_pairs_;

    /**
     * Find connected components in density field
     */
    std::vector<std::vector<size_t>> find_connected_components(
        const std::vector<double>& densities,
        const std::vector<std::array<double, 3>>& element_positions,
        double threshold) const;
};

} // namespace topology_optimization
} // namespace bendcore
