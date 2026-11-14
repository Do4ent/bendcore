#include <bendcore/modeling/reference_geometry.hpp>

#include "sketch_internal.hpp"

#include <cmath>
#include <string>

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepGProp.hxx>
#include <BRep_Tool.hxx>
#include <GCPnts_AbscissaPoint.hxx>
#include <GProp_GProps.hxx>
#include <TopAbs_Orientation.hxx>
#include <gp_Vec.hxx>

namespace bendcore::modeling {

namespace {

template <typename T>
Result<T> invalid(const std::string& text) {
    return Result<T>::failure(ErrorCode::InvalidInput, text);
}

template <typename T>
Result<T> occtFailure(const Standard_Failure& failure) {
    return Result<T>::failure(ErrorCode::OperationFailed, detail::failureText(failure));
}

// Right-handed copy of a plane position.
gp_Ax3 direct(gp_Ax3 frame) {
    if (!frame.Direct()) frame.YReverse();
    return frame;
}

bool parallel(const gp_Dir& a, const gp_Dir& b, double angular) {
    return gp_Vec(a).Crossed(gp_Vec(b)).Magnitude() <= angular;
}

Result<TopoDS_Face> faceAt(const kernel::TopologyIndex& topology, std::uint32_t index) {
    if (index >= topology.faces().size()) {
        return invalid<TopoDS_Face>("face index " + std::to_string(index) + " out of range (" +
                                    std::to_string(topology.faces().size()) + " faces)");
    }
    return topology.faces()[index].face;
}

Result<TopoDS_Edge> edgeAt(const kernel::TopologyIndex& topology, std::uint32_t index) {
    if (index >= topology.edges().size()) {
        return invalid<TopoDS_Edge>("edge index " + std::to_string(index) + " out of range (" +
                                    std::to_string(topology.edges().size()) + " edges)");
    }
    return topology.edges()[index].edge;
}

Result<BRepAdaptor_Curve> edgeCurve(const TopoDS_Edge& edge) {
    if (edge.IsNull()) return invalid<BRepAdaptor_Curve>("edge is null");
    if (BRep_Tool::Degenerated(edge) || !BRep_Tool::IsGeometric(edge)) {
        return invalid<BRepAdaptor_Curve>("edge has no 3D curve");
    }
    return BRepAdaptor_Curve(edge);
}

}  // namespace

// ----- planes ------------------------------------------------------------------------------

Result<gp_Pln> planeOfFace(const TopoDS_Face& face) {
    if (face.IsNull()) return invalid<gp_Pln>("face is null");
    try {
        if (BRep_Tool::Surface(face).IsNull()) return invalid<gp_Pln>("face has no surface");
        const BRepAdaptor_Surface surface(face, false);
        if (surface.GetType() != GeomAbs_Plane) return invalid<gp_Pln>("face is not planar");
        gp_Ax3 frame = direct(surface.Plane().Position());
        // For a direct frame Direction() = X x Y = surface normal; flip for a reversed face.
        if (face.Orientation() == TopAbs_REVERSED) {
            frame.ZReverse();
            frame.YReverse();
        }
        return gp_Pln(frame);
    } catch (const Standard_Failure& failure) {
        return occtFailure<gp_Pln>(failure);
    }
}

Result<gp_Pln> planeOffset(const gp_Pln& base, double distance) {
    if (!std::isfinite(distance)) return invalid<gp_Pln>("offset distance is not finite");
    gp_Ax3 frame = direct(base.Position());
    frame.Translate(gp_Vec(frame.Direction()) * distance);
    return gp_Pln(frame);
}

Result<gp_Pln> planeOffsetFromFace(const TopoDS_Face& face, double distance) {
    auto plane = planeOfFace(face);
    if (!plane.ok()) return plane.error();
    return planeOffset(plane.value(), distance);
}

Result<gp_Pln> planeOffsetFromFace(const kernel::TopologyIndex& topology, std::uint32_t faceIndex,
                                   double distance) {
    auto face = faceAt(topology, faceIndex);
    if (!face.ok()) return face.error();
    return planeOffsetFromFace(face.value(), distance);
}

Result<gp_Pln> planeAtAngle(const gp_Pln& base, const gp_Ax1& axis, double angle,
                            const ReferenceTolerance& tol) {
    if (!std::isfinite(angle)) return invalid<gp_Pln>("angle is not finite");
    const gp_Ax3 frame = direct(base.Position());
    if (std::abs(gp_Vec(axis.Direction()).Dot(gp_Vec(frame.Direction()))) > tol.angular) {
        return invalid<gp_Pln>("rotation axis does not lie in the base plane (not parallel)");
    }
    if (base.Distance(axis.Location()) > tol.linear) {
        return invalid<gp_Pln>("rotation axis does not lie in the base plane (off the plane)");
    }
    return gp_Pln(frame.Rotated(axis, angle));
}

Result<gp_Pln> planeThroughPoints(const gp_Pnt& p1, const gp_Pnt& p2, const gp_Pnt& p3,
                                  const ReferenceTolerance& tol) {
    const gp_Vec u(p1, p2);
    const gp_Vec v(p1, p3);
    if (u.Magnitude() <= tol.linear || v.Magnitude() <= tol.linear ||
        p2.Distance(p3) <= tol.linear) {
        return invalid<gp_Pln>("two of the three points coincide");
    }
    const gp_Vec n = u.Crossed(v);
    if (n.Magnitude() <= tol.angular * u.Magnitude() * v.Magnitude()) {
        return invalid<gp_Pln>("the three points are collinear");
    }
    return gp_Pln(gp_Ax3(p1, gp_Dir(n), gp_Dir(u)));
}

Result<gp_Pln> planeTangentToCylinder(const TopoDS_Face& face, const gp_Pnt& near,
                                      const ReferenceTolerance& tol) {
    if (face.IsNull()) return invalid<gp_Pln>("face is null");
    try {
        if (BRep_Tool::Surface(face).IsNull()) return invalid<gp_Pln>("face has no surface");
        const BRepAdaptor_Surface surface(face, false);
        if (surface.GetType() != GeomAbs_Cylinder) {
            return invalid<gp_Pln>("face is not cylindrical");
        }
        const gp_Cylinder cylinder = surface.Cylinder();
        const gp_Ax1 axis = cylinder.Axis();
        const gp_Vec along(axis.Direction());
        const gp_Vec toPoint(axis.Location(), near);
        const gp_Pnt foot = axis.Location().Translated(along * toPoint.Dot(along));
        const gp_Vec radial(foot, near);
        if (radial.Magnitude() <= tol.linear) {
            return invalid<gp_Pln>("point lies on the cylinder axis; tangent direction undefined");
        }
        const gp_Dir outward(radial);
        // Surface normal of a cylinder points away from the axis for a direct frame.
        gp_Dir normal = outward;
        if (!cylinder.Position().Direct()) normal.Reverse();
        if (face.Orientation() == TopAbs_REVERSED) normal.Reverse();
        const gp_Pnt origin = foot.Translated(gp_Vec(outward) * cylinder.Radius());
        return gp_Pln(gp_Ax3(origin, normal, axis.Direction()));
    } catch (const Standard_Failure& failure) {
        return occtFailure<gp_Pln>(failure);
    }
}

Result<gp_Pln> planeTangentToCylinder(const kernel::TopologyIndex& topology,
                                      std::uint32_t faceIndex, const gp_Pnt& near,
                                      const ReferenceTolerance& tol) {
    auto face = faceAt(topology, faceIndex);
    if (!face.ok()) return face.error();
    return planeTangentToCylinder(face.value(), near, tol);
}

Result<gp_Pln> planeMidway(const gp_Pln& a, const gp_Pln& b, const ReferenceTolerance& tol) {
    const gp_Ax3 fa = direct(a.Position());
    const gp_Ax3 fb = direct(b.Position());
    if (!parallel(fa.Direction(), fb.Direction(), tol.angular)) {
        return invalid<gp_Pln>("planes are not parallel");
    }
    const double gap = gp_Vec(fa.Location(), fb.Location()).Dot(gp_Vec(fa.Direction()));
    if (std::abs(gap) <= tol.linear) return invalid<gp_Pln>("planes coincide");
    return gp_Pln(fa.Translated(gp_Vec(fa.Direction()) * (0.5 * gap)));
}

Result<gp_Pln> planeMidway(const TopoDS_Face& a, const TopoDS_Face& b,
                           const ReferenceTolerance& tol) {
    auto pa = planeOfFace(a);
    if (!pa.ok()) return pa.error();
    auto pb = planeOfFace(b);
    if (!pb.ok()) return pb.error();
    return planeMidway(pa.value(), pb.value(), tol);
}

Result<gp_Pln> planeMidway(const kernel::TopologyIndex& topology, std::uint32_t faceA,
                           std::uint32_t faceB, const ReferenceTolerance& tol) {
    auto a = faceAt(topology, faceA);
    if (!a.ok()) return a.error();
    auto b = faceAt(topology, faceB);
    if (!b.ok()) return b.error();
    return planeMidway(a.value(), b.value(), tol);
}

// ----- axes --------------------------------------------------------------------------------

Result<gp_Ax1> axisThroughPoints(const gp_Pnt& p1, const gp_Pnt& p2,
                                 const ReferenceTolerance& tol) {
    if (p1.Distance(p2) <= tol.linear) return invalid<gp_Ax1>("points coincide");
    return gp_Ax1(p1, gp_Dir(gp_Vec(p1, p2)));
}

Result<gp_Ax1> axisOfFace(const TopoDS_Face& face) {
    if (face.IsNull()) return invalid<gp_Ax1>("face is null");
    try {
        if (BRep_Tool::Surface(face).IsNull()) return invalid<gp_Ax1>("face has no surface");
        const BRepAdaptor_Surface surface(face, false);
        switch (surface.GetType()) {
            case GeomAbs_Cylinder: return surface.Cylinder().Axis();
            case GeomAbs_Cone: return surface.Cone().Axis();
            default: return invalid<gp_Ax1>("face is neither cylindrical nor conical");
        }
    } catch (const Standard_Failure& failure) {
        return occtFailure<gp_Ax1>(failure);
    }
}

Result<gp_Ax1> axisOfFace(const kernel::TopologyIndex& topology, std::uint32_t faceIndex) {
    auto face = faceAt(topology, faceIndex);
    if (!face.ok()) return face.error();
    return axisOfFace(face.value());
}

Result<gp_Ax1> axisOfEdge(const TopoDS_Edge& edge, const ReferenceTolerance& tol) {
    auto curve = edgeCurve(edge);
    if (!curve.ok()) return curve.error();
    try {
        const BRepAdaptor_Curve& c = curve.value();
        if (c.GetType() != GeomAbs_Line) return invalid<gp_Ax1>("edge is not straight");
        const bool reversed = edge.Orientation() == TopAbs_REVERSED;
        const gp_Pnt start = c.Value(reversed ? c.LastParameter() : c.FirstParameter());
        const gp_Pnt end = c.Value(reversed ? c.FirstParameter() : c.LastParameter());
        return axisThroughPoints(start, end, tol);
    } catch (const Standard_Failure& failure) {
        return occtFailure<gp_Ax1>(failure);
    }
}

Result<gp_Ax1> axisOfEdge(const kernel::TopologyIndex& topology, std::uint32_t edgeIndex,
                          const ReferenceTolerance& tol) {
    auto edge = edgeAt(topology, edgeIndex);
    if (!edge.ok()) return edge.error();
    return axisOfEdge(edge.value(), tol);
}

Result<gp_Ax1> axisOfPlaneIntersection(const gp_Pln& a, const gp_Pln& b,
                                       const ReferenceTolerance& tol) {
    const gp_Dir na = direct(a.Position()).Direction();
    const gp_Dir nb = direct(b.Position()).Direction();
    const gp_Vec dir = gp_Vec(na).Crossed(gp_Vec(nb));
    if (dir.Magnitude() <= tol.angular) return invalid<gp_Ax1>("planes are parallel");
    // Point on both planes closest to the origin of plane a: x = pa + s*na + t*nb solving
    // n_a.x = n_a.pa (=> s + t c = 0) and n_b.x = n_b.pb.
    const gp_Pnt pa = a.Location();
    const gp_Pnt pb = b.Location();
    const double c = gp_Vec(na).Dot(gp_Vec(nb));
    const double hb = gp_Vec(pa, pb).Dot(gp_Vec(nb));  // required n_b offset from pa
    // n_b.(s na + t nb) = s c + t = hb, s = -t c  =>  t (1 - c^2) = hb
    const double t = hb / (1.0 - c * c);
    const double s = -t * c;
    const gp_Pnt point = pa.Translated(gp_Vec(na) * s + gp_Vec(nb) * t);
    return gp_Ax1(point, gp_Dir(dir));
}

// ----- points ------------------------------------------------------------------------------

Result<gp_Pnt> pointOfVertex(const kernel::TopologyIndex& topology, std::uint32_t vertexIndex) {
    if (vertexIndex >= topology.vertices().size()) {
        return invalid<gp_Pnt>("vertex index " + std::to_string(vertexIndex) + " out of range");
    }
    const auto& p = topology.vertices()[vertexIndex].point;
    return gp_Pnt(p[0], p[1], p[2]);
}

Result<gp_Pnt> pointOnEdge(const TopoDS_Edge& edge, double lengthFraction) {
    if (!(lengthFraction >= 0.0 && lengthFraction <= 1.0)) {
        return invalid<gp_Pnt>("length fraction must be in [0, 1]");
    }
    auto curve = edgeCurve(edge);
    if (!curve.ok()) return curve.error();
    try {
        const BRepAdaptor_Curve& c = curve.value();
        const double length = GCPnts_AbscissaPoint::Length(c);
        const bool reversed = edge.Orientation() == TopAbs_REVERSED;
        const double from = reversed ? c.LastParameter() : c.FirstParameter();
        const double abscissa = (reversed ? -1.0 : 1.0) * lengthFraction * length;
        GCPnts_AbscissaPoint locator(c, abscissa, from);
        if (!locator.IsDone()) {
            return Result<gp_Pnt>::failure(ErrorCode::OperationFailed,
                                           "cannot locate the point along the edge");
        }
        return c.Value(locator.Parameter());
    } catch (const Standard_Failure& failure) {
        return occtFailure<gp_Pnt>(failure);
    }
}

Result<gp_Pnt> pointOnEdge(const kernel::TopologyIndex& topology, std::uint32_t edgeIndex,
                           double lengthFraction) {
    auto edge = edgeAt(topology, edgeIndex);
    if (!edge.ok()) return edge.error();
    return pointOnEdge(edge.value(), lengthFraction);
}

Result<gp_Pnt> pointCenterOfEdge(const TopoDS_Edge& edge) {
    auto curve = edgeCurve(edge);
    if (!curve.ok()) return curve.error();
    if (curve.value().GetType() != GeomAbs_Circle) return invalid<gp_Pnt>("edge is not circular");
    return curve.value().Circle().Location();
}

Result<gp_Pnt> pointCenterOfEdge(const kernel::TopologyIndex& topology, std::uint32_t edgeIndex) {
    auto edge = edgeAt(topology, edgeIndex);
    if (!edge.ok()) return edge.error();
    return pointCenterOfEdge(edge.value());
}

Result<gp_Pnt> pointCentroidOfFace(const TopoDS_Face& face) {
    if (face.IsNull()) return invalid<gp_Pnt>("face is null");
    try {
        GProp_GProps props;
        BRepGProp::SurfaceProperties(face, props);
        if (!(props.Mass() > 0.0)) return invalid<gp_Pnt>("face has zero area");
        return props.CentreOfMass();
    } catch (const Standard_Failure& failure) {
        return occtFailure<gp_Pnt>(failure);
    }
}

Result<gp_Pnt> pointAxisPlaneIntersection(const gp_Ax1& axis, const gp_Pln& plane,
                                          const ReferenceTolerance& tol) {
    const gp_Vec n(plane.Axis().Direction());
    const gp_Vec d(axis.Direction());
    const double denom = d.Dot(n);
    if (std::abs(denom) <= tol.angular) return invalid<gp_Pnt>("axis is parallel to the plane");
    const double t = gp_Vec(axis.Location(), plane.Location()).Dot(n) / denom;
    return axis.Location().Translated(d * t);
}

gp_Pnt pointProjectedOnPlane(const gp_Pnt& point, const gp_Pln& plane) {
    const gp_Vec n(plane.Axis().Direction());
    return point.Translated(-n * gp_Vec(plane.Location(), point).Dot(n));
}

// ----- coordinate systems ------------------------------------------------------------------

Result<gp_Ax3> coordinateSystem(const gp_Pnt& origin, const gp_Dir& zDirection,
                                const gp_Dir& xDirection, const ReferenceTolerance& tol) {
    const gp_Vec z(zDirection);
    const gp_Vec x = gp_Vec(xDirection) - z * gp_Vec(xDirection).Dot(z);
    if (x.Magnitude() <= tol.angular) return invalid<gp_Ax3>("X and Z directions are parallel");
    return gp_Ax3(origin, zDirection, gp_Dir(x));
}

Result<gp_Ax3> coordinateSystemFromPoints(const gp_Pnt& origin, const gp_Pnt& onX,
                                          const gp_Pnt& inXYPlane, const ReferenceTolerance& tol) {
    auto plane = planeThroughPoints(origin, onX, inXYPlane, tol);
    if (!plane.ok()) return plane.error();
    return plane.value().Position();
}

gp_Ax3 coordinateSystemOnPlane(const gp_Pln& plane, const gp_Pnt& origin) {
    gp_Ax3 frame = direct(plane.Position());
    frame.SetLocation(pointProjectedOnPlane(origin, plane));
    return frame;
}

}  // namespace bendcore::modeling
