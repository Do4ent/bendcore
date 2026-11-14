// STL and 3MF export (docs/kernel/ANALYSIS.md): STL is read back by the own parser and by
// OCCT (RWStl, StlAPI_Reader) and its volume compared with the exact solid volume within the
// chordal deflection bound; 3MF is unpacked (ZIP, CRC-32), its XML parsed by expat and the
// meshes checked for closure (every edge shared by exactly two triangles).

#include <bendcore/kernel/mesh_export.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>

#include <BRepAlgoAPI_Cut.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <Poly_Triangulation.hxx>
#include <RWStl.hxx>
#include <StlAPI_Reader.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS_Shape.hxx>

#include <expat.h>

using namespace bendcore::kernel;

namespace {

constexpr double kPi = 3.14159265358979323846;

Shape cylinder(double r, double h) { return Shape(BRepPrimAPI_MakeCylinder(r, h).Shape()); }
Shape sphere(double r) { return Shape(BRepPrimAPI_MakeSphere(r).Shape()); }

std::filesystem::path writeTemp(const std::string& name, const std::string& data) {
    const auto path = std::filesystem::temp_directory_path() / name;
    std::ofstream out(path, std::ios::binary);
    out.write(data.data(), static_cast<std::streamsize>(data.size()));
    return path;
}

double triangulationVolume(const occ::handle<Poly_Triangulation>& tri) {
    double v = 0.0;
    for (int i = 1; i <= tri->NbTriangles(); ++i) {
        int a, b, c;
        tri->Triangle(i).Get(a, b, c);
        const gp_XYZ pa = tri->Node(a).XYZ(), pb = tri->Node(b).XYZ(), pc = tri->Node(c).XYZ();
        v += pa.Dot(pb.Crossed(pc));
    }
    return v / 6.0;
}

std::size_t faceCount(const TopoDS_Shape& s) {
    std::size_t n = 0;
    for (TopExp_Explorer it(s, TopAbs_FACE); it.More(); it.Next()) ++n;
    return n;
}

// Inscribed tessellation: the mesh lies inside the convex body and no point of the surface
// is farther than the linear deflection from it, so 0 <= V - Vmesh <= area * deflection.
void expectWithinDeflection(double meshV, double exactV, double area, double deflection) {
    EXPECT_LE(meshV, exactV * (1 + 1e-9));
    EXPECT_GE(meshV, exactV - area * deflection);
}

}  // namespace

TEST(MeshExport, BoxBinaryStlRoundTrip) {
    const Shape box(BRepPrimAPI_MakeBox(10, 20, 30).Shape());
    auto stl = exportStlBinary(box);
    ASSERT_TRUE(stl.ok()) << stl.error().message;
    EXPECT_EQ(stl.value().size(), 84u + 50u * 12u);
    EXPECT_NE(stl.value().rfind("solid", 0), 0u) << "binary header must not start with 'solid'";

    auto facets = readStl(stl.value());
    ASSERT_TRUE(facets.ok()) << facets.error().message;
    ASSERT_EQ(facets.value().size(), 12u);
    EXPECT_NEAR(meshVolume(facets.value()), 6000.0, 1e-3);
    // Stored normals are unit and point outwards: n . (centroid - box centre) > 0.
    for (const StlFacet& f : facets.value()) {
        const double len = std::hypot(f.normal[0], f.normal[1], f.normal[2]);
        EXPECT_NEAR(len, 1.0, 1e-6);
        double c[3];
        for (int k = 0; k < 3; ++k) c[k] = (f.vertices[0][k] + f.vertices[1][k] + f.vertices[2][k]) / 3.0;
        EXPECT_GT(f.normal[0] * (c[0] - 5) + f.normal[1] * (c[1] - 10) + f.normal[2] * (c[2] - 15), 0.0);
    }

    // OCCT readers.
    const auto path = writeTemp("bendcore_box_binary.stl", stl.value());
    occ::handle<Poly_Triangulation> tri = RWStl::ReadFile(path.string().c_str());
    ASSERT_FALSE(tri.IsNull());
    EXPECT_EQ(tri->NbTriangles(), 12);
    EXPECT_EQ(tri->NbNodes(), 8) << "RWStl merges the welded corners";
    EXPECT_NEAR(triangulationVolume(tri), 6000.0, 1e-3);
    TopoDS_Shape read;
    StlAPI_Reader reader;
    ASSERT_TRUE(reader.Read(read, path.string().c_str()));
    EXPECT_EQ(faceCount(read), 12u);
    std::filesystem::remove(path);
}

TEST(MeshExport, CylinderStlVolumeWithinDeflection) {
    const double r = 15, h = 40;
    MeshExportParams params;
    params.linearDeflection = 0.02;
    params.angularDeflection = 0.2;
    auto mesh = meshForExport(cylinder(r, h), params);
    ASSERT_TRUE(mesh.ok()) << mesh.error().message;
    EXPECT_TRUE(checkClosure(mesh.value()).closed());
    const double exactV = kPi * r * r * h;
    const double area = 2 * kPi * r * h + 2 * kPi * r * r;
    expectWithinDeflection(meshVolume(mesh.value()), exactV, area, params.linearDeflection);

    auto stl = writeStlBinary(mesh.value());
    ASSERT_TRUE(stl.ok());
    auto facets = readStl(stl.value());
    ASSERT_TRUE(facets.ok());
    EXPECT_EQ(facets.value().size(), mesh.value().triangles.size());
    // float storage costs ~1e-7 relative.
    EXPECT_NEAR(meshVolume(facets.value()), meshVolume(mesh.value()), exactV * 1e-5);
    const auto path = writeTemp("bendcore_cylinder.stl", stl.value());
    occ::handle<Poly_Triangulation> tri = RWStl::ReadFile(path.string().c_str());
    ASSERT_FALSE(tri.IsNull());
    expectWithinDeflection(triangulationVolume(tri), exactV, area, params.linearDeflection + 1e-4);
    std::filesystem::remove(path);
}

TEST(MeshExport, SphereAsciiStlRoundTrip) {
    const double r = 20;
    MeshExportParams params;
    params.linearDeflection = 0.05;
    auto stl = exportStlAscii(sphere(r), params, "ball");
    ASSERT_TRUE(stl.ok()) << stl.error().message;
    EXPECT_EQ(stl.value().rfind("solid ball\n", 0), 0u);
    auto facets = readStl(stl.value());
    ASSERT_TRUE(facets.ok()) << facets.error().message;
    const double exactV = 4.0 / 3.0 * kPi * r * r * r;
    expectWithinDeflection(meshVolume(facets.value()), exactV, 4 * kPi * r * r, params.linearDeflection + 1e-4);

    const auto path = writeTemp("bendcore_sphere_ascii.stl", stl.value());
    occ::handle<Poly_Triangulation> tri = RWStl::ReadAscii(OSD_Path(path.string().c_str()));
    ASSERT_FALSE(tri.IsNull());
    EXPECT_EQ(static_cast<std::size_t>(tri->NbTriangles()), facets.value().size());
    EXPECT_NEAR(triangulationVolume(tri), meshVolume(facets.value()), exactV * 1e-6);
    std::filesystem::remove(path);
}

TEST(MeshExport, ClosedMeshesForSolidsWithSeamsPolesAndHoles) {
    BRepAlgoAPI_Cut tube(BRepPrimAPI_MakeCylinder(20, 30).Shape(), BRepPrimAPI_MakeCylinder(12, 30).Shape());
    ASSERT_TRUE(tube.IsDone());
    for (const Shape& s : {Shape(BRepPrimAPI_MakeBox(5, 6, 7).Shape()), cylinder(4, 9), sphere(7), Shape(tube.Shape())}) {
        auto mesh = meshForExport(s);
        ASSERT_TRUE(mesh.ok()) << mesh.error().message;
        const MeshClosureReport report = checkClosure(mesh.value());
        EXPECT_EQ(report.boundaryOrNonManifoldEdges, 0u);
        EXPECT_EQ(report.misorientedEdges, 0u);
        EXPECT_GT(meshVolume(mesh.value()), 0.0);
    }
    // Hollow cylinder volume within deflection (inner surface: the mesh lies outside it, so
    // the bound is two-sided).
    MeshExportParams params;
    auto mesh = meshForExport(Shape(tube.Shape()), params);
    const double exactV = kPi * (400 - 144) * 30;
    const double area = 2 * kPi * 32 * 30 + 2 * kPi * 256;
    EXPECT_NEAR(meshVolume(mesh.value()), exactV, area * params.linearDeflection);
}

TEST(MeshExport, ReadStlErrors) {
    EXPECT_EQ(readStl("hello world").error().code, ErrorCode::ParseError);
    EXPECT_EQ(readStl("solid x\n facet normal 0 0 1\n outer loop\n vertex 0 0 0\n").error().code, ErrorCode::ParseError);
    auto r = readStl("solid x\n facet normal 0 0 one\n");
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("line 2"), std::string::npos) << r.error().message;
    EXPECT_EQ(exportStlBinary(Shape()).error().code, ErrorCode::InvalidInput);
    MeshExportParams bad;
    bad.linearDeflection = 0;
    EXPECT_EQ(exportStlBinary(sphere(1), bad).error().code, ErrorCode::InvalidInput);
}

namespace {

struct ParsedModel {
    std::string unit;
    std::map<std::string, std::string> displayColors;  // base name -> displaycolor
    struct Object {
        std::string id, name, pid, pindex;
        TriangleMesh mesh;
    };
    std::vector<Object> objects;
    std::vector<std::string> buildItems;
};

void XMLCALL onStart(void* user, const XML_Char* name, const XML_Char** attrs) {
    auto* m = static_cast<ParsedModel*>(user);
    std::map<std::string, std::string> a;
    for (int i = 0; attrs[i]; i += 2) a[attrs[i]] = attrs[i + 1];
    const std::string tag = name;
    if (tag == "model") m->unit = a["unit"];
    else if (tag == "base") m->displayColors[a["name"]] = a["displaycolor"];
    else if (tag == "object") m->objects.push_back({a["id"], a["name"], a["pid"], a["pindex"], {}});
    else if (tag == "vertex")
        m->objects.back().mesh.vertices.push_back({std::stod(a["x"]), std::stod(a["y"]), std::stod(a["z"])});
    else if (tag == "triangle")
        m->objects.back().mesh.triangles.push_back({static_cast<std::uint32_t>(std::stoul(a["v1"])),
                                                    static_cast<std::uint32_t>(std::stoul(a["v2"])),
                                                    static_cast<std::uint32_t>(std::stoul(a["v3"]))});
    else if (tag == "item") m->buildItems.push_back(a["objectid"]);
}

void XMLCALL onEnd(void*, const XML_Char*) {}

bool parseXml(const std::string& xml, ParsedModel* model, std::string& error) {
    XML_Parser parser = XML_ParserCreate(nullptr);
    XML_SetUserData(parser, model);
    XML_SetElementHandler(parser, onStart, onEnd);
    const bool ok = XML_Parse(parser, xml.data(), static_cast<int>(xml.size()), 1) == XML_STATUS_OK;
    if (!ok) error = XML_ErrorString(XML_GetErrorCode(parser));
    XML_ParserFree(parser);
    return ok;
}

}  // namespace

TEST(MeshExport, ThreeMfPackageWithTwoColouredObjects) {
    const double r = 10;
    std::vector<ThreeMfObject> objects{
        {"Block <A&B>", Shape(BRepPrimAPI_MakeBox(gp_Pnt(30, 0, 0), 10, 20, 30).Shape()), Rgb{200, 30, 40}},
        {"Ball", sphere(r), Rgb{10, 120, 250}},
        {"Plain", cylinder(3, 5), std::nullopt},
    };
    auto pkg = export3mf(objects);
    ASSERT_TRUE(pkg.ok()) << pkg.error().message;
    ASSERT_GE(pkg.value().size(), 4u);
    EXPECT_EQ(pkg.value().substr(0, 4), std::string("PK\x03\x04", 4));

    auto entries = readZip(pkg.value());
    ASSERT_TRUE(entries.ok()) << entries.error().message;
    std::map<std::string, std::string> files;
    for (const ZipEntry& e : entries.value()) files[e.name] = e.data;
    ASSERT_EQ(files.size(), 3u);
    ASSERT_TRUE(files.count("[Content_Types].xml"));
    ASSERT_TRUE(files.count("_rels/.rels"));
    ASSERT_TRUE(files.count("3D/3dmodel.model"));

    std::string error;
    ParsedModel dummy;
    EXPECT_TRUE(parseXml(files["[Content_Types].xml"], &dummy, error)) << error;
    EXPECT_TRUE(parseXml(files["_rels/.rels"], &dummy, error)) << error;
    EXPECT_NE(files["_rels/.rels"].find("Target=\"/3D/3dmodel.model\""), std::string::npos);

    ParsedModel model;
    ASSERT_TRUE(parseXml(files["3D/3dmodel.model"], &model, error)) << error;
    EXPECT_EQ(model.unit, "millimeter");
    ASSERT_EQ(model.objects.size(), 3u);
    EXPECT_EQ(model.buildItems.size(), 3u);
    EXPECT_EQ(model.objects[0].name, "Block <A&B>");
    EXPECT_EQ(model.displayColors["Block <A&B>"], "#C81E28FF");
    EXPECT_EQ(model.displayColors["Ball"], "#0A78FAFF");
    EXPECT_EQ(model.objects[0].pindex, "0");
    EXPECT_EQ(model.objects[1].pindex, "1");
    EXPECT_TRUE(model.objects[2].pid.empty());

    const double volumes[3] = {6000.0, 4.0 / 3.0 * kPi * r * r * r, kPi * 9 * 5};
    const double areas[3] = {2200.0, 4 * kPi * r * r, 2 * kPi * 3 * 5 + 2 * kPi * 9};
    for (int i = 0; i < 3; ++i) {
        const TriangleMesh& mesh = model.objects[i].mesh;
        for (const auto& t : mesh.triangles)
            for (std::uint32_t v : t) ASSERT_LT(v, mesh.vertices.size());
        const MeshClosureReport closure = checkClosure(mesh);
        EXPECT_TRUE(closure.closed()) << "object " << i << ": " << closure.boundaryOrNonManifoldEdges << " open, "
                                      << closure.misorientedEdges << " misoriented";
        expectWithinDeflection(meshVolume(mesh), volumes[i], areas[i], MeshExportParams{}.linearDeflection + 1e-4);
    }
    // The block keeps its placement: all x in [30, 40].
    for (const auto& v : model.objects[0].mesh.vertices) {
        EXPECT_GE(v[0], 30 - 1e-6);
        EXPECT_LE(v[0], 40 + 1e-6);
    }

    // Corrupting one byte of the model data breaks the CRC or the deflate stream.
    std::string broken = pkg.value();
    const std::size_t at = broken.find("3D/3dmodel.model") + 20;
    broken[at] = static_cast<char>(broken[at] ^ 0x5A);
    EXPECT_FALSE(readZip(broken).ok());
    EXPECT_EQ(export3mf({}).error().code, ErrorCode::InvalidInput);
}
