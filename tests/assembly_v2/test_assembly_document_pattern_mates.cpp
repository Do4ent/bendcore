// Экземпляры массивов компонентов в сопряжениях и массив «по массиву детали»
// (docs/assembly/ASSEMBLY_DOCUMENT.md, раздел 5): сопряжение к грани экземпляра, ссылка по
// ключу при изменении числа экземпляров, висячая ссылка и её восстановление, повторные проходы
// решения при движении исходного, файл; экземпляры по линейному и круговому массиву операции
// детали — против формул положения P = R·(p₀ + i·s₁·d₁ + j·s₂·d₂) + t.

#include "assembly_document_helpers.hpp"
#include "../modeling/modeling_document_test_util.hpp"

#include <bendcore/modeling/part_features.hpp>

#include <cmath>

using namespace bendcore::assembly_v2;
using namespace bendcore::assembly_v2::testing;
using bendcore::kernel::ErrorCode;
namespace m = bendcore::modeling;
namespace tu = bendcore::modeling::testutil;

namespace {

constexpr double kPiP = 3.14159265358979323846;

// Плита 200×50×10 (неподвижна), стойка r = 4, h = 20 жёстко связана с плитой в (20, 25, 10),
// линейный массив стоек по X с шагом 40; колпачок (втулка R = 6, r = 4, h = 5) — на экземпляре.
struct PostRow {
    AssemblyDocument doc{"Row"};
    TopoDS_Shape plateShape = box(0, 0, 0, 200, 50, 10);
    TopoDS_Shape postShape = cylinder(0, 0, 0, 4, 20);
    TopoDS_Shape capShape = bushing(6, 4, 5);
    OccurrenceId plate = 0, post = 0, cap = 0;
    PatternId pattern = 0;
    LinearPatternSpec spec;

    PostRow() {
        const DefinitionId pd = doc.addPart("Plate", {plateShape, nullptr}).value();
        const DefinitionId sd = doc.addPart("Post", {postShape, nullptr}).value();
        const DefinitionId cd = doc.addPart("Cap", {capShape, nullptr}).value();
        plate = doc.addOccurrence(pd, Transform::identity()).value();
        EXPECT_TRUE(doc.setFixed(plate, true).ok());
        post = doc.addOccurrence(sd, Transform::fromTranslation({20, 25, 10})).value();
        EXPECT_TRUE(doc.addMate(MateSpec::rigid(plate, post)).ok());
        cap = doc.addOccurrence(cd, Transform::fromTranslation({0, 0, 50})).value();
        spec.seed = post;
        spec.direction = Vec3::UnitX();
        spec.spacing = 40;
        spec.count = 3;
        pattern = doc.addLinearPattern(spec).value();
    }
    OccurrenceId instance(const std::string& key) const { return doc.patternInstance(pattern, key).value_or(0); }
    // Колпачок на стойке-экземпляре: соосность отверстия и стойки, торец на торец.
    void capOn(OccurrenceId inst) {
        auto a = doc.addMate(MateSpec::concentric(faceRef(cap, cylinderFace(capShape, 4)), faceRef(inst, cylinderFace(postShape, 4))));
        ASSERT_TRUE(a.ok()) << a.error().message;
        auto b = doc.addMate(MateSpec::coincident(faceRef(cap, horizontalFace(capShape, 0, -1)),
                                                  faceRef(inst, horizontalFace(postShape, 20, 1)), Alignment::AntiAligned));
        ASSERT_TRUE(b.ok()) << b.error().message;
    }
};

const DocumentMate& mateAt(const AssemblyDocument& doc, std::size_t i) { return doc.mates().at(i); }

}  // namespace

TEST(PatternInstanceMates, MateToInstanceFaceAndStableKeys) {
    PostRow r;
    const OccurrenceId i20 = r.instance("2,0");
    ASSERT_NE(i20, 0u);
    r.capOn(i20);
    // Ссылка запомнила массив и ключ экземпляра.
    ASSERT_TRUE(mateAt(r.doc, 1).spec.b.instance);
    EXPECT_EQ(mateAt(r.doc, 1).spec.b.instance->key, "2,0");
    EXPECT_EQ(mateAt(r.doc, 1).spec.b.instance->pattern, r.pattern);

    auto s = r.doc.solve();
    ASSERT_TRUE(s.ok()) << s.error().message;
    EXPECT_TRUE(s.value().success);
    EXPECT_EQ(s.value().totalDof, 1);  // поворот колпачка вокруг оси стойки
    // Экземпляр (2, 0): x = 20 + 2·40; верх стойки z = 10 + 20.
    expectVecNear(occAt(r.doc, i20).placement.position, Vec3(100, 25, 10), 1e-12);
    expectVecNear(occAt(r.doc, r.cap).placement.position, Vec3(100, 25, 30), 1e-7);

    // Больше экземпляров: прежние ключи — прежние вхождения, сопряжение цело.
    LinearPatternSpec more = r.spec;
    more.count = 5;
    ASSERT_TRUE(r.doc.editPattern(r.pattern, more).ok());
    EXPECT_EQ(r.doc.patterns().front().instances.size(), 4u);
    EXPECT_EQ(r.instance("2,0"), i20);
    EXPECT_FALSE(mateAt(r.doc, 1).broken());
    expectVecNear(occAt(r.doc, r.instance("4,0")).placement.position, Vec3(180, 25, 10), 1e-12);

    // Экземпляр (2, 0) исчез — висячая ссылка, сопряжение не решается.
    LinearPatternSpec fewer = r.spec;
    fewer.count = 2;
    ASSERT_TRUE(r.doc.editPattern(r.pattern, fewer).ok());
    EXPECT_EQ(r.instance("2,0"), 0u);
    ASSERT_TRUE(mateAt(r.doc, 1).broken());
    EXPECT_NE(mateAt(r.doc, 1).brokenReason.find("dangling pattern instance: pattern 1 has no instance '2,0'"),
              std::string::npos)
        << mateAt(r.doc, 1).brokenReason;
    auto s2 = r.doc.solve();
    ASSERT_TRUE(s2.ok());
    EXPECT_EQ(s2.value().brokenMates.size(), 2u);

    // Экземпляр вернулся (новое вхождение с тем же ключом) — ссылка привязывается снова.
    ASSERT_TRUE(r.doc.editPattern(r.pattern, r.spec).ok());
    const OccurrenceId back = r.instance("2,0");
    ASSERT_NE(back, 0u);
    EXPECT_FALSE(mateAt(r.doc, 1).broken()) << mateAt(r.doc, 1).brokenReason;
    EXPECT_EQ(mateAt(r.doc, 1).spec.b.path.front(), back);
    ASSERT_TRUE(r.doc.solve().ok());
    expectVecNear(occAt(r.doc, r.cap).placement.position, Vec3(100, 25, 30), 1e-7);

    // Ошибки: экземпляр нельзя двигать и перетаскивать; ключа нет — addMate отказывает.
    EXPECT_FALSE(r.doc.setPlacement(back, Transform::identity()).ok());
    EXPECT_EQ(r.doc.drag(back, Vec3::Zero(), Vec3(0, 0, 100)).error().code, ErrorCode::InvalidInput);
    GeometryRef missing = faceRef(0, cylinderFace(r.postShape, 4));
    missing.path.clear();
    missing.instance = PatternInstanceRef{r.pattern, "7,0"};
    auto bad = r.doc.addMate(MateSpec::concentric(faceRef(r.cap, cylinderFace(r.capShape, 4)), missing));
    ASSERT_FALSE(bad.ok());
    EXPECT_NE(bad.error().message.find("has no instance '7,0'"), std::string::npos);
}

TEST(PatternInstanceMates, SeedMovesAndSolveRepeats) {
    PostRow r;
    r.capOn(r.instance("1,0"));
    ASSERT_TRUE(r.doc.solve().ok());
    expectVecNear(occAt(r.doc, r.cap).placement.position, Vec3(60, 25, 30), 1e-7);
    // Плита поднята на 7 мм: стойка следует (жёсткое сопряжение), экземпляр — за ней после
    // первого прохода, колпачок — за экземпляром во втором.
    ASSERT_TRUE(r.doc.setPlacement(r.plate, Transform::fromTranslation({0, 0, 7})).ok());
    auto s = r.doc.solve();
    ASSERT_TRUE(s.ok());
    EXPECT_EQ(s.value().passes, 2);
    EXPECT_TRUE(s.value().patternsConverged);
    EXPECT_TRUE(s.value().success);
    expectVecNear(occAt(r.doc, r.instance("1,0")).placement.position, Vec3(60, 25, 17), 1e-7);
    expectVecNear(occAt(r.doc, r.cap).placement.position, Vec3(60, 25, 37), 1e-7);

    // Файл: ключи экземпляров и ссылка через экземпляр сохраняются.
    auto text = r.doc.toJson();
    ASSERT_TRUE(text.ok());
    auto loaded = AssemblyDocument::fromJson(text.value(), LoadOptions{});
    ASSERT_TRUE(loaded.ok()) << loaded.error().message;
    AssemblyDocument& d = loaded.value().document;
    ASSERT_EQ(d.patterns().size(), 1u);
    EXPECT_EQ(d.patterns().front().keys, (std::vector<std::string>{"1,0", "2,0"}));
    ASSERT_TRUE(d.mates().at(1).spec.b.instance);
    EXPECT_EQ(d.mates().at(1).spec.b.instance->key, "1,0");
    EXPECT_FALSE(d.mates().at(1).broken());
    auto s2 = d.solve();
    ASSERT_TRUE(s2.ok());
    EXPECT_EQ(s2.value().passes, 1);  // ничего не двигается
    expectVecNear(occAt(d, r.cap).placement.position, Vec3(60, 25, 37), 1e-7);
}

TEST(PatternInstanceMates, HolePatternKeysFollowTheHoles) {
    // Плита с отверстиями r = 5,5 в (20, 20), (80, 20), (20, 60); штифт в (20, 20) и массив по
    // отверстиям; шайба на экземпляре в (80, 20).
    auto plateWith = [](std::vector<std::pair<double, double>> holes) {
        TopoDS_Shape s = box(0, 0, 0, 100, 80, 10);
        for (auto [x, y] : holes) s = cut(s, cylinder(x, y, -1, 5.5, 12));
        return s;
    };
    AssemblyDocument doc;
    TopoDS_Shape plate = plateWith({{20, 20}, {80, 20}, {20, 60}});
    const TopoDS_Shape pin = cylinder(0, 0, -10, 5.5, 30);
    const TopoDS_Shape washer = bushing(9, 5.5, 2);
    const DefinitionId pd = doc.addPart("Plate", {plate, nullptr}).value();
    const OccurrenceId po = doc.addOccurrence(pd, Transform::identity()).value();
    ASSERT_TRUE(doc.setFixed(po, true).ok());
    const OccurrenceId seed = doc.addOccurrence(doc.addPart("Pin", {pin, nullptr}).value(), Transform::fromTranslation({20, 20, 0})).value();
    ASSERT_TRUE(doc.addMate(MateSpec::rigid(po, seed)).ok());
    FeaturePatternSpec f;
    f.seed = seed;
    f.seedHole = faceRef(po, cylinderFace(plate, 5.5, 20, 20));
    const PatternId pid = doc.addFeaturePattern(f).value();
    const std::string key80 = "@60.000000,0.000000,0.000000";  // смещение оси (80, 20) от (20, 20)
    const OccurrenceId at80 = doc.patternInstance(pid, key80).value_or(0);
    ASSERT_NE(at80, 0u);
    const OccurrenceId w = doc.addOccurrence(doc.addPart("Washer", {washer, nullptr}).value(), Transform::fromTranslation({0, 0, 40})).value();
    ASSERT_TRUE(doc.addMate(MateSpec::concentric(faceRef(w, cylinderFace(washer, 5.5)), faceRef(at80, cylinderFace(pin, 5.5)))).ok());
    ASSERT_TRUE(doc.addMate(MateSpec::coincident(faceRef(w, horizontalFace(washer, 0, -1)), faceRef(po, horizontalFace(plate, 10, 1)),
                                                 Alignment::AntiAligned))
                    .ok());
    ASSERT_TRUE(doc.solve().ok());
    expectVecNear(occAt(doc, w).placement.position, Vec3(80, 20, 10), 1e-7);

    // Ещё одно отверстие: экземпляр в (80, 20) — то же вхождение, шайба на месте.
    auto r1 = doc.replacePartGeometry(pd, {plateWith({{20, 20}, {80, 20}, {20, 60}, {50, 70}}), nullptr});
    ASSERT_TRUE(r1.ok()) << r1.error().message;
    EXPECT_EQ(doc.patterns().front().instances.size(), 3u);
    EXPECT_EQ(doc.patternInstance(pid, key80), std::optional<OccurrenceId>(at80));
    EXPECT_FALSE(doc.mates().at(1).broken()) << doc.mates().at(1).brokenReason;
    ASSERT_TRUE(doc.solve().ok());
    expectVecNear(occAt(doc, w).placement.position, Vec3(80, 20, 10), 1e-7);

    // Отверстия (80, 20) больше нет — ссылка шайбы висячая.
    ASSERT_TRUE(doc.replacePartGeometry(pd, {plateWith({{20, 20}, {20, 60}, {50, 70}}), nullptr}).ok());
    EXPECT_FALSE(doc.patternInstance(pid, key80));
    ASSERT_TRUE(doc.mates().at(1).broken());
    EXPECT_EQ(doc.mates().at(1).brokenReason.rfind("dangling pattern instance:", 0), 0u) << doc.mates().at(1).brokenReason;
}

namespace {

// Плита 100×60×10 документа детали: отверстие ⌀8 в (15, 15) и линейный массив отверстия
// count1 × 2 с шагами 30 (X) и 25 (Y).
m::PartDocument patternedPlate(int count1) {
    m::PartDocument doc(m::partRegistry());
    tu::add(doc, {"Box", "Body", "", {{"length", "100"}, {"width", "60"}, {"height", "10"}}, {}, {}, false});
    doc.rebuild();
    const m::NamedShape s = doc.finalShape();
    tu::addSketchCut(doc, "Hole", tu::refTo(s, tu::planarFace(s.shape(), gp::DZ(), 10)), "15", "15", "10",
                     tu::centredCircle("4"), "");
    tu::add(doc, {"LinearPattern", "Arr", "Отверстия", {{"count1", std::to_string(count1)}, {"spacing1", "30"}, {"count2", "2"}, {"spacing2", "25"}},
                  {}, {{"direction1", "X"}, {"direction2", "Y"}}, false, {{"feature", "Hole"}}});
    EXPECT_TRUE(doc.rebuild().failed.empty());
    return doc;
}

}  // namespace

TEST(PartFeaturePatterns, InstancesFollowThePartPatternFormula) {
    m::PartDocument part = patternedPlate(3);
    auto data = partDataFromDocument(part);
    ASSERT_TRUE(data.ok()) << data.error().message;
    ASSERT_EQ(data.value().patterns.size(), 1u);
    const PartFeaturePattern& pp = data.value().patterns.front();
    EXPECT_EQ(pp.feature, "Arr");
    EXPECT_EQ(pp.label, "Отверстия");
    EXPECT_EQ(pp.keys, (std::vector<std::string>{"1,0", "2,0", "0,1", "1,1", "2,1"}));
    expectVecNear(pp.transforms[3].position, Vec3(30, 25, 0), 1e-12);

    AssemblyDocument doc;
    const TopoDS_Shape plateShape = data.value().shape;
    const DefinitionId pd = doc.addPart("", data.value()).value();
    EXPECT_EQ(doc.definition(pd).partPatterns.size(), 1u);
    // Плита повёрнута на 90° вокруг Z и сдвинута: R(x, y) = (−y, x), t = (200, 0, 0).
    const Transform place = Transform::fromAxisAngle(Vec3::UnitZ(), kPiP / 2, Vec3(200, 0, 0));
    const OccurrenceId po = doc.addOccurrence(pd, place).value();
    ASSERT_TRUE(doc.setFixed(po, true).ok());
    const TopoDS_Shape pinShape = cylinder(0, 0, 0, 4, 30);
    const OccurrenceId pin = doc.addOccurrence(doc.addPart("Pin", {pinShape, nullptr}).value(), Transform::fromTranslation({0, 0, 80})).value();
    const GeometryRef seedHole = faceRef(po, cylinderFace(plateShape, 4, 15, 15));
    ASSERT_TRUE(doc.addMate(MateSpec::concentric(faceRef(pin, cylinderFace(pinShape, 4)), seedHole)).ok());
    ASSERT_TRUE(doc.addMate(MateSpec::coincident(faceRef(pin, horizontalFace(pinShape, 0, -1)),
                                                 faceRef(po, horizontalFace(plateShape, 10, 1)), Alignment::AntiAligned))
                    .ok());
    ASSERT_TRUE(doc.solve().ok());
    auto world = [&](double x, double y, double z) { return Vec3(200 - y, x, z); };
    const Vec3 seedPos = occAt(doc, pin).placement.position;
    expectVecNear(Vec3(seedPos.x(), seedPos.y(), seedPos.z()), world(15, 15, 10), 1e-7);

    FeaturePatternSpec f;
    f.seed = pin;
    f.seedHole = seedHole;
    f.partPattern = "Arr";
    auto pid = doc.addFeaturePattern(f);
    ASSERT_TRUE(pid.ok()) << pid.error().message;
    const ComponentPattern& p = doc.patterns().front();
    ASSERT_EQ(p.instances.size(), 5u);
    for (std::size_t k = 0; k < p.keys.size(); ++k) {
        const int i = p.keys[k][0] - '0', j = p.keys[k][2] - '0';
        const Transform& t = occAt(doc, p.instances[k]).placement;
        // Ось штифта — в центре отверстия (i, j): (15 + 30i, 15 + 25j) плиты, z = 10.
        expectVecNear(t.position, world(15 + 30 * i, 15 + 25 * j, 10), 1e-7);
        EXPECT_LT(t.rotation.angularDistance(occAt(doc, pin).placement.rotation), 1e-12);
    }
    const OccurrenceId at11 = doc.patternInstance(pid.value(), "1,1").value();

    // Число копий в детали 3 → 4: экземпляры с прежними ключами — прежние вхождения.
    m::PartDocument wider = patternedPlate(4);
    auto rep = doc.replacePartGeometry(pd, partDataFromDocument(wider).value());
    ASSERT_TRUE(rep.ok()) << rep.error().message;
    EXPECT_EQ(doc.patterns().front().instances.size(), 7u);
    EXPECT_EQ(doc.patternInstance(pid.value(), "1,1"), std::optional<OccurrenceId>(at11));
    expectVecNear(occAt(doc, doc.patternInstance(pid.value(), "3,1").value()).placement.position, world(105, 40, 10), 1e-7);

    // Нет такой операции массива — ошибка с перечнем массивов детали.
    f.partPattern = "Nope";
    auto bad = doc.addFeaturePattern(f);
    ASSERT_FALSE(bad.ok());
    EXPECT_NE(bad.error().message.find("has no pattern feature 'Nope' (patterns: Arr)"), std::string::npos);

    // Файл: массивы операций детали и ключ «по массиву детали» сохраняются со встроенной формой.
    auto text = doc.toJson();
    ASSERT_TRUE(text.ok());
    auto loaded = AssemblyDocument::fromJson(text.value(), LoadOptions{});
    ASSERT_TRUE(loaded.ok()) << loaded.error().message;
    const AssemblyDocument& d = loaded.value().document;
    EXPECT_EQ(d.definition(pd).partPatterns.front().keys.size(), 7u);
    EXPECT_EQ(std::get<FeaturePatternSpec>(d.patterns().front().spec).partPattern, "Arr");
    expectVecNear(occAt(d, d.patternInstance(pid.value(), "3,1").value()).placement.position, world(105, 40, 10), 1e-7);
}

TEST(PartFeaturePatterns, CircularPartPatternTransforms) {
    m::PartDocument doc(m::partRegistry());
    // Плита −50…50 по X и Y, отверстие ⌀8 в (30, 0), круговой массив из 4 вокруг оси Z.
    tu::add(doc, {"Box", "Body", "", {{"length", "100"}, {"width", "100"}, {"height", "10"}, {"x", "-50"}, {"y", "-50"}},
                  {}, {}, false});
    doc.rebuild();
    const m::NamedShape s = doc.finalShape();
    tu::addSketchCut(doc, "Hole", tu::refTo(s, tu::planarFace(s.shape(), gp::DZ(), 10)), "30", "0", "10",
                     tu::centredCircle("4"), "");
    tu::add(doc, {"CircularPattern", "Ring", "", {{"count", "4"}}, {}, {{"axis", "Z"}}, false, {{"feature", "Hole"}}});
    const auto rep = doc.rebuild();
    ASSERT_TRUE(rep.failed.empty()) << tu::messages(tu::info(doc, rep.failed.front()));
    auto data = partDataFromDocument(doc);
    ASSERT_TRUE(data.ok());
    ASSERT_EQ(data.value().patterns.size(), 1u);
    const PartFeaturePattern& pp = data.value().patterns.front();
    EXPECT_EQ(pp.keys, (std::vector<std::string>{"1", "2", "3"}));
    for (int k = 1; k <= 3; ++k) {
        // Поворот на k·90° вокруг Z: центр отверстия (30, 0) → (30cos kφ, 30sin kφ), φ = 90°.
        const Vec3 c = pp.transforms[k - 1].applyToPoint(Vec3(30, 0, 0));
        expectVecNear(c, Vec3(30 * std::cos(k * kPiP / 2), 30 * std::sin(k * kPiP / 2), 0), 1e-9);
    }
}
