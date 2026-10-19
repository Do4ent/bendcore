#include "bendcore/sketch2d.hpp"

#include <deque>
#include <numbers>
#include <stdexcept>

#include "GCS.h"

namespace bendcore::sketch2d {
namespace {

// Номер ограничения в PlaneGCS (tag) — индекс + 1; тег 0 — служебные закрепления координат.
int tag_of(std::size_t index) {
    return static_cast<int>(index) + 1;
}

}  // namespace

Id Sketch::add_point(double x, double y) {
    points_.push_back({x, y});
    fixed_.push_back(false);
    pins_.push_back({0.0, 0.0});
    return static_cast<Id>(points_.size() - 1);
}

void Sketch::fix_point(Id p) {
    fixed_.at(p) = true;
    pins_.at(p) = points_.at(p);
}

Id Sketch::add_line(Id a, Id b) {
    points_.at(a);
    points_.at(b);
    lines_.push_back({a, b});
    return static_cast<Id>(lines_.size() - 1);
}

Id Sketch::add_circle(Id center, double radius) {
    points_.at(center);
    circles_.push_back({center, radius});
    return static_cast<Id>(circles_.size() - 1);
}

Id Sketch::add_constraint(const std::string& kind, Id a, Id b, double value, Id c) {
    constraints_.push_back({kind, a, b, c, value, false});
    return tag_of(constraints_.size() - 1);
}

Id Sketch::constrain_coincident(Id a, Id b) {
    return add_constraint("coincident", a, b, 0.0);
}

Id Sketch::constrain_horizontal(Id a, Id b) {
    return add_constraint("horizontal", a, b, 0.0);
}

Id Sketch::constrain_vertical(Id a, Id b) {
    return add_constraint("vertical", a, b, 0.0);
}

Id Sketch::constrain_distance(Id a, Id b, double d) {
    return add_constraint("distance", a, b, d);
}

Id Sketch::constrain_radius(Id circle, double r) {
    circles_.at(circle);
    return add_constraint("radius", circle, -1, r);
}

Id Sketch::constrain_parallel(Id l1, Id l2) {
    lines_.at(l1);
    lines_.at(l2);
    return add_constraint("parallel", l1, l2, 0.0);
}

Id Sketch::constrain_perpendicular(Id l1, Id l2) {
    lines_.at(l1);
    lines_.at(l2);
    return add_constraint("perpendicular", l1, l2, 0.0);
}

Id Sketch::constrain_equal_length(Id l1, Id l2) {
    lines_.at(l1);
    lines_.at(l2);
    return add_constraint("equal_length", l1, l2, 0.0);
}

Id Sketch::constrain_angle(Id l1, Id l2, double deg) {
    lines_.at(l1);
    lines_.at(l2);
    return add_constraint("angle", l1, l2, deg);
}

Id Sketch::constrain_point_on_line(Id p, Id line) {
    points_.at(p);
    lines_.at(line);
    return add_constraint("point_on_line", p, line, 0.0);
}

Id Sketch::constrain_point_on_circle(Id p, Id circle) {
    points_.at(p);
    circles_.at(circle);
    return add_constraint("point_on_circle", p, circle, 0.0);
}

Id Sketch::constrain_equal_radius(Id c1, Id c2) {
    circles_.at(c1);
    circles_.at(c2);
    return add_constraint("equal_radius", c1, c2, 0.0);
}

Id Sketch::constrain_symmetric(Id p1, Id p2, Id axis) {
    points_.at(p1);
    points_.at(p2);
    lines_.at(axis);
    return add_constraint("symmetric", p1, p2, 0.0, axis);
}

void Sketch::remove_constraint(Id c) {
    if (c < 1 || static_cast<std::size_t>(c) > constraints_.size()) {
        throw std::out_of_range("нет ограничения с таким номером");
    }
    constraints_[c - 1].removed = true;
}

Report Sketch::solve() {
    const std::size_t n = points_.size();

    // Хранилища с фиксированными адресами: deque не перемещает элементы при push_back.
    std::deque<double> xy;
    for (const auto& p : points_) {
        xy.push_back(p.x);
        xy.push_back(p.y);
    }
    std::deque<double> radii;
    for (const auto& c : circles_) radii.push_back(c.radius);
    std::deque<double> constants;            // значения размеров и закреплений (не неизвестные)

    GCS::System sys;
    std::vector<GCS::Point> gp(n);
    for (std::size_t i = 0; i < n; ++i) {
        gp[i].x = &xy[2 * i];
        gp[i].y = &xy[2 * i + 1];
    }
    std::vector<GCS::Line> gl(lines_.size());
    for (std::size_t k = 0; k < lines_.size(); ++k) {
        gl[k].p1 = gp[lines_[k].a];
        gl[k].p2 = gp[lines_[k].b];
    }
    std::vector<GCS::Circle> gc(circles_.size());
    for (std::size_t k = 0; k < circles_.size(); ++k) {
        gc[k].center = gp[circles_[k].center];
        gc[k].rad = &radii[k];
    }

    GCS::VEC_pD params;
    for (auto& v : xy) params.push_back(&v);
    for (auto& r : radii) params.push_back(&r);
    sys.declareUnknowns(params);

    for (std::size_t i = 0; i < n; ++i) {
        if (!fixed_[i]) continue;
        constants.push_back(pins_[i].x);
        sys.addConstraintCoordinateX(gp[i], &constants.back(), 0, true);
        constants.push_back(pins_[i].y);
        sys.addConstraintCoordinateY(gp[i], &constants.back(), 0, true);
    }

    for (std::size_t j = 0; j < constraints_.size(); ++j) {
        const auto& c = constraints_[j];
        if (c.removed) continue;
        const int tag = tag_of(j);
        if (c.kind == "coincident") {
            sys.addConstraintP2PCoincident(gp[c.a], gp[c.b], tag, true);
        } else if (c.kind == "horizontal") {
            sys.addConstraintHorizontal(gp[c.a], gp[c.b], tag, true);
        } else if (c.kind == "vertical") {
            sys.addConstraintVertical(gp[c.a], gp[c.b], tag, true);
        } else if (c.kind == "distance") {
            constants.push_back(c.value);
            sys.addConstraintP2PDistance(gp[c.a], gp[c.b], &constants.back(), tag, true);
        } else if (c.kind == "radius") {
            constants.push_back(c.value);
            sys.addConstraintCircleRadius(gc[c.a], &constants.back(), tag, true);
        } else if (c.kind == "parallel") {
            sys.addConstraintParallel(gl[c.a], gl[c.b], tag, true);
        } else if (c.kind == "perpendicular") {
            sys.addConstraintPerpendicular(gl[c.a], gl[c.b], tag, true);
        } else if (c.kind == "equal_length") {
            sys.addConstraintEqualLength(gl[c.a], gl[c.b], tag, true);
        } else if (c.kind == "angle") {
            constants.push_back(c.value * std::numbers::pi / 180.0);   // PlaneGCS работает в радианах
            sys.addConstraintL2LAngle(gl[c.a], gl[c.b], &constants.back(), tag, true);
        } else if (c.kind == "point_on_line") {
            sys.addConstraintPointOnLine(gp[c.a], gl[c.b], tag, true);
        } else if (c.kind == "point_on_circle") {
            sys.addConstraintPointOnCircle(gp[c.a], gc[c.b], tag, true);
        } else if (c.kind == "equal_radius") {
            sys.addConstraintEqualRadius(gc[c.a], gc[c.b], tag, true);
        } else if (c.kind == "symmetric") {
            sys.addConstraintP2PSymmetric(gp[c.a], gp[c.b], gl[c.c], tag, true);
        } else {
            throw std::logic_error("неизвестный вид ограничения: " + c.kind);
        }
    }

    sys.initSolution();
    const int dofs = sys.diagnose();
    GCS::VEC_I conflictTags, redundantTags;
    sys.getConflicting(conflictTags);
    sys.getRedundant(redundantTags);

    Report report;
    report.dof = dofs;
    for (int t : conflictTags) {
        if (t > 0) report.conflicting.push_back(t);
    }
    for (int t : redundantTags) {
        if (t > 0) report.redundant.push_back(t);
    }

    const GCS::SolveStatus st = sys.solve();
    if (st == GCS::SolveStatus::Success || st == GCS::SolveStatus::Converged) {
        sys.applySolution();
        for (std::size_t i = 0; i < n; ++i) points_[i] = {xy[2 * i], xy[2 * i + 1]};
        for (std::size_t k = 0; k < circles_.size(); ++k) circles_[k].radius = radii[k];
    }

    if (!report.conflicting.empty()) {
        report.status = "конфликт";
    } else if (!report.redundant.empty()) {
        report.status = "избыточно";
    } else if (dofs == 0) {
        report.status = "определено";
    } else {
        report.status = "недоопределено";
    }
    return report;
}

Point2 Sketch::point(Id p) const {
    return points_.at(p);
}

double Sketch::circle_radius(Id c) const {
    return circles_.at(c).radius;
}

}  // namespace bendcore::sketch2d
