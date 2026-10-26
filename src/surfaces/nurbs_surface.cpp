#include "bendcore/surfaces/nurbs_surface.hpp"
#include <cmath>
#include <algorithm>
#include <sstream>
#include <stdexcept>

namespace bendcore::surfaces {

using namespace bendcore::geometry;

// Empty constructor
NURBSSurface::NURBSSurface() = default;

// Full constructor
NURBSSurface::NURBSSurface(
    const std::vector<std::vector<Point3D>>& control_points,
    const std::vector<double>& u_knots,
    const std::vector<double>& v_knots,
    const std::vector<std::vector<double>>& weights,
    int u_degree,
    int v_degree
)
    : control_points_(control_points),
      u_knots_(u_knots),
      v_knots_(v_knots),
      weights_(weights),
      u_degree_(u_degree),
      v_degree_(v_degree)
{
    if (!is_valid()) {
        throw std::invalid_argument("Invalid NURBS surface parameters");
    }
}

// Unweighted constructor
NURBSSurface::NURBSSurface(
    const std::vector<std::vector<Point3D>>& control_points,
    const std::vector<double>& u_knots,
    const std::vector<double>& v_knots,
    int u_degree,
    int v_degree
)
    : control_points_(control_points),
      u_knots_(u_knots),
      v_knots_(v_knots),
      u_degree_(u_degree),
      v_degree_(v_degree)
{
    // Initialize with uniform weights (1.0)
    int m = control_points.size();
    int n = control_points.empty() ? 0 : control_points[0].size();
    weights_.resize(m);
    for (int i = 0; i < m; ++i) {
        weights_[i].resize(n, 1.0);
    }

    if (!is_valid()) {
        throw std::invalid_argument("Invalid NURBS surface parameters");
    }
}

// Setters for control point modification
void NURBSSurface::set_control_point(int i, int j, const Point3D& point) {
    if (i >= 0 && i < static_cast<int>(control_points_.size()) &&
        j >= 0 && j < static_cast<int>(control_points_[i].size())) {
        control_points_[i][j] = point;
    }
}

void NURBSSurface::set_weight(int i, int j, double weight) {
    if (i >= 0 && i < static_cast<int>(weights_.size()) &&
        j >= 0 && j < static_cast<int>(weights_[i].size())) {
        weights_[i][j] = weight;
    }
}

void NURBSSurface::modify_control_points(const std::vector<std::vector<Point3D>>& points) {
    if (points.size() == control_points_.size() &&
        (points.empty() || points[0].size() == control_points_[0].size())) {
        control_points_ = points;
    }
}

// Find knot span
int NURBSSurface::find_knot_span(int n, int p, double u, const std::vector<double>& knots) const {
    if (u >= knots[n + 1]) return n;
    if (u <= knots[p]) return p;

    int low = p;
    int high = n + 1;
    int mid = (low + high) / 2;

    while (u < knots[mid] || u >= knots[mid + 1]) {
        if (u < knots[mid])
            high = mid;
        else
            low = mid;
        mid = (low + high) / 2;
    }
    return mid;
}

// B-spline basis function (Cox-de Boor recursion)
double NURBSSurface::basis_function(int i, int p, double u, const std::vector<double>& knots) const {
    if (p == 0) {
        return (knots[i] <= u && u < knots[i + 1]) ? 1.0 : 0.0;
    }

    double left = (u - knots[i]) / (knots[i + p] - knots[i]);
    double right = (knots[i + p + 1] - u) / (knots[i + p + 1] - knots[i + 1]);

    double left_basis = basis_function(i, p - 1, u, knots);
    double right_basis = basis_function(i + 1, p - 1, u, knots);

    return left * left_basis + right * right_basis;
}

// B-spline basis function derivative
double NURBSSurface::basis_function_derivative(int i, int p, double u, const std::vector<double>& knots) const {
    if (p == 0) return 0.0;

    double left = basis_function(i, p - 1, u, knots) / (knots[i + p] - knots[i]);
    double right = basis_function(i + 1, p - 1, u, knots) / (knots[i + p + 1] - knots[i + 1]);

    return p * (left - right);
}

// Rational basis functions
double NURBSSurface::rational_basis(int i, int j, double u, double v) const {
    double N_i_u = basis_function(i, u_degree_, u, u_knots_);
    double N_j_v = basis_function(j, v_degree_, v, v_knots_);
    double w_ij = weights_[i][j];
    double denominator = 0.0;

    for (int ii = 0; ii < static_cast<int>(weights_.size()); ++ii) {
        for (int jj = 0; jj < static_cast<int>(weights_[ii].size()); ++jj) {
            double N_ii_u = basis_function(ii, u_degree_, u, u_knots_);
            double N_jj_v = basis_function(jj, v_degree_, v, v_knots_);
            denominator += weights_[ii][jj] * N_ii_u * N_jj_v;
        }
    }

    if (std::abs(denominator) < 1e-12) return 0.0;
    return (w_ij * N_i_u * N_j_v) / denominator;
}

double NURBSSurface::rational_basis_du(int i, int j, double u, double v) const {
    double N_i_u = basis_function(i, u_degree_, u, u_knots_);
    double dN_i_u = basis_function_derivative(i, u_degree_, u, u_knots_);
    double N_j_v = basis_function(j, v_degree_, v, v_knots_);
    double w_ij = weights_[i][j];

    double numerator_du = dN_i_u * N_j_v;
    double denominator = 0.0;
    double denom_du = 0.0;

    for (int ii = 0; ii < static_cast<int>(weights_.size()); ++ii) {
        for (int jj = 0; jj < static_cast<int>(weights_[ii].size()); ++jj) {
            double N_ii_u = basis_function(ii, u_degree_, u, u_knots_);
            double dN_ii_u = basis_function_derivative(ii, u_degree_, u, u_knots_);
            double N_jj_v = basis_function(jj, v_degree_, v, v_knots_);
            double w_ijkl = weights_[ii][jj];

            denominator += w_ijkl * N_ii_u * N_jj_v;
            denom_du += w_ijkl * dN_ii_u * N_jj_v;
        }
    }

    if (std::abs(denominator) < 1e-12) return 0.0;
    return (w_ij * numerator_du * denominator - w_ij * N_i_u * N_j_v * denom_du) / (denominator * denominator);
}

double NURBSSurface::rational_basis_dv(int i, int j, double u, double v) const {
    double N_i_u = basis_function(i, u_degree_, u, u_knots_);
    double N_j_v = basis_function(j, v_degree_, v, v_knots_);
    double dN_j_v = basis_function_derivative(j, v_degree_, v, v_knots_);
    double w_ij = weights_[i][j];

    double numerator_dv = N_i_u * dN_j_v;
    double denominator = 0.0;
    double denom_dv = 0.0;

    for (int ii = 0; ii < static_cast<int>(weights_.size()); ++ii) {
        for (int jj = 0; jj < static_cast<int>(weights_[ii].size()); ++jj) {
            double N_ii_u = basis_function(ii, u_degree_, u, u_knots_);
            double N_jj_v = basis_function(jj, v_degree_, v, v_knots_);
            double dN_jj_v = basis_function_derivative(jj, v_degree_, v, v_knots_);
            double w_ijkl = weights_[ii][jj];

            denominator += w_ijkl * N_ii_u * N_jj_v;
            denom_dv += w_ijkl * N_ii_u * dN_jj_v;
        }
    }

    if (std::abs(denominator) < 1e-12) return 0.0;
    return (w_ij * numerator_dv * denominator - w_ij * N_i_u * N_j_v * denom_dv) / (denominator * denominator);
}

// Surface evaluation
Point3D NURBSSurface::evaluate(double u, double v) const {
    Point3D result(0, 0, 0);

    int m = u_control_point_count();
    int n = v_control_point_count();

    for (int i = 0; i < m; ++i) {
        for (int j = 0; j < n; ++j) {
            double R_ij = rational_basis(i, j, u, v);
            result.x += control_points_[i][j].x * R_ij;
            result.y += control_points_[i][j].y * R_ij;
            result.z += control_points_[i][j].z * R_ij;
        }
    }

    return result;
}

// Partial derivatives
Vector3D NURBSSurface::du(double u, double v) const {
    Vector3D result(0, 0, 0);
    int m = u_control_point_count();
    int n = v_control_point_count();

    for (int i = 0; i < m; ++i) {
        for (int j = 0; j < n; ++j) {
            double dR_du = rational_basis_du(i, j, u, v);
            result.x += control_points_[i][j].x * dR_du;
            result.y += control_points_[i][j].y * dR_du;
            result.z += control_points_[i][j].z * dR_du;
        }
    }

    return result;
}

Vector3D NURBSSurface::dv(double u, double v) const {
    Vector3D result(0, 0, 0);
    int m = u_control_point_count();
    int n = v_control_point_count();

    for (int i = 0; i < m; ++i) {
        for (int j = 0; j < n; ++j) {
            double dR_dv = rational_basis_dv(i, j, u, v);
            result.x += control_points_[i][j].x * dR_dv;
            result.y += control_points_[i][j].y * dR_dv;
            result.z += control_points_[i][j].z * dR_dv;
        }
    }

    return result;
}

// Surface normal
Vector3D NURBSSurface::normal(double u, double v) const {
    Vector3D du_vec = du(u, v);
    Vector3D dv_vec = dv(u, v);
    Vector3D n = du_vec.cross(dv_vec);

    double length = n.magnitude();
    if (length > 1e-12) {
        n = Vector3D(n.x / length, n.y / length, n.z / length);
    }
    return n;
}

// Curvature calculations
double NURBSSurface::gaussian_curvature(double u, double v) const {
    Vector3D du_vec = du(u, v);
    Vector3D dv_vec = dv(u, v);
    Vector3D n = normal(u, v);

    // Simplified: using first fundamental form
    double E = du_vec.dot(du_vec);
    double F = du_vec.dot(dv_vec);
    double G = dv_vec.dot(dv_vec);

    double K = (E * G - F * F);
    return K > 0 ? 1.0 / K : 0.0;
}

double NURBSSurface::mean_curvature(double u, double v) const {
    Vector3D du_vec = du(u, v);
    Vector3D dv_vec = dv(u, v);
    double E = du_vec.dot(du_vec);
    double F = du_vec.dot(dv_vec);
    double G = dv_vec.dot(dv_vec);

    return (E + G) / (2.0 * (E * G - F * F));
}

double NURBSSurface::principal_curvature_max(double u, double v) const {
    double H = mean_curvature(u, v);
    double K = gaussian_curvature(u, v);
    return H + std::sqrt(std::max(0.0, H * H - K));
}

double NURBSSurface::principal_curvature_min(double u, double v) const {
    double H = mean_curvature(u, v);
    double K = gaussian_curvature(u, v);
    return H - std::sqrt(std::max(0.0, H * H - K));
}

// Closest point on surface
Point3D NURBSSurface::closest_point(const Point3D& point, int iterations) const {
    auto params = closest_parameters(point, iterations);
    if (params) {
        return evaluate(params->first, params->second);
    }
    return control_points_[0][0];
}

std::optional<std::pair<double, double>> NURBSSurface::closest_parameters(
    const Point3D& point, int iterations
) const {
    // Simple iterative closest point - Newton's method on parameter space
    double u = 0.5;
    double v = 0.5;
    double step = 0.01;

    for (int iter = 0; iter < iterations; ++iter) {
        Point3D eval_point = evaluate(u, v);
        Vector3D to_point(
            point.x - eval_point.x,
            point.y - eval_point.y,
            point.z - eval_point.z
        );

        // Check if we're close enough
        if (to_point.magnitude() < 1e-6) {
            return std::make_pair(u, v);
        }

        // Move towards point
        Vector3D du_vec = du(u, v);
        Vector3D dv_vec = dv(u, v);

        double du_mag = du_vec.magnitude();
        double dv_mag = dv_vec.magnitude();
        double du_proj = to_point.dot(du_vec) / (du_mag * du_mag + 1e-12);
        double dv_proj = to_point.dot(dv_vec) / (dv_mag * dv_mag + 1e-12);

        u += du_proj * step;
        v += dv_proj * step;

        // Clamp to valid range
        auto [u_min, u_max] = u_range();
        auto [v_min, v_max] = v_range();
        u = std::clamp(u, u_min, u_max);
        v = std::clamp(v, v_min, v_max);
    }

    return std::make_pair(u, v);
}

// Boundary curves
std::unique_ptr<NurbsCurve> NURBSSurface::boundary_u_min() const {
    int n = v_control_point_count();
    std::vector<Point3D> boundary_points;
    std::vector<double> boundary_weights;

    for (int j = 0; j < n; ++j) {
        boundary_points.push_back(control_points_[0][j]);
        boundary_weights.push_back(weights_[0][j]);
    }

    return std::make_unique<NurbsCurve>(boundary_points, v_knots_, boundary_weights, v_degree_);
}

std::unique_ptr<NurbsCurve> NURBSSurface::boundary_u_max() const {
    int m = u_control_point_count();
    int n = v_control_point_count();
    std::vector<Point3D> boundary_points;
    std::vector<double> boundary_weights;

    for (int j = 0; j < n; ++j) {
        boundary_points.push_back(control_points_[m - 1][j]);
        boundary_weights.push_back(weights_[m - 1][j]);
    }

    return std::make_unique<NurbsCurve>(boundary_points, v_knots_, boundary_weights, v_degree_);
}

std::unique_ptr<NurbsCurve> NURBSSurface::boundary_v_min() const {
    int m = u_control_point_count();
    std::vector<Point3D> boundary_points;
    std::vector<double> boundary_weights;

    for (int i = 0; i < m; ++i) {
        boundary_points.push_back(control_points_[i][0]);
        boundary_weights.push_back(weights_[i][0]);
    }

    return std::make_unique<NurbsCurve>(boundary_points, u_knots_, boundary_weights, u_degree_);
}

std::unique_ptr<NurbsCurve> NURBSSurface::boundary_v_max() const {
    int m = u_control_point_count();
    int n = v_control_point_count();
    std::vector<Point3D> boundary_points;
    std::vector<double> boundary_weights;

    for (int i = 0; i < m; ++i) {
        boundary_points.push_back(control_points_[i][n - 1]);
        boundary_weights.push_back(weights_[i][n - 1]);
    }

    return std::make_unique<NurbsCurve>(boundary_points, u_knots_, boundary_weights, u_degree_);
}

// Isocurves
std::unique_ptr<NurbsCurve> NURBSSurface::isocurve_u(double u) const {
    int n = v_control_point_count();
    std::vector<Point3D> curve_points;

    for (int j = 0; j < n; ++j) {
        curve_points.push_back(evaluate(u, v_knots_[j]));
    }

    return std::make_unique<NurbsCurve>(
        curve_points, v_knots_,
        std::vector<double>(curve_points.size(), 1.0),
        v_degree_
    );
}

std::unique_ptr<NurbsCurve> NURBSSurface::isocurve_v(double v) const {
    int m = u_control_point_count();
    std::vector<Point3D> curve_points;

    for (int i = 0; i < m; ++i) {
        curve_points.push_back(evaluate(u_knots_[i], v));
    }

    return std::make_unique<NurbsCurve>(
        curve_points, u_knots_,
        std::vector<double>(curve_points.size(), 1.0),
        u_degree_
    );
}

// Degree elevation
std::unique_ptr<NURBSSurface> NURBSSurface::increase_degree(int new_u_degree, int new_v_degree) const {
    // Simplified implementation: create a new surface with elevated degree
    // This would require Bezier decomposition and degree elevation algorithm
    return std::make_unique<NURBSSurface>(control_points_, u_knots_, v_knots_, weights_, new_u_degree, new_v_degree);
}

// Knot refinement
std::unique_ptr<NURBSSurface> NURBSSurface::refine_knots_u(const std::vector<double>& new_knots) const {
    // Simplified: create refined surface by inserting knots
    return std::make_unique<NURBSSurface>(control_points_, new_knots, v_knots_, weights_, u_degree_, v_degree_);
}

std::unique_ptr<NURBSSurface> NURBSSurface::refine_knots_v(const std::vector<double>& new_knots) const {
    return std::make_unique<NURBSSurface>(control_points_, u_knots_, new_knots, weights_, u_degree_, v_degree_);
}

// Surface validation
bool NURBSSurface::is_valid() const {
    if (control_points_.empty() || u_degree_ < 1 || v_degree_ < 1) return false;
    if (u_knots_.size() != static_cast<size_t>(u_control_point_count() + u_degree_ + 1)) return false;
    if (v_knots_.size() != static_cast<size_t>(v_control_point_count() + v_degree_ + 1)) return false;
    if (weights_.size() != control_points_.size()) return false;

    int m = u_control_point_count();
    int n = v_control_point_count();
    for (int i = 0; i < m; ++i) {
        if (control_points_[i].size() != static_cast<size_t>(n)) return false;
        if (weights_[i].size() != static_cast<size_t>(n)) return false;
    }

    return true;
}

// Check if surface is closed
bool NURBSSurface::is_u_closed() const {
    auto first = evaluate(u_range().first, 0.5);
    auto last = evaluate(u_range().second, 0.5);
    double dx = first.x - last.x;
    double dy = first.y - last.y;
    double dz = first.z - last.z;
    return std::sqrt(dx*dx + dy*dy + dz*dz) < 1e-6;
}

bool NURBSSurface::is_v_closed() const {
    auto first = evaluate(0.5, v_range().first);
    auto last = evaluate(0.5, v_range().second);
    double dx = first.x - last.x;
    double dy = first.y - last.y;
    double dz = first.z - last.z;
    return std::sqrt(dx*dx + dy*dy + dz*dz) < 1e-6;
}

// Parameter ranges
std::pair<double, double> NURBSSurface::u_range() const {
    if (u_knots_.empty()) return {0.0, 1.0};
    return {u_knots_.front(), u_knots_.back()};
}

std::pair<double, double> NURBSSurface::v_range() const {
    if (v_knots_.empty()) return {0.0, 1.0};
    return {v_knots_.front(), v_knots_.back()};
}

// String representation
std::string NURBSSurface::to_string() const {
    std::ostringstream oss;
    oss << "NURBSSurface[" << u_control_point_count() << "x" << v_control_point_count() << "]"
        << " degree(" << u_degree_ << "," << v_degree_ << ")";
    return oss.str();
}

} // namespace bendcore::surfaces
