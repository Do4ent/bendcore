// Кратные форматы ГОСТ 2.301 (табл. 2) и удлинённые форматы ISO 5457 (табл. 2 и 3): размеры против
// таблиц стандартов, рамка и основная надпись на каждом кратном формате; разбивка поля на зоны
// (ГОСТ 2.104, ISO 5457, п. 3.5): границы против ручного расчёта, обозначения, экспорт DXF.

#include "test_support.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <set>

using namespace dv2test;

namespace {

// ГОСТ 2.301-68, табл. 2, — переписана из стандарта (короткая × длинная сторона, мм). Те же
// размеры — удлинённые форматы ISO 5457:1999, табл. 2 (A3×3, A3×4, A4×3…A4×5) и табл. 3.
struct Row {
    GostFormat base;
    int n;
    double a, b;
};
const Row kTable2[] = {
    {GostFormat::A0, 2, 1189, 1682}, {GostFormat::A0, 3, 1189, 2523}, {GostFormat::A1, 3, 841, 1783},
    {GostFormat::A1, 4, 841, 2378},  {GostFormat::A2, 3, 594, 1261},  {GostFormat::A2, 4, 594, 1682},
    {GostFormat::A2, 5, 594, 2102},  {GostFormat::A3, 3, 420, 891},   {GostFormat::A3, 4, 420, 1189},
    {GostFormat::A3, 5, 420, 1486},  {GostFormat::A3, 6, 420, 1783},  {GostFormat::A3, 7, 420, 2080},
    {GostFormat::A4, 3, 297, 630},   {GostFormat::A4, 4, 297, 841},   {GostFormat::A4, 5, 297, 1051},
    {GostFormat::A4, 6, 297, 1261},  {GostFormat::A4, 7, 297, 1471},  {GostFormat::A4, 8, 297, 1682},
    {GostFormat::A4, 9, 297, 1892},
};

void expectRect(const Rect& r, double x0, double y0, double x1, double y1) {
    EXPECT_NEAR(r.x0, x0, 1e-9);
    EXPECT_NEAR(r.y0, y0, 1e-9);
    EXPECT_NEAR(r.x1, x1, 1e-9);
    EXPECT_NEAR(r.y1, y1, 1e-9);
}

Drawing sheetDrawing(const Sheet& s) {
    auto d = Drawing::create(s, 0.5);
    EXPECT_TRUE(d) << (d ? "" : d.error().message);
    return std::move(d).value();
}

}  // namespace

TEST(DrawingV2Multiples, EveryMultipleFormatMatchesTheTables) {
    std::set<std::pair<int, int>> seen;
    for (const Row& r : kTable2) {
        for (Orientation o : {Orientation::Portrait, Orientation::Landscape}) {
            auto g = gostSheet(r.base, r.n, o);
            ASSERT_TRUE(g) << g.error().message;
            const double w = o == Orientation::Portrait ? r.a : r.b, h = o == Orientation::Portrait ? r.b : r.a;
            EXPECT_DOUBLE_EQ(g.value().widthMm, w) << g.value().name;
            EXPECT_DOUBLE_EQ(g.value().heightMm, h) << g.value().name;
            EXPECT_EQ(g.value().multiplier, r.n);
            auto i = iso5457Sheet(int(r.base), r.n, o);
            ASSERT_TRUE(i) << i.error().message;
            EXPECT_DOUBLE_EQ(i.value().widthMm, w);
            EXPECT_DOUBLE_EQ(i.value().heightMm, h);
            EXPECT_EQ(i.value().standard, SheetStandard::Iso216);
            EXPECT_EQ(i.value().multiplier, r.n);
            EXPECT_EQ(i.value().name, "ISO A" + std::to_string(int(r.base)) + "x" + std::to_string(r.n));
        }
        seen.insert({int(r.base), r.n});
    }
    // Список кратностей для интерфейса — ровно табл. 2 плюс основной формат.
    for (GostFormat b : {GostFormat::A0, GostFormat::A1, GostFormat::A2, GostFormat::A3, GostFormat::A4}) {
        const std::vector<int> m = sheetMultipliers(b);
        ASSERT_FALSE(m.empty());
        EXPECT_EQ(m.front(), 1);
        for (std::size_t k = 1; k < m.size(); ++k) EXPECT_TRUE(seen.count({int(b), m[k]})) << int(b) << "x" << m[k];
        std::size_t inTable = 0;
        for (const auto& [base, n] : seen) inTable += base == int(b) ? 1u : 0u;
        EXPECT_EQ(m.size(), inTable + 1);
    }
    EXPECT_EQ(sheetMultipliers(GostFormat::A4), (std::vector<int>{1, 3, 4, 5, 6, 7, 8, 9}));
    EXPECT_EQ(sheetMultipliers(GostFormat::A0), (std::vector<int>{1, 2, 3}));
    // Основные форматы ISO 5457 = ISO 216; кратности, которых нет в таблицах, — ошибка.
    EXPECT_DOUBLE_EQ(iso5457Sheet(3).value().widthMm, 297);
    EXPECT_EQ(iso5457Sheet(3).value().name, "ISO A3");
    EXPECT_FALSE(iso5457Sheet(4, 2));
    EXPECT_FALSE(iso5457Sheet(0, 4));
    EXPECT_FALSE(iso5457Sheet(5, 3));
}

TEST(DrawingV2Multiples, FrameAndTitleBlockOnEveryMultipleFormat) {
    // ЕСКД (ГОСТ 2.104): рамка с полями 20/5, основная надпись 185×55 в правом нижнем углу рамки,
    // графы 19–23 вдоль левой линии рамки. ISO 5457: поля 20/10, надпись ISO 7200 180×36, метки
    // центрирования на осях листа.
    for (const Row& r : kTable2) {
        for (Orientation o : {Orientation::Portrait, Orientation::Landscape}) {
            const Sheet s = gostSheet(r.base, r.n, o).value();
            Drawing d = sheetDrawing(s);
            TitleBlockData td;
            td.designation = "АБВГ.000000.001";
            td.name = "Корпус";
            auto lay = addFrameAndTitleBlock(d, td);
            ASSERT_TRUE(lay) << s.name << ": " << lay.error().message;
            expectRect(lay.value().sheet, 0, 0, s.widthMm, s.heightMm);
            expectRect(lay.value().frame, 20, 5, s.widthMm - 5, s.heightMm - 5);
            expectRect(lay.value().titleBlock, s.widthMm - 5 - 185, 5, s.widthMm - 5, 60);
            // Графы основной надписи (1–18) — внутри рамки; графы 19–23 — в поле подшивки.
            for (const auto& [n, cell] : lay.value().graphs) {
                if (n >= 19) {
                    EXPECT_NEAR(cell.x1, lay.value().frame.x0, 1e-9) << s.name << " graph " << n;
                    continue;
                }
                EXPECT_GE(cell.x0, lay.value().frame.x0 - 1e-9) << s.name << " graph " << n;
                EXPECT_LE(cell.x1, lay.value().frame.x1 + 1e-9) << s.name << " graph " << n;
                EXPECT_GE(cell.y0, lay.value().frame.y0 - 1e-9) << s.name << " graph " << n;
            }

            const Sheet is = iso5457Sheet(int(r.base), r.n, o).value();
            Drawing di = sheetDrawing(is);
            Iso7200Data id;
            id.title = "Housing";
            id.identification = "ABC-001";
            auto li = addIsoFrameAndTitleBlock(di, id);
            ASSERT_TRUE(li) << is.name << ": " << li.error().message;
            expectRect(li.value().frame, 20, 10, is.widthMm - 10, is.heightMm - 10);
            expectRect(li.value().titleBlock, is.widthMm - 10 - 180, 10, is.widthMm - 10, 46);
            // Метки центрирования: от края листа до 5 мм за рамку на осях листа.
            int marks = 0;
            for (const Entity& e : di.entities()) {
                const auto* l = std::get_if<LineSeg>(&e.geometry);
                if (!l || e.lineType != LineType::SolidThick) continue;
                if (std::abs(l->a.x - is.widthMm / 2) < 1e-9 && std::abs(l->b.x - is.widthMm / 2) < 1e-9 &&
                    (std::abs(l->a.y) < 1e-9 || std::abs(l->a.y - is.heightMm) < 1e-9))
                    ++marks;
                if (std::abs(l->a.y - is.heightMm / 2) < 1e-9 && std::abs(l->b.y - is.heightMm / 2) < 1e-9 &&
                    (std::abs(l->a.x) < 1e-9 || std::abs(l->a.x - is.widthMm) < 1e-9))
                    ++marks;
            }
            EXPECT_EQ(marks, 4) << is.name;
        }
    }
    // А4 альбомной без кратности — ошибка ГОСТ 2.104, кратный А4×3 альбомный — допустим.
    Drawing a4 = sheetDrawing(gostSheet(GostFormat::A4, 1, Orientation::Landscape).value());
    EXPECT_FALSE(addFrameAndTitleBlock(a4, {}));
    Drawing a43 = sheetDrawing(gostSheet(GostFormat::A4, 3, Orientation::Landscape).value());
    EXPECT_TRUE(addFrameAndTitleBlock(a43, {}));
}

TEST(DrawingV2Zones, GridBoundsMatchHandCalculation) {
    // ЕСКД, А3 альбомная: рамка 20…415 × 5…292, оси рамки x = 217,5, y = 148,5; поля по 50 мм:
    // round(217,5/50) = 4 и round(202,5/50) = 4 колонки по сторонам, round(148,5/50) = 3 строки.
    const Rect sheet{0, 0, 420, 297}, frame{20, 5, 415, 292};
    auto g = zoneGrid(sheet, frame, {217.5, 148.5}, ZoneLetters::Cyrillic);
    ASSERT_TRUE(g) << g.error().message;
    const std::vector<double> cols{20, 67.5, 117.5, 167.5, 217.5, 267.5, 317.5, 367.5, 415};
    const std::vector<double> rows{292, 248.5, 198.5, 148.5, 98.5, 48.5, 5};
    ASSERT_EQ(g.value().columns.size(), cols.size());
    for (std::size_t i = 0; i < cols.size(); ++i) EXPECT_NEAR(g.value().columns[i], cols[i], 1e-9) << i;
    ASSERT_EQ(g.value().rows.size(), rows.size());
    for (std::size_t i = 0; i < rows.size(); ++i) EXPECT_NEAR(g.value().rows[i], rows[i], 1e-9) << i;
    EXPECT_EQ(g.value().rowLabels, (std::vector<std::string>{"А", "Б", "В", "Г", "Д", "Е"}));
    EXPECT_EQ(g.value().columnLabels.front(), "1");
    EXPECT_EQ(g.value().columnLabels.back(), "8");
    // Зона точки: строки сверху, колонки слева.
    EXPECT_EQ(zoneOf(g.value(), {30, 280}), "А1");
    EXPECT_EQ(zoneOf(g.value(), {400, 10}), "Е8");
    EXPECT_EQ(zoneOf(g.value(), {200, 150}), "В4");  // y = 150 выше оси 148,5 — третья строка сверху
    EXPECT_EQ(zoneOf(g.value(), {220, 148}), "Г5");
    EXPECT_EQ(zoneOf(g.value(), {-5, 400}), "А1");    // вне рамки — ближайшая зона

    // ISO 5457: число полей по табл. 4 стандарта (A0 24 × 16 … A4 6 × 4) — от осей листа.
    const struct {
        int n;
        int cols, rows;  // альбомная ориентация: по длинной и короткой стороне
    } iso[] = {{0, 24, 16}, {1, 16, 12}, {2, 12, 8}, {3, 8, 6}, {4, 6, 4}};
    for (const auto& r : iso) {
        const Sheet s = iso216Sheet(r.n, Orientation::Landscape).value();
        const Rect sr{0, 0, s.widthMm, s.heightMm}, fr{20, 10, s.widthMm - 10, s.heightMm - 10};
        auto gi = zoneGrid(sr, fr, {s.widthMm / 2, s.heightMm / 2}, ZoneLetters::Latin);
        ASSERT_TRUE(gi) << gi.error().message;
        EXPECT_EQ(int(gi.value().columnLabels.size()), r.cols) << s.name;
        EXPECT_EQ(int(gi.value().rowLabels.size()), r.rows) << s.name;
        // Внутренние границы — кратны 50 мм от осей листа.
        for (std::size_t i = 1; i + 1 < gi.value().columns.size(); ++i) {
            const double k = (gi.value().columns[i] - s.widthMm / 2) / 50.0;
            EXPECT_NEAR(k, std::round(k), 1e-9) << s.name;
        }
    }
    // Латинские буквы без I и O; больше 24 строк — удвоенные буквы.
    const Sheet tall = gostSheet(GostFormat::A0, 3, Orientation::Portrait).value();  // 1189 × 2523
    auto gt = zoneGrid({0, 0, tall.widthMm, tall.heightMm}, {20, 10, tall.widthMm - 10, tall.heightMm - 10},
                       {tall.widthMm / 2, tall.heightMm / 2}, ZoneLetters::Latin);
    ASSERT_TRUE(gt);
    const auto& rl = gt.value().rowLabels;
    ASSERT_GT(rl.size(), 24u);
    EXPECT_EQ(rl[7], "H");
    EXPECT_EQ(rl[8], "J");   // I пропущена
    EXPECT_EQ(rl[13], "P");  // O пропущена
    EXPECT_EQ(rl[24], "AA");
    for (const std::string& l : rl) EXPECT_TRUE(l.find('I') == std::string::npos && l.find('O') == std::string::npos) << l;
    // Ошибки.
    EXPECT_FALSE(zoneGrid(sheet, frame, {217.5, 148.5}, ZoneLetters::Latin, 20.0));
    EXPECT_FALSE(zoneGrid(sheet, frame, {217.5, 148.5}, ZoneLetters::Latin, 80.0));
    EXPECT_FALSE(zoneGrid(sheet, frame, {10, 148.5}, ZoneLetters::Latin));
}

TEST(DrawingV2Zones, MarkingOnMarginsAndDxf) {
    const Sheet s = iso216Sheet(3, Orientation::Landscape).value();
    Drawing d = sheetDrawing(s);
    Iso7200Data id;
    id.title = "Housing";
    auto lay = addIsoFrameAndTitleBlock(d, id);
    ASSERT_TRUE(lay);
    const Point2 axes{s.widthMm / 2, s.heightMm / 2};
    auto g = zoneGrid(lay.value().sheet, lay.value().frame, axes, ZoneLetters::Latin);
    ASSERT_TRUE(g);
    auto m = zoneMarkingGeometry(g.value(), lay.value().sheet, 10.0, axes);
    ASSERT_TRUE(m) << m.error().message;
    // По два обозначения на колонку (сверху и снизу) и на строку (слева и справа).
    EXPECT_EQ(m.value().labels.size(), 2 * (8u + 6u));
    // Штрихи — на внутренних границах, кроме осей (там метки центрирования): (8 − 2) и (6 − 2) пар.
    EXPECT_EQ(m.value().ticks.size(), 2 * (6u + 4u));
    for (const LineSeg& t : m.value().ticks) {
        EXPECT_GT(std::abs(t.a.x - axes.x) + std::abs(t.b.x - axes.x), 1e-6);
        // Штрих — в полосе 10 мм у края листа.
        const bool horizontalBand = std::min(t.a.y, t.b.y) >= -1e-9 && std::max(t.a.y, t.b.y) <= 10 + 1e-9;
        const bool topBand = std::min(t.a.y, t.b.y) >= s.heightMm - 10 - 1e-9;
        const bool sideBand = std::max(t.a.x, t.b.x) <= 10 + 1e-9 || std::min(t.a.x, t.b.x) >= s.widthMm - 10 - 1e-9;
        EXPECT_TRUE(horizontalBand || topBand || sideBand);
    }
    for (const auto& l : m.value().labels) {
        const double cx = (l.cell.x0 + l.cell.x1) / 2, cy = (l.cell.y0 + l.cell.y1) / 2;
        const bool inBand = cy < 10 || cy > s.heightMm - 10 || cx < 10 || cx > s.widthMm - 10;
        EXPECT_TRUE(inBand) << l.text;
    }
    ASSERT_TRUE(addZoneMarking(d, m.value(), nullptr));
    // DXF: тексты обозначений в мм листа — «3» над третьей колонкой (110…160), «C» у третьей строки.
    auto dxf = writeDxf(d);
    ASSERT_TRUE(dxf) << dxf.error().message;
    const DxfFile f = parseDxf(dxf.value());
    const auto* ents = f.section("ENTITIES");
    ASSERT_NE(ents, nullptr);
    int found3 = 0, foundC = 0, ticks = 0;
    for (const DxfObject& o : *ents) {
        if (o.type == "TEXT" && o.first(1) == std::string("3")) {
            const double x = o.real(11), y = o.real(21);
            EXPECT_NEAR(x, 135.0, 1e-6);
            EXPECT_TRUE(std::abs(y - 5.0) < 1e-6 || std::abs(y - (s.heightMm - 5.0)) < 1e-6) << y;
            ++found3;
        }
        if (o.type == "TEXT" && o.first(1) == std::string("C")) {
            const double y = o.real(21);
            EXPECT_NEAR(y, (198.5 + 148.5) / 2.0, 1e-6);
            ++foundC;
        }
        if (o.type == "LINE" && o.first(6) == std::string("Continuous")) {
            const double x0 = o.real(10), y0 = o.real(20), x1 = o.real(11), y1 = o.real(21);
            if (std::abs(x0 - 110) < 1e-6 && std::abs(x1 - 110) < 1e-6 && std::abs(std::abs(y1 - y0) - 10) < 1e-6) ++ticks;
        }
    }
    EXPECT_EQ(found3, 2);
    EXPECT_EQ(foundC, 2);
    EXPECT_EQ(ticks, 2);
}
