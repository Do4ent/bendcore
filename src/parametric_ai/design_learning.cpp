// M6 Phase: Design Learning System (GAP-027) - Implementation
#include "bendcore/parametric_ai/design_learning.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>

namespace bendcore::parametric_ai {

DesignLearningSystem::DesignLearningSystem() = default;

DesignLearningSystem::~DesignLearningSystem() = default;

DesignFeature DesignLearningSystem::extract_design_features(
    const std::string& design_id,
    const std::vector<double>& tube_params,
    const std::vector<double>& bend_params
) {
    DesignFeature feature;
    feature.design_id = design_id;

    // Combine tube and bend parameters
    std::vector<double> combined = tube_params;
    combined.insert(combined.end(), bend_params.begin(), bend_params.end());

    // Normalize features
    feature.features = normalize_features(combined);

    // Calculate complexity score
    feature.complexity_score = 0.0;
    for (double param : bend_params) {
        feature.complexity_score += std::abs(param);
    }
    feature.complexity_score /= std::max(1.0, static_cast<double>(bend_params.size()));

    // Quality score (inverse of complexity for now)
    feature.quality_score = 1.0 - std::min(1.0, feature.complexity_score / 180.0);

    return feature;
}

SimilarityMetrics DesignLearningSystem::compute_similarity(
    const DesignFeature& design1,
    const DesignFeature& design2
) {
    SimilarityMetrics metrics;

    if (design1.features.size() != design2.features.size()) {
        return metrics;
    }

    metrics.euclidean_distance = euclidean_distance(design1.features, design2.features);
    metrics.cosine_similarity = cosine_similarity(design1.features, design2.features);
    metrics.manhattan_distance = manhattan_distance(design1.features, design2.features);

    // Normalize similarity score to 0-1 range
    // Use cosine similarity as primary metric
    metrics.similarity_score = (metrics.cosine_similarity + 1.0) / 2.0;

    return metrics;
}

std::vector<DesignCluster> DesignLearningSystem::cluster_designs(
    const std::vector<DesignFeature>& designs,
    int k,
    int max_iterations
) {
    if (designs.empty() || k <= 0 || k > static_cast<int>(designs.size())) {
        return {};
    }

    // Initialize centroids randomly
    std::vector<std::vector<double>> centroids;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, designs.size() - 1);

    std::vector<bool> used(designs.size(), false);
    for (int i = 0; i < k; ++i) {
        int idx = dis(gen);
        while (used[idx]) {
            idx = dis(gen);
        }
        used[idx] = true;
        centroids.push_back(designs[idx].features);
    }

    std::vector<int> assignments(designs.size(), 0);

    // K-means iterations
    for (int iter = 0; iter < max_iterations; ++iter) {
        // Assign points to nearest centroid
        for (size_t i = 0; i < designs.size(); ++i) {
            double min_dist = std::numeric_limits<double>::max();
            int best_cluster = 0;
            for (int c = 0; c < k; ++c) {
                double dist = euclidean_distance(designs[i].features, centroids[c]);
                if (dist < min_dist) {
                    min_dist = dist;
                    best_cluster = c;
                }
            }
            assignments[i] = best_cluster;
        }

        // Update centroids
        std::vector<std::vector<double>> new_centroids(k);
        std::vector<int> cluster_sizes(k, 0);

        for (size_t i = 0; i < designs.size(); ++i) {
            int cluster = assignments[i];
            if (new_centroids[cluster].empty()) {
                new_centroids[cluster] = std::vector<double>(designs[i].features.size(), 0.0);
            }
            for (size_t j = 0; j < designs[i].features.size(); ++j) {
                new_centroids[cluster][j] += designs[i].features[j];
            }
            cluster_sizes[cluster]++;
        }

        for (int c = 0; c < k; ++c) {
            if (cluster_sizes[c] > 0) {
                for (auto& val : new_centroids[c]) {
                    val /= cluster_sizes[c];
                }
            }
        }

        centroids = new_centroids;
    }

    // Build result clusters
    std::vector<DesignCluster> clusters(k);
    for (int i = 0; i < k; ++i) {
        clusters[i].cluster_id = i;
        clusters[i].centroid = centroids[i];
    }

    for (size_t i = 0; i < designs.size(); ++i) {
        clusters[assignments[i]].design_ids.push_back(designs[i].design_id);
    }

    // Calculate cluster radius
    for (int c = 0; c < k; ++c) {
        double max_dist = 0.0;
        for (const auto& design_id : clusters[c].design_ids) {
            for (size_t i = 0; i < designs.size(); ++i) {
                if (designs[i].design_id == design_id) {
                    double dist = euclidean_distance(
                        designs[i].features,
                        centroids[c]
                    );
                    max_dist = std::max(max_dist, dist);
                    break;
                }
            }
        }
        clusters[c].radius = max_dist;
    }

    return clusters;
}

PCAData DesignLearningSystem::perform_pca(
    const std::vector<DesignFeature>& designs,
    int num_components
) {
    PCAData pca_data;
    pca_data.num_components = num_components;

    if (designs.empty()) {
        return pca_data;
    }

    int num_features = designs[0].features.size();
    num_components = std::min(num_components, num_features);

    // Compute mean
    pca_data.mean = compute_mean(designs);

    // Center data
    std::vector<std::vector<double>> centered(designs.size());
    for (size_t i = 0; i < designs.size(); ++i) {
        centered[i].resize(num_features);
        for (int j = 0; j < num_features; ++j) {
            centered[i][j] = designs[i].features[j] - pca_data.mean[j];
        }
    }

    // Compute covariance matrix
    auto cov_matrix = compute_covariance(designs, pca_data.mean);

    // Simplified eigenvalue decomposition
    std::vector<double> eigenvalues;
    std::vector<std::vector<double>> eigenvectors;
    compute_eigen(cov_matrix, eigenvalues, eigenvectors);

    // Sort eigenvalues and eigenvectors
    std::vector<int> indices(eigenvalues.size());
    std::iota(indices.begin(), indices.end(), 0);
    std::sort(indices.begin(), indices.end(),
        [&eigenvalues](int a, int b) {
            return eigenvalues[a] > eigenvalues[b];
        });

    // Select top components
    for (int i = 0; i < num_components && i < static_cast<int>(indices.size()); ++i) {
        int idx = indices[i];
        pca_data.components.push_back(eigenvectors[idx]);
        pca_data.explained_variance.push_back(eigenvalues[idx]);
    }

    return pca_data;
}

std::vector<FeatureImportance> DesignLearningSystem::analyze_feature_importance(
    const std::vector<DesignFeature>& designs,
    int num_features_to_return
) {
    std::vector<FeatureImportance> importance_list;

    if (designs.empty()) {
        return importance_list;
    }

    int num_features = designs[0].features.size();

    // Calculate variance for each feature
    for (int f = 0; f < num_features; ++f) {
        std::vector<double> feature_values;
        for (const auto& design : designs) {
            if (f < static_cast<int>(design.features.size())) {
                feature_values.push_back(design.features[f]);
            }
        }

        if (feature_values.empty()) continue;

        double mean = std::accumulate(feature_values.begin(), feature_values.end(), 0.0) /
                      feature_values.size();
        double variance = 0.0;
        for (double val : feature_values) {
            variance += (val - mean) * (val - mean);
        }
        variance /= feature_values.size();

        FeatureImportance imp;
        imp.feature_index = f;
        imp.feature_name = "feature_" + std::to_string(f);
        imp.importance_score = std::sqrt(variance);
        imp.variance = variance;

        importance_list.push_back(imp);
    }

    // Sort by importance
    std::sort(importance_list.begin(), importance_list.end(),
        [](const FeatureImportance& a, const FeatureImportance& b) {
            return a.importance_score > b.importance_score;
        });

    // Return top features
    if (static_cast<int>(importance_list.size()) > num_features_to_return) {
        importance_list.resize(num_features_to_return);
    }

    return importance_list;
}

std::vector<double> DesignLearningSystem::reduce_dimensionality(
    const DesignFeature& design,
    const PCAData& pca_data
) {
    std::vector<double> reduced;

    if (pca_data.components.empty() || design.features.size() != pca_data.mean.size()) {
        return reduced;
    }

    // Project onto principal components
    for (const auto& component : pca_data.components) {
        double projection = 0.0;
        for (size_t i = 0; i < design.features.size(); ++i) {
            projection += (design.features[i] - pca_data.mean[i]) * component[i];
        }
        reduced.push_back(projection);
    }

    return reduced;
}

std::vector<std::pair<std::string, double>> DesignLearningSystem::find_nearest_neighbors(
    const DesignFeature& query,
    const std::vector<DesignFeature>& candidates,
    int k
) {
    std::vector<std::pair<std::string, double>> neighbors;

    for (const auto& candidate : candidates) {
        auto metrics = compute_similarity(query, candidate);
        neighbors.emplace_back(candidate.design_id, metrics.similarity_score);
    }

    // Sort by similarity score
    std::sort(neighbors.begin(), neighbors.end(),
        [](const auto& a, const auto& b) {
            return a.second > b.second;
        });

    // Return top k
    if (static_cast<int>(neighbors.size()) > k) {
        neighbors.resize(k);
    }

    return neighbors;
}

double DesignLearningSystem::euclidean_distance(
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

double DesignLearningSystem::cosine_similarity(
    const std::vector<double>& v1,
    const std::vector<double>& v2
) {
    if (v1.size() != v2.size()) {
        return 0.0;
    }

    double dot_product = 0.0;
    double norm1 = 0.0;
    double norm2 = 0.0;

    for (size_t i = 0; i < v1.size(); ++i) {
        dot_product += v1[i] * v2[i];
        norm1 += v1[i] * v1[i];
        norm2 += v2[i] * v2[i];
    }

    norm1 = std::sqrt(norm1);
    norm2 = std::sqrt(norm2);

    if (norm1 < 1e-10 || norm2 < 1e-10) {
        return 0.0;
    }

    return dot_product / (norm1 * norm2);
}

double DesignLearningSystem::manhattan_distance(
    const std::vector<double>& v1,
    const std::vector<double>& v2
) {
    if (v1.size() != v2.size()) {
        return std::numeric_limits<double>::max();
    }

    double sum = 0.0;
    for (size_t i = 0; i < v1.size(); ++i) {
        sum += std::abs(v1[i] - v2[i]);
    }

    return sum;
}

std::vector<double> DesignLearningSystem::normalize_features(
    const std::vector<double>& features
) {
    if (features.empty()) {
        return features;
    }

    std::vector<double> normalized;
    double min_val = *std::min_element(features.begin(), features.end());
    double max_val = *std::max_element(features.begin(), features.end());

    double range = max_val - min_val;
    if (range < 1e-10) {
        return std::vector<double>(features.size(), 0.5);
    }

    for (double val : features) {
        normalized.push_back((val - min_val) / range);
    }

    return normalized;
}

std::vector<double> DesignLearningSystem::compute_mean(
    const std::vector<DesignFeature>& designs
) {
    std::vector<double> mean;

    if (designs.empty()) {
        return mean;
    }

    int num_features = designs[0].features.size();
    mean.resize(num_features, 0.0);

    for (const auto& design : designs) {
        for (int i = 0; i < num_features; ++i) {
            if (i < static_cast<int>(design.features.size())) {
                mean[i] += design.features[i];
            }
        }
    }

    for (auto& val : mean) {
        val /= designs.size();
    }

    return mean;
}

std::vector<std::vector<double>> DesignLearningSystem::compute_covariance(
    const std::vector<DesignFeature>& designs,
    const std::vector<double>& mean
) {
    std::vector<std::vector<double>> cov;

    if (designs.empty() || mean.empty()) {
        return cov;
    }

    int num_features = mean.size();
    cov.resize(num_features, std::vector<double>(num_features, 0.0));

    for (const auto& design : designs) {
        for (int i = 0; i < num_features; ++i) {
            for (int j = 0; j < num_features; ++j) {
                double val_i = (i < static_cast<int>(design.features.size())) ?
                               design.features[i] - mean[i] : -mean[i];
                double val_j = (j < static_cast<int>(design.features.size())) ?
                               design.features[j] - mean[j] : -mean[j];
                cov[i][j] += val_i * val_j;
            }
        }
    }

    for (auto& row : cov) {
        for (auto& val : row) {
            val /= designs.size();
        }
    }

    return cov;
}

void DesignLearningSystem::compute_eigen(
    const std::vector<std::vector<double>>& matrix,
    std::vector<double>& eigenvalues,
    std::vector<std::vector<double>>& eigenvectors
) {
    // Simplified eigenvalue computation using power iteration method
    if (matrix.empty()) {
        return;
    }

    int n = matrix.size();
    eigenvalues.clear();
    eigenvectors.clear();

    // For simplicity, use diagonal elements as approximate eigenvalues
    for (int i = 0; i < n; ++i) {
        eigenvalues.push_back(std::abs(matrix[i][i]));

        std::vector<double> eigvec(n, 0.0);
        eigvec[i] = 1.0;
        eigenvectors.push_back(eigvec);
    }
}

}  // namespace bendcore::parametric_ai
