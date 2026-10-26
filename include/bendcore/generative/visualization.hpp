#pragma once

#include <vector>
#include <string>
#include <memory>
#include <map>
#include "design_space.hpp"
#include "optimization_manager.hpp"

namespace bendcore::generative {

/**
 * Visualization & Analysis Module
 * Design space visualization and result analysis
 */

// Design space sample point
struct DesignSpaceSample {
    std::vector<double> point;
    double objective_value = 0.0;
    std::vector<double> secondary_objectives;
    double constraint_violation = 0.0;
    bool is_feasible = false;
    int cluster_id = -1;
};

// 2D projection of design space
struct DesignSpaceProjection {
    std::vector<DesignSpaceSample> samples;
    std::vector<size_t> param_indices;  // Which parameters are projected

    [[nodiscard]] std::string to_csv() const;
    [[nodiscard]] std::string to_json() const;
};

// Pareto front statistics
struct ParetoFrontStatistics {
    double hypervolume = 0.0;
    double spacing = 0.0;
    double distribution_uniformity = 0.0;
    double cardinality = 0;
    std::vector<double> extreme_points;

    [[nodiscard]] std::string to_string() const;
};

// Solution clustering result
struct SolutionCluster {
    std::vector<std::vector<double>> members;
    std::vector<double> center;
    double radius = 0.0;
    std::vector<double> mean_objectives;
    std::vector<double> std_dev_objectives;

    [[nodiscard]] bool is_valid() const { return !members.empty(); }
};

// Design space visualizer
class DesignSpaceVisualizer {
public:
    explicit DesignSpaceVisualizer(const DesignSpace& design_space);

    // Design space sampling
    [[nodiscard]] std::vector<DesignSpaceSample> sample_design_space(size_t num_samples,
                                                                    bool random = true);
    [[nodiscard]] std::vector<DesignSpaceSample> grid_sample(const std::vector<size_t>& grid_points);

    // Projection and visualization
    [[nodiscard]] DesignSpaceProjection project_2d(const std::vector<DesignSpaceSample>& samples,
                                                  size_t param1_idx, size_t param2_idx);
    [[nodiscard]] DesignSpaceProjection project_3d(const std::vector<DesignSpaceSample>& samples,
                                                  size_t param1_idx, size_t param2_idx, size_t param3_idx);

    // Pareto frontier
    [[nodiscard]] ParetoFront identify_pareto_frontier(const std::vector<DesignSpaceSample>& samples);
    [[nodiscard]] ParetoFrontStatistics compute_pareto_statistics(const ParetoFront& front);

    // Visualization output
    [[nodiscard]] std::string generate_scatter_plot_data(const std::vector<DesignSpaceSample>& samples);
    [[nodiscard]] std::string generate_convergence_plot_data(const std::vector<double>& history);
    [[nodiscard]] std::string generate_heatmap_data(const DesignSpaceProjection& projection);

private:
    const DesignSpace& design_space_;
};

// Solution analyzer
class SolutionAnalyzer {
public:
    explicit SolutionAnalyzer(const DesignSpace& design_space);

    // Solution comparison
    [[nodiscard]] double solution_distance(const std::vector<double>& sol1,
                                          const std::vector<double>& sol2) const;
    [[nodiscard]] std::vector<double> solution_differences(const std::vector<double>& sol1,
                                                          const std::vector<double>& sol2) const;

    // Clustering
    [[nodiscard]] std::vector<SolutionCluster> cluster_solutions(const std::vector<std::vector<double>>& solutions,
                                                               size_t num_clusters = 3);

    // Trade-off analysis
    [[nodiscard]] std::vector<std::vector<double>> compute_trade_offs(const ParetoFront& front);
    [[nodiscard]] std::string generate_trade_off_report(const ParetoFront& front);

    // Sensitivity analysis
    [[nodiscard]] std::vector<double> parameter_sensitivity(const std::vector<double>& base_design,
                                                           double perturbation = 0.01);
    [[nodiscard]] std::map<std::string, double> objective_sensitivity(const std::vector<double>& base_design);

    // Design comparison
    [[nodiscard]] std::string compare_designs(const std::vector<std::vector<double>>& designs);
    [[nodiscard]] std::vector<double> design_performance_metrics(const std::vector<double>& design);

    // Robustness analysis
    [[nodiscard]] double design_robustness(const std::vector<double>& design,
                                          size_t num_samples = 100);

private:
    const DesignSpace& design_space_;
};

// Report generator
class GenerativeDesignReport {
public:
    GenerativeDesignReport(const DesignSpace& design_space,
                          const OptimizationManager& optimizer);

    // Report generation
    [[nodiscard]] std::string generate_summary_report(const OptimizationResult& result);
    [[nodiscard]] std::string generate_detailed_report(const OptimizationResult& result,
                                                      const std::vector<DesignSpaceSample>& samples);
    [[nodiscard]] std::string generate_html_report(const OptimizationResult& result,
                                                  const ParetoFront& front);

    // Export functions
    void export_to_csv(const std::string& filename, const std::vector<std::vector<double>>& designs);
    void export_to_json(const std::string& filename, const OptimizationResult& result);
    void export_visualization(const std::string& filename, const DesignSpaceProjection& projection);

private:
    const DesignSpace& design_space_;
    const OptimizationManager& optimizer_;
};

} // namespace bendcore::generative
