// Форматы ГОСТ 2.301 / ISO 216 и типы линий ГОСТ 2.303 против чисел стандартов.

#include "test_support.hpp"

#include <gtest/gtest.h>

#include <cmath>

using namespace dv2test;

TEST(DrawingV2Sheet, GostBasicFormatsTable1) {
    // ГОСТ 2.301-68, табл. 1: обозначение — размеры сторон, мм.
    const struct {
        GostFormat f;
        double a, b;
    } table[] = {{GostFormat::A0, 841, 1189}, {GostFormat::A1, 594, 841}, {GostFormat::A2, 420, 594},
                 {GostFormat::A3, 297, 420},  {GostFormat::A4, 210, 297}};
    for (const auto& r : table) {
        auto p = gostSheet(r.f, 1, Orientation::Portrait);
        ASSERT_TRUE(p) << p.error().message;
        EXPECT_DOUBLE_EQ(p.value().widthMm, r.a);
        EXPECT_DOUBLE_EQ(p.value().heightMm, r.b);
        auto l = gostSheet(r.f, 1, Orientation::Landscape);
        EXPECT_DOUBLE_EQ(l.value().widthMm, r.b);
        EXPECT_DOUBLE_EQ(l.value().heightMm, r.a);
    }
    EXPECT_EQ(gostSheet(GostFormat::A4).value().name, "А4");
}

TEST(DrawingV2Sheet, GostAdditionalFormatsTable2) {
    // ГОСТ 2.301-68, табл. 2 (выборка): кратность — размеры.
    EXPECT_DOUBLE_EQ(gostSheet(GostFormat::A0, 2).value().heightMm, 1682);
    EXPECT_DOUBLE_EQ(gostSheet(GostFormat::A1, 4).value().heightMm, 2378);
    EXPECT_DOUBLE_EQ(gostSheet(GostFormat::A2, 3).value().heightMm, 1261);
    EXPECT_DOUBLE_EQ(gostSheet(GostFormat::A3, 3).value().widthMm, 420);
    EXPECT_DOUBLE_EQ(gostSheet(GostFormat::A3, 3).value().heightMm, 891);
    EXPECT_DOUBLE_EQ(gostSheet(GostFormat::A4, 3).value().widthMm, 297);
    EXPECT_DOUBLE_EQ(gostSheet(GostFormat::A4, 3).value().heightMm, 630);
    EXPECT_DOUBLE_EQ(gostSheet(GostFormat::A4, 9).value().heightMm, 1892);
    auto l = gostSheet(GostFormat::A4, 4, Orientation::Landscape).value();
    EXPECT_DOUBLE_EQ(l.widthMm, 841);
    EXPECT_DOUBLE_EQ(l.heightMm, 297);
    EXPECT_EQ(l.name, "А4x4");
    // Кратность, которой нет в таблице 2.
    EXPECT_FALSE(gostSheet(GostFormat::A0, 4));
    EXPECT_FALSE(gostSheet(GostFormat::A4, 2));
    EXPECT_FALSE(gostSheet(GostFormat::A1, 5));
    // Кратный формат: длинная сторона ≈ n · короткая сторона основного (табл. 2 округлена).
    for (int n = 3; n <= 9; ++n) {
        EXPECT_NEAR(gostSheet(GostFormat::A4, n).value().heightMm, 210.0 * n, 2.0) << n;
    }
}

TEST(DrawingV2Sheet, Iso216ASeriesFollowsHalvingRule) {
    EXPECT_DOUBLE_EQ(iso216Sheet(0).value().widthMm, 841);
    EXPECT_DOUBLE_EQ(iso216Sheet(0).value().heightMm, 1189);
    // ISO 216: A(n+1) получается делением длинной стороны An пополам с округлением вниз до мм.
    for (int n = 0; n < 10; ++n) {
        const auto a = iso216Sheet(n).value(), b = iso216Sheet(n + 1).value();
        EXPECT_DOUBLE_EQ(b.heightMm, a.widthMm) << n;
        EXPECT_DOUBLE_EQ(b.widthMm, std::floor(a.heightMm / 2.0)) << n;
    }
    // Площадь A0 ≈ 1 м².
    EXPECT_NEAR(841.0 * 1189.0 / 1e6, 1.0, 0.001);
    EXPECT_FALSE(iso216Sheet(11));
    EXPECT_FALSE(iso216Sheet(-1));
}

TEST(DrawingV2LineTypes, ThicknessRangesGost2303Table1) {
    EXPECT_TRUE(validateMainThickness(0.5));
    EXPECT_TRUE(validateMainThickness(1.4));
    EXPECT_FALSE(validateMainThickness(0.4));
    EXPECT_FALSE(validateMainThickness(1.5));
    // (тип, мин. доля s, макс. доля s) по таблице 1.
    const struct {
        LineType t;
        double lo, hi;
    } ranges[] = {{LineType::SolidThick, 1, 1},          {LineType::SolidThin, 1.0 / 3, 0.5},
                  {LineType::SolidWavy, 1.0 / 3, 0.5},   {LineType::Dashed, 1.0 / 3, 0.5},
                  {LineType::DashDotThin, 1.0 / 3, 0.5}, {LineType::DashDotThick, 0.5, 2.0 / 3},
                  {LineType::Open, 1, 1.5},              {LineType::SolidThinZigzag, 1.0 / 3, 0.5},
                  {LineType::DashDotDot, 1.0 / 3, 0.5}};
    for (const auto& r : ranges) {
        const auto spec = defaultLineTypeSpec(r.t);
        EXPECT_TRUE(validateLineTypeSpec(spec)) << spec.gostName;
        EXPECT_GE(spec.widthFactor, r.lo - 1e-12) << spec.gostName;
        EXPECT_LE(spec.widthFactor, r.hi + 1e-12) << spec.gostName;
        auto bad = spec;
        bad.widthFactor = r.hi + 0.1;
        EXPECT_FALSE(validateLineTypeSpec(bad)) << spec.gostName;
    }
}

TEST(DrawingV2LineTypes, DashParametersGost2303Table1) {
    // Штрих и промежуток по таблице 1: штриховая 2–8 / 1–2; штрихпунктирная тонкая 5–30 / 3–5;
    // утолщённая 3–8 / 3–4; с двумя точками 5–30 / 4–6.
    const struct {
        LineType t;
        double dLo, dHi, gLo, gHi;
        int dots;
    } ranges[] = {{LineType::Dashed, 2, 8, 1, 2, 0},
                  {LineType::DashDotThin, 5, 30, 3, 5, 1},
                  {LineType::DashDotThick, 3, 8, 3, 4, 1},
                  {LineType::DashDotDot, 5, 30, 4, 6, 2}};
    for (const auto& r : ranges) {
        const auto spec = defaultLineTypeSpec(r.t);
        const auto m = patternMetrics(spec.pattern);
        EXPECT_GE(m.dash, r.dLo);
        EXPECT_LE(m.dash, r.dHi);
        EXPECT_GE(m.gap, r.gLo);
        EXPECT_LE(m.gap, r.gHi);
        EXPECT_EQ(m.dots, r.dots);
        auto longDash = spec;
        longDash.pattern[0] = r.dHi + 1.0;
        EXPECT_FALSE(validateLineTypeSpec(longDash)) << spec.gostName;
        auto wideGap = spec;
        wideGap.pattern[1] = -(r.gHi + 1.0);
        EXPECT_FALSE(validateLineTypeSpec(wideGap)) << spec.gostName;
    }
    auto continuousWithPattern = defaultLineTypeSpec(LineType::SolidThin);
    continuousWithPattern.pattern = {5, -2};
    EXPECT_FALSE(validateLineTypeSpec(continuousWithPattern));
}

TEST(DrawingV2LineTypes, RenderedDashArrayKeepsPeriod) {
    // Штрихпунктирная 15 / 1,5 · 0 · 1,5 при толщине 0,25: точка = 0,25, промежутки 1,375.
    const auto d = renderDashArray({15, -1.5, 0, -1.5}, 0.25);
    ASSERT_EQ(d.size(), 4u);
    EXPECT_DOUBLE_EQ(d[0], 15);
    EXPECT_DOUBLE_EQ(d[1], 1.375);
    EXPECT_DOUBLE_EQ(d[2], 0.25);
    EXPECT_DOUBLE_EQ(d[3], 1.375);
    double sum = 0;
    for (double v : d) sum += v;
    EXPECT_DOUBLE_EQ(sum, 18.0);
    EXPECT_TRUE(renderDashArray({}, 0.5).empty());
    const auto dd = renderDashArray({4, -1.5}, 0.25);
    ASSERT_EQ(dd.size(), 2u);
    EXPECT_DOUBLE_EQ(dd[1], 1.5);
}

TEST(DrawingV2LineTypes, DrawingOverridesAndWidths) {
    auto d = Drawing::create(gostSheet(GostFormat::A4).value(), 0.8).value();
    EXPECT_DOUBLE_EQ(d.lineWidth(LineType::SolidThick), 0.8);
    EXPECT_DOUBLE_EQ(d.lineWidth(LineType::SolidThin), 0.4);
    EXPECT_NEAR(d.lineWidth(LineType::DashDotThick), 0.8 * 2 / 3, 1e-12);
    auto spec = defaultLineTypeSpec(LineType::Dashed);
    spec.pattern = {6, -2};
    ASSERT_TRUE(d.setLineTypeSpec(spec));
    EXPECT_DOUBLE_EQ(d.lineTypeSpec(LineType::Dashed).pattern[0], 6);
    spec.pattern = {9, -2};
    EXPECT_FALSE(d.setLineTypeSpec(spec));
    EXPECT_FALSE(Drawing::create(gostSheet(GostFormat::A4).value(), 2.0));
}
