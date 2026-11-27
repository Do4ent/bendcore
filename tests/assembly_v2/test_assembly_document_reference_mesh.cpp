// Справочная сетка сборки (docs/assembly/ASSEMBLY_DOCUMENT.md, раздел 14): компонент-сетка
// не входит в BOM, массу, развёртку деталей и пересечения; положение и единицы меняют мировую
// сетку; правка отменяется через AssemblyHistory::apply; файл v4 с встроенной сеткой
// сохраняется и загружается; битый файл отклоняется.

#include "assembly_document_helpers.hpp"

#include <bendcore/assembly_v2/bom.hpp>
#include <bendcore/assembly_v2/history.hpp>
#include <bendcore/assembly_v2/interference.hpp>
#include <bendcore/kernel/mesh_export.hpp>

#include <cmath>

using namespace bendcore::assembly_v2;
using namespace bendcore::assembly_v2::testing;
namespace k = bendcore::kernel;
using k::ErrorCode;

namespace {

std::string cubeStl(double a) {
    auto stl = k::exportStlBinary(k::Shape(box(0, 0, 0, a, a, a)));
    EXPECT_TRUE(stl.ok());
    return stl.value();
}

// Две детали 10×10×10 на расстоянии 5 мм друг от друга и сетка-куб 30 мм, накрывающая обе.
struct Scene {
    AssemblyDocument doc{"Scan check"};
    DefinitionId block = 0;
    OccurrenceId a = 0, b = 0;
    ReferenceMeshId mesh = 0;

    Scene() {
        block = doc.addPart("Block", {box(0, 0, 0, 10, 10, 10), nullptr}, {"B.01", "Брусок", "Сталь", 7850}).value();
        a = doc.addOccurrence(block, Transform::identity()).value();
        b = doc.addOccurrence(block, Transform::fromTranslation(Vec3(15, 0, 0))).value();
        auto m = doc.addReferenceMesh("", "scan.stl", k::MeshFileFormat::Stl, cubeStl(30.0), "mm",
                                      Transform::fromTranslation(Vec3(-5, -5, -5)));
        EXPECT_TRUE(m.ok()) << (m.ok() ? "" : m.error().message);
        mesh = m.ok() ? m.value() : 0;
    }
};

}  // namespace

TEST(AssemblyReferenceMesh, NotInBomMassOrInterference) {
    Scene s;
    ASSERT_EQ(s.doc.referenceMeshes().size(), 1u);
    EXPECT_EQ(s.doc.referenceMeshes()[0].name, "scan.stl");
    auto bom = flatBom(s.doc);
    ASSERT_TRUE(bom.ok());
    ASSERT_EQ(bom.value().rows.size(), 1u);
    EXPECT_EQ(bom.value().totalParts, 2);
    EXPECT_NEAR(bom.value().totalVolume, 2000.0, 1e-6);
    EXPECT_NEAR(*bom.value().totalMass, 2000.0e-9 * 7850, 1e-9);
    auto placed = s.doc.placedParts();
    ASSERT_TRUE(placed.ok());
    EXPECT_EQ(placed.value().size(), 2u);
    auto inter = checkInterference(s.doc);
    ASSERT_TRUE(inter.ok());
    EXPECT_TRUE(inter.value().pairs.empty());  // the mesh crosses both blocks, but is no part
    auto xcaf = s.doc.toXcaf();
    ASSERT_TRUE(xcaf.ok());

    auto world = s.doc.worldReferenceMesh(s.mesh);
    ASSERT_TRUE(world.ok());
    const k::MeshProperties p = k::meshProperties(world.value());
    EXPECT_NEAR(p.min[0], -5.0, 1e-9);
    EXPECT_NEAR(p.max[2], 25.0, 1e-9);
    EXPECT_NEAR(p.volume, 27000.0, 1e-6);
}

TEST(AssemblyReferenceMesh, MoveUnitsAndUndo) {
    Scene s;
    AssemblyHistory h(std::move(s.doc));
    const ReferenceMeshId id = s.mesh;
    ASSERT_TRUE(h.apply("move mesh", [&](AssemblyDocument& d) {
                     return d.setReferenceMeshPlacement(id, Transform::fromAxisAngle(Vec3::UnitZ(), M_PI / 2, Vec3(100, 0, 0)));
                 }).ok());
    ASSERT_TRUE(h.apply("mesh unit", [&](AssemblyDocument& d) { return d.setReferenceMeshUnit(id, "in"); }).ok());
    auto world = h.document().worldReferenceMesh(id);
    ASSERT_TRUE(world.ok());
    k::MeshProperties p = k::meshProperties(world.value());
    EXPECT_NEAR(p.min[0], 100.0 - 30.0 * 25.4, 1e-6);
    EXPECT_NEAR(p.max[0], 100.0, 1e-6);
    EXPECT_NEAR(p.max[1], 30.0 * 25.4, 1e-6);
    EXPECT_FALSE(h.apply("bad", [&](AssemblyDocument& d) { return d.setReferenceMeshUnit(id, "ft"); }).ok());
    EXPECT_FALSE(h.apply("bad", [&](AssemblyDocument& d) { return d.setReferenceMeshDisplay(id, true, 2.0); }).ok());
    ASSERT_TRUE(h.undo().ok());
    ASSERT_TRUE(h.undo().ok());
    p = k::meshProperties(h.document().worldReferenceMesh(id).value());
    EXPECT_NEAR(p.min[0], -5.0, 1e-9);
    ASSERT_TRUE(h.apply("remove mesh", [&](AssemblyDocument& d) { return d.removeReferenceMesh(id); }).ok());
    EXPECT_TRUE(h.document().referenceMeshes().empty());
    ASSERT_TRUE(h.undo().ok());
    EXPECT_EQ(h.document().referenceMeshes().size(), 1u);
}

TEST(AssemblyReferenceMesh, SaveAndLoad) {
    Scene s;
    ASSERT_TRUE(s.doc.setReferenceMeshDisplay(s.mesh, false, 0.3).ok());
    ASSERT_TRUE(s.doc.setReferenceMeshName(s.mesh, "Скан").ok());
    ASSERT_TRUE(s.doc.setReferenceMeshUnit(s.mesh, "cm").ok());
    auto text = s.doc.toJson();
    ASSERT_TRUE(text.ok());
    EXPECT_NE(text.value().find("\"version\": 4"), std::string::npos);
    EXPECT_NE(text.value().find("\"referenceMeshes\""), std::string::npos);
    auto loaded = AssemblyDocument::fromJson(text.value(), {});
    ASSERT_TRUE(loaded.ok()) << loaded.error().message;
    const AssemblyDocument& d = loaded.value().document;
    ASSERT_EQ(d.referenceMeshes().size(), 1u);
    const ReferenceMeshComponent& m = d.referenceMeshes()[0];
    EXPECT_EQ(m.id, s.mesh);
    EXPECT_EQ(m.name, "Скан");
    EXPECT_EQ(m.unit, "cm");
    EXPECT_FALSE(m.visible);
    EXPECT_DOUBLE_EQ(m.opacity, 0.3);
    EXPECT_EQ(*m.data, *s.doc.referenceMeshes()[0].data);
    const k::MeshProperties p = k::meshProperties(d.worldReferenceMesh(m.id).value());
    EXPECT_NEAR(p.max[0], 295.0, 1e-9);  // 30 cm cube moved by -5 mm
    EXPECT_FALSE(loaded.value().report.migrated);
    // A new mesh gets a fresh id.
    AssemblyDocument copy = d;
    auto again = copy.addReferenceMesh("x", "x.stl", k::MeshFileFormat::Stl, cubeStl(1.0));
    ASSERT_TRUE(again.ok());
    EXPECT_GT(again.value(), m.id);

    // A broken embedded mesh fails the load with the reader's code.
    std::string bad = text.value();
    const auto at = bad.find("\"data\": \"") + 9;
    bad.replace(at, bad.find('"', at) - at, "AAAA");
    auto broken = AssemblyDocument::fromJson(bad, {});
    ASSERT_FALSE(broken.ok());
    EXPECT_NE(broken.error().message.find("scan.stl"), std::string::npos) << broken.error().message;
}

TEST(AssemblyReferenceMesh, BrokenFileIsRefused) {
    AssemblyDocument doc("A");
    std::string stl = cubeStl(10.0);
    stl.resize(stl.size() - 1);
    auto r = doc.addReferenceMesh("", "cut.stl", k::MeshFileFormat::Stl, stl);
    ASSERT_FALSE(r.ok());
    EXPECT_EQ(r.error().code, ErrorCode::ParseError);
    EXPECT_EQ(r.error().message.rfind("mesh 'cut.stl': binary STL", 0), 0u) << r.error().message;
    EXPECT_TRUE(doc.referenceMeshes().empty());
    EXPECT_EQ(doc.addReferenceMesh("", "a.stl", k::MeshFileFormat::Stl, cubeStl(1.0), "furlong").error().code,
              ErrorCode::InvalidInput);
    EXPECT_FALSE(doc.removeReferenceMesh(42).ok());
}
