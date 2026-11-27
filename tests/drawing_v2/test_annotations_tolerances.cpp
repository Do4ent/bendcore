// Допуски ISO 286: отклонения полей против табличных значений ISO 286-2 / ГОСТ 25347
// (эталонные значения выписаны из таблиц предельных отклонений; см. ANNOTATIONS.md,
// «Источник эталонов» — требуют сверки с официальным изданием), таблица IT против формулы
// ISO 286-1 (i = 0,45·∛D + 0,001·D), запись отклонений.

#include <bendcore/drawing_v2/tolerances.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>

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
    EXPECT_FALSE(standardToleranceUm(3150.1, 7).ok());
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
    EXPECT_FALSE(parseToleranceClass("cd7").ok());  // CD не поддерживается
    EXPECT_FALSE(parseToleranceClass("H").ok());
    EXPECT_FALSE(parseToleranceClass("H19").ok());
    EXPECT_FALSE(parseToleranceClass("Js6").ok());
    EXPECT_FALSE(limitDeviations(0.5, "a11").ok());
    EXPECT_FALSE(limitDeviations(3200, "H7").ok());
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

// ---------------------------------------------------------------------------------------------
// Расширение: j/J, t…zc / T…ZC, размеры свыше 500 мм, посадки, сверка с формулами ISO 286-1.
// Эталонные значения посчитаны по таблицам основных отклонений и IT (ISO 286-1) вручную:
// ei/es + IT, для отверстий ES = −ei + Δ; требуют сверки с официальным изданием.
// ---------------------------------------------------------------------------------------------

namespace {

bool dumpEnabled() { return std::getenv("BENDCORE_ISO286_DUMP") != nullptr; }

const double kFineBounds[] = {3,    6,    10,   14,   18,   24,   30,   40,   50,   65,   80,   100,  120,
                              140,  160,  180,  200,  225,  250,  280,  315,  355,  400,  450,  500,  560,
                              630,  710,  800,  900,  1000, 1120, 1250, 1400, 1600, 1800, 2000, 2240, 2500,
                              2800, 3150};

void expectRefs(const Ref* refs, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) {
        const Ref& r = refs[i];
        auto d = limitDeviations(r.nominal, r.cls);
        ASSERT_TRUE(d.ok()) << r.cls << ": " << d.error().message;
        EXPECT_DOUBLE_EQ(d.value().upperUm, r.upper) << r.nominal << " " << r.cls;
        EXPECT_DOUBLE_EQ(d.value().lowerUm, r.lower) << r.nominal << " " << r.cls;
    }
}

}  // namespace

TEST(Iso286Ext, JAndHoleJ) {
    const Ref refs[] = {
        {25, "j5", 5, -4},   {25, "j6", 9, -4},   {25, "j7", 13, -8},   {10, "j6", 7, -2},
        {5, "j6", 6, -2},    {2, "j7", 6, -4},    {100, "j6", 13, -9},  {25, "J6", 8, -5},
        {25, "J7", 12, -9},  {25, "J8", 20, -13}, {5, "J7", 6, -6},     {100, "J7", 22, -13},
        {400, "J8", 60, -29},
    };
    expectRefs(refs, sizeof refs / sizeof refs[0]);
    EXPECT_FALSE(limitDeviations(25, "j4").ok());
    EXPECT_FALSE(limitDeviations(25, "j8").ok());
    EXPECT_FALSE(limitDeviations(25, "J5").ok());
    EXPECT_FALSE(limitDeviations(25, "J9").ok());
    EXPECT_FALSE(limitDeviations(600, "j6").ok());
    EXPECT_FALSE(limitDeviations(600, "J7").ok());
}

TEST(Iso286Ext, ShaftTtoZc) {
    const Ref refs[] = {
        {25, "t6", 54, 41},    {40, "t6", 64, 48},    {25, "u6", 61, 48},    {25, "u7", 69, 48},
        {16, "v6", 50, 39},    {25, "v6", 68, 55},    {25, "x6", 77, 64},    {8, "x6", 43, 34},
        {25, "x8", 97, 64},    {25, "y6", 88, 75},    {25, "z6", 101, 88},   {25, "za6", 131, 118},
        {25, "zb6", 173, 160}, {25, "zc9", 270, 218}, {2, "u6", 24, 18},     {2, "zc6", 66, 60},
        {200, "u6", 265, 236}, {450, "zc7", 2463, 2400},
    };
    expectRefs(refs, sizeof refs / sizeof refs[0]);
    // t — с 24 мм, v — с 14 мм, y — с 18 мм.
    EXPECT_FALSE(limitDeviations(20, "t6").ok());
    EXPECT_TRUE(limitDeviations(24.001, "t6").ok());
    EXPECT_FALSE(limitDeviations(10, "v6").ok());
    EXPECT_TRUE(limitDeviations(15, "v6").ok());
    EXPECT_FALSE(limitDeviations(15, "y6").ok());
    EXPECT_TRUE(limitDeviations(20, "y6").ok());
}

TEST(Iso286Ext, HoleTtoZcWithDelta) {
    // ES = −ei + Δ для IT ≤ 7, ES = −ei для IT > 7 (ISO 286-1, п. 4.3.2.5–4.3.2.6).
    const Ref refs[] = {
        {25, "U7", -40, -61},   {20, "U7", -33, -54},  {25, "U8", -48, -81},    {40, "T7", -39, -64},
        {25, "X7", -56, -77},   {25, "X8", -64, -97},  {25, "Z7", -80, -101},   {25, "Z8", -88, -121},
        {25, "ZA7", -110, -131}, {25, "ZC10", -218, -302}, {100, "R7", -38, -73}, {2, "U7", -18, -28},
        {200, "U6", -227, -256},
    };
    expectRefs(refs, sizeof refs / sizeof refs[0]);
    // Свойство общего правила: при IT > 7 ES(U9) = −ei(u9).
    for (double D : {5.0, 25.0, 70.0, 300.0}) {
        auto hole = limitDeviations(D, "U9");
        auto shaft = limitDeviations(D, "u9");
        ASSERT_TRUE(hole.ok() && shaft.ok());
        EXPECT_DOUBLE_EQ(hole.value().upperUm, -shaft.value().lowerUm);
    }
}

TEST(Iso286Ext, LargeSizesIt) {
    EXPECT_DOUBLE_EQ(standardToleranceUm(500.1, 7).value(), 70);
    EXPECT_DOUBLE_EQ(standardToleranceUm(630, 7).value(), 70);
    EXPECT_DOUBLE_EQ(standardToleranceUm(630.1, 7).value(), 80);
    EXPECT_DOUBLE_EQ(standardToleranceUm(3150, 7).value(), 210);
    EXPECT_DOUBLE_EQ(standardToleranceUm(3150, 18).value(), 33000);
    EXPECT_DOUBLE_EQ(standardToleranceUm(1000, 1).value(), 11);
    EXPECT_FALSE(standardToleranceUm(3150.1, 7).ok());
    EXPECT_FALSE(standardToleranceUm(600, 0).ok());   // IT0 — только до 500 мм
    EXPECT_FALSE(standardToleranceUm(600, -1).ok());  // IT01 — только до 500 мм
    // Ряд IT(n+5) = 10·IT(n), n = 6…13, для всех интервалов свыше 500 мм.
    for (double D : {600.0, 700.0, 900.0, 1100.0, 1500.0, 1800.0, 2200.0, 3000.0}) {
        for (int g = 6; g <= 13; ++g) {
            EXPECT_DOUBLE_EQ(standardToleranceUm(D, g + 5).value(), 10 * standardToleranceUm(D, g).value())
                << D << " IT" << g;
        }
    }
}

TEST(Iso286Ext, LargeSizesDeviations) {
    const Ref refs[] = {
        {600, "H7", 70, 0},      {600, "h6", 0, -44},      {700, "e8", -160, -285},  {900, "d9", -320, -550},
        {1500, "f7", -110, -235}, {1900, "g6", -32, -124}, {600, "m6", 70, 26},      {750, "n6", 100, 50},
        {950, "p6", 156, 100},   {600, "JS7", 35, -35},    {3000, "h18", 0, -33000}, {600, "D9", 435, 260},
        {600, "E8", 255, 145},
    };
    expectRefs(refs, sizeof refs / sizeof refs[0]);
    // Явная ошибка там, где стандарт свыше 500 мм основных отклонений не задаёт.
    for (const char* c : {"a11", "b11", "c9", "j6", "v6", "x6", "y6", "z6", "za6", "zb6", "zc6", "A11", "X7", "ZC8", "J7"}) {
        EXPECT_FALSE(limitDeviations(600, c).ok()) << c;
    }
    EXPECT_FALSE(limitDeviations(3200, "H7").ok());
    EXPECT_TRUE(limitDeviations(3150, "H7").ok());
    // Граница 500 мм: 500 — прежний интервал, 500,001 — «свыше 500 до 560».
    EXPECT_DOUBLE_EQ(limitDeviations(500, "e8").value().upperUm, -135);
    EXPECT_DOUBLE_EQ(limitDeviations(500.001, "e8").value().upperUm, -145);
}

TEST(Iso286Ext, FitsClearanceTransitionInterference) {
    struct F {
        const char* fit;
        double nominal, maxC, minC;
        FitType type;
    };
    const F fits[] = {
        {"H7/s6", 25, -14, -48, FitType::Interference},
        {"H7/u6", 25, -27, -61, FitType::Interference},
        {"H8/x8", 25, -31, -97, FitType::Interference},
        {"JS7/h6", 25, 23.5, -10.5, FitType::Transition},
        {"H7/j6", 25, 25, -9, FitType::Transition},
        {"H7/k6", 25, 19, -15, FitType::Transition},
        {"H7/f7", 25, 62, 20, FitType::Clearance},
        {"H7/g6", 25, 41, 7, FitType::Clearance},
        {"H11/d11", 25, 325, 65, FitType::Clearance},
        {"H7/h6", 25, 34, 0, FitType::Clearance},
        {"H7/r6", 100, -16, -73, FitType::Interference},
        {"H7/e8", 600, 325, 145, FitType::Clearance},
    };
    for (const F& f : fits) {
        auto r = fitLimits(f.nominal, f.fit);
        ASSERT_TRUE(r.ok()) << f.fit << ": " << r.error().message;
        EXPECT_DOUBLE_EQ(r.value().maxClearanceUm, f.maxC) << f.fit;
        EXPECT_DOUBLE_EQ(r.value().minClearanceUm, f.minC) << f.fit;
        EXPECT_EQ(r.value().type, f.type) << f.fit;
        EXPECT_EQ(r.value().label, f.fit);
        EXPECT_DOUBLE_EQ(r.value().fitToleranceUm, f.maxC - f.minC) << f.fit;
    }
    // Перепутанный регистр и нераспознанная запись — ошибка.
    EXPECT_FALSE(fitLimits(25, "h6", "H7").ok());
    EXPECT_FALSE(fitLimits(25, "H7").ok());
    EXPECT_FALSE(fitLimits(25, "H7/H6").ok());
    EXPECT_FALSE(fitLimits(600, "H7/x8").ok());
    // Правило Δ: посадки H7/p6 и P7/h6 дают одни и те же предельные зазоры/натяги.
    auto a = fitLimits(25, "H7/p6");
    ASSERT_TRUE(a.ok());
    const auto P7 = limitDeviations(25, "P7").value();
    const auto h6 = limitDeviations(25, "h6").value();
    EXPECT_DOUBLE_EQ(P7.upperUm - h6.lowerUm, a.value().maxClearanceUm);
    EXPECT_DOUBLE_EQ(P7.lowerUm - h6.upperUm, a.value().minClearanceUm);
}

// Независимая сверка: формулы ISO 286-1 (табл. 2 и 4) против таблиц реализации. Допуски — по
// формулам стандарта без округления; таблицы округлены по правилам стандарта, поэтому границы:
//  - до 500 мм, IT5…IT18, интервалы свыше 10 мм: ≤ 6 %; IT6…IT18 свыше 6 мм: ≤ 5 %;
//  - до 500 мм, интервалы до 10 мм и IT01…IT4: формулы малых размеров стандарта приближённые,
//    расхождения до 28 % (значения — в docs/STANDARDS_VERIFICATION.md);
//  - свыше 500 мм: IT6…IT18 ≤ 2 %, IT1…IT5 ≤ 7 %.
TEST(Iso286Ext, ItTableMatchesFormula) {
    const double mains[] = {3,   6,   10,  18,  30,   50,   80,   120,  180,  250,  315,
                            400, 500, 630, 800, 1000, 1250, 1600, 2000, 2500, 3150};
    int checked = 0;
    for (double hi : mains) {
        for (int g = (hi <= 500 ? -1 : 1); g <= 18; ++g) {
            if (g >= 14 && hi <= 1.0) continue;
            const double table = standardToleranceUm(hi, g).value();
            const double formula = standardToleranceFormulaUm(hi, g).value();
            const double rel = std::abs(table - formula) / formula;
            if (dumpEnabled()) {
                std::printf("IT D<=%g g=%d table=%g formula=%.3f rel=%.4f abs=%.3f\n", hi, g, table, formula, rel,
                            std::abs(table - formula));
            }
            double bound = 1.0;
            if (hi > 500) bound = g >= 6 ? 0.02 : 0.07;
            else if (hi > 10 && g >= 5) bound = 0.06;
            else if (hi > 6 && g >= 6) bound = 0.05;
            else if (g >= 1 && hi > 3) bound = 0.22;  // IT1…IT4, малые размеры
            else bound = 0.3;
            EXPECT_LE(rel, bound) << "IT" << g << " up to " << hi;
            ++checked;
        }
    }
    EXPECT_GT(checked, 300);
}

// Основные отклонения: формулы табл. 4 против таблиц для интервалов свыше 18 мм (для меньших
// размеров формулы t…zc, m, n и др. стандарт сводит к таблицам — расхождения в
// STANDARDS_VERIFICATION.md). Для s свыше 500 мм, r, t, u свыше 500 мм таблица — сама расчёт.
TEST(Iso286Ext, FundamentalDeviationsMatchFormulas) {
    const char* letters[] = {"a", "b", "c", "d", "e", "f", "g", "m", "n", "p", "r", "s", "t", "u", "v", "x", "y", "z", "za", "zb", "zc"};
    int checked = 0;
    for (const char* l : letters) {
        for (std::size_t fi = 0; fi < 41; ++fi) {
            const double hi = kFineBounds[fi];
            const double lo = fi ? kFineBounds[fi - 1] : 1.0;
            const double mid = 0.5 * (lo + hi);
            auto f = shaftFundamentalFormulaUm(mid, l);
            if (!f) continue;
            auto dev = limitDeviations(mid, std::string(l) + "9");
            if (!dev) continue;
            const double table = dev.value().fundamentalUm;
            const double rel = std::abs(table - f.value()) / std::abs(f.value());
            if (dumpEnabled()) {
                std::printf("FD %s %g-%g table=%g formula=%.2f abs=%.2f rel=%.4f\n", l, lo, hi, table, f.value(),
                            std::abs(table - f.value()), rel);
            }
            if (hi <= 18) continue;
            EXPECT_LE(rel, std::string(l) == "c" ? 0.16 : 0.1) << l << " " << lo << "-" << hi;
            ++checked;
        }
    }
    EXPECT_GT(checked, 300);
}
