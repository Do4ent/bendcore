// Расчёт видимых и невидимых линий: HLRBRep_Algo (точный) и HLRBRep_PolyAlgo (по
// триангуляции). Классификация рёбер повторяет HLRBRep_HLRToShape / HLRBRep_PolyHLRToShape,
// но каждое 2D-ребро сохраняет ссылку на ребро или грань исходной модели.

#include "views_internal.hpp"

#include <BRepAdaptor_Curve.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRep_Tool.hxx>
#include <GCPnts_TangentialDeflection.hxx>
#include <HLRAlgo_EdgeIterator.hxx>
#include <HLRAlgo_EdgeStatus.hxx>
#include <HLRAlgo_Projector.hxx>
#include <HLRBRep_Algo.hxx>
#include <HLRBRep_Data.hxx>
#include <HLRBRep_EdgeData.hxx>
#include <HLRBRep_FaceIterator.hxx>
#include <HLRBRep_PolyAlgo.hxx>
#include <HLRBRep_ShapeBounds.hxx>
#include <HLRTopoBRep_OutLiner.hxx>
#include <Standard_Failure.hxx>
#include <TopoDS.hxx>
#include <gp_Ax2.hxx>
#include <gp_Circ.hxx>

#include <chrono>
#include <cmath>
#include <numbers>

namespace bendcore::drawing_v2::views {

namespace detail {

namespace {

Vec3 toVec(const gp_Pnt& p) { return {p.X(), p.Y(), p.Z()}; }

double deg(double rad) { return rad * 180.0 / std::numbers::pi; }

double normAngle(double a) {
    const double twoPi = 2.0 * std::numbers::pi;
    a = std::fmod(a, twoPi);
    if (a < 0) a += twoPi;
    return a;
}

// Ломаная почти прямая — заменить отрезком.
bool nearlyStraight(const std::vector<Point2>& pts, double tol) {
    if (pts.size() < 2) return false;
    const Point2 a = pts.front(), b = pts.back();
    const double dx = b.x - a.x, dy = b.y - a.y;
    const double len = std::hypot(dx, dy);
    if (len < tol) return false;
    for (const Point2& p : pts) {
        const double d = std::abs((p.x - a.x) * dy - (p.y - a.y) * dx) / len;
        if (d > tol) return false;
    }
    // Проекции точек должны идти монотонно (ломаная не возвращается назад).
    double prev = -1e300;
    for (const Point2& p : pts) {
        const double t = ((p.x - a.x) * dx + (p.y - a.y) * dy) / len;
        if (t < prev - tol) return false;
        prev = t;
    }
    return true;
}

}  // namespace

void addCurveToBox(Box2& box, const Curve2& c) {
    if (auto* l = std::get_if<LineSeg>(&c)) {
        box.add(l->a);
        box.add(l->b);
    } else if (auto* ci = std::get_if<Circle>(&c)) {
        box.add({ci->center.x - ci->radius, ci->center.y - ci->radius});
        box.add({ci->center.x + ci->radius, ci->center.y + ci->radius});
    } else if (auto* a = std::get_if<Arc>(&c)) {
        const double s = a->startDeg * std::numbers::pi / 180.0;
        double sweep = std::fmod(a->endDeg - a->startDeg, 360.0);
        if (sweep <= 0) sweep += 360.0;
        const int n = 64;
        for (int i = 0; i <= n; ++i) {
            const double t = s + sweep * std::numbers::pi / 180.0 * i / n;
            box.add({a->center.x + a->radius * std::cos(t), a->center.y + a->radius * std::sin(t)});
        }
        // Точные экстремумы по осям.
        for (int k = 0; k < 4; ++k) {
            const double ak = k * 90.0;
            double rel = std::fmod(ak - a->startDeg, 360.0);
            if (rel < 0) rel += 360.0;
            if (rel <= sweep)
                box.add({a->center.x + a->radius * std::cos(ak * std::numbers::pi / 180.0),
                         a->center.y + a->radius * std::sin(ak * std::numbers::pi / 180.0)});
        }
    } else if (auto* p = std::get_if<Polyline>(&c)) {
        for (const Point2& q : p->points) box.add(q);
    }
}

Box2 boundsOf(const std::vector<ViewEdge>& edges) {
    Box2 b;
    for (const ViewEdge& e : edges) addCurveToBox(b, e.curve);
    return b;
}

std::optional<Curve2> projectCurve(const BRepAdaptor_Curve& curve, double u0, double u1,
                                   const ViewFrame& frame, double deflection) {
    if (!(u1 > u0)) return std::nullopt;
    const Point2 p0 = frame.project(toVec(curve.Value(u0)));
    const Point2 p1 = frame.project(toVec(curve.Value(u1)));
    const double tol = 1e-7;
    switch (curve.GetType()) {
        case GeomAbs_Line:
            if (std::hypot(p1.x - p0.x, p1.y - p0.y) < tol) return std::nullopt;
            return Curve2{LineSeg{p0, p1}};
        case GeomAbs_Circle: {
            const gp_Circ c = curve.Circle();
            const gp_Dir ax = c.Axis().Direction();
            const Vec3 axis{ax.X(), ax.Y(), ax.Z()};
            if (std::abs(std::abs(dot(axis, frame.toward)) - 1.0) < 1e-10) {
                const Point2 ctr = frame.project(toVec(c.Location()));
                const double r = c.Radius();
                if (u1 - u0 >= 2.0 * std::numbers::pi - 1e-9) return Curve2{Circle{ctr, r}};
                const Point2 pm = frame.project(toVec(curve.Value(0.5 * (u0 + u1))));
                const double a0 = normAngle(std::atan2(p0.y - ctr.y, p0.x - ctr.x));
                const double a1 = normAngle(std::atan2(p1.y - ctr.y, p1.x - ctr.x));
                const double am = normAngle(std::atan2(pm.y - ctr.y, pm.x - ctr.x));
                // Дуга против часовой стрелки от a0 до a1 содержит середину — иначе обратная.
                const double sweep = normAngle(a1 - a0);
                const double mid = normAngle(am - a0);
                if (mid <= sweep) return Curve2{Arc{ctr, r, deg(a0), deg(a0 + sweep)}};
                return Curve2{Arc{ctr, r, deg(a1), deg(a1 + normAngle(a0 - a1))}};
            }
            break;
        }
        default: break;
    }
    // Прочие кривые и окружности, видимые под углом, — ломаная по 3D-прогибу.
    std::vector<Point2> pts;
    GCPnts_TangentialDeflection sampler(curve, u0, u1, 0.1, deflection, 2);
    for (int i = 1; i <= sampler.NbPoints(); ++i) pts.push_back(frame.project(toVec(sampler.Value(i))));
    if (pts.size() < 2) {
        pts = {p0, p1};
    }
    if (nearlyStraight(pts, std::max(tol, deflection * 0.1))) {
        if (std::hypot(pts.back().x - pts.front().x, pts.back().y - pts.front().y) < tol)
            return std::nullopt;
        return Curve2{LineSeg{pts.front(), pts.back()}};
    }
    return Curve2{Polyline{std::move(pts), false}};
}

std::vector<Point2> edgePoints(const TopoDS_Edge& edge, const ViewFrame& frame, double deflection) {
    std::vector<Point2> pts;
    if (BRep_Tool::Degenerated(edge)) return pts;
    BRepAdaptor_Curve curve(edge);
    const double u0 = curve.FirstParameter(), u1 = curve.LastParameter();
    if (curve.GetType() == GeomAbs_Line) {
        pts = {frame.project(toVec(curve.Value(u0))), frame.project(toVec(curve.Value(u1)))};
    } else {
        GCPnts_TangentialDeflection sampler(curve, u0, u1, 0.05, deflection, 4);
        for (int i = 1; i <= sampler.NbPoints(); ++i)
            pts.push_back(frame.project(toVec(sampler.Value(i))));
    }
    if (edge.Orientation() == TopAbs_REVERSED) std::reverse(pts.begin(), pts.end());
    return pts;
}

SourceMapper identityMapper(const kernel::TopologyIndex& index) {
    SourceMapper m;
    m.edge = [&index](const TopoDS_Shape& s) { return index.edgeIndex(s); };
    m.face = [&index](const TopoDS_Shape& s) { return index.faceIndex(s); };
    return m;
}

namespace {

void emitPiece(std::vector<ViewEdge>& out, const BRepAdaptor_Curve& curve, double u0, double u1,
               Visibility vis, EdgeClass cls, std::optional<std::uint32_t> edgeIdx,
               std::optional<std::uint32_t> faceIdx, const ViewFrame& frame, double deflection) {
    auto c = projectCurve(curve, u0, u1, frame, deflection);
    if (!c) return;
    ViewEdge e;
    e.curve = std::move(*c);
    e.visibility = vis;
    e.cls = cls;
    e.edgeIndex = edgeIdx;
    e.faceIndex = faceIdx;
    out.push_back(std::move(e));
}

bool wanted(const HlrOptions& o, Visibility v, EdgeClass cls) {
    if (v == Visibility::Hidden && !o.hidden) return false;
    if (cls == EdgeClass::Smooth) return v == Visibility::Visible ? o.smooth : o.hiddenSmooth;
    return true;
}

Status runExact(const TopoDS_Shape& shape, const gp_Ax2& ax, const ViewFrame& frame,
                const HlrOptions& options, const SourceMapper& mapper,
                std::vector<ViewEdge>& out) {
    occ::handle<HLRBRep_Algo> algo = new HLRBRep_Algo();
    algo->Add(shape, 0);
    algo->Projector(HLRAlgo_Projector(ax));
    algo->Update();
    algo->Hide();
    occ::handle<HLRBRep_Data> ds = algo->DataStructure();
    if (ds.IsNull() || algo->NbShapes() < 1) return okStatus();
    HLRTopoBRep_Data& tds = algo->ShapeBounds(1).Shape()->DataStructure();

    const int ne = ds->NbEdges();
    const int nf = ds->NbFaces();
    std::vector<char> outline(ne + 1, 0), iso(ne + 1, 0);
    for (int f = 1; f <= nf; ++f) {
        HLRBRep_FaceIterator it;
        for (it.InitEdge(ds->FDataArray().ChangeValue(f)); it.MoreEdge(); it.NextEdge()) {
            const int ie = it.Edge();
            if (ie < 1 || ie > ne) continue;
            if (it.IsoLine()) iso[ie] = 1;
            if (it.OutLine()) outline[ie] = 1;
        }
    }

    for (int ie = 1; ie <= ne; ++ie) {
        HLRBRep_EdgeData& ed = ds->EDataArray().ChangeValue(ie);
        if (!ed.Selected() || ed.Vertical() || iso[ie]) continue;
        const TopoDS_Shape& hs = ds->EdgeMap().FindKey(ie);
        const TopoDS_Shape old = tds.NewSOldS(hs);
        EdgeClass cls = EdgeClass::Sharp;
        std::optional<std::uint32_t> edgeIdx, faceIdx;
        if (old.ShapeType() == TopAbs_FACE) {
            cls = EdgeClass::Silhouette;  // очерк, построенный HLRTopoBRep_OutLiner на грани
            faceIdx = mapper.face(old);
        } else {
            if (ed.RgNLine()) {
                if (!outline[ie]) continue;  // шов замкнутой поверхности не изображается
                cls = EdgeClass::Silhouette;
            } else if (ed.Rg1Line()) {
                cls = outline[ie] ? EdgeClass::Silhouette : EdgeClass::Smooth;
            }
            if (old.ShapeType() == TopAbs_EDGE) {
                edgeIdx = mapper.edge(old);
                if (mapper.edgeClass)
                    if (auto o = mapper.edgeClass(TopoDS::Edge(old))) cls = *o;
            }
        }
        const BRepAdaptor_Curve& curve = ed.Geometry().GetCurve();
        double sta, end;
        float tsta, tend;
        HLRAlgo_EdgeIterator it;
        if (wanted(options, Visibility::Visible, cls))
            for (it.InitVisible(ed.Status()); it.MoreVisible(); it.NextVisible()) {
                it.Visible(sta, tsta, end, tend);
                emitPiece(out, curve, sta, end, Visibility::Visible, cls, edgeIdx, faceIdx, frame,
                          options.curveDeflection);
            }
        if (wanted(options, Visibility::Hidden, cls))
            for (it.InitHidden(ed.Status()); it.MoreHidden(); it.NextHidden()) {
                it.Hidden(sta, tsta, end, tend);
                emitPiece(out, curve, sta, end, Visibility::Hidden, cls, edgeIdx, faceIdx, frame,
                          options.curveDeflection);
            }
    }
    return okStatus();
}

Status runPoly(const TopoDS_Shape& shape, const gp_Ax2& ax, const HlrOptions& options,
               const SourceMapper& mapper, std::vector<ViewEdge>& out) {
    BRepMesh_IncrementalMesh mesh(shape, options.linearDeflection, false,
                                  options.angularDeflection, true);
    if (!mesh.IsDone())
        return Status::failure(ErrorCode::OperationFailed, "HLR (polygonal): meshing failed");
    occ::handle<HLRBRep_PolyAlgo> algo = new HLRBRep_PolyAlgo();
    algo->Load(shape);
    algo->Projector(HLRAlgo_Projector(ax));
    algo->Update();
    const gp_Trsf& T = algo->Projector().Transformation();
    HLRAlgo_EdgeStatus status;
    TopoDS_Shape S;
    bool reg1 = false, regn = false, outl = false, intl = false;
    for (algo->InitHide(); algo->MoreHide(); algo->NextHide()) {
        HLRAlgo_BiPoint::PointsT& pts = algo->Hide(status, S, reg1, regn, outl, intl);
        gp_XYZ a = pts.Pnt1, b = pts.Pnt2;
        T.Transforms(a);
        T.Transforms(b);
        const double dx = b.X() - a.X(), dy = b.Y() - a.Y();
        if (std::hypot(dx, dy) < 1e-10) continue;
        EdgeClass cls = EdgeClass::Sharp;
        std::optional<std::uint32_t> edgeIdx, faceIdx;
        if (intl) {
            cls = EdgeClass::Silhouette;
            faceIdx = mapper.face(S);
        } else {
            if (regn && !outl) continue;
            if (reg1) cls = outl ? EdgeClass::Silhouette : EdgeClass::Smooth;
            edgeIdx = mapper.edge(S);
            if (mapper.edgeClass && S.ShapeType() == TopAbs_EDGE)
                if (auto o = mapper.edgeClass(TopoDS::Edge(S))) cls = *o;
        }
        HLRAlgo_EdgeIterator it;
        double sta, end;
        float tsta, tend;
        auto emit = [&](Visibility v) {
            if (!wanted(options, v, cls)) return;
            ViewEdge e;
            e.curve = LineSeg{{a.X() + sta * dx, a.Y() + sta * dy}, {a.X() + end * dx, a.Y() + end * dy}};
            e.visibility = v;
            e.cls = cls;
            e.edgeIndex = edgeIdx;
            e.faceIndex = faceIdx;
            out.push_back(std::move(e));
        };
        for (it.InitVisible(status); it.MoreVisible(); it.NextVisible()) {
            it.Visible(sta, tsta, end, tend);
            if (end > sta) emit(Visibility::Visible);
        }
        for (it.InitHidden(status); it.MoreHidden(); it.NextHidden()) {
            it.Hidden(sta, tsta, end, tend);
            if (end > sta) emit(Visibility::Hidden);
        }
    }
    return okStatus();
}

}  // namespace

Result<ProjectedView> runHlr(const TopoDS_Shape& shape, const ViewFrame& frame,
                             const HlrOptions& options, const SourceMapper& mapper) {
    if (shape.IsNull())
        return Result<ProjectedView>::failure(ErrorCode::InvalidInput, "HLR: shape is null");
    if (!(options.curveDeflection > 0) ||
        (options.method == HlrMethod::Polygonal &&
         (!(options.linearDeflection > 0) || !(options.angularDeflection > 0))))
        return Result<ProjectedView>::failure(ErrorCode::InvalidInput,
                                              "HLR: deflections must be positive");
    const Vec3 n = frame.toward, r = frame.right;
    const gp_Ax2 ax(gp_Pnt(0, 0, 0), gp_Dir(n.x, n.y, n.z), gp_Dir(r.x, r.y, r.z));
    ProjectedView view;
    view.frame = frame;
    try {
        const auto t0 = std::chrono::steady_clock::now();
        Status st = options.method == HlrMethod::Exact
                        ? runExact(shape, ax, frame, options, mapper, view.edges)
                        : runPoly(shape, ax, options, mapper, view.edges);
        if (!st) return st.error();
        const auto t1 = std::chrono::steady_clock::now();
        view.stats.milliseconds = std::chrono::duration<double, std::milli>(t1 - t0).count();
    } catch (const Standard_Failure& f) {
        return Result<ProjectedView>::failure(
            ErrorCode::OperationFailed, std::string("HLR: OCCT failure: ") + f.what());
    } catch (const std::exception& e) {
        return Result<ProjectedView>::failure(ErrorCode::OperationFailed,
                                              std::string("HLR: ") + e.what());
    }
    view.stats.rawPieces = view.edges.size();
    removeOverlaps(view.edges);
    if (options.method == HlrMethod::Polygonal) chainSegments(view.edges);
    view.bounds = boundsOf(view.edges);
    return view;
}

}  // namespace detail

Result<ProjectedView> projectShape(const kernel::Shape& shape, const ViewFrame& frame,
                                   const HlrOptions& options) {
    if (shape.isNull())
        return Result<ProjectedView>::failure(ErrorCode::InvalidInput, "projectShape: shape is null");
    auto index = kernel::TopologyIndex::build(shape);
    if (!index) return index.error();
    return detail::runHlr(shape.occt(), frame, options, detail::identityMapper(index.value()));
}

double curveLength(const Curve2& c) {
    if (auto* l = std::get_if<LineSeg>(&c)) return std::hypot(l->b.x - l->a.x, l->b.y - l->a.y);
    if (auto* ci = std::get_if<Circle>(&c)) return 2.0 * std::numbers::pi * ci->radius;
    if (auto* a = std::get_if<Arc>(&c)) {
        double sweep = std::fmod(a->endDeg - a->startDeg, 360.0);
        if (sweep <= 0) sweep += 360.0;
        return a->radius * sweep * std::numbers::pi / 180.0;
    }
    const auto& p = std::get<Polyline>(c);
    double s = 0;
    for (std::size_t i = 1; i < p.points.size(); ++i)
        s += std::hypot(p.points[i].x - p.points[i - 1].x, p.points[i].y - p.points[i - 1].y);
    if (p.closed && p.points.size() > 2)
        s += std::hypot(p.points.front().x - p.points.back().x, p.points.front().y - p.points.back().y);
    return s;
}

}  // namespace bendcore::drawing_v2::views
