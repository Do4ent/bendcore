// Разрезы и сечения одной плоскостью (ГОСТ 2.305-2008, разделы 6, 7), след секущей плоскости,
// выносные элементы (раздел 8).

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

using ShapeToIndex = NCollection_DataMap<TopoDS_Shape, std::uint32_t, TopTools_ShapeMapHasher>;

Vec3 unit3(Vec3 a) { return (1.0 / length(a)) * a; }

bool onPlane(const gp_Pnt& p, Vec3 point, Vec3 n, double tol) {
    return std::abs(dot(Vec3{p.X(), p.Y(), p.Z()} - point, n)) <= tol;
}

// Грань результата лежит в секущей плоскости.
bool isSectionFace(const TopoDS_Face& f, Vec3 point, Vec3 n, double tol) {
    BRepAdaptor_Surface s(f, false);
    if (s.GetType() != GeomAbs_Plane) return false;
    const gp_Pln pl = s.Plane();
    const gp_Dir d = pl.Axis().Direction();
    if (std::abs(std::abs(d.X() * n.x + d.Y() * n.y + d.Z() * n.z) - 1.0) > 1e-9) return false;
    return onPlane(pl.Location(), point, n, tol);
}

bool edgeOnPlane(const TopoDS_Edge& e, Vec3 point, Vec3 n, double tol) {
    BRepAdaptor_Curve c(e);
    const double u0 = c.FirstParameter(), u1 = c.LastParameter();
    for (double t : {0.0, 0.25, 0.5, 0.75, 1.0})
        if (!onPlane(c.Value(u0 + t * (u1 - u0)), point, n, tol)) return false;
    return true;
}

}  // namespace

Result<SectionView> sectionShape(const kernel::Shape& shape, const ViewFrame& frame,
                                 const CuttingPlane& plane, SectionKind kind,
                                 const HlrOptions& options) {
    using R = Result<SectionView>;
    if (shape.isNull()) return R::failure(ErrorCode::InvalidInput, "section: shape is null");
    if (!(length(plane.sight) > 1e-12) || !std::isfinite(length(plane.point)))
        return R::failure(ErrorCode::InvalidInput, "section: invalid cutting plane");
    const Vec3 n = unit3(plane.sight);
    if (std::abs(dot(n, frame.toward) + 1.0) > 1e-9)
        return R::failure(ErrorCode::InvalidInput,
                          "section: the view must look along the sight (toward = -sight)");
    auto bbox = shape.boundingBox();
    if (!bbox) return bbox.error();
    auto index = kernel::TopologyIndex::build(shape);
    if (!index) return index.error();
    const kernel::TopologyIndex& idx = index.value();

    const auto& bb = bbox.value();
    const Vec3 lo{bb.min[0], bb.min[1], bb.min[2]}, hi{bb.max[0], bb.max[1], bb.max[2]};
    const Vec3 center = 0.5 * (lo + hi);
    const double L = length(hi - lo) + length(center - plane.point) + 10.0;
    const double tol = 1e-6 * std::max(1.0, length(hi - lo));

    try {
        // Ящик со стороны наблюдателя: ось Z ящика — −sight, начало — в секущей плоскости.
        const gp_Dir zDir(-n.x, -n.y, -n.z);
        const Vec3 xv = std::abs(n.x) < 0.9 ? unit3(cross(n, Vec3{1, 0, 0}))
                                            : unit3(cross(n, Vec3{0, 1, 0}));
        const Vec3 yv = cross(Vec3{-n.x, -n.y, -n.z}, xv);
        const Vec3 o = plane.point - L * xv - L * yv;
        const gp_Ax2 ax(gp_Pnt(o.x, o.y, o.z), zDir, gp_Dir(xv.x, xv.y, xv.z));
        const TopoDS_Shape tool = BRepPrimAPI_MakeBox(ax, 2 * L, 2 * L, L).Shape();
        BRepAlgoAPI_Cut cut(shape.occt(), tool);
        if (!cut.IsDone() || cut.HasErrors())
            return R::failure(ErrorCode::OperationFailed, "section: boolean cut failed");
        const TopoDS_Shape result = cut.Shape();

        // История: подформы результата → индексы исходной формы.
        auto edgeMap = std::make_shared<ShapeToIndex>();
        auto faceMap = std::make_shared<ShapeToIndex>();
        auto bind = [&cut](ShapeToIndex& m, const TopoDS_Shape& orig, std::uint32_t i) {
            if (cut.IsDeleted(orig)) return;
            const auto& mod = cut.Modified(orig);
            if (mod.IsEmpty()) {
                m.Bind(orig, i);
                return;
            }
            for (const TopoDS_Shape& s : mod) m.Bind(s, i);
        };
        for (std::uint32_t i = 0; i < idx.edges().size(); ++i) bind(*edgeMap, idx.edges()[i].edge, i);
        for (std::uint32_t i = 0; i < idx.faces().size(); ++i) bind(*faceMap, idx.faces()[i].face, i);
        // Рёбра, порождённые пересечением грани исходной формы с плоскостью, — ссылка на грань.
        auto genFace = std::make_shared<ShapeToIndex>();
        for (std::uint32_t i = 0; i < idx.faces().size(); ++i)
            for (const TopoDS_Shape& g : cut.Generated(idx.faces()[i].face))
                if (g.ShapeType() == TopAbs_EDGE && !genFace->IsBound(g)) genFace->Bind(g, i);

        SectionView sv;
        sv.remaining = kernel::Shape(result);

        // Грани фигуры сечения.
        std::vector<TopoDS_Face> secFaces;
        for (TopExp_Explorer ex(result, TopAbs_FACE); ex.More(); ex.Next()) {
            const TopoDS_Face f = TopoDS::Face(ex.Current());
            if (isSectionFace(f, plane.point, n, tol)) secFaces.push_back(f);
        }
        if (secFaces.empty())
            return R::failure(ErrorCode::InvalidInput,
                              "section: the cutting plane does not intersect the model");
        for (const TopoDS_Face& f : secFaces) {
            GProp_GProps g;
            BRepGProp::SurfaceProperties(f, g);
            sv.sectionArea += g.Mass();
            std::vector<std::vector<Point2>> loops;
            for (TopExp_Explorer wx(f, TopAbs_WIRE); wx.More(); wx.Next()) {
                std::vector<Point2> loop;
                for (BRepTools_WireExplorer we(TopoDS::Wire(wx.Current()), f); we.More(); we.Next()) {
                    std::vector<Point2> pts =
                        detail::edgePoints(we.Current(), frame, options.curveDeflection);
                    for (const Point2& p : pts) {
                        if (!loop.empty() && std::hypot(loop.back().x - p.x, loop.back().y - p.y) < 1e-9)
                            continue;
                        loop.push_back(p);
                    }
                }
                if (loop.size() > 1 &&
                    std::hypot(loop.front().x - loop.back().x, loop.front().y - loop.back().y) < 1e-9)
                    loop.pop_back();
                if (loop.size() >= 3) loops.push_back(std::move(loop));
            }
            if (!loops.empty()) sv.regions.push_back(std::move(loops));
        }

        detail::SourceMapper mapper;
        mapper.edge = [edgeMap, genFace](const TopoDS_Shape& s) -> std::optional<std::uint32_t> {
            if (const std::uint32_t* v = edgeMap->Seek(s)) return *v;
            return std::nullopt;
        };
        mapper.face = [faceMap](const TopoDS_Shape& s) -> std::optional<std::uint32_t> {
            if (const std::uint32_t* v = faceMap->Seek(s)) return *v;
            return std::nullopt;
        };
        const Vec3 pp = plane.point;
        mapper.edgeClass = [pp, n, tol](const TopoDS_Edge& e) -> std::optional<EdgeClass> {
            if (edgeOnPlane(e, pp, n, tol)) return EdgeClass::SectionCut;
            return std::nullopt;
        };

        if (kind == SectionKind::Cut) {
            auto v = detail::runHlr(result, frame, options, mapper);
            if (!v) return v.error();
            sv.view = std::move(v.value());
        } else {
            sv.view.frame = frame;
            TopTools_IndexedMapOfShape edges;
            for (const TopoDS_Face& f : secFaces) TopExp::MapShapes(f, TopAbs_EDGE, edges);
            for (int i = 1; i <= edges.Extent(); ++i) {
                const TopoDS_Edge e = TopoDS::Edge(edges(i));
                if (BRep_Tool::Degenerated(e)) continue;
                BRepAdaptor_Curve c(e);
                auto curve = detail::projectCurve(c, c.FirstParameter(), c.LastParameter(), frame,
                                                  options.curveDeflection);
                if (!curve) continue;
                ViewEdge ve;
                ve.curve = std::move(*curve);
                ve.cls = EdgeClass::SectionCut;
                ve.edgeIndex = mapper.edge(e);
                sv.view.edges.push_back(std::move(ve));
            }
            detail::removeOverlaps(sv.view.edges);
            sv.view.bounds = detail::boundsOf(sv.view.edges);
        }
        // Ребро сечения, порождённое гранью исходной формы, получает ссылку на эту грань.
        // Сопоставление по геометрии: середина 2D-ребра лежит на проекции порождённого ребра.
        for (ViewEdge& ve : sv.view.edges) {
            if (ve.cls != EdgeClass::SectionCut || ve.faceIndex) continue;
            for (ShapeToIndex::Iterator it(*genFace); it.More(); it.Next()) {
                const TopoDS_Edge ge = TopoDS::Edge(it.Key());
                BRepAdaptor_Curve gc(ge);
                auto gcurve = detail::projectCurve(gc, gc.FirstParameter(), gc.LastParameter(),
                                                   frame, options.curveDeflection);
                if (!gcurve) continue;
                Box2 gb, eb;
                detail::addCurveToBox(gb, *gcurve);
                detail::addCurveToBox(eb, ve.curve);
                const double t = 1e-6 * std::max(1.0, length(hi - lo));
                if (eb.minX >= gb.minX - t && eb.maxX <= gb.maxX + t && eb.minY >= gb.minY - t &&
                    eb.maxY <= gb.maxY + t && gcurve->index() == ve.curve.index()) {
                    ve.faceIndex = it.Value();
                    break;
                }
            }
        }
        return sv;
    } catch (const Standard_Failure& f) {
        return R::failure(ErrorCode::OperationFailed, std::string("section: OCCT failure: ") + f.what());
    }
}

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
