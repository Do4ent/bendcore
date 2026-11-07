// Отверстие: простое, цековка, зенковка; глухое и сквозное (docs/modeling/FEATURES.md, раздел 4).

#include "feature_common.hpp"

#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepClass_FaceClassifier.hxx>
#include <BRepPrimAPI_MakeRevol.hxx>
#include <BRep_Tool.hxx>
#include <GeomLib_IsPlanarSurface.hxx>
#include <Standard_Failure.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <gp_Ax2.hxx>
#include <gp_Pln.hxx>

#include <cmath>
#include <utility>
#include <vector>

namespace bendcore::modeling {

using namespace detail;

namespace {

bool pos(double v) { return std::isfinite(v) && v > 0.0; }

}  // namespace

Result<FeatureResult> hole(const TopoDS_Shape& target, const TopoDS_Face& face, const gp_Pnt& point,
                           const HoleParams& p, const std::optional<gp_Dir>& direction) {
    if (std::string e = checkSolidInput(target, "target body"); !e.empty()) return Error{ErrorCode::InvalidInput, e};
    if (face.IsNull()) return Error{ErrorCode::InvalidInput, "hole face is null"};
    if (!pos(p.diameter)) return Error{ErrorCode::InvalidInput, "hole diameter must be > 0"};
    if (p.end == HoleEnd::Blind && !pos(p.depth)) return Error{ErrorCode::InvalidInput, "hole depth must be > 0"};
    const bool point118 = p.end == HoleEnd::Blind && p.drillPoint;
    if (point118 && !(std::isfinite(p.drillPointAngle) && p.drillPointAngle > 0.0 && p.drillPointAngle < M_PI))
        return Error{ErrorCode::InvalidInput, "drill point angle must be in (0, pi)"};
    const double r = p.diameter / 2;
    if (p.type == HoleType::Counterbore) {
        if (!(pos(p.counterboreDiameter) && p.counterboreDiameter > p.diameter))
            return Error{ErrorCode::InvalidInput, "counterbore diameter must exceed the hole diameter"};
        if (!pos(p.counterboreDepth)) return Error{ErrorCode::InvalidInput, "counterbore depth must be > 0"};
        if (p.end == HoleEnd::Blind && p.counterboreDepth >= p.depth)
            return Error{ErrorCode::InvalidInput, "counterbore depth must be less than the hole depth"};
    }
    double csinkDepth = 0.0;
    if (p.type == HoleType::Countersink) {
        if (!(pos(p.countersinkDiameter) && p.countersinkDiameter > p.diameter))
            return Error{ErrorCode::InvalidInput, "countersink diameter must exceed the hole diameter"};
        if (!(std::isfinite(p.countersinkAngle) && p.countersinkAngle > 0.0 && p.countersinkAngle < M_PI))
            return Error{ErrorCode::InvalidInput, "countersink angle must be in (0, pi)"};
        csinkDepth = (p.countersinkDiameter - p.diameter) / 2 / std::tan(p.countersinkAngle / 2);
        if (p.end == HoleEnd::Blind && csinkDepth >= p.depth)
            return Error{ErrorCode::InvalidInput, "countersink is deeper than the hole"};
    }

    try {
        // Грань в составе тела (с её ориентацией).
        TopoDS_Face placed;
        for (TopExp_Explorer ex(target, TopAbs_FACE); ex.More(); ex.Next())
            if (ex.Current().IsSame(face)) { placed = TopoDS::Face(ex.Current()); break; }
        if (placed.IsNull()) return Error{ErrorCode::InvalidInput, "hole face does not belong to the target body"};
        GeomLib_IsPlanarSurface planar(BRep_Tool::Surface(placed), 1e-7);
        if (!planar.IsPlanar()) return Error{ErrorCode::InvalidInput, "hole face is not planar"};
        const gp_Pln pln = planar.Plan();
        if (pln.Distance(point) > 1e-6) return Error{ErrorCode::InvalidInput, "hole point is not on the face plane"};
        BRepClass_FaceClassifier cls(placed, point, 1e-6);
        if (cls.State() != TopAbs_IN) return Error{ErrorCode::InvalidInput, "hole point is outside the face"};

        gp_Dir outward = pln.Axis().Direction();
        if (placed.Orientation() == TopAbs_REVERSED) outward.Reverse();
        const gp_Dir dir = direction ? *direction : outward.Reversed();
        if (!(dir.Dot(outward) < -1e-6)) return Error{ErrorCode::InvalidInput, "hole direction must point into the material"};

        double depth = p.depth;
        if (p.end == HoleEnd::Through) {
            double lo, hi;
            projectionRange(target, point, dir, lo, hi);
            depth = hi + 1.0 + 0.01 * boundingDiagonal({target});
        }

        // Полупрофиль (радиус, глубина) по ходу сверления.
        std::vector<std::pair<double, double>> rz;
        rz.emplace_back(0.0, 0.0);
        switch (p.type) {
            case HoleType::Simple: rz.emplace_back(r, 0.0); break;
            case HoleType::Counterbore:
                rz.emplace_back(p.counterboreDiameter / 2, 0.0);
                rz.emplace_back(p.counterboreDiameter / 2, p.counterboreDepth);
                rz.emplace_back(r, p.counterboreDepth);
                break;
            case HoleType::Countersink:
                rz.emplace_back(p.countersinkDiameter / 2, 0.0);
                rz.emplace_back(r, csinkDepth);
                break;
        }
        rz.emplace_back(r, depth);
        if (point118) rz.emplace_back(0.0, depth + r / std::tan(p.drillPointAngle / 2));
        else rz.emplace_back(0.0, depth);

        const gp_Ax2 frame(point, dir);
        const gp_Vec x(frame.XDirection()), z(dir);
        BRepBuilderAPI_MakePolygon poly;
        for (const auto& [ri, zi] : rz) poly.Add(point.Translated(x * ri + z * zi));
        poly.Close();
        if (!poly.IsDone()) return Error{ErrorCode::OperationFailed, "hole profile polygon failed"};
        BRepBuilderAPI_MakeFace mf(poly.Wire(), true);
        if (!mf.IsDone()) return Error{ErrorCode::OperationFailed, "hole profile face failed"};
        BRepPrimAPI_MakeRevol rev(mf.Face(), gp_Ax1(point, dir), false);
        if (!rev.IsDone()) return Error{ErrorCode::OperationFailed, "hole tool revolve failed"};
        const TopoDS_Shape tool = rev.Shape();

        auto applied = detail::applyTool(FeatureMode::Cut, target, tool, History(), {});
        if (!applied) return applied.error();
        FeatureResult out = std::move(applied).value();
        // Грани отверстия построены «из» грани размещения.
        ShapeMap resFaces;
        TopExp::MapShapes(out.shape, TopAbs_FACE, resFaces);
        ShapeMap toolFaces;
        TopExp::MapShapes(tool, TopAbs_FACE, toolFaces);
        for (int i = 1; i <= toolFaces.Extent(); ++i) {
            if (resFaces.Contains(toolFaces(i))) {
                out.history->AddGenerated(placed, toolFaces(i));
                continue;
            }
            for (const TopoDS_Shape& img : out.history->Modified(toolFaces(i)))
                if (resFaces.Contains(img)) out.history->AddGenerated(placed, img);
        }
        return out;
    } catch (const Standard_Failure& e) {
        return Error{ErrorCode::OperationFailed, occtMessage("hole", e.what())};
    }
}

}  // namespace bendcore::modeling
