// Рамка ISO 5457 и основная надпись ISO 7200 (180 мм) со знаком метода проекций ISO 5456-2:
// размеры полей против раскладки docs/drawing/DRAWING_MODEL.md, тексты полей, знак первого и
// третьего угла, экспорт DXF (разбор групповых кодов) и PDF (структура, координаты в пунктах).

#include "test_support.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <map>
#include <set>

using namespace dv2test;

namespace {

void expectRect(const Rect& r, double x0, double y0, double w, double h) {
    EXPECT_NEAR(r.x0, x0, 1e-9);
    EXPECT_NEAR(r.y0, y0, 1e-9);
    EXPECT_NEAR(r.width(), w, 1e-9);
    EXPECT_NEAR(r.height(), h, 1e-9);
}

bool hasLine(const Drawing& d, Point2 a, Point2 b, LineType type) {
    auto same = [](Point2 p, Point2 q) { return std::fabs(p.x - q.x) < 1e-9 && std::fabs(p.y - q.y) < 1e-9; };
    for (const auto& e : d.entities()) {
        const auto* l = std::get_if<LineSeg>(&e.geometry);
        if (!l || e.lineType != type) continue;
        if ((same(l->a, a) && same(l->b, b)) || (same(l->a, b) && same(l->b, a))) return true;
    }
    return false;
}

Iso7200Data sampleIso(const std::string& language = "ru") {
    Iso7200Data d;
    d.legalOwner = "ООО «Бенд»";
    d.identification = "АБВГ.123456.001";
    d.title = "Кронштейн";
    d.documentType = "Чертёж детали";
    d.documentStatus = "Выпущен";
    d.revision = "A";
    d.issueDate = "2026-11-21";
    d.sheet = 1;
    d.sheets = 2;
    d.language = language;
    d.creator = "Иванов";
    d.approver = "Петров";
    d.department = "КБ-3";
    d.scale = "1:2";
    return d;
}

struct IsoSample {
    Drawing drawing;
    TitleBlockLayout layout;
};

IsoSample isoSample(GostFormat f, Orientation o, const Iso7200Data& data) {
    Drawing d = Drawing::create(gostSheet(f, 1, o).value(), 0.5).value();
    d.setTextFont(TextFont::Gost2304);
    IsoFrameOptions io;
    io.font = &gostStrokeFont();
    auto lay = addIsoFrameAndTitleBlock(d, data, io);
    if (!lay) throw std::runtime_error(lay.error().message);
    return {std::move(d), lay.value()};
}

std::map<std::string, const Text*> titleTexts(const Drawing& d) {
    std::map<std::string, const Text*> out;
    for (const auto& e : d.entities())
        if (const auto* t = std::get_if<Text>(&e.geometry); t && e.layer == "TITLE") out[t->utf8] = t;
    return out;
}

}  // namespace

TEST(DrawingV2TitleBlockIso, A4PortraitFrameAndFieldsMatchIso7200Layout) {
    IsoSample s = isoSample(GostFormat::A4, Orientation::Portrait, sampleIso());
    const auto& L = s.layout;
    const Drawing& d = s.drawing;
    // ISO 5457: поля 20 мм слева и 10 мм остальные; ISO 7200: ширина 180 мм — во всю рамку А4.
    expectRect(L.sheet, 0, 0, 210, 297);
    expectRect(L.frame, 20, 10, 180, 277);
    expectRect(L.titleBlock, 20, 10, kIso7200Width, kIso7200Height);
    const double X = 20, Y = 10;
    using F = Iso7200Field;
    auto cell = [&](F f) { return L.graphs.at(int(f)); };
    expectRect(cell(F::Department), X, Y + 27, 30, 9);
    expectRect(cell(F::Creator), X + 30, Y + 27, 30, 9);
    expectRect(cell(F::Projection), X, Y + 18, 15, 9);
    expectRect(cell(F::Scale), X + 15, Y + 18, 15, 9);
    expectRect(cell(F::Approver), X + 30, Y + 18, 30, 9);
    expectRect(cell(F::LegalOwner), X, Y, 60, 18);
    expectRect(cell(F::DocumentType), X + 60, Y + 27, 60, 9);
    expectRect(cell(F::Title), X + 60, Y, 60, 27);
    expectRect(cell(F::DocumentStatus), X + 120, Y + 27, 60, 9);
    expectRect(cell(F::Identification), X + 120, Y + 9, 60, 18);
    expectRect(cell(F::Revision), X + 120, Y, 10, 9);
    expectRect(cell(F::IssueDate), X + 130, Y, 26, 9);
    expectRect(cell(F::Language), X + 156, Y, 10, 9);
    expectRect(cell(F::Sheet), X + 166, Y, 14, 9);
    // Поля без перекрытий покрывают всю надпись: сумма площадей = 180 × 36.
    double area = 0;
    for (const auto& [k, r] : L.graphs) area += r.width() * r.height();
    EXPECT_NEAR(area, kIso7200Width * kIso7200Height, 1e-6);
    // Контур — сплошная толстая; рамка — толстая; линии полей — тонкие.
    EXPECT_TRUE(hasLine(d, {X, Y}, {X, Y + 36}, LineType::SolidThick));
    EXPECT_TRUE(hasLine(d, {X, Y + 36}, {X + 180, Y + 36}, LineType::SolidThick));
    EXPECT_TRUE(hasLine(d, {20, 10}, {200, 10}, LineType::SolidThick));
    EXPECT_TRUE(hasLine(d, {X, Y + 27}, {X + 180, Y + 27}, LineType::SolidThin));
    EXPECT_TRUE(hasLine(d, {X + 60, Y}, {X + 60, Y + 36}, LineType::SolidThin));
    EXPECT_TRUE(hasLine(d, {X + 166, Y}, {X + 166, Y + 9}, LineType::SolidThin));
    // Метки центрирования ISO 5457 — от края листа до 5 мм за рамку.
    EXPECT_TRUE(hasLine(d, {105, 0}, {105, 15}, LineType::SolidThick));
    EXPECT_TRUE(hasLine(d, {105, 297}, {105, 282}, LineType::SolidThick));
    EXPECT_TRUE(hasLine(d, {0, 148.5}, {25, 148.5}, LineType::SolidThick));
    EXPECT_TRUE(hasLine(d, {210, 148.5}, {195, 148.5}, LineType::SolidThick));
}

TEST(DrawingV2TitleBlockIso, FieldTextsInTheirCellsWithoutCompression) {
    for (const std::string lang : {"ru", "en"}) {
        IsoSample s = isoSample(GostFormat::A3, Orientation::Landscape, sampleIso(lang));
        const auto texts = titleTexts(s.drawing);
        const auto& L = s.layout;
        using F = Iso7200Field;
        // Значение каждого поля — внутри своей ячейки ниже полосы наименования (3,5 мм).
        const std::map<F, std::string> values = {
            {F::LegalOwner, "ООО «Бенд»"}, {F::Identification, "АБВГ.123456.001"}, {F::Title, "Кронштейн"},
            {F::DocumentType, "Чертёж детали"}, {F::DocumentStatus, "Выпущен"}, {F::Revision, "A"},
            {F::IssueDate, "2026-11-21"}, {F::Sheet, "1/2"}, {F::Language, lang}, {F::Creator, "Иванов"},
            {F::Approver, "Петров"}, {F::Department, "КБ-3"}, {F::Scale, "1:2"}};
        for (const auto& [f, v] : values) {
            ASSERT_TRUE(texts.count(v)) << lang << ": " << v;
            const Text* t = texts.at(v);
            const Rect& c = L.graphs.at(int(f));
            EXPECT_GT(t->position.x, c.x0) << v;
            EXPECT_LT(t->position.x, c.x1) << v;
            EXPECT_GT(t->position.y - t->height / 2.0, c.y0 - 1e-9) << v;
            EXPECT_LT(t->position.y + t->height / 2.0, c.y1 - 3.5 + 1e-9) << v;
            EXPECT_DOUBLE_EQ(t->widthFactor, 1.0) << v;
            // Линии знаков текста не выходят из ячейки (шрифт ГОСТ 2.304 с наклоном 75°).
            const auto lay = gostStrokeFont().layout(*t).value();
            for (const auto& st : lay.strokes)
                for (const Point2& p : st) {
                    EXPECT_GE(p.x, c.x0 - 1e-6) << v;
                    EXPECT_LE(p.x, c.x1 + 1e-6) << v;
                    EXPECT_GE(p.y, c.y0 - 1e-6) << v;
                    EXPECT_LE(p.y, c.y1 + 1e-6) << v;
                }
        }
        // Наименование — h = 7; обозначение при 7 не помещается (96d = 67,2 мм > 59 мм) — 5;
        // прочие значения — 3,5.
        EXPECT_DOUBLE_EQ(texts.at("АБВГ.123456.001")->height, 5.0);
        EXPECT_DOUBLE_EQ(texts.at("Кронштейн")->height, 7.0);
        EXPECT_DOUBLE_EQ(texts.at("Иванов")->height, 3.5);
        // Наименования полей — h = 2,5 у верхнего края ячейки, по языку документа.
        for (F f : {F::Department, F::Creator, F::Approver, F::Scale, F::LegalOwner, F::DocumentType, F::Title,
                    F::DocumentStatus, F::Identification, F::Revision, F::IssueDate, F::Language, F::Sheet}) {
            const std::string label = iso7200FieldLabel(f, lang);
            ASSERT_TRUE(texts.count(label)) << lang << ": " << label;
            const Text* t = texts.at(label);
            EXPECT_DOUBLE_EQ(t->height, 2.5);
            EXPECT_NEAR(t->position.y, L.graphs.at(int(f)).y1 - 1.75, 1e-9) << label;
            EXPECT_EQ(t->hAlign, HAlign::Left);
        }
        EXPECT_EQ(iso7200FieldLabel(F::Identification, lang), lang == "ru" ? "Обозначение" : "Identification number");
    }
    // Номер листа вне 1…листов и длинный текст, который не помещается и при 2,5, — ошибки.
    Drawing d = Drawing::create(gostSheet(GostFormat::A3, 1, Orientation::Landscape).value(), 0.5).value();
    Iso7200Data bad = sampleIso();
    bad.sheet = 3;
    EXPECT_FALSE(addIsoFrameAndTitleBlock(d, bad));
    bad = sampleIso();
    bad.creator = "Константинопольский-Задунайский";
    IsoFrameOptions io;
    io.font = &gostStrokeFont();
    auto r = addIsoFrameAndTitleBlock(d, bad, io);
    ASSERT_FALSE(r);
    EXPECT_NE(r.error().message.find("Разработал"), std::string::npos);
    // Лист меньше надписи (A5: 148 мм) — ошибка.
    Drawing a5 = Drawing::create(iso216Sheet(5, Orientation::Portrait).value(), 0.5).value();
    EXPECT_FALSE(addIsoFrameAndTitleBlock(a5, sampleIso()));
}

TEST(DrawingV2TitleBlockIso, ProjectionSymbolFirstAndThirdAngle) {
    const double h = 2.5;
    auto first = projectionSymbolGeometry(ProjectionSymbol::FirstAngle, {0, 0}, h).value();
    auto third = projectionSymbolGeometry(ProjectionSymbol::ThirdAngle, {0, 0}, h).value();
    // ISO 5456-2: окружности диаметром h и 2h, трапеция с торцами h и 2h длиной 2h, габарит 5h × 2h.
    for (const auto* g : {&first, &third}) {
        EXPECT_NEAR(g->innerCircle.radius, h / 2.0, 1e-12);
        EXPECT_NEAR(g->outerCircle.radius, h, 1e-12);
        EXPECT_NEAR(g->bounds.width(), 5.0 * h, 1e-12);
        EXPECT_NEAR(g->bounds.height(), 2.0 * h, 1e-12);
        ASSERT_EQ(g->outline.size(), 4u);
        double minX = 1e9, maxX = -1e9;
        for (const LineSeg& l : g->outline)
            for (Point2 p : {l.a, l.b}) minX = std::min(minX, p.x), maxX = std::max(maxX, p.x);
        EXPECT_NEAR(maxX - minX, 2.0 * h, 1e-12);
        // Узкий торец (h) — слева, широкий (2h) — справа: длины вертикальных сторон.
        double leftEnd = 0, rightEnd = 0;
        for (const LineSeg& l : g->outline) {
            if (std::fabs(l.a.x - l.b.x) > 1e-12) continue;
            (std::fabs(l.a.x - minX) < 1e-12 ? leftEnd : rightEnd) = std::fabs(l.a.y - l.b.y);
        }
        EXPECT_NEAR(leftEnd, h, 1e-12);
        EXPECT_NEAR(rightEnd, 2.0 * h, 1e-12);
    }
    // Первый угол (метод E): вид с торца — справа от трапеции; третий угол (метод A) — слева.
    EXPECT_GT(first.innerCircle.center.x, first.outline[1].a.x);
    EXPECT_LT(third.innerCircle.center.x, third.outline[0].a.x);
    EXPECT_NEAR(first.innerCircle.center.x, 1.5 * h, 1e-12);
    EXPECT_NEAR(third.innerCircle.center.x, -1.5 * h, 1e-12);
    EXPECT_FALSE(projectionSymbolGeometry(ProjectionSymbol::None, {0, 0}, h));
    EXPECT_FALSE(projectionSymbolGeometry(ProjectionSymbol::FirstAngle, {0, 0}, 0.0));

    // В основной надписи — знак в ячейке рядом с масштабом; без метода — знака нет.
    Iso7200Data data = sampleIso();
    data.projection = ProjectionSymbol::ThirdAngle;
    IsoSample s = isoSample(GostFormat::A3, Orientation::Landscape, data);
    const Rect& pc = s.layout.graphs.at(int(Iso7200Field::Projection));
    const Rect& sb = s.layout.projectionSymbol;
    EXPECT_GE(sb.x0, pc.x0);
    EXPECT_LE(sb.x1, pc.x1);
    EXPECT_GE(sb.y0, pc.y0);
    EXPECT_LE(sb.y1, pc.y1);
    std::vector<Circle> circles;
    for (const auto& e : s.drawing.entities())
        if (const auto* c = std::get_if<Circle>(&e.geometry)) circles.push_back(*c);
    ASSERT_EQ(circles.size(), 2u);
    const double cx = (pc.x0 + pc.x1) / 2.0 - 1.5 * h;
    for (const Circle& c : circles) EXPECT_NEAR(c.center.x, cx, 1e-9);
    data.projection = ProjectionSymbol::None;
    IsoSample none = isoSample(GostFormat::A3, Orientation::Landscape, data);
    for (const auto& e : none.drawing.entities()) EXPECT_FALSE(std::holds_alternative<Circle>(e.geometry));
    EXPECT_EQ(none.layout.projectionSymbol.width(), 0.0);
}

TEST(DrawingV2TitleBlockIso, DxfAndPdfExportCarryFieldsAndScale) {
    Iso7200Data data = sampleIso("en");
    data.projection = ProjectionSymbol::FirstAngle;
    IsoSample s = isoSample(GostFormat::A4, Orientation::Portrait, data);
    // DXF: невидимые тексты полей, окружности знака и линии рамки в мм листа.
    const std::string dxf = writeDxf(s.drawing).value();
    saveToFile(outPath("iso7200_a4.dxf"), dxf).value();
    DxfFile f = parseDxf(dxf);
    std::set<std::string> texts;
    std::vector<std::pair<Point2, double>> circles;
    bool frameLine = false, titleTop = false;
    for (const DxfObject& o : *f.section("ENTITIES")) {
        if (o.type == "TEXT") texts.insert(decodeDxfUnicode(*o.first(1)));
        if (o.type == "CIRCLE") circles.push_back({{o.real(10), o.real(20)}, o.real(40)});
        if (o.type == "LINE") {
            const double x0 = o.real(10), y0 = o.real(20), x1 = o.real(11), y1 = o.real(21);
            auto is = [&](double a, double b, double c, double e) {
                return (std::fabs(x0 - a) < 1e-6 && std::fabs(y0 - b) < 1e-6 && std::fabs(x1 - c) < 1e-6 && std::fabs(y1 - e) < 1e-6) ||
                       (std::fabs(x0 - c) < 1e-6 && std::fabs(y0 - e) < 1e-6 && std::fabs(x1 - a) < 1e-6 && std::fabs(y1 - b) < 1e-6);
            };
            frameLine = frameLine || is(20, 10, 200, 10);
            titleTop = titleTop || is(20, 46, 200, 46);
        }
    }
    EXPECT_TRUE(frameLine);
    EXPECT_TRUE(titleTop);
    for (const char* want : {"Legal owner", "Identification number", "Date of issue", "АБВГ.123456.001", "2026-11-21", "1/2", "Кронштейн"})
        EXPECT_TRUE(texts.count(want)) << want;
    ASSERT_EQ(circles.size(), 2u);
    // Знак первого угла: центр окружностей — на 1,5h правее центра ячейки (20 + 7,5; 10 + 22,5).
    for (const auto& [c, r] : circles) {
        EXPECT_NEAR(c.x, 27.5 + 3.75, 1e-6);
        EXPECT_NEAR(c.y, 32.5, 1e-6);
    }
    EXPECT_NEAR(std::min(circles[0].second, circles[1].second), 1.25, 1e-6);
    EXPECT_NEAR(std::max(circles[0].second, circles[1].second), 2.5, 1e-6);

    // PDF: страница A4 в пунктах, нижняя линия рамки ISO 5457 (y = 10 мм) в пунктах.
    const std::string pdf = writePdf(s.drawing).value();
    saveToFile(outPath("iso7200_a4.pdf"), pdf).value();
    PdfFile pf = parsePdf(pdf);
    EXPECT_NE(pf.find("/MediaBox [0 0 595.2756 841.8898]"), nullptr);
    const double k = 72.0 / 25.4;
    auto fmt = [](double v) {
        char b[32];
        std::snprintf(b, sizeof b, "%.4f", v);
        std::string str = b;
        while (str.back() == '0') str.pop_back();
        if (str.back() == '.') str.pop_back();
        return str;
    };
    bool found = false;
    for (const auto& o : pf.objects) found = found || o.stream.find(fmt(20 * k) + " " + fmt(10 * k) + " m") != std::string::npos;
    EXPECT_TRUE(found) << "frame corner (20, 10) mm in points";
    const TrueTypeFont* font = systemFont();
    if (!font) GTEST_SKIP() << "no system TrueType font for the searchable layer";
    PdfOptions po;
    po.font = font;
    const std::string searchable = writePdf(s.drawing, po).value();
    saveToFile(outPath("iso7200_a4_text.pdf"), searchable).value();
#ifdef BENDCORE_PDFTOTEXT
    int code = 0;
    const std::string out =
        runCommand(std::string(BENDCORE_PDFTOTEXT) + " -enc UTF-8 " + outPath("iso7200_a4_text.pdf") + " - 2>&1", &code);
    EXPECT_EQ(code, 0);
    for (const char* want : {"Legal owner", "АБВГ.123456.001", "Кронштейн", "Approved by", "Петров"})
        EXPECT_NE(out.find(want), std::string::npos) << "pdftotext: " << want << "\n" << out;
#endif
}
