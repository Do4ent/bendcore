// Групповые документы ГОСТ 2.113 (docs/drawing/DRAWING_DOCUMENT.md, «Групповой чертёж»;
// docs/drawing/SPECIFICATION.md, «Групповая спецификация ГОСТ 2.113»):
//  - групповой рабочий чертёж кронштейна с тремя исполнениями (таблица параметров детали):
//    буквы — ровно у различающихся размеров, значения в таблице исполнений = таблица параметров,
//    подавленное отверстие — «нет», обозначение базового исполнения в основной надписи,
//    изменение таблицы параметров → обновлённый чертёж; DXF (ezdxf), PDF (pdftotext);
//  - групповая спецификация сборки с двумя исполнениями: количества по исполнениям =
//    configurationBom = независимый подсчёт по таблице исполнений; листы, комплект PDF/DXF/SVG.

#include "test_drawing_document_support.hpp"

#include <bendcore/assembly_v2/bom.hpp>
#include <bendcore/drawing_v2/annotation_style.hpp>
#include <bendcore/kernel/material.hpp>
#include <bendcore/modeling/part_features.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>

using namespace ddtest;
using bendcore::modeling::ElementKind;
using annotations::AnchorKind;
using annotations::DimensionType;
namespace md = bendcore::modeling;
namespace asm2 = bendcore::assembly_v2;

namespace {

constexpr double kPi = 3.14159265358979323846;

std::string pdfText(const std::string& path) {
#ifdef BENDCORE_PDFTOTEXT
    int code = 0;
    std::string out = dv2test::runCommand(std::string(BENDCORE_PDFTOTEXT) + " -layout '" + path + "' -", &code);
    EXPECT_EQ(code, 0);
    return out;
#else
    (void)path;
    return {};
#endif
}

// Кронштейн (makeBracket: A, B, T, Wd, r, hx, d; отверстие MountHole) с таблицей параметров:
// базовое РД.10.001 (A = 60, d = 8), «-01» (A = 80, d = 10), «-02» (A = 60, d = 8, без
// отверстия, алюминий).
md::ConfigurationTable bracketTable() {
    md::ConfigurationTable t;
    t.columns = {{md::ConfigurationColumnKind::Variable, "A"},
                 {md::ConfigurationColumnKind::Variable, "d"},
                 {md::ConfigurationColumnKind::Suppression, "MountHole"},
                 {md::ConfigurationColumnKind::Material, ""}};
    t.rows = {{"РД.10.001", "Кронштейн", {"60", "8", "0", ""}},
              {"-01", "Кронштейн", {"80", "10", "0", ""}},
              {"-02", "Кронштейн", {"60", "8", "1", "al_6061_t6"}}};
    t.active = "РД.10.001";
    return t;
}

void bracketSheet(DrawingDocument& d, const ModelSnapshot& m) {
    SheetDef s;
    s.id = "S1";
    s.format = {GostFormat::A3, 1, Orientation::Landscape};
    ViewDef f;
    f.id = "F";
    s.views.push_back(f);
    ViewDef t;
    t.id = "T";
    t.kind = ViewKind::Projected;
    t.parent = "F";
    t.side = views::SheetSide::Below;
    s.views.push_back(t);
    auto v = [&](double x, double y, double z) { return anchorOn(m, ElementKind::Vertex, vertexAt(m, {x, y, z}), AnchorKind::Point); };
    s.dimensions.push_back(dimension("A", "F", DimensionType::Horizontal, {v(0, -15, 0), v(60, -15, 0)}, -24));
    s.dimensions.push_back(dimension("B", "F", DimensionType::Vertical, {v(0, -15, 0), v(0, -15, 50)}, 12));
    s.dimensions.push_back(dimension("T", "F", DimensionType::Vertical, {v(60, -15, 0), v(60, -15, 8)}, -12));
    s.dimensions.push_back(dimension("Wd", "T", DimensionType::Vertical, {v(60, -15, 8), v(60, 15, 8)}, -12));
    DimensionItem dd = dimension("d", "T", DimensionType::Diameter,
                                 {anchorOn(m, ElementKind::Edge, circleEdge(m, {40, 0, 8}, 4), AnchorKind::Center)});
    dd.def.leaderAngleDeg = 60;
    s.dimensions.push_back(dd);
    s.dimensions.push_back(dimension("hx", "T", DimensionType::Horizontal,
                                     {v(0, -15, 50), anchorOn(m, ElementKind::Edge, circleEdge(m, {40, 0, 8}, 4), AnchorKind::Center)},
                                     -12));
    s.technicalRequirements = {"Острые кромки притупить."};
    s.title.developer.surname = "Иванов";
    d.sheets().push_back(s);
}

const GroupDimension* groupDim(const GroupDrawingResult& g, const std::string& id) {
    for (const GroupDimension& x : g.dimensions)
        if (x.id == id) return &x;
    return nullptr;
}

int columnOf(const GroupDrawingResult& g, const std::string& key) {
    for (std::size_t i = 0; i < g.columns.size(); ++i)
        if (g.columns[i].key == key) return int(i);
    return -1;
}

double bracketVolume(double A, double d) {
    const double B = 50, T = 8, Wd = 30, r = 4;
    return (A * T + (B - T) * T) * Wd + (1 - kPi / 4) * r * r * Wd - kPi * d * d / 4 * T;
}

}  // namespace

TEST(DrawingDocumentGroup, PartGroupDrawingWithConfigurationTable) {
    auto part = makeBracket();
    ASSERT_TRUE(part->setConfigurationTable(bracketTable()).ok());
    part->rebuild();
    const ModelProperties props{"РД.10.001", "Кронштейн", "Сталь 20 ГОСТ 1050-2013", 7850.0};

    auto snapR = snapshotPartConfigurations(*part, props);
    ASSERT_TRUE(snapR.ok()) << snapR.error().message;
    const ModelSnapshot& snap = snapR.value();
    ASSERT_EQ(snap.configurations.size(), 3u);
    EXPECT_EQ(snap.configurations[1].fullDesignation, "РД.10.001-01");
    ASSERT_EQ(snap.configurationFeatures.size(), 1u);
    EXPECT_EQ(snap.configurationFeatures[0].first, "MountHole");
    EXPECT_EQ(snap.configurations[2].suppressed, std::vector<bool>{true});
    // Документ детали не изменился: активное исполнение и объём базового.
    EXPECT_EQ(part->activeConfiguration(), "РД.10.001");

    DrawingDocument d;
    d.model().kind = ModelKind::Part;
    d.model().properties = props;
    d.settings().group.enabled = true;
    d.settings().group.material = false;  // материал различается — графа всё равно нужна
    bracketSheet(d, snap);
    views::ViewCache cache(32, 0);
    auto rep = d.update(snap, cache);
    ASSERT_TRUE(rep.ok()) << rep.error().message;
    const GroupDrawingResult* g = d.groupDrawing();
    ASSERT_NE(g, nullptr);

    // Буквы — ровно у различающихся размеров: A (60/80/60), d (8/10/нет), hx (40/40/нет).
    std::set<std::string> lettered;
    for (const GroupDimension& x : g->dimensions) lettered.insert(x.id);
    EXPECT_EQ(lettered, (std::set<std::string>{"A", "d", "hx"}));
    EXPECT_EQ(groupDim(*g, "A")->letter, "A");
    EXPECT_EQ(groupDim(*g, "d")->letter, "B");
    EXPECT_EQ(groupDim(*g, "hx")->letter, "C");
    // На изображении — буквы у различающихся размеров, числа у совпадающих.
    EXPECT_EQ(dimensionResult(d, "S1", "A")->text, "A");
    EXPECT_EQ(dimensionResult(d, "S1", "d")->text, "⌀B");
    EXPECT_EQ(dimensionResult(d, "S1", "hx")->text, "C");
    EXPECT_EQ(dimensionResult(d, "S1", "B")->text, "50");
    EXPECT_EQ(dimensionResult(d, "S1", "T")->text, "8");
    EXPECT_EQ(dimensionResult(d, "S1", "Wd")->text, "30");
    // Значения размеров базового исполнения (числа) — у размеров и в отчёте.
    EXPECT_NEAR(dimensionResult(d, "S1", "A")->value, 60.0, 1e-9);

    // Таблица исполнений: значения = таблица параметров детали.
    const md::ConfigurationTable& pt = part->configurationTable();
    ASSERT_EQ(g->table.rows.size(), 3u);
    const int cA = columnOf(*g, "dim:S1/A"), cD = columnOf(*g, "dim:S1/d"), cH = columnOf(*g, "dim:S1/hx");
    const int cHole = columnOf(*g, "suppress:MountHole"), cMass = columnOf(*g, "mass"), cMat = columnOf(*g, "material");
    ASSERT_GE(cA, 0);
    ASSERT_GE(cD, 0);
    ASSERT_GE(cH, 0);
    ASSERT_GE(cHole, 0);
    ASSERT_GE(cMass, 0);
    ASSERT_GE(cMat, 0);
    EXPECT_EQ(columnOf(*g, "designation"), 0);
    EXPECT_EQ(g->table.headers[cA], "A");
    EXPECT_EQ(g->table.headers[cD], "B");
    EXPECT_EQ(g->table.headers[cHole], "MountHole");
    EXPECT_EQ(g->table.headers[cMass], "Масса, кг");
    for (std::size_t r = 0; r < 3; ++r) {
        EXPECT_EQ(g->table.rows[r][0], md::fullDesignation(pt, r));
        EXPECT_EQ(g->table.rows[r][cA], pt.rows[r].cells[0]) << "row " << r;
        const bool holeSuppressed = pt.rows[r].cells[2] == "1";
        EXPECT_EQ(g->table.rows[r][cD], holeSuppressed ? "–" : pt.rows[r].cells[1]) << "row " << r;
        EXPECT_EQ(g->table.rows[r][cH], holeSuppressed ? "–" : "40") << "row " << r;
        EXPECT_EQ(g->table.rows[r][cHole], holeSuppressed ? "нет" : "есть");
    }
    // Масса исполнения: объём · плотность (сталь 7850, алюминий из библиотеки), материал.
    const double rhoAl = [] {
        for (const auto& m : bendcore::kernel::builtInMaterials())
            if (m.id == "al_6061_t6") return m.density;
        return 0.0;
    }();
    ASSERT_GT(rhoAl, 0.0);
    EXPECT_EQ(g->table.rows[0][cMass], annotations::formatNumber(bracketVolume(60, 8) * 1e-9 * 7850, 3));
    EXPECT_EQ(g->table.rows[1][cMass], annotations::formatNumber(bracketVolume(80, 10) * 1e-9 * 7850, 3));
    EXPECT_EQ(g->table.rows[2][cMass], annotations::formatNumber(bracketVolume(60, 0) * 1e-9 * rhoAl, 3));
    EXPECT_EQ(g->table.rows[0][cMat], "Сталь 20 ГОСТ 1050-2013");
    EXPECT_NE(g->table.rows[2][cMat], g->table.rows[0][cMat]);
    // Основная надпись: базовое исполнение, масса и материал — «См. табл.».
    const ComputedSheet& cs = d.computedSheets().front();
    EXPECT_EQ(cs.title.designation, "РД.10.001");
    EXPECT_EQ(cs.title.mass, "См. табл.");
    EXPECT_EQ(cs.title.material, "См. табл.");

    // Заголовок графы — правка пользователя (ключ графы).
    d.settings().group.headers["suppress:MountHole"] = "Отверстие";
    d.settings().group.headers["mass"] = "Масса";
    ASSERT_TRUE(d.update(snap, cache).ok());
    EXPECT_EQ(d.groupDrawing()->table.headers[columnOf(*d.groupDrawing(), "suppress:MountHole")], "Отверстие");
    EXPECT_EQ(d.groupDrawing()->columns[columnOf(*d.groupDrawing(), "mass")].defaultHeader, "Масса, кг");

    // Вывод: DXF (буквы, таблица, ezdxf), PDF (pdftotext).
    const std::string dir = dv2test::outPath("group");
    ExportKitOptions ko;
    ko.directory = dir;
    ko.baseName = "bracket_group";
    ko.font = dv2test::systemFont();  // невидимый текстовый слой PDF для pdftotext
    auto kit = exportKit(d, nullptr, ko);
    ASSERT_TRUE(kit.ok()) << kit.error().message;
    const DxfContent dxf = readDxf(dir + "/bracket_group_S1.dxf");
    for (const char* want : {"РД.10.001", "РД.10.001-01", "РД.10.001-02", "Отверстие", "нет", "есть", "80", "10", "Обозначение"})
        EXPECT_TRUE(hasText(dxf, want)) << "DXF text '" << want << "'";
    if (std::string err = ezdxfCheck(dir + "/bracket_group_S1.dxf", "РД.10.001-02"); !err.empty()) ADD_FAILURE() << err;
    // Таблица над основной надписью: линии слоя TABLE, правый край — правый край основной надписи.
    double maxX = 0;
    std::size_t tableLines = 0;
    for (const auto& l : dxf.lines)
        if (l.layer == "TABLE") {
            ++tableLines;
            maxX = std::max({maxX, l.x0, l.x1});
        }
    EXPECT_GE(tableLines, 4u + 3u);
    EXPECT_NEAR(maxX, 420.0 - 5.0, 1e-6);
    if (const std::string t = ko.font ? pdfText(dir + "/bracket_group.pdf") : std::string(); !t.empty()) {
        for (const char* want : {"РД.10.001-01", "РД.10.001-02", "Отверстие", "нет", "См. табл.", "Масса"})
            EXPECT_NE(t.find(want), std::string::npos) << "pdftotext: '" << want << "' missing";
    }

    // Изменение таблицы параметров: «-01» A = 90 → новый снимок → таблица исполнений обновлена;
    // буквы те же. «-02» без отверстия → с отверстием d = 12: hx у всех 40, но d различается.
    md::ConfigurationTable t2 = part->configurationTable();
    t2.rows[1].cells[0] = "90";
    t2.rows[2].cells[1] = "12";
    t2.rows[2].cells[2] = "0";
    ASSERT_TRUE(part->setConfigurationTable(t2).ok());
    part->rebuild();
    auto snap2 = snapshotPartConfigurations(*part, props);
    ASSERT_TRUE(snap2.ok()) << snap2.error().message;
    auto rep2 = d.update(snap2.value(), cache);
    ASSERT_TRUE(rep2.ok()) << rep2.error().message;
    g = d.groupDrawing();
    lettered.clear();
    for (const GroupDimension& x : g->dimensions) lettered.insert(x.id);
    EXPECT_EQ(lettered, (std::set<std::string>{"A", "d"}));  // hx совпадает во всех исполнениях
    EXPECT_EQ(dimensionResult(d, "S1", "hx")->text, "40");
    EXPECT_EQ(g->table.rows[1][columnOf(*g, "dim:S1/A")], "90");
    EXPECT_EQ(g->table.rows[2][columnOf(*g, "dim:S1/d")], "12");
    EXPECT_EQ(columnOf(*g, "suppress:MountHole"), -1);  // отверстие есть во всех исполнениях

    // Буква, заданная вручную; повтор буквы — ошибка.
    d.sheets()[0].dimensions[4].groupLetter = "D";  // d
    ASSERT_TRUE(d.update(snap2.value(), cache).ok());
    EXPECT_EQ(dimensionResult(d, "S1", "d")->text, "⌀D");
    EXPECT_EQ(groupDim(*d.groupDrawing(), "A")->letter, "A");
    d.sheets()[0].dimensions[0].groupLetter = "D";
    auto dup = d.update(snap2.value(), cache);
    ASSERT_FALSE(dup.ok());
    EXPECT_NE(dup.error().message.find("letter 'D'"), std::string::npos) << dup.error().message;
    d.sheets()[0].dimensions[0].groupLetter.clear();

    // Файл: настройки группы и буква размера сохраняются.
    auto text = d.toJson();
    ASSERT_TRUE(text.ok());
    auto back = DrawingDocument::fromJson(text.value());
    ASSERT_TRUE(back.ok()) << back.error().message;
    EXPECT_TRUE(back.value().settings().group.enabled);
    EXPECT_EQ(back.value().settings().group.headers.at("suppress:MountHole"), "Отверстие");
    EXPECT_EQ(back.value().sheets()[0].dimensions[4].groupLetter, "D");
    EXPECT_EQ(back.value().toJson().value(), text.value());

    // Режим выключен — обычный чертёж: числа, обозначение без таблицы исполнений.
    d.settings().group.enabled = false;
    ASSERT_TRUE(d.update(snap2.value(), cache).ok());
    EXPECT_EQ(d.groupDrawing(), nullptr);
    EXPECT_EQ(dimensionResult(d, "S1", "A")->text, "60");
}

TEST(DrawingDocumentGroup, LoaderTakesConfigurationsOnlyInGroupMode) {
    auto part = makeBracket();
    ASSERT_TRUE(part->setConfigurationTable(bracketTable()).ok());
    part->rebuild();
    const std::string dir = dv2test::outPath("group_loader");
    std::filesystem::create_directories(dir);
    {
        std::ofstream out(dir + "/bracket.json");
        out << part->toJson().value();
    }
    DrawingDocument d;
    d.model().kind = ModelKind::Part;
    d.model().path = "bracket.json";
    d.model().properties = {"РД.10.001", "Кронштейн", "Сталь 20", 7850.0};
    auto first = snapshotPart(*part, d.model().properties);
    ASSERT_TRUE(first.ok());
    bracketSheet(d, first.value());
    views::ViewCache cache(32, 0);
    const ModelLoader loader = fileModelLoader(dir);
    auto plainRep = d.update(loader, cache);
    ASSERT_TRUE(plainRep.ok()) << plainRep.error().message;
    EXPECT_EQ(d.groupDrawing(), nullptr);
    d.settings().group.enabled = true;
    auto rep = d.update(loader, cache);
    ASSERT_TRUE(rep.ok()) << rep.error().message;
    ASSERT_NE(d.groupDrawing(), nullptr);
    EXPECT_EQ(d.groupDrawing()->designations.size(), 3u);
    EXPECT_EQ(dimensionResult(d, "S1", "A")->text, "A");

    // Деталь без таблицы параметров — предупреждение, обычный чертёж.
    auto plain = makeBracket();
    auto snap = snapshotPartConfigurations(*plain, d.model().properties);
    ASSERT_TRUE(snap.ok());
    EXPECT_TRUE(snap.value().configurations.empty());
    auto rep2 = d.update(snap.value(), cache);
    ASSERT_TRUE(rep2.ok()) << rep2.error().message;
    EXPECT_EQ(d.groupDrawing(), nullptr);
    bool warned = false;
    for (const std::string& w : rep2.value().warnings) warned = warned || w.find("no configuration table") != std::string::npos;
    EXPECT_TRUE(warned);
}

namespace {

// Сборка «Рамка» РЦ.200.00.000 с двумя исполнениями: основание РЦ.200.00.001 ×1, планка
// РЦ.080.00.010 ×2 (у второго вхождения в «-01» — исполнение «-01» планки), штифт ×4 (в «-01»
// два исключены).
struct Frame {
    std::string plankJson, pinJson, baseJson;
    asm2::PartResolver resolver;
    asm2::LoadOptions options;
    std::shared_ptr<asm2::AssemblyDocument> doc = std::make_shared<asm2::AssemblyDocument>("Рамка");
    std::vector<asm2::OccurrenceId> occ;  // основание, планка 1, планка 2, штифты 1–4
};

std::string partJson(const std::string& type, std::map<std::string, std::string> params, const md::ConfigurationTable* table) {
    md::PartDocument p(md::partRegistry());
    if (table) EXPECT_TRUE(p.setVariable("W", "60").ok());
    EXPECT_TRUE(p.addFeature({type, "Body", "", std::move(params), {}, {}, false}).ok());
    if (table) EXPECT_TRUE(p.setConfigurationTable(*table).ok());
    p.rebuild();
    return p.toJson().value();
}

std::unique_ptr<Frame> makeFrame() {
    auto f = std::make_unique<Frame>();
    md::ConfigurationTable t;
    t.columns = {{md::ConfigurationColumnKind::Variable, "W"}};
    t.rows = {{"РЦ.080.00.010", "Планка", {"60"}}, {"-01", "Планка широкая", {"80"}}};
    t.active = "РЦ.080.00.010";
    f->plankJson = partJson("Box", {{"length", "W"}, {"width", "40"}, {"height", "10"}}, &t);
    f->pinJson = partJson("Cylinder", {{"radius", "3"}, {"height", "20"}}, nullptr);
    f->baseJson = partJson("Box", {{"length", "120"}, {"width", "60"}, {"height", "8"}}, nullptr);
    Frame* fp = f.get();
    f->resolver = [fp](const std::string& source) -> bendcore::kernel::Result<asm2::PartData> {
        const auto [path, configuration] = asm2::splitConfigurationSource(source);
        const std::string* text = path == "plank.json" ? &fp->plankJson
                                  : path == "pin.json" ? &fp->pinJson
                                  : path == "base.json" ? &fp->baseJson
                                                        : nullptr;
        if (!text) return bendcore::kernel::Error{bendcore::kernel::ErrorCode::IoError, "no " + path};
        auto doc = md::PartDocument::fromJson(*text, md::partRegistry());
        if (!doc) return doc.error();
        if (configuration.empty()) return asm2::partDataFromDocument(doc.value());
        return asm2::partDataFromDocument(doc.value(), configuration);
    };
    f->options.parts = f->resolver;
    auto& doc = *f->doc;
    EXPECT_TRUE(doc.setProperties(doc.root(), {"РЦ.200.00.000", "Рамка", "", std::nullopt}).ok());
    const auto base = doc.addPartFromSource("Основание", "base.json", f->resolver, {"РЦ.200.00.001", "Основание", "", std::nullopt}).value();
    const auto plank = doc.addPartFromSource("Планка", "plank.json", f->resolver).value();
    const auto pin = doc.addPartFromSource("Штифт", "pin.json", f->resolver, {"", "Штифт 6×20 ГОСТ 3128-70", "", std::nullopt}).value();
    using asm2::Transform;
    f->occ.push_back(doc.addOccurrence(base, Transform::identity()).value());
    f->occ.push_back(doc.addOccurrence(plank, Transform::fromTranslation({0, 0, 8})).value());
    f->occ.push_back(doc.addOccurrence(plank, Transform::fromTranslation({0, 0, 18})).value());
    for (int i = 0; i < 4; ++i) f->occ.push_back(doc.addOccurrence(pin, Transform::fromTranslation({10.0 + 25 * i, 50, 8})).value());
    asm2::AssemblyConfigurationTable at;
    at.columns = {{asm2::AssemblyColumnKind::Configuration, f->occ[2]},
                  {asm2::AssemblyColumnKind::Suppression, f->occ[5]},
                  {asm2::AssemblyColumnKind::Suppression, f->occ[6]}};
    at.rows = {{"РЦ.200.00.000", "Рамка", {"", "0", "0"}}, {"-01", "Рамка усиленная", {"-01", "1", "1"}}};
    at.active = "РЦ.200.00.000";
    auto st = doc.setConfigurationTable(at, f->options);
    EXPECT_TRUE(st.ok()) << (st.ok() ? "" : st.error().message);
    return f;
}

// Независимый подсчёт по таблице исполнений сборки: для каждого исполнения — вхождения, не
// исключённые ячейкой подавления, по полному обозначению (или наименованию) детали.
std::vector<std::map<std::string, int>> countByTable(const Frame& f) {
    const asm2::AssemblyConfigurationTable t = f.doc->configurationTable();
    std::vector<std::map<std::string, int>> out(t.rows.size());
    const std::vector<std::string> keyOf = {"РЦ.200.00.001", "РЦ.080.00.010", "РЦ.080.00.010", "Штифт 6×20 ГОСТ 3128-70",
                                            "Штифт 6×20 ГОСТ 3128-70", "Штифт 6×20 ГОСТ 3128-70", "Штифт 6×20 ГОСТ 3128-70"};
    for (std::size_t r = 0; r < t.rows.size(); ++r) {
        for (std::size_t o = 0; o < f.occ.size(); ++o) {
            std::string key = keyOf[o];
            bool excluded = false;
            for (std::size_t c = 0; c < t.columns.size(); ++c) {
                if (t.columns[c].target != f.occ[o]) continue;
                const std::string& cell = t.rows[r].cells[c];
                if (t.columns[c].kind == asm2::AssemblyColumnKind::Suppression) excluded = cell == "1";
                if (t.columns[c].kind == asm2::AssemblyColumnKind::Configuration && !cell.empty() && cell != "#") key += cell;
            }
            if (!excluded) ++out[r][key];
        }
    }
    return out;
}

const annotations::GroupSpecItem* groupItem(const annotations::GroupSpecification& g, const std::string& key) {
    for (const auto& s : g.sections)
        for (const auto& it : s.items)
            if ((it.item.designation.empty() ? it.item.name : it.item.designation) == key) return &it;
    return nullptr;
}

}  // namespace

TEST(DrawingDocumentGroup, AssemblyGroupSpecificationQuantitiesPerConfiguration) {
    auto f = makeFrame();
    auto snap = snapshotAssembly(f->doc);
    ASSERT_TRUE(snap.ok()) << snap.error().message;

    SpecificationDocument spec;
    spec.model().kind = ModelKind::Assembly;
    spec.settings().group.enabled = true;
    spec.settings().notes["РЦ.080.00.010-01"] = "Только -01";
    spec.setLoadOptions(f->options);
    auto up = spec.update(snap.value());
    ASSERT_TRUE(up.ok()) << up.error().message;
    const annotations::GroupSpecification* g = spec.groupSpecification();
    ASSERT_NE(g, nullptr);
    EXPECT_EQ(g->designation, "РЦ.200.00.000");
    EXPECT_EQ(g->configurations, (std::vector<std::string>{"РЦ.200.00.000", "РЦ.200.00.000-01"}));
    EXPECT_EQ(g->columnHeaders, (std::vector<std::string>{"–", "01"}));

    // Количества по исполнениям: групповая спецификация = configurationBom = подсчёт по таблице.
    auto bom = asm2::configurationBom(*f->doc, f->options);
    ASSERT_TRUE(bom.ok()) << bom.error().message;
    const auto counted = countByTable(*f);
    std::size_t items = 0;
    for (const auto& s : g->sections) {
        if (s.section == annotations::SpecSection::Documentation) continue;
        for (const auto& it : s.items) {
            ++items;
            const std::string key = it.item.designation.empty() ? it.item.name : it.item.designation;
            const asm2::ConfigurationBomRow* row = nullptr;
            for (const auto& r : bom.value().rows)
                if ((r.designation.empty() ? r.name : r.designation) == key) row = &r;
            ASSERT_NE(row, nullptr) << key;
            EXPECT_EQ(it.quantities, row->quantities) << key;
            for (std::size_t c = 0; c < 2; ++c) {
                const auto found = counted[c].find(key);
                EXPECT_EQ(it.quantities[c], found == counted[c].end() ? 0 : found->second) << key << " in " << c;
            }
        }
    }
    EXPECT_EQ(items, 4u);
    EXPECT_EQ(groupItem(*g, "РЦ.080.00.010")->quantities, (std::vector<int>{2, 1}));
    EXPECT_EQ(groupItem(*g, "РЦ.080.00.010-01")->quantities, (std::vector<int>{0, 1}));
    EXPECT_EQ(groupItem(*g, "Штифт 6×20 ГОСТ 3128-70")->quantities, (std::vector<int>{4, 2}));
    EXPECT_EQ(groupItem(*g, "РЦ.200.00.001")->quantities, (std::vector<int>{1, 1}));
    EXPECT_EQ(groupItem(*g, "РЦ.080.00.010-01")->item.note, "Только -01");
    // Позиции (графы по исполнениям) — в порядке записи: детали по обозначениям, затем штифт.
    EXPECT_EQ(g->positionOf(*groupItem(*g, "РЦ.080.00.010")->item.definition), 1);
    EXPECT_EQ(groupItem(*g, "РЦ.080.00.010-01")->item.position, 2);
    EXPECT_EQ(groupItem(*g, "РЦ.200.00.001")->item.position, 3);
    EXPECT_EQ(groupItem(*g, "Штифт 6×20 ГОСТ 3128-70")->item.position, 4);
    // Спецификация активного исполнения (позиции чертежа): исполнение «-01» планки не входит.
    const annotations::Specification* active = spec.specification();
    ASSERT_NE(active, nullptr);
    const annotations::SpecSectionData* parts = active->section(annotations::SpecSection::Parts);
    ASSERT_NE(parts, nullptr);
    EXPECT_EQ(parts->items.size(), 2u);
    EXPECT_EQ(parts->items[0].quantity, 2);

    // Листы: строка исполнения с количествами по графам («–» — нет в исполнении).
    auto sheets = spec.render(nullptr);
    ASSERT_TRUE(sheets.ok()) << sheets.error().message;
    bool sawPlank01 = false, sawPin = false;
    for (const auto& page : sheets.value().rows)
        for (const auto& row : page) {
            if (row.cells[3] == "РЦ.080.00.010-01") {
                sawPlank01 = true;
                EXPECT_EQ(row.quantities, (std::vector<std::string>{"–", "1"}));
                EXPECT_TRUE(row.cells[5].empty());
            }
            if (row.cells[4].rfind("Штифт", 0) == 0) {
                sawPin = true;
                EXPECT_EQ(row.quantities, (std::vector<std::string>{"4", "2"}));
            }
        }
    EXPECT_TRUE(sawPlank01);
    EXPECT_TRUE(sawPin);

    // Заголовки граф количества — правка пользователя; сохранение и загрузка.
    spec.settings().group.columnHeaders = {"Базовое", "-01"};
    ASSERT_TRUE(spec.update(snap.value()).ok());
    EXPECT_EQ(spec.groupSpecification()->columnHeaders, (std::vector<std::string>{"Базовое", "-01"}));
    spec.settings().group.columnHeaders.clear();
    ASSERT_TRUE(spec.update(snap.value()).ok());
    auto text = spec.toJson();
    ASSERT_TRUE(text.ok());
    auto back = SpecificationDocument::fromJson(text.value());
    ASSERT_TRUE(back.ok()) << back.error().message;
    EXPECT_TRUE(back.value().settings().group.enabled);
    EXPECT_EQ(back.value().toJson().value(), text.value());

    // Комплект тем же путём, что обычная спецификация: сборочный чертёж + групповая спецификация.
    DrawingDocument d;
    d.model().kind = ModelKind::Assembly;
    SheetDef s;
    s.id = "S1";
    s.format = {GostFormat::A3, 1, Orientation::Landscape};
    ViewDef front;
    front.id = "F";
    front.autoBalloons = true;
    s.views.push_back(front);
    d.sheets().push_back(s);
    views::ViewCache cache(32, 0);
    auto rep = d.update(snap.value(), cache, spec.specification());
    ASSERT_TRUE(rep.ok()) << rep.error().message;
    const std::string dir = dv2test::outPath("group_spec");
    ExportKitOptions ko;
    ko.directory = dir;
    ko.baseName = "frame";
    ko.font = dv2test::systemFont();
    auto kit = exportKit(d, &spec, ko);
    ASSERT_TRUE(kit.ok()) << kit.error().message;
    EXPECT_EQ(kit.value().specificationPages, 1);
    const DxfContent sdxf = readDxf(dir + "/frame_SP1.dxf");
    for (const char* want : {"РЦ.200.00.000 СБ", "РЦ.080.00.010-01", "01", "–", "Только -01"})
        EXPECT_TRUE(hasText(sdxf, want)) << "DXF text '" << want << "'";
    if (std::string err = ezdxfCheck(dir + "/frame_SP1.dxf", "РЦ.080.00.010-01"); !err.empty()) ADD_FAILURE() << err;
    if (const std::string t = ko.font ? pdfText(dir + "/frame.pdf") : std::string(); !t.empty()) {
        for (const char* want : {"РЦ.200.00.000 СБ", "Детали", "Стандартные изделия", "РЦ.080.00.010-01", "Штифт 6×20"})
            EXPECT_NE(t.find(want), std::string::npos) << "pdftotext: '" << want << "' missing";
    }

    // Способ «Переменные данные для исполнений»: постоянная часть — основание (1/1), затем
    // переменные составные части под обозначением каждого исполнения.
    spec.settings().group.layout = annotations::GroupSpecLayout::VariableData;
    ASSERT_TRUE(spec.update(snap.value()).ok());
    g = spec.groupSpecification();
    EXPECT_EQ(groupItem(*g, "РЦ.200.00.001")->item.position, 1);
    EXPECT_EQ(groupItem(*g, "РЦ.080.00.010")->item.position, 2);
    EXPECT_EQ(groupItem(*g, "РЦ.080.00.010-01")->item.position, 3);
    EXPECT_EQ(groupItem(*g, "Штифт 6×20 ГОСТ 3128-70")->item.position, 4);
    auto vd = spec.render(nullptr);
    ASSERT_TRUE(vd.ok()) << vd.error().message;
    std::vector<std::string> order;  // заголовки и обозначения по порядку
    std::map<std::string, std::vector<std::string>> qty;
    for (const auto& page : vd.value().rows)
        for (const auto& row : page) {
            if (row.kind == annotations::SpecRowKind::Header) order.push_back(row.cells[4]);
            if (row.kind == annotations::SpecRowKind::Item && !row.continuation) {
                order.push_back(row.cells[3].empty() ? row.cells[4] : row.cells[3]);
                qty[order.back()].push_back(row.cells[5]);
            }
        }
    const std::vector<std::string> expected = {"Документация", "РЦ.200.00.000 СБ", "Детали", "РЦ.200.00.001",
                                               "Переменные данные для исполнений", "РЦ.200.00.000", "Детали",
                                               "РЦ.080.00.010", "Стандартные изделия", "Штифт 6×20 ГОСТ 3128-70",
                                               "РЦ.200.00.000-01", "Детали", "РЦ.080.00.010", "РЦ.080.00.010-01",
                                               "Стандартные изделия", "Штифт 6×20 ГОСТ 3128-70"};
    EXPECT_EQ(order, expected);
    EXPECT_EQ(qty["РЦ.080.00.010"], (std::vector<std::string>{"2", "1"}));
    EXPECT_EQ(qty["Штифт 6×20 ГОСТ 3128-70"], (std::vector<std::string>{"4", "2"}));
    EXPECT_EQ(qty["РЦ.200.00.001"], (std::vector<std::string>{"1"}));
}

TEST(DrawingDocumentGroup, GroupLettersAndColumnWidths) {
    EXPECT_EQ(annotations::groupDimensionLetter(0), "A");
    EXPECT_EQ(annotations::groupDimensionLetter(7), "H");
    EXPECT_EQ(annotations::groupDimensionLetter(8), "J");    // без I
    EXPECT_EQ(annotations::groupDimensionLetter(12), "N");   // без O
    EXPECT_EQ(annotations::groupDimensionLetter(23), "Z");
    EXPECT_EQ(annotations::groupDimensionLetter(24), "AA");
    for (std::size_t n = 1; n <= 7; ++n) {
        auto w = annotations::groupSpecColumnWidths(n);
        ASSERT_TRUE(w.ok());
        double total = 0;
        for (double x : w.value()) total += x;
        EXPECT_NEAR(total, 185.0, 1e-9) << n;
        EXPECT_GE(w.value()[4], 27.0) << n;
    }
    EXPECT_FALSE(annotations::groupSpecColumnWidths(8).ok());
}
