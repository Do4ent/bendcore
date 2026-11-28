// Проверки примитивов, штриховки и вспомогательных линий против аналитики.

#include "test_support.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <numbers>

using namespace dv2test;

namespace {
Drawing a4() { return Drawing::create(gostSheet(GostFormat::A4).value()).value(); }
double len(const LineSeg& s) { return std::hypot(s.b.x - s.a.x, s.b.y - s.a.y); }
}  // namespace

TEST(DrawingV2Model, EntityValidation) {
    auto d = a4();
    EXPECT_TRUE(d.add({LineSeg{{0, 0}, {10, 0}}, "0", LineType::SolidThick}));
    EXPECT_FALSE(d.add({LineSeg{{0, 0}, {0, 0}}, "0", LineType::SolidThick}));
    EXPECT_FALSE(d.add({LineSeg{{0, 0}, {10, 0}}, "NOPE", LineType::SolidThick}));
    EXPECT_FALSE(d.add({LineSeg{{0, NAN}, {10, 0}}, "0", LineType::SolidThick}));
    EXPECT_FALSE(d.add({Circle{{0, 0}, 0}, "0", LineType::SolidThick}));
    EXPECT_FALSE(d.add({Arc{{0, 0}, 5, 30, 30}, "0", LineType::SolidThick}));
    EXPECT_FALSE(d.add({Polyline{{{0, 0}}, false}, "0", LineType::SolidThick}));
    EXPECT_FALSE(d.add({Polyline{{{0, 0}, {1, 1}}, true}, "0", LineType::SolidThick}));
    // Разомкнутая линия: штрих 8…20 мм.
    EXPECT_TRUE(d.add({LineSeg{{0, 0}, {12, 0}}, "0", LineType::Open}));
    EXPECT_FALSE(d.add({LineSeg{{0, 0}, {25, 0}}, "0", LineType::Open}));
    EXPECT_FALSE(d.add({LineSeg{{0, 0}, {5, 0}}, "0", LineType::Open}));
    EXPECT_FALSE(d.add({Circle{{0, 0}, 5}, "0", LineType::Open}));
    EXPECT_EQ(d.entities().size(), 2u);
    EXPECT_FALSE(d.addLayer({"Слой", 7}));
    EXPECT_FALSE(d.addLayer({"0", 7}));
    EXPECT_FALSE(d.addLayer({"X", 300}));
}

TEST(DrawingV2Model, TextHeightsGost2304) {
    auto d = a4();
    for (double h : {2.5, 3.5, 5.0, 7.0, 10.0, 14.0, 20.0}) {
        Text t;
        t.utf8 = "Ab";
        t.height = h;
        EXPECT_TRUE(d.add({t, "0", LineType::SolidThin})) << h;
    }
    for (double h : {3.0, 4.0, 1.0, 25.0}) {
        Text t;
        t.utf8 = "Ab";
        t.height = h;
        EXPECT_FALSE(d.add({t, "0", LineType::SolidThin})) << h;
    }
    Text bad;
    bad.utf8 = "\xD0";  // обрезанная последовательность UTF-8
    EXPECT_FALSE(d.add({bad, "0", LineType::SolidThin}));
    bad.utf8 = "a\nb";
    EXPECT_FALSE(d.add({bad, "0", LineType::SolidThin}));
    Text wf;
    wf.utf8 = "x";
    wf.widthFactor = 0.4;
    EXPECT_FALSE(d.add({wf, "0", LineType::SolidThin}));
    EXPECT_DOUBLE_EQ(kGostTextSlantDeg, 75.0);
}

TEST(DrawingV2Model, Utf8RoundTrip) {
    const std::string s = "Ж Ø ±°€ 𝔸";
    auto d = decodeUtf8(s);
    ASSERT_TRUE(d);
    EXPECT_EQ(d.value()[0], U'Ж');
    EXPECT_EQ(d.value().back(), char32_t(0x1D538));
    EXPECT_EQ(encodeUtf8(d.value()), s);
    EXPECT_FALSE(decodeUtf8("\xC0\x80"));      // «длинная» кодировка
    EXPECT_FALSE(decodeUtf8("\xED\xA0\x80"));  // суррогат
}

TEST(DrawingV2Model, HatchSquareAtZeroDegrees) {
    Hatch h{{{{0, 0}, {10, 0}, {10, 10}, {0, 10}}}, 0.0, 2.0};
    auto segs = hatchSegments(h).value();
    // Линии y = (k + 1/2)·2: 1, 3, 5, 7, 9 — каждая длиной 10.
    ASSERT_EQ(segs.size(), 5u);
    for (std::size_t i = 0; i < segs.size(); ++i) {
        EXPECT_NEAR(segs[i].a.y, 1.0 + 2.0 * double(i), 1e-12);
        EXPECT_NEAR(segs[i].b.y, segs[i].a.y, 1e-12);
        EXPECT_NEAR(len(segs[i]), 10.0, 1e-12);
    }
}

TEST(DrawingV2Model, HatchAt45DegreesCoversAreaWithHole) {
    // Квадрат 40×40 с квадратным отверстием 10×10: площадь 1500.
    Hatch h{{{{0, 0}, {40, 0}, {40, 40}, {0, 40}}, {{15, 15}, {25, 15}, {25, 25}, {15, 25}}}, 45.0, 1.0};
    auto segs = hatchSegments(h).value();
    double total = 0;
    const double c = std::cos(std::numbers::pi / 4), s = std::sin(std::numbers::pi / 4);
    for (const auto& g : segs) {
        total += len(g);
        // Направление — 45°.
        EXPECT_NEAR(std::fabs((g.b.x - g.a.x) * s - (g.b.y - g.a.y) * c), 0.0, 1e-9);
        // Ни одна точка не попадает внутрь отверстия.
        const double mx = (g.a.x + g.b.x) / 2, my = (g.a.y + g.b.y) / 2;
        EXPECT_FALSE(mx > 15 + 1e-9 && mx < 25 - 1e-9 && my > 15 + 1e-9 && my < 25 - 1e-9);
        // Расстояние линии от начала координат по нормали — (k + 1/2)·шаг.
        const double v = -g.a.x * s + g.a.y * c;
        EXPECT_NEAR(v - 0.5, std::round(v - 0.5), 1e-9);
    }
    // Сумма длин · шаг ≈ площадь (правило средних точек).
    EXPECT_NEAR(total * 1.0, 1500.0, 1500.0 * 0.01);
    auto d = a4();
    EXPECT_FALSE(d.add({Hatch{{{{0, 0}, {1, 0}, {1, 1}}}, 45, 0.5}, "0", LineType::SolidThin}));
    EXPECT_FALSE(d.add({Hatch{{{{0, 0}, {1, 0}, {1, 1}}}, 45, 2}, "0", LineType::SolidThick}));
    EXPECT_TRUE(d.add({Hatch{{{{0, 0}, {1, 0}, {1, 1}}}, 45, 2}, "0", LineType::SolidThin}));
}

TEST(DrawingV2Model, WavyAndZigzagGeometry) {
    auto w = makeWavyLine({0, 0}, {40, 0}, 1.0, 8.0).value();
    EXPECT_EQ(w.points.size(), 5u * 32 + 1);
    EXPECT_DOUBLE_EQ(w.points.front().x, 0);
    EXPECT_DOUBLE_EQ(w.points.back().x, 40);
    double maxY = 0;
    for (const auto& p : w.points) maxY = std::max(maxY, std::fabs(p.y));
    EXPECT_NEAR(maxY, 1.0, 1e-9);  // отсчёт 8 из 32 попадает в пик синуса
    auto z = makeZigzagLine({0, 0}, {0, 30}, 1, 5.0, 4.0).value();
    ASSERT_EQ(z.points.size(), 6u);
    // Излом посередине: точки на расстоянии ±2,5 мм от оси, концы на оси.
    EXPECT_NEAR(z.points[2].x, -2.5, 1e-12);
    EXPECT_NEAR(z.points[3].x, 2.5, 1e-12);
    EXPECT_NEAR(z.points[1].y, 13.0, 1e-12);
    EXPECT_NEAR(z.points[4].y, 17.0, 1e-12);
    EXPECT_FALSE(makeZigzagLine({0, 0}, {3, 0}, 1, 5, 4));
    EXPECT_FALSE(makeWavyLine({0, 0}, {0, 0}));
}
