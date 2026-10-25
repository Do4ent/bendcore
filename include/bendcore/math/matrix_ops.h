#pragma once

#include <vector>
#include <cmath>
#include <utility>
#include <tuple>
#include <stdexcept>
#include <cstring>
#include <algorithm>

// Try to include Eigen if available
#ifdef HAVE_EIGEN
    #include <Eigen/Dense>
    namespace bendcore::math {
        using Matrix3d = Eigen::Matrix3d;
        using Vector3d = Eigen::Vector3d;
        using MatrixXd = Eigen::MatrixXd;
        using VectorXd = Eigen::VectorXd;
    }
#else
    // Minimal matrix implementations without Eigen
    namespace bendcore::math {
        class Matrix3d {
        public:
            double m[3][3];
            Matrix3d() { std::memset(m, 0, sizeof(m)); }
            static Matrix3d Identity() {
                Matrix3d I;
                I.m[0][0] = I.m[1][1] = I.m[2][2] = 1.0;
                return I;
            }
            double determinant() const;
            Matrix3d transpose() const;
            bool isIdentity(double eps) const;
        };

        class Vector3d {
        public:
            double m[3];
            Vector3d() { m[0] = m[1] = m[2] = 0; }
            Vector3d(double x, double y, double z) { m[0] = x; m[1] = y; m[2] = z; }
            double& operator()(int i) { return m[i]; }
            const double& operator()(int i) const { return m[i]; }
            double norm() const;
            Vector3d normalized() const;
            double dot(const Vector3d& v) const;
            Vector3d cross(const Vector3d& v) const;
            Vector3d operator*(double scalar) const;
        };

        using MatrixXd = std::vector<std::vector<double>>;
        using VectorXd = std::vector<double>;
    }
#endif

namespace bendcore::math {

/**
 * Matrix Operations wrapper around Eigen library
 * Provides optimized matrix operations for CAD calculations
 */
class MatrixOps {
public:
    using Matrix3d = bendcore::math::Matrix3d;
    using Vector3d = bendcore::math::Vector3d;
    using MatrixXd = bendcore::math::MatrixXd;
    using VectorXd = bendcore::math::VectorXd;

    /**
     * Multiply two 3x3 matrices
     */
    static Matrix3d multiply(const Matrix3d& A, const Matrix3d& B) noexcept;

    /**
     * Multiply matrix by vector
     */
    static Vector3d multiply(const Matrix3d& M, const Vector3d& v) noexcept;

    /**
     * Transform a vector by rotation matrix
     */
    static Vector3d transform(const Matrix3d& rotation, const Vector3d& point) noexcept;

    /**
     * Calculate inverse of 3x3 matrix
     */
    static Matrix3d inverse(const Matrix3d& M);

    /**
     * Calculate determinant
     */
    static double determinant(const Matrix3d& M) noexcept;

    /**
     * Transpose matrix
     */
    static Matrix3d transpose(const Matrix3d& M) noexcept;

    /**
     * Calculate eigenvalues and eigenvectors for stress analysis
     */
    static std::pair<VectorXd, MatrixXd> eigen_decomposition(const Matrix3d& M);

    /**
     * Solve linear system Ax = b
     */
    static VectorXd solve_linear_system(const MatrixXd& A, const VectorXd& b);

    /**
     * Calculate QR decomposition
     */
    static std::pair<MatrixXd, MatrixXd> qr_decomposition(const MatrixXd& M);

    /**
     * Create rotation matrix from axis and angle (Rodrigues formula)
     */
    static Matrix3d rotation_matrix(const Vector3d& axis, double angle_radians);

    /**
     * Calculate angle between two vectors
     */
    static double angle_between(const Vector3d& v1, const Vector3d& v2) noexcept;

    /**
     * Normalize vector
     */
    static Vector3d normalize(const Vector3d& v);

    /**
     * Cross product
     */
    static Vector3d cross(const Vector3d& a, const Vector3d& b) noexcept;

    /**
     * Dot product
     */
    static double dot(const Vector3d& a, const Vector3d& b) noexcept;

    /**
     * Vector magnitude
     */
    static double magnitude(const Vector3d& v) noexcept;

    /**
     * Solve least squares problem (A^T A)x = A^T b
     */
    static VectorXd least_squares(const MatrixXd& A, const VectorXd& b);

    /**
     * SVD decomposition for numerical stability
     */
    static std::tuple<MatrixXd, VectorXd, MatrixXd> svd_decomposition(const MatrixXd& M);

    /**
     * Calculate condition number for numerical stability analysis
     */
    static double condition_number(const MatrixXd& M);
};

} // namespace bendcore::math
