// След секущей плоскости (ГОСТ 2.305-2008, разд. 6), выносные элементы (разд. 8). Разрезы и
// сечения — section_cuts.cpp.

#include <bendcore/drawing_v2/sections.hpp>

#include "views_internal.hpp"

#include <BRepAdaptor_Surface.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepGProp.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepTools_WireExplorer.hxx>
#include <BRep_Tool.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <GProp_GProps.hxx>
#include <NCollection_DataMap.hxx>
#include <Standard_Failure.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <gp_Ax2.hxx>
#include <gp_Pln.hxx>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace bendcore::drawing_v2::views {

namespace {

Vec3 unit3(Vec3 a) { return (1.0 / length(a)) * a; }

}  // namespace

Result<CuttingTrace> cuttingTrace(const ViewFrame& parent, const Box2& bounds,
                                  const CuttingPlane& plane) {
    using R = Result<CuttingTrace>;
    if (!(length(plane.sight) > 1e-12) || bounds.empty)
        return R::failure(ErrorCode::InvalidInput, "cutting trace: invalid plane or empty bounds");
    const Vec3 n = unit3(plane.sight);
    if (std::abs(dot(n, parent.toward)) > 1e-9)
        return R::failure(ErrorCode::InvalidInput,
                          "cutting trace: the plane must be perpendicular to the view plane");
    const Vec3 d3 = cross(n, parent.toward);
    const Point2 d{dot(d3, parent.right), dot(d3, parent.up())};
    const Point2 p = parent.project(plane.point);
    // Отсечение прямой p + t·d прямоугольником (Лян — Барски).
    double t0 = -1e300, t1 = 1e300;
    auto clip = [&](double dd, double lo, double hi, double pp) {
        if (std::abs(dd) < 1e-15) return pp >= lo && pp <= hi;
        double a = (lo - pp) / dd, b = (hi - pp) / dd;
        if (a > b) std::swap(a, b);
        t0 = std::max(t0, a);
        t1 = std::min(t1, b);
        return t0 <= t1;
    };
    if (!clip(d.x, bounds.minX, bounds.maxX, p.x) || !clip(d.y, bounds.minY, bounds.maxY, p.y))
        return R::failure(ErrorCode::InvalidInput, "cutting trace: the plane misses the view");
    CuttingTrace tr;
    tr.a = {p.x + t0 * d.x, p.y + t0 * d.y};
    tr.b = {p.x + t1 * d.x, p.y + t1 * d.y};
    const Point2 s{dot(n, parent.right), dot(n, parent.up())};
    const double sl = std::hypot(s.x, s.y);
    tr.sight2d = {s.x / sl, s.y / sl};
    return tr;
}

namespace {

bool inside(Point2 p, Point2 c, double r) { return std::hypot(p.x - c.x, p.y - c.y) <= r; }

// Участки отрезка внутри окружности (параметры 0…1).
std::optional<std::pair<double, double>> clipSegment(Point2 a, Point2 b, Point2 c, double r) {
    const double dx = b.x - a.x, dy = b.y - a.y;
    const double fx = a.x - c.x, fy = a.y - c.y;
    const double A = dx * dx + dy * dy, B = 2 * (fx * dx + fy * dy), C = fx * fx + fy * fy - r * r;
    if (A <= 0) return std::nullopt;
    const double disc = B * B - 4 * A * C;
    if (disc <= 0) return std::nullopt;
    const double sq = std::sqrt(disc);
    const double t0 = std::max(0.0, (-B - sq) / (2 * A)), t1 = std::min(1.0, (-B + sq) / (2 * A));
    if (t1 - t0 <= 1e-12) return std::nullopt;
    return std::make_pair(t0, t1);
}

Point2 lerp(Point2 a, Point2 b, double t) { return {a.x + t * (b.x - a.x), a.y + t * (b.y - a.y)}; }

}  // namespace

Result<ProjectedView> detailView(const ProjectedView& parent, Point2 center, double radius) {
    if (!(radius > 0) || !std::isfinite(center.x) || !std::isfinite(center.y))
        return Result<ProjectedView>::failure(ErrorCode::InvalidInput,
                                              "detail view: radius must be positive");
    ProjectedView out;
    out.frame = parent.frame;
    for (const ViewEdge& e : parent.edges) {
        auto push = [&](Curve2 c) {
            ViewEdge piece = e;
            piece.curve = std::move(c);
            out.edges.push_back(std::move(piece));
        };
        if (auto* l = std::get_if<LineSeg>(&e.curve)) {
            if (auto iv = clipSegment(l->a, l->b, center, radius))
                push(LineSeg{lerp(l->a, l->b, iv->first), lerp(l->a, l->b, iv->second)});
        } else if (auto* pl = std::get_if<Polyline>(&e.curve)) {
            std::vector<Point2> pts = pl->points;
            if (pl->closed && !pts.empty()) pts.push_back(pts.front());
            std::vector<Point2> run;
            for (std::size_t i = 1; i < pts.size(); ++i) {
                auto iv = clipSegment(pts[i - 1], pts[i], center, radius);
                if (!iv) {
                    if (run.size() >= 2) push(Polyline{run, false});
                    run.clear();
                    continue;
                }
                const Point2 a = lerp(pts[i - 1], pts[i], iv->first);
                const Point2 b = lerp(pts[i - 1], pts[i], iv->second);
                if (run.empty() || std::hypot(run.back().x - a.x, run.back().y - a.y) > 1e-9) {
                    if (run.size() >= 2) push(Polyline{run, false});
                    run = {a};
                }
                run.push_back(b);
                if (iv->second < 1.0) {
                    push(Polyline{run, false});
                    run.clear();
                }
            }
            if (run.size() >= 2) push(Polyline{run, false});
        } else {
            // Дуга или окружность: углы пересечения с окружностью ограничения.
            Point2 c;
            double r, s, sweep;
            if (auto* a = std::get_if<Arc>(&e.curve)) {
                c = a->center;
                r = a->radius;
                s = a->startDeg * std::numbers::pi / 180.0;
                double sw = std::fmod(a->endDeg - a->startDeg, 360.0);
                if (sw <= 0) sw += 360.0;
                sweep = sw * std::numbers::pi / 180.0;
            } else {
                const auto& ci = std::get<Circle>(e.curve);
                c = ci.center;
                r = ci.radius;
                s = 0;
                sweep = 2 * std::numbers::pi;
            }
            const double d = std::hypot(c.x - center.x, c.y - center.y);
            std::vector<double> cuts{0.0, sweep};
            const bool crosses = d > 1e-12 && d < r + radius && d > std::abs(r - radius);
            if (crosses && std::holds_alternative<Circle>(e.curve)) {
                // Окружность начинается с первой точки пересечения — без лишнего разрыва в 0°.
                const double base = std::atan2(center.y - c.y, center.x - c.x);
                const double h = std::acos(std::clamp((r * r + d * d - radius * radius) / (2 * r * d), -1.0, 1.0));
                s = base - h;
                cuts = {0.0, 2 * h, sweep};
            } else if (crosses) {
                const double base = std::atan2(center.y - c.y, center.x - c.x);
                const double h = std::acos(std::clamp((r * r + d * d - radius * radius) / (2 * r * d), -1.0, 1.0));
                for (double ang : {base - h, base + h}) {
                    double rel = std::fmod(ang - s, 2 * std::numbers::pi);
                    if (rel < 0) rel += 2 * std::numbers::pi;
                    if (rel > 0 && rel < sweep) cuts.push_back(rel);
                }
            }
            std::sort(cuts.begin(), cuts.end());
            const bool full = std::holds_alternative<Circle>(e.curve);
            for (std::size_t i = 1; i < cuts.size(); ++i) {
                const double m = s + 0.5 * (cuts[i - 1] + cuts[i]);
                if (!inside({c.x + r * std::cos(m), c.y + r * std::sin(m)}, center, radius)) continue;
                if (full && cuts.size() == 2) {
                    push(Circle{c, r});
                    continue;
                }
                push(Arc{c, r, (s + cuts[i - 1]) * 180.0 / std::numbers::pi,
                         (s + cuts[i]) * 180.0 / std::numbers::pi});
            }
        }
    }
    // Габарит выносного элемента — окружность ограничения (по ней ставится надпись).
    out.bounds = detail::boundsOf(out.edges);
    out.bounds.add({center.x - radius, center.y - radius});
    out.bounds.add({center.x + radius, center.y + radius});
    return out;
}

}  // namespace bendcore::drawing_v2::views
