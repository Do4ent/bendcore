// Допуски ISO 286: отклонения полей против табличных значений ISO 286-2 / ГОСТ 25347
// (эталонные значения выписаны из таблиц предельных отклонений; см. ANNOTATIONS.md,
// «Источник эталонов» — требуют сверки с официальным изданием), таблица IT против формулы
// ISO 286-1 (i = 0,45·∛D + 0,001·D), запись отклонений.

#include <bendcore/drawing_v2/tolerances.hpp>

#include <gtest/gtest.h>

#include <cmath>

using namespace bendcore::drawing_v2::annotations;

namespace {

struct Ref {
    double nominal;
    const char* cls;
    double upper, lower;  // мкм
};

// Эталоны: ISO 286-2:2010 / ГОСТ 25347-2013, таблицы предельных отклонений.
const Ref kRefs[] = {
    {25, "H7", 21, 0},     {25, "h6", 0, -13},    {25, "k6", 15, 2},     {25, "g6", -7, -20},
    {25, "f7", -20, -41},  {25, "p6", 35, 22},    {25, "s6", 48, 35},    {25, "n6", 28, 15},
    {25, "m6", 21, 8},     {25, "r6", 41, 28},    {25, "e8", -40, -73},  {25, "d9", -65, -117},
    {25, "K7", 6, -15},    {25, "N7", -7, -28},   {25, "M7", 0, -21},    {25, "P7", -14, -35},
    {25, "S7", -27, -48},  {25, "R7", -20, -41},  {25, "F8", 53, 20},    {25, "K6", 2, -11},
    {25, "K8", 12, -21},   {25, "M8", 4, -29},    {25, "P9", -22, -74},  {50, "H8", 39, 0},
    {100, "H7", 35, 0},    {100, "r6", 73, 51},   {8, "H7", 15, 0},      {8, "e8", -25, -47},
    {40, "d9", -80, -142}, {10, "H7", 15, 0},     {60, "H11", 190, 0},   {2, "N7", -4, -14},
    {2, "K7", 0, -10},     {300, "M6", -9, -41},  {120, "s6", 101, 79},
};

}  // namespace

TEST(Iso286, LimitDeviationsMatchTables) {
    for (const Ref& r : kRefs) {
        auto d = limitDeviations(r.nominal, r.cls);
        ASSERT_TRUE(d.ok()) << r.cls << ": " << d.error().message;
        EXPECT_DOUBLE_EQ(d.value().upperUm, r.upper) << r.nominal << " " << r.cls;
        EXPECT_DOUBLE_EQ(d.value().lowerUm, r.lower) << r.nominal << " " << r.cls;
    }
}

TEST(Iso286, JsIsSymmetric) {
    auto d = limitDeviations(25, "js6");
    ASSERT_TRUE(d.ok());
    EXPECT_DOUBLE_EQ(d.value().upperUm, 6.5);
    EXPECT_DOUBLE_EQ(d.value().lowerUm, -6.5);
    auto D = limitDeviations(25, "JS7");
    ASSERT_TRUE(D.ok());
    EXPECT_DOUBLE_EQ(D.value().upperUm, 10.5);
}

TEST(Iso286, IntervalBoundariesAreInclusive) {
    // 30 мм — интервал «свыше 18 до 30»; 30,001 — «свыше 30 до 50».
    EXPECT_DOUBLE_EQ(standardToleranceUm(30, 7).value(), 21);
    EXPECT_DOUBLE_EQ(standardToleranceUm(30.001, 7).value(), 25);
    EXPECT_DOUBLE_EQ(standardToleranceUm(3, 6).value(), 6);
    EXPECT_DOUBLE_EQ(standardToleranceUm(500, 18).value(), 9700);
    EXPECT_FALSE(standardToleranceUm(0, 7).ok());
    EXPECT_FALSE(standardToleranceUm(500.1, 7).ok());
    EXPECT_FALSE(standardToleranceUm(1, 14).ok());  // IT14..18 не для размеров до 1 мм
}

// Таблица IT против формулы ISO 286-1 (IT5…IT16 = k·i, D — среднее геометрическое границ
// интервала). Табличные значения округлены по правилам стандарта, поэтому сверка — 10 %.
TEST(Iso286, ItTableAgreesWithFormula) {
    const double steps[] = {3, 6, 10, 18, 30, 50, 80, 120, 180, 250, 315, 400, 500};
    const int k[] = {7, 10, 16, 25, 40, 64, 100, 160, 250, 400, 640, 1000};  // IT5..IT16
    for (int s = 1; s < 13; ++s) {
        const double D = std::sqrt(steps[s - 1] * steps[s]);
        const double i = 0.45 * std::cbrt(D) + 0.001 * D;
        for (int g = 5; g <= 16; ++g) {
            const double table = standardToleranceUm(steps[s], g).value();
            EXPECT_NEAR(table / (k[g - 5] * i), 1.0, 0.1) << "IT" << g << " up to " << steps[s];
        }
        // Ряд R5 со знаменателем 10: IT(n+5) = 10·IT(n) для n ≥ 7.
        for (int g = 7; g <= 13; ++g) {
            EXPECT_DOUBLE_EQ(standardToleranceUm(steps[s], g + 5).value(), 10 * standardToleranceUm(steps[s], g).value());
        }
    }
}

TEST(Iso286, ParseAndErrors) {
    auto c = parseToleranceClass("js6");
    ASSERT_TRUE(c.ok());
    EXPECT_FALSE(c.value().hole());
    EXPECT_EQ(c.value().label(), "js6");
    EXPECT_TRUE(parseToleranceClass("H7").value().hole());
    EXPECT_FALSE(parseToleranceClass("x7").ok());  // не поддерживается
    EXPECT_FALSE(parseToleranceClass("H").ok());
    EXPECT_FALSE(parseToleranceClass("H19").ok());
    EXPECT_FALSE(parseToleranceClass("Js6").ok());
    EXPECT_FALSE(limitDeviations(0.5, "a11").ok());
    EXPECT_FALSE(limitDeviations(600, "H7").ok());
}

TEST(Iso286, Formatting) {
    EXPECT_EQ(formatDeviationMm(21), "+0,021");
    EXPECT_EQ(formatDeviationMm(-13), "−0,013");
    EXPECT_EQ(formatDeviationMm(0), "0");
    EXPECT_EQ(formatDeviationMm(6.5), "+0,0065");
    EXPECT_EQ(formatDeviationMm(190), "+0,19");

    const auto h7 = parseToleranceClass("H7").value();
    const auto dev = limitDeviations(25, h7).value();
    EXPECT_EQ(toleranceText("⌀25", h7, dev, ToleranceDisplay::ClassOnly).main, "⌀25H7");
    auto t = toleranceText("25", h7, dev, ToleranceDisplay::Deviations, true);
    EXPECT_EQ(t.main, "25");
    ASSERT_TRUE(t.upper);
    EXPECT_EQ(*t.upper, "+0,021");
    EXPECT_FALSE(t.lower);  // нулевое отклонение не пишется (ЕСКД)
    t = toleranceText("25", h7, dev, ToleranceDisplay::Deviations, false);
    ASSERT_TRUE(t.lower);
    EXPECT_EQ(*t.lower, "0");  // ISO 129-1
    t = toleranceText("25", h7, dev, ToleranceDisplay::ClassAndDeviations, true);
    EXPECT_EQ(t.main, "25H7");
    EXPECT_EQ(t.suffixOpen, "(");
    const auto js = parseToleranceClass("js6").value();
    t = toleranceText("25", js, limitDeviations(25, js).value(), ToleranceDisplay::Deviations);
    EXPECT_EQ(t.main, "25±0,0065");
    EXPECT_FALSE(t.upper);
}
