// Собственный формат документа сборки: круговой проход на эталонных механизмах (вал-втулка
// с деталями из файлов, кривошип с угловым сопряжением, плита с болтами по массиву),
// перемещённые и изменённые файлы деталей, внешние подсборки, миграция v1, ошибки формата,
// перенос обозначений и наименований через STEP.

#include "assembly_document_helpers.hpp"

#include <bendcore/assembly_v2/bom.hpp>
#include <bendcore/kernel/step_io.hpp>
#include <bendcore/modeling/basic_features.hpp>
#include <bendcore/modeling/part_document.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>

using namespace bendcore::assembly_v2;
using namespace bendcore::assembly_v2::testing;
using bendcore::kernel::ErrorCode;
namespace fs = std::filesystem;
namespace modeling = bendcore::modeling;

namespace {

fs::path testDir(const std::string& name) {
    const fs::path dir = fs::path(BENDCORE_ASSEMBLY_TEST_TMP) / name;
    fs::remove_all(dir);
    fs::create_directories(dir);
    return dir;
}

void writeText(const fs::path& p, const std::string& text) {
    fs::create_directories(p.parent_path());
    std::ofstream out(p, std::ios::binary);
    out << text;
    ASSERT_TRUE(out.good()) << p;
}

modeling::PartDocument shaftDocument(double radius, double height) {
    modeling::PartDocument doc(modeling::basic::basicRegistry());
    modeling::FeatureSpec spec;
    spec.type = "Cylinder";
    spec.id = "Shaft";
    spec.parameters = {{"radius", std::to_string(radius)}, {"height", std::to_string(height)}};
    EXPECT_TRUE(doc.addFeature(spec).ok());
    EXPECT_TRUE(doc.rebuild().failed.empty());
    return doc;
}

void writeShaft(const fs::path& dir, double height) {
    writeText(dir / "parts" / "shaft.json", shaftDocument(10, height).toJson().value());
}

void writeStepPart(const fs::path& p, const TopoDS_Shape& s) {
    writeText(p, bendcore::kernel::writeStep(bendcore::kernel::Shape(s)).value());
}

GeometryRef named(OccurrenceId o, const std::string& name) {
    return {{o}, ElementRef::named(ElementKind::Face, name), EntityUse::Auto};
}

// Положения всех вхождений верхнего уровня совпадают поэлементно (кватернион — с точностью до знака).
void expectSamePlacements(const AssemblyDocument& a, const AssemblyDocument& b, double tol) {
    ASSERT_EQ(a.topLevel().size(), b.topLevel().size());
    for (std::size_t i = 0; i < a.topLevel().size(); ++i) {
        const Occurrence& x = a.topLevel()[i];
        const Occurrence& y = b.topLevel()[i];
        SCOPED_TRACE(x.name);
        EXPECT_EQ(x.id, y.id);
        EXPECT_EQ(x.name, y.name);
        EXPECT_LE((x.placement.position - y.placement.position).norm(), tol);
        EXPECT_LE((x.placement.rotationMatrix() - y.placement.rotationMatrix()).norm(), tol);
    }
}

// Вал (документ детали parts/shaft.json, ссылки по именам) во втулке (STEP parts/bushing.step,
// ссылки по индексам).
struct ShaftFiles {
    fs::path dir;
    TopoDS_Shape bush = bushing(20, 10, 30);
    AssemblyDocument doc{"Вал во втулке"};
    DefinitionId bushingDef = 0, shaftDef = 0;
    OccurrenceId b = 0, s = 0;
    MateId concentric = 0, faces = 0;

    explicit ShaftFiles(const std::string& name) : dir(testDir(name)) {
        writeShaft(dir, 50);
        writeStepPart(dir / "parts" / "bushing.step", bush);
        const LoadOptions opt = fileLoadOptions(modeling::basic::basicRegistry(), dir.string());
        bushingDef = doc.addPartFromSource("Втулка", "parts/bushing.step", opt.parts,
                                           {"ВТ-001", "Втулка бронзовая", "БрО10", 8800})
                         .value();
        shaftDef = doc.addPartFromSource("Вал", "parts/shaft.json", opt.parts, {"ВЛ-002", "Вал", "Сталь 45", 7850}).value();
        b = doc.addOccurrence(bushingDef, Transform::identity()).value();
        EXPECT_TRUE(doc.setFixed(b, true).ok());
        s = doc.addOccurrence(shaftDef, Transform::fromAxisAngle(Vec3(1, 1, 0), 0.4, Vec3(30, -15, 40))).value();
        EXPECT_TRUE(doc.setOccurrenceDesignation(s, "Поз. 2").ok());
        MateSpec m1 = MateSpec::concentric(named(s, "Shaft:Side"), faceRef(b, cylinderFace(bush, 10)));
        m1.name = "Соосность";
        concentric = doc.addMate(m1).value();
        MateSpec m2 = MateSpec::coincident(named(s, "Shaft:Bottom"), faceRef(b, horizontalFace(bush, 30, 1)),
                                           Alignment::AntiAligned);
        m2.name = "Торец";
        faces = doc.addMate(m2).value();
        auto r = doc.solve();
        EXPECT_TRUE(r.ok() && r.value().success);
    }

    LoadOptions options() const { return fileLoadOptions(modeling::basic::basicRegistry(), dir.string()); }
};

void expectShaftSeated(const AssemblyDocument& doc, OccurrenceId shaft) {
    const Transform& p = occAt(doc, shaft).placement;
    expectVecNear(p.applyToPoint(Vec3::Zero()), Vec3(0, 0, 30), 1e-8);
    expectVecNear(p.applyToDirection(Vec3::UnitZ()), Vec3::UnitZ(), 1e-9);
}

const SourceDiagnostic* sourceOf(const LoadReport& r, DefinitionId d) {
    for (const SourceDiagnostic& s : r.sources)
        if (s.definition == d) return &s;
    return nullptr;
}

const MateLoadDiagnostic* mateOf(const LoadReport& r, MateId id) {
    for (const MateLoadDiagnostic& m : r.mates)
        if (m.mate == id) return &m;
    return nullptr;
}

}  // namespace

TEST(AssemblyDocumentIo, ShaftInBushingRoundTrip) {
    ShaftFiles m("shaft_roundtrip");
    expectShaftSeated(m.doc, m.s);
    for (bool embed : {true, false}) {
        SCOPED_TRACE(embed ? "embedded" : "sources only");
        const fs::path file = m.dir / "asm.json";
        ASSERT_TRUE(m.doc.saveFile(file.string(), {embed, 1}).ok());
        auto loaded = AssemblyDocument::loadFile(file.string(), modeling::basic::basicRegistry());
        ASSERT_TRUE(loaded.ok()) << loaded.error().message;
        AssemblyDocument& d = loaded.value().document;
        const LoadReport& rep = loaded.value().report;
        EXPECT_EQ(rep.fileVersion, AssemblyDocument::kFormatVersion);
        EXPECT_FALSE(rep.migrated);
        EXPECT_TRUE(rep.mates.empty());
        ASSERT_NE(sourceOf(rep, m.shaftDef), nullptr);
        EXPECT_EQ(sourceOf(rep, m.shaftDef)->status, SourceStatus::Loaded);
        EXPECT_EQ(sourceOf(rep, m.bushingDef)->status, SourceStatus::Loaded);
        EXPECT_EQ(d.name(), "Вал во втулке");
        EXPECT_EQ(d.definition(m.bushingDef).properties.name, "Втулка бронзовая");
        EXPECT_EQ(d.definition(m.bushingDef).properties.material, "БрО10");
        EXPECT_EQ(d.definition(m.shaftDef).properties.density, std::optional<double>(7850));
        EXPECT_NE(d.definition(m.shaftDef).elementMap, nullptr);
        EXPECT_TRUE(occAt(d, m.b).fixed);
        EXPECT_EQ(occAt(d, m.s).designation, "Поз. 2");
        ASSERT_EQ(d.mates().size(), 2u);
        EXPECT_EQ(d.findMate(m.concentric)->spec.name, "Соосность");
        EXPECT_EQ(d.findMate(m.concentric)->spec.a.element.name, "Shaft:Side");
        auto r = d.solve();
        ASSERT_TRUE(r.ok());
        EXPECT_TRUE(r.value().success);
        EXPECT_EQ(r.value().totalDof, 1);
        expectSamePlacements(m.doc, d, 1e-9);
        // Новые ID не пересекаются со старыми.
        auto extra = d.addOccurrence(m.shaftDef, Transform::identity());
        ASSERT_TRUE(extra.ok());
        EXPECT_GT(extra.value(), m.s);
        // Сохранение загруженного документа даёт тот же текст.
        EXPECT_TRUE(d.removeOccurrence(extra.value()).ok());
    }
}

// Кривошип: палец основания, рычаг с отверстием (соосность + торец), угол 30° — 0 степеней
// свободы; правка угла и подавление углового сопряжения.
TEST(AssemblyDocumentIo, CrankRoundTrip) {
    const TopoDS_Shape base = fuse(box(0, 0, 0, 100, 40, 10), cylinder(20, 20, 10, 5, 20));
    const TopoDS_Shape arm = cut(box(-8, -8, 0, 68, 16, 5), cylinder(0, 0, -1, 5, 7));
    AssemblyDocument doc("Кривошип");
    const auto baseDef = doc.addPart("Основание", {base, nullptr}).value();
    const auto armDef = doc.addPart("Рычаг", {arm, nullptr}).value();
    const auto bo = doc.addOccurrence(baseDef, Transform::identity()).value();
    ASSERT_TRUE(doc.setFixed(bo, true).ok());
    const auto ao = doc.addOccurrence(armDef, Transform::fromAxisAngle(Vec3(0, 0, 1), 0.3, Vec3(25, 15, 14))).value();
    const auto pinFace = findFace(base, [](const BRepAdaptor_Surface& s, const TopoDS_Face&) {
        return s.GetType() == GeomAbs_Cylinder;
    });
    ASSERT_TRUE(pinFace.has_value());
    ASSERT_TRUE(doc.addMate(MateSpec::concentric(faceRef(ao, cylinderFace(arm, 5)), faceRef(bo, *pinFace))).ok());
    ASSERT_TRUE(doc.addMate(MateSpec::coincident(faceRef(ao, horizontalFace(arm, 0, -1)),
                                                 faceRef(bo, horizontalFace(base, 10, 1)), Alignment::AntiAligned))
                    .ok());
    // Боковая грань рычага y = −8 и передняя грань основания y = 0: обе с нормалью −Y.
    auto sideOf = [](const TopoDS_Shape& s, double y) {
        return findFace(s, [y](const BRepAdaptor_Surface& a, const TopoDS_Face&) {
                   return a.GetType() == GeomAbs_Plane && std::abs(a.Plane().Location().Y() - y) < 1e-9 &&
                          std::abs(a.Plane().Axis().Direction().Y()) > 0.5;
               })
            .value();
    };
    auto angle = doc.addMate(MateSpec::angle(faceRef(ao, sideOf(arm, -8)), faceRef(bo, sideOf(base, 0)), kPi / 6));
    ASSERT_TRUE(angle.ok()) << angle.error().message;
    auto r = doc.solve();
    ASSERT_TRUE(r.ok() && r.value().success);
    EXPECT_EQ(r.value().totalDof, 0);
    auto checkCrank = [&](const AssemblyDocument& d, double radians) {
        const Transform& p = occAt(d, ao).placement;
        expectVecNear(p.applyToPoint(Vec3::Zero()), Vec3(20, 20, 10), 1e-8);
        EXPECT_NEAR(std::abs(p.applyToDirection(Vec3::UnitX()).dot(Vec3::UnitX())), std::cos(radians), 1e-9);
    };
    checkCrank(doc, kPi / 6);

    auto text = doc.toJson();
    ASSERT_TRUE(text.ok());
    auto loaded = AssemblyDocument::fromJson(text.value(), {});
    ASSERT_TRUE(loaded.ok()) << loaded.error().message;
    AssemblyDocument& d = loaded.value().document;
    EXPECT_EQ(sourceOf(loaded.value().report, baseDef)->status, SourceStatus::Embedded);
    auto r2 = d.solve();
    ASSERT_TRUE(r2.ok() && r2.value().success);
    EXPECT_EQ(r2.value().totalDof, 0);
    expectSamePlacements(doc, d, 1e-9);

    // Правка угла сохраняет ID сопряжения; подавление возвращает степень свободы.
    MateSpec spec = d.findMate(angle.value())->spec;
    spec.value = kPi / 3;
    ASSERT_TRUE(d.editMate(angle.value(), spec).ok());
    ASSERT_TRUE(d.solve().ok());
    checkCrank(d, kPi / 3);
    ASSERT_TRUE(d.setMateSuppressed(angle.value(), true).ok());
    auto r3 = d.solve();
    ASSERT_TRUE(r3.ok());
    EXPECT_EQ(r3.value().totalDof, 1);
    EXPECT_EQ(r3.value().suppressedMates, std::vector<MateId>{angle.value()});

    auto again = AssemblyDocument::fromJson(d.toJson().value(), {});
    ASSERT_TRUE(again.ok());
    EXPECT_TRUE(again.value().document.findMate(angle.value())->suppressed);
    EXPECT_NEAR(again.value().document.findMate(angle.value())->spec.value, kPi / 3, 0.0);
    expectSamePlacements(d, again.value().document, 1e-14);  // без решения — с точностью до ulp
}

// Плита с болтами: массив по отверстиям, подавленный болт, разнесённый вид, BOM.
TEST(AssemblyDocumentIo, PlateWithBoltsPatternRoundTrip) {
    const double holes[4][2] = {{20, 20}, {80, 20}, {20, 60}, {80, 60}};
    TopoDS_Shape plate = box(0, 0, 0, 100, 80, 10);
    for (const auto& h : holes) plate = cut(plate, cylinder(h[0], h[1], -1, 5.5, 12));
    const TopoDS_Shape boltShape = bolt(5, 30, 8, 6);
    AssemblyDocument doc("Плита с болтами");
    const auto plateDef = doc.addPart("Plate", {plate, nullptr}, {"PL-01", "Плита", "Steel", 7850}).value();
    const auto boltDef = doc.addPart("Bolt", {boltShape, nullptr}, {"BT-M10", "Болт", "Steel", 7850}).value();
    const auto po = doc.addOccurrence(plateDef, Transform::identity()).value();
    ASSERT_TRUE(doc.setFixed(po, true).ok());
    const auto seed = doc.addOccurrence(boltDef, Transform::fromAxisAngle(Vec3(0.3, 1, 0.2), 0.5, Vec3(10, 30, 60))).value();
    const GeometryRef hole1 = faceRef(po, cylinderFace(plate, 5.5, 20, 20));
    ASSERT_TRUE(doc.addMate(MateSpec::concentric(faceRef(seed, cylinderFace(boltShape, 5)), hole1)).ok());
    ASSERT_TRUE(doc.addMate(MateSpec::coincident(faceRef(seed, horizontalFace(boltShape, 0, -1)),
                                                 faceRef(po, horizontalFace(plate, 10, 1)), Alignment::AntiAligned))
                    .ok());
    ASSERT_TRUE(doc.solve().ok());
    auto pat = doc.addFeaturePattern({seed, hole1, std::nullopt, 1e-6});
    ASSERT_TRUE(pat.ok()) << pat.error().message;
    const auto instances = doc.patterns()[0].instances;
    ASSERT_EQ(instances.size(), 3u);
    ASSERT_TRUE(doc.setOccurrenceSuppressed(instances[2], true).ok());
    doc.addExplodedView({"Разнесённый", {{{seed}, Vec3::UnitZ(), 40.0}}});

    auto loaded = AssemblyDocument::fromJson(doc.toJson().value(), {});
    ASSERT_TRUE(loaded.ok()) << loaded.error().message;
    AssemblyDocument& d = loaded.value().document;
    ASSERT_EQ(d.patterns().size(), 1u);
    EXPECT_EQ(d.patterns()[0].instances, instances);
    EXPECT_EQ(d.patterns()[0].kind, PatternKind::Feature);
    EXPECT_TRUE(occAt(d, instances[2]).suppressed);
    ASSERT_EQ(d.explodedViews().size(), 1u);
    EXPECT_EQ(d.explodedViews()[0].name, "Разнесённый");
    auto r = d.solve();
    ASSERT_TRUE(r.ok() && r.value().success);
    expectSamePlacements(doc, d, 1e-9);
    // Экземпляры — в отверстиях (20,60), (80,20), (80,60) на z = 10.
    std::vector<Vec3> pos;
    for (OccurrenceId id : instances) pos.push_back(occAt(d, id).placement.applyToPoint(Vec3::Zero()));
    for (const Vec3& p : pos) EXPECT_NEAR(p.z(), 10.0, 1e-8);
    // BOM: подавленный болт не считается — 3 болта, масса по объёму и плотности.
    auto bom = flatBom(d);
    ASSERT_TRUE(bom.ok()) << bom.error().message;
    ASSERT_EQ(bom.value().rows.size(), 2u);
    EXPECT_EQ(bom.value().rows[1].designation, "BT-M10");
    EXPECT_EQ(bom.value().rows[1].quantity, 3);
    EXPECT_NEAR(bom.value().rows[1].unitVolume, kPi * 25 * 30 + kPi * 64 * 6, 1e-6);
    auto placed = d.placedParts();
    ASSERT_TRUE(placed.ok());
    EXPECT_EQ(placed.value().size(), 4u);  // плита + 3 болта
}

TEST(AssemblyDocumentIo, MovedPartFileDiagnostics) {
    ShaftFiles m("shaft_moved");
    const std::string withGeometry = m.doc.toJson({true, -1}).value();
    const std::string withoutGeometry = m.doc.toJson({false, -1}).value();
    fs::rename(m.dir / "parts" / "shaft.json", m.dir / "shaft_moved.json");

    // Встроенная геометрия: сопряжения сохранены, диагностика называет файл и каждое сопряжение.
    auto a = AssemblyDocument::fromJson(withGeometry, m.options());
    ASSERT_TRUE(a.ok()) << a.error().message;
    const SourceDiagnostic* s = sourceOf(a.value().report, m.shaftDef);
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->status, SourceStatus::MissingEmbedded);
    EXPECT_NE(s->message.find("parts/shaft.json"), std::string::npos) << s->message;
    ASSERT_EQ(a.value().report.mates.size(), 2u);
    for (MateId id : {m.concentric, m.faces}) {
        const MateLoadDiagnostic* md = mateOf(a.value().report, id);
        ASSERT_NE(md, nullptr);
        EXPECT_EQ(md->outcome, MateOutcome::Kept);
        EXPECT_EQ(md->definitions, std::vector<DefinitionId>{m.shaftDef});
        EXPECT_NE(md->message.find("parts/shaft.json"), std::string::npos) << md->message;
    }
    ASSERT_TRUE(a.value().document.solve().ok());
    expectShaftSeated(a.value().document, m.s);

    // Без встроенной геометрии: деталь пуста, оба сопряжения сломаны с причиной.
    auto b = AssemblyDocument::fromJson(withoutGeometry, m.options());
    ASSERT_TRUE(b.ok()) << b.error().message;
    EXPECT_EQ(sourceOf(b.value().report, m.shaftDef)->status, SourceStatus::Missing);
    EXPECT_FALSE(b.value().document.definition(m.shaftDef).loadError.empty());
    ASSERT_EQ(b.value().report.mates.size(), 2u);
    for (const MateLoadDiagnostic& md : b.value().report.mates) {
        EXPECT_EQ(md.outcome, MateOutcome::Broken);
        EXPECT_NE(md.message.find("parts/shaft.json"), std::string::npos) << md.message;
        EXPECT_TRUE(b.value().document.findMate(md.mate)->broken());
    }
    auto solved = b.value().document.solve();
    ASSERT_TRUE(solved.ok());
    EXPECT_EQ(solved.value().brokenMates.size(), 2u);
    EXPECT_EQ(b.value().document.placedParts().value().size(), 1u);  // только втулка
    EXPECT_EQ(flatBom(b.value().document).error().code, ErrorCode::InvalidInput);

    // Файл вернули на место — документ, сохранённый с пустой деталью, снова полный.
    const std::string again = b.value().document.toJson().value();
    fs::rename(m.dir / "shaft_moved.json", m.dir / "parts" / "shaft.json");
    auto c = AssemblyDocument::fromJson(again, m.options());
    ASSERT_TRUE(c.ok()) << c.error().message;
    for (const MateLoadDiagnostic& md : c.value().report.mates) EXPECT_NE(md.outcome, MateOutcome::Broken) << md.message;
    ASSERT_TRUE(c.value().document.solve().ok());
    expectShaftSeated(c.value().document, m.s);
}

TEST(AssemblyDocumentIo, ChangedPartFileDiagnostics) {
    ShaftFiles m("shaft_changed");
    const std::string saved = m.doc.toJson().value();
    // Вал длиннее: ссылки по именам находятся заново, вал снова на торце втулки.
    writeShaft(m.dir, 80);
    // Втулка заменена коробкой без отверстия: соосность ломается, торец находится по сигнатуре.
    writeStepPart(m.dir / "parts" / "bushing.step", box(-20, -20, 0, 40, 40, 30));
    auto a = AssemblyDocument::fromJson(saved, m.options());
    ASSERT_TRUE(a.ok()) << a.error().message;
    const LoadReport& rep = a.value().report;
    EXPECT_EQ(sourceOf(rep, m.shaftDef)->status, SourceStatus::Changed);
    EXPECT_EQ(sourceOf(rep, m.bushingDef)->status, SourceStatus::Changed);
    EXPECT_NE(sourceOf(rep, m.bushingDef)->message.find("faces"), std::string::npos) << sourceOf(rep, m.bushingDef)->message;
    const MateLoadDiagnostic* conc = mateOf(rep, m.concentric);
    ASSERT_NE(conc, nullptr);
    EXPECT_EQ(conc->outcome, MateOutcome::Broken);
    EXPECT_NE(conc->message.find("Cylinder"), std::string::npos) << conc->message;
    EXPECT_NE(std::find(conc->definitions.begin(), conc->definitions.end(), m.bushingDef), conc->definitions.end());
    const MateLoadDiagnostic* face = mateOf(rep, m.faces);
    ASSERT_NE(face, nullptr);
    EXPECT_EQ(face->outcome, MateOutcome::Remapped) << face->message;
    EXPECT_EQ(face->definitions.size(), 2u);
    auto r = a.value().document.solve();
    ASSERT_TRUE(r.ok());
    EXPECT_EQ(r.value().brokenMates, std::vector<MateId>{m.concentric});
    // Нижний торец вала на верхней грани коробки z = 30.
    const Transform& p = occAt(a.value().document, m.s).placement;
    EXPECT_NEAR(p.applyToPoint(Vec3::Zero()).z(), 30.0, 1e-8);
    EXPECT_NEAR(volumeOf(a.value().document.definition(m.shaftDef).shape), kPi * 100 * 80, 1e-6);
}

// Внешняя подсборка: файл sub/shaft_unit.json вставлен дважды, сопряжение ссылается на грань
// детали внутри подсборки; состав подсборки только для чтения; цикл файлов отклоняется.
TEST(AssemblyDocumentIo, ExternalSubassembly) {
    ShaftFiles unit("external");
    // Подсборка лежит в sub/, её детали — ../parts относительно неё.
    {
        auto moved = AssemblyDocument::fromJson(unit.doc.toJson({false, 1}).value(), unit.options());
        ASSERT_TRUE(moved.ok());
        std::string text = moved.value().document.toJson({false, 1}).value();
        for (const char* p : {"parts/shaft.json", "parts/bushing.step"}) {
            const std::string from = std::string("\"") + p + "\"";
            const std::string to = std::string("\"../") + p + "\"";
            text.replace(text.find(from), from.size(), to);
        }
        writeText(unit.dir / "sub" / "shaft_unit.json", text);
    }
    const LoadOptions opt = unit.options();
    AssemblyDocument top("Стенд");
    const TopoDS_Shape plateShape = box(-100, -50, -10, 200, 100, 10);
    const auto plateDef = top.addPart("Плита", {plateShape, nullptr}).value();
    const auto plate = top.addOccurrence(plateDef, Transform::identity()).value();
    ASSERT_TRUE(top.setFixed(plate, true).ok());
    auto sub = top.addSubassemblyFromSource("", "sub/shaft_unit.json", opt);
    ASSERT_TRUE(sub.ok()) << sub.error().message;
    EXPECT_EQ(top.definition(sub.value()).name, "Вал во втулке");
    EXPECT_EQ(top.definition(sub.value()).occurrences.size(), 2u);
    const auto u1 = top.addOccurrence(sub.value(), Transform::fromTranslation(Vec3(-50, 0, 5))).value();
    const auto u2 = top.addOccurrence(sub.value(), Transform::fromTranslation(Vec3(50, 0, 7))).value();
    // Детали из одного файла — одно определение (BOM: по 2 втулки и вала).
    auto bom = flatBom(top);
    ASSERT_TRUE(bom.ok()) << bom.error().message;
    ASSERT_EQ(bom.value().rows.size(), 3u);
    EXPECT_EQ(bom.value().rows[1].quantity, 2);
    // Нижняя грань втулки первой подсборки лежит на плите (z = 0).
    const Occurrence& innerBushing = top.definition(sub.value()).occurrences[0];
    const DefinitionId bushingDef = innerBushing.definition;
    GeometryRef bottom{{u1, innerBushing.id}, ElementRef::face(horizontalFace(unit.bush, 0, -1)), EntityUse::Auto};
    auto mate = top.addMate(MateSpec::coincident(bottom, faceRef(plate, horizontalFace(plateShape, 0, 1)), Alignment::AntiAligned));
    ASSERT_TRUE(mate.ok()) << mate.error().message;
    ASSERT_TRUE(top.solve().ok());
    EXPECT_NEAR(occAt(top, u1).placement.position.z(), 0.0, 1e-9);
    EXPECT_NEAR(occAt(top, u2).placement.position.z(), 7.0, 0.0);
    // Только для чтения.
    EXPECT_EQ(top.addOccurrence(plateDef, Transform::identity(), "x", sub.value()).error().code, ErrorCode::InvalidInput);
    EXPECT_EQ(top.setPlacement(innerBushing.id, Transform::identity()).error().code, ErrorCode::InvalidInput);
    EXPECT_EQ(top.removeOccurrence(innerBushing.id).error().code, ErrorCode::InvalidInput);

    const fs::path file = unit.dir / "top.json";
    ASSERT_TRUE(top.saveFile(file.string()).ok());
    auto loaded = AssemblyDocument::loadFile(file.string(), modeling::basic::basicRegistry());
    ASSERT_TRUE(loaded.ok()) << loaded.error().message;
    AssemblyDocument& d = loaded.value().document;
    EXPECT_TRUE(loaded.value().report.mates.empty());
    ASSERT_EQ(d.mates().size(), 1u);
    EXPECT_FALSE(d.mates()[0].broken()) << d.mates()[0].brokenReason;
    EXPECT_EQ(d.definition(d.mates()[0].bound[0].definition).name, top.definition(bushingDef).name);
    auto r = d.solve();
    ASSERT_TRUE(r.ok() && r.value().success);
    expectSamePlacements(top, d, 1e-9);
    EXPECT_EQ(flatBom(d).value().rows[1].quantity, 2);
    // Внутреннее положение вала — из файла подсборки (решено там): торец на z = 30 втулки.
    auto placed = d.placedParts().value();
    ASSERT_EQ(placed.size(), 5u);

    // Цикл: подсборка ссылается на файл верхнего уровня.
    writeText(unit.dir / "loop_a.json", R"({"format":"bendcore-assembly","version":2,"name":"A","definitions":[{"id":1,"kind":"assembly","name":"B","source":"loop_b.json"}],"occurrences":[{"id":1,"name":"B:1","definition":1,"placement":{"position":[0,0,0],"rotation":[1,0,0,0]}}]})");
    writeText(unit.dir / "loop_b.json", R"({"format":"bendcore-assembly","version":2,"name":"B","definitions":[{"id":1,"kind":"assembly","name":"A","source":"loop_a.json"}],"occurrences":[{"id":1,"name":"A:1","definition":1,"placement":{"position":[0,0,0],"rotation":[1,0,0,0]}}]})");
    AssemblyDocument host;
    auto cyc = host.addSubassemblyFromSource("A", "loop_a.json", opt);
    ASSERT_TRUE(cyc.ok()) << cyc.error().message;  // A загружен; цикл B -> A -> B оборван с диагностикой
    auto direct = AssemblyDocument::loadFile((unit.dir / "loop_a.json").string(), modeling::basic::basicRegistry());
    ASSERT_TRUE(direct.ok());
    bool sawCycle = false;
    for (const SourceDiagnostic& s : direct.value().report.sources)
        if (s.message.find("cycle") != std::string::npos) sawCycle = true;
    EXPECT_TRUE(sawCycle);
}

// Файл версии 1: положения — матрица 3×4, ссылки без сигнатур (привязка при загрузке).
TEST(AssemblyDocumentIo, MigrationFromVersion1) {
    ShaftFiles m("migration");
    const int bushingSide = static_cast<int>(cylinderFace(m.bush, 10));
    const int bushingTop = static_cast<int>(horizontalFace(m.bush, 30, 1));
    const std::string v1 = std::string(R"({"format":"bendcore-assembly","version":1,"name":"Старый",
      "definitions":[{"id":1,"kind":"part","name":"Втулка","source":"parts/bushing.step"},
                     {"id":2,"kind":"part","name":"Вал","source":"parts/shaft.json"}],
      "occurrences":[{"id":1,"name":"Втулка:1","definition":1,"fixed":true,
                       "placement":{"matrix":[1,0,0,0, 0,1,0,0, 0,0,1,0]}},
                     {"id":2,"name":"Вал:1","definition":2,
                       "placement":{"matrix":[0,-1,0,5, 1,0,0,3, 0,0,1,60]}}],
      "mates":[{"id":1,"type":"Concentric","a":{"path":[2],"kind":"face","name":"Shaft:Side"},
                "b":{"path":[1],"kind":"face","index":)") +
                             std::to_string(bushingSide) + R"(},"alignment":"Closest"},
               {"id":2,"type":"Coincident","a":{"path":[2],"kind":"face","name":"Shaft:Bottom"},
                "b":{"path":[1],"kind":"face","index":)" +
                             std::to_string(bushingTop) + R"(},"alignment":"AntiAligned"}]})";
    auto loaded = AssemblyDocument::fromJson(v1, m.options());
    ASSERT_TRUE(loaded.ok()) << loaded.error().message;
    EXPECT_EQ(loaded.value().report.fileVersion, 1);
    EXPECT_TRUE(loaded.value().report.migrated);
    AssemblyDocument& d = loaded.value().document;
    // Поворот 90° вокруг Z из матрицы.
    expectVecNear(occAt(d, 2).placement.applyToDirection(Vec3::UnitX()), Vec3::UnitY(), 1e-12);
    for (const DocumentMate& mt : d.mates()) EXPECT_TRUE(mt.bound[0].signature.valid);
    auto r = d.solve();
    ASSERT_TRUE(r.ok() && r.value().success);
    EXPECT_EQ(r.value().totalDof, 1);
    expectShaftSeated(d, 2);
    auto resaved = d.toJson().value();
    EXPECT_NE(resaved.find("\"version\": 2"), std::string::npos);
    auto back = AssemblyDocument::fromJson(resaved, m.options());
    ASSERT_TRUE(back.ok());
    EXPECT_FALSE(back.value().report.migrated);
    expectSamePlacements(d, back.value().document, 1e-14);
}

TEST(AssemblyDocumentIo, FormatErrors) {
    EXPECT_EQ(AssemblyDocument::fromJson("{not json", {}).error().code, ErrorCode::ParseError);
    EXPECT_EQ(AssemblyDocument::fromJson(R"({"format":"bendcore-part","version":2})", {}).error().code,
              ErrorCode::ParseError);
    EXPECT_EQ(AssemblyDocument::fromJson(R"({"format":"bendcore-assembly","version":99})", {}).error().code,
              ErrorCode::Unsupported);
    EXPECT_EQ(AssemblyDocument::fromJson(R"({"format":"bendcore-assembly","version":2,"definitions":[]})", {})
                  .error()
                  .code,
              ErrorCode::ParseError);  // нет occurrences
    // Деталь без источника и без геометрии.
    auto e = AssemblyDocument::fromJson(
        R"({"format":"bendcore-assembly","version":2,"definitions":[{"id":1,"kind":"part","name":"P"}],"occurrences":[]})", {});
    ASSERT_FALSE(e.ok());
    EXPECT_NE(e.error().message.find("neither a source nor embedded geometry"), std::string::npos);
    // Неизвестное определение у вхождения, повтор ID.
    EXPECT_EQ(AssemblyDocument::fromJson(
                  R"({"format":"bendcore-assembly","version":2,"definitions":[],"occurrences":[{"id":1,"name":"a","definition":5,"placement":{"position":[0,0,0],"rotation":[1,0,0,0]}}]})",
                  {})
                  .error()
                  .code,
              ErrorCode::ParseError);
    EXPECT_EQ(AssemblyDocument::loadFile("/nonexistent/dir/a.json", nullptr).error().code, ErrorCode::IoError);
}

// Обозначение и наименование деталей, позиционное обозначение вхождения через STEP.
TEST(AssemblyDocumentIo, StepCarriesDesignationsAndNames) {
    ShaftFiles m("step_props");
    ASSERT_TRUE(m.doc.setProperties(m.doc.root(), {"СБ-100", "Узел вала", "", std::nullopt}).ok());
    for (auto schema : {bendcore::kernel::StepSchema::AP214, bendcore::kernel::StepSchema::AP242}) {
        auto step = m.doc.exportStep({schema, bendcore::kernel::StepLengthUnit::Millimetre});
        ASSERT_TRUE(step.ok()) << step.error().message;
        auto back = AssemblyDocument::importStep(step.value());
        ASSERT_TRUE(back.ok()) << back.error().message;
        const AssemblyDocument& d = back.value();
        EXPECT_EQ(d.name(), "Вал во втулке");
        EXPECT_EQ(d.definition(d.root()).properties.designation, "СБ-100");
        EXPECT_EQ(d.definition(d.root()).properties.name, "Узел вала");
        ASSERT_EQ(d.topLevel().size(), 2u);
        const Definition& bush = d.definition(d.topLevel()[0].definition);
        EXPECT_EQ(bush.name, "Втулка");
        EXPECT_EQ(bush.properties.designation, "ВТ-001");
        EXPECT_EQ(bush.properties.name, "Втулка бронзовая");
        EXPECT_EQ(d.topLevel()[1].designation, "Поз. 2");
        EXPECT_EQ(d.topLevel()[0].designation, "");
        EXPECT_EQ(d.definition(d.topLevel()[1].definition).properties.designation, "ВЛ-002");
        expectSamePlacements(m.doc, d, 1e-9);
    }
    // Подавленное вхождение не экспортируется.
    ASSERT_TRUE(m.doc.setOccurrenceSuppressed(m.s, true).ok());
    auto back = AssemblyDocument::importStep(m.doc.exportStep().value());
    ASSERT_TRUE(back.ok());
    EXPECT_EQ(back.value().topLevel().size(), 1u);
}
