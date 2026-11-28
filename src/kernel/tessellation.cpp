#include <bendcore/kernel/tessellation.hpp>

#include "occt_failure.hpp"

#include <cmath>
#include <exception>
#include <optional>
#include <utility>
#include <string>

#include <BRepAdaptor_Surface.hxx>
#include <BRepLProp_SLProps.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRepTools.hxx>
#include <BRep_Tool.hxx>
#include <IMeshTools_Parameters.hxx>
#include <NCollection_IndexedDataMap.hxx>
#include <NCollection_List.hxx>
#include <Poly_Polygon3D.hxx>
#include <Poly_PolygonOnTriangulation.hxx>
#include <Poly_Triangulation.hxx>
#include <Precision.hxx>
#include <Standard_Failure.hxx>
#include <TopExp.hxx>
#include <TopLoc_Location.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <TopoDS.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>

namespace bendcore::kernel {

using detail::failureText;

namespace {

using EdgeFaceMap = NCollection_IndexedDataMap<TopoDS_Shape, NCollection_List<TopoDS_Shape>, TopTools_ShapeMapHasher>;

void appendPoint(std::vector<float>& out, const gp_XYZ& p) {
    out.push_back(static_cast<float>(p.X()));
    out.push_back(static_cast<float>(p.Y()));
    out.push_back(static_cast<float>(p.Z()));
}

// Appends the triangulation of one face. Returns false when the face has none.
bool appendFace(const TopoDS_Face& face, std::uint32_t faceIndex, Mesh& mesh) {
    TopLoc_Location location;
    const occ::handle<Poly_Triangulation>& tri = BRep_Tool::Triangulation(face, location);
    if (tri.IsNull() || tri->NbNodes() == 0 || tri->NbTriangles() == 0) {
        return false;
    }
    const gp_Trsf trsf = location.Transformation();
    const bool reversed = face.Orientation() == TopAbs_REVERSED;
    const int nbNodes = tri->NbNodes();
    const auto base = static_cast<std::uint32_t>(mesh.positions.size() / 3);

    std::vector<gp_XYZ> nodes(nbNodes);
    for (int n = 1; n <= nbNodes; ++n) {
        nodes[n - 1] = tri->Node(n).Transformed(trsf).XYZ();
    }

    // Triangles in outward winding: the triangulation follows the surface parametrisation
    // (Du x Dv), which points inward on a REVERSED face.
    FaceSpan span;
    span.faceIndex = faceIndex;
    span.firstIndex = static_cast<std::uint32_t>(mesh.indices.size());
    std::vector<gp_XYZ> fanNormals(nbNodes, gp_XYZ(0.0, 0.0, 0.0));
    for (int t = 1; t <= tri->NbTriangles(); ++t) {
        int n1 = 0, n2 = 0, n3 = 0;
        tri->Triangle(t).Get(n1, n2, n3);
        if (reversed) {
            std::swap(n2, n3);
        }
        mesh.indices.push_back(base + static_cast<std::uint32_t>(n1 - 1));
        mesh.indices.push_back(base + static_cast<std::uint32_t>(n2 - 1));
        mesh.indices.push_back(base + static_cast<std::uint32_t>(n3 - 1));
        // Area-weighted triangle normal, used where the surface normal is undefined.
        const gp_XYZ cross = (nodes[n2 - 1] - nodes[n1 - 1]).Crossed(nodes[n3 - 1] - nodes[n1 - 1]);
        fanNormals[n1 - 1] += cross;
        fanNormals[n2 - 1] += cross;
        fanNormals[n3 - 1] += cross;
    }
    span.indexCount = static_cast<std::uint32_t>(mesh.indices.size()) - span.firstIndex;
    mesh.faceSpans.push_back(span);

    // Exact normals from the surface at the node's UV; the adaptor carries the face location,
    // so they come out in global coordinates.
    const bool useSurface = tri->HasUVNodes() && !BRep_Tool::Surface(face).IsNull();
    std::optional<BRepAdaptor_Surface> surface;
    if (useSurface) {
        surface.emplace(face, false);
    }
    for (int n = 1; n <= nbNodes; ++n) {
        appendPoint(mesh.positions, nodes[n - 1]);
        gp_XYZ normal = fanNormals[n - 1];
        bool fromSurface = false;
        if (surface) {
            const gp_Pnt2d uv = tri->UVNode(n);
            BRepLProp_SLProps props(*surface, uv.X(), uv.Y(), 1, Precision::Confusion());
            if (props.IsNormalDefined()) {
                normal = props.Normal().XYZ();
                if (reversed) {
                    normal.Reverse();
                }
                fromSurface = true;
            }
        }
        if (!fromSurface) {
            const double length = normal.Modulus();
            if (length > 0.0) {
                normal /= length;
            }
        }
        appendPoint(mesh.normals, normal);
    }
    return true;
}

// Polyline of one edge: its polygon on the triangulation of an adjacent face, or its own 3D
// polygon for an edge without faces. Returns false when neither exists.
bool appendEdge(const TopoDS_Edge& edge, std::uint32_t edgeIndex,
                const EdgeFaceMap& edgeFaces, Mesh& mesh) {
    EdgePolyline polyline;
    polyline.edgeIndex = edgeIndex;
    if (const NCollection_List<TopoDS_Shape>* faces = edgeFaces.Seek(edge)) {
        for (const TopoDS_Shape& faceShape : *faces) {
            TopLoc_Location location;
            const occ::handle<Poly_Triangulation>& tri =
                BRep_Tool::Triangulation(TopoDS::Face(faceShape), location);
            if (tri.IsNull()) {
                continue;
            }
            const occ::handle<Poly_PolygonOnTriangulation>& polygon =
                BRep_Tool::PolygonOnTriangulation(edge, tri, location);
            if (polygon.IsNull() || polygon->NbNodes() < 2) {
                continue;
            }
            const gp_Trsf trsf = location.Transformation();
            for (int i = 1; i <= polygon->NbNodes(); ++i) {
                appendPoint(polyline.points, tri->Node(polygon->Node(i)).Transformed(trsf).XYZ());
            }
            mesh.edgePolylines.push_back(std::move(polyline));
            return true;
        }
    }
    TopLoc_Location location;
    const occ::handle<Poly_Polygon3D>& polygon = BRep_Tool::Polygon3D(edge, location);
    if (polygon.IsNull() || polygon->NbNodes() < 2) {
        return false;
    }
    const gp_Trsf trsf = location.Transformation();
    const NCollection_Array1<gp_Pnt>& nodes = polygon->Nodes();
    for (int i = nodes.Lower(); i <= nodes.Upper(); ++i) {
        appendPoint(polyline.points, nodes(i).Transformed(trsf).XYZ());
    }
    mesh.edgePolylines.push_back(std::move(polyline));
    return true;
}

}  // namespace

Result<Mesh> tessellate(const Shape& shape, const TessellationParams& params) {
    using R = Result<Mesh>;
    if (shape.isNull()) {
        return R::failure(ErrorCode::InvalidInput, "shape is null");
    }
    if (!std::isfinite(params.linearDeflection) || params.linearDeflection <= Precision::Confusion()) {
        return R::failure(ErrorCode::InvalidInput, "linear deflection must be a positive number");
    }
    if (!std::isfinite(params.angularDeflection) || params.angularDeflection <= Precision::Angular()) {
        return R::failure(ErrorCode::InvalidInput, "angular deflection must be a positive number of radians");
    }
    try {
        Result<TopologyIndex> indexResult = TopologyIndex::build(shape);
        if (!indexResult) {
            return indexResult.error();
        }
        const TopologyIndex& index = indexResult.value();
        if (index.faces().empty() && index.edges().empty()) {
            return R::failure(ErrorCode::InvalidInput, "shape has no faces or edges to tessellate");
        }

        BRepTools::Clean(shape.occt());
        IMeshTools_Parameters meshParams;
        meshParams.Deflection = params.linearDeflection;
        meshParams.Angle = params.angularDeflection;
        meshParams.Relative = params.relative;
        meshParams.InParallel = params.inParallel;
        BRepMesh_IncrementalMesh mesher(shape.occt(), meshParams);
        if (!mesher.IsDone()) {
            return R::failure(ErrorCode::OperationFailed,
                              "BRepMesh_IncrementalMesh failed (status flags " +
                                  std::to_string(mesher.GetStatusFlags()) + ")");
        }

        Mesh mesh;
        for (std::size_t i = 0; i < index.faces().size(); ++i) {
            if (!appendFace(index.faces()[i].face, static_cast<std::uint32_t>(i), mesh)) {
                mesh.missingFaces.push_back(static_cast<std::uint32_t>(i));
            }
        }
        if (params.requireAllFaces && !mesh.missingFaces.empty()) {
            std::string list;
            for (std::size_t k = 0; k < mesh.missingFaces.size() && k < 16; ++k) {
                list += (k ? ", " : "") + std::to_string(mesh.missingFaces[k]);
            }
            if (mesh.missingFaces.size() > 16) {
                list += ", ...";
            }
            return R::failure(ErrorCode::OperationFailed,
                              std::to_string(mesh.missingFaces.size()) + " of " +
                                  std::to_string(index.faces().size()) +
                                  " faces have no triangulation (face indices: " + list + ")");
        }

        EdgeFaceMap edgeFaces;
        TopExp::MapShapesAndAncestors(shape.occt(), TopAbs_EDGE, TopAbs_FACE, edgeFaces);
        for (std::size_t i = 0; i < index.edges().size(); ++i) {
            const EdgeInfo& info = index.edges()[i];
            if (info.curve == CurveType::Degenerated) {
                continue;  // collapsed to a point: nothing to draw
            }
            if (!appendEdge(info.edge, static_cast<std::uint32_t>(i), edgeFaces, mesh)) {
                mesh.missingEdges.push_back(static_cast<std::uint32_t>(i));
            }
        }
        return mesh;
    } catch (const Standard_Failure& failure) {
        return R::failure(ErrorCode::OperationFailed, failureText(failure));
    } catch (const std::exception& e) {
        return R::failure(ErrorCode::OperationFailed, e.what());
    }
}

}  // namespace bendcore::kernel
