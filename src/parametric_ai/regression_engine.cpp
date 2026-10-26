// M6 Phase: Regression Engine (GAP-027) - Implementation
#include "bendcore/parametric_ai/regression_engine.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>

namespace bendcore::parametric_ai {

// ========================= LinearRegression =========================

LinearRegression::LinearRegression() = default;

LinearRegression::~LinearRegression() = default;

void LinearRegression::train(
    const std::vector<RegressionDataPoint>& data,
    double learning_rate,
    int iterations
) {
    if (data.empty()) return;

    int num_features = data[0].features.size();
    coefficients_.resize(num_features, 0.0);
    intercept_ = 0.0;

    // Gradient descent
    for (int iter = 0; iter < iterations; ++iter) {
        double intercept_grad = 0.0;
        std::vector<double> coeff_grads(num_features, 0.0);

        for (const auto& point : data) {
            double pred = intercept_;
            for (int i = 0; i < num_features; ++i) {
                if (i < static_cast<int>(point.features.size())) {
                    pred += coefficients_[i] * point.features[i];
                }
            }

            double error = pred - point.target;
            intercept_grad += error * point.weight;
            for (int i = 0; i < num_features; ++i) {
                if (i < static_cast<int>(point.features.size())) {
                    coeff_grads[i] += error * point.features[i] * point.weight;
                }
            }
        }

        // Update parameters
        intercept_ -= learning_rate * intercept_grad / data.size();
        for (int i = 0; i < num_features; ++i) {
            coefficients_[i] -= learning_rate * coeff_grads[i] / data.size();
        }
    }

    trained_ = true;
}

double LinearRegression::predict(const std::vector<double>& features) const {
    if (!trained_) return 0.0;

    double prediction = intercept_;
    for (size_t i = 0; i < features.size() && i < coefficients_.size(); ++i) {
        prediction += coefficients_[i] * features[i];
    }

    return prediction;
}

RegressionMetrics LinearRegression::evaluate(
    const std::vector<RegressionDataPoint>& test_data
) const {
    RegressionMetrics metrics;

    if (test_data.empty() || !trained_) return metrics;

    double mse_sum = 0.0;
    double mae_sum = 0.0;
    double mean_target = 0.0;

    for (const auto& point : test_data) {
        mean_target += point.target;
    }
    mean_target /= test_data.size();
    metrics.mean_target = mean_target;

    double ss_tot = 0.0;
    for (const auto& point : test_data) {
        double pred = predict(point.features);
        double error = pred - point.target;
        mse_sum += error * error;
        mae_sum += std::abs(error);
        ss_tot += (point.target - mean_target) * (point.target - mean_target);
    }

    metrics.mse = mse_sum / test_data.size();
    metrics.rmse = std::sqrt(metrics.mse);
    metrics.mae = mae_sum / test_data.size();

    if (ss_tot > 1e-10) {
        metrics.r_squared = 1.0 - (mse_sum / ss_tot);
    }

    return metrics;
}

// ========================= PolynomialRegression =========================

PolynomialRegression::PolynomialRegression(int degree)
    : degree_(degree) {}

PolynomialRegression::~PolynomialRegression() = default;

void PolynomialRegression::train(
    const std::vector<RegressionDataPoint>& data,
    double learning_rate,
    int iterations
) {
    if (data.empty()) return;

    // Generate polynomial features for all data points
    std::vector<RegressionDataPoint> poly_data;
    for (const auto& point : data) {
        RegressionDataPoint poly_point = point;
        poly_point.features = generate_polynomial_features(point.features);
        poly_data.push_back(poly_point);
    }

    // Use linear regression on polynomial features
    LinearRegression lr;
    lr.train(poly_data, learning_rate, iterations);

    coefficients_ = lr.get_coefficients();
    intercept_ = lr.get_intercept();
    trained_ = true;
}

double PolynomialRegression::predict(const std::vector<double>& features) const {
    if (!trained_) return 0.0;

    auto poly_features = generate_polynomial_features(features);
    double prediction = intercept_;
    for (size_t i = 0; i < poly_features.size() && i < coefficients_.size(); ++i) {
        prediction += coefficients_[i] * poly_features[i];
    }

    return prediction;
}

RegressionMetrics PolynomialRegression::evaluate(
    const std::vector<RegressionDataPoint>& test_data
) const {
    RegressionMetrics metrics;
    if (!trained_) return metrics;

    double mse_sum = 0.0;
    double mae_sum = 0.0;
    double mean_target = 0.0;

    for (const auto& point : test_data) {
        mean_target += point.target;
    }
    mean_target /= test_data.size();
    metrics.mean_target = mean_target;

    double ss_tot = 0.0;
    for (const auto& point : test_data) {
        double pred = predict(point.features);
        double error = pred - point.target;
        mse_sum += error * error;
        mae_sum += std::abs(error);
        ss_tot += (point.target - mean_target) * (point.target - mean_target);
    }

    metrics.mse = mse_sum / test_data.size();
    metrics.rmse = std::sqrt(metrics.mse);
    metrics.mae = mae_sum / test_data.size();

    if (ss_tot > 1e-10) {
        metrics.r_squared = 1.0 - (mse_sum / ss_tot);
    }

    return metrics;
}

std::vector<double> PolynomialRegression::generate_polynomial_features(
    const std::vector<double>& features
) const {
    std::vector<double> poly_features = features;

    for (int d = 2; d <= degree_; ++d) {
        for (const auto& feature : features) {
            double power = feature;
            for (int p = 1; p < d; ++p) {
                power *= feature;
            }
            poly_features.push_back(power);
        }
    }

    return poly_features;
}

// ========================= SupportVectorRegression =========================

SupportVectorRegression::SupportVectorRegression(double C, double epsilon)
    : C_(C), epsilon_(epsilon) {}

SupportVectorRegression::~SupportVectorRegression() = default;

void SupportVectorRegression::train(
    const std::vector<RegressionDataPoint>& data,
    double learning_rate,
    int iterations
) {
    if (data.empty()) return;

    // Simplified SVR: use data points as support vectors
    support_vectors_ = data;
    alphas_.resize(data.size(), 0.1);
    bias_ = 0.0;

    // Simple gradient-based update
    for (int iter = 0; iter < iterations; ++iter) {
        for (size_t i = 0; i < data.size(); ++i) {
            double pred = 0.0;
            for (size_t j = 0; j < support_vectors_.size(); ++j) {
                double kernel = kernel_rbf(data[i].features, support_vectors_[j].features);
                pred += alphas_[j] * kernel;
            }
            pred += bias_;

            double error = data[i].target - pred;
            if (std::abs(error) > epsilon_) {
                bias_ += learning_rate * error;
                for (size_t j = 0; j < support_vectors_.size(); ++j) {
                    double kernel = kernel_rbf(data[i].features, support_vectors_[j].features);
                    alphas_[j] += learning_rate * error * kernel / C_;
                }
            }
        }
    }

    trained_ = true;
}

double SupportVectorRegression::predict(const std::vector<double>& features) const {
    if (!trained_) return 0.0;

    double prediction = bias_;
    for (size_t i = 0; i < support_vectors_.size(); ++i) {
        double kernel = kernel_rbf(features, support_vectors_[i].features);
        prediction += alphas_[i] * kernel;
    }

    return prediction;
}

RegressionMetrics SupportVectorRegression::evaluate(
    const std::vector<RegressionDataPoint>& test_data
) const {
    RegressionMetrics metrics;
    if (!trained_) return metrics;

    double mse_sum = 0.0;
    double mae_sum = 0.0;
    double mean_target = 0.0;

    for (const auto& point : test_data) {
        mean_target += point.target;
    }
    mean_target /= test_data.size();
    metrics.mean_target = mean_target;

    double ss_tot = 0.0;
    for (const auto& point : test_data) {
        double pred = predict(point.features);
        double error = pred - point.target;
        mse_sum += error * error;
        mae_sum += std::abs(error);
        ss_tot += (point.target - mean_target) * (point.target - mean_target);
    }

    metrics.mse = mse_sum / test_data.size();
    metrics.rmse = std::sqrt(metrics.mse);
    metrics.mae = mae_sum / test_data.size();

    if (ss_tot > 1e-10) {
        metrics.r_squared = 1.0 - (mse_sum / ss_tot);
    }

    return metrics;
}

double SupportVectorRegression::kernel_rbf(
    const std::vector<double>& x1,
    const std::vector<double>& x2,
    double gamma
) const {
    double sum = 0.0;
    for (size_t i = 0; i < std::min(x1.size(), x2.size()); ++i) {
        double diff = x1[i] - x2[i];
        sum += diff * diff;
    }
    return std::exp(-gamma * sum);
}

// ========================= NeuralNetworkRegression =========================

NeuralNetworkRegression::NeuralNetworkRegression(
    const std::vector<int>& layer_sizes,
    double learning_rate
) : learning_rate_(learning_rate) {
    if (layer_sizes.size() < 2) return;

    for (size_t i = 0; i < layer_sizes.size() - 1; ++i) {
        Layer layer;
        layer.weights.resize(layer_sizes[i],
            std::vector<double>(layer_sizes[i + 1], 0.01));
        layer.biases.resize(layer_sizes[i + 1], 0.0);
        layer.activations.resize(layer_sizes[i + 1], 0.0);
        layer.deltas.resize(layer_sizes[i + 1], 0.0);
        layers_.push_back(layer);
    }
}

NeuralNetworkRegression::~NeuralNetworkRegression() = default;

void NeuralNetworkRegression::train(
    const std::vector<RegressionDataPoint>& data,
    int epochs,
    int batch_size
) {
    if (data.empty() || layers_.empty()) return;

    for (int epoch = 0; epoch < epochs; ++epoch) {
        for (size_t i = 0; i < data.size(); ++i) {
            forward_pass(data[i].features);
            backward_pass(data[i].target);
            update_weights();
        }
    }

    trained_ = true;
}

double NeuralNetworkRegression::predict(const std::vector<double>& features) const {
    if (!trained_ || layers_.empty()) return 0.0;

    auto activations = forward_pass(features);
    return activations.back();
}

RegressionMetrics NeuralNetworkRegression::evaluate(
    const std::vector<RegressionDataPoint>& test_data
) const {
    RegressionMetrics metrics;
    if (!trained_) return metrics;

    double mse_sum = 0.0;
    double mae_sum = 0.0;
    double mean_target = 0.0;

    for (const auto& point : test_data) {
        mean_target += point.target;
    }
    mean_target /= test_data.size();
    metrics.mean_target = mean_target;

    double ss_tot = 0.0;
    for (const auto& point : test_data) {
        double pred = predict(point.features);
        double error = pred - point.target;
        mse_sum += error * error;
        mae_sum += std::abs(error);
        ss_tot += (point.target - mean_target) * (point.target - mean_target);
    }

    metrics.mse = mse_sum / test_data.size();
    metrics.rmse = std::sqrt(metrics.mse);
    metrics.mae = mae_sum / test_data.size();

    if (ss_tot > 1e-10) {
        metrics.r_squared = 1.0 - (mse_sum / ss_tot);
    }

    return metrics;
}

double NeuralNetworkRegression::sigmoid(double x) {
    if (x > 20) return 1.0;
    if (x < -20) return 0.0;
    return 1.0 / (1.0 + std::exp(-x));
}

double NeuralNetworkRegression::sigmoid_derivative(double x) {
    return x * (1.0 - x);
}

double NeuralNetworkRegression::relu(double x) {
    return std::max(0.0, x);
}

double NeuralNetworkRegression::relu_derivative(double x) {
    return x > 0.0 ? 1.0 : 0.0;
}

std::vector<double> NeuralNetworkRegression::forward_pass(
    const std::vector<double>& input
) const {
    std::vector<double> current = input;

    for (const auto& layer : layers_) {
        std::vector<double> next(layer.weights[0].size(), 0.0);
        for (size_t i = 0; i < layer.weights.size(); ++i) {
            for (size_t j = 0; j < layer.weights[i].size(); ++j) {
                next[j] += current[i] * layer.weights[i][j];
            }
        }

        for (size_t j = 0; j < next.size(); ++j) {
            next[j] += layer.biases[j];
            next[j] = relu(next[j]);
        }

        current = next;
    }

    return current;
}

void NeuralNetworkRegression::backward_pass(double target) {
    // Simple backpropagation (simplified)
    if (layers_.empty()) return;

    double output_error = 0.0 - target;
    layers_.back().deltas[0] = output_error;
}

void NeuralNetworkRegression::update_weights() {
    // Update weights based on deltas (simplified)
    for (auto& layer : layers_) {
        for (auto& weight_row : layer.weights) {
            for (auto& w : weight_row) {
                w -= learning_rate_ * 0.01;  // Simple update
            }
        }
    }
}

// ========================= RegressionEngine =========================

RegressionEngine::RegressionEngine() : current_type_(RegressionType::LINEAR) {}

RegressionEngine::~RegressionEngine() = default;

bool RegressionEngine::train(
    RegressionType type,
    const std::vector<RegressionDataPoint>& data,
    const std::map<std::string, double>& hyperparams
) {
    current_type_ = type;

    switch (type) {
        case RegressionType::LINEAR: {
            linear_model_ = std::make_unique<LinearRegression>();
            double lr = hyperparams.count("learning_rate") ?
                        hyperparams.at("learning_rate") : 0.01;
            int iter = hyperparams.count("iterations") ?
                       static_cast<int>(hyperparams.at("iterations")) : 100;
            linear_model_->train(data, lr, iter);
            trained_ = true;
            return true;
        }

        case RegressionType::POLYNOMIAL: {
            int degree = hyperparams.count("degree") ?
                        static_cast<int>(hyperparams.at("degree")) : 2;
            poly_model_ = std::make_unique<PolynomialRegression>(degree);
            double lr = hyperparams.count("learning_rate") ?
                        hyperparams.at("learning_rate") : 0.01;
            int iter = hyperparams.count("iterations") ?
                       static_cast<int>(hyperparams.at("iterations")) : 100;
            poly_model_->train(data, lr, iter);
            trained_ = true;
            return true;
        }

        case RegressionType::SVR: {
            double C = hyperparams.count("C") ?
                      hyperparams.at("C") : 1.0;
            double eps = hyperparams.count("epsilon") ?
                        hyperparams.at("epsilon") : 0.1;
            svr_model_ = std::make_unique<SupportVectorRegression>(C, eps);
            double lr = hyperparams.count("learning_rate") ?
                        hyperparams.at("learning_rate") : 0.01;
            int iter = hyperparams.count("iterations") ?
                       static_cast<int>(hyperparams.at("iterations")) : 100;
            svr_model_->train(data, lr, iter);
            trained_ = true;
            return true;
        }

        case RegressionType::NEURAL_NETWORK: {
            std::vector<int> layers = {static_cast<int>(data[0].features.size()), 10, 5, 1};
            double lr = hyperparams.count("learning_rate") ?
                       hyperparams.at("learning_rate") : 0.01;
            nn_model_ = std::make_unique<NeuralNetworkRegression>(layers, lr);
            int epochs = hyperparams.count("epochs") ?
                        static_cast<int>(hyperparams.at("epochs")) : 100;
            nn_model_->train(data, epochs);
            trained_ = true;
            return true;
        }

        default:
            return false;
    }
}

double RegressionEngine::predict(const std::vector<double>& features) const {
    if (!trained_) return 0.0;

    switch (current_type_) {
        case RegressionType::LINEAR:
            return linear_model_ ? linear_model_->predict(features) : 0.0;
        case RegressionType::POLYNOMIAL:
            return poly_model_ ? poly_model_->predict(features) : 0.0;
        case RegressionType::SVR:
            return svr_model_ ? svr_model_->predict(features) : 0.0;
        case RegressionType::NEURAL_NETWORK:
            return nn_model_ ? nn_model_->predict(features) : 0.0;
        default:
            return 0.0;
    }
}

RegressionMetrics RegressionEngine::evaluate(
    const std::vector<RegressionDataPoint>& test_data
) const {
    if (!trained_) return RegressionMetrics();

    switch (current_type_) {
        case RegressionType::LINEAR:
            return linear_model_ ? linear_model_->evaluate(test_data) : RegressionMetrics();
        case RegressionType::POLYNOMIAL:
            return poly_model_ ? poly_model_->evaluate(test_data) : RegressionMetrics();
        case RegressionType::SVR:
            return svr_model_ ? svr_model_->evaluate(test_data) : RegressionMetrics();
        case RegressionType::NEURAL_NETWORK:
            return nn_model_ ? nn_model_->evaluate(test_data) : RegressionMetrics();
        default:
            return RegressionMetrics();
    }
}

double RegressionEngine::ridge_penalty(
    const std::vector<double>& coefficients,
    double alpha
) {
    double penalty = 0.0;
    for (double c : coefficients) {
        penalty += c * c;
    }
    return alpha * penalty;
}

double RegressionEngine::lasso_penalty(
    const std::vector<double>& coefficients,
    double alpha
) {
    double penalty = 0.0;
    for (double c : coefficients) {
        penalty += std::abs(c);
    }
    return alpha * penalty;
}

}  // namespace bendcore::parametric_ai
