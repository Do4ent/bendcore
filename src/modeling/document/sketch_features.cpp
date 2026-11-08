// Sketch, datum plane and datum axis nodes of the part document
// (docs/modeling/PART_DOCUMENT.md, sections "Эскиз" and "Вспомогательная геометрия").

#include "part_features_internal.hpp"

#include <bendcore/modeling/reference_geometry.hpp>

#include <BRep_Tool.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedDataMapOfShapeListOfShape.hxx>
#include <TopoDS.hxx>

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <sstream>

namespace bendcore::modeling {

// ---------------------------------------------------------------- sketch data

const char* toString(SketchGeometryType type) noexcept {
    switch (type) {
        case SketchGeometryType::Line: return "line";
        case SketchGeometryType::Circle: return "circle";
        case SketchGeometryType::Arc: return "arc";
    }
    return "unknown";
}

std::optional<SketchGeometryType> sketchGeometryTypeFromString(std::string_view text) noexcept {
    for (auto t : {SketchGeometryType::Line, SketchGeometryType::Circle, SketchGeometryType::Arc}) {
        if (text == toString(t)) return t;
    }
    return std::nullopt;
}

namespace {

constexpr SketchConstraintType kConstraintTypes[] = {
    SketchConstraintType::Coincident,    SketchConstraintType::Horizontal,  SketchConstraintType::Vertical,
    SketchConstraintType::Distance,      SketchConstraintType::Radius,      SketchConstraintType::Parallel,
    SketchConstraintType::Perpendicular, SketchConstraintType::EqualLength, SketchConstraintType::Angle,
    SketchConstraintType::PointOnLine,   SketchConstraintType::PointOnCircle, SketchConstraintType::EqualRadius,
    SketchConstraintType::Symmetric,
};

}  // namespace

const char* toString(SketchConstraintType type) noexcept {
    switch (type) {
        case SketchConstraintType::Coincident: return "coincident";
        case SketchConstraintType::Horizontal: return "horizontal";
        case SketchConstraintType::Vertical: return "vertical";
        case SketchConstraintType::Distance: return "distance";
        case SketchConstraintType::Radius: return "radius";
        case SketchConstraintType::Parallel: return "parallel";
        case SketchConstraintType::Perpendicular: return "perpendicular";
        case SketchConstraintType::EqualLength: return "equal-length";
        case SketchConstraintType::Angle: return "angle";
        case SketchConstraintType::PointOnLine: return "point-on-line";
        case SketchConstraintType::PointOnCircle: return "point-on-circle";
        case SketchConstraintType::EqualRadius: return "equal-radius";
        case SketchConstraintType::Symmetric: return "symmetric";
    }
    return "unknown";
}

std::optional<SketchConstraintType> sketchConstraintTypeFromString(std::string_view text) noexcept {
    for (auto t : kConstraintTypes) {
        if (text == toString(t)) return t;
    }
    return std::nullopt;
}

std::vector<const std::string*> sketchExpressions(const SketchData& data) {
    std::vector<const std::string*> out;
    for (const auto& p : data.points) {
        out.push_back(&p.x);
        out.push_back(&p.y);
    }
    for (const auto& g : data.geometry) out.push_back(&g.radius);
    for (const auto& c : data.constraints) out.push_back(&c.value);
    return out;
}

int SketchBuilder::point(std::string x, std::string y, bool fixed) {
    data_.points.push_back({std::move(x), std::move(y), fixed});
    return static_cast<int>(data_.points.size()) - 1;
}

SketchEntityId SketchBuilder::line(int start, int end, bool construction) {
    SketchGeometryDef g;
    g.type = SketchGeometryType::Line;
    g.start = start;
    g.end = end;
    g.construction = construction;
    data_.geometry.push_back(g);
    return static_cast<SketchEntityId>(data_.geometry.size());
}

SketchEntityId SketchBuilder::circle(int center, std::string radius, bool construction) {
    SketchGeometryDef g;
    g.type = SketchGeometryType::Circle;
    g.center = center;
    g.radius = std::move(radius);
    g.construction = construction;
    data_.geometry.push_back(g);
    return static_cast<SketchEntityId>(data_.geometry.size());
}

SketchEntityId SketchBuilder::arc(int center, int start, int end, bool counterClockwise, bool construction) {
    SketchGeometryDef g;
    g.type = SketchGeometryType::Arc;
    g.center = center;
    g.start = start;
    g.end = end;
    g.counterClockwise = counterClockwise;
    g.construction = construction;
    data_.geometry.push_back(g);
    return static_cast<SketchEntityId>(data_.geometry.size());
}

SketchEntityId SketchBuilder::projected(SketchGeometryType type, int referenceIndex, bool construction) {
    SketchGeometryDef g;
    g.type = type;
    g.projection = referenceIndex;
    g.construction = construction;
    switch (type) {
        case SketchGeometryType::Line:
            g.start = point("0", "0");
            g.end = point("1", "0");
            break;
        case SketchGeometryType::Circle: g.center = point("0", "0"); break;
        case SketchGeometryType::Arc:
            g.center = point("0", "0");
            g.start = point("1", "0");
            g.end = point("0", "1");
            break;
    }
    data_.geometry.push_back(g);
    return static_cast<SketchEntityId>(data_.geometry.size());
}

int SketchBuilder::constrain(SketchConstraintType type, int a, int b, std::string value, int c) {
    data_.constraints.push_back({type, a, b, c, std::move(value)});
    return static_cast<int>(data_.constraints.size());
}

std::vector<SketchEntityId> SketchBuilder::rectangle(std::string x, std::string y, std::string w, std::string h) {
    // Initial guesses for the free corners: the corner plus 1 mm; the solver moves them.
    const int p0 = point(x, y, true);
    const int p1 = point("(" + x + ") + 1", y);
    const int p2 = point("(" + x + ") + 1", "(" + y + ") + 1");
    const int p3 = point(x, "(" + y + ") + 1");
    std::vector<SketchEntityId> e{line(p0, p1), line(p1, p2), line(p2, p3), line(p3, p0)};
    constrain(SketchConstraintType::Horizontal, p0, p1);
    constrain(SketchConstraintType::Vertical, p1, p2);
    constrain(SketchConstraintType::Horizontal, p2, p3);
    constrain(SketchConstraintType::Vertical, p3, p0);
    constrain(SketchConstraintType::Distance, p0, p1, std::move(w));
    constrain(SketchConstraintType::Distance, p1, p2, std::move(h));
    return e;
}

SketchEntityId SketchBuilder::fixedCircle(std::string cx, std::string cy, std::string r) {
    const int c = point(std::move(cx), std::move(cy), true);
    const SketchEntityId e = circle(c, "1");
    constrain(SketchConstraintType::Radius, static_cast<int>(e), -1, std::move(r));
    return e;
}

std::vector<SketchEntityId> SketchBuilder::fixedPolygon(const std::vector<std::pair<std::string, std::string>>& corners) {
    std::vector<int> pts;
    for (const auto& [x, y] : corners) pts.push_back(point(x, y, true));
    std::vector<SketchEntityId> out;
    for (std::size_t i = 0; i < pts.size(); ++i) out.push_back(line(pts[i], pts[(i + 1) % pts.size()]));
    return out;
}

// ---------------------------------------------------------------- sketch feature

namespace features {

namespace {

using detail::fail;
using detail::num;

double wrapAngle(double a) {
    while (a > M_PI) a -= 2 * M_PI;
    while (a < -M_PI) a += 2 * M_PI;
    return a;
}

// Residual of a constraint at the solved positions (mm or rad).
double residual(const SketchConstraintDef& c, double value, const BoundSketch& bs) {
    const auto& s = bs.sketch();
    auto P = [&](int p) { return s.point(p); };
    auto dir = [&](int e) {
        const auto& ent = bs.entity(static_cast<SketchEntityId>(e));
        const auto a = s.point(ent.start), b = s.point(ent.end);
        return std::pair<double, double>{b.x - a.x, b.y - a.y};
    };
    auto radius = [&](int e) { return s.circle_radius(bs.entity(static_cast<SketchEntityId>(e)).circle); };
    auto center = [&](int e) { return s.point(bs.entity(static_cast<SketchEntityId>(e)).center); };
    auto lineDistance = [&](const sketch2d::Point2& p, int e) {
        const auto& ent = bs.entity(static_cast<SketchEntityId>(e));
        const auto a = s.point(ent.start);
        const auto [dx, dy] = dir(e);
        const double len = std::hypot(dx, dy);
        return len > 0 ? std::abs((p.x - a.x) * dy - (p.y - a.y) * dx) / len : std::hypot(p.x - a.x, p.y - a.y);
    };
    switch (c.type) {
        case SketchConstraintType::Coincident: return std::hypot(P(c.a).x - P(c.b).x, P(c.a).y - P(c.b).y);
        case SketchConstraintType::Horizontal: return std::abs(P(c.a).y - P(c.b).y);
        case SketchConstraintType::Vertical: return std::abs(P(c.a).x - P(c.b).x);
        case SketchConstraintType::Distance:
            return std::abs(std::hypot(P(c.a).x - P(c.b).x, P(c.a).y - P(c.b).y) - value);
        case SketchConstraintType::Radius: return std::abs(radius(c.a) - value);
        case SketchConstraintType::Parallel:
        case SketchConstraintType::Perpendicular: {
            const auto [ax, ay] = dir(c.a);
            const auto [bx, by] = dir(c.b);
            const double la = std::hypot(ax, ay), lb = std::hypot(bx, by);
            if (la == 0 || lb == 0) return 1.0;
            const double v = c.type == SketchConstraintType::Parallel ? (ax * by - ay * bx) : (ax * bx + ay * by);
            return std::abs(v) / (la * lb);
        }
        case SketchConstraintType::EqualLength: {
            const auto [ax, ay] = dir(c.a);
            const auto [bx, by] = dir(c.b);
            return std::abs(std::hypot(ax, ay) - std::hypot(bx, by));
        }
        case SketchConstraintType::Angle: {
            const auto [ax, ay] = dir(c.a);
            const auto [bx, by] = dir(c.b);
            return std::abs(wrapAngle(std::atan2(by, bx) - std::atan2(ay, ax) - value * M_PI / 180.0));
        }
        case SketchConstraintType::PointOnLine: return lineDistance(P(c.a), c.b);
        case SketchConstraintType::PointOnCircle: {
            const auto o = center(c.b);
            return std::abs(std::hypot(P(c.a).x - o.x, P(c.a).y - o.y) - radius(c.b));
        }
        case SketchConstraintType::EqualRadius: return std::abs(radius(c.a) - radius(c.b));
        case SketchConstraintType::Symmetric: {
            const sketch2d::Point2 mid{(P(c.a).x + P(c.b).x) / 2, (P(c.a).y + P(c.b).y) / 2};
            const auto [dx, dy] = dir(c.c);
            const double len = std::hypot(dx, dy);
            const double along = len > 0 ? std::abs((P(c.b).x - P(c.a).x) * dx + (P(c.b).y - P(c.a).y) * dy) / len : 0;
            return std::max(lineDistance(mid, c.c), along);
        }
    }
    return 0.0;
}

gp_Pln basePlane(const std::string& name) {
    if (name == "XZ") return gp_Pln(gp_Ax3(gp::Origin(), -gp::DY(), gp::DX()));
    if (name == "YZ") return gp_Pln(gp_Ax3(gp::Origin(), gp::DX(), gp::DY()));
    return gp_Pln(gp_Ax3(gp::Origin(), gp::DZ(), gp::DX()));
}

}  // namespace

Result<gp_Pln> planeFromOption(const FeatureContext& ctx, const std::string& choice, const char* faceSlot,
                               const char* datumLink) {
    if (choice == "XY" || choice == "XZ" || choice == "YZ") return basePlane(choice);
    if (choice == "face") {
        const auto& faces = ctx.references(faceSlot);
        if (faces.size() != 1) {
            return Error{ErrorCode::InvalidInput, std::string("reference '") + faceSlot + "' must give one planar face, has " +
                                                      std::to_string(faces.size())};
        }
        return planeOfFace(TopoDS::Face(faces.front()));
    }
    if (choice == "datum") {
        const LinkedFeature* l = ctx.link(datumLink);
        if (!l || !l->datum || !l->datum->plane) {
            return Error{ErrorCode::InvalidInput, std::string("link '") + datumLink + "' must name a datum plane"};
        }
        return *l->datum->plane;
    }
    return Error{ErrorCode::InvalidInput, "unknown plane choice '" + choice + "'"};
}

Result<gp_Ax1> axisFromOption(const FeatureContext& ctx, const std::string& choice, const char* faceSlot,
                              const char* edgeSlot, const char* datumLink) {
    if (choice == "X") return gp_Ax1(gp::Origin(), gp::DX());
    if (choice == "Y") return gp_Ax1(gp::Origin(), gp::DY());
    if (choice == "Z") return gp_Ax1(gp::Origin(), gp::DZ());
    if (choice == "face") {
        const auto& faces = ctx.references(faceSlot);
        if (faces.size() != 1) return Error{ErrorCode::InvalidInput, std::string("reference '") + faceSlot + "' must give one face"};
        return axisOfFace(TopoDS::Face(faces.front()));
    }
    if (choice == "edge") {
        const auto& edges = ctx.references(edgeSlot);
        if (edges.size() != 1) return Error{ErrorCode::InvalidInput, std::string("reference '") + edgeSlot + "' must give one edge"};
        return axisOfEdge(TopoDS::Edge(edges.front()));
    }
    if (choice == "datum") {
        const LinkedFeature* l = ctx.link(datumLink);
        if (!l || !l->datum || !l->datum->axis) {
            return Error{ErrorCode::InvalidInput, std::string("link '") + datumLink + "' must name a datum axis"};
        }
        return *l->datum->axis;
    }
    return Error{ErrorCode::InvalidInput, "unknown axis choice '" + choice + "'"};
}

namespace {

class SketchFeature final : public Feature {
public:
    std::string type() const override { return "Sketch"; }
    std::vector<ParameterSpec> parameterSpecs() const override {
        return {{"offset", "0"}, {"originX", "0"}, {"originY", "0"}, {"originZ", "0"}};
    }
    std::vector<ReferenceSlot> referenceSlots() const override {
        return {{"face", ElementKind::Face, false, true}, {"projected", ElementKind::Edge, true, true}};
    }
    std::vector<OptionSpec> optionSpecs() const override { return {{"plane", {"XY", "XZ", "YZ", "face", "datum"}}}; }
    std::vector<LinkSlot> linkSlots() const override { return {{"plane", {"DatumPlane"}, true}}; }
    bool hasSketchData() const override { return true; }

    Result<FeatureOutput> execute(const FeatureContext& ctx) const override {
        const SketchData* data = ctx.sketch();
        if (!data) return fail(ErrorCode::InvalidInput, "sketch node without sketch data");
        auto placement = place(ctx);
        if (!placement) return placement.error();
        const SketchPlacement& pl = placement.value();

        const std::vector<double>& v = ctx.sketchValues();
        const std::size_t np = data->points.size(), ng = data->geometry.size();
        if (v.size() != 2 * np + ng + data->constraints.size()) {
            return fail(ErrorCode::InvalidInput, "sketch values do not match the sketch data");
        }

        // Initial point positions; projected curves pin their points.
        std::vector<Point2> pos(np);
        std::vector<bool> fixed(np);
        for (std::size_t k = 0; k < np; ++k) {
            pos[k] = {v[2 * k], v[2 * k + 1]};
            fixed[k] = data->points[k].fixed;
        }
        std::vector<double> radii(ng);
        std::vector<std::optional<double>> projectedRadius(ng);
        for (std::size_t g = 0; g < ng; ++g) radii[g] = v[2 * np + g];
        const auto& projected = ctx.references("projected");
        for (std::size_t g = 0; g < ng; ++g) {
            const SketchGeometryDef& def = data->geometry[g];
            if (def.projection < 0) continue;
            const std::string where = "sketch entity " + std::to_string(g + 1);
            if (static_cast<std::size_t>(def.projection) >= projected.size()) {
                return fail(ErrorCode::InvalidInput, where + ": projected edge #" + std::to_string(def.projection) +
                                                         " is not among the " + std::to_string(projected.size()) +
                                                         " resolved 'projected' references");
            }
            auto pe = projectEdge(pl, TopoDS::Edge(projected[static_cast<std::size_t>(def.projection)]));
            if (!pe) return fail(pe.error().code, where + ": " + pe.error().message);
            if (!pe.value().curve) return fail(ErrorCode::InvalidInput, where + ": the edge projects to a point");
            const Curve2d& c = *pe.value().curve;
            auto pin = [&](int p, Point2 at) {
                pos[static_cast<std::size_t>(p)] = at;
                fixed[static_cast<std::size_t>(p)] = true;
            };
            if (def.type == SketchGeometryType::Line && std::holds_alternative<LineSegment2d>(c)) {
                LineSegment2d l = std::get<LineSegment2d>(c);
                if (l.end.x < l.start.x - 1e-9 || (std::abs(l.end.x - l.start.x) <= 1e-9 && l.end.y < l.start.y)) {
                    std::swap(l.start, l.end);
                }
                pin(def.start, l.start);
                pin(def.end, l.end);
            } else if (def.type == SketchGeometryType::Circle && std::holds_alternative<Circle2d>(c)) {
                const Circle2d& ci = std::get<Circle2d>(c);
                pin(def.center, ci.center);
                radii[g] = ci.radius;
                projectedRadius[g] = ci.radius;
            } else if (def.type == SketchGeometryType::Arc && std::holds_alternative<CircularArc2d>(c)) {
                const CircularArc2d& a = std::get<CircularArc2d>(c);
                pin(def.center, a.center);
                pin(def.start, a.counterClockwise ? a.start : a.end);
                pin(def.end, a.counterClockwise ? a.end : a.start);
            } else {
                return fail(ErrorCode::InvalidInput, where + ": projected edge is not a " + toString(def.type));
            }
        }

        BoundSketch bs;
        for (std::size_t k = 0; k < np; ++k) {
            bs.sketch().add_point(pos[k].x, pos[k].y);
            if (fixed[k]) bs.sketch().fix_point(static_cast<sketch2d::Id>(k));
        }
        for (std::size_t g = 0; g < ng; ++g) {
            const SketchGeometryDef& def = data->geometry[g];
            switch (def.type) {
                case SketchGeometryType::Line: bs.addLine(def.start, def.end, def.construction); break;
                case SketchGeometryType::Circle:
                    if (!(radii[g] > 0)) {
                        return fail(ErrorCode::InvalidInput,
                                    "sketch entity " + std::to_string(g + 1) + ": radius must be positive, is " + num(radii[g]));
                    }
                    bs.addCircle(def.center, radii[g], def.construction);
                    break;
                case SketchGeometryType::Arc:
                    bs.addArc(def.center, def.start, def.end, def.projection >= 0 || def.counterClockwise, def.construction);
                    break;
            }
        }
        for (std::size_t g = 0; g < ng; ++g) {
            if (projectedRadius[g]) {
                bs.sketch().constrain_radius(bs.entity(static_cast<SketchEntityId>(g + 1)).circle, *projectedRadius[g]);
            }
        }
        const std::size_t c0 = 2 * np + ng;
        std::vector<int> tags;
        for (std::size_t k = 0; k < data->constraints.size(); ++k) {
            const SketchConstraintDef& c = data->constraints[k];
            const double val = v[c0 + k];
            auto ent = [&](int e) -> const BoundSketch::Entity& { return bs.entity(static_cast<SketchEntityId>(e)); };
            auto needs = [&](int e, BoundSketch::EntityKind kind1, std::optional<BoundSketch::EntityKind> kind2) -> Status {
                const auto k1 = ent(e).kind;
                if (k1 == kind1 || (kind2 && k1 == *kind2)) return std::monostate{};
                return Status::failure(ErrorCode::InvalidInput, "sketch constraint " + std::to_string(k + 1) + " (" +
                                                                    toString(c.type) + "): entity " + std::to_string(e) +
                                                                    " has the wrong kind");
            };
            using K = BoundSketch::EntityKind;
            Status st = std::monostate{};
            sketch2d::Sketch& s = bs.sketch();
            int tag = 0;
            switch (c.type) {
                case SketchConstraintType::Coincident: tag = s.constrain_coincident(c.a, c.b); break;
                case SketchConstraintType::Horizontal: tag = s.constrain_horizontal(c.a, c.b); break;
                case SketchConstraintType::Vertical: tag = s.constrain_vertical(c.a, c.b); break;
                case SketchConstraintType::Distance:
                    if (!(val > 0)) return fail(ErrorCode::InvalidInput, "sketch constraint " + std::to_string(k + 1) +
                                                                             ": distance must be positive, is " + num(val));
                    tag = s.constrain_distance(c.a, c.b, val);
                    break;
                case SketchConstraintType::Radius:
                    if (!(st = needs(c.a, K::Circle, K::Arc))) return st.error();
                    if (!(val > 0)) return fail(ErrorCode::InvalidInput, "sketch constraint " + std::to_string(k + 1) +
                                                                             ": radius must be positive, is " + num(val));
                    tag = s.constrain_radius(ent(c.a).circle, val);
                    break;
                case SketchConstraintType::Parallel:
                case SketchConstraintType::Perpendicular:
                case SketchConstraintType::EqualLength:
                case SketchConstraintType::Angle:
                    if (!(st = needs(c.a, K::Line, std::nullopt)) || !(st = needs(c.b, K::Line, std::nullopt))) return st.error();
                    if (c.type == SketchConstraintType::Parallel) tag = s.constrain_parallel(ent(c.a).line, ent(c.b).line);
                    if (c.type == SketchConstraintType::Perpendicular) tag = s.constrain_perpendicular(ent(c.a).line, ent(c.b).line);
                    if (c.type == SketchConstraintType::EqualLength) tag = s.constrain_equal_length(ent(c.a).line, ent(c.b).line);
                    if (c.type == SketchConstraintType::Angle) tag = s.constrain_angle(ent(c.a).line, ent(c.b).line, val);
                    break;
                case SketchConstraintType::PointOnLine:
                    if (!(st = needs(c.b, K::Line, std::nullopt))) return st.error();
                    tag = s.constrain_point_on_line(c.a, ent(c.b).line);
                    break;
                case SketchConstraintType::PointOnCircle:
                    if (!(st = needs(c.b, K::Circle, K::Arc))) return st.error();
                    tag = s.constrain_point_on_circle(c.a, ent(c.b).circle);
                    break;
                case SketchConstraintType::EqualRadius:
                    if (!(st = needs(c.a, K::Circle, K::Arc)) || !(st = needs(c.b, K::Circle, K::Arc))) return st.error();
                    tag = s.constrain_equal_radius(ent(c.a).circle, ent(c.b).circle);
                    break;
                case SketchConstraintType::Symmetric:
                    if (!(st = needs(c.c, K::Line, std::nullopt))) return st.error();
                    tag = s.constrain_symmetric(c.a, c.b, ent(c.c).line);
                    break;
            }
            tags.push_back(tag);
        }

        const sketch2d::Report report = bs.solve();
        auto constraintOfTag = [&](int tag) {
            for (std::size_t k = 0; k < tags.size(); ++k) {
                if (tags[k] == tag) return "#" + std::to_string(k + 1) + " (" + toString(data->constraints[k].type) + ")";
            }
            return std::string("#internal");
        };
        if (!report.conflicting.empty()) {
            std::string list;
            for (int t : report.conflicting) list += " " + constraintOfTag(t);
            return fail(ErrorCode::InvalidInput, "sketch constraints conflict:" + list);
        }
        double scale = 1.0;
        for (std::size_t k = 0; k < np; ++k) scale = std::max({scale, std::abs(pos[k].x), std::abs(pos[k].y)});
        for (std::size_t k = 0; k < data->constraints.size(); ++k) {
            const double r = residual(data->constraints[k], v[c0 + k], bs);
            if (!(r <= 1e-6 * scale)) {
                return fail(ErrorCode::OperationFailed, "sketch solver did not satisfy constraint #" + std::to_string(k + 1) +
                                                            " (" + toString(data->constraints[k].type) +
                                                            "), residual " + num(r));
            }
        }

        auto curves = bs.curves(1e-6 * scale);
        if (!curves) return curves.error();
        auto result = std::make_shared<SketchResult>(pl);
        result->curves = curves.value();
        result->dof = report.dof;
        result->solverStatus = report.status;
        std::vector<bool> used(np, false);
        for (const auto& g : data->geometry) {
            for (int p : {g.start, g.end, g.center}) {
                if (p >= 0) used[static_cast<std::size_t>(p)] = true;
            }
        }
        for (std::size_t k = 0; k < np; ++k) {
            const auto p = bs.sketch().point(static_cast<sketch2d::Id>(k));
            result->points.push_back({p.x, p.y});
            if (!used[k]) result->standalonePoints.push_back(k);
        }

        FeatureOutput out;
        if (!report.redundant.empty()) {
            std::string list;
            for (int t : report.redundant) list += " " + constraintOfTag(t);
            out.warnings.push_back("redundant sketch constraints:" + list);
        }
        const bool hasProfile = std::any_of(result->curves.begin(), result->curves.end(),
                                            [](const SketchCurve& c) { return !c.construction; });
        if (hasProfile) {
            auto profile = SketchProfile::build(pl, result->curves, ProfileOptions{1e-6 * scale});
            if (!profile) return profile.error();
            auto shared = std::make_shared<const SketchProfile>(std::move(profile).value());
            auto regions = selectRegions(*shared, data->regions);
            if (!regions) return regions.error();
            result->profile = shared;
            result->regions = regions.value();
            auto faces = nameFaces(ctx.featureId(), *shared, result->regions);
            if (!faces) return faces.error();
            result->faces = std::make_shared<const NamedShape>(std::move(faces).value());
        } else if (!data->regions.empty()) {
            return fail(ErrorCode::InvalidInput, "regions are selected but the sketch has no profile curves");
        }
        out.sketch = result;
        return out;
    }

private:
    static Result<SketchPlacement> place(const FeatureContext& ctx) {
        const std::string& choice = ctx.option("plane");
        if (choice == "face") {
            const auto& faces = ctx.references("face");
            if (faces.size() != 1) {
                return Error{ErrorCode::InvalidInput, "sketch on a face needs one face reference, has " +
                                                          std::to_string(faces.size())};
            }
            const TopoDS_Face& face = TopoDS::Face(faces.front());
            auto pln = planeOfFace(face);
            if (!pln) return pln.error();
            const gp_Dir n = pln.value().Axis().Direction();
            // Deterministic frame: X = global X projected on the face (global Y when X is
            // nearly normal to it); the origin is the given point projected on the face.
            const gp_Dir x = std::abs(n.Dot(gp::DX())) < 0.99 ? gp::DX() : gp::DY();
            FacePlacementOptions opt;
            opt.origin = gp_Pnt(ctx.parameter("originX"), ctx.parameter("originY"), ctx.parameter("originZ"));
            opt.xDirection = x;
            return SketchPlacement::onFace(face, opt);
        }
        auto pln = planeFromOption(ctx, choice, "face", "plane");
        if (!pln) return pln.error();
        gp_Pln p = pln.value();
        if (choice != "datum") {
            auto moved = planeOffset(p, ctx.parameter("offset"));
            if (!moved) return moved.error();
            p = moved.value();
        }
        return SketchPlacement::fromPlane(p);
    }

    static Result<std::vector<std::size_t>> selectRegions(const SketchProfile& profile,
                                                          const std::vector<SketchEntityId>& selection) {
        using R = Result<std::vector<std::size_t>>;
        if (selection.empty()) {
            auto m = profile.materialRegions();
            if (m.empty()) return R::failure(ErrorCode::InvalidInput, "the sketch profile has no material region");
            return m;
        }
        std::vector<std::size_t> out;
        for (SketchEntityId e : selection) {
            std::optional<std::size_t> best;
            for (std::size_t r = 0; r < profile.regions().size(); ++r) {
                const ProfileRegion& reg = profile.regions()[r];
                const auto& ents = profile.loops()[reg.outerLoop].entities;
                if (std::find(ents.begin(), ents.end(), e) == ents.end()) continue;
                if (!best || reg.area < profile.regions()[*best].area) best = r;
            }
            if (!best) {
                return R::failure(ErrorCode::InvalidInput, "sketch entity " + std::to_string(e) +
                                                               " is not on the outer contour of any region");
            }
            if (std::find(out.begin(), out.end(), *best) == out.end()) out.push_back(*best);
        }
        return out;
    }

    // Faces of the selected regions with names from the sketch entities.
    static Result<NamedShape> nameFaces(const std::string& sketchId, const SketchProfile& profile,
                                        const std::vector<std::size_t>& regions) {
        auto shape = profile.makeFaces(regions);
        if (!shape) return shape.error();
        const TopoDS_Shape& s = shape.value();
        std::array<std::vector<ElementRecord>, 3> records;
        auto record = [&](const TopoDS_Shape& el, std::string name) -> Result<ElementRecord> {
            auto sig = computeSignature(el);
            if (!sig) return sig.error();
            ElementRecord r;
            r.name = sketchId + ":" + name;
            r.origin = NameOrigin::Role;
            r.feature = sketchId;
            r.signature = std::move(sig).value();
            return r;
        };
        kernel::ShapeIndexMap faces, edges, vertices;
        TopExp::MapShapes(s, TopAbs_FACE, faces);
        TopExp::MapShapes(s, TopAbs_EDGE, edges);
        TopExp::MapShapes(s, TopAbs_VERTEX, vertices);
        std::vector<std::string> names[3];
        std::vector<Point2> where[3];
        for (int i = 1; i <= faces.Extent(); ++i) {
            std::string name;
            for (std::size_t r : regions) {
                const ProfileRegion& reg = profile.regions()[r];
                if (reg.face.IsSame(faces.FindKey(i))) {
                    const auto& ents = profile.loops()[reg.outerLoop].entities;
                    name = "R" + std::to_string(*std::min_element(ents.begin(), ents.end()));
                }
            }
            if (name.empty()) return Error{ErrorCode::OperationFailed, "sketch region face lost while building the profile"};
            names[0].push_back(name);
            where[0].push_back(profile.placement().toLocal(BRep_Tool::Pnt(TopExp::FirstVertex(TopoDS::Edge(
                TopExp_Explorer(faces.FindKey(i), TopAbs_EDGE).Current())))));
        }
        TopTools_IndexedDataMapOfShapeListOfShape vertexEdges;
        TopExp::MapShapesAndAncestors(s, TopAbs_VERTEX, TopAbs_EDGE, vertexEdges);
        for (int i = 1; i <= edges.Extent(); ++i) {
            auto id = profile.entityOf(edges.FindKey(i));
            if (!id) return Error{ErrorCode::OperationFailed, "profile edge without a sketch entity"};
            names[1].push_back("E" + std::to_string(*id));
            where[1].push_back(profile.placement().toLocal(BRep_Tool::Pnt(TopExp::FirstVertex(TopoDS::Edge(edges.FindKey(i))))));
        }
        for (int i = 1; i <= vertices.Extent(); ++i) {
            std::set<SketchEntityId> ids;
            const int k = vertexEdges.FindIndex(vertices.FindKey(i));
            if (k > 0) {
                for (const TopoDS_Shape& e : vertexEdges.FindFromIndex(k)) {
                    if (auto id = profile.entityOf(e)) ids.insert(*id);
                }
            }
            std::string name = "V";
            for (SketchEntityId id : ids) name += (name.size() > 1 ? "_" : "") + std::to_string(id);
            names[2].push_back(name);
            where[2].push_back(profile.placement().toLocal(BRep_Tool::Pnt(TopoDS::Vertex(vertices.FindKey(i)))));
        }
        // Equal names (a lens of two arcs has two vertices shared by the same entities) are
        // told apart by their position in the sketch plane.
        for (int kind = 0; kind < 3; ++kind) {
            std::map<std::string, std::vector<std::size_t>> groups;
            for (std::size_t i = 0; i < names[kind].size(); ++i) groups[names[kind][i]].push_back(i);
            for (auto& [name, idx] : groups) {
                if (idx.size() < 2) continue;
                std::sort(idx.begin(), idx.end(), [&](std::size_t a, std::size_t b) {
                    const Point2 &pa = where[kind][a], &pb = where[kind][b];
                    if (std::abs(pa.x - pb.x) > 1e-7) return pa.x < pb.x;
                    return pa.y < pb.y;
                });
                for (std::size_t k = 0; k < idx.size(); ++k) names[kind][idx[k]] = name + ";D" + std::to_string(k + 1);
            }
        }
        const kernel::ShapeIndexMap* maps[3] = {&faces, &edges, &vertices};
        for (int kind = 0; kind < 3; ++kind) {
            for (int i = 1; i <= maps[kind]->Extent(); ++i) {
                auto r = record(maps[kind]->FindKey(i), names[kind][static_cast<std::size_t>(i - 1)]);
                if (!r) return r.error();
                records[static_cast<std::size_t>(kind)].push_back(std::move(r).value());
            }
        }
        return NamedShape::fromRecords(s, std::move(records), {});
    }
};

// ---------------------------------------------------------------- datum plane / axis

class DatumPlaneFeature final : public Feature {
public:
    std::string type() const override { return "DatumPlane"; }
    std::vector<ParameterSpec> parameterSpecs() const override { return {{"offset", "0"}, {"angle", "0"}}; }
    std::vector<ReferenceSlot> referenceSlots() const override {
        return {{"face", ElementKind::Face, false, true},
                {"face2", ElementKind::Face, false, true},
                {"edge", ElementKind::Edge, false, true}};
    }
    std::vector<OptionSpec> optionSpecs() const override {
        return {{"method", {"offset", "angle", "midway"}},
                {"base", {"XY", "XZ", "YZ", "face", "datum"}},
                {"axis", {"edge", "datum", "X", "Y", "Z"}}};
    }
    std::vector<LinkSlot> linkSlots() const override {
        return {{"plane", {"DatumPlane"}, true}, {"axis", {"DatumAxis"}, true}};
    }

    Result<FeatureOutput> execute(const FeatureContext& ctx) const override {
        const std::string& method = ctx.option("method");
        Result<gp_Pln> plane = Error{ErrorCode::InvalidInput, "no plane"};
        if (method == "midway") {
            const auto& a = ctx.references("face");
            const auto& b = ctx.references("face2");
            if (a.size() != 1 || b.size() != 1) return fail(ErrorCode::InvalidInput, "midway plane needs references 'face' and 'face2'");
            plane = planeMidway(TopoDS::Face(a.front()), TopoDS::Face(b.front()));
        } else {
            auto base = planeFromOption(ctx, ctx.option("base"), "face", "plane");
            if (!base) return base.error();
            if (method == "offset") {
                plane = planeOffset(base.value(), ctx.parameter("offset"));
            } else {
                auto axis = axisFromOption(ctx, ctx.option("axis"), "face", "edge", "axis");
                if (!axis) return axis.error();
                plane = planeAtAngle(base.value(), axis.value(), ctx.parameter("angle") * M_PI / 180.0);
            }
        }
        if (!plane) return plane.error();
        FeatureOutput out;
        auto datum = std::make_shared<DatumResult>();
        datum->plane = plane.value();
        out.datum = datum;
        return out;
    }
};

class DatumAxisFeature final : public Feature {
public:
    std::string type() const override { return "DatumAxis"; }
    std::vector<ParameterSpec> parameterSpecs() const override { return {}; }
    std::vector<ReferenceSlot> referenceSlots() const override {
        return {{"face", ElementKind::Face, false, true}, {"edge", ElementKind::Edge, false, true}};
    }
    std::vector<OptionSpec> optionSpecs() const override {
        return {{"method", {"X", "Y", "Z", "face", "edge", "planes"}}};
    }
    std::vector<LinkSlot> linkSlots() const override {
        return {{"plane", {"DatumPlane"}, true}, {"plane2", {"DatumPlane"}, true}};
    }

    Result<FeatureOutput> execute(const FeatureContext& ctx) const override {
        const std::string& method = ctx.option("method");
        Result<gp_Ax1> axis = Error{ErrorCode::InvalidInput, "no axis"};
        if (method == "planes") {
            const LinkedFeature* a = ctx.link("plane");
            const LinkedFeature* b = ctx.link("plane2");
            if (!a || !b || !a->datum || !b->datum || !a->datum->plane || !b->datum->plane) {
                return fail(ErrorCode::InvalidInput, "axis of two planes needs links 'plane' and 'plane2'");
            }
            axis = axisOfPlaneIntersection(*a->datum->plane, *b->datum->plane);
        } else {
            axis = axisFromOption(ctx, method, "face", "edge", "");
        }
        if (!axis) return axis.error();
        FeatureOutput out;
        auto datum = std::make_shared<DatumResult>();
        datum->axis = axis.value();
        out.datum = datum;
        return out;
    }
};

}  // namespace

void addSketchFeatures(std::vector<std::pair<std::string, FeatureRegistry::Factory>>& all) {
    all.emplace_back("Sketch", [] { return std::make_unique<SketchFeature>(); });
    all.emplace_back("DatumPlane", [] { return std::make_unique<DatumPlaneFeature>(); });
    all.emplace_back("DatumAxis", [] { return std::make_unique<DatumAxisFeature>(); });
}

}  // namespace features

}  // namespace bendcore::modeling
