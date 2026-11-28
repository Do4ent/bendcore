// Отмена и повтор изменений документа сборки: вставка, удаление и замена компонентов,
// добавление, правка и подавление сопряжений, массивы — с восстановлением положений.

#include "assembly_document_helpers.hpp"

#include <bendcore/assembly_v2/history.hpp>

using namespace bendcore::assembly_v2;
using namespace bendcore::assembly_v2::testing;
using bendcore::kernel::ErrorCode;

namespace {

// Положения всех вхождений верхнего уровня побитно совпадают со снимком.
std::vector<std::pair<OccurrenceId, Transform>> snapshot(const AssemblyDocument& d) {
    std::vector<std::pair<OccurrenceId, Transform>> out;
    for (const Occurrence& o : d.topLevel()) out.emplace_back(o.id, o.placement);
    return out;
}

void expectSnapshot(const AssemblyDocument& d, const std::vector<std::pair<OccurrenceId, Transform>>& s) {
    ASSERT_EQ(d.topLevel().size(), s.size());
    for (std::size_t i = 0; i < s.size(); ++i) {
        const Occurrence& o = d.topLevel()[i];
        EXPECT_EQ(o.id, s[i].first);
        EXPECT_EQ(o.placement.position, s[i].second.position) << o.name;
        EXPECT_EQ(o.placement.rotation.coeffs(), s[i].second.rotation.coeffs()) << o.name;
    }
}

struct Mechanism {
    TopoDS_Shape bush = bushing(20, 10, 30);
    TopoDS_Shape pin = cylinder(0, 0, 0, 10, 50);
    TopoDS_Shape longPin = cylinder(0, 0, 0, 10, 80);
    DefinitionId bushingDef = 0, pinDef = 0, longPinDef = 0;
    AssemblyHistory history{AssemblyDocument("Механизм")};
    OccurrenceId b = 0;

    Mechanism() {
        EXPECT_TRUE(history
                        .apply("Parts",
                               [&](AssemblyDocument& d) -> Status {
                                   bushingDef = d.addPart("Втулка", {bush, nullptr}).value();
                                   pinDef = d.addPart("Вал", {pin, nullptr}).value();
                                   longPinDef = d.addPart("Вал длинный", {longPin, nullptr}).value();
                                   return std::monostate{};
                               })
                        .ok());
        b = history.insertComponent(bushingDef, Transform::identity()).value();
        EXPECT_TRUE(history.setFixed(b, true).ok());
    }
    const AssemblyDocument& doc() const { return history.document(); }

    // Соосность и торец: вал на верхнем торце втулки z = 30.
    void mate(OccurrenceId s, const TopoDS_Shape& shape) {
        ASSERT_TRUE(history.addMate(MateSpec::concentric(faceRef(s, cylinderFace(shape, 10)), faceRef(b, cylinderFace(bush, 10)))).ok());
        ASSERT_TRUE(history
                        .addMate(MateSpec::coincident(faceRef(s, horizontalFace(shape, 0, -1)),
                                                      faceRef(b, horizontalFace(bush, 30, 1)), Alignment::AntiAligned))
                        .ok());
    }
};

}  // namespace

TEST(AssemblyHistory, MatesUndoRestoresPlacements) {
    Mechanism m;
    const Transform start = Transform::fromAxisAngle(Vec3(1, 1, 0), 0.4, Vec3(30, -15, 40));
    const OccurrenceId s = m.history.insertComponent(m.pinDef, start).value();
    const auto beforeMates = snapshot(m.doc());
    m.mate(s, m.pin);
    expectVecNear(occAt(m.doc(), s).placement.position, Vec3(0, 0, 30), 1e-8);
    const auto seated = snapshot(m.doc());
    EXPECT_EQ(m.history.undoLabel(), "Add mate");

    ASSERT_TRUE(m.history.undo().ok());  // торец
    EXPECT_EQ(m.doc().mates().size(), 1u);
    ASSERT_TRUE(m.history.undo().ok());  // соосность
    EXPECT_TRUE(m.doc().mates().empty());
    expectSnapshot(m.doc(), beforeMates);  // вал вернулся в исходное положение побитно
    EXPECT_EQ(m.history.redoLabel(), "Add mate");

    ASSERT_TRUE(m.history.redo().ok());
    ASSERT_TRUE(m.history.redo().ok());
    EXPECT_EQ(m.doc().mates().size(), 2u);
    expectSnapshot(m.doc(), seated);
    EXPECT_FALSE(m.history.canRedo());
    EXPECT_EQ(m.history.redo().error().code, ErrorCode::InvalidInput);

    // Правка сопряжения (расстояние вместо совпадения) и её отмена.
    const MateId faces = m.doc().mates()[1].id;
    MateSpec spec = MateSpec::distance(faceRef(s, horizontalFace(m.pin, 0, -1)), faceRef(m.b, horizontalFace(m.bush, 30, 1)),
                                       -5.0, Alignment::AntiAligned);
    ASSERT_TRUE(m.history.editMate(faces, spec).ok());
    EXPECT_EQ(m.doc().findMate(faces)->spec.type, MateType::Distance);
    EXPECT_NEAR(std::abs(occAt(m.doc(), s).placement.position.z() - 30.0), 5.0, 1e-8);
    ASSERT_TRUE(m.history.undo().ok());
    EXPECT_EQ(m.doc().findMate(faces)->spec.type, MateType::Coincident);
    expectSnapshot(m.doc(), seated);

    // Подавление: сопряжение не решается; отмена возвращает его.
    ASSERT_TRUE(m.history.setMateSuppressed(faces, true).ok());
    EXPECT_TRUE(m.doc().findMate(faces)->suppressed);
    ASSERT_TRUE(m.history.undo().ok());
    EXPECT_FALSE(m.doc().findMate(faces)->suppressed);

    // Новое изменение очищает повтор.
    ASSERT_TRUE(m.history.redo().ok());
    ASSERT_TRUE(m.history.undo().ok());
    ASSERT_TRUE(m.history.moveComponent(m.b, Transform::fromTranslation(Vec3(0, 0, 1))).ok());
    EXPECT_FALSE(m.history.canRedo());
}

TEST(AssemblyHistory, ComponentsInsertRemoveReplace) {
    Mechanism m;
    const OccurrenceId s = m.history.insertComponent(m.pinDef, Transform::fromTranslation(Vec3(5, 5, 70))).value();
    m.mate(s, m.pin);
    const auto seated = snapshot(m.doc());
    const std::size_t depth = m.history.undoDepth();

    // Удаление вместе с сопряжениями; отмена возвращает и вхождение, и сопряжения с теми же ID.
    const std::vector<MateId> mateIds = {m.doc().mates()[0].id, m.doc().mates()[1].id};
    ASSERT_TRUE(m.history.removeComponent(s).ok());
    EXPECT_EQ(m.doc().findOccurrence(s), nullptr);
    EXPECT_TRUE(m.doc().mates().empty());
    ASSERT_TRUE(m.history.undo().ok());
    ASSERT_NE(m.doc().findOccurrence(s), nullptr);
    ASSERT_EQ(m.doc().mates().size(), 2u);
    EXPECT_EQ(m.doc().mates()[0].id, mateIds[0]);
    expectSnapshot(m.doc(), seated);

    // Замена: длинный вал, сопряжения перенесены; середина вала z = 70; отмена — снова короткий.
    auto rep = m.history.replaceComponent(s, m.longPinDef);
    ASSERT_TRUE(rep.ok()) << rep.error().message;
    EXPECT_TRUE(rep.value().broken.empty());
    EXPECT_EQ(occAt(m.doc(), s).definition, m.longPinDef);
    expectVecNear(occAt(m.doc(), s).placement.applyToPoint(Vec3(0, 0, 40)), Vec3(0, 0, 70), 1e-8);
    ASSERT_TRUE(m.history.undo().ok());
    EXPECT_EQ(occAt(m.doc(), s).definition, m.pinDef);
    expectSnapshot(m.doc(), seated);
    EXPECT_EQ(m.history.undoDepth(), depth);

    // Подавление компонента и отмена.
    ASSERT_TRUE(m.history.setComponentSuppressed(s, true).ok());
    EXPECT_TRUE(occAt(m.doc(), s).suppressed);
    EXPECT_EQ(m.doc().placedParts().value().size(), 1u);
    ASSERT_TRUE(m.history.undo().ok());
    EXPECT_EQ(m.doc().placedParts().value().size(), 2u);

    // Ошибка не записывается и не меняет документ.
    EXPECT_FALSE(m.history.insertComponent(999, Transform::identity()).ok());
    EXPECT_FALSE(m.history.replaceComponent(s, m.doc().root()).ok());
    EXPECT_EQ(m.history.undoDepth(), depth);
    expectSnapshot(m.doc(), seated);
}

TEST(AssemblyHistory, PatternsUndo) {
    Mechanism m;
    const OccurrenceId s = m.history.insertComponent(m.pinDef, Transform::fromTranslation(Vec3(0, 0, 30))).value();
    const auto before = snapshot(m.doc());
    LinearPatternSpec spec;
    spec.seed = s;
    spec.direction = Vec3::UnitX();
    spec.spacing = 25;
    spec.count = 4;
    auto p = m.history.addLinearPattern(spec);
    ASSERT_TRUE(p.ok()) << p.error().message;
    ASSERT_EQ(m.doc().patterns().size(), 1u);
    const auto instances = m.doc().patterns()[0].instances;
    ASSERT_EQ(instances.size(), 3u);
    expectVecNear(occAt(m.doc(), instances[2]).placement.position, Vec3(75, 0, 30), 1e-12);
    const auto withPattern = snapshot(m.doc());

    ASSERT_TRUE(m.history.undo().ok());
    EXPECT_TRUE(m.doc().patterns().empty());
    expectSnapshot(m.doc(), before);
    ASSERT_TRUE(m.history.redo().ok());
    expectSnapshot(m.doc(), withPattern);
    EXPECT_EQ(m.doc().patterns()[0].instances, instances);

    CircularPatternSpec c;
    c.seed = s;
    c.axisPoint = Vec3(0, 0, 0);
    c.axisDirection = Vec3::UnitZ();
    c.angleStep = kPi / 2;
    c.count = 4;
    ASSERT_TRUE(m.history.addCircularPattern(c).ok());
    EXPECT_EQ(m.doc().patterns().size(), 2u);
    ASSERT_TRUE(m.history.removePattern(p.value()).ok());
    EXPECT_EQ(m.doc().patterns().size(), 1u);
    ASSERT_TRUE(m.history.undo().ok());
    ASSERT_TRUE(m.history.undo().ok());
    expectSnapshot(m.doc(), withPattern);
}

TEST(AssemblyHistory, DepthLimitAndGenericApply) {
    HistoryOptions opt;
    opt.maxDepth = 3;
    AssemblyHistory h(AssemblyDocument("A"), opt);
    DefinitionId part = 0;
    ASSERT_TRUE(h.apply("Part", [&](AssemblyDocument& d) -> Status {
                     part = d.addPart("Box", {box(0, 0, 0, 1, 1, 1), nullptr}).value();
                     return std::monostate{};
                 }).ok());
    for (int i = 0; i < 5; ++i) ASSERT_TRUE(h.insertComponent(part, Transform::fromTranslation(Vec3(i, 0, 0))).ok());
    EXPECT_EQ(h.undoDepth(), 3u);
    while (h.canUndo()) ASSERT_TRUE(h.undo().ok());
    EXPECT_EQ(h.document().topLevel().size(), 2u);  // самые старые шаги отброшены
    EXPECT_EQ(h.undo().error().code, ErrorCode::InvalidInput);
    // Ошибка внутри apply откатывает частичные изменения.
    auto s = h.apply("Broken", [&](AssemblyDocument& d) -> Status {
        EXPECT_TRUE(d.addOccurrence(part, Transform::identity()).ok());
        return Status::failure(ErrorCode::InvalidInput, "rejected");
    });
    EXPECT_FALSE(s.ok());
    EXPECT_EQ(h.document().topLevel().size(), 2u);
    h.clear();
    EXPECT_FALSE(h.canRedo());
}
