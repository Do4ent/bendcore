#include "bendcore/axis.hpp"

#include <cmath>
#include <numbers>
#include <stdexcept>

#include "bendcore/spiral.hpp"

namespace bendcore {
namespace {

double deg2rad(double d) { return d * std::numbers::pi / 180.0; }

// Поворот вектора v вокруг единичной оси axis (формула Родрига), как в tubegeom.cpp.
Vec3 rot(const Vec3& v, const Vec3& axis, double angle_rad) {
    const double c = std::cos(angle_rad);
    const double s = std::sin(angle_rad);
    return add(add(mul(v, c), mul(cross(axis, v), s)), mul(axis, dot(axis, v) * (1.0 - c)));
}

}  // namespace

std::vector<AxisPiece> axis_pieces(const Path& path, Vec3 start, int spiral_steps) {
    Vec3 pos = start;
    Vec3 heading{1.0, 0.0, 0.0};
    Vec3 up{0.0, 0.0, 1.0};
    std::vector<AxisPiece> out;

    for (const auto& it : path) {
        if (const auto* s = std::get_if<Straight>(&it)) {
            if (s->S < 0) throw std::invalid_argument("S < 0: геометрическая ошибка");
            if (s->S > 0) {
                AxisPiece p;
                p.kind = AxisPiece::Kind::Line;
                p.p0 = pos;
                pos = add(pos, mul(heading, s->S));
                p.p1 = pos;
                p.t0 = p.t1 = heading;
                out.push_back(p);
            }
        } else if (const auto* b = std::get_if<Bend>(&it)) {
            if (std::abs(b->B) < 0.5) continue;  // вырожденный гиб — как в trace
            up = rot(up, heading, deg2rad(b->A));
            const double sgn = b->B >= 0 ? 1.0 : -1.0;
            const Vec3 side = mul(cross(up, heading), sgn);
            const Vec3 centre = add(pos, mul(side, b->R));
            const double total = deg2rad(std::abs(b->B));
            AxisPiece p;
            p.kind = AxisPiece::Kind::Arc;
            p.p0 = pos;
            p.centre = centre;
            p.axis = up;
            p.R = b->R;
            p.mid = add(centre, rot(sub(pos, centre), up, sgn * total / 2.0));
            p.p1 = add(centre, rot(sub(pos, centre), up, sgn * total));
            p.t0 = mul(unit(cross(up, sub(p.p0, centre))), sgn);
            p.t1 = mul(unit(cross(up, sub(p.p1, centre))), sgn);
            out.push_back(p);
            pos = p.p1;
            heading = rot(heading, up, sgn * total);
        } else if (const auto* sp = std::get_if<Spiral>(&it)) {
            // Как в trace (1.2.1): ось — проекция «верха» на плоскость ⊥ касательной.
            const Vec3 up_perp = sub(up, mul(heading, dot(up, heading)));
            if (norm(up_perp) < 1e-9) throw std::invalid_argument("не задан верх трубы для оси спирали");
            const Vec3 axis = spiral_axis(heading, sp->R, sp->p, sp->direction, unit(up_perp));
            const SpiralCentreline sc = spiral_centreline(pos, heading, sp->R, sp->p, sp->n, sp->direction,
                                                          axis, spiral_steps);
            AxisPiece p;
            p.kind = AxisPiece::Kind::Spiral;
            p.p0 = pos;
            p.p1 = sc.pts.back();
            p.t0 = heading;
            p.t1 = unit(sc.tend);
            p.axis = axis;
            p.R = sp->R;
            p.pts = sc.pts;
            out.push_back(p);
            pos = p.p1;
            heading = p.t1;
            // следующий гиб: верх — по плоскости «касательная выхода + ось», знак — по прежнему «верху»
            Vec3 n_plane = cross(heading, axis);
            if (norm(n_plane) < 1e-9) n_plane = sub(up_perp, mul(heading, dot(up_perp, heading)));
            up = unit(n_plane);
            if (dot(up, up_perp) < 0) up = mul(up, -1.0);
        }
    }
    return out;
}

}  // namespace bendcore
