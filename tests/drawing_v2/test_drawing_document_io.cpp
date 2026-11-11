// Формат файла документа чертежа: сохранение → загрузка, миграция версии 1, ошибки разбора и
// проверки определений, загрузка модели по пути (fileModelLoader), документ спецификации.

#include "test_drawing_document_support.hpp"

#include <gtest/gtest.h>

#include <filesystem>

#include <nlohmann/json.hpp>

using namespace ddtest;
using bendcore::kernel::ErrorCode;
using bendcore::modeling::ElementKind;
using annotations::AnchorKind;
using annotations::DimensionType;

namespace {

DrawingDocument blockDrawing(const ModelSnapshot& m) {
    DrawingDocument d;
    d.model().kind = ModelKind::Part;
    d.model().path = "bracket_part.json";
    d.model().properties = {"РД.10.001", "Кронштейн", "Сталь 20", 7850.0};
    SheetDef s;
    s.id = "S1";
    s.format = {GostFormat::A3, 1, Orientation::Landscape};
    s.scale = views::ScaleRatio{1, 1};
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
    s.dimensions.push_back(dimension("A", "F", DimensionType::Horizontal, {v(0, -15, 0), v(60, -15, 0)}, -12));
    DimensionItem d8 = dimension("d", "T", DimensionType::Diameter,
                                 {anchorOn(m, ElementKind::Edge, circleEdge(m, {40, 0, 8}, 4), AnchorKind::Center)});
    d8.def.tolerance = annotations::parseToleranceClass("H7").value();
    s.dimensions.push_back(d8);
    s.technicalRequirements = {"Острые кромки притупить."};
    d.sheets().push_back(std::move(s));
    return d;
}

}  // namespace

TEST(DrawingDocumentIo, RoundTripAndFileLoader) {
    auto part = makeBracket();
    const ModelProperties props{"РД.10.001", "Кронштейн", "Сталь 20", 7850.0};
    auto snap = snapshotPart(*part, props);
    ASSERT_TRUE(snap.ok());
    DrawingDocument d = blockDrawing(snap.value());
    views::ViewCache cache(8, 2);
    auto rep = d.update(snap.value(), cache);
    ASSERT_TRUE(rep.ok()) << rep.error().message;
    EXPECT_EQ(dimensionResult(d, "S1", "d")->text, "⌀8H7");
    EXPECT_EQ(d.model().resultHash, hashHex(snap.value().contentHash));
    EXPECT_EQ(d.model().revision, 1u);

    const std::string dir = dv2test::outPath("io");
    std::filesystem::create_directories(dir);
    ASSERT_TRUE(saveToFile(dir + "/bracket_part.json", part->toJson().value()).ok());
    ASSERT_TRUE(d.saveFile(dir + "/bracket.drawing.json").ok());
    int version = 0;
    auto loaded = DrawingDocument::loadFile(dir + "/bracket.drawing.json", &version);
    ASSERT_TRUE(loaded.ok()) << loaded.error().message;
    EXPECT_EQ(version, DrawingDocument::kFormatVersion);
    EXPECT_EQ(loaded.value().toJson().value(), d.toJson().value());
    // Хэш результата и последние значения сохранены: модель не изменилась.
    auto loader = fileModelLoader(dir);
    auto snap2 = loader(loaded.value().model());
    ASSERT_TRUE(snap2.ok()) << snap2.error().message;
    EXPECT_EQ(snap2.value().contentHash, snap.value().contentHash);  // перестроение из файла — та же форма
    EXPECT_FALSE(loaded.value().checkModel(snap2.value()).modelChanged);
    auto rep2 = loaded.value().update(loader, cache);
    ASSERT_TRUE(rep2.ok()) << rep2.error().message;
    EXPECT_FALSE(rep2.value().modelChanged);
    EXPECT_TRUE(rep2.value().stale.empty());
    for (const auto& dr : rep2.value().dimensions) {
        EXPECT_EQ(dr.state, annotations::DimensionState::Ok);
        EXPECT_EQ(dr.method, bendcore::modeling::ResolveMethod::Exact) << dr.id;  // ссылки по имени
        ASSERT_TRUE(dr.oldValue.has_value());
        EXPECT_NEAR(*dr.oldValue, dr.newValue, 1e-12);
    }
    // Модель на диске изменена → чертёж устарел; после обновления — новые значения и ревизия.
    setVariable(*part, "A", 75);
    ASSERT_TRUE(part->rebuild().failed.empty());
    ASSERT_TRUE(saveToFile(dir + "/bracket_part.json", part->toJson().value()).ok());
    auto snap3 = loader(loaded.value().model());
    ASSERT_TRUE(snap3.ok());
    EXPECT_TRUE(loaded.value().checkModel(snap3.value()).modelChanged);
    auto rep3 = loaded.value().update(snap3.value(), cache);
    ASSERT_TRUE(rep3.ok());
    EXPECT_EQ(rep3.value().revision, 2u);
    ASSERT_EQ(rep3.value().stale.size(), 1u);
    EXPECT_EQ(rep3.value().stale[0], "S1/A: 60 → 75");
}

TEST(DrawingDocumentIo, MigratesVersion1) {
    auto part = makeBracket();
    auto snap = snapshotPart(*part, {"РД.10.001", "Кронштейн", "Сталь 20", 7850.0});
    ASSERT_TRUE(snap.ok());
    // Имя ребра нижней кромки и вершин — как их даёт документ детали.
    const auto e = lineEdge(snap.value(), {0, -15, 0}, {60, -15, 0});
    const auto v0 = vertexAt(snap.value(), {0, -15, 0}), v1 = vertexAt(snap.value(), {60, -15, 0});
    ASSERT_TRUE(e && v0 && v1);
    const std::string en = elementReference(snap.value(), ElementKind::Edge, *e).value().name;
    const std::string n0 = elementReference(snap.value(), ElementKind::Vertex, *v0).value().name;
    const std::string n1 = elementReference(snap.value(), ElementKind::Vertex, *v1).value().name;
    nlohmann::json v1doc = {
        {"format", "bendcore-drawing"},
        {"version", 1},
        {"model", "bracket.json"},
        {"modelKind", "part"},
        {"sheets",
         {{{"id", "S1"},
           {"format", "A3L"},
           {"views", {{{"id", "F"}, {"kind", "base"}}, {{"id", "I"}, {"kind", "axonometric"}, {"parent", "F"}, {"scale", 0.5}}}},
           {"dimensions",
            {{{"id", "A"},
              {"view", "F"},
              {"type", "horizontal"},
              {"anchors", {{{"anchor", "point"}, {"ref", n0}, {"refKind", "vertex"}}, {{"anchor", "point"}, {"ref", n1}, {"refKind", "vertex"}}}},
              {"offset", -12}}}},
           {"roughness", {{{"id", "R"}, {"view", "F"}, {"edge", en}, {"spec", {{"method", "materialRemoval"}, {"value", "3,2"}}}}}}}}}};
    int version = 0;
    auto d = DrawingDocument::fromJson(v1doc.dump(), &version);
    ASSERT_TRUE(d.ok()) << d.error().message;
    EXPECT_EQ(version, 1);
    EXPECT_EQ(d.value().model().path, "bracket.json");
    EXPECT_EQ(d.value().model().kind, ModelKind::Part);
    const SheetDef& s = d.value().sheets()[0];
    EXPECT_EQ(s.format.base, GostFormat::A3);
    EXPECT_EQ(s.format.orientation, Orientation::Landscape);
    ASSERT_TRUE(s.views[1].scale.has_value());
    EXPECT_EQ(s.views[1].scale->label(), "1:2");
    EXPECT_FALSE(s.dimensions[0].def.anchors[0].ref.persistent.signature.valid);
    // Ссылки v1 (только имена) разрешаются по имени; после обновления — с сигнатурой.
    d.value().model().properties = {"РД.10.001", "Кронштейн", "Сталь 20", 7850.0};
    views::ViewCache cache(8, 2);
    auto rep = d.value().update(snap.value(), cache);
    ASSERT_TRUE(rep.ok()) << rep.error().message;
    EXPECT_TRUE(rep.value().dangling.empty()) << rep.value().dangling.front();
    EXPECT_NEAR(dimensionResult(d.value(), "S1", "A")->value, 60.0, 1e-9);
    EXPECT_TRUE(d.value().sheets()[0].dimensions[0].def.anchors[0].ref.persistent.signature.valid);
    // Сохранение — всегда текущая версия.
    auto j = nlohmann::json::parse(d.value().toJson().value());
    EXPECT_EQ(j["version"], DrawingDocument::kFormatVersion);
    EXPECT_EQ(j["model"]["path"], "bracket.json");
    EXPECT_EQ(j["sheets"][0]["format"]["orientation"], "landscape");
}

TEST(DrawingDocumentIo, ErrorsAndDangling) {
    EXPECT_EQ(DrawingDocument::fromJson("not json").error().code, ErrorCode::ParseError);
    EXPECT_EQ(DrawingDocument::fromJson(R"({"format":"bendcore-part","version":3})").error().code, ErrorCode::ParseError);
    EXPECT_EQ(DrawingDocument::fromJson(R"({"format":"bendcore-drawing","version":9,"sheets":[]})").error().code,
              ErrorCode::Unsupported);
    // Вид с родителем, объявленным позже, — ошибка проверки определений.
    const char* badParent = R"({"format":"bendcore-drawing","version":2,"model":{"kind":"part","path":"p.json"},
      "sheets":[{"id":"S1","format":{"base":"A3","multiplier":1,"orientation":"landscape"},
      "views":[{"id":"F","kind":"base"},{"id":"T","kind":"projected","parent":"X","side":"below"}]}]})";
    auto bp = DrawingDocument::fromJson(badParent);
    ASSERT_FALSE(bp.ok());
    EXPECT_NE(bp.error().message.find("parent 'X'"), std::string::npos) << bp.error().message;
    const char* badScale = R"({"format":"bendcore-drawing","version":2,"model":{"kind":"part","path":"p.json"},
      "sheets":[{"id":"S1","format":{"base":"A3","multiplier":1,"orientation":"landscape"},"scale":[1,3],
      "views":[{"id":"F","kind":"base"}]}]})";
    EXPECT_NE(DrawingDocument::fromJson(badScale).error().message.find("GOST 2.302"), std::string::npos);

    // Висячий размер: отверстие подавлено — ⌀ и знак без элемента, причина в отчёте; длина остаётся.
    auto part = makeBracket();
    auto snap = snapshotPart(*part, {});
    ASSERT_TRUE(snap.ok());
    DrawingDocument d = blockDrawing(snap.value());
    views::ViewCache cache(8, 2);
    ASSERT_TRUE(d.update(snap.value(), cache).ok());
    ASSERT_TRUE(part->setSuppressed("MountHole", true).ok());
    part->rebuild();
    auto snap2 = snapshotPart(*part, {});
    auto rep = d.update(snap2.value(), cache);
    ASSERT_TRUE(rep.ok()) << rep.error().message;
    ASSERT_EQ(rep.value().dangling.size(), 1u);
    EXPECT_EQ(rep.value().dangling[0].rfind("S1/d: ", 0), 0u) << rep.value().dangling[0];
    EXPECT_EQ(dimensionResult(d, "S1", "A")->state, annotations::DimensionState::Ok);
    // Висячий размер не выводится, лист строится.
    auto sheets = d.render(dv2test::systemFont());
    ASSERT_TRUE(sheets.ok()) << sheets.error().message;
    // Снятие подавления — ссылка снова разрешается.
    ASSERT_TRUE(part->setSuppressed("MountHole", false).ok());
    part->rebuild();
    auto rep3 = d.update(snapshotPart(*part, {}).value(), cache);
    ASSERT_TRUE(rep3.ok());
    EXPECT_TRUE(rep3.value().dangling.empty());
    EXPECT_GE(rep3.value().viewsFromCache, 2u);  // та же форма, что при первом обновлении

    // Ссылка на модель другого вида — ошибка.
    d.model().kind = ModelKind::Assembly;
    EXPECT_EQ(d.update(snap.value(), cache).error().code, ErrorCode::InvalidInput);
    // Без обновления лист не строится.
    DrawingDocument empty;
    EXPECT_FALSE(empty.render(nullptr).ok());
}

TEST(DrawingDocumentIo, SpecificationDocumentRoundTrip) {
    SpecificationDocument s;
    s.model().kind = ModelKind::Assembly;
    s.model().path = "unit.json";
    s.settings().formats = {{"ВТ.01.001", "А4"}};
    s.settings().sections = {{"Кольцо 30", annotations::SpecSection::OtherProducts}};
    s.settings().reservePositions = 2;
    s.title().developer = {"Петров", "11.26"};
    auto j = s.toJson();
    ASSERT_TRUE(j.ok());
    auto l = SpecificationDocument::fromJson(j.value());
    ASSERT_TRUE(l.ok()) << l.error().message;
    EXPECT_EQ(l.value().toJson().value(), j.value());
    EXPECT_EQ(l.value().settings().sections.at("Кольцо 30"), annotations::SpecSection::OtherProducts);
    EXPECT_FALSE(l.value().render(nullptr).ok());  // не обновлён
    EXPECT_EQ(SpecificationDocument::fromJson(R"({"format":"bendcore-specification","version":1,
      "model":{"kind":"part","path":"x"},"settings":{}})").error().code, ErrorCode::ParseError);
}
