// M6 Phase: Design Learning System (GAP-027)
// Feature extraction, design similarity, clustering, dimensionality reduction
#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cmath>

namespace bendcore::parametric_ai {

// Design feature vector representation
struct DesignFeature {
    std::string design_id;
    std::vector<double> features;  // Normalized feature vector
    std::map<std::string, double> metadata;  // Design metadata
    double complexity_score = 0.0;
    double quality_score = 0.0;
};

// Design cluster
struct DesignCluster {
    int cluster_id = -1;
    std::vector<std::string> design_ids;
    std::vector<double> centroid;
    double radius = 0.0;
    int size() const { return design_ids.size(); }
};

// Principal Component Analysis data
struct PCAData {
    std::vector<std::vector<double>> components;  // Principal components
    std::vector<double> explained_variance;
    std::vector<double> mean;
    int num_components = 0;
};

// Feature importance result
struct FeatureImportance {
    int feature_index = -1;
    std::string feature_name;
    double importance_score = 0.0;
    double variance = 0.0;
};

// Design similarity metrics
struct SimilarityMetrics {
    double euclidean_distance = 0.0;
    double cosine_similarity = 0.0;
    double manhattan_distance = 0.0;
    double similarity_score = 0.0;  // Normalized 0-1 score
};

// Design Learning System
class DesignLearningSystem {
public:
    DesignLearningSystem();
    ~DesignLearningSystem();

    // Feature extraction and learning
    DesignFeature extract_design_features(
        const std::string& design_id,
        const std::vector<double>& tube_params,
        const std::vector<double>& bend_params
    );

    // Design similarity computation
    SimilarityMetrics compute_similarity(
        const DesignFeature& design1,
        const DesignFeature& design2
    );

    // K-means clustering
    std::vector<DesignCluster> cluster_designs(
        const std::vector<DesignFeature>& designs,
        int k,
        int max_iterations = 100
    );

    // Principal Component Analysis
    PCAData perform_pca(
        const std::vector<DesignFeature>& designs,
        int num_components = 3
    );

    // Feature importance analysis
    std::vector<FeatureImportance> analyze_feature_importance(
        const std::vector<DesignFeature>& designs,
        int num_features_to_return = 10
    );

    // Dimensionality reduction using PCA
    std::vector<double> reduce_dimensionality(
        const DesignFeature& design,
        const PCAData& pca_data
    );

    // Find k nearest neighbors
    std::vector<std::pair<std::string, double>> find_nearest_neighbors(
        const DesignFeature& query,
        const std::vector<DesignFeature>& candidates,
        int k = 5
    );

private:
    // Helper functions for distance metrics
    static double euclidean_distance(
        const std::vector<double>& v1,
        const std::vector<double>& v2
    );
    static double cosine_similarity(
        const std::vector<double>& v1,
        const std::vector<double>& v2
    );
    static double manhattan_distance(
        const std::vector<double>& v1,
        const std::vector<double>& v2
    );

    // Normalize feature vector
    static std::vector<double> normalize_features(
        const std::vector<double>& features
    );

    // Compute mean of feature vectors
    static std::vector<double> compute_mean(
        const std::vector<DesignFeature>& designs
    );

    // Compute covariance matrix
    static std::vector<std::vector<double>> compute_covariance(
        const std::vector<DesignFeature>& designs,
        const std::vector<double>& mean
    );

    // Compute eigenvalues and eigenvectors (simplified)
    static void compute_eigen(
        const std::vector<std::vector<double>>& matrix,
        std::vector<double>& eigenvalues,
        std::vector<std::vector<double>>& eigenvectors
    );

    int feature_cache_size_ = 10000;
};

}  // namespace bendcore::parametric_ai
