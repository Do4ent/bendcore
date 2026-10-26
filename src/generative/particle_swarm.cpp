#include "../../include/bendcore/generative/particle_swarm.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace bendcore::generative {

// Particle methods
Particle Particle::clone() const {
    Particle cloned;
    cloned.position = position;
    cloned.velocity = velocity;
    cloned.best_position = best_position;
    cloned.best_fitness = best_fitness;
    cloned.current_fitness = current_fitness;
    cloned.constraint_violation = constraint_violation;
    cloned.is_feasible = is_feasible;
    return cloned;
}

// ParticleSwarmOptimizer methods
ParticleSwarmOptimizer::ParticleSwarmOptimizer(const DesignSpace& space, const PSOConfig& config)
    : design_space_(space), config_(config), rng_(std::random_device{}()) {
    swarm_.resize(config_.swarm_size);
    best_particle_ = Particle(space.parameter_count());
    global_best_position_.resize(space.parameter_count(), 0.5);
}

Particle ParticleSwarmOptimizer::optimize(size_t max_iterations) {
    size_t iterations = max_iterations > 0 ? max_iterations : config_.iterations;
    iteration_count_ = 0;
    convergence_history_.clear();
    global_best_fitness_ = std::numeric_limits<double>::lowest();

    initialize_swarm();
    evaluate_swarm();
    update_global_best();
    best_particle_ = swarm_[0].clone();

    for (iteration_count_ = 0; iteration_count_ < iterations; ++iteration_count_) {
        update_particles();
        evaluate_swarm();

        for (auto& particle : swarm_) {
            update_best_position(particle);
        }

        update_global_best();
        convergence_history_.push_back(global_best_fitness_);
    }

    best_particle_.position = global_best_position_;
    best_particle_.current_fitness = global_best_fitness_;
    return best_particle_;
}

void ParticleSwarmOptimizer::initialize_swarm() {
    std::uniform_real_distribution<double> pos_dist(0.0, 1.0);
    std::uniform_real_distribution<double> vel_dist(-0.5, 0.5);

    for (auto& particle : swarm_) {
        particle.position.resize(design_space_.parameter_count());
        particle.velocity.resize(design_space_.parameter_count());
        particle.best_position.resize(design_space_.parameter_count());

        for (size_t i = 0; i < design_space_.parameter_count(); ++i) {
            particle.position[i] = pos_dist(rng_);
            particle.velocity[i] = vel_dist(rng_);
            particle.best_position[i] = particle.position[i];
        }

        particle.best_fitness = std::numeric_limits<double>::lowest();
        particle.current_fitness = std::numeric_limits<double>::lowest();
    }
}

void ParticleSwarmOptimizer::evaluate_swarm() {
    for (auto& particle : swarm_) {
        evaluate_particle(particle);
    }
}

void ParticleSwarmOptimizer::evaluate_particle(Particle& particle) {
    auto x = design_space_.denormalize(particle.position);
    design_space_.enforce_bounds(x);

    particle.constraint_violation = design_space_.constraint_violation(x);
    particle.is_feasible = (particle.constraint_violation < 1e-6);

    if (particle.is_feasible) {
        particle.current_fitness = design_space_.primary_objective().compute(x);
    } else {
        particle.current_fitness = -particle.constraint_violation;
    }
}

void ParticleSwarmOptimizer::update_best_position(Particle& particle) {
    if (particle.current_fitness > particle.best_fitness) {
        particle.best_fitness = particle.current_fitness;
        particle.best_position = particle.position;
    }
}

void ParticleSwarmOptimizer::update_global_best() {
    for (const auto& particle : swarm_) {
        if (particle.best_fitness > global_best_fitness_) {
            global_best_fitness_ = particle.best_fitness;
            global_best_position_ = particle.best_position;
        }
    }
}

void ParticleSwarmOptimizer::update_particles() {
    double inertia = config_.inertia_weight;

    if (config_.dynamic_inertia) {
        inertia = get_inertia_weight(iteration_count_, config_.iterations);
    }

    for (auto& particle : swarm_) {
        update_velocity(particle);
        update_position(particle);
        handle_boundary(particle.position);
    }
}

void ParticleSwarmOptimizer::update_velocity(Particle& particle) {
    std::uniform_real_distribution<double> dist(0.0, 1.0);

    double inertia = config_.inertia_weight;
    if (config_.dynamic_inertia) {
        inertia = get_inertia_weight(iteration_count_, config_.iterations);
    }

    for (size_t i = 0; i < particle.velocity.size(); ++i) {
        double r1 = dist(rng_);
        double r2 = dist(rng_);

        particle.velocity[i] = inertia * particle.velocity[i] +
                              config_.cognitive_coefficient * r1 * (particle.best_position[i] - particle.position[i]) +
                              config_.social_coefficient * r2 * (global_best_position_[i] - particle.position[i]);

        // Velocity clamping
        particle.velocity[i] = std::max(-0.5, std::min(0.5, particle.velocity[i]));
    }
}

void ParticleSwarmOptimizer::update_position(Particle& particle) {
    for (size_t i = 0; i < particle.position.size(); ++i) {
        particle.position[i] += particle.velocity[i];
    }
}

void ParticleSwarmOptimizer::handle_boundary(std::vector<double>& position) {
    switch (config_.boundary_handling) {
        case BoundaryHandling::REFLECT:
            reflect_boundary(position);
            break;
        case BoundaryHandling::WRAP_AROUND:
            wrap_boundary(position);
            break;
        case BoundaryHandling::ABSORB:
            absorb_boundary(position);
            break;
        case BoundaryHandling::PENALTY:
            // Penalty handled in fitness evaluation
            absorb_boundary(position);
            break;
    }
}

void ParticleSwarmOptimizer::reflect_boundary(std::vector<double>& position) {
    for (auto& pos : position) {
        if (pos < 0.0) {
            pos = -pos;
        } else if (pos > 1.0) {
            pos = 2.0 - pos;
        }
        pos = std::max(0.0, std::min(1.0, pos));
    }
}

void ParticleSwarmOptimizer::wrap_boundary(std::vector<double>& position) {
    for (auto& pos : position) {
        if (pos < 0.0) {
            pos += 1.0;
        } else if (pos > 1.0) {
            pos -= 1.0;
        }
    }
}

void ParticleSwarmOptimizer::absorb_boundary(std::vector<double>& position) {
    for (auto& pos : position) {
        pos = std::max(0.0, std::min(1.0, pos));
    }
}

void ParticleSwarmOptimizer::apply_penalty_boundary(Particle& particle) {
    for (size_t i = 0; i < particle.position.size(); ++i) {
        if (particle.position[i] < 0.0 || particle.position[i] > 1.0) {
            particle.current_fitness -= 100.0;
        }
    }
}

double ParticleSwarmOptimizer::compute_fitness(const Particle& particle) const {
    return particle.current_fitness;
}

double ParticleSwarmOptimizer::get_inertia_weight(size_t iteration, size_t max_iterations) const {
    if (max_iterations == 0) return config_.inertia_weight;

    double progress = static_cast<double>(iteration) / max_iterations;
    return config_.max_inertia - (config_.max_inertia - config_.min_inertia) * progress;
}

} // namespace bendcore::generative
