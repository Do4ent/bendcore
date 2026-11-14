// Скругление, фаска, оболочка, уклон граней (docs/modeling/FEATURES.md, разделы 5, 7).

#include "feature_common.hpp"

#include <BRepFilletAPI_MakeChamfer.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <BRepOffsetAPI_DraftAngle.hxx>
#include <BRepOffsetAPI_MakeThickSolid.hxx>
#include <ChFiDS_ErrorStatus.hxx>
#include <Standard_Failure.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <NCollection_IndexedDataMap.hxx>
#include <TopoDS.hxx>
#include <gp_Pln.hxx>

#include <cmath>
#include <sstream>

namespace bendcore::modeling {

using namespace detail;

namespace {

const char* stripeStatusName(ChFiDS_ErrorStatus s) {
    switch (s) {
        case ChFiDS_Ok: return "Ok";
        case ChFiDS_Error: return "Error";
        case ChFiDS_WalkingFailure: return "WalkingFailure";
        case ChFiDS_StartsolFailure: return "StartsolFailure";
        case ChFiDS_TwistedSurface: return "TwistedSurface";
    }
    return "Unknown";
}

// Проверка списка рёбер: каждое — ребро target. Возвращает рёбра как TopoDS_Edge.
Result<std::vector<TopoDS_Edge>> targetEdges(const TopoDS_Shape& target, const std::vector<TopoDS_Shape>& edges) {
    if (edges.empty()) return Error{ErrorCode::InvalidInput, "no edges given"};
    ShapeMap m;
    TopExp::MapShapes(target, TopAbs_EDGE, m);
    std::vector<TopoDS_Edge> out;
    for (std::size_t i = 0; i < edges.size(); ++i) {
        if (edges[i].IsNull() || edges[i].ShapeType() != TopAbs_EDGE)
            return Error{ErrorCode::InvalidInput, "item #" + std::to_string(i) + " is not an edge"};
        if (!m.Contains(edges[i]))
            return Error{ErrorCode::InvalidInput, "edge #" + std::to_string(i) + " does not belong to the target body"};
        out.push_back(TopoDS::Edge(edges[i]));
    }
    return out;
}

std::size_t inputIndex(const std::vector<TopoDS_Edge>& edges, const TopoDS_Shape& e) {
    for (std::size_t i = 0; i < edges.size(); ++i)
        if (edges[i].IsSame(e)) return i;
    return edges.size();
}

// Скругление; при неудаче — причина и (если известны) индексы неудачных рёбер.
struct FilletAttempt {
    TopoDS_Shape shape;
    History history;
    std::string failure;
    std::vector<EdgeFailure> faulty;
};

FilletAttempt tryFillet(const TopoDS_Shape& target, const std::vector<TopoDS_Edge>& edges, double radius) {
    FilletAttempt a;
    try {
        BRepFilletAPI_MakeFillet mk(target);
        for (const auto& e : edges) mk.Add(radius, e);
        try {
            mk.Build();
        } catch (const Standard_Failure& e) {
            a.failure = occtMessage("BRepFilletAPI_MakeFillet::Build", e.what());
        }
        if (a.failure.empty() && !mk.IsDone()) a.failure = "BRepFilletAPI_MakeFillet failed";
        if (!a.failure.empty()) {
            for (int i = 1; i <= mk.NbFaultyContours(); ++i) {
                const int ic = mk.FaultyContour(i);
                const std::string reason = std::string("contour status ") + stripeStatusName(mk.StripeStatus(ic));
                for (int j = 1; j <= mk.NbEdges(ic); ++j) {
                    const std::size_t idx = inputIndex(edges, mk.Edge(ic, j));
                    if (idx < edges.size()) a.faulty.push_back({idx, edges[idx], reason});
                }
            }
            return a;
        }
        if (std::string p = checkSolidResult(mk.Shape()); !p.empty()) {
            a.failure = "fillet result rejected: " + p;
            return a;
        }
        a.shape = mk.Shape();
        a.history = algoHistory(target, mk);
    } catch (const Standard_Failure& e) {
        a.failure = occtMessage("fillet", e.what());
    }
    return a;
}

std::string failureText(const std::string& head, const std::vector<EdgeFailure>& faulty) {
    std::ostringstream msg;
    msg << head;
    if (!faulty.empty()) {
        msg << "; failed edges:";
        for (const auto& f : faulty) msg << " #" << f.index << " (" << f.reason << ")";
    }
    return msg.str();
}

}  // namespace

Result<std::vector<EdgeFailure>> filletFailures(const TopoDS_Shape& target, const std::vector<TopoDS_Shape>& edges,
                                                double radius) {
    if (std::string e = checkSolidInput(target, "target body"); !e.empty()) return Error{ErrorCode::InvalidInput, e};
    if (!(std::isfinite(radius) && radius > 0.0)) return Error{ErrorCode::InvalidInput, "fillet radius must be > 0"};
    auto list = targetEdges(target, edges);
    if (!list) return list.error();
    std::vector<EdgeFailure> out;
    for (std::size_t i = 0; i < list.value().size(); ++i) {
        FilletAttempt a = tryFillet(target, {list.value()[i]}, radius);
        if (!a.failure.empty()) out.push_back({i, list.value()[i], a.failure});
    }
    if (out.empty()) {
        FilletAttempt all = tryFillet(target, list.value(), radius);
        if (!all.failure.empty()) {
            if (all.faulty.empty())
                for (std::size_t i = 0; i < list.value().size(); ++i)
                    out.push_back({i, list.value()[i], "fails only together with the other edges: " + all.failure});
            else
                out = all.faulty;
        }
    }
    return out;
}

Result<FeatureResult> fillet(const TopoDS_Shape& target, const std::vector<TopoDS_Shape>& edges, double radius) {
    if (std::string e = checkSolidInput(target, "target body"); !e.empty()) return Error{ErrorCode::InvalidInput, e};
    if (!(std::isfinite(radius) && radius > 0.0)) return Error{ErrorCode::InvalidInput, "fillet radius must be > 0"};
    auto list = targetEdges(target, edges);
    if (!list) return list.error();
    FilletAttempt a = tryFillet(target, list.value(), radius);
    if (a.failure.empty()) {
        FeatureResult out;
        out.shape = a.shape;
        out.history = a.history;
        return out;
    }
    std::vector<EdgeFailure> faulty = a.faulty;
    if (faulty.empty()) {
        auto diag = filletFailures(target, edges, radius);
        if (diag) faulty = diag.value();
    }
    return Error{ErrorCode::OperationFailed, failureText("fillet radius " + std::to_string(radius) + " failed: " + a.failure,
                                                         faulty)};
}

Result<FeatureResult> chamfer(const TopoDS_Shape& target, const std::vector<TopoDS_Shape>& edges,
                              const ChamferParams& p) {
    if (std::string e = checkSolidInput(target, "target body"); !e.empty()) return Error{ErrorCode::InvalidInput, e};
    auto list = targetEdges(target, edges);
    if (!list) return list.error();
    auto positive = [](double v) { return std::isfinite(v) && v > 0.0; };
    if (!positive(p.distance)) return Error{ErrorCode::InvalidInput, "chamfer distance must be > 0"};
    if (p.type == ChamferType::TwoDistances && !positive(p.distance2))
        return Error{ErrorCode::InvalidInput, "second chamfer distance must be > 0"};
    if (p.type == ChamferType::DistanceAngle && !(std::isfinite(p.angle) && p.angle > 0.0 && p.angle < M_PI / 2))
        return Error{ErrorCode::InvalidInput, "chamfer angle must be in (0, pi/2)"};

    try {
        NCollection_IndexedDataMap<TopoDS_Shape, ShapeList, TopTools_ShapeMapHasher> edgeFaces;
        TopExp::MapShapesAndAncestors(target, TopAbs_EDGE, TopAbs_FACE, edgeFaces);
        FeatureResult out;
        BRepFilletAPI_MakeChamfer mk(target);
        for (std::size_t i = 0; i < list.value().size(); ++i) {
            const TopoDS_Edge& e = list.value()[i];
            if (p.type == ChamferType::EqualDistance) {
                mk.Add(p.distance, e);
                continue;
            }
            const auto& faces = edgeFaces.FindFromKey(e);
            if (faces.IsEmpty()) return Error{ErrorCode::InvalidInput, "edge #" + std::to_string(i) + " has no faces"};
            TopoDS_Face ref;
            if (!p.referenceFace.IsNull()) {
                for (const auto& f : faces)
                    if (f.IsSame(p.referenceFace)) ref = TopoDS::Face(f);
                if (ref.IsNull())
                    return Error{ErrorCode::InvalidInput,
                                 "reference face is not adjacent to edge #" + std::to_string(i)};
            } else {
                ref = TopoDS::Face(faces.First());
                out.warnings.push_back("edge #" + std::to_string(i) + ": reference face chosen automatically");
            }
            if (p.type == ChamferType::TwoDistances) mk.Add(p.distance, p.distance2, e, ref);
            else mk.AddDA(p.distance, p.angle, e, ref);
        }
        try {
            mk.Build();
        } catch (const Standard_Failure& e) {
            return Error{ErrorCode::OperationFailed, occtMessage("BRepFilletAPI_MakeChamfer::Build", e.what())};
        }
        if (!mk.IsDone()) return Error{ErrorCode::OperationFailed, "BRepFilletAPI_MakeChamfer failed (distance too large?)"};
        if (std::string pr = checkSolidResult(mk.Shape()); !pr.empty())
            return Error{ErrorCode::OperationFailed, "chamfer result rejected: " + pr};
        out.shape = mk.Shape();
        out.history = algoHistory(target, mk);
        return out;
    } catch (const Standard_Failure& e) {
        return Error{ErrorCode::OperationFailed, occtMessage("chamfer", e.what())};
    }
}

Result<FeatureResult> shell(const TopoDS_Shape& body, const std::vector<TopoDS_Shape>& removedFaces, double thickness,
                            ShellDirection direction) {
    if (std::string e = checkSolidInput(body, "body"); !e.empty()) return Error{ErrorCode::InvalidInput, e};
    if (!(std::isfinite(thickness) && thickness > 0.0)) return Error{ErrorCode::InvalidInput, "shell thickness must be > 0"};
    ShapeMap faces;
    TopExp::MapShapes(body, TopAbs_FACE, faces);
    ShapeList closing;
    for (std::size_t i = 0; i < removedFaces.size(); ++i) {
        if (removedFaces[i].IsNull() || removedFaces[i].ShapeType() != TopAbs_FACE || !faces.Contains(removedFaces[i]))
            return Error{ErrorCode::InvalidInput, "removed face #" + std::to_string(i) + " is not a face of the body"};
        closing.Append(removedFaces[i]);
    }
    try {
        const double offset = direction == ShellDirection::Inward ? -thickness : thickness;
        BRepOffsetAPI_MakeThickSolid mk;
        mk.MakeThickSolidByJoin(body, closing, offset, 1e-6, BRepOffset_Skin, false, false, GeomAbs_Intersection);
        if (!mk.IsDone()) return Error{ErrorCode::OperationFailed, "BRepOffsetAPI_MakeThickSolid failed (thickness too large?)"};
        if (std::string pr = checkSolidResult(mk.Shape()); !pr.empty())
            return Error{ErrorCode::OperationFailed, "shell result rejected: " + pr};
        const double v0 = volumeOf(body), v1 = volumeOf(mk.Shape());
        if (direction == ShellDirection::Inward && !(v1 < v0))
            return Error{ErrorCode::OperationFailed, "inward shell did not reduce the volume"};
        FeatureResult out;
        out.shape = mk.Shape();
        out.history = algoHistory(body, mk);
        return out;
    } catch (const Standard_Failure& e) {
        return Error{ErrorCode::OperationFailed, occtMessage("shell", e.what())};
    }
}

Result<FeatureResult> draftFaces(const TopoDS_Shape& body, const std::vector<TopoDS_Shape>& faces,
                                 const gp_Dir& pullDirection, double angle, const gp_Ax3& neutralPlane) {
    if (std::string e = checkSolidInput(body, "body"); !e.empty()) return Error{ErrorCode::InvalidInput, e};
    if (!(std::isfinite(angle) && angle != 0.0 && std::abs(angle) < M_PI / 2))
        return Error{ErrorCode::InvalidInput, "draft angle must be non-zero and in (-pi/2, pi/2)"};
    if (faces.empty()) return Error{ErrorCode::InvalidInput, "no faces given"};
    ShapeMap all;
    TopExp::MapShapes(body, TopAbs_FACE, all);
    try {
        BRepOffsetAPI_DraftAngle da(body);
        const gp_Pln neutral(neutralPlane);
        for (std::size_t i = 0; i < faces.size(); ++i) {
            if (faces[i].IsNull() || faces[i].ShapeType() != TopAbs_FACE || !all.Contains(faces[i]))
                return Error{ErrorCode::InvalidInput, "draft face #" + std::to_string(i) + " is not a face of the body"};
            // Ориентация грани берётся из тела: внешняя нормаль определяет сторону наклона.
            TopoDS_Face f;
            for (TopExp_Explorer ex(body, TopAbs_FACE); ex.More(); ex.Next())
                if (ex.Current().IsSame(faces[i])) { f = TopoDS::Face(ex.Current()); break; }
            da.Add(f, pullDirection, angle, neutral);
            if (!da.AddDone())
                return Error{ErrorCode::OperationFailed, "draft cannot be applied to face #" + std::to_string(i)};
        }
        da.Build();
        if (!da.IsDone()) return Error{ErrorCode::OperationFailed, "BRepOffsetAPI_DraftAngle failed"};
        if (std::string pr = checkSolidResult(da.Shape()); !pr.empty())
            return Error{ErrorCode::OperationFailed, "draft result rejected: " + pr};
        FeatureResult out;
        out.shape = da.Shape();
        out.history = algoHistory(body, da);
        return out;
    } catch (const Standard_Failure& e) {
        return Error{ErrorCode::OperationFailed, occtMessage("draftFaces", e.what())};
    }
}

}  // namespace bendcore::modeling
