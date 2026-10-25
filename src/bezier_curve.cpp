#include <bendcore/geometry/bezier_curve.h>
#include <algorithm>
#include <numeric>

namespace bendcore {

Vector3D BezierCurve::evaluate(double t) const {
    int n = controlPoints.size() - 1;
    Vector3D result;

    for (int i = 0; i <= n; ++i) {
        double b = bernstein(i, n, t);
        result = result + controlPoints[i] * b;
    }

    return result;
}

Vector3D BezierCurve::derivative(double t) const {
    int n = controlPoints.size() - 1;
    Vector3D result;

    for (int i = 0; i < n; ++i) {
        Vector3D diff = controlPoints[i + 1] - controlPoints[i];
        double b = bernstein(i, n - 1, t);
        result = result + diff * (b * n);
    }

    return result;
}

std::vector<Vector3D> BezierCurve::discretize(int segments) const {
    std::vector<Vector3D> points;
    points.reserve(segments + 1);

    for (int i = 0; i <= segments; ++i) {
        double t = static_cast<double>(i) / segments;
        points.push_back(evaluate(t));
    }

    return points;
}

double BezierCurve::length(int segments) const {
    auto points = discretize(segments);
    double totalLength = 0.0;

    for (size_t i = 1; i < points.size(); ++i) {
        totalLength += (points[i] - points[i - 1]).length();
    }

    return totalLength;
}

double BezierCurve::bernstein(int i, int n, double t) const {
    if (t < 0 || t > 1) return 0;

    // Compute binomial coefficient C(n, i)
    double coeff = 1.0;
    for (int k = 0; k < i; ++k) {
        coeff *= static_cast<double>(n - k) / (k + 1);
    }

    // Compute Bernstein polynomial
    double result = coeff;
    for (int k = 0; k < i; ++k) {
        result *= t;
    }
    for (int k = 0; k < n - i; ++k) {
        result *= (1 - t);
    }

    return result;
}

Vector3D NURBSCurve::evaluate(double u) const {
    int n = controlPoints.size() - 1;
    double numerator_sum = 0.0;
    Vector3D denominator_sum;

    for (int i = 0; i <= n; ++i) {
        double basis = basisFunction(i, degree, u);
        double weight = weights[i];
        numerator_sum += basis * weight;
        denominator_sum = denominator_sum + controlPoints[i] * (basis * weight);
    }

    if (numerator_sum == 0) return Vector3D();

    return denominator_sum * (1.0 / numerator_sum);
}

std::vector<Vector3D> NURBSCurve::discretize(int segments) const {
    std::vector<Vector3D> points;
    points.reserve(segments + 1);

    double uMin = knots.front();
    double uMax = knots.back();

    for (int i = 0; i <= segments; ++i) {
        double u = uMin + (uMax - uMin) * (static_cast<double>(i) / segments);
        points.push_back(evaluate(u));
    }

    return points;
}

double NURBSCurve::basisFunction(int i, int p, double u) const {
    if (p == 0) {
        if (knots[i] <= u && u < knots[i + 1]) {
            return 1.0;
        }
        return 0.0;
    }

    double left = 0.0, right = 0.0;

    double denom1 = knots[i + p] - knots[i];
    if (denom1 != 0) {
        left = (u - knots[i]) / denom1 * basisFunction(i, p - 1, u);
    }

    double denom2 = knots[i + p + 1] - knots[i + 1];
    if (denom2 != 0) {
        right = (knots[i + p + 1] - u) / denom2 * basisFunction(i + 1, p - 1, u);
    }

    return left + right;
}

} // namespace bendcore
