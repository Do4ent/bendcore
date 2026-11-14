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

// Лист без видов (новый чертёж): рамка и основная надпись; допуски формы и расположения и базы
// ГОСТ 2.308 на линиях вида кронштейна — положение снаружи контура, тексты, файл, висячие знаки.
TEST(DrawingDocumentIo, EmptySheetTolerancesAndDatums) {
    auto part = makeBracket();
    auto snap = snapshotPart(*part, {"РД.10.001", "Кронштейн", "Сталь 20", 7850.0});
    ASSERT_TRUE(snap.ok());
    const ModelSnapshot& m = snap.value();
    DrawingDocument d;
    d.model().kind = ModelKind::Part;
    d.model().path = "bracket_part.json";
    SheetDef s;
    s.id = "S1";
    s.format = {GostFormat::A3, 1, Orientation::Landscape};
    s.scale = views::ScaleRatio{1, 1};
    d.sheets().push_back(s);
    views::ViewCache cache(8, 1);
    ASSERT_TRUE(d.update(m, cache).ok());
    auto blank = d.render(dv2test::systemFont());
    ASSERT_TRUE(blank.ok()) << blank.error().message;
    std::size_t blankTexts = 0;
    for (const Entity& e : blank.value()[0].entities()) blankTexts += std::holds_alternative<Text>(e.geometry);
    EXPECT_GT(blankTexts, 0u);  // графы основной надписи
    EXPECT_EQ(d.computedSheets()[0].title.scale, "1:1");
    EXPECT_TRUE(d.computedSheets()[0].views.empty());

    ViewDef f;
    f.id = "F";
    d.sheets()[0].views.push_back(f);
    auto edge = [&](Vec3 a, Vec3 b) {
        auto i = lineEdge(m, a, b);
        EXPECT_TRUE(i.has_value());
        return elementReference(m, ElementKind::Edge, i.value_or(0)).value();
    };
    // База А — нижняя линия (z = 0), параллельность 0,02 к А — верх вертикальной полки (z = B = 50),
    // плоскостность 0,05 — левая линия (x = 0).
    d.sheets()[0].datums.push_back({"BA", "F", edge({0, -15, 0}, {60, -15, 0}), 0.5, "А"});
    ToleranceItem par{"TP", "F", edge({0, -15, 50}, {8, -15, 50}), 0.5, {}};
    par.frame.characteristic = annotations::GeometricCharacteristic::Parallelism;
    par.frame.tolerance = "0,02";
    par.frame.datums = {"А"};
    d.sheets()[0].tolerances.push_back(par);
    ToleranceItem flat{"TF", "F", edge({0, -15, 0}, {0, -15, 50}), 0.5, {}};
    flat.frame.characteristic = annotations::GeometricCharacteristic::Flatness;
    flat.frame.tolerance = "0,05";
    d.sheets()[0].tolerances.push_back(flat);
    auto rep = d.update(m, cache);
    ASSERT_TRUE(rep.ok()) << rep.error().message;
    EXPECT_TRUE(rep.value().dangling.empty());
    const ComputedSheet& cs = d.computedSheets()[0];
    const views::SheetView& sv = cs.views[0].sheetView;
    const double bottomY = sv.toSheet({0, 0}).y, topY = sv.toSheet({0, 50}).y, leftX = sv.toSheet({0, 0}).x;
    ASSERT_EQ(cs.datums.size(), 1u);
    ASSERT_TRUE(cs.datums[0].has_value());
    // Треугольник базы: основание на нижней линии, вершина — ниже (снаружи детали) на h = 3,5.
    ASSERT_FALSE(cs.datums[0]->fills.empty());
    double minY = 1e300, maxY = -1e300;
    for (const Point2& p : cs.datums[0]->fills[0].points) {
        minY = std::min(minY, p.y);
        maxY = std::max(maxY, p.y);
    }
    EXPECT_NEAR(maxY, bottomY, 1e-6);
    EXPECT_NEAR(maxY - minY, 3.5, 1e-6);
    bool letterA = false;
    for (const auto& l : cs.datums[0]->labels) letterA = letterA || l.text == "А";
    EXPECT_TRUE(letterA);
    // Рамки допусков: параллельность — выше верхней линии, плоскостность — левее левой линии.
    ASSERT_EQ(cs.tolerances.size(), 2u);
    ASSERT_TRUE(cs.tolerances[0] && cs.tolerances[1]);
    auto texts = [](const annotations::SymbolGeometry& g) {
        std::string all;
        for (const auto& l : g.labels) all += l.text + "|";
        return all;
    };
    EXPECT_NE(texts(*cs.tolerances[0]).find("0,02"), std::string::npos);
    EXPECT_NE(texts(*cs.tolerances[0]).find("А"), std::string::npos);
    EXPECT_NE(texts(*cs.tolerances[1]).find("0,05"), std::string::npos);
    for (const auto& l : cs.tolerances[0]->labels) EXPECT_GT(l.position.y, topY + 5.0);
    for (const auto& l : cs.tolerances[1]->labels) EXPECT_LT(l.position.x, leftX - 5.0);
    // Стрелка соединительной линии — на линии поверхности.
    bool arrowOnTop = false;
    for (const auto& fl : cs.tolerances[0]->fills)
        for (const Point2& p : fl.points) arrowOnTop = arrowOnTop || std::abs(p.y - topY) < 1e-6;
    EXPECT_TRUE(arrowOnTop);

    auto sheets = d.render(dv2test::systemFont());
    ASSERT_TRUE(sheets.ok()) << sheets.error().message;
    const std::string dir = dv2test::outPath("io");
    std::filesystem::create_directories(dir);
    ASSERT_TRUE(saveToFile(dir + "/tolerances.dxf", writeDxf(sheets.value()[0]).value()).ok());
    const DxfContent dxf = readDxf(dir + "/tolerances.dxf");
    EXPECT_TRUE(hasText(dxf, "0,02"));
    EXPECT_TRUE(hasText(dxf, "0,05"));

    // Формат файла: сохранение → загрузка → тот же текст.
    const std::string text = d.toJson().value();
    auto loaded = DrawingDocument::fromJson(text);
    ASSERT_TRUE(loaded.ok()) << loaded.error().message;
    EXPECT_EQ(loaded.value().toJson().value(), text);
    ASSERT_EQ(loaded.value().sheets()[0].tolerances.size(), 2u);
    EXPECT_EQ(loaded.value().sheets()[0].tolerances[0].frame.datums, std::vector<std::string>{"А"});
    EXPECT_EQ(loaded.value().sheets()[0].datums[0].letter, "А");

    // Проверка определений: допуск расположения без базы, допуск формы с базой.
    DrawingDocument bad = loaded.value();
    bad.sheets()[0].tolerances[0].frame.datums.clear();
    EXPECT_EQ(bad.update(m, cache).error().code, ErrorCode::InvalidInput);
    bad = loaded.value();
    bad.sheets()[0].tolerances[1].frame.datums = {"А"};
    EXPECT_EQ(bad.update(m, cache).error().code, ErrorCode::InvalidInput);

    // Ребро, которое не изображено прямой линией на виде (окружность отверстия на виде спереди
    // не видна), — висячий знак, лист строится.
    DrawingDocument hole = loaded.value();
    hole.sheets()[0].datums[0].edge = elementReference(m, ElementKind::Edge, circleEdge(m, {40, 0, 8}, 4).value()).value();
    auto rh = hole.update(m, cache);
    ASSERT_TRUE(rh.ok()) << rh.error().message;
    ASSERT_EQ(rh.value().dangling.size(), 1u);
    EXPECT_EQ(rh.value().dangling[0].rfind("S1/BA: ", 0), 0u) << rh.value().dangling[0];
    EXPECT_FALSE(hole.computedSheets()[0].datums[0].has_value());
    EXPECT_TRUE(hole.render(dv2test::systemFont()).ok());
}
