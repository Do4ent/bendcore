// M6 Phase: Prediction System (GAP-027)
// Parameter inference, property prediction, performance estimation, uncertainty quantification
#pragma once

#include "regression_engine.hpp"
#include "classification_system.hpp"

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <optional>

namespace bendcore::parametric_ai {

// Parameter constraint
struct ParameterConstraint {
    std::string parameter_name;
    double min_value = 0.0;
    double max_value = 1000.0;
    std::string unit;
    bool is_critical = false;
};

// Design property
struct DesignProperty {
    std::string property_name;
    double predicted_value = 0.0;
    double confidence = 0.0;
    double lower_bound = 0.0;
    double upper_bound = 0.0;
    std::string unit;
};

// Prediction result with uncertainty
struct PredictionResult {
    double predicted_value = 0.0;
    double confidence = 0.0;
    double uncertainty = 0.0;  // Standard deviation
    double lower_bound = 0.0;  // 95% confidence interval lower
    double upper_bound = 0.0;  // 95% confidence interval upper
    bool is_valid = true;
    std::string reasoning;
};

// Anomaly detection result
struct AnomalyResult {
    bool is_anomalous = false;
    double anomaly_score = 0.0;  // 0-1 score
    double threshold = 0.5;
    std::vector<std::string> anomaly_indicators;
};

// Prediction System
class PredictionSystem {
public:
    PredictionSystem();
    ~PredictionSystem();

    // Parameter inference from constraints
    std::map<std::string, PredictionResult> infer_parameters(
        const std::vector<ParameterConstraint>& constraints,
        const std::vector<double>& observed_features
    );

    // Property prediction
    std::vector<DesignProperty> predict_properties(
        const std::vector<double>& design_features
    );

    // Performance estimation
    PredictionResult estimate_performance(
        const std::vector<double>& design_features,
        const std::string& performance_metric
    );

    // Design recommendation based on constraints
    std::map<std::string, double> recommend_parameters(
        const std::vector<ParameterConstraint>& constraints,
        const std::string& optimization_objective = "balanced"
    );

    // Uncertainty quantification using Monte Carlo
    PredictionResult predict_with_uncertainty(
        const std::vector<double>& features,
        int num_samples = 1000
    );

    // Anomaly detection
    AnomalyResult detect_anomaly(
        const std::vector<double>& features,
        double threshold = 0.5
    );

    // Batch prediction
    std::vector<PredictionResult> batch_predict(
        const std::vector<std::vector<double>>& features_list
    );

    // Train system with design history
    bool train(
        const std::vector<RegressionDataPoint>& regression_data,
        const std::vector<ClassificationDataPoint>& classification_data
    );

    bool is_trained() const { return trained_; }

private:
    std::unique_ptr<RegressionEngine> regression_engine_;
    std::unique_ptr<ClassificationSystem> classification_system_;
    bool trained_ = false;

    // Reference statistics for anomaly detection
    std::vector<double> feature_mean_;
    std::vector<double> feature_std_;

    static double mahalanobis_distance(
        const std::vector<double>& x,
        const std::vector<double>& mean,
        const std::vector<double>& std
    );

    // Monte Carlo sampling
    std::vector<double> sample_with_noise(
        const std::vector<double>& features,
        double noise_level = 0.05
    ) const;
};

}  // namespace bendcore::parametric_ai
