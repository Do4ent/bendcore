// Осевая линия по кускам (axis.hpp) должна совпадать с trace() и axis_length().
// Для спирали длина по отсчётам меньше точной (хорды), поэтому допуск больше.
#include "bendcore/axis.hpp"
#include "bendcore/tubegeom.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

using namespace bendcore;

static int failures = 0;

static void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}

static double dist(const Vec3& a, const Vec3& b) { return norm(sub(a, b)); }

static void check_path(const Path& path, const char* name, double rel_tol) {
    const auto pieces = axis_pieces(path);
    const auto pts = trace(path, 64);
    double len = 0.0;
    for (std::size_t i = 0; i < pieces.size(); ++i) {
        const auto& p = pieces[i];
        if (i > 0) check(dist(p.p0, pieces[i - 1].p1) < 1e-9, "кусок непрерывен с предыдущим");
        if (p.kind == AxisPiece::Kind::Line) {
            len += dist(p.p0, p.p1);
        } else if (p.kind == AxisPiece::Kind::Arc) {
            const double c = dot(unit(sub(p.p0, p.centre)), unit(sub(p.p1, p.centre)));
            len += p.R * std::acos(std::clamp(c, -1.0, 1.0));
        } else {
            for (std::size_t k = 1; k < p.pts.size(); ++k) len += dist(p.pts[k], p.pts[k - 1]);
        }
    }
    check(dist(pieces.back().p1, pts.back()) < 1e-6, name);
    const double L = axis_length(path);
    std::printf("%s: длина кусков %.6f, axis_length %.6f\n", name, len, L);
    check(std::abs(len - L) <= rel_tol * L, "сумма длин кусков = axis_length");
}

int main() {
    check_path({Straight{100}, Bend{50, 90}, Straight{100}}, "плоский путь", 1e-9);
    check_path({Straight{50}, Bend{40, 60, 0}, Straight{30}, Bend{40, -70, 90}, Straight{40}}, "путь с поворотом A", 1e-9);
    check_path({Straight{20}, Bend{30, 45, 45}, Straight{20}, Bend{30, 45, 0}, Straight{10}}, "пространственный путь", 1e-9);
    check_path({Straight{10}, Bend{30, 0.3}, Straight{10}, Bend{30, 90}}, "вырожденный гиб пропущен", 1e-9);
    check_path({Straight{50}, Spiral{50, 30, 2, +1}, Straight{50}}, "спираль в прямых", 1e-3);
    check_path({Straight{40}, Bend{50, 90}, Spiral{50, 30, 1.5, -1}, Straight{20}}, "гиб и спираль", 1e-3);
    if (failures == 0) std::printf("OK axis\n");
    return failures == 0 ? 0 : 1;
}
