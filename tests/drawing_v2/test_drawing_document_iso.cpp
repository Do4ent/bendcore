// Оформление листа ISO (рамка ISO 5457, основная надпись ISO 7200 со знаком метода проекций
// ISO 5456-2), метод проекций документа (первый / третий угол, ГОСТ 2.305, ISO 128-30) и
// дополнительный вид по стрелке с буквой (ГОСТ 2.305, п. 5.4–5.6). Эталон — геометрия модели:
// ориентация вида — проекция вершин модели, раскладка — положение габаритов видов на листе.

#include "test_drawing_document_support.hpp"

#include <bendcore/drawing_v2/export.hpp>

#include <BRep_Tool.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <gp_Pnt.hxx>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <set>

using namespace ddtest;
namespace vw = bendcore::drawing_v2::views;

namespace {

const ComputedView* computedView(const DrawingDocument& d, const std::string& id) {
    for (const ComputedView& v : d.computedSheets().at(0).views)
        if (v.id == id) return &v;
    return nullptr;
}

Point2 center(const ComputedView& v) {
    const vw::Box2 b = v.sheetView.sheetBounds();
    return {0.5 * (b.minX + b.maxX), 0.5 * (b.minY + b.maxY)};
}

bool hasWarning(const UpdateReport& r, const std::string& part) {
    return std::any_of(r.warnings.begin(), r.warnings.end(), [&](const std::string& w) { return w.find(part) != std::string::npos; });
}

// Главный вид, вид сверху и вид слева (ЕСКД: сверху — под главным, слева — справа от него).
DrawingDocument bracketThreeViews() {
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
    t.side = *vw::standardPlacement(vw::StandardView::Top, vw::ProjectionMethod::FirstAngle);
    ViewDef l;
    l.id = "L";
    l.kind = ViewKind::Projected;
    l.parent = "F";
    l.side = *vw::standardPlacement(vw::StandardView::Left, vw::ProjectionMethod::FirstAngle);
    sd.views = {f, t, l};
    sd.title.organization = "ООО «Бенд»";
    sd.title.developer.surname = "Иванов";
    sd.title.approver.surname = "Петров";
    d.sheets().push_back(std::move(sd));
    return d;
}

// Габарит проекции вершин модели на плоскость вида (мм модели).
vw::Box2 vertexExtent(const ModelSnapshot& m, const vw::ViewFrame& f) {
    vw::Box2 b;
    for (TopExp_Explorer ex(m.shape.occt(), TopAbs_VERTEX); ex.More(); ex.Next()) {
        const gp_Pnt p = BRep_Tool::Pnt(TopoDS::Vertex(ex.Current()));
        b.add(f.project({p.X(), p.Y(), p.Z()}));
    }
    return b;
}

std::vector<const Entity*> entitiesOn(const Drawing& d, const std::string& layer) {
    std::vector<const Entity*> out;
    for (const auto& e : d.entities())
        if (e.layer == layer) out.push_back(&e);
    return out;
}

}  // namespace

TEST(DrawingDocumentIso, ThirdAngleMovesProjectedViewsAndKeepsTheirContent) {
    auto part = makeBracket();
    const ModelSnapshot m = snapshotPart(*part, {"РД.01", "Кронштейн", "Сталь 45", 7850}).value();
    vw::ViewCache cache;
    DrawingDocument d = bracketThreeViews();
    auto r0 = d.update(m, cache);
    ASSERT_TRUE(r0) << r0.error().message;
    EXPECT_FALSE(hasWarning(r0.value(), "third-angle"));
    const Point2 f0 = center(*computedView(d, "F")), t0 = center(*computedView(d, "T")), l0 = center(*computedView(d, "L"));
    // Первый угол: вид сверху под главным, вид слева справа от главного.
    EXPECT_LT(t0.y, f0.y);
    EXPECT_NEAR(t0.x, f0.x, 1e-6);
    EXPECT_GT(l0.x, f0.x);
    EXPECT_NEAR(l0.y, f0.y, 1e-6);
    const vw::ViewFrame topFrame = computedView(d, "T")->sheetView.view.frame;
    const vw::ViewFrame leftFrame = computedView(d, "L")->sheetView.view.frame;
    const vw::Box2 topBounds = computedView(d, "T")->sheetView.view.bounds;

    // Третий угол (ISO 128-30, метод A): вид сверху — над главным, вид слева — слева от него.
    EXPECT_EQ(d.setProjectionMethod(vw::ProjectionMethod::ThirdAngle), 2u);
    EXPECT_EQ(d.setProjectionMethod(vw::ProjectionMethod::ThirdAngle), 0u);
    EXPECT_EQ(*d.sheets()[0].views[1].side, vw::SheetSide::Above);
    EXPECT_EQ(*d.sheets()[0].views[2].side, vw::SheetSide::LeftOf);
    auto r1 = d.update(m, cache);
    ASSERT_TRUE(r1) << r1.error().message;
    // В оформлении ЕСКД третий угол — предупреждение (ГОСТ 2.305 его не предусматривает).
    EXPECT_TRUE(hasWarning(r1.value(), "third-angle"));
    const Point2 f1 = center(*computedView(d, "F")), t1 = center(*computedView(d, "T")), l1 = center(*computedView(d, "L"));
    EXPECT_GT(t1.y, f1.y);
    EXPECT_NEAR(t1.x, f1.x, 1e-6);
    EXPECT_LT(l1.x, f1.x);
    EXPECT_NEAR(l1.y, f1.y, 1e-6);
    // Изображения не изменились: та же ориентация и тот же габарит; HLR — из кэша.
    const vw::ViewFrame tf = computedView(d, "T")->sheetView.view.frame, lf = computedView(d, "L")->sheetView.view.frame;
    for (auto [a, b] : {std::pair{tf.toward, topFrame.toward}, std::pair{tf.right, topFrame.right},
                        std::pair{lf.toward, leftFrame.toward}, std::pair{lf.right, leftFrame.right}}) {
        EXPECT_NEAR(a.x, b.x, 1e-12);
        EXPECT_NEAR(a.y, b.y, 1e-12);
        EXPECT_NEAR(a.z, b.z, 1e-12);
    }
    EXPECT_NEAR(computedView(d, "T")->sheetView.view.bounds.width(), topBounds.width(), 1e-9);
    EXPECT_EQ(r1.value().viewsComputed, 0u);
    // Вид сверху — наблюдатель сверху: toward = +Z (ось Z модели вверх у главного вида).
    EXPECT_NEAR(tf.toward.z, 1.0, 1e-12);

    // Метод сохраняется в файле; возврат к первому углу возвращает раскладку.
    auto back = DrawingDocument::fromJson(d.toJson().value());
    ASSERT_TRUE(back) << back.error().message;
    EXPECT_EQ(back.value().settings().projection, vw::ProjectionMethod::ThirdAngle);
    EXPECT_EQ(d.setProjectionMethod(vw::ProjectionMethod::FirstAngle), 2u);
    ASSERT_TRUE(d.update(m, cache));
    EXPECT_NEAR(center(*computedView(d, "T")).y, t0.y, 1e-6);
}

TEST(DrawingDocumentIso, IsoSheetStyleTitleBlockAndProjectionSymbolInExport) {
    auto part = makeBracket();
    const ModelSnapshot m = snapshotPart(*part, {"РД.01", "Кронштейн", "Сталь 45", 7850}).value();
    vw::ViewCache cache;
    for (auto method : {vw::ProjectionMethod::FirstAngle, vw::ProjectionMethod::ThirdAngle}) {
        DrawingDocument d = bracketThreeViews();
        d.model().properties = {"РД.01", "Кронштейн", "Сталь 45", 7850};
        d.settings().sheetStyle = SheetStyle::Iso7200;
        d.setProjectionMethod(method);
        TitleFields& t = d.sheets()[0].title;
        t.documentStatus = "Выпущен";
        t.revision = "B";
        t.issueDate = "2026-11-21";
        t.department = "КБ-3";
        auto r = d.update(m, cache);
        ASSERT_TRUE(r) << r.error().message;
        EXPECT_FALSE(hasWarning(r.value(), "third-angle"));
        const ComputedSheet& cs = d.computedSheets()[0];
        EXPECT_EQ(cs.sheet.standard, SheetStandard::Iso216);
        const Iso7200Data& iso = cs.isoTitle;
        EXPECT_EQ(iso.identification, "РД.01");
        EXPECT_EQ(iso.title, "Кронштейн");
        EXPECT_EQ(iso.legalOwner, "ООО «Бенд»");
        EXPECT_EQ(iso.creator, "Иванов");
        EXPECT_EQ(iso.approver, "Петров");
        EXPECT_EQ(iso.documentType, "Чертёж детали");
        EXPECT_EQ(iso.language, "ru");
        EXPECT_EQ(iso.scale, cs.scale.label());
        EXPECT_EQ(iso.projection, method == vw::ProjectionMethod::FirstAngle ? ProjectionSymbol::FirstAngle : ProjectionSymbol::ThirdAngle);
        // Виды — внутри рамки ISO 5457 (поля 20/10) и не на основной надписи 180×36.
        for (const ComputedView& v : cs.views) {
            const vw::Box2 b = v.sheetView.sheetBounds();
            EXPECT_GE(b.minX, 20.0);
            EXPECT_GE(b.minY, 10.0);
            EXPECT_LE(b.maxX, 410.0);
            EXPECT_LE(b.maxY, 287.0);
            EXPECT_FALSE(b.maxX > 230.0 && b.minY < 46.0) << v.id << " overlaps the ISO title block";
        }

        auto drawings = d.render(nullptr);
        ASSERT_TRUE(drawings) << drawings.error().message;
        const Drawing& dr = drawings.value()[0];
        // Знак метода проекций — в ячейке (230…245) × (28…37): окружности h/2 и h (h = 2,5).
        std::vector<Circle> circles;
        std::vector<LineSeg> trapezoid;
        for (const Entity* e : entitiesOn(dr, "FRAME")) {
            if (const auto* c = std::get_if<Circle>(&e->geometry)) circles.push_back(*c);
            if (const auto* l = std::get_if<LineSeg>(&e->geometry);
                l && e->lineType == LineType::SolidThin && l->a.x > 230.0 && l->a.x < 245.0 && l->b.x > 230.0 && l->b.x < 245.0 &&
                l->a.y > 28.0 && l->a.y < 37.0)
                trapezoid.push_back(*l);
        }
        ASSERT_EQ(circles.size(), 2u);
        ASSERT_EQ(trapezoid.size(), 4u);
        double tx = 0;
        for (const LineSeg& l : trapezoid) tx += 0.125 * (l.a.x + l.b.x);
        EXPECT_NEAR(circles[0].center.y, 10.0 + 22.5, 1e-9);
        if (method == vw::ProjectionMethod::FirstAngle)
            EXPECT_GT(circles[0].center.x, tx) << "first angle: end view to the right of the cone";
        else
            EXPECT_LT(circles[0].center.x, tx) << "third angle: end view to the left of the cone";

        // DXF: тексты полей ISO и нижняя линия рамки ISO 5457 (y = 10 мм).
        const std::string path = dv2test::outPath(std::string("iso_sheet_") + (method == vw::ProjectionMethod::FirstAngle ? "e" : "a") + ".dxf");
        saveToFile(path, writeDxf(dr).value()).value();
        const DxfContent dxf = readDxf(path);
        for (const char* want : {"Владелец", "Обозначение", "Наименование", "Тип документа", "Статус документа", "Дата выпуска",
                                 "Разработал", "Утвердил", "Отдел", "РД.01", "Кронштейн", "Чертёж детали", "Выпущен", "B",
                                 "2026-11-21", "1/1", "ru", "Иванов", "Петров", "КБ-3"})
            EXPECT_TRUE(hasText(dxf, want)) << want;
        EXPECT_FALSE(hasText(dxf, "Разраб.")) << "no GOST 2.104 title block on an ISO sheet";
        bool frameBottom = false;
        for (const auto& l : dxf.lines)
            frameBottom = frameBottom || (std::abs(l.y0 - 10) < 1e-6 && std::abs(l.y1 - 10) < 1e-6 &&
                                          std::abs(std::abs(l.x1 - l.x0) - 390.0) < 1e-6);
        EXPECT_TRUE(frameBottom);
        EXPECT_EQ(ezdxfCheck(path, "Кронштейн"), "");
        // PDF листа A3 в пунктах.
        const std::string pdf = writePdf(dr).value();
        saveToFile(dv2test::outPath("iso_sheet.pdf"), pdf).value();
        EXPECT_NE(pdf.find("/MediaBox [0 0 1190.5512 841.8898]"), std::string::npos);

        // Оформление и поля ISO сохраняются в файле.
        auto back = DrawingDocument::fromJson(d.toJson().value());
        ASSERT_TRUE(back) << back.error().message;
        EXPECT_EQ(back.value().settings().sheetStyle, SheetStyle::Iso7200);
        EXPECT_EQ(back.value().sheets()[0].title.revision, "B");
        EXPECT_EQ(back.value().sheets()[0].title.issueDate, "2026-11-21");
        EXPECT_EQ(back.value().sheets()[0].title.department, "КБ-3");
    }
}

TEST(DrawingDocumentIso, AuxiliaryViewArrowLetterAndRotatedLabel) {
    auto part = makeBracket();
    const ModelSnapshot m = snapshotPart(*part, {"РД.01", "Кронштейн", "Сталь 45", 7850}).value();
    vw::ViewCache cache;
    DrawingDocument d = bracketThreeViews();
    // Наблюдатель слева сверху под 45° в плоскости главного вида: взгляд (1, 0, −1)/√2.
    const double k = 1.0 / std::sqrt(2.0);
    ViewDef a;
    a.id = "A";
    a.kind = ViewKind::Auxiliary;
    a.parent = "F";
    a.toward = {-k, 0, k};
    d.sheets()[0].views.push_back(a);
    auto r = d.update(m, cache);
    ASSERT_TRUE(r) << r.error().message;
    const ComputedView* av = computedView(d, "A");
    const ComputedView* fv = computedView(d, "F");
    ASSERT_TRUE(av && fv);
    EXPECT_EQ(av->letter, "А");
    EXPECT_EQ(av->label, "А");
    EXPECT_EQ(av->rotationDeg, 0.0);
    // Ориентация: направление взгляда −toward; «вправо» вида — по стрелке ⟂: toward главного вида
    // (−Y) на листе дополнительного вида идёт по стрелке (вправо-вниз).
    const vw::ViewFrame f = av->sheetView.view.frame;
    EXPECT_NEAR(f.toward.x, -k, 1e-12);
    EXPECT_NEAR(f.toward.z, k, 1e-12);
    const Point2 nOnSheet = f.project({0, -1, 0});
    EXPECT_NEAR(nOnSheet.x, k, 1e-12);
    EXPECT_NEAR(nOnSheet.y, -k, 1e-12);
    // Габарит изображения — габарит проекции вершин модели (кронштейн ограничен плоскими гранями).
    const vw::Box2 ext = vertexExtent(m, f);
    EXPECT_NEAR(av->sheetView.view.bounds.width(), ext.width(), 0.05);
    EXPECT_NEAR(av->sheetView.view.bounds.height(), ext.height(), 0.05);
    // Стрелка — у главного вида со стороны наблюдателя (слева сверху), смотрит вправо-вниз.
    ASSERT_TRUE(av->arrow);
    const vw::ViewArrowGeometry& ar = *av->arrow;
    EXPECT_NEAR(ar.direction.x, k, 1e-12);
    EXPECT_NEAR(ar.direction.y, -k, 1e-12);
    const vw::Box2 fb = fv->sheetView.sheetBounds();
    EXPECT_TRUE(ar.tip.x < fb.minX || ar.tip.y > fb.maxY) << "the arrow tip is outside the main view";
    EXPECT_LT(ar.tip.x, 0.5 * (fb.minX + fb.maxX));
    EXPECT_GT(ar.tip.y, 0.5 * (fb.minY + fb.maxY));
    EXPECT_NEAR(std::hypot(ar.shaft.a.x - ar.tip.x, ar.shaft.a.y - ar.tip.y), 12.0, 1e-9);
    // Остриё — на 3 мм за габаритом вдоль линии через центр вида.
    const Point2 fc = center(*fv);
    const double t = std::min(0.5 * fb.width() / k, 0.5 * fb.height() / k);
    EXPECT_NEAR(std::hypot(ar.tip.x - fc.x, ar.tip.y - fc.y), t + 3.0, 1e-9);

    auto drawings = d.render(nullptr);
    ASSERT_TRUE(drawings) << drawings.error().message;
    std::size_t letters = 0, arcs = 0;
    for (const Entity* e : entitiesOn(drawings.value()[0], "SYMBOLS")) {
        if (const auto* tx = std::get_if<Text>(&e->geometry); tx && tx->utf8 == "А") ++letters;
        if (std::holds_alternative<Arc>(e->geometry)) ++arcs;
    }
    EXPECT_EQ(letters, 2u);  // у стрелки и над видом
    EXPECT_EQ(arcs, 0u);     // не повернут — знака нет

    // Повернутый вид: «вверх» главного вида — вверх листа; поворот −45° (по часовой стрелке),
    // над видом — «А» со знаком «повернуто».
    d.sheets()[0].views.back().rotated = true;
    auto r2 = d.update(m, cache);
    ASSERT_TRUE(r2) << r2.error().message;
    av = computedView(d, "A");
    EXPECT_NEAR(av->rotationDeg, -45.0, 1e-9);
    EXPECT_NEAR(av->sheetView.view.frame.up().z, k, 1e-12);
    EXPECT_NEAR(av->sheetView.view.frame.project({0, 0, 1}).x, 0.0, 1e-12);
    auto d2 = d.render(nullptr);
    ASSERT_TRUE(d2) << d2.error().message;
    const std::string path = dv2test::outPath("auxiliary_rotated.dxf");
    saveToFile(path, writeDxf(d2.value()[0]).value()).value();
    const DxfContent dxf = readDxf(path);
    std::size_t aTexts = 0;
    for (const std::string& s : dxf.texts) aTexts += s == "А" ? 1 : 0;
    EXPECT_EQ(aTexts, 2u);
    arcs = 0;
    for (const Entity* e : entitiesOn(d2.value()[0], "SYMBOLS"))
        if (const auto* arc = std::get_if<Arc>(&e->geometry)) {
            ++arcs;
            EXPECT_NEAR(arc->radius, 3.5, 1e-12);  // диаметр знака — высота букв 7 мм
            // Знак — справа от надписи над видом.
            const vw::Box2 ab = av->sheetView.sheetBounds();
            EXPECT_GT(arc->center.x, 0.5 * (ab.minX + ab.maxX));
            EXPECT_GT(arc->center.y, ab.maxY);
        }
    EXPECT_EQ(arcs, 1u);

    // Файл: вид, поворот; направление, перпендикулярное главному виду, — ошибка (стрелку не поставить).
    auto back = DrawingDocument::fromJson(d.toJson().value());
    ASSERT_TRUE(back) << back.error().message;
    EXPECT_EQ(back.value().sheets()[0].views.back().kind, ViewKind::Auxiliary);
    EXPECT_TRUE(back.value().sheets()[0].views.back().rotated);
    d.sheets()[0].views.back().toward = {0, -1, 0};
    auto bad = d.update(m, cache);
    ASSERT_FALSE(bad);
    EXPECT_NE(bad.error().message.find("perpendicular"), std::string::npos);
}
