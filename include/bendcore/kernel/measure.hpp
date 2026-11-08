#pragma once

// Measurements on B-rep elements for the viewport: minimum distance with the closest points,
// angles between faces and edges, edge length, face area, radius/diameter, point coordinates.
// Every result carries the points the viewport needs to draw it (docs/kernel/ANALYSIS.md).

#include <bendcore/kernel/result.hpp>
#include <bendcore/kernel/shape.hpp>

#include <vector>

#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Vertex.hxx>

namespace bendcore::kernel {

struct DistanceResult {
    double distance = 0.0;  // mm
    Point3 pointOnFirst{};
    Point3 pointOnSecond{};
    std::size_t solutionCount = 0;  // pairs at the minimum distance found by BRepExtrema
};

// Minimum distance between any two shapes (vertices, edges, faces, solids, compounds):
// BRepExtrema_DistShapeShape. For a point inside a solid the distance is 0 (inner points
// count as part of the solid).
Result<DistanceResult> minimumDistance(const TopoDS_Shape& first, const TopoDS_Shape& second);

struct AngleResult {
    double radians = 0.0;
    double degrees = 0.0;
    Point3 vertex{};  // where to draw the arc: the shared point, or a point between the elements
    Point3 pointOnFirst{};
    Point3 pointOnSecond{};
};

// Angle between two planar faces: between their outward normals (face orientation taken
// into account), in [0, 180] degrees; 0 for parallel faces with the same outward side.
// The dihedral angle of the material at a shared edge is 180 minus this value.
// Unsupported for non-planar faces.
Result<AngleResult> angleBetweenFaces(const TopoDS_Face& first, const TopoDS_Face& second);

// Angle between two straight edges in [0, 180] degrees. Edges sharing a vertex are measured
// as rays from that vertex (corner angle); otherwise the directions first->last vertex of
// the edges as oriented are compared. Unsupported for curved edges.
Result<AngleResult> angleBetweenEdges(const TopoDS_Edge& first, const TopoDS_Edge& second);

struct LengthResult {
    double length = 0.0;  // mm
    Point3 start{};
    Point3 end{};
    Point3 middle{};  // point at half the arc length
};
Result<LengthResult> edgeLength(const TopoDS_Edge& edge);

struct AreaResult {
    double area = 0.0;  // mm^2
    Point3 centroid{};
};
Result<AreaResult> faceArea(const TopoDS_Face& face);

struct RadiusResult {
    double radius = 0.0;  // mm
    double diameter = 0.0;
    Point3 center{};      // circle centre, point on the cylinder/cone axis, sphere centre
    Point3 axis{};        // unit axis direction (zero vector for a sphere)
    Point3 pointOnElement{};  // where the leader touches the element
};

// Circular edges (full circle or arc), cylindrical, spherical and toroidal faces (the minor
// radius for a torus), cones (radius at the reference plane of the cone, its local origin).
// Unsupported for other geometry.
Result<RadiusResult> radiusOf(const TopoDS_Shape& edgeOrFace);

Result<Point3> vertexCoordinates(const TopoDS_Vertex& vertex);

}  // namespace bendcore::kernel
