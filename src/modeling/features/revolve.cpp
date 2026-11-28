// Вращение и вырез вращением (docs/modeling/FEATURES.md, раздел 3).

#include "feature_common.hpp"

#include <BRepAdaptor_Curve.hxx>
#include <BRepPrimAPI_MakeRevol.hxx>
#include <BRep_Tool.hxx>
#include <Standard_Failure.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>

#include <cmath>
#include <optional>

namespace bendcore::modeling {

using namespace detail;

Result<FeatureResult> revolve(const Profile& profile, const TopoDS_Shape& target, const RevolveParams& p) {
    auto pln = validateProfile(profile);
    if (!pln) return pln.error();
    const double twoPi = 2.0 * M_PI;
    const double angTol = 1e-12;

    double start = 0.0, sweep = 0.0;
    switch (p.extent) {
        case RevolveExtent::Angle:
        case RevolveExtent::Symmetric:
            if (!(std::isfinite(p.angle) && p.angle > 0.0 && p.angle <= twoPi + angTol))
                return Error{ErrorCode::InvalidInput, "revolve angle must be in (0, 2*pi]"};
            sweep = std::min(p.angle, twoPi);
            start = p.extent == RevolveExtent::Symmetric ? -sweep / 2 : 0.0;
            break;
        case RevolveExtent::Full: sweep = twoPi; break;
        case RevolveExtent::TwoSided:
            if (!(std::isfinite(p.angle) && std::isfinite(p.angle2) && p.angle >= 0.0 && p.angle2 >= 0.0))
                return Error{ErrorCode::InvalidInput, "two-sided revolve angles must be >= 0"};
            sweep = p.angle + p.angle2;
            if (!(sweep > 0.0 && sweep <= twoPi + angTol))
                return Error{ErrorCode::InvalidInput, "total revolve angle must be in (0, 2*pi]"};
            sweep = std::min(sweep, twoPi);
            start = -p.angle2;
            break;
    }
    if (!target.IsNull()) {
        if (std::string e = checkSolidInput(target, "target body"); !e.empty()) return Error{ErrorCode::InvalidInput, e};
    } else if (p.mode == FeatureMode::Cut) {
        return Error{ErrorCode::InvalidInput, "cut requires a target body"};
    }

    try {
        const gp_Dir normal = profile.placement.Direction();
        const gp_Dir axisDir = p.reversed ? p.axis.Direction().Reversed() : p.axis.Direction();
        const gp_Ax1 axis(p.axis.Location(), axisDir);
        if (std::abs(axisDir.Dot(normal)) > 1e-7 || pln.value().Distance(axis.Location()) > 1e-6)
            return Error{ErrorCode::InvalidInput, "revolve axis must lie in the sketch plane"};

        // Профиль по одну сторону от оси: знак расстояния в плоскости до оси.
        const gp_Vec side = gp_Vec(normal).Crossed(gp_Vec(axisDir));
        double dmin = 0.0, dmax = 0.0;
        bool first = true;
        for (TopExp_Explorer ex(profile.face, TopAbs_EDGE); ex.More(); ex.Next()) {
            const TopoDS_Edge e = TopoDS::Edge(ex.Current());
            if (BRep_Tool::Degenerated(e)) continue;
            BRepAdaptor_Curve c(e);
            for (int i = 0; i <= 32; ++i) {
                const double t = c.FirstParameter() + (c.LastParameter() - c.FirstParameter()) * i / 32.0;
                const double d = gp_Vec(axis.Location(), c.Value(t)).Dot(side);
                if (first) { dmin = dmax = d; first = false; }
                dmin = std::min(dmin, d);
                dmax = std::max(dmax, d);
            }
        }
        const double tol = 1e-6;
        if (dmin < -tol && dmax > tol) return Error{ErrorCode::InvalidInput, "profile crosses the revolve axis"};
        if (std::max(std::abs(dmin), std::abs(dmax)) <= tol)
            return Error{ErrorCode::InvalidInput, "profile lies on the revolve axis"};

        gp_Trsf rot;
        rot.SetRotation(axis, start);
        auto base = transformCopy(profile.face, rot);
        if (!base) return base.error();
        const bool full = sweep >= twoPi - angTol;
        std::optional<BRepPrimAPI_MakeRevol> made;
        if (full) made.emplace(base.value().shape, axis, false);
        else made.emplace(base.value().shape, axis, sweep, false);
        BRepPrimAPI_MakeRevol& rev = *made;
        if (!rev.IsDone()) return Error{ErrorCode::OperationFailed, "BRepPrimAPI_MakeRevol failed"};
        const TopoDS_Shape tool = rev.Shape();
        History history = sweepHistory(rev, tool, base.value());
        return detail::applyTool(p.mode, target, tool, history, {});
    } catch (const Standard_Failure& e) {
        return Error{ErrorCode::OperationFailed, occtMessage("revolve", e.what())};
    }
}

}  // namespace bendcore::modeling
