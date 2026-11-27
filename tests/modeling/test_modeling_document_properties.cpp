// Свойства документа детали (docs/modeling/PART_DOCUMENT.md, раздел «Свойства документа»):
// правка и отмена, файл туда и обратно, старые файлы (v3 без свойств, материал страницы),
// ошибки, обозначение исполнения от обозначения документа, действующие свойства исполнения.

#include "modeling_document_test_util.hpp"

#include <nlohmann/json.hpp>

using namespace bendcore::modeling;
using namespace bendcore::modeling::testutil;
using nlohmann::json;

namespace {

PartDocument boxDocument() {
    PartDocument doc(partRegistry());
    setVar(doc, "W", 60);
    add(doc, {"Box", "Body", "", {{"length", "W"}, {"width", "40"}, {"height", "20"}}, {}, {}, false});
    EXPECT_TRUE(doc.rebuild().failed.empty());
    return doc;
}

DocumentProperties sample() {
    DocumentProperties p;
    p.designation = "АБВГ.715311.001";
    p.name = "Плита опорная";
    p.material = "steel_s235";
    p.density = 7850.0;
    p.developer = "Иванов";
    p.organization = "ООО «Ромашка»";
    p.note = "Покрытие Хим.Окс.прм";
    return p;
}

}  // namespace

TEST(DocumentProperties, EditUndoRedo) {
    PartDocument doc = boxDocument();
    EXPECT_TRUE(doc.properties().empty());
    ASSERT_TRUE(doc.setProperties(sample()).ok());
    EXPECT_EQ(doc.properties(), sample());
    ASSERT_TRUE(doc.canUndo());
    EXPECT_EQ(doc.undoHistory().back().name, "properties");
    ASSERT_TRUE(doc.undo().ok());
    EXPECT_TRUE(doc.properties().empty());
    ASSERT_TRUE(doc.redo().ok());
    EXPECT_EQ(doc.properties(), sample());
    // Те же значения — не правка.
    const std::size_t entries = doc.undoHistory().size();
    ASSERT_TRUE(doc.setProperties(sample()).ok());
    EXPECT_EQ(doc.undoHistory().size(), entries);
    // Свойства не меняют построение: перестроение ничего не пересчитывает.
    const RebuildReport rep = doc.rebuild();
    EXPECT_TRUE(rep.recomputed.empty());
}

TEST(DocumentProperties, Errors) {
    PartDocument doc = boxDocument();
    DocumentProperties p = sample();
    p.density = 0.0;
    auto st = doc.setProperties(p);
    ASSERT_FALSE(st.ok());
    EXPECT_NE(st.error().message.find("density"), std::string::npos);
    p.density = std::nan("");
    EXPECT_FALSE(doc.setProperties(p).ok());
    p = sample();
    p.name = "Плита\nопорная";
    st = doc.setProperties(p);
    ASSERT_FALSE(st.ok());
    EXPECT_NE(st.error().message.find("name must be a single line"), std::string::npos);
    p = sample();
    p.note = "Строка 1\nСтрока 2";  // примечание может быть многострочным
    EXPECT_TRUE(doc.setProperties(p).ok());
    EXPECT_FALSE(doc.canRedo());
}

TEST(DocumentProperties, FileRoundTripAndOldFiles) {
    PartDocument doc = boxDocument();
    ASSERT_TRUE(doc.setProperties(sample()).ok());
    auto text = doc.toJson();
    ASSERT_TRUE(text.ok());
    const json j = json::parse(text.value());
    // Документ без таблицы и сеток остаётся v3: поля свойств — необязательные.
    EXPECT_EQ(j.at("version").get<int>(), 3);
    EXPECT_EQ(j.at("properties").at("designation"), "АБВГ.715311.001");
    EXPECT_EQ(j.at("properties").at("organization"), "ООО «Ромашка»");
    EXPECT_EQ(j.at("material").at("id"), "steel_s235");  // формат страницы детали с v3
    EXPECT_DOUBLE_EQ(j.at("material").at("density").get<double>(), 7850.0);
    auto loaded = PartDocument::fromJson(text.value(), partRegistry());
    ASSERT_TRUE(loaded.ok()) << loaded.error().message;
    EXPECT_EQ(loaded.value().properties(), sample());
    EXPECT_FALSE(loaded.value().canUndo());  // загрузка начинает с пустой истории

    // Старый файл без свойств (так писали до свойств документа) — пустые свойства.
    json old = j;
    old.erase("properties");
    old.erase("material");
    auto plain = PartDocument::fromJson(old.dump(), partRegistry());
    ASSERT_TRUE(plain.ok());
    EXPECT_TRUE(plain.value().properties().empty());
    EXPECT_NEAR(volume(plain.value().finalShape().shape()), 60.0 * 40 * 20, 1e-6);

    // Материал, записанный страницей детали («Свой» с плотностью вручную).
    old["material"] = {{"id", "custom"}, {"density", 1200.0}};
    auto custom = PartDocument::fromJson(old.dump(), partRegistry());
    ASSERT_TRUE(custom.ok());
    EXPECT_EQ(custom.value().properties().material, "custom");
    EXPECT_EQ(custom.value().properties().density, std::optional<double>(1200.0));
    // Повреждённое поле материала страница игнорирует — и документ тоже.
    old["material"] = "steel";
    auto broken = PartDocument::fromJson(old.dump(), partRegistry());
    ASSERT_TRUE(broken.ok());
    EXPECT_TRUE(broken.value().properties().empty());
    // Неверный тип поля свойств — ParseError.
    old.erase("material");
    old["properties"] = {{"designation", 12}};
    auto bad = PartDocument::fromJson(old.dump(), partRegistry());
    ASSERT_FALSE(bad.ok());
    EXPECT_EQ(bad.error().code, bendcore::kernel::ErrorCode::ParseError);
    EXPECT_NE(bad.error().message.find("designation"), std::string::npos);
}

TEST(DocumentProperties, ConfigurationDesignationCompletesTheDocument) {
    PartDocument doc = boxDocument();
    ConfigurationTable t;
    t.columns = {{ConfigurationColumnKind::Variable, "W"}, {ConfigurationColumnKind::Material, ""}};
    t.rows = {{"-00", "", {"60", ""}}, {"-01", "Плита длинная", {"90", "al_6061_t6"}}};
    ASSERT_TRUE(doc.setConfigurationTable(t).ok());
    // Без обозначения документа суффикс остаётся как записан (первая строка — тоже суффикс).
    EXPECT_EQ(doc.configurationDesignation(1), "-01");
    DocumentProperties p = sample();
    ASSERT_TRUE(doc.setProperties(p).ok());
    EXPECT_EQ(doc.configurationDesignation(0), "АБВГ.715311.001-00");
    EXPECT_EQ(doc.configurationDesignation(1), "АБВГ.715311.001-01");
    EXPECT_EQ(doc.configurationDesignation(2), "");

    // Без активного исполнения — свойства документа; плотность библиотечного материала, если
    // своей нет (S235 — 7850 кг/м³).
    p.density.reset();
    ASSERT_TRUE(doc.setProperties(p).ok());
    ConfigurationProperties e = doc.effectiveProperties();
    EXPECT_EQ(e.designation, "АБВГ.715311.001");
    EXPECT_EQ(e.name, "Плита опорная");
    EXPECT_EQ(e.material, "steel_s235");
    EXPECT_EQ(e.density, std::optional<double>(7850.0));

    // «-00»: наименование пусто — наименование документа, материал — документа.
    ASSERT_TRUE(doc.activateConfiguration("-00").ok());
    e = doc.effectiveProperties();
    EXPECT_EQ(e.designation, "АБВГ.715311.001-00");
    EXPECT_EQ(e.name, "Плита опорная");
    EXPECT_EQ(e.material, "steel_s235");
    // «-01»: своё наименование и материал (6061-T6 — 2700 кг/м³).
    ASSERT_TRUE(doc.activateConfiguration("-01").ok());
    e = doc.effectiveProperties();
    EXPECT_EQ(e.designation, "АБВГ.715311.001-01");
    EXPECT_EQ(e.name, "Плита длинная");
    EXPECT_EQ(e.material, "al_6061_t6");
    EXPECT_EQ(e.density, std::optional<double>(2700.0));
    // Проверка исполнений сообщает те же полные обозначения.
    const auto checks = doc.checkConfigurations();
    ASSERT_EQ(checks.size(), 2u);
    EXPECT_EQ(checks[0].fullDesignation, "АБВГ.715311.001-00");
    EXPECT_EQ(checks[1].fullDesignation, "АБВГ.715311.001-01");

    // Полное обозначение в первой строке: суффикс дополняет обозначение документа, если оно
    // задано, сама строка — как записана.
    t.rows[0].designation = "ЕЖЗИ.000.001";
    t.active.clear();
    ASSERT_TRUE(doc.setConfigurationTable(t).ok());
    EXPECT_EQ(doc.configurationDesignation(0), "ЕЖЗИ.000.001");
    EXPECT_EQ(doc.configurationDesignation(1), "АБВГ.715311.001-01");
    p.designation.clear();
    ASSERT_TRUE(doc.setProperties(p).ok());
    EXPECT_EQ(doc.configurationDesignation(1), "ЕЖЗИ.000.001-01");  // прежнее правило ГОСТ 2.113
}
