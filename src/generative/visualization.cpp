#include "../../include/bendcore/generative/visualization.hpp"
#include <sstream>
#include <cmath>
#include <algorithm>

namespace bendcore::generative {

// DesignSpaceProjection methods
std::string DesignSpaceProjection::to_csv() const {
    std::ostringstream oss;
    oss << "x,y,objective,feasible\n";

    for (const auto& sample : samples) {
        if (param_indices.size() >= 2 && sample.point.size() >= 2) {
            oss << sample.point[param_indices[0]] << ","
                << sample.point[param_indices[1]] << ","
                << sample.objective_value << ","
                << (sample.is_feasible ? "1" : "0") << "\n";
        }
    }

    return oss.str();
}

std::string DesignSpaceProjection::to_json() const {
    std::ostringstream oss;
    oss << "{\n  \"samples\": [\n";

    for (size_t i = 0; i < samples.size(); ++i) {
        const auto& sample = samples[i];
        oss << "    {\n";
        oss << "      \"objective\": " << sample.objective_value << ",\n";
        oss << "      \"feasible\": " << (sample.is_feasible ? "true" : "false") << "\n";
        oss << "    }";
        if (i < samples.size() - 1) oss << ",";
        oss << "\n";
    }

    oss << "  ]\n}";
    return oss.str();
}

// ParetoFrontStatistics methods
std::string ParetoFrontStatistics::to_string() const {
    std::ostringstream oss;
    oss << "Pareto Front Statistics:\n";
    oss << "Cardinality: " << cardinality << "\n";
    oss << "Hypervolume: " << hypervolume << "\n";
    oss << "Spacing: " << spacing << "\n";
    oss << "Uniformity: " << distribution_uniformity;
    return oss.str();
}

// DesignSpaceVisualizer methods
DesignSpaceVisualizer::DesignSpaceVisualizer(const DesignSpace& design_space)
    : design_space_(design_space) {}

std::vector<DesignSpaceSample> DesignSpaceVisualizer::sample_design_space(size_t num_samples,
                                                                         bool random) {
    std::vector<DesignSpaceSample> samples;
    std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<double> dist(0.0, 1.0);

    for (size_t i = 0; i < num_samples; ++i) {
        DesignSpaceSample sample;
        sample.point.resize(design_space_.parameter_count());

        for (size_t j = 0; j < sample.point.size(); ++j) {
            sample.point[j] = dist(rng);
        }

        auto x = design_space_.denormalize(sample.point);
        sample.objective_value = design_space_.primary_objective().compute(x);
        sample.constraint_violation = design_space_.constraint_violation(x);
        sample.is_feasible = (sample.constraint_violation < 1e-6);

        samples.push_back(sample);
    }

    return samples;
}

std::vector<DesignSpaceSample> DesignSpaceVisualizer::grid_sample(const std::vector<size_t>& grid_points) {
    std::vector<DesignSpaceSample> samples;

    // Simple 2D grid sampling
    if (grid_points.size() >= 2) {
        for (size_t i = 0; i < grid_points[0]; ++i) {
            for (size_t j = 0; j < grid_points[1]; ++j) {
                DesignSpaceSample sample;
                sample.point.resize(std::min(static_cast<size_t>(2), design_space_.parameter_count()));

                if (sample.point.size() >= 1) {
                    sample.point[0] = static_cast<double>(i) / grid_points[0];
                }
                if (sample.point.size() >= 2) {
                    sample.point[1] = static_cast<double>(j) / grid_points[1];
                }

                auto x = design_space_.denormalize(sample.point);
                sample.objective_value = design_space_.primary_objective().compute(x);
                sample.constraint_violation = design_space_.constraint_violation(x);
                sample.is_feasible = (sample.constraint_violation < 1e-6);

                samples.push_back(sample);
            }
        }
    }

    return samples;
}

DesignSpaceProjection DesignSpaceVisualizer::project_2d(const std::vector<DesignSpaceSample>& samples,
                                                       size_t param1_idx, size_t param2_idx) {
    DesignSpaceProjection projection;
    projection.param_indices = {param1_idx, param2_idx};
    projection.samples = samples;
    return projection;
}

DesignSpaceProjection DesignSpaceVisualizer::project_3d(const std::vector<DesignSpaceSample>& samples,
                                                       size_t param1_idx, size_t param2_idx, size_t param3_idx) {
    DesignSpaceProjection projection;
    projection.param_indices = {param1_idx, param2_idx, param3_idx};
    projection.samples = samples;
    return projection;
}

ParetoFront DesignSpaceVisualizer::identify_pareto_frontier(const std::vector<DesignSpaceSample>& samples) {
    ParetoFront front;

    for (const auto& sample : samples) {
        bool is_dominated = false;

        for (const auto& other : front.solutions) {
            auto x_other = design_space_.denormalize(other);
            double obj_other = design_space_.primary_objective().compute(x_other);

            if (obj_other > sample.objective_value) {
                is_dominated = true;
                break;
            }
        }

        if (!is_dominated) {
            front.solutions.push_back(sample.point);
            front.primary_objectives.push_back(sample.objective_value);
        }
    }

    return front;
}

ParetoFrontStatistics DesignSpaceVisualizer::compute_pareto_statistics(const ParetoFront& front) {
    ParetoFrontStatistics stats;
    stats.cardinality = front.solutions.size();

    // Compute hypervolume approximation
    double hv = 0.0;
    if (!front.primary_objectives.empty()) {
        double max_obj = *std::max_element(front.primary_objectives.begin(),
                                          front.primary_objectives.end());
        hv = max_obj * max_obj;
    }
    stats.hypervolume = hv;

    // Compute spacing
    double spacing_sum = 0.0;
    for (size_t i = 1; i < front.primary_objectives.size(); ++i) {
        spacing_sum += std::fabs(front.primary_objectives[i] - front.primary_objectives[i-1]);
    }
    stats.spacing = spacing_sum / front.solutions.size();

    // Distribution uniformity
    stats.distribution_uniformity = spacing_sum > 0.0 ? 1.0 / (spacing_sum + 1.0) : 0.0;

    return stats;
}

std::string DesignSpaceVisualizer::generate_scatter_plot_data(const std::vector<DesignSpaceSample>& samples) {
    return sample_design_space(samples.size(), false).empty() ? "" : "scatter_data";
}

std::string DesignSpaceVisualizer::generate_convergence_plot_data(const std::vector<double>& history) {
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < history.size(); ++i) {
        oss << history[i];
        if (i < history.size() - 1) oss << ",";
    }
    oss << "]";
    return oss.str();
}

std::string DesignSpaceVisualizer::generate_heatmap_data(const DesignSpaceProjection& projection) {
    return projection.to_csv();
}

// SolutionAnalyzer methods
SolutionAnalyzer::SolutionAnalyzer(const DesignSpace& design_space)
    : design_space_(design_space) {}

double SolutionAnalyzer::solution_distance(const std::vector<double>& sol1,
                                          const std::vector<double>& sol2) const {
    if (sol1.size() != sol2.size()) return 0.0;

    double distance = 0.0;
    for (size_t i = 0; i < sol1.size(); ++i) {
        double diff = sol1[i] - sol2[i];
        distance += diff * diff;
    }
    return std::sqrt(distance);
}

std::vector<double> SolutionAnalyzer::solution_differences(const std::vector<double>& sol1,
                                                          const std::vector<double>& sol2) const {
    std::vector<double> differences;
    if (sol1.size() != sol2.size()) return differences;

    for (size_t i = 0; i < sol1.size(); ++i) {
        differences.push_back(std::fabs(sol1[i] - sol2[i]));
    }
    return differences;
}

std::vector<SolutionCluster> SolutionAnalyzer::cluster_solutions(const std::vector<std::vector<double>>& solutions,
                                                               size_t num_clusters) {
    std::vector<SolutionCluster> clusters(num_clusters);

    if (solutions.empty()) return clusters;

    // Simple k-means clustering
    for (size_t i = 0; i < solutions.size(); ++i) {
        double min_distance = std::numeric_limits<double>::max();
        size_t best_cluster = 0;

        for (size_t c = 0; c < num_clusters; ++c) {
            if (clusters[c].members.empty()) {
                best_cluster = c;
                break;
            }

            double dist = solution_distance(solutions[i], clusters[c].center);
            if (dist < min_distance) {
                min_distance = dist;
                best_cluster = c;
            }
        }

        clusters[best_cluster].members.push_back(solutions[i]);
    }

    return clusters;
}

std::vector<std::vector<double>> SolutionAnalyzer::compute_trade_offs(const ParetoFront& front) {
    std::vector<std::vector<double>> trade_offs;

    for (size_t i = 0; i < front.solutions.size(); ++i) {
        std::vector<double> trade_off;
        for (size_t j = 0; j < front.solutions[i].size(); ++j) {
            trade_off.push_back(front.solutions[i][j]);
        }
        trade_offs.push_back(trade_off);
    }

    return trade_offs;
}

std::string SolutionAnalyzer::generate_trade_off_report(const ParetoFront& front) {
    std::ostringstream oss;
    oss << "Trade-off Analysis Report\n";
    oss << "Number of Pareto optimal solutions: " << front.solutions.size() << "\n";
    oss << "Objectives:\n";

    for (size_t i = 0; i < front.primary_objectives.size(); ++i) {
        oss << "  Solution " << i << ": " << front.primary_objectives[i] << "\n";
    }

    return oss.str();
}

std::vector<double> SolutionAnalyzer::parameter_sensitivity(const std::vector<double>& base_design,
                                                           double perturbation) {
    std::vector<double> sensitivities;

    auto x = design_space_.denormalize(base_design);
    double base_objective = design_space_.primary_objective().compute(x);

    for (size_t i = 0; i < base_design.size(); ++i) {
        auto perturbed = base_design;
        perturbed[i] += perturbation;

        auto x_perturbed = design_space_.denormalize(perturbed);
        double perturbed_objective = design_space_.primary_objective().compute(x_perturbed);

        double sensitivity = (perturbed_objective - base_objective) / perturbation;
        sensitivities.push_back(sensitivity);
    }

    return sensitivities;
}

std::map<std::string, double> SolutionAnalyzer::objective_sensitivity(const std::vector<double>& base_design) {
    std::map<std::string, double> sensitivities;
    sensitivities["primary"] = 1.0;  // Placeholder
    return sensitivities;
}

std::string SolutionAnalyzer::compare_designs(const std::vector<std::vector<double>>& designs) {
    std::ostringstream oss;
    oss << "Design Comparison:\n";
    oss << "Number of designs: " << designs.size() << "\n";

    for (size_t i = 0; i < designs.size(); ++i) {
        oss << "Design " << i << ": ";
        auto x = design_space_.denormalize(designs[i]);
        oss << design_space_.primary_objective().compute(x) << "\n";
    }

    return oss.str();
}

std::vector<double> SolutionAnalyzer::design_performance_metrics(const std::vector<double>& design) {
    std::vector<double> metrics;
    auto x = design_space_.denormalize(design);
    metrics.push_back(design_space_.primary_objective().compute(x));
    metrics.push_back(design_space_.constraint_violation(x));
    return metrics;
}

double SolutionAnalyzer::design_robustness(const std::vector<double>& design, size_t num_samples) {
    auto x = design_space_.denormalize(design);
    double base_objective = design_space_.primary_objective().compute(x);

    double variance = 0.0;
    std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<double> dist(-0.01, 0.01);

    for (size_t i = 0; i < num_samples; ++i) {
        auto perturbed = x;
        for (auto& val : perturbed) {
            val += dist(rng);
        }

        double obj = design_space_.primary_objective().compute(perturbed);
        variance += (obj - base_objective) * (obj - base_objective);
    }

    variance /= num_samples;
    return 1.0 / (1.0 + variance);
}

// GenerativeDesignReport methods
GenerativeDesignReport::GenerativeDesignReport(const DesignSpace& design_space,
                                              const OptimizationManager& optimizer)
    : design_space_(design_space), optimizer_(optimizer) {}

std::string GenerativeDesignReport::generate_summary_report(const OptimizationResult& result) {
    return result.to_string();
}

std::string GenerativeDesignReport::generate_detailed_report(const OptimizationResult& result,
                                                            const std::vector<DesignSpaceSample>& samples) {
    std::ostringstream oss;
    oss << "Detailed Optimization Report\n";
    oss << "============================\n\n";
    oss << result.to_string() << "\n\n";
    oss << "Samples analyzed: " << samples.size() << "\n";
    return oss.str();
}

std::string GenerativeDesignReport::generate_html_report(const OptimizationResult& result,
                                                        const ParetoFront& front) {
    std::ostringstream oss;
    oss << "<!DOCTYPE html>\n<html>\n<head>\n<title>Generative Design Report</title>\n</head>\n";
    oss << "<body>\n<h1>Generative Design Report</h1>\n";
    oss << "<p>Best objective: " << result.best_objective_value << "</p>\n";
    oss << "<p>Iterations: " << result.iterations_completed << "</p>\n";
    oss << "</body>\n</html>";
    return oss.str();
}

void GenerativeDesignReport::export_to_csv(const std::string& filename,
                                          const std::vector<std::vector<double>>& designs) {
    // Placeholder implementation
}

void GenerativeDesignReport::export_to_json(const std::string& filename,
                                           const OptimizationResult& result) {
    // Placeholder implementation
}

void GenerativeDesignReport::export_visualization(const std::string& filename,
                                                 const DesignSpaceProjection& projection) {
    // Placeholder implementation
}

} // namespace bendcore::generative
