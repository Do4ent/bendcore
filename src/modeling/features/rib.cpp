// Ребро жёсткости: открытый контур эскиза → тело заданной толщины до граней тела
// (docs/modeling/FEATURES.md, раздел «Ребро жёсткости»).

#include "feature_common.hpp"

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepClass3d_SolidClassifier.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRepGProp.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepTools_WireExplorer.hxx>
#include <BRep_Tool.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <GC_MakeArcOfCircle.hxx>
#include <GProp_GProps.hxx>
#include <Standard_Failure.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Vertex.hxx>
#include <gp_Circ.hxx>
#include <gp_Pln.hxx>

#include <cmath>

namespace bendcore::modeling {

using namespace detail;

namespace {

struct Contour {
    std::vector<TopoDS_Edge> edges;  // в порядке обхода
    TopoDS_Vertex first, last;
    gp_Pnt p0, p1;
    gp_Vec t0, t1;  // касательные в начале и в конце по ходу обхода
    gp_Pnt middle;  // точка в середине среднего ребра
};

// Касательная ребра в его начале (atEnd = false) или конце по ходу обхода (ориентация ребра учтена).
gp_Vec tangent(const TopoDS_Edge& e, bool atEnd) {
    BRepAdaptor_Curve c(e);
    const bool reversed = e.Orientation() == TopAbs_REVERSED;
    const double u = (atEnd != reversed) ? c.LastParameter() : c.FirstParameter();
    gp_Pnt p;
    gp_Vec d;
    c.D1(u, p, d);
    if (reversed) d.Reverse();
    if (d.Magnitude() > 0) d.Normalize();
    return d;
}

Result<Contour> readContour(const TopoDS_Wire& wire, const gp_Pln& plane, double tol) {
    if (wire.IsNull() || !TopExp_Explorer(wire, TopAbs_EDGE).More())
        return Error{ErrorCode::InvalidInput, "rib contour is empty"};
    Contour c;
    for (BRepTools_WireExplorer ex(wire); ex.More(); ex.Next()) {
        const TopoDS_Edge e = TopoDS::Edge(ex.Current());
        if (c.edges.empty()) c.first = ex.CurrentVertex();
        c.edges.push_back(e);
    }
    if (c.edges.empty()) return Error{ErrorCode::InvalidInput, "rib contour is not a connected wire"};
    {
        TopoDS_Vertex a, b;
        TopExp::Vertices(c.edges.back(), a, b, true);
        c.last = b;
    }
    if (c.first.IsNull() || c.last.IsNull()) return Error{ErrorCode::InvalidInput, "rib contour edge without vertices"};
    c.p0 = BRep_Tool::Pnt(c.first);
    c.p1 = BRep_Tool::Pnt(c.last);
    if (c.first.IsSame(c.last) || c.p0.Distance(c.p1) <= tol)
        return Error{ErrorCode::InvalidInput, "rib contour must be open (its ends coincide)"};
    for (const auto& e : c.edges) {
        BRepAdaptor_Curve bc(e);
        for (int k = 0; k <= 8; ++k) {
            const double u = bc.FirstParameter() + (bc.LastParameter() - bc.FirstParameter()) * k / 8.0;
            if (plane.Distance(bc.Value(u)) > tol)
                return Error{ErrorCode::InvalidInput, "rib contour does not lie in the sketch plane"};
        }
    }
    c.t0 = tangent(c.edges.front(), false);
    c.t1 = tangent(c.edges.back(), true);
    const TopoDS_Edge& mid = c.edges[c.edges.size() / 2];
    BRepAdaptor_Curve mc(mid);
    c.middle = mc.Value(0.5 * (mc.FirstParameter() + mc.LastParameter()));
    return c;
}

gp_Pnt centroid(const TopoDS_Shape& f) {
    GProp_GProps g;
    BRepGProp::SurfaceProperties(f, g);
    return g.CentreOfMass();
}

// Кусок разреза, содержащий точку q (классификатор тела).
std::optional<TopoDS_Shape> pieceAt(const TopoDS_Shape& pieces, const gp_Pnt& q, double tol) {
    for (const auto& s : solidsOf(pieces)) {
        BRepClass3d_SolidClassifier cls(s, q, tol);
        if (cls.State() == TopAbs_IN) return s;
    }
    return std::nullopt;
}

struct Candidate {
    TopoDS_Shape piece;
    History history;
    double volume = 0.0;
    std::vector<std::string> warnings;
};

void offsets(const RibParams& p, double& lo, double& hi) {
    switch (p.side) {
        case RibThickness::Symmetric: lo = -p.thickness / 2; hi = p.thickness / 2; break;
        case RibThickness::FirstSide: lo = 0.0; hi = p.thickness; break;
        case RibThickness::SecondSide: lo = -p.thickness; hi = 0.0; break;
    }
}

TopoDS_Edge segment(const TopoDS_Vertex& a, const TopoDS_Vertex& b) { return BRepBuilderAPI_MakeEdge(a, b).Edge(); }

// ParallelToSketch: область между продолженным контуром и далёкой параллелью хорды со стороны
// side, утолщённая по нормали эскиза и обрезанная телом.
Result<std::optional<Candidate>> parallelSide(const Contour& c, const gp_Ax3& placement, const TopoDS_Shape& target,
                                              const RibParams& p, double side, double L, double eps) {
    const gp_Dir N = placement.Direction();
    const gp_Vec chord(c.p0, c.p1);
    const gp_Dir n(gp_Vec(N).Crossed(chord));
    const gp_Vec off = gp_Vec(n) * (side * L);
    auto vtx = [](const gp_Pnt& q) { return BRepBuilderAPI_MakeVertex(q).Vertex(); };
    const TopoDS_Vertex e0 = vtx(c.p0.Translated(-c.t0 * L)), e1 = vtx(c.p1.Translated(c.t1 * L));
    const TopoDS_Vertex f0 = vtx(BRep_Tool::Pnt(e0).Translated(off)), f1 = vtx(BRep_Tool::Pnt(e1).Translated(off));
    BRepBuilderAPI_MakeWire mw;
    mw.Add(segment(e0, c.first));
    for (const auto& e : c.edges) mw.Add(e);
    mw.Add(segment(c.last, e1));
    mw.Add(segment(e1, f1));
    mw.Add(segment(f1, f0));
    mw.Add(segment(f0, e0));
    if (!mw.IsDone()) return Error{ErrorCode::OperationFailed, "rib region wire failed"};
    BRepBuilderAPI_MakeFace mf(gp_Pln(placement), mw.Wire(), true);
    if (!mf.IsDone() || !isValidShape(mf.Face()))
        return Error{ErrorCode::InvalidInput,
                     "rib region is self-intersecting: the contour must not fold back over its chord"};
    double lo = 0, hi = 0;
    offsets(p, lo, hi);
    gp_Trsf shift;
    shift.SetTranslation(gp_Vec(N) * lo);
    auto copy = transformCopy(mf.Face(), shift);
    if (!copy) return copy.error();
    BRepPrimAPI_MakePrism prism(copy.value().shape, gp_Vec(N) * (hi - lo), false, true);
    if (!prism.IsDone()) return Error{ErrorCode::OperationFailed, "rib slab prism failed"};
    const TopoDS_Shape slab = prism.Shape();
    History h = sweepHistory(prism, slab, copy.value());
    auto cut = booleanWithHistory(kernel::BooleanType::Cut, {slab}, {target}, true, false);
    if (!cut) return Error{cut.error().code, "rib trim: " + cut.error().message};
    const gp_Pnt q = c.middle.Translated(gp_Vec(n) * (side * eps) + gp_Vec(N) * (0.5 * (lo + hi)));
    auto piece = pieceAt(cut.value().shape, q, 1e-7);
    if (!piece) return std::optional<Candidate>();
    // Ограничено телом: кусок не доходит до дальних краёв области.
    double nlo, nhi, clo, chi;
    projectionRange(*piece, c.p0, gp_Dir(gp_Vec(n) * side), nlo, nhi);
    projectionRange(*piece, c.p0, gp_Dir(chord), clo, chi);
    if (nhi > 0.5 * L || clo < -0.5 * L || chi > chord.Magnitude() + 0.5 * L) return std::optional<Candidate>();
    mergeInto(h, cut.value().history);
    Candidate out{*piece, h, volumeOf(*piece), cut.value().warnings};
    return std::optional<Candidate>(std::move(out));
}

// Утолщённое в плоскости ребро контура: прямоугольник (отрезок) или кольцевой сектор (дуга).
Result<TopoDS_Face> thickEdge(const TopoDS_Edge& e, const gp_Ax3& placement, double lo, double hi) {
    const gp_Dir N = placement.Direction();
    BRepAdaptor_Curve bc(e);
    const bool reversed = e.Orientation() == TopAbs_REVERSED;
    const gp_Pnt a = bc.Value(reversed ? bc.LastParameter() : bc.FirstParameter());
    const gp_Pnt b = bc.Value(reversed ? bc.FirstParameter() : bc.LastParameter());
    if (bc.GetType() == GeomAbs_Line) {
        const gp_Vec d(a, b);
        const gp_Vec m = gp_Vec(N).Crossed(d).Normalized();
        BRepBuilderAPI_MakeWire mw;
        const gp_Pnt q[4] = {a.Translated(m * lo), b.Translated(m * lo), b.Translated(m * hi), a.Translated(m * hi)};
        for (int i = 0; i < 4; ++i) mw.Add(BRepBuilderAPI_MakeEdge(q[i], q[(i + 1) % 4]).Edge());
        BRepBuilderAPI_MakeFace mf(gp_Pln(placement), mw.Wire(), true);
        if (!mf.IsDone()) return Error{ErrorCode::OperationFailed, "rib wall face failed"};
        return mf.Face();
    }
    if (bc.GetType() == GeomAbs_Circle) {
        const gp_Circ circ = bc.Circle();
        const gp_Pnt mid = bc.Value(0.5 * (bc.FirstParameter() + bc.LastParameter()));
        const gp_Vec t = tangent(e, false);
        // m = N × касательная; вдоль m радиус растёт или убывает.
        const gp_Vec m = gp_Vec(N).Crossed(t);
        const double toward = m.Dot(gp_Vec(mid, circ.Location()));
        const double sgn = toward > 0 ? -1.0 : 1.0;  // радиус при смещении o: r + sgn·o
        const double r1 = circ.Radius() + sgn * lo, r2 = circ.Radius() + sgn * hi;
        if (!(std::min(r1, r2) > 1e-9)) return Error{ErrorCode::InvalidInput, "rib thickness exceeds the arc radius"};
        auto scaled = [&](const gp_Pnt& p, double r) {
            gp_Vec v(circ.Location(), p);
            v.Normalize();
            return circ.Location().Translated(v * r);
        };
        BRepBuilderAPI_MakeWire mw;
        gp_Circ c1 = circ, c2 = circ;
        c1.SetRadius(r1);
        c2.SetRadius(r2);
        mw.Add(BRepBuilderAPI_MakeEdge(GC_MakeArcOfCircle(scaled(a, r1), scaled(mid, r1), scaled(b, r1)).Value()).Edge());
        mw.Add(BRepBuilderAPI_MakeEdge(scaled(b, r1), scaled(b, r2)).Edge());
        mw.Add(BRepBuilderAPI_MakeEdge(GC_MakeArcOfCircle(scaled(b, r2), scaled(mid, r2), scaled(a, r2)).Value()).Edge());
        mw.Add(BRepBuilderAPI_MakeEdge(scaled(a, r2), scaled(a, r1)).Edge());
        if (!mw.IsDone()) return Error{ErrorCode::OperationFailed, "rib wall wire failed"};
        BRepBuilderAPI_MakeFace mf(gp_Pln(placement), mw.Wire(), true);
        if (!mf.IsDone()) return Error{ErrorCode::OperationFailed, "rib wall face failed"};
        return mf.Face();
    }
    return Error{ErrorCode::Unsupported, "rib perpendicular to the sketch supports lines and arcs only"};
}

// PerpendicularToSketch: утолщённый в плоскости контур выдавливается вдоль ±N и обрезается телом.
Result<std::optional<Candidate>> perpendicularSide(const Contour& c, const gp_Ax3& placement, const TopoDS_Shape& target,
                                                   const RibParams& p, double side, double L, double eps) {
    const gp_Dir N = placement.Direction();
    double lo = 0, hi = 0;
    offsets(p, lo, hi);
    std::vector<TopoDS_Shape> prisms;
    const gp_Vec along = gp_Vec(N) * (side * L);
    for (const auto& e : c.edges) {
        auto f = thickEdge(e, placement, lo, hi);
        if (!f) return f.error();
        BRepPrimAPI_MakePrism pr(f.value(), along, false, true);
        if (!pr.IsDone()) return Error{ErrorCode::OperationFailed, "rib wall prism failed"};
        prisms.push_back(pr.Shape());
    }
    // Стыки соседних рёбер (симметричная толщина): круг диаметра t в вершине.
    if (p.side == RibThickness::Symmetric) {
        for (std::size_t k = 0; k + 1 < c.edges.size(); ++k) {
            TopoDS_Vertex a, b;
            TopExp::Vertices(c.edges[k], a, b, true);
            const gp_Circ circ(gp_Ax2(BRep_Tool::Pnt(b), N), p.thickness / 2);
            BRepBuilderAPI_MakeFace mf(gp_Pln(placement), BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(circ).Edge()).Wire(), true);
            if (!mf.IsDone()) return Error{ErrorCode::OperationFailed, "rib joint face failed"};
            BRepPrimAPI_MakePrism pr(mf.Face(), along, false, true);
            prisms.push_back(pr.Shape());
        }
    }
    TopoDS_Shape wall = prisms.front();
    if (prisms.size() > 1) {
        std::vector<TopoDS_Shape> rest(prisms.begin() + 1, prisms.end());
        auto fused = booleanWithHistory(kernel::BooleanType::Fuse, {wall}, rest, false, false);
        if (!fused) return Error{fused.error().code, "rib wall union: " + fused.error().message};
        wall = fused.value().shape;
    }
    auto cut = booleanWithHistory(kernel::BooleanType::Cut, {wall}, {target}, false, false);
    if (!cut) return Error{cut.error().code, "rib trim: " + cut.error().message};
    // Точка внутри ребра у плоскости эскиза: середина контура, смещённая к середине толщины.
    gp_Vec m = gp_Vec(N).Crossed(tangent(c.edges[c.edges.size() / 2], false));
    if (m.Magnitude() > 0) m.Normalize();
    const gp_Pnt q = c.middle.Translated(gp_Vec(N) * (side * eps) + m * (0.5 * (lo + hi)));
    auto piece = pieceAt(cut.value().shape, q, 1e-7);
    if (!piece) return std::optional<Candidate>();
    double nlo, nhi;
    projectionRange(*piece, c.p0, gp_Dir(gp_Vec(N) * side), nlo, nhi);
    if (nhi > 0.5 * L) return std::optional<Candidate>();
    Candidate out{*piece, newHistory(), volumeOf(*piece), cut.value().warnings};
    return std::optional<Candidate>(std::move(out));
}

// Роли граней ребра жёсткости (до объединения с телом).
std::vector<std::pair<TopoDS_Shape, std::string>> ribRoles(const TopoDS_Shape& piece, const Contour& c,
                                                           const gp_Ax3& placement, const RibParams& p) {
    std::vector<std::pair<TopoDS_Shape, std::string>> roles;
    const gp_Dir N = placement.Direction();
    double lo = 0, hi = 0;
    offsets(p, lo, hi);
    const double tol = 1e-6 * std::max(1.0, p.thickness);
    for (TopExp_Explorer ex(piece, TopAbs_FACE); ex.More(); ex.Next()) {
        const TopoDS_Face& f = TopoDS::Face(ex.Current());
        BRepAdaptor_Surface s(f, false);
        const gp_Pnt g = centroid(f);
        const double h = gp_Vec(placement.Location(), g).Dot(gp_Vec(N));
        const bool parallel = s.GetType() == GeomAbs_Plane && s.Plane().Axis().Direction().IsParallel(N, 1e-9);
        if (p.direction == RibDirection::ParallelToSketch) {
            if (parallel && std::abs(h - hi) < tol + 1e-6) roles.emplace_back(f, "SideA");
            else if (parallel && std::abs(h - lo) < tol + 1e-6) roles.emplace_back(f, "SideB");
            continue;
        }
        if (parallel && std::abs(h) < 1e-6) {
            roles.emplace_back(f, "Top");
            continue;
        }
        if (parallel) continue;  // грани у тела исчезают при объединении
        // Стенка: ближайшее ребро контура и сторона относительно него.
        std::size_t best = 0;
        double bd = 1e300;
        gp_Pnt closest;
        const TopoDS_Vertex gv = BRepBuilderAPI_MakeVertex(g).Vertex();
        for (std::size_t k = 0; k < c.edges.size(); ++k) {
            BRepExtrema_DistShapeShape d(gv, c.edges[k]);
            if (d.IsDone() && d.NbSolution() > 0 && d.Value() < bd) {
                bd = d.Value();
                best = k;
                closest = d.PointOnShape2(1);
            }
        }
        const gp_Vec t = tangent(c.edges[best], false);
        gp_Vec m = gp_Vec(N).Crossed(t);
        gp_Vec w(closest, g);
        w -= gp_Vec(N) * w.Dot(gp_Vec(N));
        std::string sideName = "End";
        if (m.Magnitude() > 0 && w.Magnitude() > 1e-9) {
            m.Normalize();
            const double along = w.Dot(m);
            if (std::abs(along) > 0.25 * w.Magnitude()) sideName = along > 0 ? "A" : "B";
        }
        roles.emplace_back(f, "Wall.E" + std::to_string(best + 1) + "." + sideName);
    }
    return roles;
}

}  // namespace

Result<FeatureResult> rib(const TopoDS_Wire& contour, const gp_Ax3& placement, const TopoDS_Shape& target,
                          const RibParams& p) {
    if (std::string e = checkSolidInput(target, "target body"); !e.empty()) return Error{ErrorCode::InvalidInput, e};
    if (!(std::isfinite(p.thickness) && p.thickness > 0.0)) return Error{ErrorCode::InvalidInput, "rib thickness must be > 0"};
    try {
        const double diag = boundingDiagonal({target, contour});
        const double tol = 1e-6 * std::max(1.0, diag);
        auto c = readContour(contour, gp_Pln(placement), tol);
        if (!c) return c.error();
        const double L = 2.0 * diag + 10.0;
        const double eps = 1e-3 * std::max(1.0, std::min(diag, 10.0 * p.thickness));
        std::vector<double> sides;
        if (!p.flip || !*p.flip) sides.push_back(1.0);
        if (!p.flip || *p.flip) sides.push_back(-1.0);
        std::optional<Candidate> best;
        for (double side : sides) {
            auto cand = p.direction == RibDirection::ParallelToSketch
                            ? parallelSide(c.value(), placement, target, p, side, L, eps)
                            : perpendicularSide(c.value(), placement, target, p, side, L, eps);
            if (!cand) return cand.error();
            if (!cand.value()) continue;
            if (!best || cand.value()->volume < best->volume) best = std::move(cand.value());
        }
        if (!best) {
            return Error{ErrorCode::OperationFailed, p.flip ? "rib does not reach the body on the chosen side"
                                                            : "rib does not reach the body on either side"};
        }
        if (!(best->volume > 0.0)) return Error{ErrorCode::OperationFailed, "rib has no volume"};
        auto roles = ribRoles(best->piece, c.value(), placement, p);
        auto applied = applyTool(FeatureMode::Boss, target, best->piece, History(), best->warnings);
        if (!applied) return applied.error();
        FeatureResult out = std::move(applied).value();
        const History fuseHistory = out.history;
        ShapeMap res;
        TopExp::MapShapes(out.shape, res);
        for (const auto& [f, role] : roles) {
            if (res.Contains(f)) out.roles.emplace_back(f, role);
            for (const TopoDS_Shape& m : fuseHistory->Modified(f))
                if (res.Contains(m)) out.roles.emplace_back(m, role);
        }
        History full = copyHistory(best->history);
        mergeInto(full, fuseHistory);
        out.history = full;
        return out;
    } catch (const Standard_Failure& e) {
        return Error{ErrorCode::OperationFailed, occtMessage("rib", e.what())};
    }
}

}  // namespace bendcore::modeling
