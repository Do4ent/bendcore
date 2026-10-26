// M6 Phase: Design Advice System (GAP-027)
// Recommendation generation, design suggestions, parameter optimization hints, trade-off analysis
#pragma once

#include "design_learning.hpp"
#include "prediction_system.hpp"

#include <string>
#include <vector>
#include <map>
#include <memory>

namespace bendcore::parametric_ai {

// Design recommendation
struct DesignRecommendation {
    int priority = 0;  // 0-10, higher is more important
    std::string title;
    std::string description;
    std::string category;  // "optimization", "warning", "improvement", "alternative"
    std::vector<std::string> affected_parameters;
    double expected_benefit = 0.0;  // 0-1 score
    std::string justification;
};

// Parameter optimization hint
struct OptimizationHint {
    std::string parameter_name;
    double current_value = 0.0;
    double suggested_value = 0.0;
    double improvement_percentage = 0.0;
    std::string reasoning;
    std::vector<std::string> trade_offs;
};

// Design trade-off analysis
struct TradeOffAnalysis {
    std::string objective1_name;
    std::string objective2_name;
    double pareto_frontier[2] = {0.0, 0.0};
    std::vector<std::pair<double, double>> pareto_points;
    std::string recommendation;
};

// Historical pattern
struct HistoricalPattern {
    std::string pattern_name;
    double frequency = 0.0;  // How often this pattern appears
    std::vector<std::string> common_parameters;
    double success_rate = 0.0;  // Percentage of successful designs with this pattern
};

// Design Advice System
class DesignAdviceSystem {
public:
    DesignAdviceSystem();
    ~DesignAdviceSystem();

    // Generate design recommendations
    std::vector<DesignRecommendation> generate_recommendations(
        const std::vector<double>& design_features,
        const std::vector<DesignFeature>& similar_designs
    );

    // Design suggestions based on constraints
    std::vector<std::string> suggest_designs(
        const std::vector<ParameterConstraint>& constraints,
        int num_suggestions = 5
    );

    // Parameter optimization hints
    std::vector<OptimizationHint> suggest_parameter_improvements(
        const std::vector<double>& current_parameters,
        const std::string& optimization_objective = "performance"
    );

    // Trade-off analysis
    std::vector<TradeOffAnalysis> analyze_trade_offs(
        const std::vector<std::string>& objectives,
        const std::vector<double>& design_features
    );

    // Find historical patterns
    std::vector<HistoricalPattern> find_historical_patterns(
        const std::vector<DesignFeature>& design_history
    );

    // Similarity-based recommendations
    std::vector<DesignRecommendation> recommend_based_on_similar_designs(
        const std::vector<double>& current_design,
        const std::vector<DesignFeature>& similar_designs
    );

    // Performance prediction suggestions
    std::vector<DesignRecommendation> suggest_improvements_for_performance(
        const std::vector<double>& design_features,
        const std::string& target_metric,
        double target_value
    );

    // Train advice system
    bool train(
        const std::vector<DesignFeature>& design_history,
        const std::vector<std::vector<double>>& performance_data
    );

    bool is_trained() const { return trained_; }

private:
    std::unique_ptr<DesignLearningSystem> learning_system_;
    std::unique_ptr<PredictionSystem> prediction_system_;
    std::vector<DesignFeature> design_history_;
    std::vector<std::vector<double>> performance_history_;
    bool trained_ = false;

    // Helper functions
    static double calculate_pattern_frequency(
        const std::vector<DesignFeature>& designs,
        const std::vector<std::string>& pattern_parameters
    );

    static double calculate_success_rate(
        const std::vector<std::vector<double>>& performance_data
    );
};

}  // namespace bendcore::parametric_ai
