// Резьбовое отверстие: таблица ISO 261 (шаги), диаметр под сверление D − P, внутренний
// диаметр D1, удалённый объём, атрибуты условной резьбы, ошибки.

#include "features_test_util.hpp"

#include <gtest/gtest.h>

using namespace ft;

namespace {
constexpr double kPi = 3.14159265358979323846;
}

TEST(FeaturesThread, MetricTable) {
    // Крупный шаг ISO 261 и сверло D − P (ISO 2306): M3 → 0,5 / 2,5; M6 → 1 / 5; M8 → 1,25 / 6,75;
    // M10 → 1,5 / 8,5; M12 → 1,75 / 10,25; M16 → 2 / 14; M20 → 2,5 / 17,5; M24 → 3 / 21.
    const std::vector<std::array<double, 3>> coarse = {{3, 0.5, 2.5},    {6, 1.0, 5.0},   {8, 1.25, 6.75},
                                                       {10, 1.5, 8.5},   {12, 1.75, 10.25}, {16, 2.0, 14.0},
                                                       {20, 2.5, 17.5},  {24, 3.0, 21.0}};
    for (const auto& [d, pitch, drill] : coarse) {
        auto t = metricCoarse(d);
        ASSERT_TRUE(t.ok()) << d;
        EXPECT_DOUBLE_EQ(t.value().pitch, pitch) << "M" << d;
        EXPECT_EQ(t.value().series, ThreadSeries::Coarse);
        EXPECT_NEAR(tapDrillDiameter(t.value()), drill, 1e-12) << "M" << d;
    }
    // D1 = D − 5√3/8·P: M10 → 10 − 1,623798 = 8,376202 (ISO 724: 8,376).
    EXPECT_NEAR(minorDiameter(metricCoarse(10).value()), 8.376, 5e-4);
    EXPECT_NEAR(minorDiameter(metricCoarse(6).value()), 4.917, 5e-4);
    auto fine = metricThread(10, 1.25);
    ASSERT_TRUE(fine.ok());
    EXPECT_EQ(fine.value().series, ThreadSeries::Fine);
    EXPECT_EQ(threadDesignation(fine.value(), ThreadHand::Right), "M10x1.25");
    EXPECT_EQ(threadDesignation(metricCoarse(10).value(), ThreadHand::Left), "M10LH");
    auto pitches = metricFinePitches(12);
    ASSERT_TRUE(pitches.ok());
    EXPECT_EQ(pitches.value(), (std::vector<double>{1.5, 1.25, 1.0}));
    EXPECT_FALSE(metricThread(10, 1.3).ok());
    EXPECT_FALSE(metricCoarse(11).ok());
    EXPECT_EQ(metricNominals().front(), 1.0);
    EXPECT_EQ(metricNominals().back(), 48.0);
}

TEST(FeaturesThread, BlindThreadedHole) {
    const TopoDS_Shape b = box(40, 40, 30);
    const TopoDS_Face top = planeFaceAt(b, 2, 30);
    ThreadedHoleParams p;
    p.thread = metricCoarse(10).value();
    p.threadLength = 15;
    p.drillPoint = false;
    auto res = threadedHole(b, top, gp_Pnt(20, 20, 30), p);
    ASSERT_TRUE(res.ok()) << res.error().message;
    EXPECT_TRUE(valid(res.value().shape));
    // Отверстие под резьбу Ø8,5 на глубину 15 + 3·1,5 = 19,5.
    const double removed = kPi * 4.25 * 4.25 * 19.5;
    EXPECT_NEAR(volume(b) - volume(res.value().shape), removed, 1e-6 * removed);
    ASSERT_EQ(res.value().threads.size(), 1u);
    const ThreadedFace& tf = res.value().threads.front();
    BRepAdaptor_Surface s(tf.face);
    ASSERT_EQ(s.GetType(), GeomAbs_Cylinder);
    EXPECT_NEAR(s.Cylinder().Radius(), 4.25, 1e-12);
    EXPECT_TRUE(contains(res.value().shape, tf.face));
    EXPECT_EQ(tf.thread.designation, "M10");
    EXPECT_DOUBLE_EQ(tf.thread.nominal, 10.0);
    EXPECT_DOUBLE_EQ(tf.thread.pitch, 1.5);
    EXPECT_DOUBLE_EQ(tf.thread.length, 15.0);
    EXPECT_DOUBLE_EQ(tf.thread.drill, 8.5);
    EXPECT_EQ(tf.thread.hand, ThreadHand::Right);
    EXPECT_TRUE(tf.thread.internal);
    EXPECT_NEAR(tf.thread.entry.Distance(gp_Pnt(20, 20, 30)), 0.0, 1e-12);
    EXPECT_TRUE(tf.thread.direction.IsEqual(gp_Dir(0, 0, -1), 1e-12));
    // Атрибуты туда и обратно.
    auto attrs = threadAttributes(tf.thread);
    auto back = threadFromAttributes(attrs);
    ASSERT_TRUE(back.ok()) << back.error().message;
    EXPECT_EQ(back.value().designation, "M10");
    EXPECT_DOUBLE_EQ(back.value().length, 15.0);
    EXPECT_DOUBLE_EQ(back.value().minor, tf.thread.minor);
    EXPECT_TRUE(back.value().direction.IsEqual(tf.thread.direction, 1e-12));
    attrs.erase(attrs.begin());
    EXPECT_FALSE(threadFromAttributes(attrs).ok());
}

TEST(FeaturesThread, ThroughFineLeftHand) {
    const TopoDS_Shape b = box(30, 30, 12);
    ThreadedHoleParams p;
    p.thread = metricThread(12, 1.25).value();
    p.hand = ThreadHand::Left;
    p.end = ThreadEnd::Through;
    p.threadLength = 0;  // на всю длину
    auto res = threadedHole(b, planeFaceAt(b, 2, 12), gp_Pnt(15, 15, 12), p);
    ASSERT_TRUE(res.ok()) << res.error().message;
    const double removed = kPi * (10.75 / 2) * (10.75 / 2) * 12;
    EXPECT_NEAR(volume(b) - volume(res.value().shape), removed, 1e-6 * removed);
    ASSERT_EQ(res.value().threads.size(), 1u);
    EXPECT_EQ(res.value().threads.front().thread.designation, "M12x1.25LH");
    EXPECT_NEAR(res.value().threads.front().thread.length, 12.0, 1e-9);
    p.threadLength = 20;
    auto tooLong = threadedHole(b, planeFaceAt(b, 2, 12), gp_Pnt(15, 15, 12), p);
    ASSERT_FALSE(tooLong.ok());
}

TEST(FeaturesThread, Errors) {
    const TopoDS_Shape b = box(40, 40, 30);
    const TopoDS_Face top = planeFaceAt(b, 2, 30);
    ThreadedHoleParams p;
    p.thread = metricCoarse(8).value();
    p.threadLength = 12;
    p.drillDepth = 10;
    auto deep = threadedHole(b, top, gp_Pnt(20, 20, 30), p);
    ASSERT_FALSE(deep.ok());
    EXPECT_NE(deep.error().message.find("exceeds"), std::string::npos);
    p.drillDepth = 0;
    p.drillDiameter = 9.0;
    auto wide = threadedHole(b, top, gp_Pnt(20, 20, 30), p);
    ASSERT_FALSE(wide.ok());
    EXPECT_EQ(wide.error().code, ErrorCode::InvalidInput);
    p.drillDiameter.reset();
    p.thread.pitch = 0;
    EXPECT_FALSE(threadedHole(b, top, gp_Pnt(20, 20, 30), p).ok());
}
