#include "sketch_internal.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

#include <BRepAdaptor_Curve.hxx>
#include <BRep_Tool.hxx>
#include <Geom2d_TrimmedCurve.hxx>
#include <GeomAPI.hxx>
#include <GeomProjLib.hxx>
#include <Geom_Curve.hxx>
#include <Geom_Plane.hxx>
#include <Geom_TrimmedCurve.hxx>
#include <TopLoc_Location.hxx>

namespace bendcore::modeling {

Result<ProjectedEdge> projectEdge(const SketchPlacement& placement, const TopoDS_Edge& edge,
                                  double tolerance) {
    using R = Result<ProjectedEdge>;
    if (edge.IsNull()) return R::failure(ErrorCode::InvalidInput, "edge is null");
    if (!(tolerance > 0.0)) return R::failure(ErrorCode::InvalidInput, "tolerance must be positive");
    try {
        if (BRep_Tool::Degenerated(edge)) {
            return R::failure(ErrorCode::InvalidInput, "edge is degenerated (collapsed to a point)");
        }
        TopLoc_Location location;
        double first = 0.0;
        double last = 0.0;
        occ::handle<Geom_Curve> curve = BRep_Tool::Curve(edge, location, first, last);
        if (curve.IsNull()) {
            return R::failure(ErrorCode::InvalidInput, "edge has no 3D curve");
        }
        if (!location.IsIdentity()) {
            curve = occ::down_cast<Geom_Curve>(curve->Transformed(location.Transformation()));
        }

        // An edge along the sketch normal collapses to a point; a planar curve seen edge-on
        // (its plane contains the sketch normal) collapses to a segment.
        constexpr int kSamples = 64;
        std::vector<Point2> samples;
        samples.reserve(kSamples + 1);
        for (int i = 0; i <= kSamples; ++i) {
            samples.push_back(
                placement.toLocal(curve->Value(first + (last - first) * i / kSamples)));
        }
        const Point2 p0 = samples.front();
        Point2 far = p0;
        double extent = 0.0;
        for (const Point2& p : samples) {
            const double d = std::hypot(p.x - p0.x, p.y - p0.y);
            if (d > extent) {
                extent = d;
                far = p;
            }
        }
        ProjectedEdge out;
        if (extent <= tolerance) {
            out.point = p0;
            return out;
        }
        const double dx = (far.x - p0.x) / extent;
        const double dy = (far.y - p0.y) / extent;
        auto along = [&](double t) {
            const Point2 p = placement.toLocal(curve->Value(t));
            return (p.x - p0.x) * dx + (p.y - p0.y) * dy;
        };
        double offLine = 0.0;
        std::size_t iMin = 0;
        std::size_t iMax = 0;
        std::vector<double> s(samples.size());
        for (std::size_t i = 0; i < samples.size(); ++i) {
            const Point2& p = samples[i];
            s[i] = (p.x - p0.x) * dx + (p.y - p0.y) * dy;
            offLine = std::max(offLine, std::abs(-(p.x - p0.x) * dy + (p.y - p0.y) * dx));
            if (s[i] < s[iMin]) iMin = i;
            if (s[i] > s[iMax]) iMax = i;
        }
        // Golden-section refinement of an extreme between the neighbouring samples.
        auto refine = [&](std::size_t i, double sign) {
            const double step = (last - first) / kSamples;
            double a = first + step * (i == 0 ? 0.0 : static_cast<double>(i) - 1.0);
            double b = first + step * std::min<double>(kSamples, static_cast<double>(i) + 1.0);
            const double g = 0.5 * (std::sqrt(5.0) - 1.0);
            for (int k = 0; k < 80; ++k) {
                const double c = b - g * (b - a);
                const double d = a + g * (b - a);
                if (sign * along(c) > sign * along(d)) b = d; else a = c;
            }
            return sign * std::max(sign * along(0.5 * (a + b)), sign * s[i]);
        };
        const bool straight = BRepAdaptor_Curve(edge).GetType() == GeomAbs_Line;
        if (offLine <= tolerance && !straight) {
            const double sMin = refine(iMin, -1.0);
            const double sMax = refine(iMax, 1.0);
            // Extreme points of the projection, e.g. a circle seen edge-on gives its diameter.
            out.curve = LineSegment2d{Point2{p0.x + sMin * dx, p0.y + sMin * dy},
                                      Point2{p0.x + sMax * dx, p0.y + sMax * dy}};
            return out;
        }

        const gp_Pln plane = placement.plane();
        occ::handle<Geom_Curve> projected = GeomProjLib::ProjectOnPlane(
            new Geom_TrimmedCurve(curve, first, last), new Geom_Plane(plane), placement.normal(),
            true);
        if (projected.IsNull()) {
            return R::failure(ErrorCode::OperationFailed, "projection onto the sketch plane failed");
        }
        occ::handle<Geom2d_Curve> c2d = GeomAPI::To2d(projected, plane);
        if (c2d.IsNull()) {
            return R::failure(ErrorCode::OperationFailed, "projected curve is not planar");
        }
        double f2 = c2d->FirstParameter();
        double l2 = c2d->LastParameter();
        occ::handle<Geom2d_Curve> basis = c2d;
        if (auto trimmed = occ::down_cast<Geom2d_TrimmedCurve>(c2d)) basis = trimmed->BasisCurve();
        auto converted = detail::fromGeom2d(basis, f2, l2, tolerance);
        if (!converted.ok()) return converted.error();
        out.curve = converted.value();
        return out;
    } catch (const Standard_Failure& failure) {
        return R::failure(ErrorCode::OperationFailed,
                          "edge projection failed: " + detail::failureText(failure));
    }
}

Result<std::vector<ProjectedEdge>> projectEdges(const SketchPlacement& placement,
                                                const kernel::TopologyIndex& topology,
                                                const std::vector<std::uint32_t>& edgeIndices,
                                                double tolerance) {
    std::vector<ProjectedEdge> out;
    for (std::uint32_t index : edgeIndices) {
        if (index >= topology.edges().size()) {
            return Result<std::vector<ProjectedEdge>>::failure(
                ErrorCode::InvalidInput, "edge index " + std::to_string(index) + " out of range (" +
                                             std::to_string(topology.edges().size()) + " edges)");
        }
        auto projected = projectEdge(placement, topology.edges()[index].edge, tolerance);
        if (!projected.ok()) {
            return Result<std::vector<ProjectedEdge>>::failure(
                projected.error().code,
                "edge " + std::to_string(index) + ": " + projected.error().message);
        }
        projected.value().edgeIndex = index;
        out.push_back(std::move(projected).value());
    }
    return out;
}

Result<Point2> projectVertex(const SketchPlacement& placement,
                             const kernel::TopologyIndex& topology, std::uint32_t vertexIndex) {
    if (vertexIndex >= topology.vertices().size()) {
        return Result<Point2>::failure(ErrorCode::InvalidInput,
                                       "vertex index " + std::to_string(vertexIndex) +
                                           " out of range");
    }
    const auto& p = topology.vertices()[vertexIndex].point;
    return placement.toLocal(gp_Pnt(p[0], p[1], p[2]));
}

}  // namespace bendcore::modeling
