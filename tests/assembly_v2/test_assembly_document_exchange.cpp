// Круговой проход документа сборки через STEP (bendcore_kernel_xcaf): структура, имена,
// цвета, положения; преобразование положений.

#include "assembly_document_helpers.hpp"

#include <bendcore/kernel/step_io.hpp>

#include <gp_Ax1.hxx>
#include <gp_Trsf.hxx>

using namespace bendcore::assembly_v2;
using namespace bendcore::assembly_v2::testing;
using bendcore::kernel::AssemblyColor;
using bendcore::kernel::ErrorCode;

namespace {

AssemblyDocument sample() {
    AssemblyDocument doc("Gearbox");
    const DefinitionId base = doc.addPart("Base", {box(0, 0, 0, 120, 80, 15), nullptr}).value();
    const DefinitionId pin = doc.addPart("Pin", {cylinder(0, 0, 0, 4, 30), nullptr}).value();
    const DefinitionId washer = doc.addPart("Washer", {bushing(8, 4.2, 2), nullptr}).value();
    EXPECT_TRUE(doc.setDefinitionColor(base, AssemblyColor{1, 0, 0, 1}).ok());
    const DefinitionId unit = doc.addSubassembly("PinUnit").value();
    EXPECT_TRUE(doc.addOccurrence(pin, Transform::fromTranslation(Vec3(0, 0, -2)), "PIN", unit).ok());
    EXPECT_TRUE(doc.addOccurrence(washer, Transform::identity(), "WASHER", unit).ok());
    EXPECT_TRUE(doc.addOccurrence(base, Transform::identity(), "BASE").ok());
    const OccurrenceId u1 =
        doc.addOccurrence(unit, Transform::fromTranslation(Vec3(20, 20, 15)), "UNIT_1").value();
    EXPECT_TRUE(doc.addOccurrence(unit, Transform::fromAxisAngle(Vec3(0, 0, 1), kPi / 2, Vec3(100, 60, 15)), "UNIT_2").ok());
    EXPECT_TRUE(doc.setOccurrenceColor(u1, AssemblyColor{0, 0, 1, 1}).ok());
    // Неиспользуемое определение в STEP не попадает.
    EXPECT_TRUE(doc.addPart("Unused", {box(0, 0, 0, 1, 1, 1), nullptr}).ok());
    return doc;
}

}  // namespace

TEST(AssemblyDocumentExchange, LocationConversion) {
    const Transform t = Transform::fromAxisAngle(Vec3(1, 2, 3), 1.1, Vec3(4, -5, 6));
    auto back = fromLocation(toLocation(t));
    ASSERT_TRUE(back.ok());
    expectVecNear(back.value().position, t.position, 1e-12);
    const Vec3 p(7, 8, 9);
    expectVecNear(back.value().applyToPoint(p), t.applyToPoint(p), 1e-12);
    gp_Trsf scaled;
    scaled.SetScale(gp::Origin(), 2.0);
    EXPECT_EQ(fromLocation(TopLoc_Location(scaled)).error().code, ErrorCode::Unsupported);
}

TEST(AssemblyDocumentExchange, StepRoundTripKeepsStructureAndPlacements) {
    const AssemblyDocument doc = sample();
    for (auto schema : {bendcore::kernel::StepSchema::AP214, bendcore::kernel::StepSchema::AP242}) {
        auto step = doc.exportStep({schema, bendcore::kernel::StepLengthUnit::Millimetre});
        ASSERT_TRUE(step.ok()) << step.error().message;
        auto back = AssemblyDocument::importStep(step.value());
        ASSERT_TRUE(back.ok()) << back.error().message;
        const AssemblyDocument& d = back.value();
        EXPECT_EQ(d.name(), "Gearbox");
        ASSERT_EQ(d.topLevel().size(), 3u);
        EXPECT_EQ(d.topLevel()[0].name, "BASE");
        EXPECT_EQ(d.topLevel()[1].name, "UNIT_1");
        EXPECT_EQ(d.topLevel()[2].name, "UNIT_2");
        // Подсборка — одно общее определение с двумя вхождениями.
        EXPECT_EQ(d.topLevel()[1].definition, d.topLevel()[2].definition);
        const Definition& unit = d.definition(d.topLevel()[1].definition);
        EXPECT_EQ(unit.kind, DefinitionKind::Assembly);
        EXPECT_EQ(unit.name, "PinUnit");
        ASSERT_EQ(unit.occurrences.size(), 2u);
        EXPECT_EQ(unit.occurrences[0].name, "PIN");
        EXPECT_EQ(d.definition(unit.occurrences[1].definition).name, "Washer");
        EXPECT_EQ(d.definitions().size(), 5u);  // корень, Base, PinUnit, Pin, Washer
        // Цвета определения и вхождения.
        const Definition& base = d.definition(d.topLevel()[0].definition);
        ASSERT_TRUE(base.color.has_value());
        EXPECT_NEAR(base.color->r, 1, 1e-6);
        EXPECT_NEAR(base.color->g, 0, 1e-6);
        ASSERT_TRUE(d.topLevel()[1].color.has_value());
        EXPECT_NEAR(d.topLevel()[1].color->b, 1, 1e-6);
        EXPECT_FALSE(d.topLevel()[2].color.has_value());
        // Положения вхождений по всем уровням.
        for (std::size_t i = 0; i < 3; ++i) {
            const Transform& a = doc.topLevel()[i].placement;
            const Transform& b = d.topLevel()[i].placement;
            expectVecNear(b.position, a.position, 1e-9);
            EXPECT_NEAR(b.rotation.angularDistance(a.rotation), 0, 1e-9);
        }
        expectVecNear(unit.occurrences[0].placement.position, Vec3(0, 0, -2), 1e-9);
        // Развёртка: те же детали в тех же местах с теми же объёмами.
        auto pa = doc.placedParts();
        auto pb = d.placedParts();
        ASSERT_TRUE(pa.ok() && pb.ok());
        ASSERT_EQ(pa.value().size(), 5u);
        ASSERT_EQ(pb.value().size(), 5u);
        for (std::size_t i = 0; i < 5; ++i) {
            EXPECT_EQ(pa.value()[i].pathName, pb.value()[i].pathName);
            EXPECT_NEAR(volumeOf(pb.value()[i].shape), volumeOf(pa.value()[i].shape), 1e-6);
            const auto ba = bounds(pa.value()[i].shape), bb = bounds(pb.value()[i].shape);
            for (int k = 0; k < 6; ++k) EXPECT_NEAR(bb[k], ba[k], 1e-6) << pa.value()[i].pathName;
        }
        // Штифт второго узла повёрнут на 90°: габарит по построению.
        EXPECT_EQ(pb.value()[4].pathName, "UNIT_2/WASHER");
        const auto pin2 = bounds(pb.value()[3].shape);
        EXPECT_NEAR(pin2[0], 96, 1e-6);
        EXPECT_NEAR(pin2[3], 104, 1e-6);
        EXPECT_NEAR(pin2[2], 13, 1e-6);
        EXPECT_NEAR(pin2[5], 43, 1e-6);
    }
}

TEST(AssemblyDocumentExchange, SinglePartStepBecomesOneOccurrence) {
    auto step = bendcore::kernel::writeStep(bendcore::kernel::Shape(cylinder(0, 0, 0, 5, 10)));
    ASSERT_TRUE(step.ok());
    auto doc = AssemblyDocument::importStep(step.value());
    ASSERT_TRUE(doc.ok()) << doc.error().message;
    ASSERT_EQ(doc.value().topLevel().size(), 1u);
    const Definition& d = doc.value().definition(doc.value().topLevel()[0].definition);
    EXPECT_EQ(d.kind, DefinitionKind::Part);
    EXPECT_NEAR(volumeOf(d.shape), kPi * 25 * 10, 1e-6);
    EXPECT_EQ(AssemblyDocument::importStep("garbage").error().code, ErrorCode::ParseError);
    AssemblyDocument empty;
    EXPECT_FALSE(empty.exportStep().ok());
}
