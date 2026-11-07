#include "sketch_internal.hpp"

#include <cmath>
#include <cstdio>
#include <stdexcept>

namespace bendcore::modeling {

namespace {

double distance(const sketch2d::Point2& a, const sketch2d::Point2& b) {
    return std::hypot(b.x - a.x, b.y - a.y);
}

Point2 toPoint(const sketch2d::Point2& p) { return Point2{p.x, p.y}; }

}  // namespace

SketchEntityId BoundSketch::addLine(sketch2d::Id start, sketch2d::Id end, bool construction) {
    Entity e;
    e.line = sketch_.add_line(start, end);
    e.id = static_cast<SketchEntityId>(entities_.size() + 1);
    e.kind = EntityKind::Line;
    e.construction = construction;
    e.start = start;
    e.end = end;
    entities_.push_back(e);
    return e.id;
}

SketchEntityId BoundSketch::addCircle(sketch2d::Id center, double radius, bool construction) {
    Entity e;
    e.circle = sketch_.add_circle(center, radius);
    e.id = static_cast<SketchEntityId>(entities_.size() + 1);
    e.kind = EntityKind::Circle;
    e.construction = construction;
    e.center = center;
    entities_.push_back(e);
    return e.id;
}

SketchEntityId BoundSketch::addArc(sketch2d::Id center, sketch2d::Id start, sketch2d::Id end,
                                   bool counterClockwise, bool construction) {
    const double radius = distance(sketch_.point(center), sketch_.point(start));
    Entity e;
    e.circle = sketch_.add_circle(center, radius);
    e.startOnCircle = sketch_.constrain_point_on_circle(start, e.circle);
    e.endOnCircle = sketch_.constrain_point_on_circle(end, e.circle);
    e.id = static_cast<SketchEntityId>(entities_.size() + 1);
    e.kind = EntityKind::Arc;
    e.construction = construction;
    e.center = center;
    e.start = start;
    e.end = end;
    e.counterClockwise = counterClockwise;
    entities_.push_back(e);
    return e.id;
}

const BoundSketch::Entity& BoundSketch::entity(SketchEntityId id) const {
    if (id == 0 || id > entities_.size()) {
        throw std::out_of_range("no sketch entity with id " + std::to_string(id));
    }
    return entities_[id - 1];
}

void BoundSketch::setConstruction(SketchEntityId id, bool construction) {
    entity(id);
    entities_[id - 1].construction = construction;
}

Result<std::vector<SketchCurve>> BoundSketch::curves(double tolerance) const {
    std::vector<SketchCurve> out;
    out.reserve(entities_.size());
    for (const Entity& e : entities_) {
        SketchCurve curve;
        curve.id = e.id;
        curve.construction = e.construction;
        switch (e.kind) {
            case EntityKind::Line:
                curve.geometry =
                    LineSegment2d{toPoint(sketch_.point(e.start)), toPoint(sketch_.point(e.end))};
                break;
            case EntityKind::Circle:
                curve.geometry =
                    Circle2d{toPoint(sketch_.point(e.center)), sketch_.circle_radius(e.circle)};
                break;
            case EntityKind::Arc: {
                const double radius = sketch_.circle_radius(e.circle);
                const auto c = sketch_.point(e.center);
                for (sketch2d::Id p : {e.start, e.end}) {
                    const double off = std::abs(distance(c, sketch_.point(p)) - radius);
                    if (off > tolerance) {
                        char buffer[200];
                        std::snprintf(buffer, sizeof(buffer),
                                      "arc entity %u: point %d is %.3g mm off its circle; "
                                      "solve the sketch first",
                                      static_cast<unsigned>(e.id), p, off);
                        return Result<std::vector<SketchCurve>>::failure(ErrorCode::InvalidInput,
                                                                         buffer);
                    }
                }
                curve.geometry = CircularArc2d{toPoint(c), toPoint(sketch_.point(e.start)),
                                               toPoint(sketch_.point(e.end)), e.counterClockwise};
                break;
            }
        }
        out.push_back(std::move(curve));
    }
    return out;
}

}  // namespace bendcore::modeling
