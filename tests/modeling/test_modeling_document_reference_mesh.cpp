// Справочная сетка детали (docs/modeling/PART_DOCUMENT.md, «Справочная сетка»): STL куба
// встраивается в документ, размещается (дюймы, поворот, смещение), не меняет тело и его объём,
// записывается в историю отмены, сохраняется и загружается (v5, байты файла те же); битый файл
// отклоняется; сечение плоскостью XY/XZ становится эскизом из вспомогательных отрезков —
// квадрат со стороной 10 мм.

#include "modeling_document_test_util.hpp"

#include <bendcore/kernel/mesh_export.hpp>

#include <nlohmann/json.hpp>

#include <BRepPrimAPI_MakeBox.hxx>

#include <cmath>
#include <set>

using namespace bendcore::modeling;
using namespace bendcore::modeling::testutil;
namespace k = bendcore::kernel;
using nlohmann::json;

namespace {

std::string cubeStl(double a) {
    auto stl = k::exportStlBinary(k::Shape(BRepPrimAPI_MakeBox(a, a, a).Shape()));
    EXPECT_TRUE(stl.ok());
    return stl.value();
}

ReferenceMeshSpec cubeSpec(double a = 10.0) {
    ReferenceMeshSpec spec;
    spec.fileName = "cube.stl";
    spec.data = cubeStl(a);
    return spec;
}

PartDocument boxPart() {
    PartDocument doc(partRegistry());
    add(doc, {"Box", "Body", "", {{"length", "40"}, {"width", "30"}, {"height", "20"}}, {}, {}, false});
    EXPECT_TRUE(doc.rebuild().failed.empty());
    return doc;
}

}  // namespace

TEST(PartReferenceMesh, InsertPlaceAndKeepBody) {
    PartDocument doc = boxPart();
    const double v0 = volume(doc.finalShape().shape());
    auto id = doc.addReferenceMesh(cubeSpec());
    ASSERT_TRUE(id.ok()) << id.error().message;
    EXPECT_EQ(id.value(), "Mesh001");
    auto info = doc.referenceMesh("Mesh001");
    ASSERT_TRUE(info.ok());
    EXPECT_EQ(info.value().label, "cube.stl");
    EXPECT_EQ(info.value().properties.triangles, 12u);
    EXPECT_TRUE(info.value().properties.closed);
    EXPECT_NEAR(info.value().properties.volume, 1000.0, 1e-9);
    EXPECT_NEAR(info.value().properties.area, 600.0, 1e-9);

    // The mesh is not part of the tree: no feature, same body, same volume after a rebuild.
    EXPECT_EQ(doc.size(), 1u);
    const RebuildReport rep = doc.rebuild();
    EXPECT_TRUE(rep.recomputed.empty());
    EXPECT_NEAR(volume(doc.finalShape().shape()), v0, 1e-9);
    EXPECT_EQ(doc.bodies().size(), 1u);

    // File in inches, turned 90 degrees about Z, moved 100 mm along X.
    ASSERT_TRUE(doc.setReferenceMeshPlacement("Mesh001", "in", {0, 0, 90}, {100, 0, 0}).ok());
    info = doc.referenceMesh("Mesh001");
    const auto& p = info.value().properties;
    EXPECT_NEAR(p.min[0], 100.0 - 254.0, 1e-9);
    EXPECT_NEAR(p.max[0], 100.0, 1e-9);
    EXPECT_NEAR(p.max[1], 254.0, 1e-9);
    EXPECT_NEAR(p.max[2], 254.0, 1e-9);
    EXPECT_NEAR(p.volume, 254.0 * 254.0 * 254.0, 1e-3);
    EXPECT_FALSE(doc.setReferenceMeshPlacement("Mesh001", "yd", {0, 0, 0}, {0, 0, 0}).ok());
    EXPECT_FALSE(doc.setReferenceMeshDisplay("Mesh001", true, 0.0).ok());
    ASSERT_TRUE(doc.setReferenceMeshDisplay("Mesh001", false, 0.3).ok());
    ASSERT_TRUE(doc.setReferenceMeshLabel("Mesh001", "Scan").ok());
    EXPECT_FALSE(doc.referenceMesh("Mesh001").value().visible);

    // Undo: label, display, placement, insert; redo brings them back.
    for (int i = 0; i < 3; ++i) ASSERT_TRUE(doc.undo().ok());
    EXPECT_NEAR(doc.referenceMesh("Mesh001").value().properties.max[0], 10.0, 1e-9);
    ASSERT_TRUE(doc.undo().ok());
    EXPECT_TRUE(doc.referenceMeshIds().empty());
    ASSERT_TRUE(doc.redo().ok());
    ASSERT_TRUE(doc.redo().ok());
    EXPECT_NEAR(doc.referenceMesh("Mesh001").value().properties.max[1], 254.0, 1e-9);
    EXPECT_NEAR(volume(doc.finalShape().shape()), v0, 1e-9);

    ASSERT_TRUE(doc.removeReferenceMesh("Mesh001").ok());
    EXPECT_TRUE(doc.referenceMeshIds().empty());
    EXPECT_FALSE(doc.removeReferenceMesh("Mesh001").ok());
}

TEST(PartReferenceMesh, SaveAndLoadEmbeddedFile) {
    PartDocument doc = boxPart();
    ReferenceMeshSpec spec = cubeSpec();
    spec.unit = "cm";
    spec.rotationDegrees = {0, 30, 0};
    spec.translation = {1, 2, 3};
    spec.opacity = 0.4;
    const std::string bytes = spec.data;
    ASSERT_TRUE(doc.addReferenceMesh(spec).ok());
    auto text = doc.toJson();
    ASSERT_TRUE(text.ok());
    const json j = json::parse(text.value());
    EXPECT_EQ(j.at("version"), 5);
    ASSERT_EQ(j.at("referenceMeshes").size(), 1u);
    EXPECT_EQ(j.at("referenceMeshes")[0].at("unit"), "cm");
    EXPECT_TRUE(j.at("referenceMeshes")[0].at("data").is_string());

    auto loaded = PartDocument::fromJson(text.value(), partRegistry());
    ASSERT_TRUE(loaded.ok()) << loaded.error().message;
    PartDocument& b = loaded.value();
    ASSERT_EQ(b.referenceMeshIds(), std::vector<std::string>{"Mesh001"});
    EXPECT_EQ(b.referenceMeshData("Mesh001").value(), bytes);
    const auto ia = doc.referenceMesh("Mesh001").value();
    const auto ib = b.referenceMesh("Mesh001").value();
    EXPECT_EQ(ib.unit, "cm");
    EXPECT_DOUBLE_EQ(ib.opacity, 0.4);
    EXPECT_EQ(ib.rotationDegrees, ia.rotationDegrees);
    EXPECT_EQ(ib.translation, ia.translation);
    for (int k = 0; k < 3; ++k) {
        EXPECT_DOUBLE_EQ(ib.properties.min[k], ia.properties.min[k]);
        EXPECT_DOUBLE_EQ(ib.properties.max[k], ia.properties.max[k]);
    }
    EXPECT_NEAR(ib.properties.volume, 1.0e6, 1e-3);  // 10 cm cube
    EXPECT_FALSE(b.canUndo());
    EXPECT_NEAR(volume(b.finalShape().shape()), 40.0 * 30.0 * 20.0, 1e-6);

    // Without meshes the file stays v3; a v5 file is refused by nothing older than v5 here.
    ASSERT_TRUE(b.removeReferenceMesh("Mesh001").ok());
    EXPECT_EQ(json::parse(b.toJson().value()).at("version"), 3);

    // A damaged embedded file fails the load with the reader's message.
    json bad = j;
    bad["referenceMeshes"][0]["data"] = "AAAA";
    auto broken = PartDocument::fromJson(bad.dump(), partRegistry());
    ASSERT_FALSE(broken.ok());
    EXPECT_NE(broken.error().message.find("cube.stl"), std::string::npos) << broken.error().message;
    bad["referenceMeshes"][0]["data"] = "*not base64*";
    EXPECT_EQ(PartDocument::fromJson(bad.dump(), partRegistry()).error().code, k::ErrorCode::ParseError);
}

TEST(PartReferenceMesh, BrokenFileIsRefused) {
    PartDocument doc = boxPart();
    ReferenceMeshSpec spec = cubeSpec();
    spec.data.resize(spec.data.size() - 3);
    auto r = doc.addReferenceMesh(spec);
    ASSERT_FALSE(r.ok());
    EXPECT_EQ(r.error().code, k::ErrorCode::ParseError);
    EXPECT_EQ(r.error().message.rfind("mesh 'cube.stl': binary STL: the header declares 12 triangles", 0), 0u) << r.error().message;
    EXPECT_TRUE(doc.referenceMeshIds().empty());
    EXPECT_FALSE(doc.canUndo() && doc.undoHistory().back().name.find("mesh") != std::string::npos);

    k::MeshReadLimits limits;
    limits.maxBytes = 200;
    auto big = doc.addReferenceMesh(cubeSpec(), limits);
    ASSERT_FALSE(big.ok());
    EXPECT_EQ(big.error().code, k::ErrorCode::InvalidInput);
    ASSERT_TRUE(doc.addReferenceMesh(cubeSpec()).ok());
    ReferenceMeshSpec dup = cubeSpec();
    dup.id = "Mesh001";
    EXPECT_FALSE(doc.addReferenceMesh(dup).ok());
    dup.id = "Body";  // feature ids are taken too
    EXPECT_FALSE(doc.addReferenceMesh(dup).ok());
}

TEST(PartReferenceMesh, SectionBecomesConstructionSketch) {
    PartDocument doc = boxPart();
    ASSERT_TRUE(doc.addReferenceMesh(cubeSpec()).ok());

    auto checkSquare = [&](const std::string& sketchId, double side) {
        auto f = doc.feature(sketchId);
        ASSERT_TRUE(f.ok());
        ASSERT_TRUE(f.value().sketch.has_value());
        const SketchData& s = *f.value().sketch;
        ASSERT_EQ(s.geometry.size(), 4u);
        ASSERT_EQ(s.points.size(), 4u);
        std::set<double> xs, ys;
        for (const auto& p : s.points) {
            EXPECT_TRUE(p.fixed);
            xs.insert(std::stod(p.x));
            ys.insert(std::stod(p.y));
        }
        EXPECT_EQ(xs, (std::set<double>{0.0, side}));
        EXPECT_EQ(ys, (std::set<double>{0.0, side}));
        for (const auto& g : s.geometry) {
            EXPECT_TRUE(g.construction);
            EXPECT_EQ(g.type, SketchGeometryType::Line);
            const auto& a = s.points[g.start];
            const auto& b = s.points[g.end];
            EXPECT_DOUBLE_EQ(std::hypot(std::stod(a.x) - std::stod(b.x), std::stod(a.y) - std::stod(b.y)), side);
        }
    };

    auto xy = doc.addMeshSectionSketch("Mesh001", "XY", 3.7);
    ASSERT_TRUE(xy.ok()) << xy.error().message;
    checkSquare(xy.value(), 10.0);
    EXPECT_EQ(doc.feature(xy.value()).value().options.at("plane"), "XY");
    // XZ: the sketch normal is -Y, so offset -5 is the plane y = 5; sketch axes are X and Z.
    auto xz = doc.addMeshSectionSketch("Mesh001", "XZ", -5.0, "MeshCut");
    ASSERT_TRUE(xz.ok()) << xz.error().message;
    EXPECT_EQ(xz.value(), "MeshCut");
    checkSquare("MeshCut", 10.0);

    const RebuildReport rep = doc.rebuild();
    EXPECT_TRUE(rep.failed.empty()) << (rep.failed.empty() ? "" : messages(info(doc, rep.failed.front())));
    EXPECT_NE(info(doc, xy.value()).state, FeatureState::Error);
    EXPECT_NEAR(volume(doc.finalShape().shape()), 40.0 * 30.0 * 20.0, 1e-6);

    // Undo removes the sketch; errors are explicit.
    ASSERT_TRUE(doc.undo().ok());
    EXPECT_FALSE(doc.feature("MeshCut").ok());
    auto miss = doc.addMeshSectionSketch("Mesh001", "XY", 50.0);
    ASSERT_FALSE(miss.ok());
    EXPECT_EQ(miss.error().message, "plane XY at 50 mm does not cut mesh 'Mesh001'");
    EXPECT_FALSE(doc.addMeshSectionSketch("Mesh001", "UV", 1.0).ok());
    EXPECT_FALSE(doc.addMeshSectionSketch("Nope", "XY", 1.0).ok());
}
