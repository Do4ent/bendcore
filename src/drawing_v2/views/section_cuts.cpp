// Разрезы и сечения (ГОСТ 2.305-2008, разд. 6, 7): полный разрез одной плоскостью, ступенчатый
// разрез несколькими параллельными плоскостями, местный разрез по замкнутой области вида;
// условности разрезов сборок (детали, которые не рассекаются, — ГОСТ 2.305, разд. 9; ГОСТ 2.109).
//
// Общая схема: из модели вычитается инструмент (ящики со стороны наблюдателя или призма по
// границе области) — BRepAlgoAPI_Cut; фигура сечения — образы граней инструмента, лежащих в
// секущих плоскостях (история булевой операции), стенки инструмента (уступы, граница выреза) —
// не изображаются. Вид — HLR оставшейся части; ссылки 2D-рёбер переводятся на исходную модель
// через историю (Modified / Generated).

#include <bendcore/drawing_v2/sections.hpp>

#include "views_internal.hpp"

#include <BRepAdaptor_Curve.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepGProp.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepTools_WireExplorer.hxx>
#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <GeomAPI_Interpolate.hxx>
#include <Geom_BSplineCurve.hxx>
#include <NCollection_DataMap.hxx>
#include <NCollection_HArray1.hxx>
#include <NCollection_List.hxx>
#include <Standard_Failure.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <NCollection_IndexedMap.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Iterator.hxx>
#include <gp_Ax2.hxx>
#include <gp_Ax3.hxx>
#include <gp_Trsf.hxx>

#include <algorithm>
#include <cmath>
#include <memory>

namespace bendcore::drawing_v2::views {

namespace {

using ShapeToIndex = NCollection_DataMap<TopoDS_Shape, std::uint32_t, TopTools_ShapeMapHasher>;
using ShapeList = NCollection_List<TopoDS_Shape>;
using ShapeSet = NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher>;

Vec3 unit3(Vec3 a) { return (1.0 / length(a)) * a; }
gp_Pnt pnt(Vec3 v) { return gp_Pnt(v.x, v.y, v.z); }
gp_Dir dir(Vec3 v) { return gp_Dir(v.x, v.y, v.z); }

// Инструмент разреза: тело, вычитаемое из модели; грани в секущих плоскостях (их образы —
// фигура сечения) и стенки (рёбра их образов не изображаются).
struct CutTool {
    TopoDS_Shape solid;
    std::vector<TopoDS_Shape> sectionFaces;
    std::vector<TopoDS_Shape> wallFaces;
};

struct CutSpec {
    std::vector<CutTool> tools;
    // Местный разрез: замкнутые границы областей в координатах вида — по ним строятся
    // участки волнистой линии (там, где стенки выреза пересекают модель).
    std::vector<std::vector<Point2>> boundaryLoops;
};

// Образы грани инструмента в результате булевой операции.
void imagesOf(BRepAlgoAPI_Cut& cut, const TopoDS_Shape& f, const ShapeSet& resultFaces,
              std::vector<TopoDS_Face>& out) {
    auto take = [&](const TopoDS_Shape& s) {
        const int i = resultFaces.FindIndex(s);
        if (i > 0) out.push_back(TopoDS::Face(resultFaces.FindKey(i)));
    };
    const ShapeList& mod = cut.Modified(f);
    if (mod.IsEmpty()) {
        take(f);
        return;
    }
    for (const TopoDS_Shape& s : mod)
        if (s.ShapeType() == TopAbs_FACE) take(s);
}

double dist2(Point2 a, Point2 b) { return (a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y); }

double pointSegment(Point2 p, Point2 a, Point2 b) {
    const double dx = b.x - a.x, dy = b.y - a.y;
    const double l2 = dx * dx + dy * dy;
    double t = l2 > 0 ? ((p.x - a.x) * dx + (p.y - a.y) * dy) / l2 : 0.0;
    t = std::clamp(t, 0.0, 1.0);
    return std::hypot(p.x - (a.x + t * dx), p.y - (a.y + t * dy));
}

// Участки замкнутой границы loop, лежащие на проекциях рёбер стенок (под ними есть материал).
std::vector<std::vector<Point2>> coveredRuns(const std::vector<Point2>& loop,
                                             const std::vector<std::vector<Point2>>& wall, double tol) {
    // Сгущение границы: шаг не больше h, вершины сохраняются.
    double perimeter = 0;
    for (std::size_t i = 0; i < loop.size(); ++i) perimeter += std::sqrt(dist2(loop[i], loop[(i + 1) % loop.size()]));
    const double h = std::min(0.5, perimeter / 400.0);
    std::vector<Point2> s;
    for (std::size_t i = 0; i < loop.size(); ++i) {
        const Point2 a = loop[i], b = loop[(i + 1) % loop.size()];
        const int n = std::max(1, int(std::ceil(std::sqrt(dist2(a, b)) / h)));
        for (int k = 0; k < n; ++k) s.push_back({a.x + (b.x - a.x) * k / n, a.y + (b.y - a.y) * k / n});
    }
    std::vector<char> cov(s.size(), 0);
    for (std::size_t k = 0; k < s.size(); ++k) {
        for (const auto& w : wall) {
            bool hit = false;
            if (w.size() == 1) hit = std::sqrt(dist2(w[0], s[k])) <= tol;
            for (std::size_t j = 1; j < w.size() && !hit; ++j) hit = pointSegment(s[k], w[j - 1], w[j]) <= tol;
            if (hit) {
                cov[k] = 1;
                break;
            }
        }
    }
    std::vector<std::vector<Point2>> runs;
    const std::size_t n = s.size();
    std::size_t start = n;
    for (std::size_t k = 0; k < n; ++k)
        if (!cov[k]) {
            start = k;
            break;
        }
    if (start == n) {  // вся граница над материалом — замкнутый участок
        std::vector<Point2> closed = s;
        closed.push_back(s.front());
        runs.push_back(std::move(closed));
        return runs;
    }
    std::vector<Point2> run;
    for (std::size_t m = 1; m <= n; ++m) {
        const std::size_t k = (start + m) % n;
        if (cov[k]) {
            run.push_back(s[k]);
        } else {
            if (run.size() >= 2) runs.push_back(run);
            run.clear();
        }
    }
    if (run.size() >= 2) runs.push_back(run);
    return runs;
}

bool segmentsCross(Point2 a, Point2 b, Point2 c, Point2 d) {
    auto orient = [](Point2 p, Point2 q, Point2 r) { return (q.x - p.x) * (r.y - p.y) - (q.y - p.y) * (r.x - p.x); };
    const double d1 = orient(c, d, a), d2 = orient(c, d, b), d3 = orient(a, b, c), d4 = orient(a, b, d);
    return ((d1 > 0 && d2 < 0) || (d1 < 0 && d2 > 0)) && ((d3 > 0 && d4 < 0) || (d3 < 0 && d4 > 0));
}

double signedArea(const std::vector<Point2>& p) {
    double a = 0;
    for (std::size_t i = 0; i < p.size(); ++i) {
        const Point2 u = p[i], v = p[(i + 1) % p.size()];
        a += u.x * v.y - v.x * u.y;
    }
    return 0.5 * a;
}

// Ребро/контур границы местного разреза в плоскости {x·toward = w}: ломаная или замкнутый
// интерполяционный сплайн.
Result<TopoDS_Wire> boundaryWire(const LocalBoundary& b, const ViewFrame& frame, double w) {
    using R = Result<TopoDS_Wire>;
    if (b.points.size() < 3) return R::failure(ErrorCode::InvalidInput, "local section: the boundary needs at least 3 points");
    for (const Point2& p : b.points)
        if (!std::isfinite(p.x) || !std::isfinite(p.y))
            return R::failure(ErrorCode::InvalidInput, "local section: boundary point is not finite");
    for (std::size_t i = 0; i < b.points.size(); ++i)
        if (std::sqrt(dist2(b.points[i], b.points[(i + 1) % b.points.size()])) < 1e-6)
            return R::failure(ErrorCode::InvalidInput, "local section: coincident boundary points");
    const Vec3 up = frame.up();
    auto to3 = [&](Point2 p) { return pnt(p.x * frame.right + p.y * up + w * frame.toward); };
    try {
        if (!b.spline) {
            BRepBuilderAPI_MakePolygon poly;
            for (const Point2& p : b.points) poly.Add(to3(p));
            poly.Close();
            if (!poly.IsDone()) return R::failure(ErrorCode::InvalidInput, "local section: cannot build the boundary polygon");
            return poly.Wire();
        }
        auto pts = new NCollection_HArray1<gp_Pnt>(1, int(b.points.size()));
        for (std::size_t i = 0; i < b.points.size(); ++i) pts->SetValue(int(i) + 1, to3(b.points[i]));
        occ::handle<NCollection_HArray1<gp_Pnt>> hp(pts);
        GeomAPI_Interpolate interp(hp, true, 1e-7);
        interp.Perform();
        if (!interp.IsDone()) return R::failure(ErrorCode::InvalidInput, "local section: spline interpolation failed");
        BRepBuilderAPI_MakeEdge me(interp.Curve());
        if (!me.IsDone()) return R::failure(ErrorCode::InvalidInput, "local section: cannot build the spline edge");
        BRepBuilderAPI_MakeWire mw(me.Edge());
        return mw.Wire();
    } catch (const Standard_Failure& f) {
        return R::failure(ErrorCode::InvalidInput, std::string("local section: boundary: ") + f.what());
    }
}

// Точки контура (координаты вида) и проверка: без самопересечений, ненулевая площадь.
Result<std::vector<Point2>> loopOf(const TopoDS_Wire& wire, const ViewFrame& frame, double deflection) {
    using R = Result<std::vector<Point2>>;
    std::vector<Point2> loop;
    for (BRepTools_WireExplorer we(wire); we.More(); we.Next()) {
        for (const Point2& p : detail::edgePoints(we.Current(), frame, deflection)) {
            if (!loop.empty() && std::sqrt(dist2(loop.back(), p)) < 1e-9) continue;
            loop.push_back(p);
        }
    }
    if (loop.size() > 1 && std::sqrt(dist2(loop.front(), loop.back())) < 1e-9) loop.pop_back();
    if (loop.size() < 3) return R::failure(ErrorCode::InvalidInput, "local section: degenerate boundary");
    const std::size_t n = loop.size();
    for (std::size_t i = 0; i < n; ++i)
        for (std::size_t j = i + 2; j < n; ++j) {
            if (i == 0 && j == n - 1) continue;
            if (segmentsCross(loop[i], loop[(i + 1) % n], loop[j], loop[(j + 1) % n]))
                return R::failure(ErrorCode::InvalidInput, "local section: the boundary intersects itself");
        }
    if (std::abs(signedArea(loop)) < 1e-9) return R::failure(ErrorCode::InvalidInput, "local section: the boundary has zero area");
    return loop;
}

Result<SectionView> cutAndProject(const kernel::Shape& shape, const ViewFrame& frame, const CutSpec& spec,
                                  SectionKind kind, const HlrOptions& options, const SectionParts& parts) {
    using R = Result<SectionView>;
    auto bbox = shape.boundingBox();
    if (!bbox) return bbox.error();
    auto index = kernel::TopologyIndex::build(shape);
    if (!index) return index.error();
    const kernel::TopologyIndex& idx = index.value();
    const auto& bb = bbox.value();
    const Vec3 lo{bb.min[0], bb.min[1], bb.min[2]}, hi{bb.max[0], bb.max[1], bb.max[2]};
    const double size = std::max(1.0, length(hi - lo));

    std::vector<TopoDS_Shape> children;
    if (parts.perChild && shape.occt().ShapeType() == TopAbs_COMPOUND) {
        for (TopoDS_Iterator it(shape.occt()); it.More(); it.Next()) children.push_back(it.Value());
    } else {
        children.push_back(shape.occt());
    }
    if (!parts.keepWhole.empty() && parts.keepWhole.size() != children.size())
        return R::failure(ErrorCode::InvalidInput, "section: keepWhole must have one flag per part (" +
                                                       std::to_string(children.size()) + ")");

    auto edgeMap = std::make_shared<ShapeToIndex>();
    auto faceMap = std::make_shared<ShapeToIndex>();
    auto genFace = std::make_shared<ShapeToIndex>();
    auto wallEdges = std::make_shared<ShapeSet>();
    auto sectionEdges = std::make_shared<ShapeSet>();
    struct SecFace {
        TopoDS_Face face;
        int owner;
    };
    std::vector<SecFace> secFaces;
    ShapeList toolList;
    for (const CutTool& t : spec.tools) toolList.Append(t.solid);

    try {
        BRep_Builder builder;
        TopoDS_Compound result;
        builder.MakeCompound(result);
        for (std::size_t ci = 0; ci < children.size(); ++ci) {
            const TopoDS_Shape& child = children[ci];
            const int owner = parts.perChild ? int(ci) : 0;
            if (!parts.keepWhole.empty() && parts.keepWhole[ci]) {
                // Не рассекается: изображается целиком, ссылки — тождественные.
                for (TopExp_Explorer ex(child, TopAbs_EDGE); ex.More(); ex.Next())
                    if (auto k = idx.edgeIndex(ex.Current()); k && !edgeMap->IsBound(ex.Current())) edgeMap->Bind(ex.Current(), *k);
                for (TopExp_Explorer ex(child, TopAbs_FACE); ex.More(); ex.Next())
                    if (auto k = idx.faceIndex(ex.Current()); k && !faceMap->IsBound(ex.Current())) faceMap->Bind(ex.Current(), *k);
                builder.Add(result, child);
                continue;
            }
            BRepAlgoAPI_Cut cut;
            ShapeList args;
            args.Append(child);
            cut.SetArguments(args);
            cut.SetTools(toolList);
            cut.Build();
            if (!cut.IsDone() || cut.HasErrors())
                return R::failure(ErrorCode::OperationFailed, "section: boolean cut failed");
            const TopoDS_Shape r = cut.Shape();
            auto bind = [&cut](ShapeToIndex& m, const TopoDS_Shape& orig, std::uint32_t i) {
                if (cut.IsDeleted(orig)) return;
                const ShapeList& mod = cut.Modified(orig);
                if (mod.IsEmpty()) {
                    if (!m.IsBound(orig)) m.Bind(orig, i);
                    return;
                }
                for (const TopoDS_Shape& s : mod)
                    if (!m.IsBound(s)) m.Bind(s, i);
            };
            ShapeSet childEdges, childFaces;
            TopExp::MapShapes(child, TopAbs_EDGE, childEdges);
            TopExp::MapShapes(child, TopAbs_FACE, childFaces);
            for (int i = 1; i <= childEdges.Extent(); ++i)
                if (auto k = idx.edgeIndex(childEdges(i))) bind(*edgeMap, childEdges(i), *k);
            for (int i = 1; i <= childFaces.Extent(); ++i) {
                auto k = idx.faceIndex(childFaces(i));
                if (!k) continue;
                bind(*faceMap, childFaces(i), *k);
                // Рёбра, порождённые пересечением грани модели с инструментом, — ссылка на грань.
                for (const TopoDS_Shape& g : cut.Generated(childFaces(i)))
                    if (g.ShapeType() == TopAbs_EDGE && !genFace->IsBound(g)) genFace->Bind(g, *k);
            }
            ShapeSet rfaces;
            TopExp::MapShapes(r, TopAbs_FACE, rfaces);
            for (const CutTool& t : spec.tools) {
                for (const TopoDS_Shape& sf : t.sectionFaces) {
                    std::vector<TopoDS_Face> img;
                    imagesOf(cut, sf, rfaces, img);
                    for (const TopoDS_Face& f : img) {
                        secFaces.push_back({f, owner});
                        TopExp::MapShapes(f, TopAbs_EDGE, *sectionEdges);
                    }
                }
                for (const TopoDS_Shape& wf : t.wallFaces) {
                    std::vector<TopoDS_Face> img;
                    imagesOf(cut, wf, rfaces, img);
                    for (const TopoDS_Face& f : img) TopExp::MapShapes(f, TopAbs_EDGE, *wallEdges);
                }
            }
            builder.Add(result, r);
        }
        if (secFaces.empty())
            return R::failure(ErrorCode::InvalidInput, "section: the cutting plane does not intersect the model");

        SectionView sv;
        sv.remaining = kernel::Shape(result);
        for (const SecFace& sf : secFaces) {
            GProp_GProps g;
            BRepGProp::SurfaceProperties(sf.face, g);
            sv.sectionArea += g.Mass();
            std::vector<std::vector<Point2>> loops;
            for (TopExp_Explorer wx(sf.face, TopAbs_WIRE); wx.More(); wx.Next()) {
                std::vector<Point2> loop;
                for (BRepTools_WireExplorer we(TopoDS::Wire(wx.Current()), sf.face); we.More(); we.Next()) {
                    for (const Point2& p : detail::edgePoints(we.Current(), frame, options.curveDeflection)) {
                        if (!loop.empty() && std::sqrt(dist2(loop.back(), p)) < 1e-9) continue;
                        loop.push_back(p);
                    }
                }
                if (loop.size() > 1 && std::sqrt(dist2(loop.front(), loop.back())) < 1e-9) loop.pop_back();
                if (loop.size() >= 3) loops.push_back(std::move(loop));
            }
            if (!loops.empty()) {
                sv.regions.push_back(std::move(loops));
                sv.regionOwner.push_back(sf.owner);
            }
        }

        detail::SourceMapper mapper;
        mapper.edge = [edgeMap](const TopoDS_Shape& s) -> std::optional<std::uint32_t> {
            if (const std::uint32_t* v = edgeMap->Seek(s)) return *v;
            return std::nullopt;
        };
        mapper.face = [faceMap](const TopoDS_Shape& s) -> std::optional<std::uint32_t> {
            if (const std::uint32_t* v = faceMap->Seek(s)) return *v;
            return std::nullopt;
        };
        mapper.edgeClass = [sectionEdges](const TopoDS_Edge& e) -> std::optional<EdgeClass> {
            if (sectionEdges->Contains(e)) return EdgeClass::SectionCut;
            return std::nullopt;
        };
        if (wallEdges->Extent() > 0)
            mapper.skip = [wallEdges](const TopoDS_Shape& s) { return wallEdges->Contains(s); };

        if (kind == SectionKind::Cut) {
            auto v = detail::runHlr(result, frame, options, mapper);
            if (!v) return v.error();
            sv.view = std::move(v.value());
        } else {
            sv.view.frame = frame;
            for (int i = 1; i <= sectionEdges->Extent(); ++i) {
                const TopoDS_Edge e = TopoDS::Edge((*sectionEdges)(i));
                if (BRep_Tool::Degenerated(e) || wallEdges->Contains(e)) continue;
                BRepAdaptor_Curve c(e);
                auto curve = detail::projectCurve(c, c.FirstParameter(), c.LastParameter(), frame, options.curveDeflection);
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
        // Сопоставление по геометрии: 2D-ребро лежит в габарите проекции порождённого ребра.
        const double t = 1e-6 * size;
        std::vector<std::pair<Box2, std::pair<std::size_t, std::uint32_t>>> gens;
        std::vector<Curve2> genCurves;
        for (ShapeToIndex::Iterator it(*genFace); it.More(); it.Next()) {
            const TopoDS_Edge ge = TopoDS::Edge(it.Key());
            BRepAdaptor_Curve gc(ge);
            auto gcurve = detail::projectCurve(gc, gc.FirstParameter(), gc.LastParameter(), frame, options.curveDeflection);
            if (!gcurve) continue;
            Box2 gb;
            detail::addCurveToBox(gb, *gcurve);
            gens.push_back({gb, {gcurve->index(), it.Value()}});
        }
        for (ViewEdge& ve : sv.view.edges) {
            if (ve.cls != EdgeClass::SectionCut || ve.faceIndex) continue;
            Box2 eb;
            detail::addCurveToBox(eb, ve.curve);
            for (const auto& [gb, info] : gens) {
                if (eb.minX >= gb.minX - t && eb.maxX <= gb.maxX + t && eb.minY >= gb.minY - t &&
                    eb.maxY <= gb.maxY + t && info.first == ve.curve.index()) {
                    ve.faceIndex = info.second;
                    break;
                }
            }
        }
        // Волнистая линия местного разреза: участки границы над материалом.
        if (!spec.boundaryLoops.empty()) {
            std::vector<std::vector<Point2>> wall;
            for (int i = 1; i <= wallEdges->Extent(); ++i) {
                auto pts = detail::edgePoints(TopoDS::Edge((*wallEdges)(i)), frame, options.curveDeflection);
                if (!pts.empty()) wall.push_back(std::move(pts));
            }
            const double tol = 0.02 + 1e-5 * size + 2.0 * options.curveDeflection;
            for (const auto& loop : spec.boundaryLoops)
                for (auto& run : coveredRuns(loop, wall, tol)) sv.breakLines.push_back(std::move(run));
        }
        return sv;
    } catch (const Standard_Failure& f) {
        return R::failure(ErrorCode::OperationFailed, std::string("section: OCCT failure: ") + f.what());
    }
}

// Ящик со стороны наблюдателя: начало — в секущей плоскости (грань ZMin), ось Z — к наблюдателю
// (−n), ось X — xv; размеры dx × dy × dz.
BRepPrimAPI_MakeBox viewerBox(Vec3 origin, Vec3 n, Vec3 xv, double dx, double dy, double dz) {
    const gp_Ax2 ax(pnt(origin), dir(-1.0 * n), dir(xv));
    BRepPrimAPI_MakeBox mb(ax, dx, dy, dz);
    mb.Build();
    return mb;
}

struct Extent {
    Vec3 lo, hi, center;
    double L = 0;
};

Result<Extent> extentOf(const kernel::Shape& shape, Vec3 extraPoint) {
    auto bbox = shape.boundingBox();
    if (!bbox) return bbox.error();
    const auto& bb = bbox.value();
    Extent e;
    e.lo = {bb.min[0], bb.min[1], bb.min[2]};
    e.hi = {bb.max[0], bb.max[1], bb.max[2]};
    e.center = 0.5 * (e.lo + e.hi);
    e.L = length(e.hi - e.lo) + length(e.center - extraPoint) + 10.0;
    return e;
}

Status checkSight(const ViewFrame& frame, Vec3 n) {
    if (std::abs(dot(n, frame.toward) + 1.0) > 1e-9)
        return Status::failure(ErrorCode::InvalidInput, "section: the view must look along the sight (toward = -sight)");
    return okStatus();
}

}  // namespace

Result<SectionView> sectionShape(const kernel::Shape& shape, const ViewFrame& frame, const CuttingPlane& plane,
                                 SectionKind kind, const HlrOptions& options, const SectionParts& parts) {
    using R = Result<SectionView>;
    if (shape.isNull()) return R::failure(ErrorCode::InvalidInput, "section: shape is null");
    if (!(length(plane.sight) > 1e-12) || !std::isfinite(length(plane.point)))
        return R::failure(ErrorCode::InvalidInput, "section: invalid cutting plane");
    const Vec3 n = unit3(plane.sight);
    if (auto st = checkSight(frame, n); !st) return st.error();
    auto ext = extentOf(shape, plane.point);
    if (!ext) return ext.error();
    const double L = ext.value().L;
    try {
        const Vec3 xv = std::abs(n.x) < 0.9 ? unit3(cross(n, Vec3{1, 0, 0})) : unit3(cross(n, Vec3{0, 1, 0}));
        const Vec3 yv = cross(-1.0 * n, xv);
        BRepPrimAPI_MakeBox mb = viewerBox(plane.point - L * xv - L * yv, n, xv, 2 * L, 2 * L, L);
        CutSpec spec;
        spec.tools.push_back({mb.Shape(), {mb.BottomFace()}, {}});
        return cutAndProject(shape, frame, spec, kind, options, parts);
    } catch (const Standard_Failure& f) {
        return R::failure(ErrorCode::OperationFailed, std::string("section: OCCT failure: ") + f.what());
    }
}

Result<SectionView> steppedSectionShape(const kernel::Shape& shape, const ViewFrame& frame,
                                        const std::vector<Vec3>& planePoints, Vec3 stepAxis, SectionKind kind,
                                        const HlrOptions& options, const SectionParts& parts) {
    using R = Result<SectionView>;
    if (shape.isNull()) return R::failure(ErrorCode::InvalidInput, "stepped section: shape is null");
    if (planePoints.size() < 2) return R::failure(ErrorCode::InvalidInput, "stepped section: needs at least two plane points");
    if (!(length(stepAxis) > 1e-12)) return R::failure(ErrorCode::InvalidInput, "stepped section: zero step axis");
    for (const Vec3& p : planePoints)
        if (!std::isfinite(length(p))) return R::failure(ErrorCode::InvalidInput, "stepped section: plane point is not finite");
    const Vec3 n = -1.0 * frame.toward;
    const Vec3 a = unit3(stepAxis);
    if (std::abs(dot(a, n)) > 1e-9)
        return R::failure(ErrorCode::InvalidInput, "stepped section: the step axis must be perpendicular to the sight");
    const Vec3 w = cross(n, a);
    std::vector<Vec3> pts = planePoints;
    std::sort(pts.begin(), pts.end(), [&](Vec3 p, Vec3 q) { return dot(p, a) < dot(q, a); });
    auto ext = extentOf(shape, pts.front());
    if (!ext) return ext.error();
    double L = ext.value().L;
    for (const Vec3& p : pts) L = std::max(L, length(p - ext.value().center) + length(ext.value().hi - ext.value().lo) + 10.0);
    const double tol = 1e-6 * std::max(1.0, length(ext.value().hi - ext.value().lo));
    for (std::size_t i = 1; i < pts.size(); ++i)
        if (dot(pts[i], a) - dot(pts[i - 1], a) <= tol)
            return R::failure(ErrorCode::InvalidInput, "stepped section: two plane points at the same position along the step axis");
    const double uc = dot(ext.value().center, a), wc = dot(ext.value().center, w);
    try {
        CutSpec spec;
        for (std::size_t i = 0; i < pts.size(); ++i) {
            const double ui = dot(pts[i], a);
            const double lowU = i == 0 ? std::min(uc, ui) - L : 0.5 * (dot(pts[i - 1], a) + ui);
            const double highU = i + 1 == pts.size() ? std::max(uc, ui) + L : 0.5 * (ui + dot(pts[i + 1], a));
            // Ось Y ящика — (−n) × a = −w: начало — на w = wc + L, ящик идёт до wc − L.
            const Vec3 origin = pts[i] + (lowU - ui) * a + ((wc + L) - dot(pts[i], w)) * w;
            BRepPrimAPI_MakeBox mb = viewerBox(origin, n, a, highU - lowU, 2 * L, L);
            CutTool tool{mb.Shape(), {mb.BottomFace()}, {}};
            if (i > 0) tool.wallFaces.push_back(mb.BackFace());               // XMin — уступ
            if (i + 1 < pts.size()) tool.wallFaces.push_back(mb.FrontFace());  // XMax — уступ
            spec.tools.push_back(std::move(tool));
        }
        return cutAndProject(shape, frame, spec, kind, options, parts);
    } catch (const Standard_Failure& f) {
        return R::failure(ErrorCode::OperationFailed, std::string("stepped section: OCCT failure: ") + f.what());
    }
}

Result<double> frontExtent(const kernel::Shape& shape, const ViewFrame& frame) {
    using R = Result<double>;
    if (shape.isNull()) return R::failure(ErrorCode::InvalidInput, "front extent: shape is null");
    try {
        gp_Trsf t;
        t.SetTransformation(gp_Ax3(gp_Pnt(0, 0, 0), dir(frame.toward), dir(frame.right)));
        BRepBuilderAPI_Transform tr(shape.occt(), t, false);
        Bnd_Box box;
        BRepBndLib::AddOptimal(tr.Shape(), box, false, false);
        if (box.IsVoid()) return R::failure(ErrorCode::InvalidInput, "front extent: empty shape");
        double x0, y0, z0, x1, y1, z1;
        box.Get(x0, y0, z0, x1, y1, z1);
        return z1;
    } catch (const Standard_Failure& f) {
        return R::failure(ErrorCode::OperationFailed, std::string("front extent: OCCT failure: ") + f.what());
    }
}

Result<std::vector<Point2>> localBoundaryLoop(const LocalBoundary& boundary, double deflection) {
    if (!(deflection > 0)) return Result<std::vector<Point2>>::failure(ErrorCode::InvalidInput, "local section: deflection must be positive");
    const ViewFrame f{{0, 0, 1}, {1, 0, 0}};
    auto wire = boundaryWire(boundary, f, 0.0);
    if (!wire) return wire.error();
    return loopOf(wire.value(), f, deflection);
}

Result<SectionView> localSectionShape(const kernel::Shape& shape, const ViewFrame& frame,
                                      const std::vector<LocalSection>& cuts, const HlrOptions& options,
                                      const SectionParts& parts) {
    using R = Result<SectionView>;
    if (shape.isNull()) return R::failure(ErrorCode::InvalidInput, "local section: shape is null");
    if (cuts.empty()) return R::failure(ErrorCode::InvalidInput, "local section: no regions");
    auto front = frontExtent(shape, frame);
    if (!front) return front.error();
    auto ext = extentOf(shape, Vec3{0, 0, 0});
    if (!ext) return ext.error();
    const double size = length(ext.value().hi - ext.value().lo);
    // Глубина модели вдоль взгляда: передняя и задняя точки (габарит в системе вида).
    ViewFrame back{-1.0 * frame.toward, frame.right};
    auto rear = frontExtent(shape, back);
    if (!rear) return rear.error();
    const double thickness = front.value() + rear.value();
    try {
        CutSpec spec;
        for (const LocalSection& c : cuts) {
            if (!(c.depth > 0) || !std::isfinite(c.depth))
                return R::failure(ErrorCode::InvalidInput, "local section: depth must be positive");
            if (c.depth >= thickness)
                return R::failure(ErrorCode::InvalidInput, "local section: depth is beyond the model");
            const double w = front.value() - c.depth;
            auto wire = boundaryWire(c.boundary, frame, w);
            if (!wire) return wire.error();
            auto loop = loopOf(wire.value(), frame, options.curveDeflection);
            if (!loop) return loop.error();
            BRepBuilderAPI_MakeFace mf(wire.value(), true);
            if (!mf.IsDone()) return R::failure(ErrorCode::InvalidInput, "local section: cannot build the region face");
            const double len = c.depth + size + 10.0;
            const Vec3 v = len * frame.toward;
            BRepPrimAPI_MakePrism prism(mf.Face(), gp_Vec(v.x, v.y, v.z));
            prism.Build();
            if (!prism.IsDone()) return R::failure(ErrorCode::OperationFailed, "local section: cannot build the cutting prism");
            CutTool tool;
            tool.solid = prism.Shape();
            tool.sectionFaces.push_back(prism.FirstShape());
            for (TopExp_Explorer ex(wire.value(), TopAbs_EDGE); ex.More(); ex.Next())
                for (const TopoDS_Shape& g : prism.Generated(ex.Current()))
                    if (g.ShapeType() == TopAbs_FACE) tool.wallFaces.push_back(g);
            spec.tools.push_back(std::move(tool));
            spec.boundaryLoops.push_back(std::move(loop).value());
        }
        auto sv = cutAndProject(shape, frame, spec, SectionKind::Cut, options, parts);
        if (!sv) {
            if (sv.error().message.find("does not intersect") != std::string::npos)
                return R::failure(ErrorCode::InvalidInput, "local section: the region does not cut the model");
            return sv.error();
        }
        return sv;
    } catch (const Standard_Failure& f) {
        return R::failure(ErrorCode::OperationFailed, std::string("local section: OCCT failure: ") + f.what());
    }
}

}  // namespace bendcore::drawing_v2::views
