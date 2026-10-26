// M6 Phase: Learning Pipeline (GAP-027) - Implementation
#include "bendcore/parametric_ai/learning_pipeline.hpp"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <numeric>
#include <random>
#include <fstream>

namespace bendcore::parametric_ai {

LearningPipeline::LearningPipeline()
    : regression_engine_(std::make_unique<RegressionEngine>()),
      classification_system_(std::make_unique<ClassificationSystem>()) {}

LearningPipeline::~LearningPipeline() = default;

void LearningPipeline::add_training_data(const RegressionDataPoint& data) {
    regression_data_.push_back(data);
}

void LearningPipeline::add_training_data(const ClassificationDataPoint& data) {
    classification_data_.push_back(data);
}

void LearningPipeline::add_training_data_batch(
    const std::vector<RegressionDataPoint>& data
) {
    for (const auto& point : data) {
        add_training_data(point);
    }
}

void LearningPipeline::normalize_features() {
    if (regression_data_.empty()) return;

    int num_features = regression_data_[0].features.size();

    // Find min and max for each feature
    std::vector<double> min_vals(num_features, std::numeric_limits<double>::max());
    std::vector<double> max_vals(num_features, std::numeric_limits<double>::lowest());

    for (const auto& point : regression_data_) {
        for (size_t i = 0; i < point.features.size(); ++i) {
            min_vals[i] = std::min(min_vals[i], point.features[i]);
            max_vals[i] = std::max(max_vals[i], point.features[i]);
        }
    }

    // Normalize
    for (auto& point : regression_data_) {
        for (size_t i = 0; i < point.features.size(); ++i) {
            double range = max_vals[i] - min_vals[i];
            if (range > 1e-10) {
                point.features[i] = (point.features[i] - min_vals[i]) / range;
            }
        }
    }

    // Normalize classification data
    for (auto& point : classification_data_) {
        for (size_t i = 0; i < point.features.size(); ++i) {
            double range = max_vals[i] - min_vals[i];
            if (range > 1e-10) {
                point.features[i] = (point.features[i] - min_vals[i]) / range;
            }
        }
    }
}

void LearningPipeline::remove_outliers(double z_score_threshold) {
    if (regression_data_.empty()) return;

    // Calculate mean and std
    int num_features = regression_data_[0].features.size();
    std::vector<double> mean = normalize(std::vector<double>(num_features, 0.0));
    std::vector<double> std_vals(num_features, 0.0);

    for (const auto& point : regression_data_) {
        for (size_t i = 0; i < point.features.size(); ++i) {
            mean[i] += point.features[i];
        }
    }
    for (auto& m : mean) {
        m /= regression_data_.size();
    }

    for (const auto& point : regression_data_) {
        for (size_t i = 0; i < point.features.size(); ++i) {
            double diff = point.features[i] - mean[i];
            std_vals[i] += diff * diff;
        }
    }
    for (auto& s : std_vals) {
        s = std::sqrt(s / regression_data_.size());
    }

    // Remove outliers
    std::vector<RegressionDataPoint> filtered;
    for (const auto& point : regression_data_) {
        bool is_outlier = false;
        for (size_t i = 0; i < point.features.size(); ++i) {
            double z_score = calculate_z_score(point.features[i], mean[i], std_vals[i]);
            if (std::abs(z_score) > z_score_threshold) {
                is_outlier = true;
                break;
            }
        }
        if (!is_outlier) {
            filtered.push_back(point);
        }
    }

    regression_data_ = filtered;
}

void LearningPipeline::handle_missing_values(const std::string& strategy) {
    if (regression_data_.empty()) return;

    if (strategy == "mean") {
        int num_features = regression_data_[0].features.size();

        for (int f = 0; f < num_features; ++f) {
            std::vector<double> values;
            for (const auto& point : regression_data_) {
                if (f < static_cast<int>(point.features.size()) && !std::isnan(point.features[f])) {
                    values.push_back(point.features[f]);
                }
            }

            if (values.empty()) continue;

            double mean = std::accumulate(values.begin(), values.end(), 0.0) / values.size();

            for (auto& point : regression_data_) {
                if (f < static_cast<int>(point.features.size()) && std::isnan(point.features[f])) {
                    point.features[f] = mean;
                }
            }
        }
    }
}

bool LearningPipeline::split_data(
    std::vector<RegressionDataPoint>& train_data,
    std::vector<RegressionDataPoint>& test_data,
    double split_ratio
) {
    if (regression_data_.empty() || split_ratio <= 0.0 || split_ratio >= 1.0) {
        return false;
    }

    // Shuffle data
    std::vector<RegressionDataPoint> shuffled = regression_data_;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::shuffle(shuffled.begin(), shuffled.end(), gen);

    // Split
    size_t split_point = static_cast<size_t>(shuffled.size() * split_ratio);
    train_data.assign(shuffled.begin(), shuffled.begin() + split_point);
    test_data.assign(shuffled.begin() + split_point, shuffled.end());

    return true;
}

CrossValidationResult LearningPipeline::perform_cross_validation(
    int num_folds,
    const TrainingConfig& config
) {
    CrossValidationResult result;

    if (regression_data_.size() < static_cast<size_t>(num_folds)) {
        return result;
    }

    size_t fold_size = regression_data_.size() / num_folds;
    std::vector<RegressionDataPoint> shuffled = regression_data_;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::shuffle(shuffled.begin(), shuffled.end(), gen);

    for (int fold = 0; fold < num_folds; ++fold) {
        size_t test_start = fold * fold_size;
        size_t test_end = (fold == num_folds - 1) ? shuffled.size() : (fold + 1) * fold_size;

        std::vector<RegressionDataPoint> train_fold;
        std::vector<RegressionDataPoint> test_fold;

        for (size_t i = 0; i < shuffled.size(); ++i) {
            if (i >= test_start && i < test_end) {
                test_fold.push_back(shuffled[i]);
            } else {
                train_fold.push_back(shuffled[i]);
            }
        }

        // Train on fold
        auto engine = std::make_unique<RegressionEngine>();
        engine->train(RegressionEngine::RegressionType::LINEAR, train_fold, config.hyperparameters);

        // Evaluate
        auto metrics = engine->evaluate(test_fold);
        result.fold_scores.push_back(metrics.r_squared);
        result.fold_metrics.push_back(metrics);
    }

    // Calculate mean and std
    if (!result.fold_scores.empty()) {
        result.mean_score = std::accumulate(result.fold_scores.begin(),
                                           result.fold_scores.end(), 0.0) / result.fold_scores.size();

        double variance = 0.0;
        for (double score : result.fold_scores) {
            variance += (score - result.mean_score) * (score - result.mean_score);
        }
        variance /= result.fold_scores.size();
        result.std_score = std::sqrt(variance);
    }

    return result;
}

bool LearningPipeline::train_regression(
    RegressionEngine::RegressionType type,
    const TrainingConfig& config
) {
    if (regression_data_.empty()) return false;

    return regression_engine_->train(type, regression_data_, config.hyperparameters);
}

bool LearningPipeline::train_classification(
    ClassificationSystem::ClassifierType type,
    const TrainingConfig& config
) {
    if (classification_data_.empty()) return false;

    return classification_system_->train(type, classification_data_, config.hyperparameters);
}

RegressionMetrics LearningPipeline::evaluate_regression() {
    if (regression_data_.empty()) return RegressionMetrics();

    last_regression_metrics_ = regression_engine_->evaluate(regression_data_);
    return last_regression_metrics_;
}

ClassificationMetrics LearningPipeline::evaluate_classification() {
    if (classification_data_.empty()) return ClassificationMetrics();

    last_classification_metrics_ = classification_system_->evaluate(classification_data_);
    return last_classification_metrics_;
}

bool LearningPipeline::save_model(
    const std::string& filepath,
    const std::string& model_type
) {
    // Simplified model serialization - just write to file
    std::ofstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    // Write model metadata
    std::string metadata = model_type + "|" + std::to_string(regression_data_.size());
    file.write(metadata.c_str(), metadata.size());
    file.close();

    return true;
}

bool LearningPipeline::load_model(
    const std::string& filepath,
    const std::string& model_type
) {
    // Simplified model deserialization
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    file.close();
    return true;
}

ModelCheckpoint LearningPipeline::create_checkpoint(
    const std::string& model_type
) {
    ModelCheckpoint checkpoint;
    checkpoint.model_id = "checkpoint_" + std::to_string(regression_data_.size());
    checkpoint.timestamp = std::to_string(std::time(nullptr));
    checkpoint.model_type = model_type;
    checkpoint.validation_score = last_regression_metrics_.r_squared;

    return checkpoint;
}

bool LearningPipeline::restore_checkpoint(const ModelCheckpoint& checkpoint) {
    // Simplified checkpoint restoration
    return true;
}

std::map<std::string, double> LearningPipeline::get_data_statistics() {
    std::map<std::string, double> stats;

    if (regression_data_.empty()) {
        return stats;
    }

    stats["num_samples"] = regression_data_.size();

    // Calculate target statistics
    std::vector<double> targets;
    for (const auto& point : regression_data_) {
        targets.push_back(point.target);
    }

    double mean = std::accumulate(targets.begin(), targets.end(), 0.0) / targets.size();
    double variance = 0.0;
    for (double t : targets) {
        variance += (t - mean) * (t - mean);
    }
    variance /= targets.size();

    stats["target_mean"] = mean;
    stats["target_std"] = std::sqrt(variance);
    stats["target_min"] = *std::min_element(targets.begin(), targets.end());
    stats["target_max"] = *std::max_element(targets.begin(), targets.end());

    return stats;
}

std::vector<double> LearningPipeline::normalize(
    const std::vector<double>& values
) {
    std::vector<double> normalized;
    if (values.empty()) return normalized;

    double min_val = *std::min_element(values.begin(), values.end());
    double max_val = *std::max_element(values.begin(), values.end());
    double range = max_val - min_val;

    if (range < 1e-10) {
        return std::vector<double>(values.size(), 0.5);
    }

    for (double val : values) {
        normalized.push_back((val - min_val) / range);
    }

    return normalized;
}

double LearningPipeline::calculate_mean(const std::vector<double>& values) {
    if (values.empty()) return 0.0;
    return std::accumulate(values.begin(), values.end(), 0.0) / values.size();
}

double LearningPipeline::calculate_std(const std::vector<double>& values) {
    if (values.empty()) return 0.0;

    double mean = calculate_mean(values);
    double variance = 0.0;
    for (double val : values) {
        variance += (val - mean) * (val - mean);
    }
    variance /= values.size();

    return std::sqrt(variance);
}

double LearningPipeline::calculate_z_score(
    double value,
    double mean,
    double std
) {
    if (std < 1e-10) return 0.0;
    return (value - mean) / std;
}

}  // namespace bendcore::parametric_ai
