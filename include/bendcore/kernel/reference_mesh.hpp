#pragma once

// Reference meshes: STL (binary and ASCII) and 3MF files read as triangle meshes for reference
// in parts and assemblies (docs/kernel/ANALYSIS.md, section 7). A reference mesh is not a B-rep:
// it takes no part in boolean operations, mass properties, BOM or interference. The kernel checks
// the file (size and triangle limits, truncated binary STL, non-finite coordinates), drops
// zero-area triangles, welds the vertices, places the mesh (unit scale, rotation, translation)
// and measures and cuts it: bounding box, area, divergence-theorem volume, closure, a point on a
// triangle (with vertex snapping), ray picking, the nearest point and the section by a plane.

#include <bendcore/kernel/mesh_export.hpp>
#include <bendcore/kernel/result.hpp>
#include <bendcore/kernel/section.hpp>
#include <bendcore/kernel/shape.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bendcore::kernel {

enum class MeshFileFormat { Stl, ThreeMf };

// "stl" | "3mf"; from a file name: the extension, case-insensitive.
const char* toString(MeshFileFormat format) noexcept;
std::optional<MeshFileFormat> meshFileFormatFromString(std::string_view text) noexcept;
std::optional<MeshFileFormat> meshFileFormatOfName(std::string_view fileName) noexcept;

struct MeshReadLimits {
    std::size_t maxBytes = 128ull * 1024ull * 1024ull;  // whole file
    std::size_t maxTriangles = 4'000'000;
};

struct MeshReadReport {
    std::string encoding;            // "binary" | "ascii" (STL), "3mf"
    std::size_t facetsRead = 0;      // triangles in the file
    std::size_t degenerateDropped = 0;  // zero-area triangles (and triangles collapsed by welding)
    std::size_t objects = 1;         // 3MF build items
};

struct ReferenceMeshData {
    TriangleMesh mesh;      // welded, file units (no placement applied)
    MeshReadReport report;
};

// Reads a mesh file. Errors:
//   InvalidInput  empty file, file over limits.maxBytes, more than limits.maxTriangles,
//                 no triangle of non-zero area;
//   ParseError    truncated or malformed binary/ASCII STL, non-finite coordinates, 3MF
//                 without a model part, malformed model XML;
//   Unsupported   3MF components or a deflated 3MF entry in a build without zlib.
// Vertices are welded by exact equality of their coordinates; a triangle is degenerate when its
// area is <= 1e-12 of the squared bounding-box diagonal (or two corners weld together).
Result<ReferenceMeshData> readReferenceMesh(std::string_view data, MeshFileFormat format,
                                            const MeshReadLimits& limits = {});

// Length units of a mesh file: "mm" (1), "cm" (10), "m" (1000), "in" (25.4) -> mm per unit.
std::optional<double> meshUnitScale(std::string_view unit) noexcept;

// Placement of a reference mesh: p' = translation + R(rotation) * (unitScale * p), where R
// rotates about the fixed X axis, then Y, then Z (degrees): R = Rz * Ry * Rx.
struct MeshPlacement {
    double unitScale = 1.0;
    Point3 rotationDegrees{0.0, 0.0, 0.0};
    Point3 translation{0.0, 0.0, 0.0};
};

// Applies the placement (gp_Trsf); InvalidInput for a non-positive or non-finite scale or
// non-finite angles/translation. Orientation of the triangles is kept (uniform scale > 0).
Result<TriangleMesh> placeMesh(const TriangleMesh& mesh, const MeshPlacement& placement);

struct MeshProperties {
    std::size_t vertices = 0;
    std::size_t triangles = 0;
    Point3 min{};
    Point3 max{};
    double area = 0.0;    // mm^2
    double volume = 0.0;  // mm^3, divergence theorem; meaningful when closed
    bool closed = false;  // every edge shared by two triangles with opposite directions
    std::size_t openEdges = 0;  // boundary, non-manifold or misoriented edges
};
MeshProperties meshProperties(const TriangleMesh& mesh);

struct MeshPoint {
    Point3 point{};
    std::size_t triangle = 0;
    std::optional<std::size_t> vertex;  // set when snapped to a vertex
};

// Closest point of triangle `triangle` to `hint`; snapped to the nearest corner when that corner
// is within `snapDistance` of the closest point. InvalidInput for a bad triangle index.
Result<MeshPoint> pointOnTriangle(const TriangleMesh& mesh, std::size_t triangle, const Point3& hint,
                                  double snapDistance = 0.0);

// Nearest point of the whole mesh to `p` (all triangles); InvalidInput for an empty mesh.
Result<MeshPoint> nearestPointOnMesh(const TriangleMesh& mesh, const Point3& p);

// First triangle hit by the ray origin + t * direction, t >= 0 (Moller-Trumbore, both sides).
std::optional<MeshPoint> raycastMesh(const TriangleMesh& mesh, const Point3& origin, const Point3& direction);

struct MeshSectionContour {
    std::vector<Point3> points;  // without repeating the first point when closed
    bool closed = false;
};

struct MeshSection {
    std::vector<MeshSectionContour> contours;
    // Net area enclosed by the contours (outer loops positive, holes negative) for a closed,
    // consistently oriented mesh, mm^2; 0 when the plane misses the mesh.
    double area = 0.0;
    double length = 0.0;  // total contour length, mm
};

// Section of the mesh by the plane: every triangle crossing the plane gives a segment (vertices
// on the plane count as lying on the positive side), segments are chained into contours through
// the shared mesh edges, and points closer than `tolerance` (mm) to the line of their neighbours
// are dropped. A plane that misses the mesh gives no contours. InvalidInput for a zero normal.
Result<MeshSection> sectionMesh(const TriangleMesh& mesh, const SectionPlane& plane, double tolerance = 1.0e-6);

}  // namespace bendcore::kernel
