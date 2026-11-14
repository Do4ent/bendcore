#include <bendcore/kernel/tessellation.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <set>

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <BRep_Builder.hxx>
#include <Poly_Triangulation.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Face.hxx>
#include <gp_Pln.hxx>
#include <gp_Trsf.hxx>

using namespace bendcore::kernel;

namespace {

constexpr double kPi = 3.14159265358979323846;

gp_XYZ vertex(const Mesh& mesh, std::uint32_t i) {
    return gp_XYZ(mesh.positions[3 * i], mesh.positions[3 * i + 1], mesh.positions[3 * i + 2]);
}

gp_XYZ normal(const Mesh& mesh, std::uint32_t i) {
    return gp_XYZ(mesh.normals[3 * i], mesh.normals[3 * i + 1], mesh.normals[3 * i + 2]);
}

// Structural invariants every mesh must satisfy.
void expectWellFormed(const Mesh& mesh, std::size_t faceCount) {
    ASSERT_EQ(mesh.positions.size() % 3, 0u);
    ASSERT_EQ(mesh.normals.size(), mesh.positions.size());
    ASSERT_EQ(mesh.indices.size() % 3, 0u);
    const std::size_t vertexCount = mesh.positions.size() / 3;
    for (std::uint32_t index : mesh.indices) ASSERT_LT(index, vertexCount);

    // Spans tile [0, indices.size()) exactly: sorted, contiguous, no overlap, one per face.
    std::uint32_t next = 0;
    std::set<std::uint32_t> faces;
    for (const FaceSpan& span : mesh.faceSpans) {
        EXPECT_EQ(span.firstIndex, next);
        EXPECT_GT(span.indexCount, 0u);
        EXPECT_EQ(span.indexCount % 3, 0u);
        EXPECT_LT(span.faceIndex, faceCount);
        EXPECT_TRUE(faces.insert(span.faceIndex).second) << "face " << span.faceIndex << " twice";
        next = span.firstIndex + span.indexCount;
    }
    EXPECT_EQ(next, mesh.indices.size());
    EXPECT_EQ(faces.size() + mesh.missingFaces.size(), faceCount);

    for (std::size_t v = 0; v < vertexCount; ++v) {
        EXPECT_NEAR(normal(mesh, static_cast<std::uint32_t>(v)).Modulus(), 1.0, 1e-5);
    }
    // Vertex normals agree with the winding of every triangle (both outward).
    for (std::size_t t = 0; t < mesh.indices.size(); t += 3) {
        const gp_XYZ a = vertex(mesh, mesh.indices[t]);
        const gp_XYZ b = vertex(mesh, mesh.indices[t + 1]);
        const gp_XYZ c = vertex(mesh, mesh.indices[t + 2]);
        const gp_XYZ geometric = (b - a).Crossed(c - a);
        if (geometric.Modulus() < 1e-9) continue;
        for (int k = 0; k < 3; ++k) {
            EXPECT_GT(geometric.Dot(normal(mesh, mesh.indices[t + k])), 0.0) << "triangle " << t / 3;
        }
    }
}

double meshArea(const Mesh& mesh) {
    double area = 0.0;
    for (std::size_t t = 0; t < mesh.indices.size(); t += 3) {
        const gp_XYZ a = vertex(mesh, mesh.indices[t]);
        area += 0.5 * (vertex(mesh, mesh.indices[t + 1]) - a).Crossed(vertex(mesh, mesh.indices[t + 2]) - a).Modulus();
    }
    return area;
}

// Divergence theorem: positive and equal to the volume only for a closed, outward-wound mesh.
double meshSignedVolume(const Mesh& mesh) {
    double volume = 0.0;
    for (std::size_t t = 0; t < mesh.indices.size(); t += 3) {
        volume += vertex(mesh, mesh.indices[t])
                      .Dot(vertex(mesh, mesh.indices[t + 1]).Crossed(vertex(mesh, mesh.indices[t + 2]))) /
                  6.0;
    }
    return volume;
}

bool hasReversedFace(const Shape& shape) {
    for (const FaceInfo& face : TopologyIndex::build(shape).value().faces()) {
        if (face.face.Orientation() == TopAbs_REVERSED) return true;
    }
    return false;
}

Mesh mustTessellate(const Shape& shape, const TessellationParams& params = {}) {
    auto result = tessellate(shape, params);
    EXPECT_TRUE(result.ok()) << (result.ok() ? "" : result.error().message);
    return result.ok() ? result.value() : Mesh{};
}

}  // namespace

TEST(KernelTessellation, BoxMeshIsClosedAndOutward) {
    const Shape box(BRepPrimAPI_MakeBox(10.0, 20.0, 30.0).Shape());
    ASSERT_TRUE(hasReversedFace(box));  // exercises the REVERSED winding and normal flip
    const Mesh mesh = mustTessellate(box);
    expectWellFormed(mesh, 6);
    EXPECT_EQ(mesh.faceSpans.size(), 6u);
    EXPECT_NEAR(meshArea(mesh), 2200.0, 1e-3);
    EXPECT_NEAR(meshSignedVolume(mesh), 6000.0, 1e-2);

    const gp_XYZ center(5.0, 10.0, 15.0);
    for (const FaceSpan& span : mesh.faceSpans) {
        for (std::uint32_t k = span.firstIndex; k < span.firstIndex + span.indexCount; ++k) {
            const std::uint32_t v = mesh.indices[k];
            const gp_XYZ n = normal(mesh, v);
            EXPECT_GT(n.Dot(vertex(mesh, v) - center), 0.0);
            // Plane normals are one of the axis directions.
            EXPECT_NEAR(std::max({std::abs(n.X()), std::abs(n.Y()), std::abs(n.Z())}), 1.0, 1e-6);
        }
    }
}

TEST(KernelTessellation, SphereNormalsAreRadialIncludingPoles) {
    const double r = 7.0;
    const gp_XYZ c(1.0, -2.0, 3.0);
    const Shape sphere(BRepPrimAPI_MakeSphere(gp_Pnt(c), r).Shape());
    TessellationParams params;
    params.linearDeflection = 0.05;
    params.angularDeflection = 0.1;
    const Mesh mesh = mustTessellate(sphere, params);
    expectWellFormed(mesh, 1);
    const std::size_t vertexCount = mesh.positions.size() / 3;
    for (std::uint32_t v = 0; v < vertexCount; ++v) {
        const gp_XYZ radial = vertex(mesh, v) - c;
        EXPECT_NEAR(radial.Modulus(), r, 1e-4);
        EXPECT_GT(normal(mesh, v).Dot(radial), 0.0);
        EXPECT_GT(normal(mesh, v).Dot(radial / radial.Modulus()), 0.999) << "vertex " << v;
    }
    const double volume = 4.0 / 3.0 * kPi * r * r * r;
    EXPECT_NEAR(meshSignedVolume(mesh), volume, volume * 0.01);
}

TEST(KernelTessellation, CylinderHasOutwardNormalsOnEveryFace) {
    const double r = 5.0, h = 20.0;
    const Shape cylinder(BRepPrimAPI_MakeCylinder(r, h).Shape());
    ASSERT_TRUE(hasReversedFace(cylinder));
    const Mesh mesh = mustTessellate(cylinder);
    expectWellFormed(mesh, 3);
    const auto index = TopologyIndex::build(cylinder);
    for (const FaceSpan& span : mesh.faceSpans) {
        const SurfaceType type = index.value().faces()[span.faceIndex].surface;
        for (std::uint32_t k = span.firstIndex; k < span.firstIndex + span.indexCount; ++k) {
            const std::uint32_t v = mesh.indices[k];
            const gp_XYZ p = vertex(mesh, v);
            const gp_XYZ n = normal(mesh, v);
            if (type == SurfaceType::Cylinder) {
                const gp_XYZ radial(p.X(), p.Y(), 0.0);
                EXPECT_NEAR(n.Dot(radial / radial.Modulus()), 1.0, 1e-6);
            } else {
                // Bottom cap points down, top cap up.
                EXPECT_NEAR(n.Z(), p.Z() < h / 2 ? -1.0 : 1.0, 1e-6);
            }
        }
    }
    const double volume = kPi * r * r * h;
    EXPECT_NEAR(meshSignedVolume(mesh), volume, volume * 0.02);
}

TEST(KernelTessellation, MeshAreaConvergesToAnalyticArea) {
    const double r = 7.0;
    const double exact = 4.0 * kPi * r * r;
    double previousError = 1e300;
    for (double deflection : {0.5, 0.1, 0.02, 0.004}) {
        TessellationParams params;
        params.linearDeflection = deflection;
        params.angularDeflection = 0.5;
        // A fresh shape each time: the result must depend only on the parameters.
        const Mesh mesh = mustTessellate(Shape(BRepPrimAPI_MakeSphere(r).Shape()), params);
        const double error = std::abs(meshArea(mesh) - exact) / exact;
        EXPECT_LT(error, previousError) << "deflection " << deflection;
        previousError = error;
    }
    EXPECT_LT(previousError, 1e-3);

    // Coarse after fine on the same shape: earlier triangulation is discarded.
    const Shape sphere(BRepPrimAPI_MakeSphere(r).Shape());
    TessellationParams fine;
    fine.linearDeflection = 0.004;
    TessellationParams coarse;
    coarse.linearDeflection = 0.5;
    const std::size_t fineCount = mustTessellate(sphere, fine).indices.size();
    const std::size_t coarseCount = mustTessellate(sphere, coarse).indices.size();
    EXPECT_LT(coarseCount, fineCount);
    EXPECT_EQ(coarseCount, mustTessellate(Shape(BRepPrimAPI_MakeSphere(r).Shape()), coarse).indices.size());
}

TEST(KernelTessellation, RelativeDeflectionScalesWithSize) {
    TessellationParams relative;
    relative.relative = true;
    relative.linearDeflection = 0.01;
    const std::size_t small = mustTessellate(Shape(BRepPrimAPI_MakeSphere(1.0).Shape()), relative).indices.size();
    const std::size_t large = mustTessellate(Shape(BRepPrimAPI_MakeSphere(100.0).Shape()), relative).indices.size();
    EXPECT_NEAR(static_cast<double>(large), static_cast<double>(small), 0.1 * small);

    TessellationParams absolute;
    absolute.linearDeflection = 0.01;
    absolute.angularDeflection = 1.0;
    const std::size_t smallAbs = mustTessellate(Shape(BRepPrimAPI_MakeSphere(1.0).Shape()), absolute).indices.size();
    const std::size_t largeAbs = mustTessellate(Shape(BRepPrimAPI_MakeSphere(100.0).Shape()), absolute).indices.size();
    EXPECT_GT(largeAbs, 10 * smallAbs);
}

TEST(KernelTessellation, LocationIsAppliedToVerticesNormalsAndEdges) {
    gp_Trsf rotation;
    rotation.SetRotation(gp_Ax1(gp_Pnt(0, 0, 0), gp_Dir(1, 1, 0)), kPi / 6.0);
    gp_Trsf translation;
    translation.SetTranslation(gp_Vec(100.0, -50.0, 25.0));
    const gp_Trsf trsf = translation * rotation;
    const TopoDS_Shape base = BRepPrimAPI_MakeBox(10.0, 20.0, 30.0).Shape();
    const Shape moved(base.Moved(TopLoc_Location(trsf)));

    const Mesh mesh = mustTessellate(moved);
    expectWellFormed(mesh, 6);
    EXPECT_NEAR(meshSignedVolume(mesh), 6000.0, 0.05);

    const gp_Trsf inverse = trsf.Inverted();
    const std::size_t vertexCount = mesh.positions.size() / 3;
    for (std::uint32_t v = 0; v < vertexCount; ++v) {
        // Back in the box's own frame every vertex is a corner or lies on the boundary.
        gp_XYZ local = vertex(mesh, v);
        inverse.Transforms(local);
        const double extent[3] = {10.0, 20.0, 30.0};
        int onBoundary = 0;
        for (int k = 0; k < 3; ++k) {
            const double x = local.Coord(k + 1);
            EXPECT_GT(x, -1e-4);
            EXPECT_LT(x, extent[k] + 1e-4);
            if (std::abs(x) < 1e-4 || std::abs(x - extent[k]) < 1e-4) ++onBoundary;
        }
        EXPECT_GE(onBoundary, 1);
        // Normals rotate with the shape: back-rotated they are axis-aligned and outward.
        gp_XYZ n = normal(mesh, v);
        n.Multiply(rotation.Inverted().VectorialPart());
        EXPECT_GT(n.Dot(local - gp_XYZ(5.0, 10.0, 15.0)), 0.0);
        EXPECT_NEAR(std::max({std::abs(n.X()), std::abs(n.Y()), std::abs(n.Z())}), 1.0, 1e-5);
    }

    // Edge polylines end at the transformed box corners.
    ASSERT_EQ(mesh.edgePolylines.size(), 12u);
    EXPECT_TRUE(mesh.missingEdges.empty());
    for (const EdgePolyline& line : mesh.edgePolylines) {
        ASSERT_GE(line.points.size(), 6u);
        const gp_XYZ first(line.points[0], line.points[1], line.points[2]);
        const std::size_t last = line.points.size() - 3;
        const gp_XYZ end(line.points[last], line.points[last + 1], line.points[last + 2]);
        for (const gp_XYZ& p : {first, end}) {
            gp_XYZ local = p;
            inverse.Transforms(local);
            for (int k = 1; k <= 3; ++k) {
                const double x = local.Coord(k);
                const double extent = k == 1 ? 10.0 : (k == 2 ? 20.0 : 30.0);
                EXPECT_TRUE(std::abs(x) < 1e-4 || std::abs(x - extent) < 1e-4) << x;
            }
        }
    }
}

TEST(KernelTessellation, TwoInstancesOfOneSolidAreBothPlaced) {
    gp_Trsf shift;
    shift.SetTranslation(gp_Vec(0.0, 0.0, 100.0));
    const TopoDS_Shape box = BRepPrimAPI_MakeBox(2.0, 3.0, 4.0).Shape();
    BRep_Builder builder;
    TopoDS_Compound compound;
    builder.MakeCompound(compound);
    builder.Add(compound, box);
    builder.Add(compound, box.Moved(TopLoc_Location(shift)));
    const Shape shape(compound);

    const Mesh mesh = mustTessellate(shape);
    expectWellFormed(mesh, 12);
    EXPECT_EQ(mesh.faceSpans.size(), 12u);
    EXPECT_NEAR(meshSignedVolume(mesh), 48.0, 1e-3);
    float maxZ = -1e30f;
    for (std::size_t i = 2; i < mesh.positions.size(); i += 3) maxZ = std::max(maxZ, mesh.positions[i]);
    EXPECT_NEAR(maxZ, 104.0f, 1e-4);
}

TEST(KernelTessellation, EdgePolylinesFollowTheCurves) {
    const double r = 5.0, h = 20.0;
    const Shape cylinder(BRepPrimAPI_MakeCylinder(r, h).Shape());
    const auto index = TopologyIndex::build(cylinder);
    const Mesh mesh = mustTessellate(cylinder);
    EXPECT_EQ(mesh.edgePolylines.size(), index.value().edges().size());
    std::set<std::uint32_t> seen;
    for (const EdgePolyline& line : mesh.edgePolylines) {
        EXPECT_TRUE(seen.insert(line.edgeIndex).second);
        const CurveType type = index.value().edges()[line.edgeIndex].curve;
        for (std::size_t i = 0; i < line.points.size(); i += 3) {
            // Rim circles and the seam line all lie on the cylinder.
            EXPECT_NEAR(std::hypot(line.points[i], line.points[i + 1]), r, 1e-4);
            EXPECT_GT(line.points[i + 2], -1e-4);
            EXPECT_LT(line.points[i + 2], h + 1e-4);
        }
        if (type == CurveType::Circle) EXPECT_GT(line.points.size() / 3, 8u);
    }

    // Degenerated pole edges of a sphere have no polyline and are not reported missing.
    const Shape sphere(BRepPrimAPI_MakeSphere(3.0).Shape());
    const Mesh sphereMesh = mustTessellate(sphere);
    EXPECT_EQ(sphereMesh.edgePolylines.size(), 1u);
    EXPECT_TRUE(sphereMesh.missingEdges.empty());
}

TEST(KernelTessellation, FreeEdgeGetsItsOwnPolygon) {
    BRep_Builder builder;
    TopoDS_Compound compound;
    builder.MakeCompound(compound);
    builder.Add(compound, BRepPrimAPI_MakeBox(1.0, 1.0, 1.0).Shape());
    const TopoDS_Edge free = BRepBuilderAPI_MakeEdge(gp_Pnt(10, 0, 0), gp_Pnt(10, 0, 5)).Edge();
    builder.Add(compound, free);
    const Shape shape(compound);
    const auto index = TopologyIndex::build(shape);
    const std::uint32_t freeIndex = index.value().edgeIndex(free).value();
    const Mesh mesh = mustTessellate(shape);
    EXPECT_TRUE(mesh.missingEdges.empty());
    const auto it = std::find_if(mesh.edgePolylines.begin(), mesh.edgePolylines.end(),
                                 [&](const EdgePolyline& l) { return l.edgeIndex == freeIndex; });
    ASSERT_NE(it, mesh.edgePolylines.end());
    ASSERT_GE(it->points.size(), 6u);
    EXPECT_NEAR(it->points[0], 10.0f, 1e-5);
    EXPECT_NEAR(it->points[it->points.size() - 1], 5.0f, 1e-5);
}

TEST(KernelTessellation, FaceWithoutTriangulationIsReported) {
    // An unbounded plane cannot be meshed.
    BRep_Builder builder;
    TopoDS_Compound compound;
    builder.MakeCompound(compound);
    builder.Add(compound, BRepPrimAPI_MakeBox(1.0, 1.0, 1.0).Shape());
    const TopoDS_Face infinite = BRepBuilderAPI_MakeFace(gp_Pln(gp_Pnt(0, 0, 10), gp_Dir(0, 0, 1))).Face();
    builder.Add(compound, infinite);
    const Shape shape(compound);
    const std::uint32_t planeIndex = TopologyIndex::build(shape).value().faceIndex(infinite).value();

    auto strict = tessellate(shape);
    ASSERT_FALSE(strict.ok());
    EXPECT_EQ(strict.error().code, ErrorCode::OperationFailed);
    EXPECT_NE(strict.error().message.find("1 of 7 faces"), std::string::npos) << strict.error().message;

    TessellationParams lenient;
    lenient.requireAllFaces = false;
    const Mesh mesh = mustTessellate(shape, lenient);
    EXPECT_EQ(mesh.missingFaces, std::vector<std::uint32_t>{planeIndex});
    EXPECT_EQ(mesh.faceSpans.size(), 6u);
    expectWellFormed(mesh, 7);
}

TEST(KernelTessellation, TriangulationOnlyFaceUsesTriangleNormals) {
    // A face carrying only a triangulation (as from a mesh import) keeps it and gets
    // normals from its triangles.
    occ::handle<Poly_Triangulation> tri = new Poly_Triangulation(3, 1, false);
    tri->SetNode(1, gp_Pnt(0, 0, 0));
    tri->SetNode(2, gp_Pnt(0, 4, 0));
    tri->SetNode(3, gp_Pnt(0, 0, 3));
    tri->SetTriangle(1, Poly_Triangle(1, 2, 3));
    BRep_Builder builder;
    TopoDS_Face face;
    builder.MakeFace(face, tri);
    const Mesh mesh = mustTessellate(Shape(face));
    expectWellFormed(mesh, 1);
    ASSERT_EQ(mesh.positions.size(), 9u);
    for (std::uint32_t v = 0; v < 3; ++v) {
        EXPECT_NEAR(normal(mesh, v).X(), 1.0, 1e-6);  // (0,4,0) x (0,0,3) = +X
    }
    EXPECT_NEAR(meshArea(mesh), 6.0, 1e-6);
}

TEST(KernelTessellation, InvalidArgumentsAreRejected) {
    EXPECT_EQ(tessellate(Shape()).error().code, ErrorCode::InvalidInput);
    const Shape box(BRepPrimAPI_MakeBox(1.0, 1.0, 1.0).Shape());
    TessellationParams params;
    params.linearDeflection = 0.0;
    EXPECT_EQ(tessellate(box, params).error().code, ErrorCode::InvalidInput);
    params.linearDeflection = std::nan("");
    EXPECT_EQ(tessellate(box, params).error().code, ErrorCode::InvalidInput);
    params.linearDeflection = 0.1;
    params.angularDeflection = -1.0;
    EXPECT_EQ(tessellate(box, params).error().code, ErrorCode::InvalidInput);
    TopoDS_Compound empty;
    BRep_Builder().MakeCompound(empty);
    EXPECT_EQ(tessellate(Shape(empty)).error().code, ErrorCode::InvalidInput);
}
