// Постановка крепежа в отверстия через документ сборки: болт + шайбы + гайка в плите,
// решение сопряжений, отсутствие пересечений, BOM; винт в глухом отверстии; ошибки.

#include <bendcore/assembly_v2/bom.hpp>
#include <bendcore/assembly_v2/fasteners.hpp>
#include <bendcore/assembly_v2/interference.hpp>

#include <BRepAlgoAPI_Cut.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <gp_Ax2.hxx>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <numbers>

using namespace bendcore::assembly_v2;
using namespace bendcore::assembly_v2::fasteners;
using bendcore::kernel::ErrorCode;

namespace {

TopoDS_Shape cut(const TopoDS_Shape& a, const TopoDS_Shape& b) {
    BRepAlgoAPI_Cut op(a, b);
    EXPECT_TRUE(op.IsDone());
    return op.Shape();
}

TopoDS_Shape holeTool(double x, double y, double z, double r, double h) {
    return BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(x, y, z), gp::DZ()), r, h).Shape();
}

// Плита 100×60×20 с двумя сквозными отверстиями Ø11 (25, 30), (75, 30) и отверстием Ø6.
TopoDS_Shape plate() {
    TopoDS_Shape s = BRepPrimAPI_MakeBox(gp_Pnt(0, 0, 0), 100, 60, 20).Shape();
    s = cut(s, holeTool(25, 30, -1, 5.5, 22));
    s = cut(s, holeTool(75, 30, -1, 5.5, 22));
    return cut(s, holeTool(50, 10, -1, 3, 22));
}

Fastener make(FastenerStandard st, int d, double l = 0) {
    auto r = makeFastener({st, d, l});
    EXPECT_TRUE(r.ok()) << (r.ok() ? "" : r.error().message);
    return r.value();
}

void expectVec(const Vec3& a, const Vec3& b, double tol) {
    EXPECT_NEAR((a - b).norm(), 0, tol) << "(" << a.transpose() << ") vs (" << b.transpose() << ")";
}

struct BoltedPlate {
    AssemblyDocument doc{"Плита с болтами"};
    OccurrenceId plateOcc = 0;
    DefinitionId bolt = 0, washer = 0, nut = 0;

    explicit BoltedPlate(double boltLength) {
        auto p = doc.addPart("Плита", {plate(), nullptr}, {"ПЛ-01", "Плита", "Сталь", 7850});
        plateOcc = doc.addOccurrence(p.value(), Transform::identity()).value();
        EXPECT_TRUE(doc.setFixed(plateOcc, true).ok());
        bolt = addFastenerPart(doc, make(FastenerStandard::Gost7798, 10, boltLength)).value();
        washer = addFastenerPart(doc, make(FastenerStandard::Gost11371, 10)).value();
        nut = addFastenerPart(doc, make(FastenerStandard::Gost5915, 10)).value();
    }
    FastenerStack stack() const { return {bolt, {washer}, {washer}, nut}; }
};

}  // namespace

TEST(FastenerPlacement, FindHoleSites) {
    BoltedPlate m(40);
    auto sites = findHoleSites(m.doc, {m.plateOcc}, Vec3(0, 0, -1), 5.5);
    ASSERT_TRUE(sites.ok()) << sites.error().message;
    ASSERT_EQ(sites.value().size(), 2u);  // Ø6 не подходит по радиусу
    for (std::size_t i = 0; i < 2; ++i) {
        const HoleSite& s = sites.value()[i];
        ASSERT_TRUE(s.exitFace.has_value());
        EXPECT_NEAR(s.depth, 20, 1e-9);
        auto entry = m.doc.worldEntity(s.entryFace).value();
        auto exit = m.doc.worldEntity(*s.exitFace).value();
        EXPECT_NEAR(entry.point.z(), 20, 1e-9);
        EXPECT_NEAR(entry.direction.z(), 1, 1e-12);
        EXPECT_NEAR(exit.point.z(), 0, 1e-9);
        auto hole = m.doc.worldEntity(s.hole).value();
        EXPECT_NEAR(hole.point.x(), i == 0 ? 25 : 75, 1e-9);
        EXPECT_NEAR(hole.point.y(), 30, 1e-9);
    }
    // Снизу вверх вход и выход меняются местами.
    auto up = findHoleSites(m.doc, {m.plateOcc}, Vec3(0, 0, 1), 5.5);
    ASSERT_TRUE(up.ok());
    EXPECT_NEAR(m.doc.worldEntity(up.value()[0].entryFace).value().point.z(), 0, 1e-9);
    // Вдоль X отверстий нет.
    auto none = findHoleSites(m.doc, {m.plateOcc}, Vec3(1, 0, 0), 5.5);
    ASSERT_FALSE(none.ok());
    EXPECT_EQ(none.error().code, ErrorCode::InvalidInput);
}

TEST(FastenerPlacement, BoltWasherNutInPlate) {
    BoltedPlate m(40);
    auto sites = findHoleSites(m.doc, {m.plateOcc}, Vec3(0, 0, -1), 5.5).value();
    auto placed = insertFastenerStacks(m.doc, m.stack(), sites);
    ASSERT_TRUE(placed.ok()) << placed.error().message;
    ASSERT_EQ(placed.value().size(), 2u);
    for (std::size_t i = 0; i < 2; ++i) {
        const StackPlacement& sp = placed.value()[i];
        // Пакет: шайба 2 + плита 20 + шайба 2 + гайка 8 = 32; болт 40 выступает на 8.
        EXPECT_NEAR(sp.grip, 32, 1e-9);
        EXPECT_NEAR(sp.protrusion, 8, 1e-9);
        EXPECT_EQ(sp.mates.size(), 8u);
    }
    // Положения до решения уже удовлетворяют сопряжениям.
    std::vector<Transform> before;
    for (const Occurrence& o : m.doc.topLevel()) before.push_back(o.placement);

    auto solved = m.doc.solve();
    ASSERT_TRUE(solved.ok()) << solved.error().message;
    EXPECT_TRUE(solved.value().success);
    EXPECT_LT(solved.value().residualNorm, 1e-9);
    EXPECT_EQ(solved.value().totalDof, 8);  // 2 × (болт, 2 шайбы, гайка): поворот вокруг оси
    for (std::size_t i = 0; i < before.size(); ++i) {
        expectVec(m.doc.topLevel()[i].placement.position, before[i].position, 1e-8);
    }

    const StackPlacement& first = placed.value()[0];
    const Occurrence* bolt = m.doc.findOccurrence(first.screw);
    expectVec(bolt->placement.position, Vec3(25, 30, 22), 1e-8);  // опора головки на шайбе z = 22
    expectVec(bolt->placement.rotation * Vec3::UnitZ(), Vec3::UnitZ(), 1e-12);
    const Occurrence* nut = m.doc.findOccurrence(*first.nut);
    expectVec(nut->placement.position, Vec3(25, 30, -2), 1e-8);   // под шайбой z ∈ [−2, 0]
    expectVec(nut->placement.rotation * Vec3::UnitZ(), -Vec3::UnitZ(), 1e-12);
    expectVec(m.doc.findOccurrence(first.headWashers[0])->placement.position, Vec3(25, 30, 20), 1e-8);
    expectVec(m.doc.findOccurrence(first.nutWashers[0])->placement.position, Vec3(25, 30, 0), 1e-8);

    // Пересечений нет. Касания: шайба–плита (2), болт–шайба, гайка–шайба и условная резьба
    // гайка–болт (отверстие гайки = номинальный диаметр стержня) — 5 на отверстие.
    auto report = checkInterference(m.doc);
    ASSERT_TRUE(report.ok()) << report.error().message;
    int touching = 0;
    for (const InterferencePair& p : report.value().pairs) {
        EXPECT_EQ(p.kind, ContactKind::Touching)
            << report.value().parts[p.a].pathName << " / " << report.value().parts[p.b].pathName << " V=" << p.volume;
        if (p.kind == ContactKind::Touching) ++touching;
    }
    EXPECT_EQ(touching, 10);
    EXPECT_LE(report.value().totalVolume, 1e-6);

    // BOM: наименования — обозначения по стандарту, количества, массы стали.
    auto bom = flatBom(m.doc);
    ASSERT_TRUE(bom.ok()) << bom.error().message;
    auto row = [&](const std::string& name) -> const BomRow* {
        for (const BomRow& r : bom.value().rows) {
            if (r.name == name) return &r;
        }
        return nullptr;
    };
    const BomRow* boltRow = row("Болт М10×40 ГОСТ 7798-70");
    const BomRow* washerRow = row("Шайба 10 ГОСТ 11371-78");
    const BomRow* nutRow = row("Гайка М10 ГОСТ 5915-70");
    ASSERT_TRUE(boltRow && washerRow && nutRow);
    EXPECT_EQ(boltRow->quantity, 2);
    EXPECT_EQ(washerRow->quantity, 4);
    EXPECT_EQ(nutRow->quantity, 2);
    EXPECT_EQ(boltRow->designation, "");
    const double washerV = 3.14159265358979323846 / 4 * (21 * 21 - 10.5 * 10.5) * 2;
    EXPECT_NEAR(washerRow->unitMass.value(), washerV * 7850e-9, 1e-12);
}

TEST(FastenerPlacement, SocketScrewInBlindHoleAndRotatedPart) {
    AssemblyDocument doc("Глухое отверстие");
    // Плита 30 мм с глухим отверстием Ø11 глубиной 15 сверху, повёрнутая на 90° вокруг X.
    TopoDS_Shape shape = cut(BRepPrimAPI_MakeBox(gp_Pnt(0, 0, 0), 40, 40, 30).Shape(), holeTool(20, 20, 15, 5.5, 16));
    auto p = doc.addPart("Корпус", {shape, nullptr});
    const Transform pose = Transform::fromAxisAngle(Vec3::UnitX(), std::numbers::pi / 2, Vec3(5, 0, 0));
    const OccurrenceId body = doc.addOccurrence(p.value(), pose).value();
    ASSERT_TRUE(doc.setFixed(body, true).ok());
    const DefinitionId screw = addFastenerPart(doc, make(FastenerStandard::Iso4762, 10, 12)).value();

    const Vec3 insertion = pose.applyToDirection(-Vec3::UnitZ());
    auto sites = findHoleSites(doc, {body}, insertion, 5.5);
    ASSERT_TRUE(sites.ok()) << sites.error().message;
    ASSERT_EQ(sites.value().size(), 1u);
    EXPECT_FALSE(sites.value()[0].exitFace.has_value());

    // Гайка в глухом отверстии — ошибка, документ не меняется.
    const DefinitionId nut = addFastenerPart(doc, make(FastenerStandard::Iso4032, 10)).value();
    const std::size_t count = doc.topLevel().size();
    auto bad = insertFastenerStacks(doc, {screw, {}, {}, nut}, sites.value());
    ASSERT_FALSE(bad.ok());
    EXPECT_EQ(bad.error().code, ErrorCode::InvalidInput);
    EXPECT_EQ(doc.topLevel().size(), count);
    EXPECT_TRUE(doc.mates().empty());

    auto placed = insertFastenerStacks(doc, {screw, {}, {}, std::nullopt}, sites.value());
    ASSERT_TRUE(placed.ok()) << placed.error().message;
    EXPECT_NEAR(placed.value()[0].protrusion, 12, 1e-9);  // стержень уходит в отверстие на l
    auto solved = doc.solve();
    ASSERT_TRUE(solved.ok());
    EXPECT_TRUE(solved.value().success);
    EXPECT_EQ(solved.value().totalDof, 1);
    const Occurrence* s = doc.findOccurrence(placed.value()[0].screw);
    expectVec(s->placement.position, pose.applyToPoint(Vec3(20, 20, 30)), 1e-8);
    expectVec(s->placement.rotation * Vec3::UnitZ(), pose.applyToDirection(Vec3::UnitZ()), 1e-12);
    auto report = checkInterference(doc);
    ASSERT_TRUE(report.ok());
    for (const InterferencePair& pair : report.value().pairs) EXPECT_EQ(pair.kind, ContactKind::Touching);
}

TEST(FastenerPlacement, ShortBoltAndMissingFacesAreErrors) {
    BoltedPlate m(30);  // пакет 32 > 30
    auto sites = findHoleSites(m.doc, {m.plateOcc}, Vec3(0, 0, -1), 5.5).value();
    auto r = insertFastenerStacks(m.doc, m.stack(), sites);
    ASSERT_FALSE(r.ok());
    EXPECT_EQ(r.error().code, ErrorCode::InvalidInput);
    EXPECT_NE(r.error().message.find("too short"), std::string::npos);
    EXPECT_EQ(m.doc.topLevel().size(), 1u);

    // Деталь без карты граней крепежа как болт.
    auto plain = m.doc.addPart("Цилиндр", {holeTool(0, 0, 0, 5, 10), nullptr}).value();
    auto bad = insertFastenerStacks(m.doc, {plain, {}, {}, std::nullopt}, sites);
    ASSERT_FALSE(bad.ok());
    EXPECT_NE(bad.error().message.find("element map"), std::string::npos);
}
