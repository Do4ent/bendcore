// Поиск осей цилиндрических и конических граней, круговых рёбер и круговых массивов для осевых
// линий и центровых меток (фаза 4, п. 4; docs/drawing/VIEWS.md, «Осевые линии»).

#include <bendcore/drawing_v2/view_sheet.hpp>

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepTools.hxx>
#include <Standard_Failure.hxx>
#include <gp_Circ.hxx>
#include <gp_Cone.hxx>
#include <gp_Cylinder.hxx>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace bendcore::drawing_v2::views {

namespace {

struct CylGroup {
    Vec3 loc;  // точка на оси
    Vec3 dir;  // единичное направление оси
    double radius = 0;
    double sweep = 0;           // суммарный угловой охват, рад
    double vmin = 0, vmax = 0;  // протяжённость по оси от loc
    std::uint32_t face = 0;     // первая грань группы
    bool cone = false;          // коническая грань: группа по оси, радиус наибольший
};

// Окружность круговых рёбер: центр и нормаль в модели, суммарный охват дуг.
struct CircGroup {
    Vec3 center;
    Vec3 dir;
    double radius = 0;
    double sweep = 0;
    std::uint32_t edge = 0;
};

}  // namespace

Result<CenterGeometry> findCenterGeometry(const kernel::Shape& shape, const ViewFrame& frame,
                                          double minSweepRad) {
    using R = Result<CenterGeometry>;
    if (shape.isNull()) return R::failure(ErrorCode::InvalidInput, "center lines: shape is null");
    auto index = kernel::TopologyIndex::build(shape);
    if (!index) return index.error();
    auto bbox = shape.boundingBox();
    if (!bbox) return bbox.error();
    const auto& b = bbox.value();
    const double size = std::hypot(b.max[0] - b.min[0], b.max[1] - b.min[1], b.max[2] - b.min[2]);
    const double tol = 1e-6 * std::max(1.0, size);

    std::vector<CylGroup> groups;
    std::vector<CircGroup> circles;
    try {
        const auto& faces = index.value().faces();
        for (std::uint32_t fi = 0; fi < faces.size(); ++fi) {
            const bool isCyl = faces[fi].surface == kernel::SurfaceType::Cylinder;
            const bool isCone = faces[fi].surface == kernel::SurfaceType::Cone;
            if (!isCyl && !isCone) continue;
            BRepAdaptor_Surface s(faces[fi].face);
            double umin, umax, vmin, vmax;
            BRepTools::UVBounds(faces[fi].face, umin, umax, vmin, vmax);
            gp_Ax1 ax;
            double r = 0;
            if (isCyl) {
                const gp_Cylinder cyl = s.Cylinder();
                ax = cyl.Axis();
                r = cyl.Radius();
            } else {
                // Конус: точка образующей с параметром v — на высоте v·cos α от начала оси,
                // радиус R + v·sin α.
                const gp_Cone cone = s.Cone();
                ax = cone.Axis();
                const double ca = std::cos(cone.SemiAngle()), sa = std::sin(cone.SemiAngle());
                r = std::max(cone.RefRadius() + vmin * sa, cone.RefRadius() + vmax * sa);
                vmin *= ca;
                vmax *= ca;
            }
            Vec3 loc{ax.Location().X(), ax.Location().Y(), ax.Location().Z()};
            Vec3 dir{ax.Direction().X(), ax.Direction().Y(), ax.Direction().Z()};
            const double sweep = std::min(umax - umin, 2 * std::numbers::pi);
            bool merged = false;
            for (CylGroup& g : groups) {
                if (g.cone != isCone) continue;
                const double par = dot(g.dir, dir);
                if (std::abs(std::abs(par) - 1.0) > 1e-9) continue;
                const Vec3 d = loc - g.loc;
                const Vec3 perp = d - dot(d, g.dir) * g.dir;
                if (length(perp) > tol || (!isCone && std::abs(g.radius - r) > tol)) continue;
                const double off = dot(d, g.dir);
                const double a = off + par * vmin, c = off + par * vmax;
                g.vmin = std::min({g.vmin, a, c});
                g.vmax = std::max({g.vmax, a, c});
                g.sweep = std::min(2 * std::numbers::pi, g.sweep + sweep);
                g.radius = std::max(g.radius, r);
                merged = true;
                break;
            }
            if (!merged) groups.push_back({loc, dir, r, sweep, std::min(vmin, vmax), std::max(vmin, vmax), fi, isCone});
        }
        // Круговые рёбра в плоскости, параллельной плоскости вида.
        const auto& edges = index.value().edges();
        for (std::uint32_t ei = 0; ei < edges.size(); ++ei) {
            if (edges[ei].curve != kernel::CurveType::Circle) continue;
            BRepAdaptor_Curve c(edges[ei].edge);
            const gp_Circ circ = c.Circle();
            const gp_Ax1 ax = circ.Axis();
            const Vec3 dir{ax.Direction().X(), ax.Direction().Y(), ax.Direction().Z()};
            if (std::abs(std::abs(dot(dir, frame.toward)) - 1.0) > 1e-9) continue;
            const Vec3 center{circ.Location().X(), circ.Location().Y(), circ.Location().Z()};
            const double sweep = std::min(std::abs(c.LastParameter() - c.FirstParameter()), 2 * std::numbers::pi);
            const double r = circ.Radius();
            bool merged = false;
            for (CircGroup& g : circles) {
                if (length(center - g.center) > tol || std::abs(g.radius - r) > tol) continue;
                g.sweep = std::min(2 * std::numbers::pi, g.sweep + sweep);
                merged = true;
                break;
            }
            if (!merged) circles.push_back({center, dir, r, sweep, ei});
        }
    } catch (const Standard_Failure& f) {
        return R::failure(ErrorCode::OperationFailed, std::string("center lines: ") + f.what());
    }

    CenterGeometry out;
    for (const CylGroup& g : groups) {
        if (g.sweep < minSweepRad - 1e-9) continue;
        const double along = dot(g.dir, frame.toward);
        if (std::abs(std::abs(along) - 1.0) < 1e-9) {
            out.marks.push_back({frame.project(g.loc), g.radius, g.face, std::nullopt, std::nullopt});
        } else if (std::abs(along) < 1e-9) {
            if (g.vmax - g.vmin < tol) continue;
            out.axes.push_back({frame.project(g.loc + g.vmin * g.dir),
                                frame.project(g.loc + g.vmax * g.dir), g.radius, g.face});
        }
    }
    for (const CircGroup& g : circles) {
        if (g.sweep < minSweepRad - 1e-9) continue;
        out.marks.push_back({frame.project(g.center), g.radius, std::numeric_limits<std::uint32_t>::max(), g.edge, std::nullopt});
    }
    // Соосные отверстия разных радиусов (зенковка, ступень) — одна метка по большему радиусу.
    std::vector<CenterMark> marks;
    for (const CenterMark& m : out.marks) {
        auto it = std::find_if(marks.begin(), marks.end(), [&](const CenterMark& k) {
            return std::hypot(k.center.x - m.center.x, k.center.y - m.center.y) <= tol;
        });
        if (it == marks.end()) marks.push_back(m);
        else if (m.radius > it->radius) *it = m;
    }
    out.marks = std::move(marks);
    // Круговые массивы: не меньше трёх меток на одном расстоянии от центра другой метки.
    std::vector<bool> used(out.marks.size(), false);
    for (std::size_t ci = 0; ci < out.marks.size(); ++ci) {
        const Point2 c = out.marks[ci].center;
        std::vector<std::pair<double, std::size_t>> byDist;
        for (std::size_t k = 0; k < out.marks.size(); ++k) {
            if (k == ci || used[k]) continue;
            const double d = std::hypot(out.marks[k].center.x - c.x, out.marks[k].center.y - c.y);
            if (d > out.marks[k].radius + tol) byDist.emplace_back(d, k);
        }
        std::sort(byDist.begin(), byDist.end());
        for (std::size_t i = 0; i < byDist.size();) {
            std::size_t j = i;
            while (j < byDist.size() && byDist[j].first - byDist[i].first <= tol) ++j;
            if (j - i >= 3) {
                PitchCircle pc{c, byDist[i].first, {}};
                for (std::size_t k = i; k < j; ++k) {
                    const std::size_t m = byDist[k].second;
                    const Point2 p = out.marks[m].center;
                    const double d = byDist[k].first;
                    out.marks[m].radial = Point2{(p.x - c.x) / d, (p.y - c.y) / d};
                    used[m] = true;
                    pc.members.push_back(m);
                }
                out.circles.push_back(std::move(pc));
            }
            i = j;
        }
    }
    // Осевые линии на одной прямой с перекрывающимися участками — одна линия.
    std::vector<AxisLine> axes;
    for (const AxisLine& a : out.axes) {
        bool merged = false;
        for (AxisLine& k : axes) {
            const double dx = k.b.x - k.a.x, dy = k.b.y - k.a.y;
            const double len = std::hypot(dx, dy);
            if (len < tol) continue;
            const double ux = dx / len, uy = dy / len;
            auto off = [&](Point2 p) { return std::abs((p.x - k.a.x) * uy - (p.y - k.a.y) * ux); };
            if (off(a.a) > tol || off(a.b) > tol) continue;
            auto t = [&](Point2 p) { return (p.x - k.a.x) * ux + (p.y - k.a.y) * uy; };
            const double t0 = std::min(t(a.a), t(a.b)), t1 = std::max(t(a.a), t(a.b));
            if (t0 > len + tol || t1 < -tol) continue;
            const double n0 = std::min(0.0, t0), n1 = std::max(len, t1);
            const Point2 base = k.a;
            k.a = {base.x + ux * n0, base.y + uy * n0};
            k.b = {base.x + ux * n1, base.y + uy * n1};
            k.radius = std::max(k.radius, a.radius);
            merged = true;
            break;
        }
        if (!merged) axes.push_back(a);
    }
    out.axes = std::move(axes);
    return out;
}

}  // namespace bendcore::drawing_v2::views
