#include "bendcore/spiral.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace bendcore {

Vec3 spiral_axis(const Vec3& heading, double R, double p, int direction, Vec3 up) {
    const Vec3 h = unit(heading);
    const double q = direction * p;
    const double L = std::hypot(2.0 * std::numbers::pi * R, q);
    const double s = q / L;                                   // sinα
    const double c = 2.0 * std::numbers::pi * R / L;          // cosα
    const Vec3 w = unit(sub(up, mul(h, dot(up, h))));
    return add(mul(h, s), mul(w, c));
}

SpiralCentreline spiral_centreline(const Vec3& start, const Vec3& heading, double R, double p, double n,
                                   int direction, std::optional<Vec3> axis, int steps_per_turn) {
    if (R <= 0 || n <= 0 || p < 0) throw std::invalid_argument("R и n должны быть > 0, p ≥ 0");
    const Vec3 h = unit(heading);
    const double q = direction * p;
    const double L = std::hypot(2.0 * std::numbers::pi * R, q);
    const double s = q / L;

    const Vec3 a = axis ? unit(*axis) : spiral_axis(h, R, p, direction);
    if (std::abs(dot(h, a) - s) > 1e-6) throw std::invalid_argument("прямая не касается спирали: h·a должно быть sinα");

    const Vec3 u = unit(sub(h, mul(a, s)));                   // ⊥ a
    const Vec3 e1 = cross(u, a);                              // радиальный единичный в точке входа
    const Vec3 centre = sub(start, mul(e1, R));

    const int steps = std::max(8, static_cast<int>(steps_per_turn * n));
    const double total = 2.0 * std::numbers::pi * n;
    SpiralCentreline out;
    out.pts.reserve(static_cast<std::size_t>(steps) + 1);
    for (int k = 0; k <= steps; ++k) {
        const double th = total * k / steps;
        Vec3 pt = centre;
        pt = add(pt, mul(e1, R * std::cos(th)));
        pt = add(pt, mul(u, R * std::sin(th)));
        pt = add(pt, mul(a, q * th / (2.0 * std::numbers::pi)));
        out.pts.push_back(pt);
    }
    const double th_end = total;
    Vec3 tend = add(mul(e1, -R * std::sin(th_end)), mul(u, R * std::cos(th_end)));
    tend = add(tend, mul(a, q / (2.0 * std::numbers::pi)));
    out.tend = unit(tend);
    out.axis = a;
    return out;
}

double spiral_axis_length(double R, double p, double n) {
    return n * std::sqrt(std::pow(2.0 * std::numbers::pi * R, 2) + p * p);
}

}  // namespace bendcore
