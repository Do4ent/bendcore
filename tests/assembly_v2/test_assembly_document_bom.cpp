// Состав изделия: количества и массы против формул; разнесённый вид.

#include "assembly_document_helpers.hpp"

#include <bendcore/assembly_v2/bom.hpp>

using namespace bendcore::assembly_v2;
using namespace bendcore::assembly_v2::testing;
using bendcore::kernel::ErrorCode;

namespace {

constexpr double kSteel = 7850, kAlu = 2700;

// Рама 100×50×10 (сталь), 2 колеса (ступица r = 10, h = 20, сталь + 2 диска 30×30×2, алюминий),
// 4 болта r = 3, h = 20 (плотность задаётся в тесте).
struct Cart {
    AssemblyDocument doc{"Cart"};
    DefinitionId frame = 0, hub = 0, rim = 0, bolt = 0, wheel = 0;
    OccurrenceId frameOcc = 0, wheel1 = 0, wheel2 = 0;
    std::vector<OccurrenceId> bolts;

    Cart() {
        frame = doc.addPart("Frame", {box(0, 0, 0, 100, 50, 10), nullptr}, {"CRT.01", "Рама", "Сталь 20", kSteel}).value();
        hub = doc.addPart("Hub", {cylinder(0, 0, 0, 10, 20), nullptr}, {"CRT.02.01", "Ступица", "Сталь 45", kSteel}).value();
        rim = doc.addPart("Rim", {box(-15, -15, 0, 30, 30, 2), nullptr}, {"CRT.02.02", "Диск", "АД31", kAlu}).value();
        bolt = doc.addPart("Bolt", {cylinder(0, 0, 0, 3, 20), nullptr}, {"", "Болт М6", "", std::nullopt}).value();
        wheel = doc.addSubassembly("Wheel", {"CRT.02", "Колесо", "", std::nullopt}).value();
        EXPECT_TRUE(doc.addOccurrence(hub, Transform::identity(), "", wheel).ok());
        EXPECT_TRUE(doc.addOccurrence(rim, Transform::fromTranslation(Vec3(0, 0, -2)), "", wheel).ok());
        EXPECT_TRUE(doc.addOccurrence(rim, Transform::fromTranslation(Vec3(0, 0, 20)), "", wheel).ok());
        frameOcc = doc.addOccurrence(frame, Transform::identity()).value();
        wheel1 = doc.addOccurrence(wheel, Transform::fromTranslation(Vec3(10, -30, 0))).value();
        wheel2 = doc.addOccurrence(wheel, Transform::fromTranslation(Vec3(90, -30, 0))).value();
        for (int i = 0; i < 4; ++i)
            bolts.push_back(doc.addOccurrence(bolt, Transform::fromTranslation(Vec3(10 + 25 * i, 25, 10))).value());
    }
};

const double vFrame = 100 * 50 * 10;
const double vHub = kPi * 10 * 10 * 20;
const double vRim = 30 * 30 * 2;
const double vBolt = kPi * 3 * 3 * 20;
double kg(double mm3, double rho) { return mm3 * 1e-9 * rho; }

}  // namespace

TEST(AssemblyDocumentBom, FlatQuantitiesAndMasses) {
    Cart c;
    auto bom = flatBom(c.doc);
    ASSERT_TRUE(bom.ok()) << bom.error().message;
    const auto& rows = bom.value().rows;
    ASSERT_EQ(rows.size(), 4u);
    // Порядок первого появления при обходе: рама, ступица, диск, болт.
    EXPECT_EQ(rows[0].definition, c.frame);
    EXPECT_EQ(rows[1].definition, c.hub);
    EXPECT_EQ(rows[2].definition, c.rim);
    EXPECT_EQ(rows[3].definition, c.bolt);
    EXPECT_EQ(rows[0].quantity, 1);
    EXPECT_EQ(rows[1].quantity, 2);
    EXPECT_EQ(rows[2].quantity, 4);
    EXPECT_EQ(rows[3].quantity, 4);
    EXPECT_EQ(rows[0].designation, "CRT.01");
    EXPECT_EQ(rows[0].name, "Рама");
    EXPECT_EQ(rows[2].material, "АД31");
    EXPECT_EQ(rows[3].position, 4);
    EXPECT_NEAR(rows[1].unitVolume, vHub, 1e-6);
    ASSERT_TRUE(rows[0].unitMass && rows[1].totalMass && rows[2].totalMass);
    EXPECT_NEAR(*rows[0].unitMass, kg(vFrame, kSteel), 1e-12);
    EXPECT_NEAR(*rows[1].totalMass, 2 * kg(vHub, kSteel), 1e-12);
    EXPECT_NEAR(*rows[2].totalMass, 4 * kg(vRim, kAlu), 1e-12);
    EXPECT_FALSE(rows[3].unitMass.has_value());  // у болта нет плотности
    EXPECT_FALSE(bom.value().totalMass.has_value());
    EXPECT_EQ(bom.value().totalParts, 11);
    EXPECT_NEAR(bom.value().totalVolume, vFrame + 2 * vHub + 4 * vRim + 4 * vBolt, 1e-6);

    ASSERT_TRUE(c.doc.setProperties(c.bolt, {"", "Болт М6", "Сталь", kSteel}).ok());
    auto bom2 = flatBom(c.doc);
    ASSERT_TRUE(bom2.ok());
    ASSERT_TRUE(bom2.value().totalMass.has_value());
    const double total = kg(vFrame, kSteel) + 2 * kg(vHub, kSteel) + 4 * kg(vRim, kAlu) + 4 * kg(vBolt, kSteel);
    EXPECT_NEAR(*bom2.value().totalMass, total, 1e-12);
    EXPECT_FALSE(c.doc.setProperties(c.bolt, {"", "", "", -1.0}).ok());
}

TEST(AssemblyDocumentBom, StructuredLevels) {
    Cart c;
    ASSERT_TRUE(c.doc.setProperties(c.bolt, {"", "Болт М6", "Сталь", kSteel}).ok());
    auto bom = structuredBom(c.doc);
    ASSERT_TRUE(bom.ok()) << bom.error().message;
    const auto& rows = bom.value().rows;
    ASSERT_EQ(rows.size(), 3u);  // рама, колесо, болт
    EXPECT_EQ(rows[1].kind, DefinitionKind::Assembly);
    EXPECT_EQ(rows[1].designation, "CRT.02");
    EXPECT_EQ(rows[1].quantity, 2);
    EXPECT_EQ(rows[2].quantity, 4);
    EXPECT_EQ(rows[2].positionPath, "3");
    ASSERT_EQ(rows[1].children.size(), 2u);
    EXPECT_EQ(rows[1].children[0].positionPath, "2.1");
    EXPECT_EQ(rows[1].children[0].level, 1);
    EXPECT_EQ(rows[1].children[0].quantity, 1);  // ступиц в одном колесе
    EXPECT_EQ(rows[1].children[1].quantity, 2);  // дисков в одном колесе
    const double wheelMass = kg(vHub, kSteel) + 2 * kg(vRim, kAlu);
    ASSERT_TRUE(rows[1].unitMass && rows[1].totalMass);
    EXPECT_NEAR(*rows[1].unitMass, wheelMass, 1e-12);
    EXPECT_NEAR(*rows[1].totalMass, 2 * wheelMass, 1e-12);
    EXPECT_NEAR(rows[1].unitVolume, vHub + 2 * vRim, 1e-6);
    const double total = kg(vFrame, kSteel) + 2 * wheelMass + 4 * kg(vBolt, kSteel);
    ASSERT_TRUE(bom.value().totalMass);
    EXPECT_NEAR(*bom.value().totalMass, total, 1e-12);
}

TEST(AssemblyDocumentBom, ExplodedViewKeepsOriginalPlacements) {
    Cart c;
    ExplodedView v;
    v.name = "Разнесение";
    v.steps.push_back({{c.wheel1, c.wheel2}, Vec3(0, -2, 0), 40});         // колёса вниз по Y на 40
    v.steps.push_back({c.bolts, Vec3(0, 0, 1), 30});                        // болты вверх на 30
    v.steps.push_back({{c.wheel1}, Vec3(-1, 0, 0), 10});                    // первое колесо ещё влево
    EXPECT_EQ(c.doc.addExplodedView(v), 0u);
    ASSERT_EQ(c.doc.explodedViews().size(), 1u);

    auto half = explodedPlacements(c.doc, v, 1.5);  // шаг 0 выполнен, шаг 1 наполовину
    ASSERT_TRUE(half.ok());
    expectVecNear(half.value().at(c.wheel1).position, Vec3(10, -70, 0), 1e-12);
    expectVecNear(half.value().at(c.bolts[2]).position, Vec3(60, 25, 25), 1e-12);
    expectVecNear(half.value().at(c.frameOcc).position, Vec3::Zero(), 0);
    auto full = explodedPlacements(c.doc, v, 3);
    ASSERT_TRUE(full.ok());
    expectVecNear(full.value().at(c.wheel1).position, Vec3(0, -70, 0), 1e-12);
    expectVecNear(full.value().at(c.wheel2).position, Vec3(90, -70, 0), 1e-12);
    expectVecNear(full.value().at(c.bolts[0]).position, Vec3(10, 25, 40), 1e-12);
    auto none = explodedPlacements(c.doc, v, 0);
    ASSERT_TRUE(none.ok());
    expectVecNear(none.value().at(c.wheel1).position, Vec3(10, -30, 0), 0);

    // Исходные положения документа не изменились.
    expectVecNear(occAt(c.doc, c.wheel1).placement.position, Vec3(10, -30, 0), 0);
    expectVecNear(occAt(c.doc, c.bolts[0]).placement.position, Vec3(10, 25, 10), 0);

    // Формы в разнесённом виде: диски первого колеса сдвинуты на (−10, −40, 0).
    auto placed = c.doc.placedParts(&full.value());
    auto base = c.doc.placedParts();
    ASSERT_TRUE(placed.ok() && base.ok());
    ASSERT_EQ(placed.value().size(), base.value().size());
    for (std::size_t i = 0; i < placed.value().size(); ++i) {
        if (placed.value()[i].path[0] != c.wheel1) continue;
        const auto a = bounds(base.value()[i].shape), b = bounds(placed.value()[i].shape);
        EXPECT_NEAR(b[0] - a[0], -10, 1e-9);
        EXPECT_NEAR(b[1] - a[1], -40, 1e-9);
        EXPECT_NEAR(b[2] - a[2], 0, 1e-9);
    }

    ExplodedView bad;
    bad.steps.push_back({{c.wheel1}, Vec3::Zero(), 5});
    EXPECT_EQ(explodedPlacements(c.doc, bad, 1).error().code, ErrorCode::InvalidInput);
    bad.steps[0] = {{12345}, Vec3::UnitX(), 5};
    EXPECT_EQ(explodedPlacements(c.doc, bad, 1).error().code, ErrorCode::InvalidInput);
}
