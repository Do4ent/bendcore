// Таблица параметров (исполнения) детали: объём каждого исполнения по формуле, переключение
// туда и обратно, скругление переживает переключение, подавленное отверстие, явные ошибки,
// файл (v4 и старые файлы без таблицы), CSV туда и обратно (docs/modeling/PART_DOCUMENT.md,
// раздел «Таблица параметров»).

#include "modeling_document_test_util.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>

using namespace bendcore::modeling;
using namespace bendcore::modeling::testutil;
using nlohmann::json;

namespace {

constexpr double kFillet = 2.0;   // радиус скругления верхнего переднего ребра
constexpr double kHoleR = 5.0;    // радиус сквозного отверстия в центре верхней грани

struct Dims {
    double w, d, h;
    bool hole;
};

// Брусок W×D×H (по x, y, z), скругление R = 2 ребра y = 0, z = H по длине W, сквозное отверстие
// r = 5 в центре верхней грани: V = W·D·H − (1 − π/4)·R²·W − π·r²·H (отверстие не задевает
// скругление при D/2 − r > R).
double blockVolume(const Dims& m) {
    return m.w * m.d * m.h - filletLoss(kFillet, m.w) - (m.hole ? kPi * kHoleR * kHoleR * m.h : 0.0);
}

const Dims kBase{60, 40, 20, true};
const Dims kV01{80, 50, 30, true};
const Dims kV02{50, 30, 15, false};

struct Block {
    PartDocument doc{partRegistry()};
    std::string box, hole, holeSketch, fillet;

    Block() {
        setVar(doc, "W", kBase.w);
        setVar(doc, "D", kBase.d);
        setVar(doc, "H", kBase.h);
        box = add(doc, {"Box", "Body", "", {{"length", "W"}, {"width", "D"}, {"height", "H"}}, {}, {}, false});
        doc.rebuild();
        NamedShape s = doc.finalShape();
        fillet = add(doc, {"Fillet", "Round", "", {{"radius", num(kFillet)}},
                           {{"edges", {refTo(s, singleLine(s.shape(), 0, 0, kBase.h))}}}, {}, false});
        doc.rebuild();
        s = doc.finalShape();
        auto cut = addSketchCut(doc, "Hole", refTo(s, planarFace(s.shape(), gp::DZ(), kBase.h)), "W / 2", "D / 2", "H",
                                centredCircle(num(kHoleR)), "");
        hole = cut.cut;
        holeSketch = cut.sketch;
        const auto rep = doc.rebuild();
        EXPECT_TRUE(rep.failed.empty());
    }

    ConfigurationTable table() const {
        ConfigurationTable t;
        t.columns = {{ConfigurationColumnKind::Variable, "W"},
                     {ConfigurationColumnKind::Variable, "D"},
                     {ConfigurationColumnKind::Variable, "H"},
                     {ConfigurationColumnKind::Suppression, "Hole"},
                     {ConfigurationColumnKind::Material, ""}};
        t.rows = {{"РЦ.080.00.010", "Брусок", {"60", "40", "20", "0", "steel_s235"}},
                  {"-01", "Брусок большой", {"80", "50", "30", "0", "al_6061_t6"}},
                  {"-02", "Брусок без отверстия", {"50", "30", "15", "1", ""}}};
        return t;
    }
};

double finalVolume(const PartDocument& doc) {
    const NamedShape s = doc.finalShape();
    return s.isNull() ? 0.0 : volume(s.shape());
}

}  // namespace

TEST(ModelingDocumentConfigurations, VolumeOfEveryConfigurationMatchesTheFormula) {
    Block b;
    ASSERT_NEAR(finalVolume(b.doc), blockVolume(kBase), 1e-6);
    auto st = b.doc.setConfigurationTable(b.table());
    ASSERT_TRUE(st.ok()) << st.error().message;

    const std::vector<std::pair<std::string, Dims>> cases = {
        {"-01", kV01}, {"-02", kV02}, {"РЦ.080.00.010", kBase}};
    for (const auto& [designation, dims] : cases) {
        ASSERT_TRUE(b.doc.activateConfiguration(designation).ok()) << designation;
        const RebuildReport rep = b.doc.rebuild();
        EXPECT_TRUE(rep.failed.empty()) << designation;
        EXPECT_NEAR(finalVolume(b.doc), blockVolume(dims), 1e-6) << designation;
        EXPECT_NEAR(b.doc.variableValue("W").value(), dims.w, 0.0);
        EXPECT_EQ(info(b.doc, b.hole).state, dims.hole ? FeatureState::Ok : FeatureState::Suppressed) << designation;
        EXPECT_EQ(info(b.doc, b.hole).suppressed, !dims.hole);
        // The fillet stays on the top-front edge: axis along x at y = R, z = H − R.
        EXPECT_TRUE(hasCylinder(b.doc.finalShape().shape(), kFillet, 0, kFillet, dims.h - kFillet)) << designation;
        EXPECT_EQ(info(b.doc, b.fillet).state, FeatureState::Ok) << messages(info(b.doc, b.fillet));
    }
    EXPECT_EQ(b.doc.activeConfiguration(), "РЦ.080.00.010");
    EXPECT_EQ(b.doc.configurationMaterial(), "steel_s235");
    ASSERT_TRUE(b.doc.activateConfiguration("-02").ok());
    EXPECT_EQ(b.doc.configurationMaterial(), "");
    EXPECT_EQ(fullDesignation(b.doc.configurationTable(), 1), "РЦ.080.00.010-01");
    EXPECT_EQ(fullDesignation(b.doc.configurationTable(), 0), "РЦ.080.00.010");
}

TEST(ModelingDocumentConfigurations, SwitchingBackRestoresTheSameVolumeAndReferences) {
    Block b;
    ASSERT_TRUE(b.doc.setConfigurationTable(b.table()).ok());
    ASSERT_TRUE(b.doc.activateConfiguration("РЦ.080.00.010").ok());
    b.doc.rebuild();
    const double v0 = finalVolume(b.doc);
    const std::string edge0 = info(b.doc, b.fillet).references.at("edges").at(0).name;
    for (int round = 0; round < 3; ++round) {
        for (const char* d : {"-01", "-02"}) {
            ASSERT_TRUE(b.doc.activateConfiguration(d).ok());
            b.doc.rebuild();
            const FeatureInfo fi = info(b.doc, b.fillet);
            ASSERT_EQ(fi.referenceReports.size(), 1u);
            EXPECT_EQ(fi.referenceReports[0].resolution.method, ResolveMethod::Exact) << d;
        }
        ASSERT_TRUE(b.doc.activateConfiguration("РЦ.080.00.010").ok());
        const RebuildReport rep = b.doc.rebuild();
        EXPECT_EQ(finalVolume(b.doc), v0) << "bit for bit after round " << round;
        EXPECT_TRUE(rep.recomputed.empty()) << "results come from the pool";
        EXPECT_EQ(info(b.doc, b.fillet).references.at("edges").at(0).name, edge0);
    }
    // Base values (no active configuration) are the document variables.
    ASSERT_TRUE(b.doc.activateConfiguration("").ok());
    b.doc.rebuild();
    EXPECT_NEAR(finalVolume(b.doc), blockVolume(kBase), 1e-6);
}

TEST(ModelingDocumentConfigurations, EditsOfDrivenValuesWriteTheActiveCell) {
    Block b;
    ASSERT_TRUE(b.doc.setConfigurationTable(b.table()).ok());
    ASSERT_TRUE(b.doc.activateConfiguration("-01").ok());
    ASSERT_TRUE(b.doc.setVariable("H", "35").ok());
    EXPECT_EQ(b.doc.configurationTable().rows[1].cells[2], "35");
    auto vars = b.doc.variables();
    EXPECT_NE(std::find(vars.begin(), vars.end(), std::make_pair(std::string("H"), std::string("35"))), vars.end());
    b.doc.rebuild();
    EXPECT_NEAR(finalVolume(b.doc), blockVolume({80, 50, 35, true}), 1e-6);
    ASSERT_TRUE(b.doc.setSuppressed(b.hole, true).ok());
    EXPECT_EQ(b.doc.configurationTable().rows[1].cells[3], "1");
    b.doc.rebuild();
    EXPECT_NEAR(finalVolume(b.doc), blockVolume({80, 50, 35, false}), 1e-6);
    // Base row untouched.
    ASSERT_TRUE(b.doc.activateConfiguration("РЦ.080.00.010").ok());
    b.doc.rebuild();
    EXPECT_NEAR(finalVolume(b.doc), blockVolume(kBase), 1e-6);
    // Undo: the cell edits and the activation come back step by step.
    ASSERT_TRUE(b.doc.undo().ok());  // activation
    EXPECT_EQ(b.doc.activeConfiguration(), "-01");
    ASSERT_TRUE(b.doc.undo().ok());  // suppression cell
    EXPECT_EQ(b.doc.configurationTable().rows[1].cells[3], "0");
    EXPECT_NEAR(finalVolume(b.doc), blockVolume({80, 50, 35, true}), 1e-6);
}

TEST(ModelingDocumentConfigurations, ExplicitErrors) {
    Block b;
    auto expectError = [&](ConfigurationTable t, const std::string& needle) {
        auto st = b.doc.setConfigurationTable(std::move(t));
        ASSERT_FALSE(st.ok()) << needle;
        EXPECT_EQ(st.error().code, bendcore::kernel::ErrorCode::InvalidInput);
        EXPECT_NE(st.error().message.find(needle), std::string::npos) << st.error().message;
    };
    ConfigurationTable t = b.table();
    t.columns[0].target = "Q";
    expectError(t, "unknown variable 'Q'");
    t = b.table();
    t.columns[3].target = "Nope";
    expectError(t, "unknown feature 'Nope'");
    t = b.table();
    t.rows[1].cells[0] = "8O+";
    expectError(t, "invalid value '8O+'");
    t = b.table();
    t.rows[2].cells[3] = "maybe";
    expectError(t, "invalid value 'maybe'");
    t = b.table();
    t.rows[0].cells[4] = "unobtainium";
    expectError(t, "unknown material 'unobtainium'");
    t = b.table();
    t.rows[2].designation = "-01";
    expectError(t, "repeated designation '-01'");
    t = b.table();
    t.rows[1].cells.pop_back();
    expectError(t, "4 cells");
    t = b.table();
    t.active = "-07";
    expectError(t, "no configuration '-07'");
    EXPECT_TRUE(b.doc.configurationTable().empty()) << "a refused table changes nothing";
    EXPECT_FALSE(b.doc.activateConfiguration("-01").ok());

    // A configuration that does not build: the hole is wider than the block.
    t = b.table();
    t.rows.push_back({"-03", "Тонкий", {"8", "8", "20", "0", ""}});
    ASSERT_TRUE(b.doc.setConfigurationTable(t).ok());
    ASSERT_TRUE(b.doc.activateConfiguration("-01").ok());
    b.doc.rebuild();
    const double v01 = finalVolume(b.doc);
    const auto checks = b.doc.checkConfigurations();
    ASSERT_EQ(checks.size(), 4u);
    for (std::size_t i = 0; i < 3; ++i) EXPECT_TRUE(checks[i].ok) << checks[i].designation;
    EXPECT_NEAR(*checks[1].volume, blockVolume(kV01), 1e-6);
    EXPECT_EQ(checks[1].fullDesignation, "РЦ.080.00.010-01");
    EXPECT_FALSE(checks[3].ok);
    ASSERT_FALSE(checks[3].problems.empty());
    std::string all;
    for (const auto& p : checks[3].problems) all += p + "\n";
    EXPECT_NE(all.find("Hole"), std::string::npos) << all;
    EXPECT_EQ(b.doc.activeConfiguration(), "-01") << "the active configuration is restored";
    EXPECT_EQ(finalVolume(b.doc), v01);
    // Variable error in a cell (cycle) is a cause as well.
    ASSERT_TRUE(b.doc.setConfigurationCell(3, 0, "W + 1").ok());
    const auto again = b.doc.checkConfigurations();
    std::string cyc;
    for (const auto& p : again[3].problems) cyc += p + "\n";
    EXPECT_NE(cyc.find("cyclic dependency"), std::string::npos) << cyc;
}

TEST(ModelingDocumentConfigurations, RemovingAVariableOrFeatureRemovesItsColumn) {
    Block b;
    ASSERT_TRUE(b.doc.setConfigurationTable(b.table()).ok());
    ASSERT_TRUE(b.doc.removeFeature(b.hole).ok());
    EXPECT_FALSE(b.doc.configurationTable().columnOf({ConfigurationColumnKind::Suppression, "Hole"}));
    EXPECT_EQ(b.doc.configurationTable().rows[0].cells.size(), 4u);
    ASSERT_TRUE(b.doc.undo().ok());
    EXPECT_TRUE(b.doc.configurationTable().columnOf({ConfigurationColumnKind::Suppression, "Hole"}));
    ASSERT_TRUE(b.doc.setVariable("T", "3").ok());
    ASSERT_TRUE(b.doc.addConfigurationColumn({ConfigurationColumnKind::Variable, "T"}).ok());
    EXPECT_EQ(b.doc.configurationTable().rows[2].cells.back(), "3") << "new column takes the base value";
    ASSERT_TRUE(b.doc.addConfigurationColumn({ConfigurationColumnKind::Suppression, b.fillet}).ok());
    EXPECT_EQ(b.doc.configurationTable().rows[0].cells.back(), "0");
    EXPECT_FALSE(b.doc.addConfigurationColumn({ConfigurationColumnKind::Variable, "T"}).ok()) << "repeated column";
    ASSERT_TRUE(b.doc.removeVariable("T").ok());
    EXPECT_FALSE(b.doc.configurationTable().columnOf({ConfigurationColumnKind::Variable, "T"}));
}

TEST(ModelingDocumentConfigurations, FileRoundTripAndOldFiles) {
    Block b;
    const std::string old = b.doc.toJson().value();
    EXPECT_EQ(json::parse(old).at("version"), 3) << "without a table the file stays v3";
    EXPECT_FALSE(json::parse(old).contains("configurations"));

    ASSERT_TRUE(b.doc.setConfigurationTable(b.table()).ok());
    ASSERT_TRUE(b.doc.activateConfiguration("-01").ok());
    b.doc.rebuild();
    const std::string text = b.doc.toJson().value();
    const json j = json::parse(text);
    EXPECT_EQ(j.at("version"), 4);
    EXPECT_EQ(j.at("configurations").at("active"), "-01");
    EXPECT_EQ(j.at("configurations").at("rows").size(), 3u);

    auto loaded = PartDocument::fromJson(text, partRegistry());
    ASSERT_TRUE(loaded.ok()) << loaded.error().message;
    EXPECT_EQ(loaded.value().configurationTable(), b.doc.configurationTable());
    EXPECT_EQ(loaded.value().activeConfiguration(), "-01");
    EXPECT_NEAR(finalVolume(loaded.value()), blockVolume(kV01), 1e-6);
    // Base variables are kept in the file, not the overrides.
    for (const auto& v : j.at("variables")) {
        if (v.at("name") == "W") EXPECT_EQ(v.at("expression"), "60");
    }

    auto plain = PartDocument::fromJson(old, partRegistry());
    ASSERT_TRUE(plain.ok());
    EXPECT_TRUE(plain.value().configurationTable().empty());
    EXPECT_NEAR(finalVolume(plain.value()), blockVolume(kBase), 1e-6);

    // Broken table in a file: explicit parse error with the cause.
    json bad = j;
    bad["configurations"]["columns"][0]["target"] = "Q";
    auto r = PartDocument::fromJson(bad.dump(), partRegistry());
    ASSERT_FALSE(r.ok());
    EXPECT_EQ(r.error().code, bendcore::kernel::ErrorCode::ParseError);
    EXPECT_NE(r.error().message.find("unknown variable 'Q'"), std::string::npos) << r.error().message;
    bad = j;
    bad["configurations"]["columns"][0]["kind"] = "colour";
    EXPECT_FALSE(PartDocument::fromJson(bad.dump(), partRegistry()).ok());
}

TEST(ModelingDocumentConfigurations, CsvRoundTripWithoutLoss) {
    Block b;
    ConfigurationTable t = b.table();
    t.rows[1].cells[0] = "80.25";
    t.rows[1].name = "Брусок; \"большой\"";
    t.rows[2].cells[1] = "max(30, W / 2)";
    for (char decimal : {'.', ','}) {
        CsvOptions o;
        o.decimal = decimal;
        const std::string csv = configurationTableToCsv(t, o);
        if (decimal == ',') {
            EXPECT_NE(csv.find(";80,25;"), std::string::npos) << csv;
        } else {
            EXPECT_NE(csv.find(";80.25;"), std::string::npos) << csv;
        }
        EXPECT_NE(csv.find("suppress:Hole"), std::string::npos);
        EXPECT_NE(csv.find("\"Брусок; \"\"большой\"\"\""), std::string::npos) << csv;
        auto back = configurationTableFromCsv(csv);
        ASSERT_TRUE(back.ok()) << back.error().message;
        EXPECT_EQ(back.value(), t) << "decimal " << decimal;
        ASSERT_TRUE(b.doc.setConfigurationTable(back.value()).ok());
    }
    // Hand-written file: Russian headers, LF, no BOM, decimal comma, blank line.
    const std::string hand =
        "Обозначение;Наименование;W;D;H;подавить:Hole;Материал\n"
        "РЦ.080.00.010;Брусок;60;40;20;нет;steel_s235\n\n"
        "-01;Брусок;62,5;40;20;да;\n";
    auto h = configurationTableFromCsv(hand);
    ASSERT_TRUE(h.ok()) << h.error().message;
    EXPECT_EQ(h.value().columns, t.columns);
    EXPECT_EQ(h.value().rows[1].cells[0], "62.5");
    ASSERT_TRUE(b.doc.setConfigurationTable(h.value()).ok());
    ASSERT_TRUE(b.doc.activateConfiguration("-01").ok());
    b.doc.rebuild();
    EXPECT_NEAR(finalVolume(b.doc), blockVolume({62.5, 40, 20, false}), 1e-6);

    // Errors carry the line.
    auto e1 = configurationTableFromCsv("designation;name;W\nA;B\n");
    ASSERT_FALSE(e1.ok());
    EXPECT_NE(e1.error().message.find("line 2"), std::string::npos) << e1.error().message;
    auto e2 = configurationTableFromCsv("designation;name;W\n\"A;B;1\n");
    ASSERT_FALSE(e2.ok());
    EXPECT_NE(e2.error().message.find("unterminated quote"), std::string::npos);
    auto e3 = configurationTableFromCsv("id;name;W\n");
    ASSERT_FALSE(e3.ok());
    EXPECT_EQ(e3.error().code, bendcore::kernel::ErrorCode::ParseError);
    auto e4 = configurationTableFromCsv("designation;name;W;W\n");
    ASSERT_FALSE(e4.ok());
    EXPECT_NE(e4.error().message.find("repeated"), std::string::npos);
}
