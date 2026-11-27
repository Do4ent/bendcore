// Исполнения сборки (docs/assembly/ASSEMBLY_DOCUMENT.md, раздел 8.2) на редуктор-подобной сборке:
// корпус с отверстиями, вал-исполнение документа детали, два вида болтов. Масса, BOM,
// степени свободы и число пересечений каждого исполнения сверяются с независимым расчётом по
// формулам; переключение туда и обратно, файл, CSV, отмена, вложенная подсборка с исполнением.

#include "assembly_document_helpers.hpp"
#include "../modeling/modeling_document_test_util.hpp"

#include <bendcore/assembly_v2/bom.hpp>
#include <bendcore/assembly_v2/history.hpp>
#include <bendcore/assembly_v2/interference.hpp>
#include <bendcore/modeling/part_features.hpp>

#include <filesystem>
#include <fstream>

using namespace bendcore::assembly_v2;
using namespace bendcore::assembly_v2::testing;
using bendcore::kernel::ErrorCode;
namespace fs = std::filesystem;
namespace m = bendcore::modeling;
namespace tu = bendcore::modeling::testutil;

namespace {

constexpr double kSteel = 7850;  // steel_s235 (материал вала)
constexpr double kIron = 7200;   // корпус
constexpr double kBolt = 7850;   // болты

double kg(double mm3, double rho) { return mm3 * 1e-9 * rho; }

// Корпус 100×60×20: центральное отверстие r = 10 в (50, 30), четыре r = 5 под болты.
const std::array<std::array<double, 2>, 4> kHoles = {{{20, 15}, {80, 45}, {20, 45}, {80, 15}}};
double housingVolume() { return 100.0 * 60 * 20 - kPi * 100 * 20 - 4 * kPi * 25 * 20; }
TopoDS_Shape housingShape() {
    TopoDS_Shape s = cut(box(0, 0, 0, 100, 60, 20), cylinder(50, 30, -1, 10, 22));
    for (const auto& h : kHoles) s = cut(s, cylinder(h[0], h[1], -1, 5, 22));
    return s;
}
// Болт: стержень r = 5 длины L, головка r = 8 высоты 6.
double boltVolume(double length) { return kPi * 25 * length + kPi * 64 * 6; }
// Вал — цилиндр R × L.
double shaftVolume(double r, double l) { return kPi * r * r * l; }

// Документ детали вала с таблицей параметров: базовый R10×60, «-01» R10×80, «-02» R10.5×60.
fs::path writeShaft(const std::string& name) {
    const fs::path dir = fs::path(BENDCORE_ASSEMBLY_TEST_TMP) / name;
    fs::create_directories(dir);
    m::PartDocument doc(m::partRegistry());
    EXPECT_TRUE(doc.setVariable("R", "10").ok());
    EXPECT_TRUE(doc.setVariable("L", "60").ok());
    tu::add(doc, {"Cylinder", "Shaft", "", {{"radius", "R"}, {"height", "L"}}, {}, {}, false});
    m::ConfigurationTable t;
    t.columns = {{m::ConfigurationColumnKind::Variable, "R"},
                 {m::ConfigurationColumnKind::Variable, "L"},
                 {m::ConfigurationColumnKind::Material, ""}};
    t.rows = {{"РЦ.100.00.001", "Вал", {"10", "60", "steel_s235"}},
              {"-01", "Вал удлинённый", {"10", "80", "steel_s235"}},
              {"-02", "Вал утолщённый", {"10.5", "60", "steel_s235"}}};
    t.active = "РЦ.100.00.001";
    EXPECT_TRUE(doc.setConfigurationTable(t).ok());
    doc.rebuild();
    std::ofstream(dir / "shaft.json") << doc.toJson().value();
    return dir;
}

struct Gearbox {
    fs::path dir;
    LoadOptions options;
    AssemblyDocument doc{"Редуктор"};
    OccurrenceId housing = 0, shaft = 0;
    std::array<OccurrenceId, 2> boltA{};
    std::array<OccurrenceId, 4> boltB{};
    MateId outreach = 0;  // Distance: верх вала над верхом корпуса
    std::vector<MateId> boltBMates;
};

GeometryRef named(OccurrenceId o, const std::string& name) {
    return {{o}, ElementRef::named(bendcore::modeling::ElementKind::Face, name), EntityUse::Auto};
}

// Болт в отверстие h: соосно и головкой на верх корпуса.
std::vector<MateId> mateBolt(AssemblyDocument& doc, OccurrenceId housing, OccurrenceId bolt, const TopoDS_Shape& boltShape,
                             const TopoDS_Shape& housing0, std::size_t h) {
    std::vector<MateId> ids;
    auto c = doc.addMate(MateSpec::concentric(faceRef(bolt, cylinderFace(boltShape, 5)),
                                              faceRef(housing, cylinderFace(housing0, 5, kHoles[h][0], kHoles[h][1]))));
    EXPECT_TRUE(c.ok()) << c.error().message;
    auto p = doc.addMate(MateSpec::coincident(faceRef(bolt, horizontalFace(boltShape, 0, -1)),
                                              faceRef(housing, horizontalFace(housing0, 20, 1))));
    EXPECT_TRUE(p.ok()) << p.error().message;
    ids.push_back(c.value());
    ids.push_back(p.value());
    return ids;
}

std::unique_ptr<Gearbox> makeGearbox(const std::string& name) {
    auto g = std::make_unique<Gearbox>();
    g->dir = writeShaft(name);
    g->options = fileLoadOptions(m::partRegistry(), g->dir.string());
    AssemblyDocument& doc = g->doc;
    PartProperties rp;
    rp.designation = "РЦ.100.00.000";
    rp.name = "Редуктор";
    EXPECT_TRUE(doc.setProperties(doc.root(), rp).ok());

    const TopoDS_Shape housing0 = housingShape();
    PartProperties hp{"РЦ.100.00.002", "Корпус", "СЧ20", kIron, std::nullopt};
    const DefinitionId housing = doc.addPart("Корпус", PartData{housing0, nullptr, std::nullopt}, hp).value();
    g->housing = doc.addOccurrence(housing, Transform::identity(), "Корпус").value();
    EXPECT_TRUE(doc.setFixed(g->housing, true).ok());

    auto shaft = doc.addPartFromSource("Вал", "shaft.json", g->options.parts);
    EXPECT_TRUE(shaft.ok()) << shaft.error().message;
    g->shaft = doc.addOccurrence(shaft.value(), Transform::fromTranslation(Vec3(50, 30, -20)), "Вал").value();
    EXPECT_TRUE(doc.addMate(MateSpec::concentric(named(g->shaft, "Shaft:Side"),
                                                 faceRef(g->housing, cylinderFace(housing0, 10, 50, 30))))
                    .ok());
    MateSpec d = MateSpec::distance(named(g->shaft, "Shaft:Top"), faceRef(g->housing, horizontalFace(housing0, 20, 1)), 20);
    d.name = "Вылет вала";
    auto dm = doc.addMate(d);
    EXPECT_TRUE(dm.ok()) << dm.error().message;
    g->outreach = dm.value();

    const TopoDS_Shape a = bolt(5, 30, 8, 6), b = bolt(5, 40, 8, 6);
    const DefinitionId boltA = doc.addPart("Болт М10×30", PartData{a, nullptr, std::nullopt},
                                           {"", "Болт М10×30", "Сталь 35", kBolt, std::nullopt}).value();
    const DefinitionId boltB = doc.addPart("Болт М10×40", PartData{b, nullptr, std::nullopt},
                                           {"", "Болт М10×40", "Сталь 35", kBolt, std::nullopt}).value();
    for (std::size_t i = 0; i < 2; ++i) {
        const std::size_t h = i == 0 ? 0 : 1;
        g->boltA[i] = doc.addOccurrence(boltA, Transform::fromTranslation(Vec3(kHoles[h][0], kHoles[h][1], 20)),
                                        "A" + std::to_string(i + 1)).value();
        mateBolt(doc, g->housing, g->boltA[i], a, housing0, h);
    }
    for (std::size_t i = 0; i < 4; ++i) {
        g->boltB[i] = doc.addOccurrence(boltB, Transform::fromTranslation(Vec3(kHoles[i][0], kHoles[i][1], 25)),
                                        "B" + std::to_string(i + 1)).value();
        for (MateId id : mateBolt(doc, g->housing, g->boltB[i], b, housing0, i)) g->boltBMates.push_back(id);
    }

    // Базовое состояние документа (без исполнения): болты М10×40 исключены.
    for (OccurrenceId id : g->boltB) EXPECT_TRUE(doc.setOccurrenceSuppressed(id, true).ok());
    AssemblyConfigurationTable t;
    for (OccurrenceId id : g->boltA) t.columns.push_back({AssemblyColumnKind::Suppression, id});
    for (OccurrenceId id : g->boltB) t.columns.push_back({AssemblyColumnKind::Suppression, id});
    t.columns.push_back({AssemblyColumnKind::Configuration, g->shaft});
    t.columns.push_back({AssemblyColumnKind::MateValue, g->outreach});
    t.rows = {{"РЦ.100.00.000", "Редуктор", {"0", "0", "1", "1", "1", "1", "#", "20"}},
              {"-01", "Редуктор с удлинённым валом", {"1", "1", "0", "0", "0", "0", "-01", "40"}},
              {"-02", "Редуктор, вал с натягом", {"", "", "", "", "", "", "-02", ""}}};
    t.active = "РЦ.100.00.000";
    auto s = doc.setConfigurationTable(t, g->options);
    EXPECT_TRUE(s.ok()) << s.error().message;
    return g;
}

int interferences(const AssemblyDocument& doc, double* volume = nullptr) {
    auto r = checkInterference(doc);
    EXPECT_TRUE(r.ok()) << r.error().message;
    int n = 0;
    double v = 0;
    for (const InterferencePair& p : r.value().pairs)
        if (p.kind == ContactKind::Interference) {
            ++n;
            v += p.volume;
        }
    if (volume) *volume = v;
    return n;
}

const BomRow* rowNamed(const Bom& bom, const std::string& name) {
    for (const BomRow& r : bom.rows)
        if (r.name == name) return &r;
    return nullptr;
}

std::map<OccurrenceId, Transform> placements(const AssemblyDocument& doc) {
    std::map<OccurrenceId, Transform> out;
    for (const Occurrence& o : doc.topLevel()) out[o.id] = o.placement;
    return out;
}

void expectSamePlacements(const std::map<OccurrenceId, Transform>& a, const std::map<OccurrenceId, Transform>& b) {
    ASSERT_EQ(a.size(), b.size());
    for (const auto& [id, t] : a) {
        const Transform& u = b.at(id);
        EXPECT_EQ(t.position, u.position) << "occurrence " << id;
        EXPECT_EQ(t.rotation.coeffs(), u.rotation.coeffs()) << "occurrence " << id;
    }
}

}  // namespace

TEST(AssemblyConfigurations, GearboxMassBomDofInterferencePerConfiguration) {
    auto g = makeGearbox("gearbox_configurations");
    AssemblyDocument& doc = g->doc;
    const double housingKg = kg(housingVolume(), kIron);

    // ---- базовое исполнение: два болта М10×30, вал R10×60 с вылетом 20.
    auto base = doc.activateConfiguration("РЦ.100.00.000", g->options);
    ASSERT_TRUE(base.ok()) << base.error().message;
    EXPECT_EQ(base.value().fullDesignation, "РЦ.100.00.000");
    ASSERT_TRUE(base.value().solve.has_value());
    EXPECT_TRUE(base.value().solve->success);
    EXPECT_LT(base.value().solve->residualNorm, 1e-9);
    // Вращение вала и двух болтов вокруг своих осей.
    EXPECT_EQ(base.value().solve->totalDof, 3);
    EXPECT_EQ(base.value().excluded.size(), 4u);
    // Сопряжения четырёх исключённых болтов отключены — без ошибки, в отчёте.
    EXPECT_EQ(base.value().disabledMates.size(), 8u);
    for (const auto& [mate, occ] : base.value().disabledMates) {
        EXPECT_NE(std::find(g->boltBMates.begin(), g->boltBMates.end(), mate), g->boltBMates.end());
        EXPECT_NE(std::find(g->boltB.begin(), g->boltB.end(), occ), g->boltB.end());
    }
    EXPECT_EQ(base.value().solve->suppressedMates.size(), 8u);
    EXPECT_TRUE(base.value().solve->brokenMates.empty());
    auto bom = flatBom(doc);
    ASSERT_TRUE(bom.ok()) << bom.error().message;
    ASSERT_EQ(bom.value().rows.size(), 3u);
    EXPECT_EQ(rowNamed(bom.value(), "Болт М10×30")->quantity, 2);
    EXPECT_EQ(rowNamed(bom.value(), "Вал")->quantity, 1);
    EXPECT_EQ(rowNamed(bom.value(), "Болт М10×40"), nullptr);
    EXPECT_EQ(bom.value().totalParts, 4);
    const double baseKg = housingKg + kg(shaftVolume(10, 60), kSteel) + 2 * kg(boltVolume(30), kBolt);
    EXPECT_NEAR(*bom.value().totalMass, baseKg, 1e-9);
    // Вал и болты касаются стенок отверстий; пересечений нет.
    EXPECT_EQ(interferences(doc), 0);
    auto top = doc.worldEntity(named(g->shaft, "Shaft:Top"));
    ASSERT_TRUE(top.ok());
    EXPECT_NEAR(top.value().point.z(), 40.0, 1e-9);

    // ---- «-01»: четыре болта М10×40, вал «-01» (L = 80), вылет 40.
    auto v01 = doc.activateConfiguration("-01", g->options);
    ASSERT_TRUE(v01.ok()) << v01.error().message;
    EXPECT_EQ(v01.value().fullDesignation, "РЦ.100.00.000-01");
    EXPECT_TRUE(v01.value().solve->success);
    EXPECT_LT(v01.value().solve->residualNorm, 1e-9);
    EXPECT_EQ(v01.value().solve->totalDof, 5);
    EXPECT_EQ(v01.value().disabledMates.size(), 4u);
    ASSERT_EQ(v01.value().replaced.size(), 1u);
    EXPECT_EQ(v01.value().replaced[0].first, g->shaft);
    EXPECT_TRUE(v01.value().replaced[0].second.broken.empty());
    EXPECT_EQ(doc.definition(doc.findOccurrence(g->shaft)->definition).source, "shaft.json#-01");
    bom = flatBom(doc);
    ASSERT_TRUE(bom.ok());
    EXPECT_EQ(rowNamed(bom.value(), "Болт М10×40")->quantity, 4);
    EXPECT_EQ(rowNamed(bom.value(), "Болт М10×30"), nullptr);
    EXPECT_EQ(rowNamed(bom.value(), "Вал удлинённый")->designation, "РЦ.100.00.001-01");
    EXPECT_EQ(bom.value().totalParts, 6);
    EXPECT_NEAR(*bom.value().totalMass, housingKg + kg(shaftVolume(10, 80), kSteel) + 4 * kg(boltVolume(40), kBolt), 1e-9);
    EXPECT_EQ(interferences(doc), 0);
    top = doc.worldEntity(named(g->shaft, "Shaft:Top"));
    EXPECT_NEAR(top.value().point.z(), 60.0, 1e-9);

    // ---- «-02»: пустые ячейки — базовые значения, вал R10.5 в отверстии R10 — натяг.
    auto v02 = doc.activateConfiguration("-02", g->options);
    ASSERT_TRUE(v02.ok()) << v02.error().message;
    EXPECT_EQ(v02.value().solve->totalDof, 3);
    bom = flatBom(doc);
    EXPECT_EQ(rowNamed(bom.value(), "Болт М10×30")->quantity, 2);
    EXPECT_NEAR(*bom.value().totalMass, housingKg + kg(shaftVolume(10.5, 60), kSteel) + 2 * kg(boltVolume(30), kBolt), 1e-9);
    double volume = 0;
    EXPECT_EQ(interferences(doc, &volume), 1);
    EXPECT_NEAR(volume, kPi * (10.5 * 10.5 - 100) * 20, 1e-6 * volume);
    top = doc.worldEntity(named(g->shaft, "Shaft:Top"));
    EXPECT_NEAR(top.value().point.z(), 40.0, 1e-9);

    // Состав всех исполнений сразу (групповая спецификация).
    auto all = configurationBom(doc, g->options);
    ASSERT_TRUE(all.ok()) << all.error().message;
    ASSERT_EQ(all.value().configurations.size(), 3u);
    EXPECT_EQ(all.value().configurations[1].fullDesignation, "РЦ.100.00.000-01");
    EXPECT_EQ(all.value().configurations[2].name, "Редуктор, вал с натягом");
    EXPECT_NEAR(*all.value().configurations[0].totalMass, baseKg, 1e-9);
    EXPECT_EQ(all.value().configurations[1].totalParts, 6);
    auto quantities = [&](const std::string& name) {
        for (const ConfigurationBomRow& r : all.value().rows)
            if (r.name == name) return r.quantities;
        return std::vector<int>{};
    };
    EXPECT_EQ(quantities("Корпус"), (std::vector<int>{1, 1, 1}));
    EXPECT_EQ(quantities("Болт М10×30"), (std::vector<int>{2, 0, 2}));
    EXPECT_EQ(quantities("Болт М10×40"), (std::vector<int>{0, 4, 0}));
    EXPECT_EQ(quantities("Вал"), (std::vector<int>{1, 0, 0}));
    EXPECT_EQ(quantities("Вал удлинённый"), (std::vector<int>{0, 1, 0}));
    EXPECT_EQ(quantities("Вал утолщённый"), (std::vector<int>{0, 0, 1}));
    EXPECT_EQ(all.value().rows.size(), 6u);
    // Документ не изменился: активное исполнение то же.
    EXPECT_EQ(doc.activeConfiguration(), "-02");
}

TEST(AssemblyConfigurations, SwitchingBackAndForthGivesSamePlacements) {
    auto g = makeGearbox("gearbox_switching");
    AssemblyDocument& doc = g->doc;
    ASSERT_TRUE(doc.solve().ok());
    const auto base = placements(doc);
    const auto table = doc.configurationTable();
    ASSERT_TRUE(doc.activateConfiguration("-01", g->options).ok());
    const auto v01 = placements(doc);
    EXPECT_NE(doc.findOccurrence(g->boltA[0])->suppressed, false);
    ASSERT_TRUE(doc.activateConfiguration("РЦ.100.00.000", g->options).ok());
    expectSamePlacements(base, placements(doc));
    ASSERT_TRUE(doc.activateConfiguration("-01", g->options).ok());
    expectSamePlacements(v01, placements(doc));
    // Без исполнения — базовые значения (те же, что у первой строки).
    ASSERT_TRUE(doc.activateConfiguration("", g->options).ok());
    EXPECT_FALSE(doc.findOccurrence(g->boltA[0])->suppressed);
    EXPECT_TRUE(doc.findOccurrence(g->boltB[0])->suppressed);
    EXPECT_EQ(doc.findMate(g->outreach)->spec.value, 20.0);
    EXPECT_EQ(doc.definition(doc.findOccurrence(g->shaft)->definition).source, "shaft.json");
    // Таблица после переключений — та же.
    ASSERT_TRUE(doc.activateConfiguration("РЦ.100.00.000", g->options).ok());
    EXPECT_EQ(doc.configurationTable(), table);

    // Правка при активном исполнении пишет его ячейку и сохраняется при переключении.
    ASSERT_TRUE(doc.activateConfiguration("-01", g->options).ok());
    MateSpec spec = doc.findMate(g->outreach)->spec;
    spec.value = 45;
    ASSERT_TRUE(doc.editMate(g->outreach, spec).ok());
    const auto edited = doc.configurationTable();
    EXPECT_EQ(edited.rows[1].cells[7], "45");
    EXPECT_EQ(edited.rows[0].cells[7], "20");
    ASSERT_TRUE(doc.activateConfiguration("-02", g->options).ok());
    EXPECT_EQ(doc.findMate(g->outreach)->spec.value, 20.0) << "-02 takes the base value";
    ASSERT_TRUE(doc.activateConfiguration("-01", g->options).ok());
    EXPECT_EQ(doc.findMate(g->outreach)->spec.value, 45.0);

    // Ошибки — явные, документ не меняется.
    const auto before = placements(doc);
    auto none = doc.activateConfiguration("-07", g->options);
    ASSERT_FALSE(none.ok());
    EXPECT_NE(none.error().message.find("no configuration '-07'"), std::string::npos);
    AssemblyConfigurationTable bad = doc.configurationTable();
    bad.rows[2].cells[6] = "-09";
    bad.active = "-02";
    auto s = doc.setConfigurationTable(bad, g->options);
    ASSERT_FALSE(s.ok());
    EXPECT_NE(s.error().message.find("-09"), std::string::npos) << s.error().message;
    EXPECT_EQ(doc.activeConfiguration(), "-01");
    expectSamePlacements(before, placements(doc));
    bad = doc.configurationTable();
    bad.rows[0].cells[0] = "maybe";
    EXPECT_NE(doc.setConfigurationTable(bad, g->options).error().message.find("invalid value 'maybe'"), std::string::npos);
    bad = doc.configurationTable();
    bad.columns.push_back({AssemblyColumnKind::Configuration, g->housing});
    for (auto& r : bad.rows) r.cells.push_back("");
    EXPECT_NE(doc.setConfigurationTable(bad, g->options).error().message.find("no source document"), std::string::npos);
    bad = doc.configurationTable();
    bad.rows[1].designation = bad.rows[0].designation;
    EXPECT_NE(doc.setConfigurationTable(bad, g->options).error().message.find("repeated designation"), std::string::npos);

    // Удаление вхождения удаляет его столбец.
    ASSERT_TRUE(doc.removeOccurrence(g->boltB[3]).ok());
    EXPECT_EQ(doc.configurationTable().columns.size(), 7u);
}

TEST(AssemblyConfigurations, FileRoundTripAndOldVersions) {
    auto g = makeGearbox("gearbox_file");
    AssemblyDocument& doc = g->doc;
    ASSERT_TRUE(doc.activateConfiguration("РЦ.100.00.000", g->options).ok());
    const auto base = placements(doc);
    ASSERT_TRUE(doc.activateConfiguration("-01", g->options).ok());
    const auto v01 = placements(doc);
    const fs::path file = g->dir / "gearbox.json";
    ASSERT_TRUE(doc.saveFile(file.string()).ok());
    auto loaded = AssemblyDocument::loadFile(file.string(), m::partRegistry());
    ASSERT_TRUE(loaded.ok()) << loaded.error().message;
    AssemblyDocument& back = loaded.value().document;
    EXPECT_EQ(loaded.value().report.fileVersion, AssemblyDocument::kFormatVersion);
    EXPECT_EQ(back.configurationTable(), doc.configurationTable());
    EXPECT_EQ(back.configurationBase(), doc.configurationBase());
    EXPECT_EQ(back.activeConfiguration(), "-01");
    expectSamePlacements(v01, placements(back));
    // Положения другого исполнения сохранены в файле.
    ASSERT_TRUE(back.activateConfiguration("РЦ.100.00.000", g->options).ok());
    expectSamePlacements(base, placements(back));
    EXPECT_FALSE(back.findOccurrence(g->boltA[0])->suppressed);

    // Файл без таблицы (v2) читается, таблица пуста.
    AssemblyDocument plain("Plain");
    const DefinitionId p = plain.addPart("P", PartData{box(0, 0, 0, 1, 1, 1), nullptr, std::nullopt}).value();
    plain.addOccurrence(p, Transform::identity());
    std::string text = plain.toJson().value();
    const auto at = text.find("\"version\": " + std::to_string(AssemblyDocument::kFormatVersion));
    ASSERT_NE(at, std::string::npos);
    text.replace(at, 12, "\"version\": 2");
    auto v2 = AssemblyDocument::fromJson(text, {});
    ASSERT_TRUE(v2.ok()) << v2.error().message;
    EXPECT_TRUE(v2.value().report.migrated);
    EXPECT_TRUE(v2.value().document.configurationTable().empty());

    // Повреждённая таблица — ParseError с причиной.
    std::string broken = doc.toJson().value();
    const auto kind = broken.find("\"suppression\"");
    ASSERT_NE(kind, std::string::npos);
    broken.replace(kind, 13, "\"teleport\"");
    auto e = AssemblyDocument::fromJson(broken, g->options);
    ASSERT_FALSE(e.ok());
    EXPECT_EQ(e.error().code, ErrorCode::ParseError);
    EXPECT_NE(e.error().message.find("unknown column kind 'teleport'"), std::string::npos) << e.error().message;
}

TEST(AssemblyConfigurations, CsvRoundTrip) {
    auto g = makeGearbox("gearbox_csv");
    AssemblyDocument& doc = g->doc;
    AssemblyConfigurationTable t = doc.configurationTable();
    t.rows[2].cells[7] = "22.5";
    t.active.clear();
    for (char decimal : {'.', ','}) {
        const std::string csv = configurationTableToCsv(doc, t, decimal);
        EXPECT_NE(csv.find("designation;name;suppress:A1;suppress:A2;suppress:B1"), std::string::npos) << csv;
        EXPECT_NE(csv.find("configuration:Вал;mate:Вылет вала"), std::string::npos) << csv;
        EXPECT_NE(csv.find(decimal == ',' ? "22,5" : "22.5"), std::string::npos);
        auto back = configurationTableFromCsv(doc, csv);
        ASSERT_TRUE(back.ok()) << back.error().message;
        EXPECT_EQ(back.value(), t);
    }
    // Одинаковые имена — по ID; русские заголовки; ошибки разбора.
    ASSERT_TRUE(doc.setOccurrenceName(g->boltB[0], "A1").ok());
    const std::string csv = configurationTableToCsv(doc, t);
    EXPECT_NE(csv.find("suppress:#" + std::to_string(g->boltA[0])), std::string::npos) << csv;
    auto back = configurationTableFromCsv(doc, csv);
    ASSERT_TRUE(back.ok()) << back.error().message;
    EXPECT_EQ(back.value(), t);
    auto ru = configurationTableFromCsv(doc, "Обозначение;Наименование;подавить:B2;исполнение:Вал;сопряжение:Вылет вала\r\n"
                                             "-05;Опыт;да;-01;12,5\r\n");
    ASSERT_TRUE(ru.ok()) << ru.error().message;
    ASSERT_EQ(ru.value().columns.size(), 3u);
    EXPECT_EQ(ru.value().columns[0], (AssemblyConfigurationColumn{AssemblyColumnKind::Suppression, g->boltB[1]}));
    EXPECT_EQ(ru.value().rows[0].cells, (std::vector<std::string>{"да", "-01", "12.5"}));
    auto ambiguous = configurationTableFromCsv(doc, "designation;name;suppress:A1\r\n-01;x;1\r\n");
    ASSERT_FALSE(ambiguous.ok());
    EXPECT_NE(ambiguous.error().message.find("use #<id>"), std::string::npos);
    EXPECT_NE(configurationTableFromCsv(doc, "designation;name;suppress:Нет такого\r\n").error().message.find("no occurrence named"),
              std::string::npos);
    EXPECT_NE(configurationTableFromCsv(doc, "designation;name;material\r\n").error().message.find("no material"),
              std::string::npos);
    EXPECT_NE(configurationTableFromCsv(doc, "designation;name;W\r\n").error().message.find("expected 'suppress:"),
              std::string::npos);
    EXPECT_EQ(configurationTableFromCsv(doc, "designation;name;suppress:B2\r\n-01;\"x\r\n").error().code, ErrorCode::ParseError);
}

TEST(AssemblyConfigurations, UndoRestoresCompositionAndPlacements) {
    auto g = makeGearbox("gearbox_undo");
    ASSERT_TRUE(g->doc.solve().ok());
    AssemblyHistory h(g->doc);
    const auto base = placements(h.document());
    auto a = h.activateConfiguration("-01", g->options);
    ASSERT_TRUE(a.ok()) << a.error().message;
    EXPECT_EQ(h.undoLabel(), "Activate configuration");
    EXPECT_TRUE(h.document().findOccurrence(g->boltA[0])->suppressed);
    ASSERT_TRUE(h.undo().ok());
    EXPECT_EQ(h.document().activeConfiguration(), "РЦ.100.00.000");
    EXPECT_FALSE(h.document().findOccurrence(g->boltA[0])->suppressed);
    expectSamePlacements(base, placements(h.document()));
    ASSERT_TRUE(h.redo().ok());
    EXPECT_EQ(h.document().activeConfiguration(), "-01");
    AssemblyConfigurationTable t = h.document().configurationTable();
    t.rows.pop_back();
    ASSERT_TRUE(h.setConfigurationTable(t, g->options).ok());
    EXPECT_EQ(h.document().configurationTable().rows.size(), 2u);
    ASSERT_TRUE(h.undo().ok());
    EXPECT_EQ(h.document().configurationTable().rows.size(), 3u);
}

// Вложенная подсборка с исполнением: узел крепления (планка + два болта, второй исключён в
// базовом исполнении, высота головки над планкой — значение сопряжения).
TEST(AssemblyConfigurations, NestedSubassemblyConfiguration) {
    const fs::path dir = fs::path(BENDCORE_ASSEMBLY_TEST_TMP) / "nested_configuration";
    fs::create_directories(dir);
    const TopoDS_Shape plate0 = cut(cut(box(0, 0, 0, 60, 20, 10), cylinder(15, 10, -1, 5, 12)), cylinder(45, 10, -1, 5, 12));
    const TopoDS_Shape bolt0 = bolt(5, 20, 8, 6);
    MateId lift = 0;
    OccurrenceId bolt1 = 0, bolt2 = 0, plateOcc = 0;
    {
        AssemblyDocument unit("Узел крепления");
        ASSERT_TRUE(unit.setProperties(unit.root(), {"РЦ.300.00.000", "Узел крепления", "", std::nullopt, std::nullopt}).ok());
        const DefinitionId plate = unit.addPart("Планка", PartData{plate0, nullptr, std::nullopt},
                                                {"РЦ.300.00.001", "Планка", "", 7850.0, std::nullopt}).value();
        const DefinitionId b = unit.addPart("Болт", PartData{bolt0, nullptr, std::nullopt},
                                            {"", "Болт М10×20", "", 7850.0, std::nullopt}).value();
        plateOcc = unit.addOccurrence(plate, Transform::identity(), "Планка").value();
        ASSERT_TRUE(unit.setFixed(plateOcc, true).ok());
        bolt1 = unit.addOccurrence(b, Transform::fromTranslation(Vec3(15, 10, 10)), "Болт 1").value();
        bolt2 = unit.addOccurrence(b, Transform::fromTranslation(Vec3(45, 10, 10)), "Болт 2").value();
        ASSERT_TRUE(unit.addMate(MateSpec::concentric(faceRef(bolt1, cylinderFace(bolt0, 5)),
                                                      faceRef(plateOcc, cylinderFace(plate0, 5, 15, 10)))).ok());
        MateSpec d = MateSpec::distance(faceRef(bolt1, horizontalFace(bolt0, 0, -1)), faceRef(plateOcc, horizontalFace(plate0, 10, 1)), 0);
        d.name = "Подъём";
        lift = unit.addMate(d).value();
        ASSERT_TRUE(unit.addMate(MateSpec::concentric(faceRef(bolt2, cylinderFace(bolt0, 5)),
                                                      faceRef(plateOcc, cylinderFace(plate0, 5, 45, 10)))).ok());
        AssemblyConfigurationTable t;
        t.columns = {{AssemblyColumnKind::Suppression, bolt2}, {AssemblyColumnKind::MateValue, lift}};
        t.rows = {{"РЦ.300.00.000", "Узел крепления", {"1", "0"}}, {"-01", "Узел крепления двойной", {"0", "5"}}};
        t.active = "РЦ.300.00.000";
        ASSERT_TRUE(unit.setConfigurationTable(t).ok());
        ASSERT_TRUE(unit.saveFile((dir / "unit.json").string()).ok());
    }
    const LoadOptions options = fileLoadOptions(m::partRegistry(), dir.string());
    AssemblyDocument doc("Стенд");
    const DefinitionId base = doc.addPart("Основание", PartData{box(-50, -50, -10, 200, 200, 10), nullptr, std::nullopt},
                                          {"РЦ.400.00.001", "Основание", "", 7850.0, std::nullopt}).value();
    const OccurrenceId baseOcc = doc.addOccurrence(base, Transform::identity(), "Основание").value();
    ASSERT_TRUE(doc.setFixed(baseOcc, true).ok());
    auto unitDef = doc.addSubassemblyFromSource("", "unit.json", options);
    ASSERT_TRUE(unitDef.ok()) << unitDef.error().message;
    EXPECT_EQ(doc.definition(unitDef.value()).properties.designation, "РЦ.300.00.000");
    const OccurrenceId unit = doc.addOccurrence(unitDef.value(), Transform::identity(), "Узел").value();
    // Вхождение внутри подсборки по его ID в файле узла (sourceId).
    auto inner = [&](OccurrenceId sourceId) {
        for (const Occurrence& o : doc.definition(doc.findOccurrence(unit)->definition).occurrences)
            if (o.sourceId == std::optional<OccurrenceId>(sourceId)) return o.id;
        return OccurrenceId{0};
    };
    const TopoDS_Shape base0 = box(-50, -50, -10, 200, 200, 10);
    // Сопряжение внутрь подсборки: низ планки узла на верх основания.
    auto mate = doc.addMate(MateSpec::coincident(
        GeometryRef{{unit, inner(plateOcc)}, ElementRef::face(horizontalFace(plate0, 0, -1)), EntityUse::Auto},
        faceRef(baseOcc, horizontalFace(base0, 0, 1))));
    ASSERT_TRUE(mate.ok()) << mate.error().message;
    auto flat = flatBom(doc);
    ASSERT_TRUE(flat.ok());
    EXPECT_EQ(rowNamed(flat.value(), "Болт М10×20")->quantity, 1);

    // Исполнение «-01» у вхождения подсборки: состав и решённые положения узла «-01».
    auto rep = doc.setOccurrenceConfiguration(unit, "-01", options);
    ASSERT_TRUE(rep.ok()) << rep.error().message;
    const Definition& d01 = doc.definition(doc.findOccurrence(unit)->definition);
    EXPECT_EQ(d01.source, "unit.json#-01");
    EXPECT_EQ(d01.properties.designation, "РЦ.300.00.000-01");
    EXPECT_EQ(d01.properties.name, "Узел крепления двойной");
    EXPECT_TRUE(doc.isReadOnlyAssembly(d01.id));
    EXPECT_TRUE(std::none_of(rep.value().broken.begin(), rep.value().broken.end(),
                             [&](const auto& b) { return b.first == mate.value(); }))
        << "the mate into the subassembly follows the occurrence by its file ID";
    EXPECT_EQ(doc.findMate(mate.value())->spec.a.path[1], inner(plateOcc));
    flat = flatBom(doc);
    EXPECT_EQ(rowNamed(flat.value(), "Болт М10×20")->quantity, 2);
    EXPECT_NEAR(*flat.value().totalMass,
                kg(200.0 * 200 * 10, 7850) + kg(60.0 * 20 * 10 - 2 * kPi * 25 * 10, 7850) + 2 * kg(kPi * 25 * 20 + kPi * 64 * 6, 7850),
                1e-9);
    ASSERT_TRUE(doc.solve().ok());
    // Болт 1 поднят на 5 мм над планкой (значение сопряжения исполнения «-01» подсборки).
    auto head = doc.worldEntity(GeometryRef{{unit, inner(bolt1)}, ElementRef::face(horizontalFace(bolt0, 0, -1)), EntityUse::Auto});
    ASSERT_TRUE(head.ok()) << head.error().message;
    auto plateTop = doc.worldEntity(GeometryRef{{unit, inner(plateOcc)}, ElementRef::face(horizontalFace(plate0, 10, 1)), EntityUse::Auto});
    EXPECT_NEAR(head.value().point.z() - plateTop.value().point.z(), 5.0, 1e-9);

    // Таблица верхней сборки со столбцом исполнения подсборки; состав по исполнениям; файл.
    AssemblyConfigurationTable t;
    t.columns = {{AssemblyColumnKind::Configuration, unit}};
    t.rows = {{"РЦ.400.00.000", "Стенд", {"#"}}, {"-01", "Стенд двойной", {"-01"}}};
    t.active = "-01";
    ASSERT_TRUE(doc.setConfigurationTable(t, options).ok());
    auto all = configurationBom(doc, options);
    ASSERT_TRUE(all.ok()) << all.error().message;
    for (const ConfigurationBomRow& r : all.value().parts)
        if (r.name == "Болт М10×20") EXPECT_EQ(r.quantities, (std::vector<int>{1, 2}));
    for (const ConfigurationBomRow& r : all.value().rows)
        if (r.kind == DefinitionKind::Assembly) EXPECT_EQ(r.quantities[0] + r.quantities[1], 1) << r.designation;
    ASSERT_TRUE(doc.saveFile((dir / "stand.json").string()).ok());
    auto loaded = AssemblyDocument::loadFile((dir / "stand.json").string(), m::partRegistry());
    ASSERT_TRUE(loaded.ok()) << loaded.error().message;
    for (const SourceDiagnostic& s : loaded.value().report.sources)
        EXPECT_NE(s.status, SourceStatus::AssemblyMissing) << s.source << ": " << s.message;
    const Occurrence* u = loaded.value().document.findOccurrence(unit);
    ASSERT_NE(u, nullptr);
    EXPECT_EQ(loaded.value().document.definition(u->definition).source, "unit.json#-01");
    EXPECT_EQ(rowNamed(flatBom(loaded.value().document).value(), "Болт М10×20")->quantity, 2);
    ASSERT_TRUE(loaded.value().document.activateConfiguration("РЦ.400.00.000", options).ok());
    EXPECT_EQ(rowNamed(flatBom(loaded.value().document).value(), "Болт М10×20")->quantity, 1);

    // Нет такого исполнения у подсборки — явная ошибка.
    auto none = doc.setOccurrenceConfiguration(unit, "-05", options);
    ASSERT_FALSE(none.ok());
    EXPECT_NE(none.error().message.find("no configuration '-05'"), std::string::npos) << none.error().message;
}
