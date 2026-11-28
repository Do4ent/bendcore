// По сечениям (loft): усечённая пирамида и усечённый конус против аналитики, три сечения,
// сечения с отверстиями, история, вырез, ошибки.

#include "features_test_util.hpp"

#include <gtest/gtest.h>

using namespace ft;

namespace {

constexpr double kPi = 3.14159265358979323846;

Profile square(double a, double z) { return rect(-a / 2, -a / 2, a / 2, a / 2, z); }

Profile disk(double r, double z, double holeR = 0.0) {
    const gp_Ax2 ax(gp_Pnt(0, 0, z), gp::DZ(), gp::DX());
    BRepBuilderAPI_MakeFace mf(gp_Pln(gp_Ax3(ax)),
                               BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(gp_Circ(ax, r)).Edge()).Wire(), true);
    if (holeR > 0) {
        TopoDS_Wire h = BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(gp_Circ(ax, holeR)).Edge()).Wire();
        h.Reverse();
        mf.Add(h);
    }
    return Profile{mf.Face(), gp_Ax3(ax)};
}

double frustum(double a1, double a2, double h) { return h / 3 * (a1 + a2 + std::sqrt(a1 * a2)); }

}  // namespace

TEST(FeaturesLoft, SquareToSquareIsFrustumOfPyramid) {
    const double a = 20, b = 8, h = 15;
    for (bool ruled : {true, false}) {
        LoftParams p;
        p.ruled = ruled;
        auto res = loft({square(a, 0), square(b, h)}, {}, p);
        ASSERT_TRUE(res.ok()) << res.error().message;
        EXPECT_TRUE(valid(res.value().shape));
        EXPECT_EQ(count(res.value().shape, TopAbs_FACE), 6);
        const double v = frustum(a * a, b * b, h);
        EXPECT_NEAR(volume(res.value().shape), v, (ruled ? 1e-7 : 1e-4) * v) << "ruled=" << ruled;
    }
}

TEST(FeaturesLoft, CircleToCircleIsFrustumOfCone) {
    const double R = 5, r = 2, h = 10;
    auto res = loft({disk(R, 0), disk(r, h)}, {}, LoftParams{FeatureMode::Boss, true, false});
    ASSERT_TRUE(res.ok()) << res.error().message;
    const double v = kPi * h / 3 * (R * R + R * r + r * r);
    EXPECT_NEAR(volume(res.value().shape), v, 1e-6 * v);
    // Три сечения на одной образующей конуса: гладкое тело совпадает с конусом.
    auto three = loft({disk(R, 0), disk((R + r) / 2, h / 2), disk(r, h)}, {}, LoftParams{});
    ASSERT_TRUE(three.ok()) << three.error().message;
    EXPECT_TRUE(valid(three.value().shape));
    EXPECT_NEAR(volume(three.value().shape), v, 1e-4 * v);
}

TEST(FeaturesLoft, HistoryOfSectionsAndCaps) {
    const Profile s0 = square(10, 0), s1 = square(6, 8);
    auto res = loft({s0, s1}, {}, LoftParams{FeatureMode::Boss, true, false});
    ASSERT_TRUE(res.ok()) << res.error().message;
    const auto& h = res.value().history;
    ASSERT_EQ(h->Modified(s0.face).Extent(), 1);
    EXPECT_NEAR(faceCentroid(h->Modified(s0.face).First()).Z(), 0.0, 1e-9);
    bool top = false;
    for (const TopoDS_Shape& g : h->Generated(s1.face))
        if (std::abs(faceCentroid(g).Z() - 8.0) < 1e-9) top = true;
    EXPECT_TRUE(top);
    // Каждое ребро нижнего сечения порождает боковую грань.
    int lateral = 0;
    for (TopExp_Explorer ex(s0.face, TopAbs_EDGE); ex.More(); ex.Next())
        for (const TopoDS_Shape& g : h->Generated(ex.Current()))
            if (g.ShapeType() == TopAbs_FACE) ++lateral;
    EXPECT_EQ(lateral, 4);
}

TEST(FeaturesLoft, SectionsWithHolesAndCut) {
    const double R = 6, r = 4, hole = 2, h = 10;
    auto res = loft({disk(R, 0, hole), disk(r, h, hole)}, {}, LoftParams{FeatureMode::Boss, true, false});
    ASSERT_TRUE(res.ok()) << res.error().message;
    const double v = kPi * h / 3 * (R * R + R * r + r * r) - kPi * hole * hole * h;
    EXPECT_NEAR(volume(res.value().shape), v, 1e-6 * v);

    // Вырез усечённой пирамиды из коробки.
    const TopoDS_Shape b = box(30, 30, 20);
    LoftParams cut{FeatureMode::Cut, true, false};
    gp_Trsf t;
    t.SetTranslation(gp_Vec(15, 15, 0));
    Profile p0 = square(10, 10), p1 = square(4, 20);
    p0.face = TopoDS::Face(p0.face.Moved(TopLoc_Location(t)));
    p1.face = TopoDS::Face(p1.face.Moved(TopLoc_Location(t)));
    p0.placement.Translate(gp_Vec(15, 15, 0));
    p1.placement.Translate(gp_Vec(15, 15, 0));
    auto pocket = loft({p0, p1}, b, cut);
    ASSERT_TRUE(pocket.ok()) << pocket.error().message;
    EXPECT_NEAR(volume(pocket.value().shape), 18000.0 - frustum(100, 16, 10), 1e-6 * 18000);
}

TEST(FeaturesLoft, Errors) {
    auto one = loft({square(5, 0)}, {}, LoftParams{});
    ASSERT_FALSE(one.ok());
    EXPECT_EQ(one.error().code, ErrorCode::InvalidInput);
    auto same = loft({square(5, 0), square(3, 0)}, {}, LoftParams{});
    ASSERT_FALSE(same.ok());
    EXPECT_NE(same.error().message.find("same plane"), std::string::npos) << same.error().message;
    auto contours = loft({disk(5, 0, 1), disk(3, 5)}, {}, LoftParams{});
    ASSERT_FALSE(contours.ok());
    EXPECT_NE(contours.error().message.find("contours"), std::string::npos) << contours.error().message;
    LoftParams closed;
    closed.closed = true;
    auto twoClosed = loft({square(5, 0), square(3, 5)}, {}, closed);
    ASSERT_FALSE(twoClosed.ok());
    LoftParams cut;
    cut.mode = FeatureMode::Cut;
    auto noTarget = loft({square(5, 0), square(3, 5)}, {}, cut);
    ASSERT_FALSE(noTarget.ok());
}
