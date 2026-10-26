#pragma once

#include <vector>
#include <optional>
#include <memory>
#include <cmath>

#include "bendcore/geometry/geometry_kernel.hpp"
#include "bendcore/geometry/curve_operations.hpp"

namespace bendcore::surfaces {

/**
 * M5 Phase - Surface Modeling (GAP-021)
 * NURBS Surface Implementation - Core surface representation
 */

/**
 * NURBSSurface — Non-Uniform Rational B-Spline Surface
 *
 * Represents a parametric surface defined by:
 * - Control points (2D grid)
 * - Knot vectors in U and V directions
 * - Weights (rational component)
 * - Surface degrees in U and V directions
 */
class NURBSSurface {
public:
    // Constructor: empty surface
    NURBSSurface();

    // Constructor: from all parameters
    NURBSSurface(
        const std::vector<std::vector<geometry::Point3D>>& control_points,
        const std::vector<double>& u_knots,
        const std::vector<double>& v_knots,
        const std::vector<std::vector<double>>& weights,
        int u_degree,
        int v_degree
    );

    // Constructor: unweighted (uniform weights = 1.0)
    NURBSSurface(
        const std::vector<std::vector<geometry::Point3D>>& control_points,
        const std::vector<double>& u_knots,
        const std::vector<double>& v_knots,
        int u_degree,
        int v_degree
    );

    // Getters for surface parameters
    [[nodiscard]] int u_degree() const { return u_degree_; }
    [[nodiscard]] int v_degree() const { return v_degree_; }
    [[nodiscard]] int u_control_point_count() const { return control_points_.size(); }
    [[nodiscard]] int v_control_point_count() const {
        return control_points_.empty() ? 0 : control_points_[0].size();
    }
    [[nodiscard]] const std::vector<std::vector<geometry::Point3D>>& control_points() const {
        return control_points_;
    }
    [[nodiscard]] const std::vector<double>& u_knots() const { return u_knots_; }
    [[nodiscard]] const std::vector<double>& v_knots() const { return v_knots_; }
    [[nodiscard]] const std::vector<std::vector<double>>& weights() const {
        return weights_;
    }

    // Setters for control point modification
    void set_control_point(int i, int j, const geometry::Point3D& point);
    void set_weight(int i, int j, double weight);
    void modify_control_points(const std::vector<std::vector<geometry::Point3D>>& points);

    // Surface evaluation
    [[nodiscard]] geometry::Point3D evaluate(double u, double v) const;
    [[nodiscard]] geometry::Vector3D normal(double u, double v) const;
    [[nodiscard]] geometry::Vector3D du(double u, double v) const;  // Partial derivative ∂S/∂u
    [[nodiscard]] geometry::Vector3D dv(double u, double v) const;  // Partial derivative ∂S/∂v

    // Curvature and continuity analysis
    [[nodiscard]] double gaussian_curvature(double u, double v) const;
    [[nodiscard]] double mean_curvature(double u, double v) const;
    [[nodiscard]] double principal_curvature_max(double u, double v) const;
    [[nodiscard]] double principal_curvature_min(double u, double v) const;

    // Closest point on surface
    [[nodiscard]] geometry::Point3D closest_point(const geometry::Point3D& point, int iterations = 20) const;
    [[nodiscard]] std::optional<std::pair<double, double>> closest_parameters(
        const geometry::Point3D& point, int iterations = 20
    ) const;

    // Boundary curves
    [[nodiscard]] std::unique_ptr<geometry::NurbsCurve> boundary_u_min() const;
    [[nodiscard]] std::unique_ptr<geometry::NurbsCurve> boundary_u_max() const;
    [[nodiscard]] std::unique_ptr<geometry::NurbsCurve> boundary_v_min() const;
    [[nodiscard]] std::unique_ptr<geometry::NurbsCurve> boundary_v_max() const;

    // Curve on surface extraction
    [[nodiscard]] std::unique_ptr<geometry::NurbsCurve> isocurve_u(double u) const;
    [[nodiscard]] std::unique_ptr<geometry::NurbsCurve> isocurve_v(double v) const;

    // Surface modification
    [[nodiscard]] std::unique_ptr<NURBSSurface> increase_degree(int new_u_degree, int new_v_degree) const;
    [[nodiscard]] std::unique_ptr<NURBSSurface> refine_knots_u(const std::vector<double>& new_knots) const;
    [[nodiscard]] std::unique_ptr<NURBSSurface> refine_knots_v(const std::vector<double>& new_knots) const;

    // Surface properties
    [[nodiscard]] bool is_valid() const;
    [[nodiscard]] bool is_u_closed() const;
    [[nodiscard]] bool is_v_closed() const;
    [[nodiscard]] std::pair<double, double> u_range() const;
    [[nodiscard]] std::pair<double, double> v_range() const;

    // Serialization
    [[nodiscard]] std::string to_string() const;

private:
    std::vector<std::vector<geometry::Point3D>> control_points_;
    std::vector<double> u_knots_;
    std::vector<double> v_knots_;
    std::vector<std::vector<double>> weights_;
    int u_degree_ = 0;
    int v_degree_ = 0;

    // B-spline basis functions with caching
    [[nodiscard]] double basis_function(int i, int p, double u, const std::vector<double>& knots) const;
    [[nodiscard]] double basis_function_derivative(int i, int p, double u, const std::vector<double>& knots) const;

    // Rational basis functions
    [[nodiscard]] double rational_basis(int i, int j, double u, double v) const;
    [[nodiscard]] double rational_basis_du(int i, int j, double u, double v) const;
    [[nodiscard]] double rational_basis_dv(int i, int j, double u, double v) const;

    // Helper for finding knot span
    [[nodiscard]] int find_knot_span(int n, int p, double u, const std::vector<double>& knots) const;
};

/**
 * Surface continuity levels
 */
enum class ContinuityLevel {
    G0,  // Positional continuity (touching)
    G1,  // Tangent continuity (same tangent plane)
    G2,  // Curvature continuity (same curvature)
    C0,  // Parametric continuity
    C1,  // First derivative continuity
    C2   // Second derivative continuity
};

/**
 * Surface continuity checking result
 */
struct ContinuityAnalysis {
    ContinuityLevel level = ContinuityLevel::G0;
    double max_gap = 0.0;           // Maximum positional gap
    double max_angle = 0.0;         // Maximum tangent angle
    double max_curvature_diff = 0.0; // Maximum curvature difference
    bool is_continuous = false;
    std::string description;
};

} // namespace bendcore::surfaces
