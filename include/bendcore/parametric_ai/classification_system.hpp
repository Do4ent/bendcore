// M6 Phase: Classification System (GAP-027)
// Design type classification, feature pattern recognition, quality classification
#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>

namespace bendcore::parametric_ai {

// Classification data point
struct ClassificationDataPoint {
    std::vector<double> features;
    int class_label = -1;
    std::string class_name;
    double confidence = 1.0;
};

// Classification result
struct ClassificationResult {
    int predicted_class = -1;
    std::string predicted_class_name;
    double confidence = 0.0;
    std::map<int, double> class_probabilities;
    std::vector<std::string> feature_contributions;
};

// Classification metrics
struct ClassificationMetrics {
    double accuracy = 0.0;
    double precision = 0.0;
    double recall = 0.0;
    double f1_score = 0.0;
    std::map<int, double> per_class_accuracy;
};

// Decision Tree Node
struct DecisionTreeNode {
    bool is_leaf = false;
    int feature_index = -1;
    double threshold = 0.0;
    int class_label = -1;
    std::shared_ptr<DecisionTreeNode> left;
    std::shared_ptr<DecisionTreeNode> right;
};

// Decision Tree Classifier
class DecisionTreeClassifier {
public:
    DecisionTreeClassifier(int max_depth = 10);
    ~DecisionTreeClassifier();

    void train(
        const std::vector<ClassificationDataPoint>& data,
        int min_samples = 1
    );

    ClassificationResult predict(const std::vector<double>& features) const;
    ClassificationMetrics evaluate(
        const std::vector<ClassificationDataPoint>& test_data
    ) const;

    bool is_trained() const { return root_ != nullptr; }

private:
    std::shared_ptr<DecisionTreeNode> root_;
    int max_depth_;

    std::shared_ptr<DecisionTreeNode> build_tree(
        const std::vector<ClassificationDataPoint>& data,
        int depth
    );

    int find_best_split(
        const std::vector<ClassificationDataPoint>& data,
        int& best_feature,
        double& best_threshold
    ) const;

    double calculate_gini(
        const std::vector<ClassificationDataPoint>& data
    ) const;

    ClassificationResult traverse_tree(
        const std::vector<double>& features
    ) const;
};

// K-Nearest Neighbors Classifier
class KNearestNeighborsClassifier {
public:
    KNearestNeighborsClassifier(int k = 3);
    ~KNearestNeighborsClassifier();

    void train(const std::vector<ClassificationDataPoint>& data);

    ClassificationResult predict(const std::vector<double>& features) const;
    ClassificationMetrics evaluate(
        const std::vector<ClassificationDataPoint>& test_data
    ) const;

    bool is_trained() const { return !training_data_.empty(); }

private:
    int k_;
    std::vector<ClassificationDataPoint> training_data_;

    static double euclidean_distance(
        const std::vector<double>& v1,
        const std::vector<double>& v2
    );
};

// Naive Bayes Classifier
class NaiveBayesClassifier {
public:
    NaiveBayesClassifier();
    ~NaiveBayesClassifier();

    void train(const std::vector<ClassificationDataPoint>& data);

    ClassificationResult predict(const std::vector<double>& features) const;
    ClassificationMetrics evaluate(
        const std::vector<ClassificationDataPoint>& test_data
    ) const;

    bool is_trained() const { return !class_priors_.empty(); }

private:
    std::map<int, double> class_priors_;
    std::map<int, std::vector<double>> feature_means_;
    std::map<int, std::vector<double>> feature_variances_;
    int num_features_ = 0;

    double gaussian_probability(
        double x,
        double mean,
        double variance
    ) const;
};

// Classification Engine with ensemble methods
class ClassificationSystem {
public:
    enum class ClassifierType {
        DECISION_TREE,
        KNN,
        NAIVE_BAYES
    };

    ClassificationSystem();
    ~ClassificationSystem();

    // Train a classifier
    bool train(
        ClassifierType type,
        const std::vector<ClassificationDataPoint>& data,
        const std::map<std::string, double>& hyperparams
    );

    // Predict using trained classifier
    ClassificationResult predict(const std::vector<double>& features) const;

    // Evaluate classifier
    ClassificationMetrics evaluate(
        const std::vector<ClassificationDataPoint>& test_data
    ) const;

    // Ensemble prediction (voting from multiple classifiers)
    ClassificationResult ensemble_predict(
        const std::vector<double>& features,
        const std::vector<ClassifierType>& types
    ) const;

    ClassifierType get_current_type() const { return current_type_; }
    bool is_trained() const { return trained_; }

private:
    ClassifierType current_type_;
    std::unique_ptr<DecisionTreeClassifier> tree_classifier_;
    std::unique_ptr<KNearestNeighborsClassifier> knn_classifier_;
    std::unique_ptr<NaiveBayesClassifier> nb_classifier_;
    bool trained_ = false;

    std::map<std::string, int> class_name_to_label_;
    std::map<int, std::string> class_label_to_name_;
};

}  // namespace bendcore::parametric_ai
