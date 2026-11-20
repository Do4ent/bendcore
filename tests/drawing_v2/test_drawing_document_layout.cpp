// Свободное размещение видов вне проекционной связи (docs/drawing/DRAWING_DOCUMENT.md,
// «Компоновка листа»): явное положение снимает ограничение ячейкой автокомпоновки, масштаб —
// заданный, перекрытие видов — предупреждение. Эталон — геометрия модели: габарит вида на листе
// = габарит вида в мм модели × масштаб, центр — заданная точка.

#include "test_drawing_document_support.hpp"

#include <bendcore/drawing_v2/export.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <iostream>

using namespace ddtest;
namespace vw = bendcore::drawing_v2::views;

namespace {

const ComputedView* computedView(const DrawingDocument& d, const std::string& sheet, const std::string& id) {
    for (const ComputedSheet& cs : d.computedSheets()) {
        if (cs.id != sheet) continue;
        for (const ComputedView& v : cs.views)
            if (v.id == id) return &v;
    }
    return nullptr;
}

ViewDef* viewDef(DrawingDocument& d, const std::string& sheet, const std::string& id) {
    for (ViewDef& v : d.findSheet(sheet)->views)
        if (v.id == id) return &v;
    return nullptr;
}

bool hasWarning(const UpdateReport& r, const std::string& part) {
    return std::any_of(r.warnings.begin(), r.warnings.end(), [&](const std::string& w) { return w.find(part) != std::string::npos; });
}

struct Box4 {
    double x0, y0, x1, y1;
};
Box4 sheetBox(const ComputedView& v, double labelRoom) {
    const vw::Box2 b = v.sheetView.sheetBounds();
    return {b.minX, b.minY, b.maxX, b.maxY + labelRoom};
}
bool intersects(const Box4& a, const Box4& b) {
    return std::min(a.x1, b.x1) > std::max(a.x0, b.x0) && std::min(a.y1, b.y1) > std::max(a.y0, b.y0);
}

// Кронштейн: главный вид и вид сверху; разрез вне связи — по выбору масштаба и положения.
DrawingDocument bracketDrawing() {
    DrawingDocument d;
    SheetDef sd;
    sd.id = "S1";
    sd.format = {GostFormat::A3, 1, Orientation::Landscape};
    ViewDef f;
    f.id = "F";
    ViewDef t;
    t.id = "T";
    t.kind = ViewKind::Projected;
    t.parent = "F";
    t.side = vw::SheetSide::Below;
    ViewDef a;
    a.id = "A";
    a.kind = ViewKind::Section;
    a.toward = {-1, 0, 0};
    a.right = {0, -1, 0};
    a.planePoint = {40, 0, 0};
    a.markView = "F";
    sd.views = {f, t, a};
    d.sheets().push_back(std::move(sd));
    return d;
}

}  // namespace

TEST(DrawingDocumentLayout, ExplicitPositionIgnoresTheCellAndKeepsTheChosenScale) {
    auto part = makeBracket();
    const ModelSnapshot m = snapshotPart(*part, {"РД.01", "Кронштейн", "Сталь 45", 7850}).value();
    vw::ViewCache cache;
    DrawingDocument d = bracketDrawing();
    auto r0 = d.update(m, cache);
    ASSERT_TRUE(r0) << r0.error().message;
    const ComputedView* a0 = computedView(d, "S1", "A");
    ASSERT_NE(a0, nullptr);
    const vw::ScaleRatio sheetScale = d.computedSheets()[0].scale;
    const double w0 = a0->sheetView.view.bounds.width();
    const double h0 = a0->sheetView.view.bounds.height();

    // Масштаб 2:1 в автоматической ячейке справа не помещается: без положения — предупреждение,
    // масштаб остаётся заданным (не уменьшается до ячейки).
    viewDef(d, "S1", "A")->scale = vw::ScaleRatio{2, 1};
    auto r1 = d.update(m, cache);
    ASSERT_TRUE(r1) << r1.error().message;
    const ComputedView* a1 = computedView(d, "S1", "A");
    EXPECT_EQ(a1->scale.label(), "2:1");
    EXPECT_NEAR(a1->sheetView.scale, 2.0, 1e-12);
    if (sheetScale.value() < 2.0 - 1e-12) EXPECT_NE(a1->label.find("(2:1)"), std::string::npos) << a1->label;

    // Явное положение: центр габарита изображения (с местом надписи сверху) — в заданной точке,
    // размер — габарит модели × 2.
    const Point2 at{300, 150};
    viewDef(d, "S1", "A")->position = at;
    auto r2 = d.update(m, cache);
    ASSERT_TRUE(r2) << r2.error().message;
    EXPECT_FALSE(hasWarning(r2.value(), "does not fit its cell"));
    const ComputedView* a2 = computedView(d, "S1", "A");
    const vw::Box2 b = a2->sheetView.sheetBounds();
    EXPECT_NEAR(b.width(), 2.0 * w0, 1e-9);
    EXPECT_NEAR(b.height(), 2.0 * h0, 1e-9);
    EXPECT_NEAR(0.5 * (b.minX + b.maxX), at.x, 1e-9);
    EXPECT_NEAR(0.5 * (b.minY + b.maxY), at.y - 7.0, 1e-9);  // половина места надписи «А–А (2:1)»

    // Положение на главном виде: обновление проходит, перекрытие — предупреждение.
    const ComputedView* fv = computedView(d, "S1", "F");
    const vw::Box2 fb = fv->sheetView.sheetBounds();
    viewDef(d, "S1", "A")->position = Point2{0.5 * (fb.minX + fb.maxX), 0.5 * (fb.minY + fb.maxY)};
    auto r3 = d.update(m, cache);
    ASSERT_TRUE(r3) << r3.error().message;
    EXPECT_TRUE(hasWarning(r3.value(), "views 'F' and 'A' overlap"));
    // За рамкой и на основной надписи — тоже предупреждения.
    viewDef(d, "S1", "A")->position = Point2{405, 20};
    auto r4 = d.update(m, cache);
    ASSERT_TRUE(r4) << r4.error().message;
    EXPECT_TRUE(hasWarning(r4.value(), "view 'A' overlaps the title block"));
    EXPECT_TRUE(hasWarning(r4.value(), "view 'A' goes beyond the drawing frame"));
    // Положение сохраняется в файле.
    auto js = d.toJson();
    ASSERT_TRUE(js);
    auto back = DrawingDocument::fromJson(js.value());
    ASSERT_TRUE(back) << back.error().message;
    const ViewDef* ra = nullptr;
    for (const ViewDef& v : back.value().sheets()[0].views)
        if (v.id == "A") ra = &v;
    ASSERT_TRUE(ra && ra->position && ra->scale);
    EXPECT_NEAR(ra->position->x, 405, 1e-12);
    EXPECT_EQ(ra->scale->label(), "2:1");
}

// Сборочный чертёж редуктора (examples/gearbox): разрез Б–Б (по оси колеса), выпущенный в
// комплекте в масштабе 1:2,5 колонкой справа, ставится в масштабе 1:1 на свободное место листа А1.
TEST(DrawingDocumentLayout, GearboxSectionBAtFullScaleOnFreeSpace) {
    const std::filesystem::path dir = std::filesystem::path(BENDCORE_SOURCE_DIR) / "examples" / "gearbox";
    if (!std::filesystem::exists(dir / "gearbox.bcdrw.json")) GTEST_SKIP() << "examples/gearbox is missing";
    auto loaded = DrawingDocument::loadFile((dir / "gearbox.bcdrw.json").string());
    ASSERT_TRUE(loaded) << loaded.error().message;
    DrawingDocument d = std::move(loaded).value();
    const ModelLoader loader = fileModelLoader(dir.string());
    vw::ViewCache cache;
    auto r0 = d.update(loader, cache);
    ASSERT_TRUE(r0) << r0.error().message;
    const std::string sheet = d.sheets()[0].id;
    ASSERT_NE(viewDef(d, sheet, "B"), nullptr);
    // Проекционная группа (главный вид и разрез А–А) — к левому краю поля (положение главного
    // вида — центр группы), чтобы освободить место между ней и колонкой разрезов.
    {
        const ComputedView* f = computedView(d, sheet, "F");
        const ComputedView* a = computedView(d, sheet, "A");
        vw::Box2 g = f->sheetView.sheetBounds();
        const vw::Box2 ab = a->sheetView.sheetBounds();
        g.add({ab.minX, ab.minY});
        g.add({ab.maxX, ab.maxY + 14.0});
        viewDef(d, sheet, "F")->position = Point2{20 + 15 + g.width() / 2, 0.5 * (g.minY + g.maxY)};
        auto rg = d.update(loader, cache);
        ASSERT_TRUE(rg) << rg.error().message;
        const double left = std::min(computedView(d, sheet, "F")->sheetView.sheetBounds().minX,
                                     computedView(d, sheet, "A")->sheetView.sheetBounds().minX);
        EXPECT_NEAR(left, 35.0, 1e-6);
    }
    const ComputedView* b0 = computedView(d, sheet, "B");
    const double w = b0->sheetView.view.bounds.width(), h = b0->sheetView.view.bounds.height();
    const ComputedSheet& cs0 = d.computedSheets()[0];
    const double W = cs0.sheet.widthMm, H = cs0.sheet.heightMm;

    // Остальные изображения, основная надпись и место технических требований над ней.
    std::vector<Box4> busy;
    for (const ComputedView& v : cs0.views)
        if (v.id != "B") busy.push_back(sheetBox(v, v.kind == ViewKind::Section || v.kind == ViewKind::Detail ? 14.0 : 0.0));
    busy.push_back({W - 5 - 185, 5, W - 5, 5 + 55 + 110});
    // Свободное место для изображения 1:1 с надписью «Б–Б»: перебор центров с шагом 5 мм внутри
    // рамки (поля 20/5 мм ГОСТ 2.104), 10 мм до соседних изображений.
    std::optional<Point2> free;
    for (double cy = H - 15 - (h + 14) / 2; cy >= 15 + (h + 14) / 2 && !free; cy -= 5) {
        for (double cx = 30 + w / 2; cx <= W - 15 - w / 2 && !free; cx += 5) {
            const Box4 r{cx - w / 2 - 10, cy - (h + 14) / 2 - 10, cx + w / 2 + 10, cy + (h + 14) / 2 + 10};
            if (std::none_of(busy.begin(), busy.end(), [&](const Box4& q) { return intersects(r, q); })) free = Point2{cx, cy};
        }
    }
    if (!free) for (const Box4& q : busy) std::cerr << q.x0 << " " << q.y0 << " " << q.x1 << " " << q.y1 << "\n";
    ASSERT_TRUE(free) << "no free space " << w << "×" << h << " on the sheet";

    ViewDef* b = viewDef(d, sheet, "B");
    b->scale = vw::ScaleRatio{1, 1};
    b->position = *free;
    auto r1 = d.update(loader, cache);
    ASSERT_TRUE(r1) << r1.error().message;
    const ComputedView* b1 = computedView(d, sheet, "B");
    EXPECT_EQ(b1->scale.label(), "1:1");
    EXPECT_NEAR(b1->sheetView.scale, 1.0, 1e-12);
    const vw::Box2 bb = b1->sheetView.sheetBounds();
    EXPECT_NEAR(bb.width(), w, 1e-9);
    EXPECT_NEAR(bb.height(), h, 1e-9);
    EXPECT_NEAR(0.5 * (bb.minX + bb.maxX), free->x, 1e-9);
    // Масштаб листа 1:1 — в надписи разреза масштаба нет (ГОСТ 2.305, разд. 6).
    if (std::abs(d.computedSheets()[0].scale.value() - 1.0) < 1e-12) EXPECT_EQ(b1->label, b1->letter + "–" + b1->letter);
    for (const std::string& wmsg : r1.value().warnings) {
        EXPECT_EQ(wmsg.find("'B'"), std::string::npos) << wmsg;
        EXPECT_EQ(wmsg.find("does not fit"), std::string::npos) << wmsg;
    }
    // Проекционная группа осталась на месте; разрезы В–В и Г–Г без положения — в той же колонке
    // (ячейки делятся между видами без явного положения, их стало две).
    for (const ComputedView& v : d.computedSheets()[0].views) {
        if (v.id == "B") continue;
        const ComputedView* before = nullptr;
        for (const ComputedView& u : cs0.views)
            if (u.id == v.id) before = &u;
        ASSERT_NE(before, nullptr);
        EXPECT_NEAR(v.sheetView.anchor.x, before->sheetView.anchor.x, 1e-9) << v.id;
        if (v.id == "F" || v.id == "A") EXPECT_NEAR(v.sheetView.anchor.y, before->sheetView.anchor.y, 1e-9) << v.id;
    }
    auto pages = d.render(nullptr);
    ASSERT_TRUE(pages) << pages.error().message;
    const std::string out = std::string(BENDCORE_DRAWING_V2_OUT) + "/gearbox_section_b_1to1.svg";
    ASSERT_TRUE(saveToFile(out, writeSvg(pages.value()[0]).value()));
}
