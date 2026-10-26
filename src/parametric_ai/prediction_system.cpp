// M6 Phase: Prediction System (GAP-027) - Implementation
#include "bendcore/parametric_ai/prediction_system.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>
#include <limits>

namespace bendcore::parametric_ai {

PredictionSystem::PredictionSystem()
    : regression_engine_(std::make_unique<RegressionEngine>()),
      classification_system_(std::make_unique<ClassificationSystem>()) {}

PredictionSystem::~PredictionSystem() = default;

std::map<std::string, PredictionResult> PredictionSystem::infer_parameters(
    const std::vector<ParameterConstraint>& constraints,
    const std::vector<double>& observed_features
) {
    std::map<std::string, PredictionResult> inferred;

    if (!trained_ || !regression_engine_) {
        return inferred;
    }

    for (const auto& constraint : constraints) {
        double predicted = regression_engine_->predict(observed_features);

        // Clamp to constraint bounds
        predicted = std::max(constraint.min_value, std::min(constraint.max_value, predicted));

        PredictionResult result;
        result.predicted_value = predicted;
        result.confidence = 0.7;
        result.uncertainty = (constraint.max_value - constraint.min_value) * 0.1;
        result.lower_bound = constraint.min_value;
        result.upper_bound = constraint.max_value;
        result.is_valid = true;
        result.reasoning = "Parameter inferred from observed features";

        inferred[constraint.parameter_name] = result;
    }

    return inferred;
}

std::vector<DesignProperty> PredictionSystem::predict_properties(
    const std::vector<double>& design_features
) {
    std::vector<DesignProperty> properties;

    if (!trained_ || !regression_engine_) {
        return properties;
    }

    // Predict various design properties
    std::vector<std::string> property_names = {
        "weight", "stress", "deflection", "cost", "manufacturability"
    };

    for (const auto& prop_name : property_names) {
        double predicted = regression_engine_->predict(design_features);

        DesignProperty prop;
        prop.property_name = prop_name;
        prop.predicted_value = std::max(0.0, predicted);
        prop.confidence = 0.65;
        prop.lower_bound = predicted * 0.8;
        prop.upper_bound = predicted * 1.2;

        properties.push_back(prop);
    }

    return properties;
}

PredictionResult PredictionSystem::estimate_performance(
    const std::vector<double>& design_features,
    const std::string& performance_metric
) {
    PredictionResult result;

    if (!trained_ || !regression_engine_) {
        return result;
    }

    result.predicted_value = regression_engine_->predict(design_features);
    result.confidence = 0.7;
    result.uncertainty = std::abs(result.predicted_value) * 0.15;
    result.lower_bound = result.predicted_value - result.uncertainty;
    result.upper_bound = result.predicted_value + result.uncertainty;
    result.is_valid = true;
    result.reasoning = "Performance estimated from design features";

    return result;
}

std::map<std::string, double> PredictionSystem::recommend_parameters(
    const std::vector<ParameterConstraint>& constraints,
    const std::string& optimization_objective
) {
    std::map<std::string, double> recommendations;

    // Generate candidate parameters
    for (const auto& constraint : constraints) {
        double recommended;

        if (optimization_objective == "minimum") {
            recommended = constraint.min_value;
        } else if (optimization_objective == "maximum") {
            recommended = constraint.max_value;
        } else {  // balanced (default)
            recommended = (constraint.min_value + constraint.max_value) / 2.0;
        }

        recommendations[constraint.parameter_name] = recommended;
    }

    return recommendations;
}

PredictionResult PredictionSystem::predict_with_uncertainty(
    const std::vector<double>& features,
    int num_samples
) {
    PredictionResult result;

    if (!trained_ || !regression_engine_) {
        return result;
    }

    // Base prediction
    double base_prediction = regression_engine_->predict(features);
    result.predicted_value = base_prediction;

    // Monte Carlo sampling for uncertainty
    std::vector<double> samples;
    for (int i = 0; i < num_samples; ++i) {
        auto noisy_features = sample_with_noise(features, 0.05);
        double sample = regression_engine_->predict(noisy_features);
        samples.push_back(sample);
    }

    // Calculate statistics
    std::sort(samples.begin(), samples.end());

    double mean = std::accumulate(samples.begin(), samples.end(), 0.0) / num_samples;
    double variance = 0.0;
    for (double s : samples) {
        variance += (s - mean) * (s - mean);
    }
    variance /= num_samples;
    result.uncertainty = std::sqrt(variance);
    result.confidence = 0.75;

    // 95% confidence interval
    int lower_idx = static_cast<int>(0.025 * num_samples);
    int upper_idx = static_cast<int>(0.975 * num_samples);
    result.lower_bound = samples[lower_idx];
    result.upper_bound = samples[upper_idx];
    result.is_valid = true;

    return result;
}

AnomalyResult PredictionSystem::detect_anomaly(
    const std::vector<double>& features,
    double threshold
) {
    AnomalyResult result;
    result.threshold = threshold;

    if (feature_mean_.empty() || feature_std_.empty()) {
        return result;
    }

    double distance = mahalanobis_distance(features, feature_mean_, feature_std_);
    result.anomaly_score = std::min(1.0, distance / 10.0);  // Normalize to 0-1
    result.is_anomalous = result.anomaly_score > threshold;

    if (result.is_anomalous) {
        result.anomaly_indicators.push_back("High Mahalanobis distance");
    }

    return result;
}

std::vector<PredictionResult> PredictionSystem::batch_predict(
    const std::vector<std::vector<double>>& features_list
) {
    std::vector<PredictionResult> results;

    if (!trained_ || !regression_engine_) {
        return results;
    }

    for (const auto& features : features_list) {
        auto result = estimate_performance(features, "general");
        results.push_back(result);
    }

    return results;
}

bool PredictionSystem::train(
    const std::vector<RegressionDataPoint>& regression_data,
    const std::vector<ClassificationDataPoint>& classification_data
) {
    if (regression_data.empty() && classification_data.empty()) {
        return false;
    }

    // Train regression engine
    if (!regression_data.empty()) {
        std::map<std::string, double> params;
        params["learning_rate"] = 0.01;
        params["iterations"] = 100;
        regression_engine_->train(
            RegressionEngine::RegressionType::LINEAR,
            regression_data,
            params
        );

        // Calculate feature statistics for anomaly detection
        if (regression_data[0].features.size() > 0) {
            int num_features = regression_data[0].features.size();
            feature_mean_.resize(num_features, 0.0);
            feature_std_.resize(num_features, 0.0);

            // Calculate mean
            for (const auto& point : regression_data) {
                for (size_t i = 0; i < point.features.size() && i < feature_mean_.size(); ++i) {
                    feature_mean_[i] += point.features[i];
                }
            }
            for (auto& m : feature_mean_) {
                m /= regression_data.size();
            }

            // Calculate std
            for (const auto& point : regression_data) {
                for (size_t i = 0; i < point.features.size() && i < feature_std_.size(); ++i) {
                    double diff = point.features[i] - feature_mean_[i];
                    feature_std_[i] += diff * diff;
                }
            }
            for (auto& s : feature_std_) {
                s = std::sqrt(s / regression_data.size());
                if (s < 1e-10) s = 1e-10;
            }
        }
    }

    // Train classification engine
    if (!classification_data.empty()) {
        std::map<std::string, double> params;
        params["k"] = 3;
        classification_system_->train(
            ClassificationSystem::ClassifierType::KNN,
            classification_data,
            params
        );
    }

    trained_ = true;
    return true;
}

double PredictionSystem::mahalanobis_distance(
    const std::vector<double>& x,
    const std::vector<double>& mean,
    const std::vector<double>& std
) {
    if (x.size() != mean.size() || x.size() != std.size()) {
        return std::numeric_limits<double>::max();
    }

    double distance = 0.0;
    for (size_t i = 0; i < x.size(); ++i) {
        double z = (x[i] - mean[i]) / std[i];
        distance += z * z;
    }

    return std::sqrt(distance);
}

std::vector<double> PredictionSystem::sample_with_noise(
    const std::vector<double>& features,
    double noise_level
) const {
    std::vector<double> noisy = features;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<> dis(0.0, noise_level);

    for (auto& f : noisy) {
        f += f * dis(gen);
    }

    return noisy;
}

}  // namespace bendcore::parametric_ai
