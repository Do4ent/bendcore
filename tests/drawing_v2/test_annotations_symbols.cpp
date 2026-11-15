// Знаки: шероховатость ГОСТ 2.309 (геометрия знака: стороны под 60°, высоты h и H), рамка
// допуска ГОСТ 2.308 (высота 2h, клетки), база, выноски, технические требования (перенос,
// нумерация, положение над основной надписью), позиции.

#include <bendcore/drawing_v2/annotations.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <numbers>

using namespace bendcore::drawing_v2;
using namespace bendcore::drawing_v2::annotations;

namespace {

double angleOf(const LineSeg& l) { return std::atan2(l.b.y - l.a.y, l.b.x - l.a.x) * 180.0 / std::numbers::pi; }

}  // namespace

TEST(AnnotationsSymbols, RoughnessSignGeometry) {
    RoughnessSpec spec;
    spec.method = RoughnessMethod::MaterialRemoval;
    spec.value = "3,2";
    auto g = roughnessSign({100, 50}, 0.0, spec);
    ASSERT_TRUE(g.ok()) << g.error().message;
    const double h = 3.5, H = 2.5 * h;
    // Короткая сторона: от вершины под 120° к оси x (60° к поверхности) высотой h.
    const LineSeg s = g.value().lines[0], l = g.value().lines[1];
    EXPECT_NEAR(angleOf(s), 120.0, 1e-9);
    EXPECT_NEAR(s.b.y - 50, h, 1e-9);
    EXPECT_NEAR(angleOf(l), 60.0, 1e-9);
    EXPECT_NEAR(l.b.y - 50, H, 1e-9);
    // Черта удаления материала на высоте h, полка на высоте H, параметр «Ra 3,2» под полкой.
    EXPECT_NEAR(g.value().lines[2].a.y - 50, h, 1e-9);
    EXPECT_NEAR(g.value().lines[2].b.y - 50, h, 1e-9);
    EXPECT_NEAR(g.value().lines[3].a.y - 50, H, 1e-9);
    ASSERT_EQ(g.value().labels.size(), 1u);
    EXPECT_EQ(g.value().labels[0].text, "Ra 3,2");
    EXPECT_LT(g.value().labels[0].position.y + h, 50 + H);

    spec.method = RoughnessMethod::NoMaterialRemoval;
    g = roughnessSign({0, 0}, 90.0, spec);
    ASSERT_EQ(g.value().circles.size(), 1u);
    EXPECT_NEAR(g.value().circles[0].radius, h / 3, 1e-9);
    // Повёрнутый на 90° знак: центр вписанной окружности слева от поверхности (−x).
    EXPECT_NEAR(g.value().circles[0].center.x, -2 * h / 3, 1e-9);

    spec.heightFactor = 6;
    EXPECT_FALSE(roughnessSign({0, 0}, 0, spec).ok());
    RoughnessSpec bad;
    bad.parameter.clear();
    bad.value = "1,6";
    EXPECT_FALSE(roughnessSign({0, 0}, 0, bad).ok());
}

TEST(AnnotationsSymbols, RoughnessRemainderInTopRightCorner) {
    const Rect frame{20, 5, 415, 292};
    RoughnessSpec spec;
    spec.value = "6,3";
    auto g = roughnessRemainder(frame, spec);
    ASSERT_TRUE(g.ok()) << g.error().message;
    bool open = false, close = false;
    double maxX = 0, maxY = 0;
    for (const auto& l : g.value().lines) {
        maxX = std::max({maxX, l.a.x, l.b.x});
        maxY = std::max({maxY, l.a.y, l.b.y});
    }
    for (const auto& l : g.value().labels) {
        open |= l.text == "(";
        close |= l.text == ")";
        EXPECT_DOUBLE_EQ(l.height, 5.0);  // 1,5·3,5 → 5 по ряду ГОСТ 2.304
    }
    EXPECT_TRUE(open && close);
    EXPECT_LE(maxX, frame.x1);
    EXPECT_LE(maxY, frame.y1);
    EXPECT_GT(maxY, frame.y1 - 20);
}

TEST(AnnotationsSymbols, FeatureControlFrame) {
    FeatureControlFrame f;
    f.characteristic = GeometricCharacteristic::Perpendicularity;
    f.tolerance = "0,02";
    f.datums = {"А"};
    auto r = featureControlFrame({10, 10}, f);
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_NEAR(r.value().box.height(), 7.0, 1e-12);  // 2h
    // Клетки: знак (7), значение, база — ширина ≥ 3·7.
    EXPECT_GE(r.value().box.width(), 21.0);
    bool value = false, datum = false;
    for (const auto& l : r.value().geometry.labels) {
        value |= l.text == "0,02";
        datum |= l.text == "А";
    }
    EXPECT_TRUE(value && datum);
    // Знак ⊥: вертикальная линия по центру первой клетки.
    bool vertical = false;
    for (const auto& l : r.value().geometry.lines) {
        if (std::abs(l.a.x - 13.5) < 1e-9 && std::abs(l.b.x - 13.5) < 1e-9) vertical = true;
    }
    EXPECT_TRUE(vertical);

    f.characteristic = GeometricCharacteristic::Flatness;
    EXPECT_FALSE(featureControlFrame({0, 0}, f).ok());  // допуск формы с базой
    f.datums.clear();
    EXPECT_TRUE(featureControlFrame({0, 0}, f).ok());
    f.characteristic = GeometricCharacteristic::Parallelism;
    EXPECT_FALSE(featureControlFrame({0, 0}, f).ok());  // без базы
    f.characteristic = GeometricCharacteristic::Position;
    f.diameterZone = true;
    auto p = featureControlFrame({0, 0}, f);
    ASSERT_TRUE(p.ok());
    bool dia = false;
    for (const auto& l : p.value().geometry.labels) dia |= l.text == "⌀0,02";
    EXPECT_TRUE(dia);
    // Все 14 знаков строятся внутри своей клетки.
    for (int c = 0; c <= int(GeometricCharacteristic::TotalRunout); ++c) {
        const auto g = characteristicSymbol(GeometricCharacteristic(c), {0, 0}, 7.0);
        EXPECT_FALSE(g.lines.empty() && g.circles.empty() && g.arcs.empty() && g.polylines.empty()) << c;
        for (const auto& l : g.lines) {
            EXPECT_LE(std::max(std::abs(l.a.x), std::abs(l.b.x)), 3.5) << c;
            EXPECT_LE(std::max(std::abs(l.a.y), std::abs(l.b.y)), 3.5) << c;
        }
    }
}

TEST(AnnotationsSymbols, DatumLeaderAndBalloon) {
    auto d = datumSymbol({50, 20}, 0.0, "А", 5.0);
    ASSERT_TRUE(d.ok());
    ASSERT_EQ(d.value().fills.size(), 1u);
    const auto& tri = d.value().fills[0].points;
    EXPECT_NEAR(tri[2].y - 20, 3.5, 1e-9);                         // высота треугольника h
    EXPECT_NEAR(tri[1].x - tri[0].x, 2 * 3.5 / std::sqrt(3.0), 1e-9);  // равносторонний
    ASSERT_EQ(d.value().polylines.size(), 1u);
    EXPECT_NEAR(d.value().polylines[0].points[0].y, 20 + 3.5 + 5, 1e-9);

    auto n = leaderNote({0, 0}, {10, 10}, "2 отв.", LeaderEnd::Arrow);
    ASSERT_TRUE(n.ok());
    ASSERT_EQ(n.value().fills.size(), 1u);
    EXPECT_NEAR(n.value().fills[0].points[0].x, 0, 1e-12);  // остриё стрелки в точке
    EXPECT_NEAR(n.value().lines[1].a.y, 10, 1e-12);          // полка горизонтальна

    Balloon b{3, {10, 10}, {30, 30}, ""};
    EXPECT_FALSE(balloonGeometry(b).ok());  // номер не назначен
    b.text = "12";
    auto g = balloonGeometry(b);
    ASSERT_TRUE(g.ok());
    EXPECT_EQ(g.value().labels[0].text, "12");
    EXPECT_DOUBLE_EQ(g.value().labels[0].height, 5.0);
    BalloonOptions iso;
    iso.style = BalloonStyle::Circle;
    g = balloonGeometry(b, iso);
    ASSERT_TRUE(g.ok());
    ASSERT_EQ(g.value().circles.size(), 1u);
    // Линия-выноска начинается на окружности.
    const auto& l = g.value().lines[0];
    EXPECT_NEAR(std::hypot(l.a.x - 30, l.a.y - 30), g.value().circles[0].radius, 1e-9);
    std::vector<Balloon> row{b, b};
    row[1].at = {50, 41};
    alignBalloonsInRow(row, 45);
    EXPECT_DOUBLE_EQ(row[0].at.y, 45);
    EXPECT_DOUBLE_EQ(row[1].at.y, 45);
}

TEST(AnnotationsSymbols, TechnicalRequirementsAboveTitleBlock) {
    Drawing d = Drawing::create(gostSheet(GostFormat::A3, 1, Orientation::Landscape).value()).value();
    auto lay = addFrameAndTitleBlock(d, TitleBlockData{});
    ASSERT_TRUE(lay.ok());
    const std::vector<std::string> items = {
        "Неуказанные предельные отклонения размеров: отверстий H14, валов h14, остальных ±IT14/2.",
        "Острые кромки притупить.",
        "Покрытие Хим. Окс. прм. Длинный текст пункта, который должен перенестись на следующую строку, потому что "
        "колонка технических требований не шире основной надписи по ГОСТ 2.316."};
    auto r = addTechnicalRequirements(d, lay.value(), items);
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_GE(r.value().lines.size(), 4u);
    EXPECT_EQ(r.value().lines[0].rfind("1. ", 0), 0u);
    bool second = false;
    for (const auto& l : r.value().lines) second |= l.rfind("2. Острые", 0) == 0;
    EXPECT_TRUE(second);
    AnnotationStyle st;
    for (const auto& l : r.value().lines) EXPECT_LE(estimateTextWidth(l, 5.0, nullptr), 185.0 + 1e-9);
    EXPECT_NEAR(r.value().box.x0, lay.value().titleBlock.x0, 1e-9);
    EXPECT_NEAR(r.value().box.y0, lay.value().titleBlock.y1 + 10, 1e-9);
    // Строки сверху вниз: последняя — на нижней базовой линии блока.
    int texts = 0;
    for (const auto& e : d.entities()) {
        if (const auto* t = std::get_if<Text>(&e.geometry); t && e.layer == "DIM") {
            ++texts;
            EXPECT_GE(t->position.y, r.value().box.y0 - 1e-9);
        }
    }
    EXPECT_EQ(texts, int(r.value().lines.size()));
    EXPECT_FALSE(addTechnicalRequirements(d, lay.value(), {""}).ok());
    auto single = wrapText("Один пункт", 185, 5, st);
    ASSERT_TRUE(single.ok());
    EXPECT_EQ(single.value().size(), 1u);
}

namespace {

// Горизонтальные границы начертания выведенного текста — по линиям шрифта ГОСТ 2.304, как их
// выводят PDF/DXF/SVG (gostStrokeFont().layout), с толщиной линии d = h/10.
std::pair<double, double> drawnInk(const Text& t) {
    auto lay = gostStrokeFont().layout(t);
    EXPECT_TRUE(lay.ok());
    double lo = 1e300, hi = -1e300;
    for (const auto& s : lay.value().strokes)
        for (const auto& p : s) {
            lo = std::min(lo, p.x);
            hi = std::max(hi, p.x);
        }
    return {lo - lay.value().lineWidth / 2, hi + lay.value().lineWidth / 2};
}

}  // namespace

// Перенос по начертанию: ни одна строка не выходит за поле 185 мм над основной надписью и не
// касается рамки (у правой кромки основной надписи поле совпадает с рамкой). Пункт с листа
// корпуса редуктора по ширине знаков помещался до слова «на», а наклон 75° и толщина линии
// доводили «на» до линии рамки.
TEST(AnnotationsSymbols, TechnicalRequirementsInkStaysInsideTheField) {
    Drawing d = Drawing::create(gostSheet(GostFormat::A1, 1, Orientation::Landscape).value()).value();
    auto lay = addFrameAndTitleBlock(d, TitleBlockData{});
    ASSERT_TRUE(lay.ok());
    const double x0 = lay.value().titleBlock.x0;
    ASSERT_NEAR(lay.value().titleBlock.x1 - x0, 185.0, 1e-9);
    ASSERT_NEAR(lay.value().titleBlock.x1, lay.value().frame.x1, 1e-9);  // поле доходит до рамки
    const std::string item4 = "Резьбовые отверстия М6-6H глубиной 14 — 8 шт. (по 4 на торцах бобышек), сливное М16×1,5-6H.";
    // Воспроизведение: перенос по ширине знаков (без наклона и толщины линии) доводит строку до
    // линии рамки (толщина s = 0,5 мм по умолчанию листа, половина линии — внутри поля).
    auto old = wrapText("4. " + item4, 185.0, 5.0, AnnotationStyle{});
    ASSERT_TRUE(old.ok());
    Text oldFirst;
    oldFirst.position = {x0, 0};
    oldFirst.utf8 = old.value().front();
    oldFirst.height = 5.0;
    EXPECT_GT(drawnInk(oldFirst).second, lay.value().frame.x1 - 0.5 / 2) << oldFirst.utf8;

    const std::vector<std::string> items = {
        "Отливка по ГОСТ 26645-85, класс точности 9-0-0-9. Формовочные уклоны 1°.",
        "Неуказанные литейные радиусы 3...5 мм.",
        "Отверстия под подшипники обрабатывать совместно с крышкой корпуса не требуется (разъём выше осей).",
        item4,
        "Неуказанные предельные отклонения размеров: отверстий H14, валов h14, остальных ±IT14/2.",
        "Очень длинный пункт технических требований, который заведомо не помещается в одну строку поля шириной "
        "сто восемьдесят пять миллиметров и должен переноситься по словам несколько раз подряд без выхода за рамку."};
    TechnicalRequirementsOptions to;
    auto r = addTechnicalRequirements(d, lay.value(), items, to);
    ASSERT_TRUE(r.ok()) << r.error().message;
    std::size_t longFirst = 0, longLast = 0;
    for (std::size_t i = 0; i < r.value().lines.size(); ++i) {
        if (r.value().lines[i].find("Очень") != std::string::npos) longFirst = i;
        if (r.value().lines[i].find("рамку.") != std::string::npos) longLast = i;
    }
    EXPECT_GE(longLast, longFirst + 1);  // длинный пункт перенесён
    std::size_t texts = 0;
    for (const auto& e : d.entities()) {
        const auto* t = std::get_if<Text>(&e.geometry);
        if (!t || e.layer != "DIM") continue;
        ++texts;
        const auto [lo, hi] = drawnInk(*t);
        EXPECT_GE(lo, x0 - 1e-9) << t->utf8;
        EXPECT_LE(hi, x0 + 185.0 - to.rightClearance + 1e-9) << t->utf8;
        EXPECT_LT(hi, lay.value().frame.x1) << t->utf8;
    }
    EXPECT_EQ(texts, r.value().lines.size());
    // Первая строка пункта 4 короче прежней (последнее слово «на» перенесено).
    std::string first4;
    for (const auto& l : r.value().lines)
        if (l.rfind("4. ", 0) == 0) first4 = l;
    ASSERT_FALSE(first4.empty());
    EXPECT_LT(first4.size(), oldFirst.utf8.size());
    EXPECT_EQ(oldFirst.utf8.rfind(first4, 0), 0u);
    // Те же строки — у technicalRequirementsLines (по ним лист резервирует высоту).
    auto lines = technicalRequirementsLines(items, to);
    ASSERT_TRUE(lines.ok());
    EXPECT_EQ(lines.value(), r.value().lines);
    // Слово шире поля — ошибка с номером пункта.
    auto wide = technicalRequirementsLines({"Ок", std::string(60, 'W')}, to);
    ASSERT_FALSE(wide.ok());
    EXPECT_NE(wide.error().message.find("item 2"), std::string::npos);
}
