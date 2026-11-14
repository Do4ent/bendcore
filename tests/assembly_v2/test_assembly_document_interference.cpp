// Пересечения: известный объём наложения коробок, касание без пересечения, вложенное тело,
// отбор по габаритам, детали внутри подсборки.

#include "assembly_document_helpers.hpp"

#include <bendcore/assembly_v2/interference.hpp>
#include <bendcore/kernel/boolean.hpp>

#include <BRepExtrema_DistShapeShape.hxx>
#include <TopExp_Explorer.hxx>

#include <set>
#include <string>
#include <tuple>
#include <vector>

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

// Локальный алгоритм (сетка кусков крупной детали, отбор по габаритам граней) против прямого
// расчёта по целым деталям — алгоритма до фазы 5: Common всей пары и BRepExtrema по целым формам.
// Плита 100 × 80 × 10 с 30 отверстиями r = 3 (36 граней — крупная деталь, разрезается сеткой
// 3 × 3; плоскость сетки y = 80/3 проходит через отверстия ряда y = 24). Штифты: точно по отверстию
// (касание), с зазором r = 2,5 (габариты пересекаются, пары нет), с натягом r = 3,5 (пересечение
// π·(3,5² − 3²)·10), куб внутри тела плиты (вложен, граней рядом нет), куб на верхней грани
// (касание), куб над плитой с зазором 0,2 (не кандидат).
TEST(AssemblyDocumentInterference, LocalAlgorithmMatchesWholeShapeReference) {
    TopoDS_Shape plate = box(0, 0, 0, 100, 80, 10);
    std::vector<std::pair<double, double>> holes;
    for (int j = 0; j < 5; ++j)
        for (int i = 0; i < 6; ++i) holes.emplace_back(10 + 16 * i, 8 + 16 * j);
    for (const auto& [x, y] : holes) plate = cut(plate, cylinder(x, y, -1, 3, 12));
    int plateFaces = 0;
    for (TopExp_Explorer f(plate, TopAbs_FACE); f.More(); f.Next()) ++plateFaces;
    ASSERT_GT(plateFaces, 32);

    AssemblyDocument doc;
    const DefinitionId plateDef = doc.addPart("Plate", {plate, nullptr}).value();
    const DefinitionId fit = doc.addPart("Fit", {cylinder(0, 0, -5, 3, 20), nullptr}).value();
    const DefinitionId loose = doc.addPart("Loose", {cylinder(0, 0, -5, 2.5, 20), nullptr}).value();
    const DefinitionId tight = doc.addPart("Tight", {cylinder(0, 0, -5, 3.5, 20), nullptr}).value();
    const DefinitionId cube = doc.addPart("Cube", {box(0, 0, 0, 2, 2, 2), nullptr}).value();
    ASSERT_TRUE(doc.addOccurrence(plateDef, Transform::identity(), "Plate").ok());
    // Отверстия 6–11 (ряд y = 24) пересекает плоскость сетки: штифт попадает в два куска.
    const std::vector<std::tuple<DefinitionId, int, std::string>> pins = {
        {fit, 8, "Fit8"}, {fit, 0, "Fit0"}, {fit, 29, "Fit29"}, {loose, 9, "Loose9"},
        {loose, 3, "Loose3"}, {tight, 16, "Tight16"}, {tight, 7, "Tight7"}};
    for (const auto& [def, h, name] : pins)
        ASSERT_TRUE(doc.addOccurrence(def, Transform::fromTranslation(Vec3(holes[h].first, holes[h].second, 0)), name).ok());
    ASSERT_TRUE(doc.addOccurrence(cube, Transform::fromTranslation(Vec3(16, 14, 4)), "Inside").ok());
    ASSERT_TRUE(doc.addOccurrence(cube, Transform::fromTranslation(Vec3(48, 30, 10)), "OnTop").ok());
    ASSERT_TRUE(doc.addOccurrence(cube, Transform::fromTranslation(Vec3(80, 60, 10.2)), "Above").ok());

    auto r = checkInterference(doc);
    ASSERT_TRUE(r.ok()) << r.error().message;
    const InterferenceReport& rep = r.value();

    // Эталон: прямой расчёт по целым формам для всех пар.
    InterferenceOptions o;
    std::set<std::pair<std::string, std::string>> expected;
    for (std::size_t i = 0; i < rep.parts.size(); ++i)
        for (std::size_t j = i + 1; j < rep.parts.size(); ++j) {
            const TopoDS_Shape& a = rep.parts[i].shape;
            const TopoDS_Shape& b = rep.parts[j].shape;
            auto common = bendcore::kernel::booleanOp(bendcore::kernel::BooleanType::Common, a, b, o.boolean);
            ASSERT_TRUE(common.ok()) << common.error().message;
            const double v = common.value().volume;
            BRepExtrema_DistShapeShape dist(a, b);
            ASSERT_TRUE(dist.IsDone());
            const std::string na = rep.parts[i].pathName, nb = rep.parts[j].pathName;
            if (v <= o.volumeTolerance && dist.Value() > o.contactTolerance) {
                EXPECT_EQ(findPair(rep, na, nb), nullptr) << na << " - " << nb;
                continue;
            }
            expected.emplace(na, nb);
            const InterferencePair* p = findPair(rep, na, nb);
            ASSERT_NE(p, nullptr) << na << " - " << nb;
            if (v > o.volumeTolerance) {
                EXPECT_EQ(p->kind, ContactKind::Interference) << na << " - " << nb;
                EXPECT_NEAR(p->volume, v, 1e-9 * v + 1e-9) << na << " - " << nb;
            } else {
                EXPECT_EQ(p->kind, ContactKind::Touching) << na << " - " << nb;
                EXPECT_NEAR(p->distance, dist.Value(), 1e-9) << na << " - " << nb;
            }
        }
    EXPECT_EQ(rep.pairs.size(), expected.size());

    // Аналитика: 3 касания штифтов, касание куба сверху, 2 натяга, вложенный куб.
    const double ring = kPi * (3.5 * 3.5 - 3.0 * 3.0) * 10.0;
    for (const char* t : {"Tight16", "Tight7"}) {
        const InterferencePair* p = findPair(rep, "Plate", t);
        ASSERT_NE(p, nullptr) << t;
        EXPECT_EQ(p->kind, ContactKind::Interference);
        EXPECT_NEAR(p->volume, ring, 1e-6 * ring) << t;
    }
    for (const char* t : {"Fit8", "Fit0", "Fit29", "OnTop"}) {
        const InterferencePair* p = findPair(rep, "Plate", t);
        ASSERT_NE(p, nullptr) << t;
        EXPECT_EQ(p->kind, ContactKind::Touching) << t;
    }
    for (const char* t : {"Loose9", "Loose3", "Above"}) EXPECT_EQ(findPair(rep, "Plate", t), nullptr) << t;
    const InterferencePair* in = findPair(rep, "Plate", "Inside");
    ASSERT_NE(in, nullptr);
    EXPECT_EQ(in->kind, ContactKind::Interference);
    EXPECT_NEAR(in->volume, 8.0, 1e-9);
    EXPECT_NEAR(rep.totalVolume, 2 * ring + 8.0, 1e-6);
}
