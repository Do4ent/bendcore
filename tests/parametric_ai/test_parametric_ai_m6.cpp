// M6 Phase: Parametric AI System Tests (GAP-027)
// Comprehensive test suite with 28+ unit tests

#include <gtest/gtest.h>
#include "bendcore/parametric_ai/design_learning.hpp"
#include "bendcore/parametric_ai/regression_engine.hpp"
#include "bendcore/parametric_ai/classification_system.hpp"
#include "bendcore/parametric_ai/prediction_system.hpp"
#include "bendcore/parametric_ai/learning_pipeline.hpp"
#include "bendcore/parametric_ai/design_advice.hpp"

using namespace bendcore::parametric_ai;

// ======================== Design Learning Tests ========================

class DesignLearningTest : public ::testing::Test {
protected:
    DesignLearningSystem learning_system;

    std::vector<DesignFeature> create_test_designs() {
        std::vector<DesignFeature> designs;

        DesignFeature d1;
        d1.design_id = "DESIGN-001";
        d1.features = {10.0, 20.0, 30.0, 45.0, 90.0};
        d1.complexity_score = 0.5;
        d1.quality_score = 0.8;
        designs.push_back(d1);

        DesignFeature d2;
        d2.design_id = "DESIGN-002";
        d2.features = {12.0, 22.0, 32.0, 50.0, 95.0};
        d2.complexity_score = 0.55;
        d2.quality_score = 0.75;
        designs.push_back(d2);

        DesignFeature d3;
        d3.design_id = "DESIGN-003";
        d3.features = {100.0, 200.0, 300.0, 45.0, 90.0};
        d3.complexity_score = 0.8;
        d3.quality_score = 0.6;
        designs.push_back(d3);

        return designs;
    }
};

TEST_F(DesignLearningTest, ExtractDesignFeatures) {
    auto feature = learning_system.extract_design_features(
        "TEST-001",
        {10.0, 12.0, 5.0},  // tube params: OD, ID, length
        {45.0, 90.0, 30.0}  // bend params: angle1, angle2, angle3
    );

    EXPECT_EQ(feature.design_id, "TEST-001");
    EXPECT_EQ(feature.features.size(), 6);
    EXPECT_GT(feature.complexity_score, 0.0);
    EXPECT_GT(feature.quality_score, 0.0);
    EXPECT_LE(feature.quality_score, 1.0);
}

TEST_F(DesignLearningTest, ComputeSimilarity) {
    auto designs = create_test_designs();
    auto metrics = learning_system.compute_similarity(designs[0], designs[1]);

    EXPECT_GE(metrics.similarity_score, 0.0);
    EXPECT_LE(metrics.similarity_score, 1.0);
    EXPECT_GE(metrics.euclidean_distance, 0.0);
    EXPECT_GE(metrics.manhattan_distance, 0.0);
}

TEST_F(DesignLearningTest, ClusterDesigns) {
    auto designs = create_test_designs();
    auto clusters = learning_system.cluster_designs(designs, 2, 100);

    EXPECT_EQ(clusters.size(), 2);
    for (const auto& cluster : clusters) {
        EXPECT_GE(cluster.cluster_id, 0);
        EXPECT_LE(cluster.cluster_id, 1);
        EXPECT_GT(cluster.radius, 0.0);
    }
}

TEST_F(DesignLearningTest, PerformPCA) {
    auto designs = create_test_designs();
    auto pca_data = learning_system.perform_pca(designs, 2);

    EXPECT_EQ(pca_data.num_components, 2);
    EXPECT_EQ(pca_data.components.size(), 2);
    EXPECT_EQ(pca_data.explained_variance.size(), 2);
    EXPECT_EQ(pca_data.mean.size(), 5);
}

TEST_F(DesignLearningTest, AnalyzeFeatureImportance) {
    auto designs = create_test_designs();
    auto importance = learning_system.analyze_feature_importance(designs, 3);

    EXPECT_LE(importance.size(), 3);
    for (const auto& imp : importance) {
        EXPECT_GE(imp.importance_score, 0.0);
        EXPECT_GE(imp.variance, 0.0);
    }
}

TEST_F(DesignLearningTest, ReduceDimensionality) {
    auto designs = create_test_designs();
    auto pca_data = learning_system.perform_pca(designs, 3);
    auto reduced = learning_system.reduce_dimensionality(designs[0], pca_data);

    EXPECT_EQ(reduced.size(), 3);
}

TEST_F(DesignLearningTest, FindNearestNeighbors) {
    auto designs = create_test_designs();
    auto neighbors = learning_system.find_nearest_neighbors(designs[0], designs, 2);

    EXPECT_LE(neighbors.size(), 2);
    for (const auto& [id, similarity] : neighbors) {
        EXPECT_GE(similarity, 0.0);
        EXPECT_LE(similarity, 1.0);
    }
}

// ======================== Regression Engine Tests ========================

class RegressionEngineTest : public ::testing::Test {
protected:
    std::vector<RegressionDataPoint> create_training_data() {
        std::vector<RegressionDataPoint> data;

        for (int i = 0; i < 10; ++i) {
            RegressionDataPoint point;
            point.features = {static_cast<double>(i), static_cast<double>(i * 2)};
            point.target = point.features[0] * 2.0 + point.features[1] * 1.5 + 3.0;
            data.push_back(point);
        }

        return data;
    }
};

TEST_F(RegressionEngineTest, LinearRegressionTraining) {
    auto data = create_training_data();
    LinearRegression lr;
    lr.train(data, 0.01, 100);

    EXPECT_TRUE(lr.is_trained());
    EXPECT_EQ(lr.get_coefficients().size(), 2);
}

TEST_F(RegressionEngineTest, LinearRegressionPrediction) {
    auto data = create_training_data();
    LinearRegression lr;
    lr.train(data, 0.01, 100);

    double pred = lr.predict({2.0, 4.0});
    EXPECT_GT(pred, 0.0);
}

TEST_F(RegressionEngineTest, LinearRegressionEvaluation) {
    auto data = create_training_data();
    LinearRegression lr;
    lr.train(data, 0.01, 100);

    auto metrics = lr.evaluate(data);
    EXPECT_GE(metrics.mse, 0.0);
    EXPECT_GE(metrics.rmse, 0.0);
    EXPECT_GE(metrics.mae, 0.0);
}

TEST_F(RegressionEngineTest, PolynomialRegressionTraining) {
    auto data = create_training_data();
    PolynomialRegression pr(2);
    pr.train(data, 0.01, 100);

    EXPECT_TRUE(pr.is_trained());
    EXPECT_EQ(pr.get_degree(), 2);
}

TEST_F(RegressionEngineTest, SVRTraining) {
    auto data = create_training_data();
    SupportVectorRegression svr(1.0, 0.1);
    svr.train(data, 0.01, 100);

    EXPECT_TRUE(svr.is_trained());
}

TEST_F(RegressionEngineTest, NeuralNetworkRegressionTraining) {
    auto data = create_training_data();
    std::vector<int> layers = {2, 5, 1};
    NeuralNetworkRegression nn(layers, 0.01);
    nn.train(data, 10);

    EXPECT_TRUE(nn.is_trained());
}

TEST_F(RegressionEngineTest, RegressionEngineLinear) {
    auto data = create_training_data();
    RegressionEngine engine;
    std::map<std::string, double> params;
    params["learning_rate"] = 0.01;
    params["iterations"] = 100;

    EXPECT_TRUE(engine.train(RegressionEngine::RegressionType::LINEAR, data, params));
    EXPECT_TRUE(engine.is_trained());

    double pred = engine.predict({2.0, 4.0});
    EXPECT_GT(pred, 0.0);
}

TEST_F(RegressionEngineTest, RegressionEnginePolynomial) {
    auto data = create_training_data();
    RegressionEngine engine;
    std::map<std::string, double> params;
    params["degree"] = 2;

    EXPECT_TRUE(engine.train(RegressionEngine::RegressionType::POLYNOMIAL, data, params));
    EXPECT_TRUE(engine.is_trained());
}

// ======================== Classification System Tests ========================

class ClassificationSystemTest : public ::testing::Test {
protected:
    std::vector<ClassificationDataPoint> create_classification_data() {
        std::vector<ClassificationDataPoint> data;

        for (int i = 0; i < 5; ++i) {
            ClassificationDataPoint point;
            point.features = {static_cast<double>(i), static_cast<double>(i * 2)};
            point.class_label = (i < 3) ? 0 : 1;
            point.class_name = (i < 3) ? "ClassA" : "ClassB";
            data.push_back(point);
        }

        return data;
    }
};

TEST_F(ClassificationSystemTest, DecisionTreeTraining) {
    auto data = create_classification_data();
    DecisionTreeClassifier dt(5);
    dt.train(data);

    EXPECT_TRUE(dt.is_trained());
}

TEST_F(ClassificationSystemTest, DecisionTreePrediction) {
    auto data = create_classification_data();
    DecisionTreeClassifier dt(5);
    dt.train(data);

    auto result = dt.predict({1.0, 2.0});
    EXPECT_GE(result.predicted_class, -1);
}

TEST_F(ClassificationSystemTest, KNNTraining) {
    auto data = create_classification_data();
    KNearestNeighborsClassifier knn(3);
    knn.train(data);

    EXPECT_TRUE(knn.is_trained());
}

TEST_F(ClassificationSystemTest, KNNPrediction) {
    auto data = create_classification_data();
    KNearestNeighborsClassifier knn(3);
    knn.train(data);

    auto result = knn.predict({1.0, 2.0});
    EXPECT_GE(result.predicted_class, 0);
    EXPECT_GE(result.confidence, 0.0);
    EXPECT_LE(result.confidence, 1.0);
}

TEST_F(ClassificationSystemTest, NaiveBayesTraining) {
    auto data = create_classification_data();
    NaiveBayesClassifier nb;
    nb.train(data);

    EXPECT_TRUE(nb.is_trained());
}

TEST_F(ClassificationSystemTest, ClassificationSystemKNN) {
    auto data = create_classification_data();
    ClassificationSystem system;
    std::map<std::string, double> params;
    params["k"] = 3;

    EXPECT_TRUE(system.train(ClassificationSystem::ClassifierType::KNN, data, params));
    EXPECT_TRUE(system.is_trained());

    auto result = system.predict({1.0, 2.0});
    EXPECT_GE(result.predicted_class, 0);
}

// ======================== Prediction System Tests ========================

class PredictionSystemTest : public ::testing::Test {
protected:
    std::vector<RegressionDataPoint> create_regression_data() {
        std::vector<RegressionDataPoint> data;
        for (int i = 0; i < 10; ++i) {
            RegressionDataPoint point;
            point.features = {static_cast<double>(i), static_cast<double>(i * 2)};
            point.target = i * 3.0;
            data.push_back(point);
        }
        return data;
    }
};

TEST_F(PredictionSystemTest, PredictionSystemTraining) {
    auto data = create_regression_data();
    std::vector<ClassificationDataPoint> empty;

    PredictionSystem system;
    EXPECT_TRUE(system.train(data, empty));
    EXPECT_TRUE(system.is_trained());
}

TEST_F(PredictionSystemTest, InferParameters) {
    auto data = create_regression_data();
    std::vector<ClassificationDataPoint> empty;

    PredictionSystem system;
    system.train(data, empty);

    std::vector<ParameterConstraint> constraints;
    ParameterConstraint c;
    c.parameter_name = "length";
    c.min_value = 10.0;
    c.max_value = 100.0;
    constraints.push_back(c);

    auto inferred = system.infer_parameters(constraints, {1.0, 2.0});
    EXPECT_EQ(inferred.size(), 1);
}

TEST_F(PredictionSystemTest, PredictProperties) {
    auto data = create_regression_data();
    std::vector<ClassificationDataPoint> empty;

    PredictionSystem system;
    system.train(data, empty);

    auto props = system.predict_properties({1.0, 2.0});
    EXPECT_GT(props.size(), 0);
}

TEST_F(PredictionSystemTest, DetectAnomaly) {
    auto data = create_regression_data();
    std::vector<ClassificationDataPoint> empty;

    PredictionSystem system;
    system.train(data, empty);

    auto anomaly = system.detect_anomaly({100.0, 200.0}, 0.5);
    EXPECT_GE(anomaly.anomaly_score, 0.0);
    EXPECT_LE(anomaly.anomaly_score, 1.0);
}

TEST_F(PredictionSystemTest, PredictWithUncertainty) {
    auto data = create_regression_data();
    std::vector<ClassificationDataPoint> empty;

    PredictionSystem system;
    system.train(data, empty);

    auto result = system.predict_with_uncertainty({1.0, 2.0}, 100);
    EXPECT_GT(result.uncertainty, 0.0);
    EXPECT_LE(result.lower_bound, result.predicted_value);
    EXPECT_GE(result.upper_bound, result.predicted_value);
}

TEST_F(PredictionSystemTest, BatchPredict) {
    auto data = create_regression_data();
    std::vector<ClassificationDataPoint> empty;

    PredictionSystem system;
    system.train(data, empty);

    std::vector<std::vector<double>> batch = {{1.0, 2.0}, {2.0, 4.0}, {3.0, 6.0}};
    auto results = system.batch_predict(batch);
    EXPECT_EQ(results.size(), 3);
}

// ======================== Learning Pipeline Tests ========================

class LearningPipelineTest : public ::testing::Test {
protected:
    std::vector<RegressionDataPoint> create_training_data() {
        std::vector<RegressionDataPoint> data;
        for (int i = 0; i < 20; ++i) {
            RegressionDataPoint point;
            point.features = {static_cast<double>(i), static_cast<double>(i * 2)};
            point.target = i * 3.0 + 5.0;
            data.push_back(point);
        }
        return data;
    }
};

TEST_F(LearningPipelineTest, AddTrainingData) {
    LearningPipeline pipeline;
    auto data = create_training_data();

    for (const auto& point : data) {
        pipeline.add_training_data(point);
    }

    EXPECT_EQ(pipeline.get_training_data_count(), 20);
}

TEST_F(LearningPipelineTest, SplitData) {
    LearningPipeline pipeline;
    auto data = create_training_data();

    for (const auto& point : data) {
        pipeline.add_training_data(point);
    }

    std::vector<RegressionDataPoint> train, test;
    EXPECT_TRUE(pipeline.split_data(train, test, 0.8));
    EXPECT_GT(train.size(), 0);
    EXPECT_GT(test.size(), 0);
    EXPECT_EQ(train.size() + test.size(), 20);
}

TEST_F(LearningPipelineTest, CrossValidation) {
    LearningPipeline pipeline;
    auto data = create_training_data();

    for (const auto& point : data) {
        pipeline.add_training_data(point);
    }

    TrainingConfig config;
    auto result = pipeline.perform_cross_validation(3, config);

    EXPECT_EQ(result.fold_scores.size(), 3);
    EXPECT_GE(result.mean_score, -1.0);
    EXPECT_LE(result.mean_score, 1.0);
}

TEST_F(LearningPipelineTest, TrainRegression) {
    LearningPipeline pipeline;
    auto data = create_training_data();

    for (const auto& point : data) {
        pipeline.add_training_data(point);
    }

    TrainingConfig config;
    EXPECT_TRUE(pipeline.train_regression(RegressionEngine::RegressionType::LINEAR, config));
}

TEST_F(LearningPipelineTest, EvaluateRegression) {
    LearningPipeline pipeline;
    auto data = create_training_data();

    for (const auto& point : data) {
        pipeline.add_training_data(point);
    }

    TrainingConfig config;
    pipeline.train_regression(RegressionEngine::RegressionType::LINEAR, config);
    auto metrics = pipeline.evaluate_regression();

    EXPECT_GE(metrics.mse, 0.0);
}

// ======================== Design Advice Tests ========================

class DesignAdviceTest : public ::testing::Test {
protected:
    std::vector<DesignFeature> create_design_history() {
        std::vector<DesignFeature> history;

        for (int i = 0; i < 5; ++i) {
            DesignFeature feature;
            feature.design_id = "DESIGN-" + std::to_string(i);
            feature.features = {10.0 + i, 20.0 + i, 30.0 + i};
            feature.complexity_score = 0.5 + i * 0.05;
            feature.quality_score = 0.8 - i * 0.05;
            history.push_back(feature);
        }

        return history;
    }
};

TEST_F(DesignAdviceTest, GenerateRecommendations) {
    DesignAdviceSystem system;
    auto history = create_design_history();

    system.train(history, {
        {0.8}, {0.75}, {0.7}, {0.65}, {0.6}
    });

    auto recs = system.generate_recommendations({12.0, 22.0, 32.0}, history);
    EXPECT_GT(recs.size(), 0);
}

TEST_F(DesignAdviceTest, SuggestDesigns) {
    DesignAdviceSystem system;
    std::vector<ParameterConstraint> constraints;

    ParameterConstraint c1;
    c1.parameter_name = "length";
    c1.min_value = 10.0;
    c1.max_value = 100.0;
    constraints.push_back(c1);

    auto suggestions = system.suggest_designs(constraints, 3);
    EXPECT_EQ(suggestions.size(), 3);
}

TEST_F(DesignAdviceTest, SuggestParameterImprovements) {
    DesignAdviceSystem system;
    auto hints = system.suggest_parameter_improvements({10.0, 20.0, 30.0}, "performance");

    EXPECT_GT(hints.size(), 0);
    for (const auto& hint : hints) {
        EXPECT_GT(hint.parameter_name.length(), 0);
    }
}

TEST_F(DesignAdviceTest, AnalyzeTradeOffs) {
    DesignAdviceSystem system;
    auto analyses = system.analyze_trade_offs(
        {"performance", "cost", "weight"},
        {10.0, 20.0}
    );

    EXPECT_GT(analyses.size(), 0);
}

TEST_F(DesignAdviceTest, FindHistoricalPatterns) {
    DesignAdviceSystem system;
    auto history = create_design_history();

    auto patterns = system.find_historical_patterns(history);
    EXPECT_GT(patterns.size(), 0);
    for (const auto& pattern : patterns) {
        EXPECT_GE(pattern.frequency, 0.0);
        EXPECT_LE(pattern.frequency, 1.0);
    }
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
