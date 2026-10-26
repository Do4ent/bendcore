// M6 Phase: Regression Engine (GAP-027)
// Linear, polynomial, SVR, and neural network regression models
#pragma once

#include <vector>
#include <string>
#include <map>
#include <memory>

namespace bendcore::parametric_ai {

// Training data point
struct RegressionDataPoint {
    std::vector<double> features;  // Input features
    double target = 0.0;           // Target value
    double weight = 1.0;           // Sample weight
};

// Model evaluation metrics
struct RegressionMetrics {
    double mse = 0.0;              // Mean Squared Error
    double rmse = 0.0;             // Root Mean Squared Error
    double mae = 0.0;              // Mean Absolute Error
    double r_squared = 0.0;        // R² score
    double mean_target = 0.0;
};

// Linear Regression Model
class LinearRegression {
public:
    LinearRegression();
    ~LinearRegression();

    void train(
        const std::vector<RegressionDataPoint>& data,
        double learning_rate = 0.01,
        int iterations = 100
    );

    double predict(const std::vector<double>& features) const;
    RegressionMetrics evaluate(const std::vector<RegressionDataPoint>& test_data) const;

    const std::vector<double>& get_coefficients() const { return coefficients_; }
    double get_intercept() const { return intercept_; }
    bool is_trained() const { return trained_; }

private:
    std::vector<double> coefficients_;
    double intercept_ = 0.0;
    bool trained_ = false;

    static std::vector<double> normalize_features(const std::vector<double>& features);
};

// Polynomial Regression Model
class PolynomialRegression {
public:
    PolynomialRegression(int degree = 2);
    ~PolynomialRegression();

    void train(
        const std::vector<RegressionDataPoint>& data,
        double learning_rate = 0.01,
        int iterations = 100
    );

    double predict(const std::vector<double>& features) const;
    RegressionMetrics evaluate(const std::vector<RegressionDataPoint>& test_data) const;

    int get_degree() const { return degree_; }
    bool is_trained() const { return trained_; }

private:
    int degree_;
    std::vector<double> coefficients_;
    double intercept_ = 0.0;
    bool trained_ = false;

    std::vector<double> generate_polynomial_features(
        const std::vector<double>& features
    ) const;
};

// Support Vector Regression
class SupportVectorRegression {
public:
    SupportVectorRegression(double C = 1.0, double epsilon = 0.1);
    ~SupportVectorRegression();

    void train(
        const std::vector<RegressionDataPoint>& data,
        double learning_rate = 0.01,
        int iterations = 100
    );

    double predict(const std::vector<double>& features) const;
    RegressionMetrics evaluate(const std::vector<RegressionDataPoint>& test_data) const;

    bool is_trained() const { return trained_; }

private:
    double C_;
    double epsilon_;
    std::vector<RegressionDataPoint> support_vectors_;
    std::vector<double> alphas_;
    double bias_ = 0.0;
    bool trained_ = false;

    double kernel_rbf(
        const std::vector<double>& x1,
        const std::vector<double>& x2,
        double gamma = 0.1
    ) const;
};

// Neural Network Regression (Simple MLP)
class NeuralNetworkRegression {
public:
    NeuralNetworkRegression(
        const std::vector<int>& layer_sizes,
        double learning_rate = 0.01
    );
    ~NeuralNetworkRegression();

    void train(
        const std::vector<RegressionDataPoint>& data,
        int epochs = 100,
        int batch_size = 32
    );

    double predict(const std::vector<double>& features) const;
    RegressionMetrics evaluate(const std::vector<RegressionDataPoint>& test_data) const;

    bool is_trained() const { return trained_; }

private:
    struct Layer {
        std::vector<std::vector<double>> weights;
        std::vector<double> biases;
        std::vector<double> activations;
        std::vector<double> deltas;
    };

    std::vector<Layer> layers_;
    double learning_rate_;
    bool trained_ = false;

    static double sigmoid(double x);
    static double sigmoid_derivative(double x);
    static double relu(double x);
    static double relu_derivative(double x);

    std::vector<double> forward_pass(const std::vector<double>& input) const;
    void backward_pass(double target);
    void update_weights();
};

// Regression Model Wrapper with Ridge/Lasso Regularization
class RegressionEngine {
public:
    enum class RegressionType {
        LINEAR,
        POLYNOMIAL,
        SVR,
        NEURAL_NETWORK
    };

    RegressionEngine();
    ~RegressionEngine();

    // Create and train a regression model
    bool train(
        RegressionType type,
        const std::vector<RegressionDataPoint>& data,
        const std::map<std::string, double>& hyperparams
    );

    double predict(const std::vector<double>& features) const;
    RegressionMetrics evaluate(const std::vector<RegressionDataPoint>& test_data) const;

    RegressionType get_model_type() const { return current_type_; }
    bool is_trained() const { return trained_; }

private:
    RegressionType current_type_;
    std::unique_ptr<LinearRegression> linear_model_;
    std::unique_ptr<PolynomialRegression> poly_model_;
    std::unique_ptr<SupportVectorRegression> svr_model_;
    std::unique_ptr<NeuralNetworkRegression> nn_model_;
    bool trained_ = false;

    // Ridge regression (L2 regularization)
    static double ridge_penalty(
        const std::vector<double>& coefficients,
        double alpha
    );

    // Lasso regression (L1 regularization)
    static double lasso_penalty(
        const std::vector<double>& coefficients,
        double alpha
    );
};

}  // namespace bendcore::parametric_ai
