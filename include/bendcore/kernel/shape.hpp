#pragma once

// Shape: the bendcore value type over an OCCT TopoDS_Shape, with validity checking,
// geometric properties and a deterministic topology index (docs/kernel/KERNEL_CORE.md).

#include <bendcore/kernel/result.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <NCollection_IndexedMap.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Vertex.hxx>

namespace bendcore::kernel {

using Point3 = std::array<double, 3>;
using ShapeIndexMap = NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher>;

struct BoundingBox {
    Point3 min{};
    Point3 max{};
};

struct VolumeProperties {
    // Signed: negative when the solid is oriented inside out.
    double volume = 0.0;
    Point3 centerOfMass{};
};

struct SurfaceProperties {
    double area = 0.0;
    Point3 centroid{};
};

struct TopologyCounts {
    std::size_t solids = 0;
    std::size_t shells = 0;
    std::size_t faces = 0;
    std::size_t edges = 0;
    std::size_t vertices = 0;
};

struct ValidityReport {
    bool valid = false;
    // One human-readable line per problem: "<sub-shape type> #<index>: <BRepCheck status>".
    std::vector<std::string> issues;
};

class Shape {
public:
    Shape() = default;
    explicit Shape(TopoDS_Shape shape) : shape_(std::move(shape)) {}

    bool isNull() const noexcept { return shape_.IsNull(); }
    const TopoDS_Shape& occt() const noexcept { return shape_; }

    // BRepCheck_Analyzer with geometric controls. A null shape is reported as invalid.
    ValidityReport checkValidity() const;

    // Tight box over the exact geometry (BRepBndLib::AddOptimal, triangulation ignored).
    Result<BoundingBox> boundingBox() const;

    // BRepGProp::VolumeProperties; requires at least one solid.
    Result<VolumeProperties> volumeProperties() const;

    // BRepGProp::SurfaceProperties; requires at least one face.
    Result<SurfaceProperties> surfaceProperties() const;

    // Number of distinct sub-shapes (TopExp::MapShapes semantics: same TShape and location).
    TopologyCounts counts() const;

private:
    TopoDS_Shape shape_;
};

enum class SurfaceType { Plane, Cylinder, Cone, Sphere, Torus, BSpline, Other };
enum class CurveType { Line, Circle, Ellipse, BSpline, Other, Degenerated };

const char* toString(SurfaceType type) noexcept;
const char* toString(CurveType type) noexcept;

struct FaceInfo {
    TopoDS_Face face;  // as met by TopExp_Explorer: accumulated location and orientation
    SurfaceType surface = SurfaceType::Other;
};

struct EdgeInfo {
    TopoDS_Edge edge;
    CurveType curve = CurveType::Other;
};

struct VertexInfo {
    TopoDS_Vertex vertex;
    Point3 point{};
};

// Indices 0..n-1 of faces, edges and vertices in TopExp::MapShapes order. The order depends
// only on the shape structure, so the same shape (or its BRep round trip) always yields the
// same indices.
class TopologyIndex {
public:
    static Result<TopologyIndex> build(const Shape& shape);

    const std::vector<FaceInfo>& faces() const noexcept { return faces_; }
    const std::vector<EdgeInfo>& edges() const noexcept { return edges_; }
    const std::vector<VertexInfo>& vertices() const noexcept { return vertices_; }

    // Index of a sub-shape (orientation ignored); nullopt when it is not part of the shape.
    std::optional<std::uint32_t> faceIndex(const TopoDS_Shape& face) const;
    std::optional<std::uint32_t> edgeIndex(const TopoDS_Shape& edge) const;
    std::optional<std::uint32_t> vertexIndex(const TopoDS_Shape& vertex) const;

private:
    std::vector<FaceInfo> faces_;
    std::vector<EdgeInfo> edges_;
    std::vector<VertexInfo> vertices_;
    ShapeIndexMap faceMap_;
    ShapeIndexMap edgeMap_;
    ShapeIndexMap vertexMap_;
};

}  // namespace bendcore::kernel
