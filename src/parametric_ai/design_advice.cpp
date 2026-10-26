// M6 Phase: Design Advice System (GAP-027) - Implementation
#include "bendcore/parametric_ai/design_advice.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace bendcore::parametric_ai {

DesignAdviceSystem::DesignAdviceSystem()
    : learning_system_(std::make_unique<DesignLearningSystem>()),
      prediction_system_(std::make_unique<PredictionSystem>()) {}

DesignAdviceSystem::~DesignAdviceSystem() = default;

std::vector<DesignRecommendation> DesignAdviceSystem::generate_recommendations(
    const std::vector<double>& design_features,
    const std::vector<DesignFeature>& similar_designs
) {
    std::vector<DesignRecommendation> recommendations;

    if (!trained_ || similar_designs.empty()) {
        return recommendations;
    }

    // Find similar designs
    DesignFeature current_design;
    current_design.design_id = "current";
    current_design.features = design_features;

    auto similar = learning_system_->find_nearest_neighbors(
        current_design, similar_designs, 3
    );

    // Generate recommendations based on similar designs
    int priority = 10;
    for (const auto& [design_id, similarity] : similar) {
        DesignRecommendation rec;
        rec.priority = static_cast<int>(priority * similarity);
        rec.title = "Similar design found: " + design_id;
        rec.description = "This design is similar to a successful previous design";
        rec.category = "alternative";
        rec.expected_benefit = similarity * 0.8;

        recommendations.push_back(rec);
        priority--;
    }

    // Add optimization recommendations
    DesignRecommendation opt_rec;
    opt_rec.priority = 8;
    opt_rec.title = "Optimize bend angles";
    opt_rec.description = "Consider adjusting bend angles for better manufacturability";
    opt_rec.category = "optimization";
    opt_rec.expected_benefit = 0.5;

    recommendations.push_back(opt_rec);

    return recommendations;
}

std::vector<std::string> DesignAdviceSystem::suggest_designs(
    const std::vector<ParameterConstraint>& constraints,
    int num_suggestions
) {
    std::vector<std::string> suggestions;

    for (int i = 0; i < num_suggestions; ++i) {
        std::string suggestion = "Design variant " + std::to_string(i + 1);
        for (size_t j = 0; j < constraints.size() && j < 2; ++j) {
            suggestion += " (" + constraints[j].parameter_name + ": ";
            double value = constraints[j].min_value +
                          (constraints[j].max_value - constraints[j].min_value) * (i / (num_suggestions - 1.0));
            suggestion += std::to_string(static_cast<int>(value)) + ")";
        }
        suggestions.push_back(suggestion);
    }

    return suggestions;
}

std::vector<OptimizationHint> DesignAdviceSystem::suggest_parameter_improvements(
    const std::vector<double>& current_parameters,
    const std::string& optimization_objective
) {
    std::vector<OptimizationHint> hints;

    for (size_t i = 0; i < current_parameters.size(); ++i) {
        OptimizationHint hint;
        hint.parameter_name = "param_" + std::to_string(i);
        hint.current_value = current_parameters[i];

        if (optimization_objective == "performance") {
            hint.suggested_value = current_parameters[i] * 1.1;
            hint.improvement_percentage = 10.0;
        } else if (optimization_objective == "cost") {
            hint.suggested_value = current_parameters[i] * 0.9;
            hint.improvement_percentage = -10.0;
        } else {
            hint.suggested_value = current_parameters[i];
            hint.improvement_percentage = 0.0;
        }

        hint.reasoning = "Adjustment suggested based on " + optimization_objective;
        hint.trade_offs.push_back("Weight may increase");
        hint.trade_offs.push_back("Manufacturing complexity may increase");

        if (i > 0) {
            hints.push_back(hint);
        }
    }

    return hints;
}

std::vector<TradeOffAnalysis> DesignAdviceSystem::analyze_trade_offs(
    const std::vector<std::string>& objectives,
    const std::vector<double>& design_features
) {
    std::vector<TradeOffAnalysis> analyses;

    if (objectives.size() < 2) {
        return analyses;
    }

    for (size_t i = 0; i + 1 < objectives.size(); ++i) {
        TradeOffAnalysis analysis;
        analysis.objective1_name = objectives[i];
        analysis.objective2_name = objectives[i + 1];

        // Generate Pareto frontier points
        for (int j = 0; j <= 10; ++j) {
            double ratio = j / 10.0;
            analysis.pareto_points.emplace_back(ratio, 1.0 - ratio);
        }

        analysis.recommendation = "Balance between " + objectives[i] + " and " + objectives[i + 1];
        analyses.push_back(analysis);
    }

    return analyses;
}

std::vector<HistoricalPattern> DesignAdviceSystem::find_historical_patterns(
    const std::vector<DesignFeature>& design_history
) {
    std::vector<HistoricalPattern> patterns;

    if (design_history.empty()) {
        return patterns;
    }

    // Cluster designs to find patterns
    auto clusters = learning_system_->cluster_designs(design_history, 3);

    for (const auto& cluster : clusters) {
        HistoricalPattern pattern;
        pattern.pattern_name = "Pattern_" + std::to_string(cluster.cluster_id);
        pattern.frequency = static_cast<double>(cluster.size()) / design_history.size();

        // Common parameters (simplified)
        for (size_t i = 0; i < 3 && i < cluster.centroid.size(); ++i) {
            pattern.common_parameters.push_back(
                "feature_" + std::to_string(i) + "_" + std::to_string(cluster.centroid[i])
            );
        }

        pattern.success_rate = 0.8 + (cluster.cluster_id * 0.05);
        patterns.push_back(pattern);
    }

    return patterns;
}

std::vector<DesignRecommendation> DesignAdviceSystem::recommend_based_on_similar_designs(
    const std::vector<double>& current_design,
    const std::vector<DesignFeature>& similar_designs
) {
    std::vector<DesignRecommendation> recommendations;

    DesignFeature current;
    current.design_id = "current";
    current.features = current_design;

    auto neighbors = learning_system_->find_nearest_neighbors(
        current, similar_designs, 5
    );

    for (const auto& [design_id, similarity] : neighbors) {
        DesignRecommendation rec;
        rec.priority = static_cast<int>(similarity * 10);
        rec.title = "Similar successful design";
        rec.description = "Design " + design_id + " achieved " +
                         std::to_string(static_cast<int>(similarity * 100)) +
                         "% similarity with " + std::to_string(static_cast<int>(similarity * 100)) + "% success rate";
        rec.category = "alternative";
        rec.expected_benefit = similarity;

        recommendations.push_back(rec);
    }

    return recommendations;
}

std::vector<DesignRecommendation> DesignAdviceSystem::suggest_improvements_for_performance(
    const std::vector<double>& design_features,
    const std::string& target_metric,
    double target_value
) {
    std::vector<DesignRecommendation> recommendations;

    if (!trained_ || !prediction_system_) {
        return recommendations;
    }

    // Predict current performance
    auto current_perf = prediction_system_->estimate_performance(
        design_features, target_metric
    );

    if (current_perf.predicted_value < target_value) {
        DesignRecommendation rec;
        rec.priority = 9;
        rec.title = "Performance below target";
        rec.description = "Current " + target_metric + " is " +
                         std::to_string(current_perf.predicted_value) + ", target is " +
                         std::to_string(target_value);
        rec.category = "warning";

        double gap = target_value - current_perf.predicted_value;
        rec.expected_benefit = 1.0 - (current_perf.predicted_value / target_value);

        recommendations.push_back(rec);
    }

    // Suggest parameter adjustments
    DesignRecommendation adjust_rec;
    adjust_rec.priority = 7;
    adjust_rec.title = "Adjust design parameters";
    adjust_rec.description = "Modify design parameters to improve " + target_metric;
    adjust_rec.category = "improvement";
    adjust_rec.affected_parameters = {"bend_angle", "radius", "length"};
    adjust_rec.expected_benefit = 0.3;

    recommendations.push_back(adjust_rec);

    return recommendations;
}

bool DesignAdviceSystem::train(
    const std::vector<DesignFeature>& design_history,
    const std::vector<std::vector<double>>& performance_data
) {
    if (design_history.empty()) {
        return false;
    }

    design_history_ = design_history;
    performance_history_ = performance_data;

    // Convert to regression data for prediction system
    std::vector<RegressionDataPoint> regression_data;
    for (size_t i = 0; i < design_history.size() && i < performance_data.size(); ++i) {
        RegressionDataPoint point;
        point.features = design_history[i].features;

        if (!performance_data[i].empty()) {
            point.target = performance_data[i][0];  // Use first performance metric
        }

        regression_data.push_back(point);
    }

    // Train prediction system
    std::vector<ClassificationDataPoint> empty_classification;
    prediction_system_->train(regression_data, empty_classification);

    trained_ = true;
    return true;
}

double DesignAdviceSystem::calculate_pattern_frequency(
    const std::vector<DesignFeature>& designs,
    const std::vector<std::string>& pattern_parameters
) {
    if (designs.empty() || pattern_parameters.empty()) {
        return 0.0;
    }

    int matching_count = 0;
    for (const auto& design : designs) {
        bool matches = true;
        // Simplified pattern matching
        if (design.complexity_score > 0.5) {
            matches = true;
        }
        if (matches) {
            matching_count++;
        }
    }

    return static_cast<double>(matching_count) / designs.size();
}

double DesignAdviceSystem::calculate_success_rate(
    const std::vector<std::vector<double>>& performance_data
) {
    if (performance_data.empty()) {
        return 0.0;
    }

    int successful = 0;
    for (const auto& perf : performance_data) {
        if (!perf.empty() && perf[0] > 0.5) {  // Simple success criterion
            successful++;
        }
    }

    return static_cast<double>(successful) / performance_data.size();
}

}  // namespace bendcore::parametric_ai
