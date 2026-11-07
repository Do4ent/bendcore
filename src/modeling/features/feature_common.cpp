#include "feature_common.hpp"

#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <BRepTools_History.hxx>
#include <BRep_Tool.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <GeomLib_IsPlanarSurface.hxx>
#include <Geom_Surface.hxx>
#include <Standard_Failure.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopoDS_Wire.hxx>

#include <algorithm>
#include <cmath>
#include <map>
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

// ---------------------------------------------------------------- история булевой операции

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
    const History& kernelHistory = res.value().history;

    try {
        ShapeMap trackedMap, untrackedMap, resMap;
        for (const auto& s : arguments) TopExp::MapShapes(s, trackArguments ? trackedMap : untrackedMap);
        for (const auto& s : tools) TopExp::MapShapes(s, trackTools ? trackedMap : untrackedMap);
        TopExp::MapShapes(step.shape, resMap);

        // История ядра, ограниченная элементами отслеживаемых входов. Пустой результат
        // (booleanOp возвращает его без истории) — все элементы входов удалены.
        ShapeMap fromTracked;
        for (int i = 1; i <= trackedMap.Extent(); ++i) {
            const TopoDS_Shape& s = trackedMap(i);
            if (!BRepTools_History::IsSupportedType(s)) continue;
            if (kernelHistory.IsNull()) {
                if (!resMap.Contains(s)) step.history->Remove(s);
                else fromTracked.Add(s);
                continue;
            }
            if (kernelHistory->IsRemoved(s)) step.history->Remove(s);
            for (const TopoDS_Shape& m : kernelHistory->Modified(s)) {
                step.history->AddModified(s, m);
                fromTracked.Add(m);
            }
            for (const TopoDS_Shape& g : kernelHistory->Generated(s)) {
                step.history->AddGenerated(s, g);
                fromTracked.Add(g);
            }
            if (resMap.Contains(s)) fromTracked.Add(s);
        }

        // Грани результата, происходящие только из неотслеживаемых входов.
        ShapeMap untrackedFaces;
        for (int i = 1; i <= untrackedMap.Extent(); ++i) {
            const TopoDS_Shape& s = untrackedMap(i);
            if (s.ShapeType() != TopAbs_FACE) continue;
            ShapeList images;
            if (resMap.Contains(s)) images.Append(s);
            if (!kernelHistory.IsNull())
                for (const TopoDS_Shape& m : kernelHistory->Modified(s)) images.Append(m);
            for (const TopoDS_Shape& f : images)
                if (!fromTracked.Contains(f)) untrackedFaces.Add(f);
        }
        for (int i = 1; i <= untrackedFaces.Extent(); ++i) step.untrackedImages.push_back(untrackedFaces(i));
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
