// Поиск осей цилиндрических граней для осевых линий и центровых меток (фаза 4, п. 4).

#include <bendcore/drawing_v2/view_sheet.hpp>

#include <BRepAdaptor_Surface.hxx>
#include <BRepTools.hxx>
#include <Standard_Failure.hxx>
#include <gp_Cylinder.hxx>

#include <algorithm>
#include <cmath>
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
    try {
        const auto& faces = index.value().faces();
        for (std::uint32_t fi = 0; fi < faces.size(); ++fi) {
            if (faces[fi].surface != kernel::SurfaceType::Cylinder) continue;
            BRepAdaptor_Surface s(faces[fi].face);
            const gp_Cylinder cyl = s.Cylinder();
            const gp_Ax1 ax = cyl.Axis();
            Vec3 loc{ax.Location().X(), ax.Location().Y(), ax.Location().Z()};
            Vec3 dir{ax.Direction().X(), ax.Direction().Y(), ax.Direction().Z()};
            double umin, umax, vmin, vmax;
            BRepTools::UVBounds(faces[fi].face, umin, umax, vmin, vmax);
            const double sweep = std::min(umax - umin, 2 * std::numbers::pi);
            const double r = cyl.Radius();
            bool merged = false;
            for (CylGroup& g : groups) {
                const double par = dot(g.dir, dir);
                if (std::abs(std::abs(par) - 1.0) > 1e-9) continue;
                const Vec3 d = loc - g.loc;
                const Vec3 perp = d - dot(d, g.dir) * g.dir;
                if (length(perp) > tol || std::abs(g.radius - r) > tol) continue;
                const double off = dot(d, g.dir);
                const double a = off + par * vmin, c = off + par * vmax;
                g.vmin = std::min({g.vmin, a, c});
                g.vmax = std::max({g.vmax, a, c});
                g.sweep = std::min(2 * std::numbers::pi, g.sweep + sweep);
                merged = true;
                break;
            }
            if (!merged) groups.push_back({loc, dir, r, sweep, vmin, vmax, fi});
        }
    } catch (const Standard_Failure& f) {
        return R::failure(ErrorCode::OperationFailed, std::string("center lines: ") + f.what());
    }

    CenterGeometry out;
    for (const CylGroup& g : groups) {
        if (g.sweep < minSweepRad - 1e-9) continue;
        const double along = dot(g.dir, frame.toward);
        if (std::abs(std::abs(along) - 1.0) < 1e-9) {
            out.marks.push_back({frame.project(g.loc), g.radius, g.face});
        } else if (std::abs(along) < 1e-9) {
            out.axes.push_back({frame.project(g.loc + g.vmin * g.dir),
                                frame.project(g.loc + g.vmax * g.dir), g.radius, g.face});
        }
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
