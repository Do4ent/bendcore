#include "feature_common.hpp"

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepClass_FaceClassifier.hxx>
#include <BRepGProp.hxx>
#include <BRepTools.hxx>
#include <BRep_Tool.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <GeomAPI_ProjectPointOnCurve.hxx>
#include <GeomAPI_ProjectPointOnSurf.hxx>
#include <GeomLib_IsPlanarSurface.hxx>
#include <Geom_Curve.hxx>
#include <Geom_Surface.hxx>
#include <Standard_Failure.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Pnt2d.hxx>

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <sstream>

namespace bendcore::modeling {

const char* toString(FeatureMode mode) noexcept {
    switch (mode) {
        case FeatureMode::Boss: return "Boss";
        case FeatureMode::Cut: return "Cut";
    }
    return "Unknown";
}

const char* toString(ExtentType type) noexcept {
    switch (type) {
        case ExtentType::Blind: return "Blind";
        case ExtentType::Symmetric: return "Symmetric";
        case ExtentType::TwoSided: return "TwoSided";
        case ExtentType::ThroughAll: return "ThroughAll";
        case ExtentType::ThroughAllBoth: return "ThroughAllBoth";
        case ExtentType::UpToFace: return "UpToFace";
        case ExtentType::UpToSurface: return "UpToSurface";
    }
    return "Unknown";
}

}  // namespace bendcore::modeling

namespace bendcore::modeling::detail {

namespace {

bool hasSolid(const TopoDS_Shape& s) { return !s.IsNull() && TopExp_Explorer(s, TopAbs_SOLID).More(); }

}  // namespace

std::string occtMessage(const char* where, const char* what) {
    return std::string("OCCT exception in ") + where + ": " + (what ? what : "");
}

double volumeOf(const TopoDS_Shape& shape) {
    if (!hasSolid(shape)) return 0.0;
    GProp_GProps props;
    BRepGProp::VolumeProperties(shape, props);
    return props.Mass();
}

int countSolids(const TopoDS_Shape& shape) {
    if (shape.IsNull()) return 0;
    ShapeMap m;
    TopExp::MapShapes(shape, TopAbs_SOLID, m);
    return m.Extent();
}

double boundingDiagonal(const std::vector<TopoDS_Shape>& shapes) {
    Bnd_Box box;
    for (const auto& s : shapes)
        if (!s.IsNull()) BRepBndLib::Add(s, box);
    if (box.IsVoid()) return 0.0;
    return std::sqrt(box.SquareExtent());
}

void projectionRange(const TopoDS_Shape& shape, const gp_Pnt& origin, const gp_Dir& dir, double& lo, double& hi) {
    Bnd_Box box;
    BRepBndLib::Add(shape, box);
    lo = 0.0;
    hi = 0.0;
    if (box.IsVoid()) return;
    double x0, y0, z0, x1, y1, z1;
    box.Get(x0, y0, z0, x1, y1, z1);
    bool first = true;
    for (int i = 0; i < 8; ++i) {
        const gp_Pnt c((i & 1) ? x1 : x0, (i & 2) ? y1 : y0, (i & 4) ? z1 : z0);
        const double t = gp_Vec(origin, c).Dot(gp_Vec(dir));
        if (first) { lo = hi = t; first = false; }
        lo = std::min(lo, t);
        hi = std::max(hi, t);
    }
}

bool containsSubShape(const TopoDS_Shape& whole, const TopoDS_Shape& s) {
    if (whole.IsNull() || s.IsNull()) return false;
    ShapeMap m;
    TopExp::MapShapes(whole, s.ShapeType(), m);
    return m.Contains(s);
}

History newHistory() { return new BRepTools_History(); }

History copyHistory(const History& h) {
    History out = newHistory();
    if (!h.IsNull()) out->Merge(h);
    return out;
}

void mergeInto(History& a, const History& b) {
    if (a.IsNull()) a = newHistory();
    if (!b.IsNull()) a->Merge(b);
}

Result<gp_Pln> validateProfile(const Profile& profile) {
    const TopoDS_Face& face = profile.face;
    if (face.IsNull()) return Error{ErrorCode::InvalidInput, "profile face is null"};
    try {
        if (face.ShapeType() != TopAbs_FACE) return Error{ErrorCode::InvalidInput, "profile is not a face"};
        opencascade::handle<Geom_Surface> surf = BRep_Tool::Surface(face);
        if (surf.IsNull()) return Error{ErrorCode::InvalidInput, "profile face has no surface"};
        GeomLib_IsPlanarSurface planar(surf, 1e-7);
        if (!planar.IsPlanar()) return Error{ErrorCode::InvalidInput, "profile face is not planar"};
        const gp_Pln pln = planar.Plan();

        const gp_Dir n = profile.placement.Direction();
        if (!pln.Axis().Direction().IsParallel(n, 1e-7))
            return Error{ErrorCode::InvalidInput, "profile face is not parallel to the sketch plane"};
        if (pln.Distance(profile.placement.Location()) > 1e-6)
            return Error{ErrorCode::InvalidInput, "profile face does not lie in the sketch plane"};

        int wires = 0;
        for (TopExp_Explorer wex(face, TopAbs_WIRE); wex.More(); wex.Next()) {
            ++wires;
            const TopoDS_Wire wire = TopoDS::Wire(wex.Current());
            std::map<int, int> uses;
            ShapeMap vertices;
            int edges = 0;
            for (TopExp_Explorer eex(wire, TopAbs_EDGE); eex.More(); eex.Next()) {
                const TopoDS_Edge e = TopoDS::Edge(eex.Current());
                if (BRep_Tool::Degenerated(e)) continue;
                ++edges;
                TopoDS_Vertex v1, v2;
                TopExp::Vertices(e, v1, v2);
                if (v1.IsNull() || v2.IsNull())
                    return Error{ErrorCode::InvalidInput, "profile edge without end vertices"};
                ++uses[vertices.Add(v1)];
                ++uses[vertices.Add(v2)];
            }
            if (edges == 0) return Error{ErrorCode::InvalidInput, "profile contour has no edges"};
            for (const auto& [index, count] : uses)
                if (count % 2 != 0) {
                    std::ostringstream msg;
                    msg << "profile contour #" << wires << " is not closed (vertex used " << count << " time(s))";
                    return Error{ErrorCode::InvalidInput, msg.str()};
                }
        }
        if (wires == 0) return Error{ErrorCode::InvalidInput, "profile face has no contours"};

        GProp_GProps props;
        BRepGProp::SurfaceProperties(face, props);
        if (!(std::abs(props.Mass()) > 1e-12)) return Error{ErrorCode::InvalidInput, "profile face has zero area"};
        if (!BRepCheck_Analyzer(face).IsValid())
            return Error{ErrorCode::InvalidInput, "profile face is invalid (BRepCheck_Analyzer)"};
        return pln;
    } catch (const Standard_Failure& e) {
        return Error{ErrorCode::InvalidInput, occtMessage("validateProfile", e.what())};
    }
}

std::string checkSolidInput(const TopoDS_Shape& shape, const char* role) {
    if (shape.IsNull()) return std::string(role) + " is null";
    if (!hasSolid(shape)) return std::string(role) + " contains no solid";
    if (!BRepCheck_Analyzer(shape).IsValid()) return std::string(role) + " is invalid (BRepCheck_Analyzer)";
    return {};
}

std::string checkSolidResult(const TopoDS_Shape& shape) {
    if (shape.IsNull()) return "result is null";
    if (!hasSolid(shape)) return "result contains no solid";
    if (!BRepCheck_Analyzer(shape).IsValid()) return "result is invalid (BRepCheck_Analyzer)";
    for (TopExp_Explorer ex(shape, TopAbs_SOLID); ex.More(); ex.Next())
        if (!(volumeOf(ex.Current()) > 0.0)) return "result contains a solid with non-positive volume";
    return {};
}

Result<TransformedCopy> transformCopy(const TopoDS_Shape& shape, const gp_Trsf& trsf) {
    try {
        BRepBuilderAPI_Transform tr(shape, trsf, true);
        if (!tr.IsDone()) return Error{ErrorCode::OperationFailed, "BRepBuilderAPI_Transform failed"};
        TransformedCopy out;
        out.shape = tr.Shape();
        for (TopAbs_ShapeEnum type : {TopAbs_SOLID, TopAbs_FACE, TopAbs_EDGE, TopAbs_VERTEX}) {
            ShapeMap m;
            TopExp::MapShapes(shape, type, m);
            for (int i = 1; i <= m.Extent(); ++i) {
                TopoDS_Shape img = tr.ModifiedShape(m(i));
                if (!img.IsNull()) out.pairs.emplace_back(m(i), img);
            }
        }
        return out;
    } catch (const Standard_Failure& e) {
        return Error{ErrorCode::OperationFailed, occtMessage("transformCopy", e.what())};
    }
}

// ---------------------------------------------------------------- восстановление истории

namespace {

struct InputFace {
    TopoDS_Face face;
    opencascade::handle<Geom_Surface> surface;
    double u0 = 0, u1 = 0, v0 = 0, v1 = 0;
    Bnd_Box box;
    int input = 0;
};

struct InputEdge {
    TopoDS_Edge edge;
    opencascade::handle<Geom_Curve> curve;
    double first = 0, last = 0;
    Bnd_Box box;
    int input = 0;
};

struct InputVertex {
    TopoDS_Vertex vertex;
    gp_Pnt point;
    int input = 0;
};

enum class OnFace { Out, On, In };

OnFace classifyOnFace(const gp_Pnt& p, const InputFace& f, double tol) {
    if (f.box.IsOut(p)) return OnFace::Out;
    const double du = 1e-3 * std::max(1.0, f.u1 - f.u0), dv = 1e-3 * std::max(1.0, f.v1 - f.v0);
    GeomAPI_ProjectPointOnSurf proj(p, f.surface, f.u0 - du, f.u1 + du, f.v0 - dv, f.v1 + dv);
    if (!proj.IsDone() || proj.NbPoints() == 0) return OnFace::Out;
    if (proj.LowerDistance() > tol) return OnFace::Out;
    double u, v;
    proj.LowerDistanceParameters(u, v);
    BRepClass_FaceClassifier cls(f.face, gp_Pnt2d(u, v), tol);
    switch (cls.State()) {
        case TopAbs_IN: return OnFace::In;
        case TopAbs_ON: return OnFace::On;
        default: return OnFace::Out;
    }
}

bool onEdge(const gp_Pnt& p, const InputEdge& e, double tol) {
    if (e.box.IsOut(p)) return false;
    GeomAPI_ProjectPointOnCurve proj(p, e.curve, e.first, e.last);
    if (proj.NbPoints() == 0) {
        return e.curve->Value(e.first).Distance(p) <= tol || e.curve->Value(e.last).Distance(p) <= tol;
    }
    return proj.LowerDistance() <= tol;
}

// Точки строго внутри грани (сетка по параметрам + классификатор).
std::vector<gp_Pnt> interiorSamples(const TopoDS_Face& face, double tol) {
    std::vector<gp_Pnt> pts;
    double u0, u1, v0, v1;
    BRepTools::UVBounds(face, u0, u1, v0, v1);
    BRepAdaptor_Surface surf(face, false);
    for (int n : {7, 25, 81}) {
        pts.clear();
        for (int i = 0; i < n; ++i)
            for (int j = 0; j < n; ++j) {
                const double u = u0 + (u1 - u0) * (i + 0.5) / n;
                const double v = v0 + (v1 - v0) * (j + 0.5) / n;
                BRepClass_FaceClassifier cls(face, gp_Pnt2d(u, v), tol);
                if (cls.State() == TopAbs_IN) pts.push_back(surf.Value(u, v));
            }
        if (!pts.empty()) break;
    }
    if (pts.size() > 9) {
        std::vector<gp_Pnt> picked;
        for (int k = 0; k < 9; ++k) picked.push_back(pts[(pts.size() - 1) * k / 8]);
        pts = picked;
    }
    return pts;
}

std::vector<gp_Pnt> edgeSamples(const TopoDS_Edge& edge) {
    BRepAdaptor_Curve c(edge);
    const double f = c.FirstParameter(), l = c.LastParameter();
    return {c.Value(f + 0.25 * (l - f)), c.Value(f + 0.5 * (l - f)), c.Value(f + 0.75 * (l - f))};
}

Bnd_Box boxOf(const TopoDS_Shape& s, double gap) {
    Bnd_Box b;
    BRepBndLib::Add(s, b, false);
    b.Enlarge(gap);
    return b;
}

}  // namespace

Result<BooleanStep> booleanWithHistory(kernel::BooleanType type, const std::vector<TopoDS_Shape>& arguments,
                                       const std::vector<TopoDS_Shape>& tools, bool trackArguments,
                                       bool trackTools) {
    auto res = kernel::booleanOp(type, arguments, tools);
    if (!res) return res.error();
    BooleanStep step;
    step.shape = res.value().shape;
    step.warnings = res.value().warnings;
    step.fuzzyValue = res.value().fuzzyValue;
    step.history = newHistory();

    try {
        const double tol = std::max(1e-5, 10.0 * step.fuzzyValue);

        std::vector<TopoDS_Shape> inputs(arguments);
        inputs.insert(inputs.end(), tools.begin(), tools.end());
        const int nArgs = static_cast<int>(arguments.size());
        auto tracked = [&](int input) { return input < nArgs ? trackArguments : trackTools; };

        // Элементы входов (без повторов между входами).
        ShapeMap inFaceMap, inEdgeMap, inVertexMap, inSolidMap;
        std::vector<InputFace> inFaces;
        std::vector<InputEdge> inEdges;
        std::vector<InputVertex> inVertices;
        std::vector<int> solidInput;
        for (int k = 0; k < static_cast<int>(inputs.size()); ++k) {
            ShapeMap m;
            TopExp::MapShapes(inputs[k], TopAbs_FACE, m);
            for (int i = 1; i <= m.Extent(); ++i) {
                if (inFaceMap.Contains(m(i))) continue;
                inFaceMap.Add(m(i));
                InputFace f;
                f.face = TopoDS::Face(m(i));
                f.surface = BRep_Tool::Surface(f.face);
                BRepTools::UVBounds(f.face, f.u0, f.u1, f.v0, f.v1);
                f.box = boxOf(f.face, tol + BRep_Tool::Tolerance(f.face));
                f.input = k;
                inFaces.push_back(f);
            }
            m.Clear();
            TopExp::MapShapes(inputs[k], TopAbs_EDGE, m);
            for (int i = 1; i <= m.Extent(); ++i) {
                if (inEdgeMap.Contains(m(i))) continue;
                const TopoDS_Edge e = TopoDS::Edge(m(i));
                inEdgeMap.Add(e);
                InputEdge ie;
                ie.edge = e;
                ie.input = k;
                if (!BRep_Tool::Degenerated(e)) {
                    ie.curve = BRep_Tool::Curve(e, ie.first, ie.last);
                    ie.box = boxOf(e, tol + BRep_Tool::Tolerance(e));
                }
                inEdges.push_back(ie);
            }
            m.Clear();
            TopExp::MapShapes(inputs[k], TopAbs_VERTEX, m);
            for (int i = 1; i <= m.Extent(); ++i) {
                if (inVertexMap.Contains(m(i))) continue;
                inVertexMap.Add(m(i));
                const TopoDS_Vertex v = TopoDS::Vertex(m(i));
                inVertices.push_back({v, BRep_Tool::Pnt(v), k});
            }
            m.Clear();
            TopExp::MapShapes(inputs[k], TopAbs_SOLID, m);
            for (int i = 1; i <= m.Extent(); ++i) {
                if (inSolidMap.Contains(m(i))) continue;
                inSolidMap.Add(m(i));
                solidInput.push_back(k);
            }
        }

        std::vector<bool> faceHasImage(inFaces.size(), false), edgeHasImage(inEdges.size(), false),
            vertexHasImage(inVertices.size(), false), solidHasImage(inSolidMap.Extent(), false);

        ShapeMap resFaces, resEdges, resVertices, resSolids;
        TopExp::MapShapes(step.shape, TopAbs_FACE, resFaces);
        TopExp::MapShapes(step.shape, TopAbs_EDGE, resEdges);
        TopExp::MapShapes(step.shape, TopAbs_VERTEX, resVertices);
        TopExp::MapShapes(step.shape, TopAbs_SOLID, resSolids);

        // Прообразы граней результата: индексы граней входа.
        std::vector<std::vector<int>> faceOrigins(resFaces.Extent() + 1);
        for (int r = 1; r <= resFaces.Extent(); ++r) {
            const TopoDS_Face rf = TopoDS::Face(resFaces(r));
            if (const int idx = inFaceMap.FindIndex(rf); idx > 0) {
                faceHasImage[idx - 1] = true;
                faceOrigins[r].push_back(idx - 1);
                continue;
            }
            const double ftol = tol + BRep_Tool::Tolerance(rf);
            const std::vector<gp_Pnt> pts = interiorSamples(rf, ftol);
            if (pts.empty()) {
                step.warnings.push_back("history: no interior sample on a result face");
                continue;
            }
            for (int i = 0; i < static_cast<int>(inFaces.size()); ++i) {
                bool all = true, anyIn = false;
                for (const auto& p : pts) {
                    const OnFace s = classifyOnFace(p, inFaces[i], ftol);
                    if (s == OnFace::Out) { all = false; break; }
                    anyIn = anyIn || s == OnFace::In;
                }
                if (!all || !anyIn) continue;
                faceOrigins[r].push_back(i);
                faceHasImage[i] = true;
                if (tracked(inFaces[i].input)) step.history->AddModified(inFaces[i].face, rf);
            }
            if (faceOrigins[r].empty()) step.warnings.push_back("history: a result face has no origin among inputs");
        }
        for (int r = 1; r <= resFaces.Extent(); ++r) {
            bool fromTracked = false;
            for (int i : faceOrigins[r]) fromTracked = fromTracked || tracked(inFaces[i].input);
            if (!fromTracked && !faceOrigins[r].empty()) step.untrackedImages.push_back(resFaces(r));
        }

        for (int r = 1; r <= resEdges.Extent(); ++r) {
            const TopoDS_Edge re = TopoDS::Edge(resEdges(r));
            if (const int idx = inEdgeMap.FindIndex(re); idx > 0) {
                edgeHasImage[idx - 1] = true;
                continue;
            }
            if (BRep_Tool::Degenerated(re)) continue;
            const double etol = tol + BRep_Tool::Tolerance(re);
            const std::vector<gp_Pnt> pts = edgeSamples(re);
            bool found = false;
            for (int i = 0; i < static_cast<int>(inEdges.size()); ++i) {
                if (inEdges[i].curve.IsNull()) continue;
                bool all = true;
                for (const auto& p : pts)
                    if (!onEdge(p, inEdges[i], etol)) { all = false; break; }
                if (!all) continue;
                found = true;
                edgeHasImage[i] = true;
                if (tracked(inEdges[i].input)) step.history->AddModified(inEdges[i].edge, re);
            }
            if (found) continue;
            for (const auto& f : inFaces) {
                if (!tracked(f.input)) continue;
                bool all = true;
                for (const auto& p : pts)
                    if (classifyOnFace(p, f, etol) == OnFace::Out) { all = false; break; }
                if (all) step.history->AddGenerated(f.face, re);
            }
        }

        for (int r = 1; r <= resVertices.Extent(); ++r) {
            const TopoDS_Vertex rv = TopoDS::Vertex(resVertices(r));
            if (const int idx = inVertexMap.FindIndex(rv); idx > 0) {
                vertexHasImage[idx - 1] = true;
                continue;
            }
            const gp_Pnt p = BRep_Tool::Pnt(rv);
            const double vtol = tol + BRep_Tool::Tolerance(rv);
            bool found = false;
            for (int i = 0; i < static_cast<int>(inVertices.size()); ++i) {
                if (inVertices[i].point.Distance(p) > vtol) continue;
                found = true;
                vertexHasImage[i] = true;
                if (tracked(inVertices[i].input)) step.history->AddModified(inVertices[i].vertex, rv);
            }
            if (found) continue;
            for (const auto& e : inEdges)
                if (!e.curve.IsNull() && tracked(e.input) && onEdge(p, e, vtol))
                    step.history->AddGenerated(e.edge, rv);
        }

        // Тела: образ тела входа — тело результата, у которого есть грань из этого тела.
        // Для вычитания инструменты телами-прообразами не считаются.
        std::vector<std::set<int>> faceOwners(inFaces.size());
        for (int s = 1; s <= inSolidMap.Extent(); ++s) {
            ShapeMap fm;
            TopExp::MapShapes(inSolidMap(s), TopAbs_FACE, fm);
            for (int i = 1; i <= fm.Extent(); ++i)
                if (const int idx = inFaceMap.FindIndex(fm(i)); idx > 0) faceOwners[idx - 1].insert(s);
        }
        for (int r = 1; r <= resSolids.Extent(); ++r) {
            const TopoDS_Shape rs = resSolids(r);
            if (const int idx = inSolidMap.FindIndex(rs); idx > 0) {
                solidHasImage[idx - 1] = true;
                continue;
            }
            std::set<int> owners;
            ShapeMap fm;
            TopExp::MapShapes(rs, TopAbs_FACE, fm);
            for (int i = 1; i <= fm.Extent(); ++i) {
                const int ri = resFaces.FindIndex(fm(i));
                if (ri <= 0) continue;
                for (int fi : faceOrigins[ri]) owners.insert(faceOwners[fi].begin(), faceOwners[fi].end());
            }
            for (int s : owners) {
                const int input = solidInput[s - 1];
                if (type == kernel::BooleanType::Cut && input >= nArgs) continue;
                solidHasImage[s - 1] = true;
                if (tracked(input)) step.history->AddModified(inSolidMap(s), rs);
            }
        }

        for (std::size_t i = 0; i < inFaces.size(); ++i)
            if (!faceHasImage[i] && tracked(inFaces[i].input)) step.history->Remove(inFaces[i].face);
        for (std::size_t i = 0; i < inEdges.size(); ++i)
            if (!edgeHasImage[i] && tracked(inEdges[i].input) &&
                step.history->Generated(inEdges[i].edge).IsEmpty())
                step.history->Remove(inEdges[i].edge);
        for (std::size_t i = 0; i < inVertices.size(); ++i)
            if (!vertexHasImage[i] && tracked(inVertices[i].input) &&
                step.history->Generated(inVertices[i].vertex).IsEmpty())
                step.history->Remove(inVertices[i].vertex);
        for (int s = 1; s <= inSolidMap.Extent(); ++s)
            if (!solidHasImage[s - 1] && tracked(solidInput[s - 1])) step.history->Remove(inSolidMap(s));
        return step;
    } catch (const Standard_Failure& e) {
        return Error{ErrorCode::OperationFailed, occtMessage("boolean history", e.what())};
    }
}

History normalizeHistory(const TopoDS_Shape& input, const TopoDS_Shape& result, const History& raw) {
    History h = newHistory();
    ShapeMap resMap;
    TopExp::MapShapes(result, resMap);
    for (TopAbs_ShapeEnum type : {TopAbs_SOLID, TopAbs_FACE, TopAbs_EDGE, TopAbs_VERTEX}) {
        ShapeMap m;
        TopExp::MapShapes(input, type, m);
        for (int i = 1; i <= m.Extent(); ++i) {
            const TopoDS_Shape& s = m(i);
            for (const ShapeList* list : {&raw->Modified(s), &raw->Generated(s)})
                for (const TopoDS_Shape& img : *list) {
                    if (img.IsNull() || img.IsSame(s) || !resMap.Contains(img)) continue;
                    if (img.ShapeType() == s.ShapeType()) h->AddModified(s, img);
                    else h->AddGenerated(s, img);
                }
            if (!h->Modified(s).IsEmpty()) continue;
            if (resMap.Contains(s)) continue;
            h->Remove(s);  // нет в результате и нет Modified; Generated-образы допустимы
        }
    }
    return h;
}

void addCopyGenerations(History& history, const std::vector<std::pair<TopoDS_Shape, TopoDS_Shape>>& pairs,
                        const TopoDS_Shape& result, const History& stepHistory) {
    ShapeMap resMap;
    TopExp::MapShapes(result, resMap);
    for (const auto& [orig, cp] : pairs) {
        if (resMap.Contains(cp)) {
            history->AddGenerated(orig, cp);
            continue;
        }
        if (stepHistory.IsNull()) continue;
        for (const TopoDS_Shape& img : stepHistory->Modified(cp))
            if (resMap.Contains(img)) history->AddGenerated(orig, img);
    }
}

Result<FeatureResult> applyTool(FeatureMode mode, const TopoDS_Shape& target, const TopoDS_Shape& tool,
                                const History& toolHistory, std::vector<std::string> warnings) {
    if (std::string p = checkSolidResult(tool); !p.empty())
        return Error{ErrorCode::OperationFailed, "tool body: " + p};
    FeatureResult out;
    out.tool = tool;
    out.history = copyHistory(toolHistory);
    out.warnings = std::move(warnings);
    if (target.IsNull()) {
        if (mode == FeatureMode::Cut) return Error{ErrorCode::InvalidInput, "cut requires a target body"};
        out.shape = tool;
        return out;
    }
    if (std::string p = checkSolidInput(target, "target body"); !p.empty()) return Error{ErrorCode::InvalidInput, p};
    const kernel::BooleanType type = mode == FeatureMode::Boss ? kernel::BooleanType::Fuse : kernel::BooleanType::Cut;
    auto step = booleanWithHistory(type, {target}, {tool}, true, true);
    if (!step) return step.error();
    BooleanStep& st = step.value();
    if (mode == FeatureMode::Cut) {
        if (countSolids(st.shape) == 0) return Error{ErrorCode::OperationFailed, "cut removes the whole body"};
        const double vt = volumeOf(target), vr = volumeOf(st.shape);
        if (std::abs(vt - vr) <= 1e-9 * std::max(1.0, vt))
            return Error{ErrorCode::OperationFailed, "cut tool does not intersect the target body"};
    }
    if (std::string p = checkSolidResult(st.shape); !p.empty()) return Error{ErrorCode::OperationFailed, p};
    const int solids = countSolids(st.shape);
    if (solids > 1) out.warnings.push_back("result has " + std::to_string(solids) + " solids");
    mergeInto(out.history, st.history);
    out.warnings.insert(out.warnings.end(), st.warnings.begin(), st.warnings.end());
    out.shape = st.shape;
    return out;
}

}  // namespace bendcore::modeling::detail
