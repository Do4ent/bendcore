#include <bendcore/kernel/healing.hpp>

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <BRepTools.hxx>
#include <BRepTools_Modification.hxx>
#include <BRepTools_Modifier.hxx>
#include <BRepTopAdaptor_FClass2d.hxx>
#include <BRep_Tool.hxx>
#include <GProp_GProps.hxx>
#include <Geom2dAPI_Interpolate.hxx>
#include <Geom2d_BSplineCurve.hxx>
#include <Geom2d_Line.hxx>
#include <GeomAPI_ProjectPointOnCurve.hxx>
#include <GeomAPI_ProjectPointOnSurf.hxx>
#include <GeomConvert_CurveToAnaCurve.hxx>
#include <GeomConvert_SurfToAnaSurf.hxx>
#include <GeomLProp_SLProps.hxx>
#include <GeomProjLib.hxx>
#include <NCollection_IndexedDataMap.hxx>
#include <Geom_BSplineCurve.hxx>
#include <Geom_BSplineSurface.hxx>
#include <Geom_BezierCurve.hxx>
#include <Geom_BezierSurface.hxx>
#include <Geom_ElementarySurface.hxx>
#include <Geom_RectangularTrimmedSurface.hxx>
#include <Geom_Surface.hxx>
#include <Geom_TrimmedCurve.hxx>
#include <NCollection_Array1.hxx>
#include <NCollection_HArray1.hxx>
#include <NCollection_IndexedMap.hxx>
#include <Precision.hxx>
#include <ShapeAnalysis_Surface.hxx>
#include <ShapeCustom.hxx>
#include <ShapeFix_FixSmallFace.hxx>
#include <ShapeFix_Shape.hxx>
#include <ShapeFix_Wireframe.hxx>
#include <ShapeUpgrade_UnifySameDomain.hxx>
#include <Standard_Failure.hxx>
#include <TopAbs.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Vertex.hxx>
#include <gp_Pnt2d.hxx>
#include <gp_Vec2d.hxx>

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <sstream>
#include <unordered_map>

namespace bendcore::kernel {

namespace {

using ShapeMap = NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher>;

int countUnique(const TopoDS_Shape& shape, TopAbs_ShapeEnum type) {
    ShapeMap map;
    TopExp::MapShapes(shape, type, map);
    return map.Extent();
}

const char* surfaceTypeName(GeomAbs_SurfaceType type) {
    switch (type) {
        case GeomAbs_Plane: return "Plane";
        case GeomAbs_Cylinder: return "Cylinder";
        case GeomAbs_Cone: return "Cone";
        case GeomAbs_Sphere: return "Sphere";
        case GeomAbs_Torus: return "Torus";
        case GeomAbs_BezierSurface: return "BezierSurface";
        case GeomAbs_BSplineSurface: return "BSplineSurface";
        case GeomAbs_SurfaceOfRevolution: return "SurfaceOfRevolution";
        case GeomAbs_SurfaceOfExtrusion: return "SurfaceOfExtrusion";
        case GeomAbs_OffsetSurface: return "OffsetSurface";
        case GeomAbs_OtherSurface: return "OtherSurface";
    }
    return "Unknown";
}

const char* curveTypeName(GeomAbs_CurveType type) {
    switch (type) {
        case GeomAbs_Line: return "Line";
        case GeomAbs_Circle: return "Circle";
        case GeomAbs_Ellipse: return "Ellipse";
        case GeomAbs_Hyperbola: return "Hyperbola";
        case GeomAbs_Parabola: return "Parabola";
        case GeomAbs_BezierCurve: return "BezierCurve";
        case GeomAbs_BSplineCurve: return "BSplineCurve";
        case GeomAbs_OffsetCurve: return "OffsetCurve";
        case GeomAbs_OtherCurve: return "OtherCurve";
    }
    return "Unknown";
}

bool hasSolids(const TopoDS_Shape& shape) {
    return TopExp_Explorer(shape, TopAbs_SOLID).More();
}

double volumeOf(const TopoDS_Shape& shape) {
    if (!hasSolids(shape)) return 0.0;
    GProp_GProps props;
    // Adaptive integration: the fixed Gauss rule is off by ~1e-4 relative on rational B-splines.
    BRepGProp::VolumeProperties(shape, props, 1e-9);
    return props.Mass();
}

double maxToleranceOf(const TopoDS_Shape& shape) {
    double tol = 0.0;
    for (TopExp_Explorer ex(shape, TopAbs_FACE); ex.More(); ex.Next())
        tol = std::max(tol, BRep_Tool::Tolerance(TopoDS::Face(ex.Current())));
    for (TopExp_Explorer ex(shape, TopAbs_EDGE); ex.More(); ex.Next())
        tol = std::max(tol, BRep_Tool::Tolerance(TopoDS::Edge(ex.Current())));
    for (TopExp_Explorer ex(shape, TopAbs_VERTEX); ex.More(); ex.Next())
        tol = std::max(tol, BRep_Tool::Tolerance(TopoDS::Vertex(ex.Current())));
    return tol;
}

bool isValid(const TopoDS_Shape& shape) {
    return !shape.IsNull() && BRepCheck_Analyzer(shape).IsValid();
}

int countIndirectFaces(const TopoDS_Shape& shape) {
    ShapeMap faces;
    TopExp::MapShapes(shape, TopAbs_FACE, faces);
    int n = 0;
    for (int i = 1; i <= faces.Extent(); ++i) {
        occ::handle<Geom_Surface> s = BRep_Tool::Surface(TopoDS::Face(faces(i)));
        if (auto trimmed = occ::down_cast<Geom_RectangularTrimmedSurface>(s)) s = trimmed->BasisSurface();
        auto elem = occ::down_cast<Geom_ElementarySurface>(s);
        if (!elem.IsNull() && !elem->Position().Direct()) ++n;
    }
    return n;
}

occ::handle<Geom_Surface> unwrapSurface(occ::handle<Geom_Surface> s) {
    while (auto trimmed = occ::down_cast<Geom_RectangularTrimmedSurface>(s)) s = trimmed->BasisSurface();
    return s;
}

occ::handle<Geom_Curve> unwrapCurve(occ::handle<Geom_Curve> c) {
    while (auto trimmed = occ::down_cast<Geom_TrimmedCurve>(c)) c = trimmed->BasisCurve();
    return c;
}

bool isFreeFormSurface(const occ::handle<Geom_Surface>& s) {
    return s->IsKind(STANDARD_TYPE(Geom_BSplineSurface)) || s->IsKind(STANDARD_TYPE(Geom_BezierSurface));
}

bool isFreeFormCurve(const occ::handle<Geom_Curve>& c) {
    return c->IsKind(STANDARD_TYPE(Geom_BSplineCurve)) || c->IsKind(STANDARD_TYPE(Geom_BezierCurve));
}

// Shift x by whole periods into [lo, lo + period).
double normalizePeriodic(double x, double lo, double period) {
    return x - std::floor((x - lo) / period) * period;
}

// BRepTools_Modification that replaces free-form (B-spline / Bezier) surfaces and 3D curves by
// analytic ones found by GeomConvert_SurfToAnaSurf / GeomConvert_CurveToAnaCurve within a
// tolerance, and recomputes every pcurve affected by a new surface or a reparametrized curve.
class AnalyticModification final : public BRepTools_Modification {
public:
    AnalyticModification(const TopoDS_Shape& shape, double tolerance) : tol_(tolerance) {
        TopExp::MapShapesAndAncestors(shape, TopAbs_EDGE, TopAbs_FACE, edgeFaces_);
    }

    int surfacesConverted() const {
        int n = 0;
        for (const auto& [key, info] : surfaces_) n += info.has_value();
        return n;
    }
    int curvesConverted() const {
        int n = 0;
        for (const auto& [key, info] : curves_) n += info.has_value();
        return n;
    }
    double maxDeviation() const { return maxDeviation_; }

    bool NewSurface(const TopoDS_Face& F, occ::handle<Geom_Surface>& S, TopLoc_Location& L, double& Tol,
                    bool& RevWires, bool& RevFace) override {
        const SurfaceInfo* info = surfaceInfo(F);
        if (!info) return false;
        S = info->surface;
        L = info->location;
        Tol = std::max(BRep_Tool::Tolerance(F), info->deviation);
        RevWires = false;
        RevFace = false;
        return true;
    }

    bool NewCurve(const TopoDS_Edge& E, occ::handle<Geom_Curve>& C, TopLoc_Location& L, double& Tol) override {
        const CurveInfo* info = curveInfo(E);
        if (!info) return false;
        C = info->curve;
        L = info->location;
        Tol = std::max(BRep_Tool::Tolerance(E), info->deviation);
        return true;
    }

    bool NewPoint(const TopoDS_Vertex&, gp_Pnt&, double&) override { return false; }

    bool NewParameter(const TopoDS_Vertex& V, const TopoDS_Edge& E, double& P, double& Tol) override {
        const CurveInfo* info = curveInfo(E);
        if (!info) return false;
        const gp_Pnt pnt = BRep_Tool::Pnt(V);
        const gp_Pnt pf = info->globalCurve->Value(info->first);
        const gp_Pnt pl = info->globalCurve->Value(info->last);
        const double df = pnt.Distance(pf);
        const double dl = pnt.Distance(pl);
        if (info->closed) {
            // Both ends coincide: the vertex orientation relative to the forward edge tells which
            // end it is (the modifier passes it composed with the edge orientation).
            TopAbs_Orientation rel = V.Orientation();
            if (E.Orientation() == TopAbs_REVERSED) rel = TopAbs::Reverse(rel);
            P = (rel == TopAbs_REVERSED) ? info->last : info->first;
            Tol = std::max(BRep_Tool::Tolerance(V), df);
        } else {
            P = (df <= dl) ? info->first : info->last;
            Tol = std::max(BRep_Tool::Tolerance(V), std::min(df, dl));
        }
        return true;
    }

    bool NewCurve2d(const TopoDS_Edge& E, const TopoDS_Face& F, const TopoDS_Edge&, const TopoDS_Face&,
                    occ::handle<Geom2d_Curve>& C, double& Tol) override {
        const SurfaceInfo* sInfo = surfaceInfo(F);
        const CurveInfo* cInfo = curveInfo(E);
        if (!sInfo && !cInfo) return false;
        FaceContext& ctx = faceContext(F, sInfo);
        double t0 = 0.0, t1 = 0.0;
        occ::handle<Geom2d_Curve> oldPc = BRep_Tool::CurveOnSurface(E, F, t0, t1);
        if (oldPc.IsNull()) return false;

        const bool degenerated = BRep_Tool::Degenerated(E);
        double a = t0, b = t1;
        occ::handle<Geom_Curve> curve3d;  // global
        if (!degenerated) {
            if (cInfo) {
                curve3d = cInfo->globalCurve;
                a = cInfo->first;
                b = cInfo->last;
            } else {
                double f = 0.0, l = 0.0;
                curve3d = BRep_Tool::Curve(E, f, l);
                if (curve3d.IsNull()) return false;
                a = f;
                b = l;
            }
        }

        // Point slightly inside the face next to the middle of the old pcurve: it tells on which
        // side of a periodic seam the new pcurve must lie.
        const double tm = 0.5 * (t0 + t1);
        gp_Pnt2d uvMid;
        gp_Vec2d d1;
        oldPc->D1(tm, uvMid, d1);
        gp_Vec2d inside(0.0, 0.0);
        if (d1.Magnitude() > gp::Resolution()) {
            gp_Vec2d n(-d1.Y(), d1.X());
            n.Normalize();
            const double eps = 1e-3 * ctx.oldUVSize;
            for (double sign : {1.0, -1.0}) {
                gp_Vec2d off = n * (sign * eps);
                if (ctx.classifier->Perform(gp_Pnt2d(uvMid.XY() + off.XY()), false) == TopAbs_IN) {
                    inside = off;
                    break;
                }
            }
        }
        gp_Pnt2d refNew = ctx.sas->ValueOfUV(ctx.oldSurface->Value(uvMid.X() + inside.X(), uvMid.Y() + inside.Y()),
                                             Precision::Confusion());
        refNew = normalizeToWindow(ctx, refNew);

        if (!degenerated && ctx.newSurface->IsKind(STANDARD_TYPE(Geom_ElementarySurface))) {
            // Exact analytic pcurve (ProjLib) shifted by whole periods next to the reference point.
            occ::handle<Geom2d_Curve> pc = GeomProjLib::Curve2d(curve3d, a, b, ctx.newSurface);
            if (!pc.IsNull()) {
                const gp_Pnt2d m = pc->Value(0.5 * (a + b));
                gp_Vec2d shift(0.0, 0.0);
                if (ctx.newSurface->IsUPeriodic()) {
                    const double T = ctx.newSurface->UPeriod();
                    shift.SetX(std::round((refNew.X() - m.X()) / T) * T);
                }
                if (ctx.newSurface->IsVPeriodic()) {
                    const double T = ctx.newSurface->VPeriod();
                    shift.SetY(std::round((refNew.Y() - m.Y()) / T) * T);
                }
                if (shift.Magnitude() > 0.0) pc = occ::down_cast<Geom2d_Curve>(pc->Translated(shift));
                const double deviation = pcurveDeviation(pc, curve3d, ctx.newSurface, a, b);
                if (deviation <= 10.0 * std::max(tol_, BRep_Tool::Tolerance(E))) {
                    maxDeviation_ = std::max(maxDeviation_, deviation);
                    C = pc;
                    Tol = std::max(BRep_Tool::Tolerance(E), deviation);
                    return true;
                }
            }
        }

        constexpr int N = 48;
        std::vector<double> params(N + 1);
        std::vector<gp_Pnt2d> uv(N + 1);
        std::vector<bool> singular(N + 1, false);
        for (int i = 0; i <= N; ++i) {
            params[i] = a + (b - a) * double(i) / N;
            gp_Pnt p3d;
            if (degenerated) {
                const gp_Pnt2d uvOld = oldPc->Value(params[i]);
                p3d = ctx.oldSurface->Value(uvOld.X() + inside.X(), uvOld.Y() + inside.Y());
            } else {
                p3d = curve3d->Value(params[i]);
                singular[i] = ctx.sas->IsDegenerated(p3d, Precision::Confusion());
            }
            uv[i] = ctx.sas->ValueOfUV(p3d, Precision::Confusion());
        }
        repairSingularSamples(params, uv, singular);
        unwrap(ctx, uv);
        // Shift the whole pcurve by periods so that it lies next to the reference point.
        const gp_Pnt2d mid = uv[N / 2];
        double shiftU = 0.0, shiftV = 0.0;
        if (ctx.newSurface->IsUPeriodic()) {
            const double T = ctx.newSurface->UPeriod();
            shiftU = std::round((refNew.X() - mid.X()) / T) * T;
        }
        if (ctx.newSurface->IsVPeriodic()) {
            const double T = ctx.newSurface->VPeriod();
            shiftV = std::round((refNew.Y() - mid.Y()) / T) * T;
        }
        for (auto& p : uv) p.SetCoord(p.X() + shiftU, p.Y() + shiftV);

        if (degenerated) {
            // The whole edge maps onto the singular iso-line of the new surface.
            gp_Pnt2d first, last;
            double fp = 0.0, lp = 0.0;
            const gp_Pnt pole = BRep_Tool::Pnt(TopExp::FirstVertex(E));
            if (ctx.sas->DegeneratedValues(pole, std::max(BRep_Tool::Tolerance(E), Precision::Confusion()), first,
                                           last, fp, lp)) {
                const bool uIso = std::abs(first.Y() - last.Y()) < Precision::PConfusion();
                for (auto& p : uv) {
                    if (uIso) p.SetY(first.Y());
                    else p.SetX(first.X());
                }
            }
        }

        C = fitPCurve(params, uv);
        if (C.IsNull()) return false;

        const double deviation = degenerated ? 0.0 : pcurveDeviation(C, curve3d, ctx.newSurface, a, b);
        maxDeviation_ = std::max(maxDeviation_, deviation);
        Tol = std::max(BRep_Tool::Tolerance(E), deviation);
        return true;
    }

    GeomAbs_Shape Continuity(const TopoDS_Edge& E, const TopoDS_Face& F1, const TopoDS_Face& F2,
                             const TopoDS_Edge&, const TopoDS_Face&, const TopoDS_Face&) override {
        return BRep_Tool::Continuity(E, F1, F2);
    }

private:
    struct SurfaceInfo {
        occ::handle<Geom_Surface> surface;  // in the face location frame
        TopLoc_Location location;
        double deviation = 0.0;
    };
    struct CurveInfo {
        occ::handle<Geom_Curve> curve;  // in the edge location frame
        occ::handle<Geom_Curve> globalCurve;
        TopLoc_Location location;
        double first = 0.0;
        double last = 0.0;
        bool closed = false;
        double deviation = 0.0;
    };
    struct FaceContext {
        occ::handle<Geom_Surface> oldSurface;  // global
        occ::handle<Geom_Surface> newSurface;  // global
        std::unique_ptr<ShapeAnalysis_Surface> sas;
        std::unique_ptr<BRepTopAdaptor_FClass2d> classifier;
        double oldUVSize = 1.0;
        gp_Pnt2d windowCenter;  // new UV of the old face's UV-box centre
    };

    const SurfaceInfo* surfaceInfo(const TopoDS_Face& F) {
        const auto key = F.TShape().get();
        auto it = surfaces_.find(key);
        if (it == surfaces_.end()) it = surfaces_.emplace(key, computeSurface(F)).first;
        return it->second ? &*it->second : nullptr;
    }

    const CurveInfo* curveInfo(const TopoDS_Edge& E) {
        const auto key = E.TShape().get();
        auto it = curves_.find(key);
        if (it == curves_.end()) it = curves_.emplace(key, computeCurve(E)).first;
        return it->second ? &*it->second : nullptr;
    }

    std::optional<SurfaceInfo> computeSurface(const TopoDS_Face& F) const {
        TopLoc_Location loc;
        occ::handle<Geom_Surface> s = BRep_Tool::Surface(F, loc);
        if (s.IsNull()) return std::nullopt;
        s = unwrapSurface(s);
        if (!isFreeFormSurface(s)) return std::nullopt;
        double u0, u1, v0, v1;
        BRepTools::UVBounds(F, u0, u1, v0, v1);
        GeomConvert_SurfToAnaSurf conv(s);
        conv.SetConvType(GeomConvert_Simplest);
        // The bounded variant fails on closed periodic surfaces; the unbounded one is then tried.
        // Either way the candidate is verified independently on the face domain below.
        occ::handle<Geom_Surface> ana = conv.ConvertToAnalytical(tol_, u0, u1, v0, v1);
        if (ana.IsNull()) {
            GeomConvert_SurfToAnaSurf whole(s);
            whole.SetConvType(GeomConvert_Simplest);
            ana = whole.ConvertToAnalytical(tol_);
        }
        if (ana.IsNull()) return std::nullopt;
        ana = unwrapSurface(ana);
        if (!ana->IsKind(STANDARD_TYPE(Geom_ElementarySurface))) return std::nullopt;

        // Independent check of the deviation and of the normal direction on a grid over the face.
        constexpr int G = 6;
        double deviation = 0.0;
        int normalSign = 0;
        for (int i = 0; i <= G; ++i) {
            for (int j = 0; j <= G; ++j) {
                const double u = u0 + (u1 - u0) * i / G;
                const double v = v0 + (v1 - v0) * j / G;
                const gp_Pnt p = s->Value(u, v);
                GeomAPI_ProjectPointOnSurf proj(p, ana);
                if (!proj.IsDone() || proj.NbPoints() == 0) return std::nullopt;
                deviation = std::max(deviation, proj.LowerDistance());
                if (normalSign == 0 && i > 0 && i < G && j > 0 && j < G) {
                    double pu, pv;
                    proj.LowerDistanceParameters(pu, pv);
                    GeomLProp_SLProps oldProps(s, u, v, 1, Precision::Confusion());
                    GeomLProp_SLProps newProps(ana, pu, pv, 1, Precision::Confusion());
                    if (oldProps.IsNormalDefined() && newProps.IsNormalDefined())
                        normalSign = oldProps.Normal().Dot(newProps.Normal()) > 0 ? 1 : -1;
                }
            }
        }
        if (deviation > tol_ || normalSign == 0) return std::nullopt;
        if (normalSign < 0) ana = ana->UReversed();
        return SurfaceInfo{ana, loc, deviation};
    }

    std::optional<CurveInfo> computeCurve(const TopoDS_Edge& E) {
        if (BRep_Tool::Degenerated(E)) return std::nullopt;
        TopLoc_Location loc;
        double f = 0.0, l = 0.0;
        occ::handle<Geom_Curve> c = BRep_Tool::Curve(E, loc, f, l);
        if (c.IsNull()) return std::nullopt;
        c = unwrapCurve(c);
        if (!isFreeFormCurve(c)) return std::nullopt;
        // A new curve parametrization forces new pcurves on every face of the edge; they are
        // computed exactly only on elementary surfaces, so the curve is converted only if all
        // its faces are (or become) elementary.
        if (const auto* faces = edgeFaces_.Seek(E)) {
            for (const TopoDS_Shape& f : *faces) {
                const TopoDS_Face& face = TopoDS::Face(f);
                if (surfaceInfo(face)) continue;
                occ::handle<Geom_Surface> fs = BRep_Tool::Surface(face);
                if (fs.IsNull() || !unwrapSurface(fs)->IsKind(STANDARD_TYPE(Geom_ElementarySurface)))
                    return std::nullopt;
            }
        }
        GeomConvert_CurveToAnaCurve conv(c);
        occ::handle<Geom_Curve> ana;
        double nf = 0.0, nl = 0.0;
        if (!conv.ConvertToAnalytical(tol_, ana, f, l, nf, nl) || ana.IsNull()) return std::nullopt;
        ana = unwrapCurve(ana);
        if (isFreeFormCurve(ana) || !(nl > nf)) return std::nullopt;

        // Independent check: ends, deviation and direction must match the original curve.
        const double tolEnds = std::max(tol_, BRep_Tool::Tolerance(E));
        if (ana->Value(nf).Distance(c->Value(f)) > tolEnds || ana->Value(nl).Distance(c->Value(l)) > tolEnds)
            return std::nullopt;
        constexpr int S = 16;
        double deviation = 0.0;
        for (int i = 1; i < S; ++i) {
            const double t = f + (l - f) * i / S;
            gp_Pnt p;
            gp_Vec d;
            c->D1(t, p, d);
            GeomAPI_ProjectPointOnCurve proj(p, ana, nf, nl);
            if (proj.NbPoints() == 0) return std::nullopt;
            deviation = std::max(deviation, proj.LowerDistance());
            gp_Pnt q;
            gp_Vec dq;
            ana->D1(proj.LowerDistanceParameter(), q, dq);
            if (d.Dot(dq) <= 0.0) return std::nullopt;
        }
        if (deviation > tol_) return std::nullopt;

        CurveInfo info;
        info.curve = ana;
        info.location = loc;
        info.globalCurve = loc.IsIdentity()
                               ? ana
                               : occ::down_cast<Geom_Curve>(ana->Transformed(loc.Transformation()));
        info.first = nf;
        info.last = nl;
        info.closed = ana->Value(nf).Distance(ana->Value(nl)) <= tolEnds;
        info.deviation = deviation;
        return info;
    }

    FaceContext& faceContext(const TopoDS_Face& F, const SurfaceInfo* sInfo) {
        const auto key = F.TShape().get();
        auto it = faces_.find(key);
        if (it != faces_.end()) return *it->second;
        auto ctx = std::make_unique<FaceContext>();
        TopLoc_Location loc;
        occ::handle<Geom_Surface> base = BRep_Tool::Surface(F, loc);
        ctx->oldSurface = BRep_Tool::Surface(F);
        occ::handle<Geom_Surface> fresh = sInfo ? sInfo->surface : base;
        ctx->newSurface = loc.IsIdentity() ? fresh
                                           : occ::down_cast<Geom_Surface>(fresh->Transformed(loc.Transformation()));
        ctx->sas = std::make_unique<ShapeAnalysis_Surface>(ctx->newSurface);
        const TopoDS_Face forward = TopoDS::Face(F.Oriented(TopAbs_FORWARD));
        ctx->classifier = std::make_unique<BRepTopAdaptor_FClass2d>(forward, Precision::PConfusion());
        double u0, u1, v0, v1;
        BRepTools::UVBounds(forward, u0, u1, v0, v1);
        ctx->oldUVSize = std::max(std::hypot(u1 - u0, v1 - v0), Precision::PConfusion());
        ctx->windowCenter =
            ctx->sas->ValueOfUV(ctx->oldSurface->Value(0.5 * (u0 + u1), 0.5 * (v0 + v1)), Precision::Confusion());
        return *faces_.emplace(key, std::move(ctx)).first->second;
    }

    // Puts a new-UV point into the period window centred on the face, so that all pcurves of a
    // face are expressed in one consistent period.
    static gp_Pnt2d normalizeToWindow(const FaceContext& ctx, gp_Pnt2d p) {
        if (ctx.newSurface->IsUPeriodic()) {
            const double T = ctx.newSurface->UPeriod();
            p.SetX(normalizePeriodic(p.X(), ctx.windowCenter.X() - 0.5 * T, T));
        }
        if (ctx.newSurface->IsVPeriodic()) {
            const double T = ctx.newSurface->VPeriod();
            p.SetY(normalizePeriodic(p.Y(), ctx.windowCenter.Y() - 0.5 * T, T));
        }
        return p;
    }

    // Removes period jumps between consecutive samples.
    static void unwrap(const FaceContext& ctx, std::vector<gp_Pnt2d>& uv) {
        const bool up = ctx.newSurface->IsUPeriodic();
        const bool vp = ctx.newSurface->IsVPeriodic();
        for (std::size_t i = 1; i < uv.size(); ++i) {
            if (up) {
                const double T = ctx.newSurface->UPeriod();
                uv[i].SetX(uv[i].X() - std::round((uv[i].X() - uv[i - 1].X()) / T) * T);
            }
            if (vp) {
                const double T = ctx.newSurface->VPeriod();
                uv[i].SetY(uv[i].Y() - std::round((uv[i].Y() - uv[i - 1].Y()) / T) * T);
            }
        }
    }

    // At a surface singularity (e.g. sphere pole) one UV coordinate is arbitrary: take it by
    // linear extrapolation from the nearest regular samples.
    void repairSingularSamples(const std::vector<double>& params, std::vector<gp_Pnt2d>& uv,
                               const std::vector<bool>& singular) const {
        const int n = static_cast<int>(uv.size());
        for (int i = 0; i < n; ++i) {
            if (!singular[i]) continue;
            int j1 = -1, j2 = -1;
            const int step = (i < n / 2) ? 1 : -1;
            for (int j = i + step; j >= 0 && j < n; j += step) {
                if (singular[j]) continue;
                if (j1 < 0) j1 = j;
                else {
                    j2 = j;
                    break;
                }
            }
            if (j1 < 0 || j2 < 0) continue;
            gp_Pnt2d p1 = uv[j1], p2 = uv[j2];
            const double k = (params[i] - params[j1]) / (params[j2] - params[j1]);
            const gp_Pnt2d extra(p1.X() + k * (p2.X() - p1.X()), p1.Y() + k * (p2.Y() - p1.Y()));
            // Keep the coordinate that the projection fixes exactly (pole latitude), replace the
            // other one: choose the replacement that moves the point least in 3D.
            const gp_Pnt2d keepV(extra.X(), uv[i].Y());
            const gp_Pnt2d keepU(uv[i].X(), extra.Y());
            uv[i] = (std::abs(uv[i].Y() - extra.Y()) <= std::abs(uv[i].X() - extra.X())) ? keepV : keepU;
        }
    }

    static double pcurveDeviation(const occ::handle<Geom2d_Curve>& pc, const occ::handle<Geom_Curve>& c3d,
                                  const occ::handle<Geom_Surface>& surface, double a, double b) {
        constexpr int M = 200;
        double deviation = 0.0;
        for (int i = 0; i <= M; ++i) {
            const double t = a + (b - a) * i / M;
            const gp_Pnt2d p2 = pc->Value(t);
            deviation = std::max(deviation, surface->Value(p2.X(), p2.Y()).Distance(c3d->Value(t)));
        }
        return deviation;
    }

    static occ::handle<Geom2d_Curve> fitPCurve(const std::vector<double>& params, const std::vector<gp_Pnt2d>& uv) {
        const std::size_t n = uv.size();
        const double a = params.front(), b = params.back();
        const gp_XY slope = (uv.back().XY() - uv.front().XY()) / (b - a);
        double scale = 1.0;
        for (const auto& p : uv) scale = std::max({scale, std::abs(p.X()), std::abs(p.Y())});
        double linearDev = 0.0;
        for (std::size_t i = 0; i < n; ++i) {
            const gp_XY lin = uv.front().XY() + slope * (params[i] - a);
            linearDev = std::max(linearDev, (uv[i].XY() - lin).Modulus());
        }
        if (linearDev <= 1e-9 * scale) {
            const double speed = slope.Modulus();
            if (speed > 0.0 && std::abs(speed - 1.0) < 1e-12) {
                // Unit-speed line: Geom2d_Line parameter equals the edge parameter.
                const gp_Pnt2d origin(uv.front().XY() - slope * a);
                return new Geom2d_Line(origin, gp_Dir2d(slope));
            }
            NCollection_Array1<gp_Pnt2d> poles(1, 2);
            poles(1) = uv.front();
            poles(2) = uv.back();
            NCollection_Array1<double> knots(1, 2);
            knots(1) = a;
            knots(2) = b;
            NCollection_Array1<int> mults(1, 2);
            mults(1) = 2;
            mults(2) = 2;
            return new Geom2d_BSplineCurve(poles, knots, mults, 1);
        }
        auto pts = new NCollection_HArray1<gp_Pnt2d>(1, static_cast<int>(n));
        auto pars = new NCollection_HArray1<double>(1, static_cast<int>(n));
        for (std::size_t i = 0; i < n; ++i) {
            pts->SetValue(static_cast<int>(i) + 1, uv[i]);
            pars->SetValue(static_cast<int>(i) + 1, params[i]);
        }
        Geom2dAPI_Interpolate interp(pts, pars, false, Precision::PConfusion());
        interp.Perform();
        if (!interp.IsDone()) return {};
        return occ::handle<Geom2d_Curve>(interp.Curve());
    }

    double tol_;
    double maxDeviation_ = 0.0;
    NCollection_IndexedDataMap<TopoDS_Shape, NCollection_List<TopoDS_Shape>, TopTools_ShapeMapHasher> edgeFaces_;
    std::unordered_map<const TopoDS_TShape*, std::optional<SurfaceInfo>> surfaces_;
    std::unordered_map<const TopoDS_TShape*, std::optional<CurveInfo>> curves_;
    std::unordered_map<const TopoDS_TShape*, std::unique_ptr<FaceContext>> faces_;
};

struct ConversionOutcome {
    TopoDS_Shape shape;
    int surfaces = 0;
    int curves = 0;
    double deviation = 0.0;
    std::string failure;  // non-empty: the step was rolled back
};

ConversionOutcome runAnalyticConversion(const TopoDS_Shape& shape, double tolerance, double maxTolerance) {
    ConversionOutcome out;
    out.shape = shape;
    occ::handle<AnalyticModification> mod = new AnalyticModification(shape, tolerance);
    BRepTools_Modifier modifier(shape, mod);
    if (!modifier.IsDone()) {
        out.failure = "BRepTools_Modifier failed";
        return out;
    }
    TopoDS_Shape converted = modifier.ModifiedShape(shape);
    if (mod->surfacesConverted() == 0 && mod->curvesConverted() == 0) return out;
    // New pcurves are built by interpolation; ShapeFix restores SameParameter and tolerances.
    ShapeFix_Shape fix(converted);
    fix.SetPrecision(tolerance);
    fix.SetMinTolerance(Precision::Confusion());
    fix.SetMaxTolerance(maxTolerance);
    fix.Perform();
    converted = fix.Shape();
    const bool wasValid = isValid(shape);
    if (wasValid && !isValid(converted)) {
        out.failure = "result of analytic conversion is invalid (BRepCheck_Analyzer)";
        return out;
    }
    if (hasSolids(shape)) {
        const double v0 = volumeOf(shape), v1 = volumeOf(converted);
        GProp_GProps area;
        BRepGProp::SurfaceProperties(shape, area);
        const double allowed = 10.0 * std::max(tolerance, mod->maxDeviation()) * area.Mass() + 1e-9 * std::abs(v0);
        if (std::abs(v1 - v0) > allowed) {
            std::ostringstream msg;
            msg << "analytic conversion changed volume by " << (v1 - v0) << " (allowed " << allowed << ")";
            out.failure = msg.str();
            return out;
        }
    }
    out.shape = converted;
    out.surfaces = mod->surfacesConverted();
    out.curves = mod->curvesConverted();
    out.deviation = mod->maxDeviation();
    return out;
}

std::optional<Error> validateOptions(const HealingOptions& o) {
    auto bad = [](double x) { return !(x > 0.0) || !std::isfinite(x); };
    if (bad(o.precision)) return Error{ErrorCode::InvalidInput, "precision must be positive"};
    if (bad(o.maxTolerance) || o.maxTolerance < o.precision)
        return Error{ErrorCode::InvalidInput, "maxTolerance must be positive and >= precision"};
    if (bad(o.smallElementSize)) return Error{ErrorCode::InvalidInput, "smallElementSize must be positive"};
    if (bad(o.analyticTolerance)) return Error{ErrorCode::InvalidInput, "analyticTolerance must be positive"};
    if (bad(o.unifyLinearTolerance)) return Error{ErrorCode::InvalidInput, "unifyLinearTolerance must be positive"};
    if (bad(o.unifyAngularTolerance)) return Error{ErrorCode::InvalidInput, "unifyAngularTolerance must be positive"};
    return std::nullopt;
}

std::optional<Error> validateShape(const TopoDS_Shape& shape) {
    if (shape.IsNull()) return Error{ErrorCode::InvalidInput, "shape is null"};
    if (!TopExp_Explorer(shape, TopAbs_EDGE).More())
        return Error{ErrorCode::InvalidInput, "shape contains no edges or faces"};
    return std::nullopt;
}

void fillBefore(HealingReport& r, const TopoDS_Shape& s) {
    r.surfacesBefore = countSurfaceTypes(s);
    r.curvesBefore = countCurveTypes(s);
    r.facesBefore = countUnique(s, TopAbs_FACE);
    r.edgesBefore = countUnique(s, TopAbs_EDGE);
    r.validBefore = isValid(s);
    r.volumeBefore = volumeOf(s);
}

void fillAfter(HealingReport& r, const TopoDS_Shape& s) {
    r.surfacesAfter = countSurfaceTypes(s);
    r.curvesAfter = countCurveTypes(s);
    r.facesAfter = countUnique(s, TopAbs_FACE);
    r.edgesAfter = countUnique(s, TopAbs_EDGE);
    r.valid = isValid(s);
    r.volumeAfter = volumeOf(s);
    r.volumeChange = r.volumeAfter - r.volumeBefore;
    r.maxToleranceAfter = maxToleranceOf(s);
}

// Keeps the result of a step only if it did not break a valid shape.
bool acceptStep(const char* name, const TopoDS_Shape& before, bool beforeValid, TopoDS_Shape& candidate,
                HealingReport& report) {
    if (candidate.IsNull()) {
        report.warnings.push_back(std::string(name) + ": produced a null shape, step rolled back");
        return false;
    }
    if (beforeValid && !isValid(candidate)) {
        report.warnings.push_back(std::string(name) + ": made a valid shape invalid, step rolled back");
        candidate = before;
        return false;
    }
    report.steps.push_back(name);
    return true;
}

}  // namespace

TypeCounts countSurfaceTypes(const TopoDS_Shape& shape) {
    TypeCounts counts;
    ShapeMap faces;
    TopExp::MapShapes(shape, TopAbs_FACE, faces);
    for (int i = 1; i <= faces.Extent(); ++i)
        ++counts[surfaceTypeName(BRepAdaptor_Surface(TopoDS::Face(faces(i)), false).GetType())];
    return counts;
}

TypeCounts countCurveTypes(const TopoDS_Shape& shape) {
    TypeCounts counts;
    ShapeMap edges;
    TopExp::MapShapes(shape, TopAbs_EDGE, edges);
    for (int i = 1; i <= edges.Extent(); ++i) {
        const TopoDS_Edge& e = TopoDS::Edge(edges(i));
        if (BRep_Tool::Degenerated(e) || !BRep_Tool::IsGeometric(e)) continue;
        ++counts[curveTypeName(BRepAdaptor_Curve(e).GetType())];
    }
    return counts;
}

Result<HealingResult> convertToAnalytic(const TopoDS_Shape& shape, double tolerance) {
    if (auto err = validateShape(shape)) return *err;
    if (!(tolerance > 0.0) || !std::isfinite(tolerance))
        return Error{ErrorCode::InvalidInput, "tolerance must be positive"};
    try {
        HealingResult result;
        fillBefore(result.report, shape);
        ConversionOutcome conv = runAnalyticConversion(shape, tolerance, std::max(1e-3, 100.0 * tolerance));
        if (!conv.failure.empty()) result.report.warnings.push_back("convertToAnalytic: " + conv.failure);
        else result.report.steps.push_back("convertToAnalytic");
        result.shape = conv.shape;
        result.report.surfacesConverted = conv.surfaces;
        result.report.curvesConverted = conv.curves;
        result.report.maxConversionDeviation = conv.deviation;
        fillAfter(result.report, result.shape);
        return result;
    } catch (const Standard_Failure& e) {
        return Error{ErrorCode::OperationFailed, std::string("OCCT exception in convertToAnalytic: ") + e.what()};
    } catch (const std::exception& e) {
        return Error{ErrorCode::OperationFailed, std::string("exception in convertToAnalytic: ") + e.what()};
    }
}

Result<HealingResult> healShape(const TopoDS_Shape& shape, const HealingOptions& options) {
    if (auto err = validateShape(shape)) return *err;
    if (auto err = validateOptions(options)) return *err;
    try {
        HealingResult result;
        HealingReport& report = result.report;
        fillBefore(report, shape);
        TopoDS_Shape current = shape;
        bool currentValid = report.validBefore;

        if (options.directFaces) {
            const int indirect = countIndirectFaces(current);
            TopoDS_Shape next = ShapeCustom::DirectFaces(current);
            if (acceptStep("directFaces", current, currentValid, next, report)) {
                report.directedFaces = indirect - countIndirectFaces(next);
                current = next;
                currentValid = isValid(current);
            }
        }

        if (options.fixShape) {
            ShapeFix_Shape fix(current);
            fix.SetPrecision(options.precision);
            fix.SetMinTolerance(options.precision);
            fix.SetMaxTolerance(options.maxTolerance);
            fix.Perform();
            TopoDS_Shape next = fix.Shape();
            if (acceptStep("fixShape", current, currentValid, next, report)) {
                current = next;
                currentValid = isValid(current);
            }
        }

        if (options.removeSmallElements) {
            const int faces0 = countUnique(current, TopAbs_FACE);
            const int edges0 = countUnique(current, TopAbs_EDGE);
            ShapeFix_FixSmallFace smallFaces;
            smallFaces.Init(current);
            smallFaces.SetPrecision(options.smallElementSize);
            smallFaces.SetMaxTolerance(options.maxTolerance);
            smallFaces.Perform();
            TopoDS_Shape next = smallFaces.FixShape();
            if (!next.IsNull()) {
                ShapeFix_Wireframe wire(next);
                wire.SetPrecision(options.smallElementSize);
                wire.SetMaxTolerance(options.maxTolerance);
                wire.ModeDropSmallEdges() = true;
                wire.FixSmallEdges();
                wire.FixWireGaps();
                next = wire.Shape();
            }
            if (!next.IsNull()) {
                // Merging small edges leaves pcurves and tolerances to be made consistent.
                ShapeFix_Shape fix(next);
                fix.SetPrecision(options.precision);
                fix.SetMinTolerance(options.precision);
                fix.SetMaxTolerance(options.maxTolerance);
                fix.Perform();
                next = fix.Shape();
            }
            if (acceptStep("removeSmallElements", current, currentValid, next, report)) {
                report.smallFacesRemoved = faces0 - countUnique(next, TopAbs_FACE);
                report.smallEdgesRemoved = edges0 - countUnique(next, TopAbs_EDGE);
                current = next;
                currentValid = isValid(current);
            }
        }

        if (options.convertToAnalytic) {
            ConversionOutcome conv = runAnalyticConversion(current, options.analyticTolerance, options.maxTolerance);
            if (!conv.failure.empty()) {
                report.warnings.push_back("convertToAnalytic: " + conv.failure + ", step rolled back");
            } else {
                report.steps.push_back("convertToAnalytic");
                report.surfacesConverted = conv.surfaces;
                report.curvesConverted = conv.curves;
                report.maxConversionDeviation = conv.deviation;
                current = conv.shape;
                currentValid = isValid(current);
            }
        }

        if (options.unifySameDomain) {
            const int faces0 = countUnique(current, TopAbs_FACE);
            ShapeUpgrade_UnifySameDomain unify(current, true, true, false);
            unify.SetLinearTolerance(options.unifyLinearTolerance);
            unify.SetAngularTolerance(options.unifyAngularTolerance);
            unify.AllowInternalEdges(false);
            unify.Build();
            TopoDS_Shape next = unify.Shape();
            if (acceptStep("unifySameDomain", current, currentValid, next, report)) {
                report.facesMerged = faces0 - countUnique(next, TopAbs_FACE);
                current = next;
                currentValid = isValid(current);
            }
        }

        result.shape = current;
        fillAfter(report, current);
        return result;
    } catch (const Standard_Failure& e) {
        return Error{ErrorCode::OperationFailed, std::string("OCCT exception in healShape: ") + e.what()};
    } catch (const std::exception& e) {
        return Error{ErrorCode::OperationFailed, std::string("exception in healShape: ") + e.what()};
    }
}

}  // namespace bendcore::kernel
