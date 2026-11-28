// Выдавливание и вырез выдавливанием (docs/modeling/FEATURES.md, раздел 2).

#include "feature_common.hpp"

#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepOffsetAPI_DraftAngle.hxx>
#include <BRepOffsetAPI_MakeThickSolid.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRep_Tool.hxx>
#include <GeomLib_IsPlanarSurface.hxx>
#include <Geom_Surface.hxx>
#include <BRepTools.hxx>
#include <Standard_Failure.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <gp_Pln.hxx>

#include <cmath>

namespace bendcore::modeling {

namespace {

using namespace detail;

bool finitePositive(double v) { return std::isfinite(v) && v > 0.0; }

// Грань, ограничивающая выдавливание: сама грань (UpToFace) или её поверхность,
// продолженная на size во все непериодические стороны (UpToSurface).
Result<TopoDS_Face> limitFace(const TopoDS_Face& face, bool extend, double size, const gp_Pnt& center) {
    if (!extend) return face;
    opencascade::handle<Geom_Surface> surf = BRep_Tool::Surface(face);
    GeomLib_IsPlanarSurface planar(surf, 1e-7);
    if (planar.IsPlanar()) {
        gp_Pln pln = planar.Plan();
        double u, v;
        // Центр квадрата — проекция центра модели на плоскость.
        const gp_Pnt o = pln.Location();
        const gp_Vec d(o, center);
        u = d.Dot(gp_Vec(pln.XAxis().Direction()));
        v = d.Dot(gp_Vec(pln.YAxis().Direction()));
        BRepBuilderAPI_MakeFace mf(pln, u - size, u + size, v - size, v + size);
        if (!mf.IsDone()) return Error{ErrorCode::OperationFailed, "cannot extend the limit plane"};
        return mf.Face();
    }
    double u0, u1, v0, v1, su0, su1, sv0, sv1;
    BRepTools::UVBounds(face, u0, u1, v0, v1);
    surf->Bounds(su0, su1, sv0, sv1);
    if (!surf->IsUPeriodic()) { u0 = std::max(su0, u0 - size); u1 = std::min(su1, u1 + size); }
    else { u0 = su0; u1 = su1; }
    if (!surf->IsVPeriodic()) { v0 = std::max(sv0, v0 - size); v1 = std::min(sv1, v1 + size); }
    else { v0 = sv0; v1 = sv1; }
    BRepBuilderAPI_MakeFace mf(surf, u0, u1, v0, v1, 1e-7);
    if (!mf.IsDone()) return Error{ErrorCode::OperationFailed, "cannot extend the limit surface"};
    return mf.Face();
}

}  // namespace

Result<FeatureResult> extrude(const Profile& profile, const TopoDS_Shape& target, const ExtrudeParams& p) {
    auto pln = validateProfile(profile);
    if (!pln) return pln.error();

    const bool upTo = p.extent == ExtentType::UpToFace || p.extent == ExtentType::UpToSurface;
    const bool through = p.extent == ExtentType::ThroughAll || p.extent == ExtentType::ThroughAllBoth;
    switch (p.extent) {
        case ExtentType::Blind:
        case ExtentType::Symmetric:
            if (!finitePositive(p.distance)) return Error{ErrorCode::InvalidInput, "extrude distance must be > 0"};
            break;
        case ExtentType::TwoSided:
            if (!(std::isfinite(p.distance) && std::isfinite(p.distance2) && p.distance >= 0.0 && p.distance2 >= 0.0))
                return Error{ErrorCode::InvalidInput, "two-sided distances must be >= 0"};
            if (!(p.distance + p.distance2 > 0.0))
                return Error{ErrorCode::InvalidInput, "two-sided extrude has zero total length"};
            break;
        default: break;
    }
    if (!(std::isfinite(p.draftAngle) && p.draftAngle >= 0.0 && p.draftAngle < M_PI / 2))
        return Error{ErrorCode::InvalidInput, "draft angle must be in [0, pi/2)"};
    const bool draft = p.draftAngle > 0.0;
    if (draft && (p.extent == ExtentType::Symmetric || p.extent == ExtentType::TwoSided ||
                  p.extent == ExtentType::ThroughAllBoth))
        return Error{ErrorCode::Unsupported, "draft is supported only for one-sided extents"};
    if (p.thin) {
        if (!finitePositive(p.thin->thickness)) return Error{ErrorCode::InvalidInput, "thin wall thickness must be > 0"};
        if (draft) return Error{ErrorCode::Unsupported, "thin wall together with draft is not supported"};
    }
    if (!target.IsNull()) {
        if (std::string e = checkSolidInput(target, "target body"); !e.empty()) return Error{ErrorCode::InvalidInput, e};
    } else if (p.mode == FeatureMode::Cut) {
        return Error{ErrorCode::InvalidInput, "cut requires a target body"};
    }
    if (through && target.IsNull()) return Error{ErrorCode::InvalidInput, "through-all extent requires a target body"};
    if (upTo && p.limit.IsNull()) return Error{ErrorCode::InvalidInput, "up-to extent requires a limit face"};

    try {
        const gp_Dir dir = p.reversed ? profile.placement.Direction().Reversed() : profile.placement.Direction();
        const gp_Pnt origin = profile.placement.Location();
        const double diag = boundingDiagonal({profile.face, target, p.limit});
        const double margin = 1.0 + 0.01 * diag;

        double start = 0.0, length = 0.0;
        switch (p.extent) {
            case ExtentType::Blind: length = p.distance; break;
            case ExtentType::Symmetric: start = -p.distance / 2; length = p.distance; break;
            case ExtentType::TwoSided: start = -p.distance2; length = p.distance + p.distance2; break;
            case ExtentType::ThroughAll:
            case ExtentType::ThroughAllBoth: {
                double lo, hi;
                projectionRange(target, origin, dir, lo, hi);
                if (p.extent == ExtentType::ThroughAll) {
                    if (hi <= 1e-7)
                        return Error{ErrorCode::InvalidInput, "target body lies behind the sketch plane"};
                    length = hi + margin;
                } else {
                    start = std::min(lo, 0.0) - margin;
                    length = std::max(hi, 0.0) + margin - start;
                }
                break;
            }
            case ExtentType::UpToFace:
            case ExtentType::UpToSurface: {
                double lo, hi;
                projectionRange(p.limit, origin, dir, lo, hi);
                if (hi <= 1e-7) return Error{ErrorCode::InvalidInput, "limit face lies behind the sketch plane"};
                length = hi + diag + margin;
                break;
            }
        }

        gp_Trsf shift;
        shift.SetTranslation(gp_Vec(dir) * start);
        auto base = transformCopy(profile.face, shift);
        if (!base) return base.error();
        BRepPrimAPI_MakePrism prism(base.value().shape, gp_Vec(dir) * length, false, true);
        if (!prism.IsDone()) return Error{ErrorCode::OperationFailed, "BRepPrimAPI_MakePrism failed"};
        TopoDS_Shape tool = prism.Shape();
        History history = sweepHistory(prism, tool, base.value());
        std::vector<std::string> warnings;

        if (draft) {
            const TopoDS_Shape first = prism.FirstShape();
            const TopoDS_Shape last = prism.LastShape();
            const gp_Pln neutral(origin, dir);
            // DraftAngle: положительный угол наклоняет грань внутрь тела по ходу Direction
            // (проверено тестом объёма усечённой пирамиды); Outward — отрицательный угол.
            const double angle = p.draftDirection == DraftDirection::Inward ? p.draftAngle : -p.draftAngle;
            BRepOffsetAPI_DraftAngle da(tool);
            for (TopExp_Explorer ex(tool, TopAbs_FACE); ex.More(); ex.Next()) {
                if (ex.Current().IsSame(first) || ex.Current().IsSame(last)) continue;
                da.Add(TopoDS::Face(ex.Current()), dir, angle, neutral);
                if (!da.AddDone())
                    return Error{ErrorCode::OperationFailed, "draft cannot be applied to a side face of the extrusion"};
            }
            da.Build();
            if (!da.IsDone()) return Error{ErrorCode::OperationFailed, "BRepOffsetAPI_DraftAngle failed"};
            History dh = algoHistory(tool, da);
            tool = da.Shape();
            mergeInto(history, dh);
        }

        if (p.thin) {
            ShapeList caps;
            caps.Append(prism.FirstShape());
            caps.Append(prism.LastShape());
            const double offset = p.thin->side == ThinSide::Inside ? -p.thin->thickness : p.thin->thickness;
            BRepOffsetAPI_MakeThickSolid thick;
            thick.MakeThickSolidByJoin(tool, caps, offset, 1e-6, BRepOffset_Skin, false, false, GeomAbs_Intersection);
            if (!thick.IsDone()) return Error{ErrorCode::OperationFailed, "thin wall: BRepOffsetAPI_MakeThickSolid failed"};
            History th = algoHistory(tool, thick);
            tool = thick.Shape();
            mergeInto(history, th);
        }

        if (upTo) {
            gp_Pnt center = origin;
            auto lf = limitFace(p.limit, p.extent == ExtentType::UpToSurface, 2.0 * diag + length, center);
            if (!lf) return lf.error();
            BRepPrimAPI_MakePrism beyond(lf.value(), gp_Vec(dir) * (length + diag + margin), false, true);
            if (!beyond.IsDone()) return Error{ErrorCode::OperationFailed, "cannot sweep the limit face"};
            auto trim = booleanWithHistory(kernel::BooleanType::Cut, {tool}, {beyond.Shape()}, true, false);
            if (!trim) return Error{trim.error().code, "up-to trim: " + trim.error().message};
            tool = trim.value().shape;
            double lo, hi;
            projectionRange(tool, origin, dir, lo, hi);
            if (hi >= length - 0.5 * margin)
                return Error{ErrorCode::InvalidInput, "profile is not fully bounded by the limit face"};
            if (countSolids(tool) != 1)
                return Error{ErrorCode::OperationFailed, "up-to trim produced " + std::to_string(countSolids(tool)) +
                                                             " solids"};
            mergeInto(history, trim.value().history);
            for (const auto& cap : trim.value().untrackedImages) history->AddGenerated(profile.face, cap);
            warnings.insert(warnings.end(), trim.value().warnings.begin(), trim.value().warnings.end());
        }

        return detail::applyTool(p.mode, target, tool, history, std::move(warnings));
    } catch (const Standard_Failure& e) {
        return Error{ErrorCode::OperationFailed, occtMessage("extrude", e.what())};
    }
}

}  // namespace bendcore::modeling
