#include "sketch_internal.hpp"

#include <cmath>
#include <cstdio>
#include <numbers>
#include <stdexcept>

namespace bendcore::modeling {

namespace {

using sketch2d::GeoKind;

double distance(const sketch2d::Point2& a, const sketch2d::Point2& b) {
    return std::hypot(b.x - a.x, b.y - a.y);
}

Point2 toPoint(const sketch2d::Point2& p) { return Point2{p.x, p.y}; }

Result<SketchCurve> offCurve(SketchEntityId id, const char* what, sketch2d::Id p, double off) {
    char buffer[220];
    std::snprintf(buffer, sizeof(buffer), "%s entity %u: point %d is %.3g mm off its curve; solve the sketch first",
                  what, static_cast<unsigned>(id), p, off);
    return Result<SketchCurve>::failure(ErrorCode::InvalidInput, buffer);
}

}  // namespace

SketchEntityId BoundSketch::push(Entity e) {
    e.id = static_cast<SketchEntityId>(entities_.size() + 1);
    sketch_.set_construction(e.geo, e.construction);
    entities_.push_back(e);
    return e.id;
}

SketchEntityId BoundSketch::addLine(sketch2d::Id start, sketch2d::Id end, bool construction) {
    Entity e;
    e.line = sketch_.add_line(start, end);
    e.geo = sketch2d::line_geo(e.line);
    e.kind = EntityKind::Line;
    e.construction = construction;
    e.start = start;
    e.end = end;
    return push(e);
}

SketchEntityId BoundSketch::addCircle(sketch2d::Id center, double radius, bool construction) {
    Entity e;
    e.circle = sketch_.add_circle(center, radius);
    e.geo = sketch2d::circle_geo(e.circle);
    e.kind = EntityKind::Circle;
    e.construction = construction;
    e.center = center;
    return push(e);
}

SketchEntityId BoundSketch::addArc(sketch2d::Id center, sketch2d::Id start, sketch2d::Id end,
                                   bool counterClockwise, bool construction) {
    Entity e;
    e.arc = sketch_.add_arc(center, start, end, counterClockwise);
    e.geo = sketch2d::arc_geo(e.arc);
    e.kind = EntityKind::Arc;
    e.construction = construction;
    e.center = center;
    e.start = start;
    e.end = end;
    e.counterClockwise = counterClockwise;
    return push(e);
}

SketchEntityId BoundSketch::addEllipse(sketch2d::Id center, sketch2d::Id focus, double minorRadius,
                                       bool construction) {
    Entity e;
    e.geo = sketch2d::ellipse_geo(sketch_.add_ellipse(center, focus, minorRadius));
    e.kind = EntityKind::Ellipse;
    e.construction = construction;
    e.center = center;
    e.focus = focus;
    return push(e);
}

SketchEntityId BoundSketch::addEllipticalArc(sketch2d::Id center, sketch2d::Id focus, double minorRadius,
                                             sketch2d::Id start, sketch2d::Id end, bool counterClockwise,
                                             bool construction) {
    Entity e;
    e.geo = sketch2d::ellipse_arc_geo(sketch_.add_ellipse_arc(center, focus, minorRadius, start, end, counterClockwise));
    e.kind = EntityKind::EllipticalArc;
    e.construction = construction;
    e.center = center;
    e.focus = focus;
    e.start = start;
    e.end = end;
    e.counterClockwise = counterClockwise;
    return push(e);
}

SketchEntityId BoundSketch::addBSpline(const sketch2d::BSplineData& data, bool construction) {
    Entity e;
    e.geo = sketch2d::bspline_geo(sketch_.add_bspline(data));
    e.kind = EntityKind::BSpline;
    e.construction = construction;
    if (!data.periodic) {
        e.start = data.poles.front();
        e.end = data.poles.back();
    }
    return push(e);
}

const BoundSketch::Entity& BoundSketch::entity(SketchEntityId id) const {
    if (id == 0 || id > entities_.size()) {
        throw std::out_of_range("no sketch entity with id " + std::to_string(id));
    }
    return entities_[id - 1];
}

std::optional<SketchEntityId> BoundSketch::entityOf(const sketch2d::Geo& geo) const {
    for (const Entity& e : entities_) {
        if (e.geo == geo) return e.id;
    }
    return std::nullopt;
}

void BoundSketch::setConstruction(SketchEntityId id, bool construction) {
    entity(id);
    entities_[id - 1].construction = construction;
    sketch_.set_construction(entities_[id - 1].geo, construction);
}

void BoundSketch::removeEntity(SketchEntityId id) {
    entity(id);
    Entity& e = entities_[id - 1];
    if (e.removed) return;
    sketch_.remove(e.geo);
    e.removed = true;
}

void BoundSketch::setEndPoints(SketchEntityId id, sketch2d::Id start, sketch2d::Id end) {
    entity(id);
    Entity& e = entities_[id - 1];
    if (e.kind == EntityKind::Line) {
        sketch_.set_line_points(e.line, start, end);
    } else if (e.kind == EntityKind::Arc) {
        sketch_.set_arc_points(e.arc, start, end);
    } else {
        throw std::invalid_argument("end points can be replaced only for lines and arcs");
    }
    e.start = start;
    e.end = end;
}

Result<SketchCurve> BoundSketch::curve(SketchEntityId id, double tolerance) const {
    const Entity& e = entity(id);
    if (e.removed) {
        return Result<SketchCurve>::failure(ErrorCode::InvalidInput,
                                            "sketch entity " + std::to_string(id) + " is removed");
    }
    SketchCurve curve;
    curve.id = e.id;
    curve.construction = sketch_.is_construction(e.geo);
    switch (e.kind) {
        case EntityKind::Line: {
            const auto l = sketch_.line(e.line);
            curve.geometry = LineSegment2d{toPoint(sketch_.point(l.start)), toPoint(sketch_.point(l.end))};
            break;
        }
        case EntityKind::Circle: {
            const auto c = sketch_.circle(e.circle);
            curve.geometry = Circle2d{toPoint(sketch_.point(c.center)), c.radius};
            break;
        }
        case EntityKind::Arc: {
            const auto a = sketch_.arc(e.arc);
            const auto c = sketch_.point(a.center);
            const double radius = distance(c, sketch_.point(a.start));
            const double off = std::abs(distance(c, sketch_.point(a.end)) - radius);
            const double offR = std::abs(radius - a.radius);
            if (off > tolerance || offR > tolerance) {
                return offCurve(e.id, "arc", off > tolerance ? a.end : a.start, std::max(off, offR));
            }
            curve.geometry = CircularArc2d{toPoint(c), toPoint(sketch_.point(a.start)), toPoint(sketch_.point(a.end)), a.ccw};
            break;
        }
        case EntityKind::Ellipse:
        case EntityKind::EllipticalArc: {
            Ellipse2d el;
            const sketch2d::Id centerId =
                e.kind == EntityKind::Ellipse ? sketch_.ellipse(e.geo.id).center : sketch_.ellipse_arc(e.geo.id).center;
            el.center = toPoint(sketch_.point(centerId));
            el.majorDirection = toPoint(sketch_.ellipse_major_direction(e.geo));
            el.majorRadius = sketch_.ellipse_major_radius(e.geo);
            el.minorRadius = e.kind == EntityKind::Ellipse ? sketch_.ellipse(e.geo.id).minor_radius
                                                           : sketch_.ellipse_arc(e.geo.id).minor_radius;
            if (e.kind == EntityKind::Ellipse) {
                curve.geometry = el;
                break;
            }
            const auto a = sketch_.ellipse_arc(e.geo.id);
            // Distance from the ellipse: |PF1| + |PF2| − 2a, halved.
            const auto f1 = sketch_.point(a.focus);
            const sketch2d::Point2 f2{2 * el.center.x - f1.x, 2 * el.center.y - f1.y};
            for (sketch2d::Id p : {a.start, a.end}) {
                const auto q = sketch_.point(p);
                const double off = std::abs(distance(q, f1) + distance(q, f2) - 2 * el.majorRadius) / 2;
                if (off > tolerance) return offCurve(e.id, "elliptical arc", p, off);
            }
            const double us = sketch_.ellipse_parameter(e.geo, sketch_.point(a.start));
            const double ue = sketch_.ellipse_parameter(e.geo, sketch_.point(a.end));
            double t0 = a.ccw ? us : ue;
            double t1 = a.ccw ? ue : us;
            while (t1 <= t0) t1 += 2 * std::numbers::pi;
            curve.geometry = EllipticalArc2d{el, t0, t1};
            break;
        }
        case EntityKind::BSpline: {
            const auto& d = sketch_.bspline(e.geo.id);
            BSpline2d s;
            s.degree = d.degree;
            for (sketch2d::Id p : d.poles) s.poles.push_back(toPoint(sketch_.point(p)));
            s.weights = d.weights;
            s.knots = d.knots;
            s.multiplicities = d.multiplicities;
            s.periodic = d.periodic;
            curve.geometry = std::move(s);
            break;
        }
    }
    return curve;
}

Result<std::vector<SketchCurve>> BoundSketch::curves(double tolerance) const {
    std::vector<SketchCurve> out;
    out.reserve(entities_.size());
    for (const Entity& e : entities_) {
        if (e.removed) continue;
        auto c = curve(e.id, tolerance);
        if (!c) return c.error();
        out.push_back(std::move(c).value());
    }
    return out;
}

}  // namespace bendcore::modeling
