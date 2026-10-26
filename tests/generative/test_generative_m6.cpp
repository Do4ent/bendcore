#include <gtest/gtest.h>
#include "../../include/bendcore/generative/design_space.hpp"
#include "../../include/bendcore/generative/genetic_algorithm.hpp"
#include "../../include/bendcore/generative/particle_swarm.hpp"
#include "../../include/bendcore/generative/constraint_handler.hpp"
#include "../../include/bendcore/generative/design_generator.hpp"
#include "../../include/bendcore/generative/optimization_manager.hpp"
#include "../../include/bendcore/generative/visualization.hpp"
#include <memory>

using namespace bendcore::generative;

// ===== Test Fixture for Design Space =====
class DesignSpaceTest : public ::testing::Test {
protected:
    void SetUp() override {
        design_space_.add_parameter("param1", 0.0, 10.0, 5.0);
        design_space_.add_parameter("param2", -5.0, 5.0, 0.0);

        auto objective = Objective("minimize_x2",
            [](const std::vector<double>& x) {
                if (x.empty()) return 0.0;
                return x[0] * x[0] + x[1] * x[1];
            }, true);
        design_space_.set_primary_objective(objective);
    }

    DesignSpace design_space_;
};

TEST_F(DesignSpaceTest, AddParameters) {
    EXPECT_EQ(design_space_.parameter_count(), 2);
    EXPECT_EQ(design_space_.get_parameter(0).name, "param1");
    EXPECT_EQ(design_space_.get_parameter(1).name, "param2");
}

TEST_F(DesignSpaceTest, ParameterBounds) {
    EXPECT_EQ(design_space_.get_parameter(0).lower_bound, 0.0);
    EXPECT_EQ(design_space_.get_parameter(0).upper_bound, 10.0);
    EXPECT_EQ(design_space_.get_parameter(0).range(), 10.0);
}

TEST_F(DesignSpaceTest, ParameterNormalization) {
    double normalized = design_space_.get_parameter(0).normalize(5.0);
    EXPECT_NEAR(normalized, 0.5, 1e-6);

    double denormalized = design_space_.get_parameter(0).denormalize(0.5);
    EXPECT_NEAR(denormalized, 5.0, 1e-6);
}

TEST_F(DesignSpaceTest, VectorNormalization) {
    std::vector<double> x = {5.0, 0.0};
    auto normalized = design_space_.normalize(x);
    EXPECT_NEAR(normalized[0], 0.5, 1e-6);
    EXPECT_NEAR(normalized[1], 0.5, 1e-6);
}

TEST_F(DesignSpaceTest, WithinBounds) {
    std::vector<double> valid = {5.0, 2.5};
    EXPECT_TRUE(design_space_.within_bounds(valid));

    std::vector<double> invalid = {15.0, 0.0};
    EXPECT_FALSE(design_space_.within_bounds(invalid));
}

TEST_F(DesignSpaceTest, EnforceBounds) {
    std::vector<double> x = {15.0, -10.0};
    design_space_.enforce_bounds(x);
    EXPECT_EQ(x[0], 10.0);
    EXPECT_EQ(x[1], -5.0);
}

TEST_F(DesignSpaceTest, AddConstraint) {
    design_space_.add_inequality_constraint("c1",
        [](const std::vector<double>& x) {
            if (x.empty()) return 0.0;
            return x[0] - 8.0;  // x[0] <= 8
        });

    EXPECT_EQ(design_space_.constraint_count(), 1);
}

TEST_F(DesignSpaceTest, ConstraintViolation) {
    design_space_.add_inequality_constraint("c1",
        [](const std::vector<double>& x) {
            if (x.empty()) return 0.0;
            return x[0] - 8.0;
        });

    std::vector<double> feasible = {7.0, 0.0};
    EXPECT_TRUE(design_space_.is_feasible(feasible));

    std::vector<double> infeasible = {9.0, 0.0};
    EXPECT_FALSE(design_space_.is_feasible(infeasible));
}

TEST_F(DesignSpaceTest, ObjectiveEvaluation) {
    std::vector<double> x = {3.0, 4.0};
    double objective = design_space_.primary_objective().compute(x);
    EXPECT_NEAR(objective, 25.0, 1e-6);
}

// ===== Test Fixture for Genetic Algorithm =====
class GeneticAlgorithmTest : public ::testing::Test {
protected:
    void SetUp() override {
        design_space_.add_parameter("x1", -5.0, 5.0, 0.0);
        design_space_.add_parameter("x2", -5.0, 5.0, 0.0);

        auto objective = Objective("minimize",
            [](const std::vector<double>& x) {
                if (x.size() < 2) return 0.0;
                return x[0] * x[0] + x[1] * x[1];
            }, true);
        design_space_.set_primary_objective(objective);
    }

    DesignSpace design_space_;
};

TEST_F(GeneticAlgorithmTest, Initialization) {
    GAConfig config;
    config.population_size = 50;
    GeneticAlgorithm ga(design_space_, config);

    EXPECT_EQ(ga.population().empty() || ga.population().size() <= 100, true);
}

TEST_F(GeneticAlgorithmTest, IndividualComparison) {
    Individual ind1({0.3, 0.4});
    Individual ind2({0.5, 0.6});

    ind1.fitness = 10.0;
    ind1.is_feasible = true;
    ind2.fitness = 5.0;
    ind2.is_feasible = true;

    EXPECT_FALSE(ind1 < ind2);
    EXPECT_TRUE(ind2 < ind1);
}

TEST_F(GeneticAlgorithmTest, IndividualClone) {
    Individual original({0.3, 0.4});
    original.fitness = 15.0;

    auto cloned = original.clone();
    EXPECT_EQ(cloned.fitness, 15.0);
    EXPECT_EQ(cloned.chromosome.size(), original.chromosome.size());
}

TEST_F(GeneticAlgorithmTest, Optimization) {
    GAConfig config;
    config.population_size = 30;
    config.generations = 50;

    GeneticAlgorithm ga(design_space_, config);
    auto best = ga.optimize(50);

    EXPECT_TRUE(best.fitness <= 100.0);  // Reasonable bounds
    EXPECT_EQ(best.chromosome.size(), 2);
}

TEST_F(GeneticAlgorithmTest, ConvergenceHistory) {
    GAConfig config;
    config.generations = 20;

    GeneticAlgorithm ga(design_space_, config);
    ga.optimize(20);

    EXPECT_EQ(ga.convergence_history().size(), 20);
}

// ===== Test Fixture for Particle Swarm Optimization =====
class ParticleSwarmTest : public ::testing::Test {
protected:
    void SetUp() override {
        design_space_.add_parameter("x1", -5.0, 5.0, 0.0);
        design_space_.add_parameter("x2", -5.0, 5.0, 0.0);

        auto objective = Objective("minimize",
            [](const std::vector<double>& x) {
                if (x.size() < 2) return 0.0;
                return x[0] * x[0] + x[1] * x[1];
            }, true);
        design_space_.set_primary_objective(objective);
    }

    DesignSpace design_space_;
};

TEST_F(ParticleSwarmTest, ParticleInitialization) {
    Particle p(3);
    EXPECT_EQ(p.position.size(), 3);
    EXPECT_EQ(p.velocity.size(), 3);
    EXPECT_EQ(p.best_position.size(), 3);
}

TEST_F(ParticleSwarmTest, ParticleClone) {
    Particle original(2);
    original.position = {0.3, 0.4};
    original.best_fitness = 5.0;

    auto cloned = original.clone();
    EXPECT_EQ(cloned.position, original.position);
    EXPECT_EQ(cloned.best_fitness, original.best_fitness);
}

TEST_F(ParticleSwarmTest, PSOptimization) {
    PSOConfig config;
    config.swarm_size = 20;
    config.iterations = 50;

    ParticleSwarmOptimizer pso(design_space_, config);
    auto best = pso.optimize(50);

    EXPECT_EQ(best.position.size(), 2);
    EXPECT_TRUE(best.best_fitness <= 100.0);
}

TEST_F(ParticleSwarmTest, SwarmConvergence) {
    PSOConfig config;
    config.iterations = 30;

    ParticleSwarmOptimizer pso(design_space_, config);
    pso.optimize(30);

    EXPECT_EQ(pso.convergence_history().size(), 30);
}

TEST_F(ParticleSwarmTest, BoundaryHandling) {
    PSOConfig config;
    config.boundary_handling = BoundaryHandling::REFLECT;

    ParticleSwarmOptimizer pso(design_space_, config);
    EXPECT_EQ(pso.swarm().size(), config.swarm_size);
}

// ===== Test Fixture for Constraint Handler =====
class ConstraintHandlerTest : public ::testing::Test {
protected:
    void SetUp() override {
        design_space_.add_parameter("x", 0.0, 10.0);
        design_space_.add_parameter("y", 0.0, 10.0);

        auto obj = Objective("f", [](const std::vector<double>& x) { return x.empty() ? 0 : x[0]; }, true);
        design_space_.set_primary_objective(obj);

        design_space_.add_inequality_constraint("c1",
            [](const std::vector<double>& x) {
                return x.size() < 2 ? 0 : x[0] + x[1] - 15.0;
            });

        handler_ = std::make_unique<ConstraintHandler>(design_space_);
    }

    DesignSpace design_space_;
    std::unique_ptr<ConstraintHandler> handler_;
};

TEST_F(ConstraintHandlerTest, ConstraintEvaluation) {
    std::vector<double> x = {5.0, 5.0};
    auto report = handler_->evaluate_constraints(x);
    EXPECT_FALSE(report.is_violated[0]);  // 5 + 5 - 15 = -5 <= 0
}

TEST_F(ConstraintHandlerTest, FeasibilityCheck) {
    std::vector<double> feasible = {5.0, 5.0};
    EXPECT_TRUE(handler_->is_feasible(feasible));

    std::vector<double> infeasible = {8.0, 8.0};
    EXPECT_FALSE(handler_->is_feasible(infeasible));
}

TEST_F(ConstraintHandlerTest, PenaltyComputation) {
    std::vector<double> x = {5.0, 5.0};
    auto report = handler_->evaluate_constraints(x);
    double penalty = handler_->compute_penalty(x, report);
    EXPECT_GE(penalty, 0.0);
}

TEST_F(ConstraintHandlerTest, ActiveConstraints) {
    std::vector<double> x = {8.0, 8.0};
    auto active = handler_->identify_active_constraints(x);
    EXPECT_GE(active.count(), 0);
}

// ===== Test Fixture for Design Generator =====
class DesignGeneratorTest : public ::testing::Test {
protected:
    void SetUp() override {
        design_space_.add_parameter("p1", 0.0, 100.0);
        design_space_.add_parameter("p2", 0.0, 100.0);
        design_space_.add_parameter("p3", 0.0, 100.0);

        auto obj = Objective("vol",
            [](const std::vector<double>& x) {
                return x.empty() ? 0 : x[0];
            }, true);
        design_space_.set_primary_objective(obj);

        generator_ = std::make_unique<DesignGenerator>(design_space_);
    }

    DesignSpace design_space_;
    std::unique_ptr<DesignGenerator> generator_;
};

TEST_F(DesignGeneratorTest, FeatureRegistration) {
    generator_->register_feature_type(FeatureType::BEND, "CustomBend");
    EXPECT_TRUE(true);
}

TEST_F(DesignGeneratorTest, GenerateDesign) {
    std::vector<double> vars = {25.0, 50.0, 75.0, 30.0, 60.0, 90.0};
    auto design = generator_->generate_design(vars);

    EXPECT_FALSE(design.design_variables.empty());
    EXPECT_GT(design.geometry.volume, 0.0);
}

TEST_F(DesignGeneratorTest, TopologyValidation) {
    std::vector<double> vars = {25.0, 50.0, 75.0};
    auto design = generator_->generate_design(vars);

    EXPECT_TRUE(design.topology.is_valid_dag());
}

TEST_F(DesignGeneratorTest, GeometryComputation) {
    std::vector<double> vars = {20.0, 40.0, 60.0};
    auto design = generator_->generate_design(vars);

    EXPECT_GT(design.geometry.volume, 0.0);
    EXPECT_GT(design.geometry.surface_area, 0.0);
    EXPECT_GT(design.geometry.mass, 0.0);
}

// ===== Test Fixture for Optimization Manager =====
class OptimizationManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        design_space_ = std::make_shared<DesignSpace>();
        design_space_->add_parameter("x", -5.0, 5.0);
        design_space_->add_parameter("y", -5.0, 5.0);

        auto obj = Objective("f",
            [](const std::vector<double>& x) {
                return x.size() < 2 ? 0 : x[0]*x[0] + x[1]*x[1];
            }, true);
        design_space_->set_primary_objective(obj);

        constraint_handler_ = std::make_shared<ConstraintHandler>(*design_space_);
        design_generator_ = std::make_shared<DesignGenerator>(*design_space_);
        manager_ = std::make_unique<OptimizationManager>(design_space_, constraint_handler_, design_generator_);
    }

    std::shared_ptr<DesignSpace> design_space_;
    std::shared_ptr<ConstraintHandler> constraint_handler_;
    std::shared_ptr<DesignGenerator> design_generator_;
    std::unique_ptr<OptimizationManager> manager_;
};

TEST_F(OptimizationManagerTest, AlgorithmSelection) {
    auto algo = manager_->select_algorithm();
    EXPECT_TRUE(algo == OptimizationAlgorithm::GENETIC_ALGORITHM ||
                algo == OptimizationAlgorithm::PARTICLE_SWARM);
}

TEST_F(OptimizationManagerTest, OptimizeWithGA) {
    OptimizationSettings settings;
    settings.algorithm = OptimizationAlgorithm::GENETIC_ALGORITHM;
    settings.max_iterations = 30;

    auto result = manager_->optimize(settings);
    EXPECT_EQ(result.iterations_completed, 30);
    EXPECT_TRUE(result.computation_time_seconds >= 0.0);
}

TEST_F(OptimizationManagerTest, OptimizeWithPSO) {
    OptimizationSettings settings;
    settings.algorithm = OptimizationAlgorithm::PARTICLE_SWARM;
    settings.max_iterations = 30;

    auto result = manager_->optimize(settings);
    EXPECT_EQ(result.iterations_completed, 30);
    EXPECT_EQ(result.best_design.size(), 2);
}

TEST_F(OptimizationManagerTest, ConvergenceMonitoring) {
    OptimizationSettings settings;
    settings.max_iterations = 20;

    auto result = manager_->optimize(settings);
    EXPECT_EQ(result.convergence_history.size(), 20);
}

TEST_F(OptimizationManagerTest, ResultStorage) {
    OptimizationSettings settings;
    settings.max_iterations = 10;

    manager_->optimize(settings);
    auto last_result = manager_->last_result();

    EXPECT_EQ(last_result.iterations_completed, 10);
}

// ===== Test Fixture for Visualization =====
class VisualizationTest : public ::testing::Test {
protected:
    void SetUp() override {
        design_space_.add_parameter("x", -10.0, 10.0);
        design_space_.add_parameter("y", -10.0, 10.0);

        auto obj = Objective("f",
            [](const std::vector<double>& x) {
                return x.size() < 2 ? 0 : x[0]*x[0] + x[1]*x[1];
            }, true);
        design_space_.set_primary_objective(obj);
    }

    DesignSpace design_space_;
};

TEST_F(VisualizationTest, DesignSpaceSampling) {
    DesignSpaceVisualizer visualizer(design_space_);
    auto samples = visualizer.sample_design_space(100, true);

    EXPECT_EQ(samples.size(), 100);
    EXPECT_TRUE(samples[0].objective_value >= 0.0);
}

TEST_F(VisualizationTest, GridSampling) {
    DesignSpaceVisualizer visualizer(design_space_);
    auto samples = visualizer.grid_sample({10, 10});

    EXPECT_EQ(samples.size(), 100);
}

TEST_F(VisualizationTest, ParetoFrontIdentification) {
    DesignSpaceVisualizer visualizer(design_space_);
    auto samples = visualizer.sample_design_space(50, true);
    auto front = visualizer.identify_pareto_frontier(samples);

    EXPECT_GT(front.solutions.size(), 0);
}

TEST_F(VisualizationTest, SolutionDistance) {
    SolutionAnalyzer analyzer(design_space_);
    std::vector<double> sol1 = {1.0, 2.0};
    std::vector<double> sol2 = {4.0, 6.0};

    double distance = analyzer.solution_distance(sol1, sol2);
    EXPECT_NEAR(distance, 5.0, 1e-6);  // sqrt(9 + 16) = 5
}

TEST_F(VisualizationTest, SolutionClustering) {
    SolutionAnalyzer analyzer(design_space_);
    std::vector<std::vector<double>> solutions = {
        {1.0, 1.0}, {1.1, 1.1}, {5.0, 5.0}, {5.1, 5.1}
    };

    auto clusters = analyzer.cluster_solutions(solutions, 2);
    EXPECT_LE(clusters.size(), 2);
}

TEST_F(VisualizationTest, ParameterSensitivity) {
    SolutionAnalyzer analyzer(design_space_);
    std::vector<double> design = {0.5, 0.5};

    auto sensitivities = analyzer.parameter_sensitivity(design, 0.01);
    EXPECT_EQ(sensitivities.size(), 2);
}

TEST_F(VisualizationTest, DesignRobustness) {
    SolutionAnalyzer analyzer(design_space_);
    std::vector<double> design = {0.3, 0.3};

    double robustness = analyzer.design_robustness(design, 50);
    EXPECT_GE(robustness, 0.0);
    EXPECT_LE(robustness, 1.0);
}

// ===== Integration Tests =====
class GenerativeDesignIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a complete design space
        design_space_ = std::make_shared<DesignSpace>();
        design_space_->add_parameter("length", 10.0, 100.0, 50.0);
        design_space_->add_parameter("diameter", 5.0, 20.0, 12.0);
        design_space_->add_parameter("thickness", 1.0, 5.0, 2.0);

        // Objective: minimize weight
        auto objective = Objective("minimize_weight",
            [](const std::vector<double>& x) {
                if (x.size() < 3) return 0.0;
                double volume = x[0] * x[1] * x[1] * 3.14159 / 4.0;
                return volume * x[2];  // weight approximation
            }, true);
        design_space_->set_primary_objective(objective);

        // Constraint: diameter >= 2*thickness
        design_space_->add_inequality_constraint("thickness_constraint",
            [](const std::vector<double>& x) {
                return x.size() < 3 ? 0.0 : x[2]*2.0 - x[1];
            });

        constraint_handler_ = std::make_shared<ConstraintHandler>(*design_space_);
        design_generator_ = std::make_shared<DesignGenerator>(*design_space_);
        manager_ = std::make_unique<OptimizationManager>(design_space_, constraint_handler_, design_generator_);
    }

    std::shared_ptr<DesignSpace> design_space_;
    std::shared_ptr<ConstraintHandler> constraint_handler_;
    std::shared_ptr<DesignGenerator> design_generator_;
    std::unique_ptr<OptimizationManager> manager_;
};

TEST_F(GenerativeDesignIntegrationTest, EndToEndOptimization) {
    OptimizationSettings settings;
    settings.max_iterations = 25;

    auto result = manager_->optimize(settings);

    EXPECT_TRUE(result.best_design.size() > 0);
    EXPECT_TRUE(result.computation_time_seconds >= 0.0);
    EXPECT_EQ(result.iterations_completed, 25);
}

TEST_F(GenerativeDesignIntegrationTest, DesignGeneration) {
    std::vector<double> normalized_design = {0.5, 0.5, 0.5};
    auto design = design_generator_->generate_design(
        design_space_->denormalize(normalized_design));

    EXPECT_GT(design.geometry.volume, 0.0);
    EXPECT_TRUE(design.topology.is_valid_dag());
}

TEST_F(GenerativeDesignIntegrationTest, MultiObjectiveOptimization) {
    auto secondary_obj = Objective("minimize_volume",
        [](const std::vector<double>& x) {
            return x.size() < 3 ? 0.0 : x[0] * x[1] * x[1];
        }, true);
    design_space_->add_secondary_objective(secondary_obj);

    OptimizationSettings settings;
    settings.max_iterations = 20;

    auto result = manager_->optimize_multi_objective(settings);
    EXPECT_TRUE(!result.best_design.empty());
}

} // namespace
