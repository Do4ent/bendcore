// Reference meshes (docs/kernel/ANALYSIS.md, section 7): STL files of a cube and of a cylinder
// (an N-gon prism) are written byte by byte here, independently of the kernel writers, read
// back and compared with the analytic triangle count, bounding box, area and divergence-theorem
// volume; the section of the cube is a square with the exact side; broken files fail with a
// clear error; the placement, picking and nearest-point functions are checked on known points.

#include <bendcore/kernel/reference_mesh.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

#include <BRepPrimAPI_MakeBox.hxx>

using namespace bendcore::kernel;

namespace {

constexpr double kPi = 3.14159265358979323846;
using V = std::array<double, 3>;
using Tri = std::array<V, 3>;

std::vector<Tri> cubeTriangles(double a) {
    // Corners of [0, a]^3, faces counter-clockwise from outside, two triangles each.
    auto P = [a](int x, int y, int z) { return V{x * a, y * a, z * a}; };
    const int quads[6][4][3] = {
        {{0, 0, 0}, {0, 1, 0}, {1, 1, 0}, {1, 0, 0}},  // z = 0, normal -z
        {{0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}},  // z = a
        {{0, 0, 0}, {1, 0, 0}, {1, 0, 1}, {0, 0, 1}},  // y = 0
        {{0, 1, 0}, {0, 1, 1}, {1, 1, 1}, {1, 1, 0}},  // y = a
        {{0, 0, 0}, {0, 0, 1}, {0, 1, 1}, {0, 1, 0}},  // x = 0
        {{1, 0, 0}, {1, 1, 0}, {1, 1, 1}, {1, 0, 1}},  // x = a
    };
    std::vector<Tri> out;
    for (const auto& q : quads) {
        const V c0 = P(q[0][0], q[0][1], q[0][2]), c1 = P(q[1][0], q[1][1], q[1][2]);
        const V c2 = P(q[2][0], q[2][1], q[2][2]), c3 = P(q[3][0], q[3][1], q[3][2]);
        out.push_back({c0, c1, c2});
        out.push_back({c0, c2, c3});
    }
    return out;
}

// Prism over a regular n-gon inscribed in the circle of radius r, z in [0, h]: 4n triangles.
std::vector<Tri> prismTriangles(double r, double h, int n) {
    std::vector<Tri> out;
    auto rim = [&](int i, double z) {
        const double t = 2.0 * kPi * (i % n) / n;
        return V{r * std::cos(t), r * std::sin(t), z};
    };
    const V b{0, 0, 0}, top{0, 0, h};
    for (int i = 0; i < n; ++i) {
        out.push_back({b, rim(i + 1, 0), rim(i, 0)});      // bottom, normal -z
        out.push_back({top, rim(i, h), rim(i + 1, h)});    // top, normal +z
        out.push_back({rim(i, 0), rim(i + 1, 0), rim(i + 1, h)});
        out.push_back({rim(i, 0), rim(i + 1, h), rim(i, h)});
    }
    return out;
}

std::string binaryStl(const std::vector<Tri>& tris) {
    std::string out(80, ' ');
    out.replace(0, 9, "test mesh");
    const std::uint32_t n = static_cast<std::uint32_t>(tris.size());
    out.append(reinterpret_cast<const char*>(&n), 4);
    for (const Tri& t : tris) {
        const float zero[3] = {0, 0, 0};
        out.append(reinterpret_cast<const char*>(zero), 12);
        for (const V& v : t)
            for (double c : v) {
                const float f = static_cast<float>(c);
                out.append(reinterpret_cast<const char*>(&f), 4);
            }
        out.append(2, '\0');
    }
    return out;
}

std::string asciiStl(const std::vector<Tri>& tris) {
    std::ostringstream s;
    s.precision(9);
    s << "solid test\n";
    for (const Tri& t : tris) {
        s << "  facet normal 0 0 0\n    outer loop\n";
        for (const V& v : t) s << "      vertex " << v[0] << ' ' << v[1] << ' ' << v[2] << '\n';
        s << "    endloop\n  endfacet\n";
    }
    s << "endsolid test\n";
    return s.str();
}

ReferenceMeshData readOk(const std::string& data, MeshFileFormat f = MeshFileFormat::Stl) {
    auto r = readReferenceMesh(data, f);
    EXPECT_TRUE(r.ok()) << (r.ok() ? "" : r.error().message);
    return r.ok() ? r.value() : ReferenceMeshData{};
}

}  // namespace

TEST(ReferenceMesh, CubeBinaryAndAscii) {
    for (const bool binary : {true, false}) {
        const auto tris = cubeTriangles(10.0);
        const ReferenceMeshData m = readOk(binary ? binaryStl(tris) : asciiStl(tris));
        EXPECT_EQ(m.report.encoding, binary ? "binary" : "ascii");
        EXPECT_EQ(m.report.facetsRead, 12u);
        EXPECT_EQ(m.report.degenerateDropped, 0u);
        const MeshProperties p = meshProperties(m.mesh);
        EXPECT_EQ(p.triangles, 12u);
        EXPECT_EQ(p.vertices, 8u);  // welded
        EXPECT_TRUE(p.closed);
        EXPECT_EQ(p.openEdges, 0u);
        for (int k = 0; k < 3; ++k) {
            EXPECT_DOUBLE_EQ(p.min[k], 0.0);
            EXPECT_DOUBLE_EQ(p.max[k], 10.0);
        }
        EXPECT_NEAR(p.area, 600.0, 1e-9);
        EXPECT_NEAR(p.volume, 1000.0, 1e-9);
    }
}

TEST(ReferenceMesh, CylinderPrismAgainstAnalytic) {
    const double r = 5.0, h = 20.0;
    const int n = 256;
    for (const bool binary : {true, false}) {
        const auto tris = prismTriangles(r, h, n);
        const ReferenceMeshData m = readOk(binary ? binaryStl(tris) : asciiStl(tris));
        const MeshProperties p = meshProperties(m.mesh);
        EXPECT_EQ(p.triangles, static_cast<std::size_t>(4 * n));
        EXPECT_EQ(p.vertices, static_cast<std::size_t>(2 * n + 2));
        EXPECT_TRUE(p.closed);
        // Exact polygon prism (float coordinates: ~1e-6 relative).
        const double polyArea = 0.5 * n * r * r * std::sin(2.0 * kPi / n);
        const double polyPerimeter = 2.0 * n * r * std::sin(kPi / n);
        EXPECT_NEAR(p.volume, polyArea * h, 1e-5 * polyArea * h);
        EXPECT_NEAR(p.area, 2.0 * polyArea + polyPerimeter * h, 1e-5 * p.area);
        // Against the cylinder: the inscribed polygon loses at most the segment area
        // r^2 (theta - sin theta) / 2 per side.
        const double theta = 2.0 * kPi / n;
        const double lostArea = n * 0.5 * r * r * (theta - std::sin(theta));
        EXPECT_LT(kPi * r * r * h - p.volume, lostArea * h * 1.01);
        EXPECT_GT(kPi * r * r * h - p.volume, 0.0);
        EXPECT_NEAR(p.volume, kPi * r * r * h, 1e-3 * kPi * r * r * h);
        EXPECT_NEAR(p.max[2] - p.min[2], h, 1e-6);
        EXPECT_NEAR(p.max[0], r, 1e-6);
        EXPECT_NEAR(p.min[0], -r, 1e-4);
    }
}

TEST(ReferenceMesh, BinaryStlWithSolidHeaderIsBinary) {
    auto data = binaryStl(cubeTriangles(2.0));
    data.replace(0, 9, "solid cad");  // some writers start binary headers with "solid"
    const ReferenceMeshData m = readOk(data);
    EXPECT_EQ(m.report.encoding, "binary");
    EXPECT_EQ(m.mesh.triangles.size(), 12u);
}

TEST(ReferenceMesh, CubeSectionIsExactSquare) {
    const ReferenceMeshData m = readOk(binaryStl(cubeTriangles(10.0)));
    auto s = sectionMesh(m.mesh, SectionPlane{{0, 0, 3.7}, {0, 0, 1}});
    ASSERT_TRUE(s.ok()) << s.error().message;
    ASSERT_EQ(s.value().contours.size(), 1u);
    const MeshSectionContour& c = s.value().contours[0];
    EXPECT_TRUE(c.closed);
    ASSERT_EQ(c.points.size(), 4u);  // collinear points of the triangle diagonals dropped
    for (std::size_t i = 0; i < 4; ++i) {
        const V& a = c.points[i];
        const V& b = c.points[(i + 1) % 4];
        EXPECT_NEAR(std::hypot(b[0] - a[0], b[1] - a[1], b[2] - a[2]), 10.0, 1e-12);
        EXPECT_NEAR(a[2], 3.7, 1e-12);
    }
    EXPECT_NEAR(s.value().length, 40.0, 1e-12);
    EXPECT_NEAR(s.value().area, 100.0, 1e-9);  // counter-clockwise seen from +z

    // Inclined plane through the cube centre at 45 degrees about X: rectangle 10 x 10*sqrt(2).
    auto inclined = sectionMesh(m.mesh, SectionPlane{{5, 5, 5}, {0, -1, 1}});
    ASSERT_TRUE(inclined.ok());
    ASSERT_EQ(inclined.value().contours.size(), 1u);
    EXPECT_NEAR(inclined.value().area, 100.0 * std::sqrt(2.0), 1e-9);
    EXPECT_NEAR(inclined.value().length, 20.0 + 20.0 * std::sqrt(2.0), 1e-9);

    // Plane through vertices (on a face diagonal plane x = y) and plane missing the mesh.
    auto diag = sectionMesh(m.mesh, SectionPlane{{0, 0, 0}, {1, -1, 0}});
    ASSERT_TRUE(diag.ok());
    EXPECT_NEAR(std::abs(diag.value().area), 100.0 * std::sqrt(2.0), 1e-9);
    auto miss = sectionMesh(m.mesh, SectionPlane{{0, 0, 11}, {0, 0, 1}});
    ASSERT_TRUE(miss.ok());
    EXPECT_TRUE(miss.value().contours.empty());
    EXPECT_EQ(miss.value().area, 0.0);
    EXPECT_FALSE(sectionMesh(m.mesh, SectionPlane{{0, 0, 0}, {0, 0, 0}}).ok());
}

TEST(ReferenceMesh, CylinderSectionAreaIsPolygonArea) {
    const double r = 5.0, h = 20.0;
    const int n = 64;
    const ReferenceMeshData m = readOk(binaryStl(prismTriangles(r, h, n)));
    auto s = sectionMesh(m.mesh, SectionPlane{{0, 0, 7}, {0, 0, 1}});
    ASSERT_TRUE(s.ok());
    ASSERT_EQ(s.value().contours.size(), 1u);
    EXPECT_EQ(s.value().contours[0].points.size(), static_cast<std::size_t>(n));
    EXPECT_NEAR(s.value().area, 0.5 * n * r * r * std::sin(2.0 * kPi / n), 1e-4);
}

TEST(ReferenceMesh, ThreeMfFromExport) {
    const Shape box(BRepPrimAPI_MakeBox(10.0, 20.0, 30.0).Shape());
    auto pkg = export3mf({ThreeMfObject{"box", box, std::nullopt}});
    ASSERT_TRUE(pkg.ok()) << pkg.error().message;
    auto r = readReferenceMesh(pkg.value(), MeshFileFormat::ThreeMf);
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_EQ(r.value().report.encoding, "3mf");
    const MeshProperties p = meshProperties(r.value().mesh);
    EXPECT_TRUE(p.closed);
    EXPECT_NEAR(p.volume, 6000.0, 1e-6);
    EXPECT_NEAR(p.area, 2.0 * (200 + 300 + 600), 1e-6);
    EXPECT_NEAR(p.max[2], 30.0, 1e-9);
}

TEST(ReferenceMesh, ThreeMfUnitsAndTransform) {
    const std::string model =
        "<?xml version=\"1.0\"?>\n<model unit=\"inch\" xmlns=\"http://schemas.microsoft.com/3dmanufacturing/core/2015/02\">"
        "<resources><object id=\"7\" type=\"model\"><mesh><vertices>"
        "<vertex x=\"0\" y=\"0\" z=\"0\"/><vertex x=\"1\" y=\"0\" z=\"0\"/><vertex x=\"0\" y=\"1\" z=\"0\"/>"
        "</vertices><triangles><triangle v1=\"0\" v2=\"1\" v3=\"2\"/></triangles></mesh></object></resources>"
        "<build><item objectid=\"7\" transform=\"1 0 0 0 1 0 0 0 1 2 0 0\"/></build></model>";
    auto pkg = writeZip({{"3D/3dmodel.model", model}}, ZipMethod::Stored);
    ASSERT_TRUE(pkg.ok());
    auto r = readReferenceMesh(pkg.value(), MeshFileFormat::ThreeMf);
    ASSERT_TRUE(r.ok()) << r.error().message;
    const MeshProperties p = meshProperties(r.value().mesh);
    EXPECT_NEAR(p.min[0], 2.0 * 25.4, 1e-9);
    EXPECT_NEAR(p.max[0], 3.0 * 25.4, 1e-9);
    EXPECT_NEAR(p.area, 0.5 * 25.4 * 25.4, 1e-9);
    EXPECT_FALSE(p.closed);

    auto noItem = writeZip({{"3D/3dmodel.model", "<model><resources/><build/></model>"}}, ZipMethod::Stored);
    auto bad = readReferenceMesh(noItem.value(), MeshFileFormat::ThreeMf);
    ASSERT_FALSE(bad.ok());
    EXPECT_EQ(bad.error().code, ErrorCode::ParseError);
    auto noModel = writeZip({{"readme.txt", "x"}}, ZipMethod::Stored);
    EXPECT_EQ(readReferenceMesh(noModel.value(), MeshFileFormat::ThreeMf).error().code, ErrorCode::ParseError);
    auto comp = writeZip({{"3D/3dmodel.model", "<model><resources><object id=\"1\"><components><component objectid=\"2\"/></components></object></resources></model>"}},
                         ZipMethod::Stored);
    EXPECT_EQ(readReferenceMesh(comp.value(), MeshFileFormat::ThreeMf).error().code, ErrorCode::Unsupported);
    EXPECT_EQ(readReferenceMesh("not a zip archive at all", MeshFileFormat::ThreeMf).error().code, ErrorCode::ParseError);
}

TEST(ReferenceMesh, BrokenFilesFailClearly) {
    auto code = [](const std::string& d, const MeshReadLimits& l = {}) {
        auto r = readReferenceMesh(d, MeshFileFormat::Stl, l);
        EXPECT_FALSE(r.ok());
        return r.ok() ? std::make_pair(ErrorCode::Fault, std::string()) : std::make_pair(r.error().code, r.error().message);
    };
    EXPECT_EQ(code("").first, ErrorCode::InvalidInput);

    // Truncated binary: the header count does not match the size.
    std::string bin = binaryStl(cubeTriangles(1.0));
    auto truncated = code(bin.substr(0, bin.size() - 7));
    EXPECT_EQ(truncated.first, ErrorCode::ParseError);
    EXPECT_NE(truncated.second.find("declares 12 triangles"), std::string::npos) << truncated.second;

    EXPECT_EQ(code("garbage").first, ErrorCode::ParseError);
    auto ascii = asciiStl(cubeTriangles(1.0));
    auto badAscii = ascii;
    badAscii.replace(badAscii.find("vertex"), 6, "vortex");
    auto a = code(badAscii);
    EXPECT_EQ(a.first, ErrorCode::ParseError);
    EXPECT_NE(a.second.find("line 4"), std::string::npos) << a.second;
    EXPECT_EQ(code(ascii.substr(0, ascii.size() / 2)).first, ErrorCode::ParseError);  // no endsolid

    // Over the limits.
    MeshReadLimits small;
    small.maxBytes = 100;
    auto big = code(bin, small);
    EXPECT_EQ(big.first, ErrorCode::InvalidInput);
    EXPECT_NE(big.second.find("MB, the limit is"), std::string::npos) << big.second;
    MeshReadLimits few;
    few.maxTriangles = 5;
    auto many = code(bin, few);
    EXPECT_EQ(many.first, ErrorCode::InvalidInput);
    EXPECT_EQ(many.second, "the mesh has 12 triangles, the limit is 5");
    EXPECT_EQ(code(ascii, few).second, "the mesh has 12 triangles, the limit is 5");

    // Non-finite coordinate.
    std::string nan = bin;
    const float qnan = std::nanf("");
    std::memcpy(&nan[84 + 12], &qnan, 4);
    EXPECT_EQ(code(nan).first, ErrorCode::ParseError);

    // Only zero-area triangles; zero-area triangles among good ones are dropped.
    std::vector<Tri> flat{{V{0, 0, 0}, V{1, 1, 1}, V{2, 2, 2}}, {V{0, 0, 0}, V{0, 0, 0}, V{1, 0, 0}}};
    EXPECT_EQ(code(binaryStl(flat)).second, "the mesh has no triangle of non-zero area");
    auto withFlat = cubeTriangles(1.0);
    withFlat.push_back(flat[0]);
    withFlat.push_back(flat[1]);
    const ReferenceMeshData m = readOk(binaryStl(withFlat));
    EXPECT_EQ(m.report.facetsRead, 14u);
    EXPECT_EQ(m.report.degenerateDropped, 2u);
    EXPECT_TRUE(meshProperties(m.mesh).closed);
    // Header of zero triangles.
    EXPECT_EQ(code(binaryStl({})).second, "the mesh has no triangle of non-zero area");
}

TEST(ReferenceMesh, PlacementPickingAndNearestPoint) {
    const ReferenceMeshData m = readOk(binaryStl(cubeTriangles(1.0)));
    MeshPlacement pl;
    pl.unitScale = *meshUnitScale("in");
    pl.rotationDegrees = {0, 0, 90};
    pl.translation = {100, 0, 0};
    auto placed = placeMesh(m.mesh, pl);
    ASSERT_TRUE(placed.ok());
    const MeshProperties p = meshProperties(placed.value());
    EXPECT_NEAR(p.min[0], 100.0 - 25.4, 1e-9);  // x' = -y after the rotation
    EXPECT_NEAR(p.max[0], 100.0, 1e-9);
    EXPECT_NEAR(p.max[1], 25.4, 1e-9);
    EXPECT_NEAR(p.volume, std::pow(25.4, 3), 1e-6);  // orientation kept
    EXPECT_TRUE(p.closed);
    pl.unitScale = 0.0;
    EXPECT_EQ(placeMesh(m.mesh, pl).error().code, ErrorCode::InvalidInput);
    EXPECT_FALSE(meshUnitScale("yard").has_value());

    // Ray along -z into the top face of the unit cube at (0.25, 0.5).
    auto hit = raycastMesh(m.mesh, {0.25, 0.5, 5.0}, {0, 0, -1});
    ASSERT_TRUE(hit.has_value());
    EXPECT_NEAR(hit->point[2], 1.0, 1e-12);
    EXPECT_FALSE(raycastMesh(m.mesh, {5, 5, 5}, {0, 0, -1}).has_value());

    // Point on the hit triangle, snapped to the corner (0, 0, 1) within 0.3.
    auto onTri = pointOnTriangle(m.mesh, hit->triangle, {0.1, 0.1, 1.2}, 0.0);
    ASSERT_TRUE(onTri.ok());
    EXPECT_FALSE(onTri.value().vertex.has_value());
    EXPECT_NEAR(onTri.value().point[2], 1.0, 1e-12);
    auto snapped = pointOnTriangle(m.mesh, hit->triangle, {0.1, 0.1, 1.0}, 0.3);
    ASSERT_TRUE(snapped.ok());
    ASSERT_TRUE(snapped.value().vertex.has_value());
    EXPECT_EQ(snapped.value().point, (V{0, 0, 1}));
    EXPECT_EQ(pointOnTriangle(m.mesh, 99, {0, 0, 0}).error().code, ErrorCode::InvalidInput);

    // Distance between opposite corners picked on the mesh: sqrt(3).
    auto far = nearestPointOnMesh(m.mesh, {2, 2, 2});
    auto near = nearestPointOnMesh(m.mesh, {-1, -1, -1});
    ASSERT_TRUE(far.ok() && near.ok());
    const V d{far.value().point[0] - near.value().point[0], far.value().point[1] - near.value().point[1],
              far.value().point[2] - near.value().point[2]};
    EXPECT_NEAR(std::hypot(d[0], d[1], d[2]), std::sqrt(3.0), 1e-12);
    auto inside = nearestPointOnMesh(m.mesh, {0.5, 0.5, 0.9});
    EXPECT_NEAR(inside.value().point[2], 1.0, 1e-12);
}

TEST(ReferenceMesh, FormatNames) {
    EXPECT_EQ(meshFileFormatOfName("Part.STL"), MeshFileFormat::Stl);
    EXPECT_EQ(meshFileFormatOfName("a.b.3mf"), MeshFileFormat::ThreeMf);
    EXPECT_FALSE(meshFileFormatOfName("model.obj").has_value());
    EXPECT_FALSE(meshFileFormatOfName("noext").has_value());
    EXPECT_STREQ(toString(MeshFileFormat::ThreeMf), "3mf");
}
