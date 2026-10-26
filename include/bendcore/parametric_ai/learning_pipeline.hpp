// M6 Phase: Learning Pipeline (GAP-027)
// Training data management, model training & validation, cross-validation, serialization
#pragma once

#include "regression_engine.hpp"
#include "classification_system.hpp"

#include <string>
#include <vector>
#include <map>
#include <memory>

namespace bendcore::parametric_ai {

// Training configuration
struct TrainingConfig {
    double train_test_split = 0.8;
    int cross_validation_folds = 5;
    int num_epochs = 100;
    double learning_rate = 0.01;
    int batch_size = 32;
    std::map<std::string, double> hyperparameters;
};

// Cross-validation result
struct CrossValidationResult {
    std::vector<double> fold_scores;
    double mean_score = 0.0;
    double std_score = 0.0;
    std::vector<RegressionMetrics> fold_metrics;
};

// Model checkpoint
struct ModelCheckpoint {
    std::string model_id;
    std::string timestamp;
    std::string model_type;
    double validation_score = 0.0;
    std::vector<uint8_t> serialized_data;
};

// Learning Pipeline
class LearningPipeline {
public:
    LearningPipeline();
    ~LearningPipeline();

    // Add training data
    void add_training_data(const RegressionDataPoint& data);
    void add_training_data(const ClassificationDataPoint& data);
    void add_training_data_batch(
        const std::vector<RegressionDataPoint>& data
    );

    // Data preprocessing
    void normalize_features();
    void remove_outliers(double z_score_threshold = 3.0);
    void handle_missing_values(const std::string& strategy = "mean");

    // Train-test split
    bool split_data(
        std::vector<RegressionDataPoint>& train_data,
        std::vector<RegressionDataPoint>& test_data,
        double split_ratio = 0.8
    );

    // Cross-validation
    CrossValidationResult perform_cross_validation(
        int num_folds = 5,
        const TrainingConfig& config = TrainingConfig()
    );

    // Train regression model
    bool train_regression(
        RegressionEngine::RegressionType type,
        const TrainingConfig& config
    );

    // Train classification model
    bool train_classification(
        ClassificationSystem::ClassifierType type,
        const TrainingConfig& config
    );

    // Model evaluation
    RegressionMetrics evaluate_regression();
    ClassificationMetrics evaluate_classification();

    // Model serialization
    bool save_model(
        const std::string& filepath,
        const std::string& model_type = "regression"
    );

    bool load_model(
        const std::string& filepath,
        const std::string& model_type = "regression"
    );

    // Model checkpoint management
    ModelCheckpoint create_checkpoint(
        const std::string& model_type = "regression"
    );

    bool restore_checkpoint(const ModelCheckpoint& checkpoint);

    // Get statistics
    std::map<std::string, double> get_data_statistics();
    int get_training_data_count() const { return regression_data_.size(); }
    int get_classification_data_count() const { return classification_data_.size(); }

private:
    std::vector<RegressionDataPoint> regression_data_;
    std::vector<ClassificationDataPoint> classification_data_;
    std::unique_ptr<RegressionEngine> regression_engine_;
    std::unique_ptr<ClassificationSystem> classification_system_;

    RegressionMetrics last_regression_metrics_;
    ClassificationMetrics last_classification_metrics_;

    // Data preprocessing helpers
    static std::vector<double> normalize(
        const std::vector<double>& values
    );

    static double calculate_mean(const std::vector<double>& values);
    static double calculate_std(const std::vector<double>& values);
    static double calculate_z_score(
        double value,
        double mean,
        double std
    );
};

}  // namespace bendcore::parametric_ai
