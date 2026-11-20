// Размеры на фигуре сечения и разреза (docs/drawing/ANNOTATIONS.md, «Размеры на сечениях»):
// точки привязки — линии контура сечения, порождённые гранями модели (история булевой операции
// разреза), ассоциативный пересчёт после изменения модели. Деталь — эталонный вал со шпоночным
// пазом (docs/modeling/REFERENCE_PARTS.md, №2): ось X, ступень R1 = 15 на x ∈ [0, 60], паз шириной
// b, глубиной t от образующей z = R1, длиной lk с центром x = kx. Эталон — параметры модели.

#include "test_drawing_document_support.hpp"

#include <bendcore/drawing_v2/export.hpp>

#include <BRepAdaptor_Surface.hxx>
#include <TopoDS.hxx>
#include <gp_Pln.hxx>
#include <gtest/gtest.h>


using namespace ddtest;
namespace vw = bendcore::drawing_v2::views;
using bendcore::modeling::ElementKind;

namespace {

// Плоская грань с нормалью ±n через точку p (единственная).
std::optional<std::uint32_t> planeFace(const ModelSnapshot& m, Vec3 p, Vec3 n, double tol = 1e-6) {
    std::optional<std::uint32_t> out;
    const std::size_t count = m.named->count(ElementKind::Face);
    for (std::size_t i = 0; i < count; ++i) {
        BRepAdaptor_Surface s(TopoDS::Face(m.named->element(ElementKind::Face, i)));
        if (s.GetType() != GeomAbs_Plane) continue;
        const gp_Pln pl = s.Plane();
        if (!pl.Axis().Direction().IsParallel(gp_Dir(n.x, n.y, n.z), 1e-9)) continue;
        if (pl.Distance(gp_Pnt(p.x, p.y, p.z)) > tol) continue;
        if (out) return std::nullopt;
        out = std::uint32_t(i);
    }
    return out;
}

annotations::Anchor onSection(const ModelSnapshot& m, std::optional<std::uint32_t> face, Point2 at) {
    annotations::Anchor a = anchorOn(m, ElementKind::Face, face, annotations::AnchorKind::Section);
    a.at = at;
    return a;
}

struct Shaft {
    double R1 = 15, b = 6, t = 4, kx = 25;
};

void addDimensions(DrawingDocument& d, const ModelSnapshot& m, const Shaft& s) {
    SheetDef& sd = d.sheets()[0];
    const auto side1 = planeFace(m, {s.kx, -s.b / 2, s.R1 - 1}, {0, 1, 0});
    const auto side2 = planeFace(m, {s.kx, s.b / 2, s.R1 - 1}, {0, 1, 0});
    const auto bottom = planeFace(m, {s.kx, 0, s.R1 - s.t}, {0, 0, 1});
    const auto cyl = cylinderFace(m, s.R1, {0, 0, 0}, {1, 0, 0});
    // Сечение C (взгляд вдоль −X, x вида = y модели, y вида = z): ширина паза b между стенками,
    // d − t1 — от дна паза до нижней образующей (квадрант окружности сечения).
    sd.dimensions.push_back(dimension("b", "C", annotations::DimensionType::Horizontal,
                                      {onSection(m, side1, {-s.b / 2, s.R1 - 1}), onSection(m, side2, {s.b / 2, s.R1 - 1})}, 8));
    sd.dimensions.push_back(dimension("dt", "C", annotations::DimensionType::Vertical,
                                      {onSection(m, bottom, {0, s.R1 - s.t}), onSection(m, cyl, {0.4, -s.R1 - 0.2})}, -12));
    // Продольный разрез A по оси (x вида = x модели, y вида = z): глубина t1 от образующей z = R1
    // (линия сечения цилиндра вне паза) до дна паза; диаметр — две линии сечения цилиндра.
    sd.dimensions.push_back(dimension("t1", "A", annotations::DimensionType::Vertical,
                                      {onSection(m, bottom, {s.kx, s.R1 - s.t}), onSection(m, cyl, {5, s.R1})}, -10));
    sd.dimensions.push_back(dimension("d", "A", annotations::DimensionType::Diameter,
                                      {anchorOn(m, ElementKind::Face, cyl, annotations::AnchorKind::Center)}, 6));
}

DrawingDocument shaftDrawing() {
    DrawingDocument d;
    SheetDef sd;
    sd.id = "S1";
    sd.format = {GostFormat::A3, 1, Orientation::Landscape};
    sd.scale = vw::ScaleRatio{2, 1};
    ViewDef f;
    f.id = "F";
    ViewDef t;
    t.id = "T";
    t.kind = ViewKind::Projected;
    t.parent = "F";
    t.side = vw::SheetSide::Below;
    ViewDef a;  // продольный разрез на месте главного вида не нужен — вне связи
    a.id = "A";
    a.kind = ViewKind::Section;
    a.toward = {0, -1, 0};
    a.right = {1, 0, 0};
    a.planePoint = {0, 0, 0};
    a.markView = "T";
    a.position = Point2{290, 230};
    ViewDef c;
    c.id = "C";
    c.kind = ViewKind::Section;
    c.sectionKind = vw::SectionKind::Slice;
    c.toward = {1, 0, 0};
    c.right = {0, 1, 0};
    c.planePoint = {25, 0, 0};
    c.markView = "F";
    c.position = Point2{330, 130};
    sd.views = {f, t, a, c};
    d.sheets().push_back(std::move(sd));
    return d;
}

}  // namespace

TEST(DrawingDocumentSectionDimensions, KeywayDepthAndWidthOnShaftSections) {
    auto part = makeShaft();
    ModelSnapshot m = snapshotPart(*part, {"РД.02", "Вал", "Сталь 45", 7850}).value();
    DrawingDocument d = shaftDrawing();
    Shaft s;
    addDimensions(d, m, s);
    vw::ViewCache cache;
    auto r = d.update(m, cache);
    ASSERT_TRUE(r) << r.error().message;
    ASSERT_TRUE(r.value().dangling.empty()) << r.value().dangling.front();
    auto value = [&](const std::string& id) {
        const auto* res = dimensionResult(d, "S1", id);
        EXPECT_NE(res, nullptr) << id;
        if (!res) return -1.0;
        EXPECT_EQ(res->state, annotations::DimensionState::Ok) << id << ": " << res->reason;
        return res->value;
    };
    EXPECT_NEAR(value("b"), s.b, 1e-6);
    EXPECT_NEAR(value("dt"), 2 * s.R1 - s.t, 1e-6);  // d − t1 = 26
    EXPECT_NEAR(value("t1"), s.t, 1e-6);
    EXPECT_NEAR(value("d"), 2 * s.R1, 1e-6);
    EXPECT_EQ(dimensionResult(d, "S1", "dt")->text, "26");
    // Точка на окружности сечения — ровно нижний квадрант (подсказка смещена на 0,4 мм по x).
    const DimensionItem& dt = d.sheets()[0].dimensions[1];
    EXPECT_NEAR(dt.def.anchors[1].at.x, 0.0, 1e-12);
    EXPECT_NEAR(dt.def.anchors[1].at.y, -s.R1, 1e-9);

    // Файл: точки контура сечения сохраняются с подсказкой и читаются обратно.
    auto js = d.toJson();
    ASSERT_TRUE(js);
    EXPECT_NE(js.value().find("\"anchor\": \"section\""), std::string::npos);
    auto back = DrawingDocument::fromJson(js.value());
    ASSERT_TRUE(back) << back.error().message;
    EXPECT_EQ(back.value().sheets()[0].dimensions[0].def.anchors[0].kind, annotations::AnchorKind::Section);
    EXPECT_NEAR(back.value().sheets()[0].dimensions[0].def.anchors[0].at.x, -s.b / 2, 1e-9);

    // Ассоциативность: паз 8 × 5 — размеры следуют за гранями (имена из истории операций).
    setVariable(*part, "b", 8);
    setVariable(*part, "t", 5);
    ASSERT_TRUE(part->rebuild().failed.empty());
    m = snapshotPart(*part, {"РД.02", "Вал", "Сталь 45", 7850}).value();
    auto r2 = d.update(m, cache);
    ASSERT_TRUE(r2) << r2.error().message;
    ASSERT_TRUE(r2.value().dangling.empty()) << r2.value().dangling.front();
    EXPECT_NEAR(value("b"), 8, 1e-6);
    EXPECT_NEAR(value("dt"), 25, 1e-6);
    EXPECT_NEAR(value("t1"), 5, 1e-6);
    EXPECT_NEAR(value("d"), 30, 1e-6);
    EXPECT_EQ(r2.value().stale.size(), 3u);

    // Вывод: размерные линии в DXF — длиной значение × масштаб 2:1, числа 8, 25, 5.
    auto pages = d.render(nullptr);
    ASSERT_TRUE(pages) << pages.error().message;
    const std::string path = std::string(BENDCORE_DRAWING_V2_OUT) + "/shaft_section_dimensions.dxf";
    ASSERT_TRUE(saveToFile(path, writeDxf(pages.value()[0]).value()));
    const DxfContent dxf = readDxf(path);
    EXPECT_TRUE(hasDimLine(dxf, 16.0, true, 1e-6));
    EXPECT_TRUE(hasDimLine(dxf, 50.0, false, 1e-6));
    EXPECT_TRUE(hasDimLine(dxf, 10.0, false, 1e-6));
    for (const char* t : {"8", "25", "5", "⌀30"}) EXPECT_TRUE(hasText(dxf, t)) << t;
}

// Привязка к грани без линий сечения на виде — висячий размер с причиной; ссылка не на грань —
// ошибка определения.
TEST(DrawingDocumentSectionDimensions, FaceWithoutSectionContourIsDangling) {
    auto part = makeShaft();
    ModelSnapshot m = snapshotPart(*part, {"РД.02", "Вал", "Сталь 45", 7850}).value();
    DrawingDocument d = shaftDrawing();
    Shaft s;
    // Торец x = 0 (с фаской — плоскость x = 0) не пересекается плоскостью сечения x = 25.
    const auto end = planeFace(m, {0, 0, 0}, {1, 0, 0});
    const auto side1 = planeFace(m, {s.kx, -s.b / 2, s.R1 - 1}, {0, 1, 0});
    d.sheets()[0].dimensions.push_back(
        dimension("x", "C", annotations::DimensionType::Horizontal, {onSection(m, end, {0, 0}), onSection(m, side1, {-3, 13})}));
    vw::ViewCache cache;
    auto r = d.update(m, cache);
    ASSERT_TRUE(r) << r.error().message;
    ASSERT_EQ(r.value().dangling.size(), 1u);
    EXPECT_NE(r.value().dangling[0].find("has no section contour in the view"), std::string::npos) << r.value().dangling[0];

    annotations::DimensionDef bad;
    bad.type = annotations::DimensionType::Horizontal;
    bad.anchors = {anchorOn(m, ElementKind::Edge, std::uint32_t(0), annotations::AnchorKind::Section),
                   onSection(m, side1, {-3, 13})};
    const ComputedView* c = nullptr;
    for (const ComputedView& v : d.computedSheets()[0].views)
        if (v.id == "C") c = &v;
    ASSERT_NE(c, nullptr);
    auto binding = annotations::ModelBinding::fromNamedShape(*m.named).value();
    auto ev = annotations::evaluateDimension(bad, binding, c->sheetView);
    ASSERT_FALSE(ev);
    EXPECT_NE(ev.error().message.find("anchor Section needs a face reference"), std::string::npos);
}
