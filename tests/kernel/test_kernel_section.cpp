// Planar sections and half-space clipping against closed-form areas and volumes
// (docs/kernel/ANALYSIS.md).

#include <bendcore/kernel/section.hpp>

#include <gtest/gtest.h>

#include <cmath>

#include <BRepAlgoAPI_Cut.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <BRep_Tool.hxx>

using namespace bendcore::kernel;

namespace {

constexpr double kPi = 3.14159265358979323846;

std::size_t countOf(const Shape& s, TopAbs_ShapeEnum type) {
    if (s.isNull()) return 0;
    std::size_t n = 0;
    for (TopExp_Explorer it(s.occt(), type); it.More(); it.Next()) ++n;
    return n;
}

double volumeOf(const Shape& s) {
    if (countOf(s, TopAbs_SOLID) == 0) return 0.0;
    auto v = s.volumeProperties();
    return v.ok() ? v.value().volume : -1.0;
}

std::size_t closedWires(const Shape& wires) {
    std::size_t n = 0;
    for (TopExp_Explorer it(wires.occt(), TopAbs_WIRE); it.More(); it.Next())
        if (BRep_Tool::IsClosed(TopoDS::Wire(it.Current()))) ++n;
    return n;
}

}  // namespace

TEST(Section, BoxByHorizontalPlane) {
    const Shape box(BRepPrimAPI_MakeBox(10, 20, 30).Shape());
    const SectionPlane plane{{0, 0, 12}, {0, 0, 1}};
    auto s = sectionByPlane(box, plane);
    ASSERT_TRUE(s.ok()) << s.error().message;
    EXPECT_NEAR(s.value().area, 200.0, 1e-9);
    EXPECT_EQ(countOf(s.value().edges, TopAbs_EDGE), 4u);
    EXPECT_EQ(closedWires(s.value().wires), 1u);
    EXPECT_EQ(countOf(s.value().faces, TopAbs_FACE), 1u);
    ASSERT_EQ(s.value().polylines.size(), 4u);
    for (const auto& line : s.value().polylines)
        for (const Point3& p : line) EXPECT_NEAR(p[2], 12.0, 1e-9);

    auto clip = clipByPlane(box, plane);
    ASSERT_TRUE(clip.ok()) << clip.error().message;
    EXPECT_TRUE(clip.value().checkValidity().valid);
    EXPECT_NEAR(volumeOf(clip.value()), 10 * 20 * 12, 1e-7);
    // Opposite normal keeps the upper part.
    auto upper = clipByPlane(box, SectionPlane{{0, 0, 12}, {0, 0, -3}});
    ASSERT_TRUE(upper.ok());
    EXPECT_NEAR(volumeOf(upper.value()), 10 * 20 * 18, 1e-7);
}

TEST(Section, ObliquePlaneThroughCube) {
    // Plane through the cube centre with normal (1, 1, 0): a 10 x 10*sqrt(2) rectangle.
    const Shape cube(BRepPrimAPI_MakeBox(10, 10, 10).Shape());
    const SectionPlane plane{{5, 5, 5}, {1, 1, 0}};
    auto s = sectionByPlane(cube, plane);
    ASSERT_TRUE(s.ok());
    EXPECT_NEAR(s.value().area, 100 * std::sqrt(2.0), 1e-7);
    auto clip = clipByPlane(cube, plane);
    ASSERT_TRUE(clip.ok());
    EXPECT_NEAR(volumeOf(clip.value()), 500.0, 1e-6);
}

TEST(Section, CylinderAlongAndAcrossTheAxis) {
    const double r = 10, h = 40;
    const Shape cyl(BRepPrimAPI_MakeCylinder(r, h).Shape());
    auto along = sectionByPlane(cyl, SectionPlane{{0, 0, 0}, {1, 0, 0}});
    ASSERT_TRUE(along.ok()) << along.error().message;
    EXPECT_NEAR(along.value().area, 2 * r * h, 1e-7);
    auto across = sectionByPlane(cyl, SectionPlane{{0, 0, 25}, {0, 0, 1}});
    ASSERT_TRUE(across.ok());
    EXPECT_NEAR(across.value().area, kPi * r * r, 1e-7);
    for (const auto& line : across.value().polylines)
        for (const Point3& p : line) EXPECT_NEAR(std::hypot(p[0], p[1]), r, 1e-7);

    auto half = clipByPlane(cyl, SectionPlane{{0, 0, 0}, {1, 0, 0}});
    ASSERT_TRUE(half.ok());
    EXPECT_NEAR(volumeOf(half.value()), kPi * r * r * h / 2, 1e-6);
}

TEST(Section, HollowCylinderGivesAnnulus) {
    const double ro = 20, ri = 12, h = 30;
    BRepAlgoAPI_Cut tube(BRepPrimAPI_MakeCylinder(ro, h).Shape(), BRepPrimAPI_MakeCylinder(ri, h).Shape());
    ASSERT_TRUE(tube.IsDone());
    auto s = sectionByPlane(Shape(tube.Shape()), SectionPlane{{0, 0, 15}, {0, 0, 1}});
    ASSERT_TRUE(s.ok()) << s.error().message;
    EXPECT_NEAR(s.value().area, kPi * (ro * ro - ri * ri), 1e-6);
    EXPECT_EQ(closedWires(s.value().wires), 2u);
    EXPECT_EQ(countOf(s.value().faces, TopAbs_FACE), 1u) << "one face with a hole";
    for (const auto& line : s.value().polylines)
        for (const Point3& p : line) {
            const double rho = std::hypot(p[0], p[1]);
            EXPECT_TRUE(std::abs(rho - ro) < 1e-7 || std::abs(rho - ri) < 1e-7) << rho;
        }
    // Along the axis: two rectangles (ro - ri) x h.
    auto along = sectionByPlane(Shape(tube.Shape()), SectionPlane{{0, 0, 0}, {0, 1, 0}});
    ASSERT_TRUE(along.ok());
    EXPECT_NEAR(along.value().area, 2 * (ro - ri) * h, 1e-6);
    EXPECT_EQ(countOf(along.value().faces, TopAbs_FACE), 2u);
}

TEST(Section, SphereCapAndCircle) {
    const double r = 20, d = 12;
    const Shape ball(BRepPrimAPI_MakeSphere(r).Shape());
    auto s = sectionByPlane(ball, SectionPlane{{0, 0, d}, {0, 0, 1}});
    ASSERT_TRUE(s.ok());
    EXPECT_NEAR(s.value().area, kPi * (r * r - d * d), 1e-6);
    auto clip = clipByPlane(ball, SectionPlane{{0, 0, d}, {0, 0, 1}});
    ASSERT_TRUE(clip.ok());
    const double cap = kPi * (r - d) * (r - d) * (3 * r - (r - d)) / 3;
    EXPECT_NEAR(volumeOf(clip.value()), 4.0 / 3.0 * kPi * r * r * r - cap, 1e-4);
}

TEST(Section, PlaneMissingTheShapeAndErrors) {
    const Shape box(BRepPrimAPI_MakeBox(10, 10, 10).Shape());
    auto miss = sectionByPlane(box, SectionPlane{{0, 0, 50}, {0, 0, 1}});
    ASSERT_TRUE(miss.ok());
    EXPECT_EQ(miss.value().area, 0.0);
    EXPECT_EQ(countOf(miss.value().edges, TopAbs_EDGE), 0u);
    EXPECT_TRUE(miss.value().polylines.empty());
    // Whole shape on the kept side / on the removed side.
    auto kept = clipByPlane(box, SectionPlane{{0, 0, 50}, {0, 0, 1}});
    ASSERT_TRUE(kept.ok());
    EXPECT_NEAR(volumeOf(kept.value()), 1000.0, 1e-7);
    auto removed = clipByPlane(box, SectionPlane{{0, 0, -5}, {0, 0, 1}});
    ASSERT_TRUE(removed.ok());
    EXPECT_EQ(countOf(removed.value(), TopAbs_SOLID), 0u);

    EXPECT_EQ(sectionByPlane(box, SectionPlane{{0, 0, 0}, {0, 0, 0}}).error().code, ErrorCode::InvalidInput);
    EXPECT_EQ(clipByPlane(Shape(), SectionPlane{}).error().code, ErrorCode::InvalidInput);
    EXPECT_EQ(sectionByPlane(box, SectionPlane{}, 0.0).error().code, ErrorCode::InvalidInput);
}
