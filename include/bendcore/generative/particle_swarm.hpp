#pragma once

#include <vector>
#include <random>
#include <algorithm>
#include <limits>
#include "design_space.hpp"

namespace bendcore::generative {

/**
 * Particle Swarm Optimization Engine
 * Swarm intelligence-based optimizer inspired by bird flocking and fish schooling
 */

// Boundary handling strategy
enum class BoundaryHandling {
    REFLECT,        // Reflect particle at boundary
    WRAP_AROUND,    // Continuous domain (periodic)
    ABSORB,         // Stop at boundary
    PENALTY         // Apply penalty weight
};

// Particle state in the swarm
struct Particle {
    std::vector<double> position;       // Current position
    std::vector<double> velocity;       // Current velocity
    std::vector<double> best_position;  // Personal best position
    double best_fitness = std::numeric_limits<double>::lowest();
    double current_fitness = std::numeric_limits<double>::lowest();
    double constraint_violation = 0.0;
    bool is_feasible = false;

    Particle() = default;
    explicit Particle(size_t dimension)
        : position(dimension, 0.5), velocity(dimension, 0.0), best_position(dimension, 0.5) {}

    [[nodiscard]] Particle clone() const;
};

// PSO configuration
struct PSOConfig {
    size_t swarm_size = 50;
    size_t iterations = 200;
    double cognitive_coefficient = 2.0;      // Attraction to personal best
    double social_coefficient = 2.0;         // Attraction to global best
    double inertia_weight = 0.7;
    bool dynamic_inertia = true;
    double min_inertia = 0.4;
    double max_inertia = 0.9;
    BoundaryHandling boundary_handling = BoundaryHandling::REFLECT;
};

// Particle Swarm Optimizer
class ParticleSwarmOptimizer {
public:
    ParticleSwarmOptimizer(const DesignSpace& space, const PSOConfig& config = PSOConfig());

    // Run optimization
    [[nodiscard]] Particle optimize(size_t max_iterations = 0);

    // Get results
    [[nodiscard]] const std::vector<Particle>& swarm() const { return swarm_; }
    [[nodiscard]] const Particle& best_particle() const { return best_particle_; }
    [[nodiscard]] const std::vector<double>& global_best_position() const { return global_best_position_; }
    [[nodiscard]] double global_best_fitness() const { return global_best_fitness_; }
    [[nodiscard]] std::vector<double> convergence_history() const { return convergence_history_; }
    [[nodiscard]] size_t iteration_count() const { return iteration_count_; }

    // Configuration
    void set_cognitive_coefficient(double c) { config_.cognitive_coefficient = c; }
    void set_social_coefficient(double c) { config_.social_coefficient = c; }
    void set_inertia_weight(double w) { config_.inertia_weight = w; }
    void set_dynamic_inertia(bool enable) { config_.dynamic_inertia = enable; }
    void set_boundary_handling(BoundaryHandling strategy) { config_.boundary_handling = strategy; }

private:
    const DesignSpace& design_space_;
    PSOConfig config_;
    std::vector<Particle> swarm_;
    Particle best_particle_;
    std::vector<double> global_best_position_;
    double global_best_fitness_ = std::numeric_limits<double>::lowest();
    std::vector<double> convergence_history_;
    size_t iteration_count_ = 0;

    std::mt19937 rng_;

    // Internal operations
    void initialize_swarm();
    void evaluate_swarm();
    void update_particles();
    void evaluate_particle(Particle& particle);
    void update_best_position(Particle& particle);
    void update_global_best();

    // Velocity and position updates
    void update_velocity(Particle& particle);
    void update_position(Particle& particle);
    void handle_boundary(std::vector<double>& position);

    // Boundary handling strategies
    void reflect_boundary(std::vector<double>& position);
    void wrap_boundary(std::vector<double>& position);
    void absorb_boundary(std::vector<double>& position);
    void apply_penalty_boundary(Particle& particle);

    // Utility
    [[nodiscard]] double compute_fitness(const Particle& particle) const;
    [[nodiscard]] double get_inertia_weight(size_t iteration, size_t max_iterations) const;
};

} // namespace bendcore::generative
