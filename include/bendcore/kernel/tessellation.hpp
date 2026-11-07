#pragma once

// Tessellation for the viewport: triangles grouped by face, edge polylines, both keyed by
// TopologyIndex so that picking a triangle or a segment yields the B-rep face or edge.

#include <bendcore/kernel/result.hpp>
#include <bendcore/kernel/shape.hpp>

#include <cstdint>
#include <vector>

namespace bendcore::kernel {

struct TessellationParams {
    double linearDeflection = 0.1;   // mm, or a fraction of the edge/face size when relative
    double angularDeflection = 0.5;  // radians
    bool relative = false;
    bool inParallel = false;
    // true: any face without triangulation fails the call. false: such faces are listed in
    // Mesh::missingFaces and the rest of the mesh is returned.
    bool requireAllFaces = true;
};

struct FaceSpan {
    std::uint32_t faceIndex = 0;   // TopologyIndex::faces()
    std::uint32_t firstIndex = 0;  // into Mesh::indices
    std::uint32_t indexCount = 0;  // multiple of 3
};

struct EdgePolyline {
    std::uint32_t edgeIndex = 0;  // TopologyIndex::edges()
    std::vector<float> points;    // xyz triples
};

struct Mesh {
    std::vector<float> positions;       // xyz per vertex, global coordinates, mm
    std::vector<float> normals;         // unit xyz per vertex, pointing out of the material
    std::vector<std::uint32_t> indices;  // triangles, counter-clockwise seen from outside
    std::vector<FaceSpan> faceSpans;
    std::vector<EdgePolyline> edgePolylines;
    std::vector<std::uint32_t> missingFaces;  // faces BRepMesh produced no triangulation for
    std::vector<std::uint32_t> missingEdges;  // non-degenerated edges without a polygon
};

// Runs BRepMesh_IncrementalMesh on the shape (earlier triangulations are removed first, so
// the result depends only on the parameters) and extracts the mesh. The triangulation is
// stored on the shape's faces, as usual in OCCT.
Result<Mesh> tessellate(const Shape& shape, const TessellationParams& params = {});

}  // namespace bendcore::kernel
