#pragma once

// Reference (datum) geometry: planes, axes, points and coordinate systems built from
// analytic input or from faces, edges and vertices of a shape (docs/modeling/REFERENCE_GEOMETRY.md).
//
// Planes are gp_Pln (a right-handed gp_Ax3: the normal is Axis().Direction()), axes are gp_Ax1,
// points gp_Pnt, coordinate systems gp_Ax3. Sub-shapes are taken either directly (TopoDS_*,
// e.g. resolved by topological naming) or by TopologyIndex index. Degenerate input is an
// InvalidInput error; a sub-shape of an unsuitable kind is an InvalidInput error naming it.

#include <bendcore/kernel/result.hpp>
#include <bendcore/kernel/shape.hpp>

#include <cstdint>

#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Vertex.hxx>
#include <gp_Ax1.hxx>
#include <gp_Ax3.hxx>
#include <gp_Dir.hxx>
#include <gp_Pln.hxx>
#include <gp_Pnt.hxx>

namespace bendcore::modeling {

struct ReferenceTolerance {
    double linear = 1e-7;   // mm: coincident points, zero distances
    double angular = 1e-9;  // rad: parallel / collinear directions
};

// ----- planes ------------------------------------------------------------------------------

// Plane moved by `distance` along its normal (negative = against it).
kernel::Result<gp_Pln> planeOffset(const gp_Pln& base, double distance);
// Planar face: offset along the outward face normal; the result normal is the outward normal.
kernel::Result<gp_Pln> planeOffsetFromFace(const TopoDS_Face& face, double distance);
kernel::Result<gp_Pln> planeOffsetFromFace(const kernel::TopologyIndex& topology,
                                           std::uint32_t faceIndex, double distance);

// Base plane rotated by `angle` (rad, right-hand rule about axis direction) around an axis that
// lies in the base plane. Error when the axis is not in the plane.
kernel::Result<gp_Pln> planeAtAngle(const gp_Pln& base, const gp_Ax1& axis, double angle,
                                    const ReferenceTolerance& tol = {});

// Plane through three points: origin p1, X towards p2, normal (p2-p1)x(p3-p1).
// Error when points coincide or are collinear.
kernel::Result<gp_Pln> planeThroughPoints(const gp_Pnt& p1, const gp_Pnt& p2, const gp_Pnt& p3,
                                          const ReferenceTolerance& tol = {});

// Plane tangent to a cylindrical face along the generator nearest to `near`: the normal is the
// outward face normal there (radial, flipped for a reversed face such as a hole), the origin is the
// foot of `near` on that generator, X is the cylinder axis direction. Error when the face is not
// cylindrical or `near` lies on the axis.
kernel::Result<gp_Pln> planeTangentToCylinder(const TopoDS_Face& face, const gp_Pnt& near,
                                              const ReferenceTolerance& tol = {});
kernel::Result<gp_Pln> planeTangentToCylinder(const kernel::TopologyIndex& topology,
                                              std::uint32_t faceIndex, const gp_Pnt& near,
                                              const ReferenceTolerance& tol = {});

// Plane halfway between two parallel planes, normal of the first. Error when not parallel or
// coincident.
kernel::Result<gp_Pln> planeMidway(const gp_Pln& a, const gp_Pln& b,
                                   const ReferenceTolerance& tol = {});
kernel::Result<gp_Pln> planeMidway(const TopoDS_Face& a, const TopoDS_Face& b,
                                   const ReferenceTolerance& tol = {});
kernel::Result<gp_Pln> planeMidway(const kernel::TopologyIndex& topology, std::uint32_t faceA,
                                   std::uint32_t faceB, const ReferenceTolerance& tol = {});

// Plane of a planar face with the outward normal.
kernel::Result<gp_Pln> planeOfFace(const TopoDS_Face& face);

// ----- axes --------------------------------------------------------------------------------

// From p1 towards p2. Error when the points coincide.
kernel::Result<gp_Ax1> axisThroughPoints(const gp_Pnt& p1, const gp_Pnt& p2,
                                         const ReferenceTolerance& tol = {});
// Axis of a cylindrical or conical face (direction of the surface axis). Other surfaces are an
// error.
kernel::Result<gp_Ax1> axisOfFace(const TopoDS_Face& face);
kernel::Result<gp_Ax1> axisOfFace(const kernel::TopologyIndex& topology, std::uint32_t faceIndex);
// Straight edge: origin at its first vertex, direction along the edge (edge orientation honoured).
kernel::Result<gp_Ax1> axisOfEdge(const TopoDS_Edge& edge, const ReferenceTolerance& tol = {});
kernel::Result<gp_Ax1> axisOfEdge(const kernel::TopologyIndex& topology, std::uint32_t edgeIndex,
                                  const ReferenceTolerance& tol = {});
// Intersection line of two planes, direction na x nb, origin = point of the line closest to the
// origin of plane a. Error when the planes are parallel.
kernel::Result<gp_Ax1> axisOfPlaneIntersection(const gp_Pln& a, const gp_Pln& b,
                                               const ReferenceTolerance& tol = {});

// ----- points ------------------------------------------------------------------------------

kernel::Result<gp_Pnt> pointOfVertex(const kernel::TopologyIndex& topology,
                                     std::uint32_t vertexIndex);
// Point at a fraction 0..1 of the edge length from its first vertex (edge orientation honoured).
kernel::Result<gp_Pnt> pointOnEdge(const TopoDS_Edge& edge, double lengthFraction);
kernel::Result<gp_Pnt> pointOnEdge(const kernel::TopologyIndex& topology, std::uint32_t edgeIndex,
                                   double lengthFraction);
// Centre of a circular edge (full circle or arc). Error for other curves.
kernel::Result<gp_Pnt> pointCenterOfEdge(const TopoDS_Edge& edge);
kernel::Result<gp_Pnt> pointCenterOfEdge(const kernel::TopologyIndex& topology,
                                         std::uint32_t edgeIndex);
// Area centroid of a face.
kernel::Result<gp_Pnt> pointCentroidOfFace(const TopoDS_Face& face);
// Intersection of an axis with a plane. Error when the axis is parallel to the plane.
kernel::Result<gp_Pnt> pointAxisPlaneIntersection(const gp_Ax1& axis, const gp_Pln& plane,
                                                  const ReferenceTolerance& tol = {});
// Orthogonal projection of a point onto a plane.
gp_Pnt pointProjectedOnPlane(const gp_Pnt& point, const gp_Pln& plane);

// ----- coordinate systems ------------------------------------------------------------------

// Right-handed system: Z = zDirection, X = xDirection made orthogonal to Z.
// Error when the directions are parallel.
kernel::Result<gp_Ax3> coordinateSystem(const gp_Pnt& origin, const gp_Dir& zDirection,
                                        const gp_Dir& xDirection,
                                        const ReferenceTolerance& tol = {});
// Origin, a point on +X and a point in the XY plane with y > 0. Error when degenerate.
kernel::Result<gp_Ax3> coordinateSystemFromPoints(const gp_Pnt& origin, const gp_Pnt& onX,
                                                  const gp_Pnt& inXYPlane,
                                                  const ReferenceTolerance& tol = {});
// Origin projected on the plane, axes of the plane.
gp_Ax3 coordinateSystemOnPlane(const gp_Pln& plane, const gp_Pnt& origin);

}  // namespace bendcore::modeling
