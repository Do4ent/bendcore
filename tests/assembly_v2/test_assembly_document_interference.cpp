// Пересечения: известный объём наложения коробок, касание без пересечения, вложенное тело,
// отбор по габаритам, детали внутри подсборки.

#include "assembly_document_helpers.hpp"

#include <bendcore/assembly_v2/interference.hpp>

using namespace bendcore::assembly_v2;
using namespace bendcore::assembly_v2::testing;
using bendcore::kernel::ErrorCode;

namespace {

const InterferencePair* findPair(const InterferenceReport& r, const std::string& a, const std::string& b) {
    for (const InterferencePair& p : r.pairs) {
        const std::string& na = r.parts[p.a].pathName;
        const std::string& nb = r.parts[p.b].pathName;
        if ((na == a && nb == b) || (na == b && nb == a)) return &p;
    }
    return nullptr;
}

}  // namespace

TEST(AssemblyDocumentInterference, OverlapVolumeAndTouching) {
    AssemblyDocument doc;
    const DefinitionId cube20 = doc.addPart("Cube20", {box(0, 0, 0, 20, 20, 20), nullptr}).value();
    const DefinitionId cube10 = doc.addPart("Cube10", {box(0, 0, 0, 10, 10, 10), nullptr}).value();
    // A: [0,20]³; B: [10,30]³ — наложение 10³; C: [20,30]×[0,10]×[−5,5] — касается A по грани x = 20,
    // от B отстоит на 5 по z; D далеко.
    ASSERT_TRUE(doc.addOccurrence(cube20, Transform::identity(), "A").ok());
    ASSERT_TRUE(doc.addOccurrence(cube20, Transform::fromTranslation(Vec3(10, 10, 10)), "B").ok());
    ASSERT_TRUE(doc.addOccurrence(cube10, Transform::fromTranslation(Vec3(20, 0, -5)), "C").ok());
    ASSERT_TRUE(doc.addOccurrence(cube10, Transform::fromTranslation(Vec3(200, 200, 200)), "D").ok());

    auto r = checkInterference(doc);
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_EQ(r.value().totalPairs, 6u);
    EXPECT_EQ(r.value().candidatePairs, 2u);  // A–B и A–C
    ASSERT_EQ(r.value().pairs.size(), 2u);
    const InterferencePair* ab = findPair(r.value(), "A", "B");
    ASSERT_NE(ab, nullptr);
    EXPECT_EQ(ab->kind, ContactKind::Interference);
    EXPECT_NEAR(ab->volume, 1000.0, 1e-6);
    const InterferencePair* ac = findPair(r.value(), "A", "C");
    ASSERT_NE(ac, nullptr);
    EXPECT_EQ(ac->kind, ContactKind::Touching);
    EXPECT_NEAR(ac->volume, 0.0, 1e-6);
    EXPECT_NEAR(ac->distance, 0.0, 1e-6);
    EXPECT_NEAR(r.value().totalVolume, 1000.0, 1e-6);

    // Без касаний в отчёте — только пересечение.
    InterferenceOptions o;
    o.reportTouching = false;
    auto r2 = checkInterference(doc, o);
    ASSERT_TRUE(r2.ok());
    ASSERT_EQ(r2.value().pairs.size(), 1u);
    EXPECT_EQ(r2.value().pairs[0].kind, ContactKind::Interference);

    // Сдвиг C на 0,5 мм от A: касания нет.
    const OccurrenceId c = doc.topLevel()[2].id;
    ASSERT_TRUE(doc.setPlacement(c, Transform::fromTranslation(Vec3(20.5, 0, -5))).ok());
    auto r3 = checkInterference(doc);
    ASSERT_TRUE(r3.ok());
    EXPECT_EQ(findPair(r3.value(), "A", "C"), nullptr);

    o.volumeTolerance = -1;
    EXPECT_EQ(checkInterference(doc, o).error().code, ErrorCode::InvalidInput);
}

TEST(AssemblyDocumentInterference, RotatedOverlapAndNestedSolid) {
    AssemblyDocument doc;
    const DefinitionId plateDef = doc.addPart("Slab", {box(-10, -10, 0, 20, 20, 4), nullptr}).value();
    const DefinitionId pinDef = doc.addPart("Pin", {cylinder(0, 0, 0, 2, 10), nullptr}).value();
    // Плита, повёрнутая на 30° вокруг Z, и штифт, пронизывающий её: объём наложения π·r²·4.
    ASSERT_TRUE(doc.addOccurrence(plateDef, Transform::fromAxisAngle(Vec3::UnitZ(), kPi / 6), "Slab").ok());
    ASSERT_TRUE(doc.addOccurrence(pinDef, Transform::fromTranslation(Vec3(3, -1, -3)), "Pin").ok());
    // Маленький куб целиком внутри плиты: объём наложения — его объём.
    const DefinitionId small = doc.addPart("Small", {box(0, 0, 0, 1, 1, 1), nullptr}).value();
    ASSERT_TRUE(doc.addOccurrence(small, Transform::fromTranslation(Vec3(-6, 5, 1.5)), "Small").ok());
    auto r = checkInterference(doc);
    ASSERT_TRUE(r.ok()) << r.error().message;
    const InterferencePair* pin = findPair(r.value(), "Slab:1", "Pin:1");
    ASSERT_EQ(pin, nullptr);  // имена заданы явно
    const InterferencePair* sp = findPair(r.value(), "Slab", "Pin");
    ASSERT_NE(sp, nullptr);
    EXPECT_EQ(sp->kind, ContactKind::Interference);
    EXPECT_NEAR(sp->volume, kPi * 4 * 4, 1e-6);
    const InterferencePair* ss = findPair(r.value(), "Slab", "Small");
    ASSERT_NE(ss, nullptr);
    EXPECT_EQ(ss->kind, ContactKind::Interference);
    EXPECT_NEAR(ss->volume, 1.0, 1e-9);
}

TEST(AssemblyDocumentInterference, PartsInsideSubassembly) {
    AssemblyDocument doc;
    const DefinitionId cube = doc.addPart("Cube", {box(0, 0, 0, 10, 10, 10), nullptr}).value();
    const DefinitionId sub = doc.addSubassembly("Pair").value();
    ASSERT_TRUE(doc.addOccurrence(cube, Transform::identity(), "L", sub).ok());
    ASSERT_TRUE(doc.addOccurrence(cube, Transform::fromTranslation(Vec3(5, 0, 0)), "R", sub).ok());
    ASSERT_TRUE(doc.addOccurrence(sub, Transform::fromTranslation(Vec3(0, 0, 100)), "Sub").ok());
    auto r = checkInterference(doc);
    ASSERT_TRUE(r.ok());
    ASSERT_EQ(r.value().pairs.size(), 1u);
    EXPECT_NEAR(r.value().pairs[0].volume, 500.0, 1e-6);
    EXPECT_EQ(findPair(r.value(), "Sub/L", "Sub/R"), &r.value().pairs[0]);
    InterferenceOptions o;
    o.includeSameTopLevel = false;
    auto r2 = checkInterference(doc, o);
    ASSERT_TRUE(r2.ok());
    EXPECT_TRUE(r2.value().pairs.empty());
    EXPECT_EQ(r2.value().totalPairs, 0u);
}
