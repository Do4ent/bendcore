// Массивы компонентов: плита с 4 отверстиями и болты по массиву отверстий, линейный и
// круговой массивы.

#include "assembly_document_helpers.hpp"

#include <bendcore/assembly_v2/bom.hpp>
#include <bendcore/assembly_v2/interference.hpp>

#include <algorithm>

using namespace bendcore::assembly_v2;
using namespace bendcore::assembly_v2::testing;
using bendcore::kernel::ErrorCode;

namespace {

const double kHoles[4][2] = {{20, 20}, {80, 20}, {20, 60}, {80, 60}};

// Плита 100×80×10 с четырьмя сквозными отверстиями r = 5.5 и одним r = 3 в центре.
TopoDS_Shape plate() {
    TopoDS_Shape s = box(0, 0, 0, 100, 80, 10);
    for (const auto& h : kHoles) s = cut(s, cylinder(h[0], h[1], -1, 5.5, 12));
    return cut(s, cylinder(50, 40, -1, 3, 12));
}

struct PlateWithBolts {
    AssemblyDocument doc{"Plate"};
    TopoDS_Shape plateShape = plate();
    TopoDS_Shape boltShape = bolt(5, 30, 8, 6);
    DefinitionId plateDef = 0, boltDef = 0;
    OccurrenceId plateOcc = 0, seed = 0;

    PlateWithBolts() {
        plateDef = doc.addPart("Plate", {plateShape, nullptr}, {"PL-01", "Плита", "Steel", 7850}).value();
        boltDef = doc.addPart("Bolt", {boltShape, nullptr}, {"BT-M10", "Болт", "Steel", 7850}).value();
        plateOcc = doc.addOccurrence(plateDef, Transform::identity()).value();
        EXPECT_TRUE(doc.setFixed(plateOcc, true).ok());
        seed = doc.addOccurrence(boltDef, Transform::fromAxisAngle(Vec3(0.3, 1, 0.2), 0.5, Vec3(10, 30, 60))).value();
        EXPECT_TRUE(doc.addMate(MateSpec::concentric(faceRef(seed, cylinderFace(boltShape, 5)), hole1())).ok());
        EXPECT_TRUE(doc.addMate(MateSpec::coincident(faceRef(seed, horizontalFace(boltShape, 0, -1)),
                                                     faceRef(plateOcc, horizontalFace(plateShape, 10, 1)),
                                                     Alignment::AntiAligned))
                        .ok());
    }
    GeometryRef hole1() const { return faceRef(plateOcc, cylinderFace(plateShape, 5.5, 20, 20)); }
};

std::vector<Vec3> sortedPositions(const AssemblyDocument& doc, const std::vector<OccurrenceId>& ids) {
    std::vector<Vec3> out;
    for (OccurrenceId id : ids) out.push_back(occAt(doc, id).placement.position);
    std::sort(out.begin(), out.end(), [](const Vec3& a, const Vec3& b) {
        return std::lexicographical_compare(a.data(), a.data() + 3, b.data(), b.data() + 3);
    });
    return out;
}

}  // namespace

TEST(AssemblyDocumentPatterns, BoltsFollowHolePattern) {
    PlateWithBolts m;
    auto solved = m.doc.solve();
    ASSERT_TRUE(solved.ok());
    EXPECT_TRUE(solved.value().success);
    EXPECT_EQ(solved.value().totalDof, 1);  // поворот болта вокруг оси
    expectVecNear(occAt(m.doc, m.seed).placement.position, Vec3(20, 20, 10), 1e-8);

    FeaturePatternSpec spec;
    spec.seed = m.seed;
    spec.seedHole = m.hole1();
    auto pid = m.doc.addFeaturePattern(spec);
    ASSERT_TRUE(pid.ok()) << pid.error().message;
    const ComponentPattern& p = m.doc.patterns().front();
    ASSERT_EQ(p.instances.size(), 3u);  // отверстие r = 3 не подходит по радиусу
    // Порядок — по расстоянию от исходного отверстия: (0, 40), (60, 0), (60, 40).
    expectVecNear(p.transforms[0].position, Vec3(0, 40, 0), 1e-9);
    expectVecNear(p.transforms[1].position, Vec3(60, 0, 0), 1e-9);
    expectVecNear(p.transforms[2].position, Vec3(60, 40, 0), 1e-9);
    for (std::size_t i = 0; i < 3; ++i) {
        const Occurrence& o = occAt(m.doc, p.instances[i]);
        EXPECT_EQ(o.definition, m.boltDef);
        EXPECT_EQ(o.pattern, std::optional<PatternId>(pid.value()));
        // Ось экземпляра — ось отверстия, головка на верхней грани плиты.
        expectVecNear(o.placement.applyToDirection(Vec3::UnitZ()), Vec3::UnitZ(), 1e-9);
    }
    const std::vector<Vec3> got = sortedPositions(m.doc, p.instances);
    const std::vector<Vec3> expected{{20, 60, 10}, {80, 20, 10}, {80, 60, 10}};
    for (std::size_t i = 0; i < 3; ++i) expectVecNear(got[i], expected[i], 1e-8);

    // Сопряжение к экземпляру запрещено — его положение задаёт массив.
    EXPECT_EQ(m.doc.addMate(MateSpec::rigid(p.instances[0], m.plateOcc)).error().code, ErrorCode::InvalidInput);

    // Болты в отверстиях с зазором не пересекают плиту, головки касаются верхней грани.
    auto inter = checkInterference(m.doc);
    ASSERT_TRUE(inter.ok()) << inter.error().message;
    EXPECT_EQ(inter.value().parts.size(), 5u);
    EXPECT_EQ(inter.value().candidatePairs, 4u);  // болты разнесены — их габариты не пересекаются
    ASSERT_EQ(inter.value().pairs.size(), 4u);
    for (const InterferencePair& pr : inter.value().pairs) {
        EXPECT_EQ(pr.kind, ContactKind::Touching);
        EXPECT_LT(pr.volume, 1e-6);
    }

    auto bom = flatBom(m.doc);
    ASSERT_TRUE(bom.ok());
    ASSERT_EQ(bom.value().rows.size(), 2u);
    EXPECT_EQ(bom.value().rows[1].designation, "BT-M10");
    EXPECT_EQ(bom.value().rows[1].quantity, 4);

    // Плита переставлена (поворот 90° вокруг Z и сдвиг): болты следуют за отверстиями.
    const Transform moved = Transform::fromAxisAngle(Vec3::UnitZ(), kPi / 2, Vec3(200, 0, 0));
    ASSERT_TRUE(m.doc.setPlacement(m.plateOcc, moved).ok());
    ASSERT_TRUE(m.doc.solve().value().success);
    std::vector<OccurrenceId> all = p.instances;
    all.push_back(m.seed);
    const std::vector<Vec3> got2 = sortedPositions(m.doc, all);
    std::vector<Vec3> exp2;
    for (const auto& h : kHoles) exp2.push_back(moved.applyToPoint(Vec3(h[0], h[1], 10)));
    std::sort(exp2.begin(), exp2.end(), [](const Vec3& a, const Vec3& b) {
        return std::lexicographical_compare(a.data(), a.data() + 3, b.data(), b.data() + 3);
    });
    for (std::size_t i = 0; i < 4; ++i) expectVecNear(got2[i], exp2[i], 1e-7);

    // Удаление массива убирает экземпляры.
    ASSERT_TRUE(m.doc.removePattern(pid.value()).ok());
    EXPECT_EQ(m.doc.topLevel().size(), 2u);
}

TEST(AssemblyDocumentPatterns, SeedHoleWithWrongRadiusIsRejected) {
    PlateWithBolts m;
    FeaturePatternSpec spec;
    spec.seed = m.seed;
    spec.seedHole = m.hole1();
    spec.radius = 3.0;
    EXPECT_EQ(m.doc.addFeaturePattern(spec).error().code, ErrorCode::InvalidInput);
    spec.radius.reset();
    spec.seedHole = faceRef(m.plateOcc, horizontalFace(m.plateShape, 10, 1));
    EXPECT_FALSE(m.doc.addFeaturePattern(spec).ok());
    EXPECT_TRUE(m.doc.patterns().empty());
}

TEST(AssemblyDocumentPatterns, LinearTwoDirectionsAndReferenceFrame) {
    AssemblyDocument doc;
    const DefinitionId cube = doc.addPart("Cube", {box(0, 0, 0, 5, 5, 5), nullptr}).value();
    const OccurrenceId seed = doc.addOccurrence(cube, Transform::fromTranslation(Vec3(1, 2, 3))).value();
    LinearPatternSpec l;
    l.seed = seed;
    l.direction = Vec3(2, 0, 0);  // нормируется
    l.spacing = 20;
    l.count = 3;
    l.direction2 = Vec3::UnitY();
    l.spacing2 = 15;
    l.count2 = 2;
    auto id = doc.addLinearPattern(l);
    ASSERT_TRUE(id.ok());
    const ComponentPattern& p = doc.patterns().front();
    ASSERT_EQ(p.instances.size(), 5u);
    const Vec3 expected[] = {{21, 2, 3}, {41, 2, 3}, {1, 17, 3}, {21, 17, 3}, {41, 17, 3}};
    for (std::size_t i = 0; i < 5; ++i) expectVecNear(occAt(doc, p.instances[i]).placement.position, expected[i], 1e-12);

    // Направление в СК опорного вхождения, повёрнутого на 90° вокруг Z: X опоры — мировой Y.
    const OccurrenceId ref = doc.addOccurrence(cube, Transform::fromAxisAngle(Vec3::UnitZ(), kPi / 2, Vec3(7, 7, 0))).value();
    LinearPatternSpec r;
    r.seed = seed;
    r.reference = ref;
    r.spacing = 10;
    r.count = 2;
    auto id2 = doc.addLinearPattern(r);
    ASSERT_TRUE(id2.ok());
    expectVecNear(occAt(doc, doc.patterns()[1].instances[0]).placement.position, Vec3(1, 12, 3), 1e-12);

    // Перемещение исходного компонента — экземпляры пересчитываются.
    ASSERT_TRUE(doc.setPlacement(seed, Transform::fromTranslation(Vec3(0, 0, 0))).ok());
    ASSERT_TRUE(doc.updatePatterns().ok());
    expectVecNear(occAt(doc, doc.patterns()[0].instances[4]).placement.position, Vec3(40, 15, 0), 1e-12);

    LinearPatternSpec bad = l;
    bad.direction = Vec3::Zero();
    EXPECT_EQ(doc.addLinearPattern(bad).error().code, ErrorCode::InvalidInput);
    bad = l;
    bad.count = 1;
    bad.count2 = 1;
    EXPECT_EQ(doc.addLinearPattern(bad).error().code, ErrorCode::InvalidInput);
    // Удаление исходного компонента удаляет его массивы вместе с экземплярами.
    ASSERT_TRUE(doc.removeOccurrence(seed).ok());
    EXPECT_TRUE(doc.patterns().empty());
    EXPECT_EQ(doc.topLevel().size(), 1u);
}

TEST(AssemblyDocumentPatterns, CircularPattern) {
    AssemblyDocument doc;
    const DefinitionId cube = doc.addPart("Cube", {box(-1, -1, -1, 2, 2, 2), nullptr}).value();
    const OccurrenceId seed = doc.addOccurrence(cube, Transform::fromTranslation(Vec3(50, 0, 0))).value();
    CircularPatternSpec c;
    c.seed = seed;
    c.axisDirection = Vec3::UnitZ();
    c.angleStep = kPi / 3;
    c.count = 6;
    ASSERT_TRUE(doc.addCircularPattern(c).ok());
    const ComponentPattern& p = doc.patterns().front();
    ASSERT_EQ(p.instances.size(), 5u);
    for (int k = 1; k < 6; ++k) {
        const Transform& t = occAt(doc, p.instances[k - 1]).placement;
        expectVecNear(t.position, Vec3(50 * std::cos(k * kPi / 3), 50 * std::sin(k * kPi / 3), 0), 1e-9);
        expectVecNear(t.applyToDirection(Vec3::UnitX()), Vec3(std::cos(k * kPi / 3), std::sin(k * kPi / 3), 0), 1e-12);
    }
    c.angleStep = 0;
    EXPECT_EQ(doc.addCircularPattern(c).error().code, ErrorCode::InvalidInput);
}
