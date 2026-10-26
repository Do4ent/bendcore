// M6 Phase: Classification System (GAP-027) - Implementation
#include "bendcore/parametric_ai/classification_system.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <limits>

namespace bendcore::parametric_ai {

// ========================= DecisionTreeClassifier =========================

DecisionTreeClassifier::DecisionTreeClassifier(int max_depth)
    : max_depth_(max_depth) {}

DecisionTreeClassifier::~DecisionTreeClassifier() = default;

void DecisionTreeClassifier::train(
    const std::vector<ClassificationDataPoint>& data,
    int min_samples
) {
    if (data.empty()) return;
    root_ = build_tree(data, 0);
}

ClassificationResult DecisionTreeClassifier::predict(
    const std::vector<double>& features
) const {
    ClassificationResult result;
    if (!root_) return result;

    return traverse_tree(features);
}

ClassificationMetrics DecisionTreeClassifier::evaluate(
    const std::vector<ClassificationDataPoint>& test_data
) const {
    ClassificationMetrics metrics;
    if (!root_) return metrics;

    int correct = 0;
    int total = test_data.size();

    for (const auto& point : test_data) {
        auto result = predict(point.features);
        if (result.predicted_class == point.class_label) {
            correct++;
        }
    }

    metrics.accuracy = total > 0 ? static_cast<double>(correct) / total : 0.0;
    return metrics;
}

std::shared_ptr<DecisionTreeNode> DecisionTreeClassifier::build_tree(
    const std::vector<ClassificationDataPoint>& data,
    int depth
) {
    auto node = std::make_shared<DecisionTreeNode>();

    if (data.empty() || depth >= max_depth_) {
        node->is_leaf = true;
        return node;
    }

    // Check if all points have the same class
    bool all_same = true;
    int first_class = data[0].class_label;
    for (const auto& point : data) {
        if (point.class_label != first_class) {
            all_same = false;
            break;
        }
    }

    if (all_same) {
        node->is_leaf = true;
        node->class_label = first_class;
        return node;
    }

    int best_feature = -1;
    double best_threshold = 0.0;
    int best_split_quality = 0;

    find_best_split(data, best_feature, best_threshold);

    if (best_feature == -1) {
        node->is_leaf = true;
        node->class_label = data[0].class_label;
        return node;
    }

    node->feature_index = best_feature;
    node->threshold = best_threshold;

    // Split data
    std::vector<ClassificationDataPoint> left_data, right_data;
    for (const auto& point : data) {
        if (point.features[best_feature] < best_threshold) {
            left_data.push_back(point);
        } else {
            right_data.push_back(point);
        }
    }

    if (!left_data.empty()) {
        node->left = build_tree(left_data, depth + 1);
    }
    if (!right_data.empty()) {
        node->right = build_tree(right_data, depth + 1);
    }

    return node;
}

int DecisionTreeClassifier::find_best_split(
    const std::vector<ClassificationDataPoint>& data,
    int& best_feature,
    double& best_threshold
) const {
    if (data.empty() || data[0].features.empty()) {
        best_feature = -1;
        return 0;
    }

    best_feature = -1;
    int best_quality = 0;
    double current_gini = calculate_gini(data);

    int num_features = data[0].features.size();

    for (int f = 0; f < num_features; ++f) {
        std::vector<double> feature_values;
        for (const auto& point : data) {
            if (f < static_cast<int>(point.features.size())) {
                feature_values.push_back(point.features[f]);
            }
        }

        if (feature_values.empty()) continue;

        std::sort(feature_values.begin(), feature_values.end());
        feature_values.erase(
            std::unique(feature_values.begin(), feature_values.end()),
            feature_values.end()
        );

        for (size_t i = 0; i + 1 < feature_values.size(); ++i) {
            double threshold = (feature_values[i] + feature_values[i + 1]) / 2.0;

            std::vector<ClassificationDataPoint> left, right;
            for (const auto& point : data) {
                if (point.features[f] < threshold) {
                    left.push_back(point);
                } else {
                    right.push_back(point);
                }
            }

            if (left.empty() || right.empty()) continue;

            double left_gini = calculate_gini(left);
            double right_gini = calculate_gini(right);
            double weighted_gini = (left.size() * left_gini + right.size() * right_gini) / data.size();
            double improvement = static_cast<int>((current_gini - weighted_gini) * 10000);

            if (improvement > best_quality) {
                best_quality = improvement;
                best_feature = f;
                best_threshold = threshold;
            }
        }
    }

    return best_quality;
}

double DecisionTreeClassifier::calculate_gini(
    const std::vector<ClassificationDataPoint>& data
) const {
    if (data.empty()) return 0.0;

    std::map<int, int> class_counts;
    for (const auto& point : data) {
        class_counts[point.class_label]++;
    }

    double gini = 1.0;
    for (const auto& [label, count] : class_counts) {
        double prob = static_cast<double>(count) / data.size();
        gini -= prob * prob;
    }

    return gini;
}

ClassificationResult DecisionTreeClassifier::traverse_tree(
    const std::vector<double>& features
) const {
    ClassificationResult result;
    if (!root_) return result;

    auto node = root_;
    while (!node->is_leaf) {
        if (node->feature_index >= 0 &&
            node->feature_index < static_cast<int>(features.size())) {
            if (features[node->feature_index] < node->threshold) {
                if (node->left) {
                    node = node->left;
                } else {
                    break;
                }
            } else {
                if (node->right) {
                    node = node->right;
                } else {
                    break;
                }
            }
        } else {
            break;
        }
    }

    result.predicted_class = node->class_label;
    result.confidence = 0.8;
    return result;
}

// ========================= KNearestNeighborsClassifier =========================

KNearestNeighborsClassifier::KNearestNeighborsClassifier(int k)
    : k_(k) {}

KNearestNeighborsClassifier::~KNearestNeighborsClassifier() = default;

void KNearestNeighborsClassifier::train(
    const std::vector<ClassificationDataPoint>& data
) {
    training_data_ = data;
}

ClassificationResult KNearestNeighborsClassifier::predict(
    const std::vector<double>& features
) const {
    ClassificationResult result;

    if (training_data_.empty()) return result;

    // Find k nearest neighbors
    std::vector<std::pair<const ClassificationDataPoint*, double>> distances;
    for (const auto& point : training_data_) {
        double dist = euclidean_distance(features, point.features);
        distances.emplace_back(&point, dist);
    }

    // Sort by distance
    std::sort(distances.begin(), distances.end(),
        [](const auto& a, const auto& b) {
            return a.second < b.second;
        });

    // Count class votes
    std::map<int, int> class_votes;
    int count = 0;
    for (const auto& [point, dist] : distances) {
        if (count >= k_) break;
        class_votes[point->class_label]++;
        count++;
    }

    // Find class with most votes
    int best_class = -1;
    int max_votes = 0;
    for (const auto& [class_label, votes] : class_votes) {
        if (votes > max_votes) {
            max_votes = votes;
            best_class = class_label;
        }
    }

    result.predicted_class = best_class;
    result.confidence = max_votes > 0 ? static_cast<double>(max_votes) / k_ : 0.0;
    return result;
}

ClassificationMetrics KNearestNeighborsClassifier::evaluate(
    const std::vector<ClassificationDataPoint>& test_data
) const {
    ClassificationMetrics metrics;

    int correct = 0;
    for (const auto& point : test_data) {
        auto result = predict(point.features);
        if (result.predicted_class == point.class_label) {
            correct++;
        }
    }

    metrics.accuracy = test_data.empty() ? 0.0 :
                      static_cast<double>(correct) / test_data.size();
    return metrics;
}

double KNearestNeighborsClassifier::euclidean_distance(
    const std::vector<double>& v1,
    const std::vector<double>& v2
) {
    if (v1.size() != v2.size()) {
        return std::numeric_limits<double>::max();
    }

    double sum = 0.0;
    for (size_t i = 0; i < v1.size(); ++i) {
        double diff = v1[i] - v2[i];
        sum += diff * diff;
    }

    return std::sqrt(sum);
}

// ========================= NaiveBayesClassifier =========================

NaiveBayesClassifier::NaiveBayesClassifier() = default;

NaiveBayesClassifier::~NaiveBayesClassifier() = default;

void NaiveBayesClassifier::train(
    const std::vector<ClassificationDataPoint>& data
) {
    if (data.empty()) return;

    num_features_ = data[0].features.size();

    // Count classes and calculate priors
    std::map<int, int> class_counts;
    for (const auto& point : data) {
        class_counts[point.class_label]++;
    }

    for (const auto& [class_label, count] : class_counts) {
        class_priors_[class_label] = static_cast<double>(count) / data.size();
        feature_means_[class_label].resize(num_features_, 0.0);
        feature_variances_[class_label].resize(num_features_, 0.0);
    }

    // Calculate mean and variance for each feature per class
    for (const auto& point : data) {
        int class_label = point.class_label;
        for (int f = 0; f < num_features_ && f < static_cast<int>(point.features.size()); ++f) {
            feature_means_[class_label][f] += point.features[f];
        }
    }

    // Divide by class counts to get means
    for (auto& [class_label, means] : feature_means_) {
        int count = class_counts[class_label];
        for (auto& mean : means) {
            mean /= count;
        }
    }

    // Calculate variances
    for (const auto& point : data) {
        int class_label = point.class_label;
        for (int f = 0; f < num_features_ && f < static_cast<int>(point.features.size()); ++f) {
            double diff = point.features[f] - feature_means_[class_label][f];
            feature_variances_[class_label][f] += diff * diff;
        }
    }

    for (auto& [class_label, variances] : feature_variances_) {
        int count = class_counts[class_label];
        for (auto& var : variances) {
            var /= count;
            if (var < 1e-10) var = 1e-10;  // Avoid division by zero
        }
    }
}

ClassificationResult NaiveBayesClassifier::predict(
    const std::vector<double>& features
) const {
    ClassificationResult result;

    if (class_priors_.empty()) return result;

    double max_prob = -std::numeric_limits<double>::max();
    int best_class = -1;

    for (const auto& [class_label, prior] : class_priors_) {
        double log_prob = std::log(prior + 1e-10);

        for (int f = 0; f < num_features_ && f < static_cast<int>(features.size()); ++f) {
            double mean = feature_means_.at(class_label)[f];
            double var = feature_variances_.at(class_label)[f];
            double prob = gaussian_probability(features[f], mean, var);
            log_prob += std::log(prob + 1e-10);
        }

        if (log_prob > max_prob) {
            max_prob = log_prob;
            best_class = class_label;
        }
    }

    result.predicted_class = best_class;
    result.confidence = 0.7;  // Simplified confidence
    return result;
}

ClassificationMetrics NaiveBayesClassifier::evaluate(
    const std::vector<ClassificationDataPoint>& test_data
) const {
    ClassificationMetrics metrics;

    int correct = 0;
    for (const auto& point : test_data) {
        auto result = predict(point.features);
        if (result.predicted_class == point.class_label) {
            correct++;
        }
    }

    metrics.accuracy = test_data.empty() ? 0.0 :
                      static_cast<double>(correct) / test_data.size();
    return metrics;
}

double NaiveBayesClassifier::gaussian_probability(
    double x,
    double mean,
    double variance
) const {
    double numerator = std::exp(-(x - mean) * (x - mean) / (2.0 * variance));
    double denominator = std::sqrt(2.0 * M_PI * variance);
    return numerator / denominator;
}

// ========================= ClassificationSystem =========================

ClassificationSystem::ClassificationSystem()
    : current_type_(ClassifierType::DECISION_TREE) {}

ClassificationSystem::~ClassificationSystem() = default;

bool ClassificationSystem::train(
    ClassifierType type,
    const std::vector<ClassificationDataPoint>& data,
    const std::map<std::string, double>& hyperparams
) {
    if (data.empty()) return false;

    current_type_ = type;

    // Build class name mappings
    for (const auto& point : data) {
        if (class_label_to_name_.find(point.class_label) == class_label_to_name_.end()) {
            class_label_to_name_[point.class_label] = point.class_name;
            class_name_to_label_[point.class_name] = point.class_label;
        }
    }

    switch (type) {
        case ClassifierType::DECISION_TREE: {
            int max_depth = hyperparams.count("max_depth") ?
                          static_cast<int>(hyperparams.at("max_depth")) : 10;
            tree_classifier_ = std::make_unique<DecisionTreeClassifier>(max_depth);
            tree_classifier_->train(data);
            trained_ = true;
            return true;
        }

        case ClassifierType::KNN: {
            int k = hyperparams.count("k") ?
                   static_cast<int>(hyperparams.at("k")) : 3;
            knn_classifier_ = std::make_unique<KNearestNeighborsClassifier>(k);
            knn_classifier_->train(data);
            trained_ = true;
            return true;
        }

        case ClassifierType::NAIVE_BAYES: {
            nb_classifier_ = std::make_unique<NaiveBayesClassifier>();
            nb_classifier_->train(data);
            trained_ = true;
            return true;
        }

        default:
            return false;
    }
}

ClassificationResult ClassificationSystem::predict(
    const std::vector<double>& features
) const {
    if (!trained_) return ClassificationResult();

    ClassificationResult result;

    switch (current_type_) {
        case ClassifierType::DECISION_TREE:
            result = tree_classifier_ ? tree_classifier_->predict(features) : ClassificationResult();
            break;

        case ClassifierType::KNN:
            result = knn_classifier_ ? knn_classifier_->predict(features) : ClassificationResult();
            break;

        case ClassifierType::NAIVE_BAYES:
            result = nb_classifier_ ? nb_classifier_->predict(features) : ClassificationResult();
            break;
    }

    // Add class name if available
    if (class_label_to_name_.count(result.predicted_class)) {
        result.predicted_class_name = class_label_to_name_.at(result.predicted_class);
    }

    return result;
}

ClassificationMetrics ClassificationSystem::evaluate(
    const std::vector<ClassificationDataPoint>& test_data
) const {
    if (!trained_) return ClassificationMetrics();

    switch (current_type_) {
        case ClassifierType::DECISION_TREE:
            return tree_classifier_ ? tree_classifier_->evaluate(test_data) : ClassificationMetrics();

        case ClassifierType::KNN:
            return knn_classifier_ ? knn_classifier_->evaluate(test_data) : ClassificationMetrics();

        case ClassifierType::NAIVE_BAYES:
            return nb_classifier_ ? nb_classifier_->evaluate(test_data) : ClassificationMetrics();

        default:
            return ClassificationMetrics();
    }
}

ClassificationResult ClassificationSystem::ensemble_predict(
    const std::vector<double>& features,
    const std::vector<ClassifierType>& types
) const {
    ClassificationResult result;
    std::map<int, int> class_votes;

    for (auto type : types) {
        ClassificationResult type_result;

        switch (type) {
            case ClassifierType::DECISION_TREE:
                type_result = tree_classifier_ ? tree_classifier_->predict(features) : ClassificationResult();
                break;
            case ClassifierType::KNN:
                type_result = knn_classifier_ ? knn_classifier_->predict(features) : ClassificationResult();
                break;
            case ClassifierType::NAIVE_BAYES:
                type_result = nb_classifier_ ? nb_classifier_->predict(features) : ClassificationResult();
                break;
        }

        if (type_result.predicted_class >= 0) {
            class_votes[type_result.predicted_class]++;
        }
    }

    // Find class with most votes
    int best_class = -1;
    int max_votes = 0;
    for (const auto& [class_label, votes] : class_votes) {
        if (votes > max_votes) {
            max_votes = votes;
            best_class = class_label;
        }
    }

    result.predicted_class = best_class;
    result.confidence = types.empty() ? 0.0 : static_cast<double>(max_votes) / types.size();

    if (class_label_to_name_.count(best_class)) {
        result.predicted_class_name = class_label_to_name_.at(best_class);
    }

    return result;
}

}  // namespace bendcore::parametric_ai
