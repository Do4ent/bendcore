// Аналитический якобиан каждого вида сопряжения против центральных разностей и обращение
// невязки в ноль на конфигурациях, удовлетворяющих сопряжению по построению.

#include "test_helpers.hpp"

#include <functional>
#include <string>
#include <vector>

using namespace bendcore::assembly_v2;
using namespace bendcore::assembly_v2::testing;

namespace {

struct Case {
    std::string name;
    std::function<Mate(std::mt19937&)> make;
};

Feature F(ComponentId c, const Entity& e) { return {c, e}; }

std::vector<Case> allCases() {
    auto pt = [](std::mt19937& g) { return Entity::makePoint(randomVec(g, 50)); };
    auto ax = [](std::mt19937& g) { return Entity::makeAxis(randomVec(g, 50), randomUnit(g)); };
    auto pl = [](std::mt19937& g) { return Entity::makePlane(randomVec(g, 50), randomUnit(g)); };
    auto cy = [](std::mt19937& g, double r) { return Entity::makeCylinder(randomVec(g, 50), randomUnit(g), r); };
    auto sp = [](std::mt19937& g, double r) { return Entity::makeSphere(randomVec(g, 50), r); };
    return {
        {"coincident point-point", [=](auto& g) { return Mate::coincident(F(0, pt(g)), F(1, sp(g, 3))); }},
        {"coincident point-plane", [=](auto& g) { return Mate::coincident(F(0, pt(g)), F(1, pl(g))); }},
        {"coincident plane-point", [=](auto& g) { return Mate::coincident(F(0, pl(g)), F(1, pt(g))); }},
        {"coincident point-axis", [=](auto& g) { return Mate::coincident(F(0, pt(g)), F(1, ax(g))); }},
        {"coincident plane-plane aligned",
         [=](auto& g) { return Mate::coincident(F(0, pl(g)), F(1, pl(g)), Alignment::Aligned); }},
        {"coincident plane-plane anti",
         [=](auto& g) { return Mate::coincident(F(0, pl(g)), F(1, pl(g)), Alignment::AntiAligned); }},
        {"concentric axis-cylinder", [=](auto& g) { return Mate::concentric(F(0, ax(g)), F(1, cy(g, 4))); }},
        {"concentric cylinder-cylinder anti",
         [=](auto& g) { return Mate::concentric(F(0, cy(g, 4)), F(1, cy(g, 4)), Alignment::AntiAligned); }},
        {"concentric sphere-sphere", [=](auto& g) { return Mate::concentric(F(0, sp(g, 2)), F(1, sp(g, 5))); }},
        {"distance plane-plane", [=](auto& g) { return Mate::distance(F(0, pl(g)), F(1, pl(g)), 7.5); }},
        {"distance point-plane", [=](auto& g) { return Mate::distance(F(0, pt(g)), F(1, pl(g)), -3.0); }},
        {"distance point-point", [=](auto& g) { return Mate::distance(F(0, pt(g)), F(1, pt(g)), 12.0); }},
        {"angle", [=](auto& g) { return Mate::angle(F(0, ax(g)), F(1, pl(g)), 0.7); }},
        {"parallel", [=](auto& g) { return Mate::parallel(F(0, cy(g, 2)), F(1, ax(g)), Alignment::AntiAligned); }},
        {"perpendicular", [=](auto& g) { return Mate::perpendicular(F(0, pl(g)), F(1, ax(g))); }},
        {"tangent plane-cylinder", [=](auto& g) { return Mate::tangent(F(0, pl(g)), F(1, cy(g, 6))); }},
        {"tangent cylinder-plane", [=](auto& g) { return Mate::tangent(F(0, cy(g, 6)), F(1, pl(g))); }},
        {"tangent cylinder-cylinder external",
         [=](auto& g) { return Mate::tangent(F(0, cy(g, 6)), F(1, cy(g, 9))); }},
        {"tangent cylinder-cylinder internal",
         [=](auto& g) { return Mate::tangent(F(0, cy(g, 6)), F(1, cy(g, 15)), Alignment::Closest, true); }},
        {"tangent plane-sphere", [=](auto& g) { return Mate::tangent(F(0, pl(g)), F(1, sp(g, 8))); }},
        {"tangent sphere-plane", [=](auto& g) { return Mate::tangent(F(0, sp(g, 8)), F(1, pl(g))); }},
        {"rigid", [=](auto& g) { return Mate::rigid(0, 1, randomPose(g, 40)); }},
    };
}

TEST(AssemblyV2Jacobian, MatchesCentralDifferencesForAllMateTypes) {
    std::mt19937 rng(20261106);
    const double h = 1e-6;
    for (const Case& c : allCases()) {
        for (int trial = 0; trial < 20; ++trial) {
            auto prepared = prepareMate(c.make(rng));
            ASSERT_TRUE(prepared.ok()) << c.name << ": " << prepared.error().message;
            const Transform pa = randomPose(rng, 100);
            const Transform pb = randomPose(rng, 100);
            // Ориентацию фиксируем до разностей, чтобы Closest не переключился внутри шага.
            const Mate m = resolveAlignment(prepared.value(), pa, pb);
            auto ev = evaluateMate(m, pa, pb);
            ASSERT_TRUE(ev.ok());
            const int rows = equationCount(m.form);
            ASSERT_EQ(ev.value().residual.size(), rows) << c.name;

            for (int side = 0; side < 2; ++side) {
                const auto& analytic = side == 0 ? ev.value().jacobianA : ev.value().jacobianB;
                for (int j = 0; j < 6; ++j) {
                    Eigen::Matrix<double, 6, 1> dx = Eigen::Matrix<double, 6, 1>::Zero();
                    dx(j) = h;
                    const Transform ap = side == 0 ? applyIncrement(pa, dx) : pa;
                    const Transform bp = side == 1 ? applyIncrement(pb, dx) : pb;
                    const Transform am = side == 0 ? applyIncrement(pa, -dx) : pa;
                    const Transform bm = side == 1 ? applyIncrement(pb, -dx) : pb;
                    const Eigen::VectorXd fd =
                        (evaluateMate(m, ap, bp).value().residual - evaluateMate(m, am, bm).value().residual) /
                        (2.0 * h);
                    const double scale = 1.0 + analytic.col(j).cwiseAbs().maxCoeff();
                    const double err = (fd - analytic.col(j)).cwiseAbs().maxCoeff();
                    EXPECT_LT(err, 1e-6 * scale) << c.name << " side " << side << " var " << j;
                }
            }
        }
    }
}

TEST(AssemblyV2Jacobian, SameComponentMateIsTranslationInvariant) {
    // Сдвиг тела не меняет ни разность его точек, ни его направления: столбцы сдвига Ja + Jb = 0.
    std::mt19937 rng(7);
    for (const Case& c : allCases()) {
        auto prepared = prepareMate(c.make(rng));
        ASSERT_TRUE(prepared.ok());
        Mate m = prepared.value();
        if (m.form == MateForm::Rigid) continue;  // жёсткая связь тела с собой запрещена
        m.b.component = m.a.component;
        const Transform p = randomPose(rng, 100);
        auto ev = evaluateMate(m, p, p);
        ASSERT_TRUE(ev.ok());
        EXPECT_LT((ev.value().jacobianA + ev.value().jacobianB).leftCols<3>().cwiseAbs().maxCoeff(), 1e-12) << c.name;
    }
}

// Конфигурации, удовлетворяющие сопряжению по построению, в случайной общей СК.
TEST(AssemblyV2Jacobian, ResidualVanishesOnSatisfiedConfigurations) {
    std::mt19937 rng(99);
    for (int trial = 0; trial < 10; ++trial) {
        const Transform pa = randomPose(rng, 100);
        const Transform pb = randomPose(rng, 100);
        // Мировые сущности строим в СК, сдвинутой случайным преобразованием W.
        const Transform W = randomPose(rng, 50);
        auto wp = [&](const Vec3& p) { return W.applyToPoint(p); };
        auto wd = [&](const Vec3& d) { return W.applyToDirection(d); };
        auto check = [&](const char* name, const Entity& worldA, const Entity& worldB, Mate proto, double expected) {
            proto.a = {0, toLocal(worldA, pa)};
            proto.b = {1, toLocal(worldB, pb)};
            auto prepared = prepareMate(proto);
            ASSERT_TRUE(prepared.ok()) << name << ": " << prepared.error().message;
            auto ev = evaluateMate(prepared.value(), pa, pb);
            ASSERT_TRUE(ev.ok());
            EXPECT_NEAR(ev.value().residual.cwiseAbs().maxCoeff(), expected, 1e-9) << name;
        };
        const Vec3 o = wp(Vec3(1, 2, 3));
        const Vec3 z = wd(Vec3::UnitZ());
        const Vec3 x = wd(Vec3::UnitX());
        // Точка на плоскости, на оси; плоскости встречные со сдвигом; соосность.
        check("point-plane", Entity::makePoint(wp(Vec3(5, -7, 0))), Entity::makePlane(wp(Vec3::Zero()), z),
              Mate::coincident({}, {}), 0.0);
        check("point-axis", Entity::makePoint(wp(Vec3(0, 0, 42))), Entity::makeAxis(wp(Vec3::Zero()), z),
              Mate::coincident({}, {}), 0.0);
        check("plane-plane distance", Entity::makePlane(wp(Vec3(3, 4, 12.5)), -z),
              Entity::makePlane(wp(Vec3::Zero()), z), Mate::distance({}, {}, 12.5, Alignment::AntiAligned), 0.0);
        check("concentric", Entity::makeCylinder(wp(Vec3(0, 0, -9)), -z, 4), Entity::makeAxis(wp(Vec3::Zero()), z),
              Mate::concentric({}, {}), 0.0);
        check("point-point distance", Entity::makePoint(o), Entity::makePoint(o + wd(Vec3(3, 4, 0))),
              Mate::distance({}, {}, 5.0), 0.0);
        check("angle 30", Entity::makeAxis(o, x), Entity::makeAxis(o, wd(Vec3(std::cos(deg(30)), std::sin(deg(30)), 0))),
              Mate::angle({}, {}, deg(30)), 0.0);
        // Плоскость z = 0 и цилиндр радиуса 6, ось на высоте 6, параллельна плоскости.
        check("tangent plane-cylinder", Entity::makePlane(wp(Vec3::Zero()), z),
              Entity::makeCylinder(wp(Vec3(0, 0, 6)), x, 6), Mate::tangent({}, {}), 0.0);
        check("tangent cylinder-cylinder", Entity::makeCylinder(wp(Vec3::Zero()), z, 6),
              Entity::makeCylinder(wp(Vec3(9, 12, 0)), z, 9), Mate::tangent({}, {}), 0.0);  // 6 + 9 = 15
        check("tangent internal", Entity::makeCylinder(wp(Vec3::Zero()), z, 20),
              Entity::makeCylinder(wp(Vec3(0, 12, 7)), -z, 8), Mate::tangent({}, {}, Alignment::Closest, true), 0.0);
        check("tangent plane-sphere below", Entity::makePlane(wp(Vec3::Zero()), z),
              Entity::makeSphere(wp(Vec3(4, 4, -8)), 8), Mate::tangent({}, {}, Alignment::AntiAligned), 0.0);
        // Известная невязка: точка в 3 мм над плоскостью при совпадении.
        check("point-plane offset", Entity::makePoint(wp(Vec3(0, 0, 3))), Entity::makePlane(wp(Vec3::Zero()), z),
              Mate::coincident({}, {}), 3.0);
        // Сфера касается снизу, а требуется сверху: невязка 2ρ = 16.
        check("tangent plane-sphere wrong side", Entity::makePlane(wp(Vec3::Zero()), z),
              Entity::makeSphere(wp(Vec3(4, 4, -8)), 8), Mate::tangent({}, {}, Alignment::Aligned), 16.0);
    }
}

TEST(AssemblyV2Jacobian, RigidResidualVanishesAtCapturedRelativePose) {
    std::mt19937 rng(3);
    const Transform pa = randomPose(rng, 100);
    const Transform pb = randomPose(rng, 100);
    auto prepared = prepareMate(Mate::rigid(0, 1, pa.inverse() * pb));
    ASSERT_TRUE(prepared.ok());
    EXPECT_LT(evaluateMate(prepared.value(), pa, pb).value().residual.norm(), 1e-12);
}

TEST(AssemblyV2Jacobian, InvalidMatesAreRejectedWithReason) {
    const Feature point{0, Entity::makePoint(Vec3::Zero())};
    const Feature plane{1, Entity::makePlane(Vec3::Zero(), Vec3::UnitZ())};
    const Feature axis{1, Entity::makeAxis(Vec3::Zero(), Vec3::UnitX())};
    const Feature cylZeroR{1, Entity::makeCylinder(Vec3::Zero(), Vec3::UnitZ(), 0.0)};

    auto angle = prepareMate(Mate::angle(point, plane, 0.5));
    ASSERT_FALSE(angle.ok());
    EXPECT_EQ(angle.error().code, bendcore::kernel::ErrorCode::InvalidInput);

    auto tangent = prepareMate(Mate::tangent(point, plane));
    ASSERT_FALSE(tangent.ok());
    EXPECT_EQ(tangent.error().code, bendcore::kernel::ErrorCode::Unsupported);

    EXPECT_FALSE(prepareMate(Mate::distance(point, Feature{1, Entity::makePoint(Vec3::Ones())}, 0.0)).ok());
    EXPECT_FALSE(prepareMate(Mate::angle(axis, plane, 0.0)).ok());
    EXPECT_FALSE(prepareMate(Mate::concentric(axis, cylZeroR)).ok());
    EXPECT_FALSE(prepareMate(Mate::rigid(2, 2)).ok());
    Feature zeroDir{1, Entity::makeAxis(Vec3::Zero(), Vec3::UnitX())};
    zeroDir.entity.direction = Vec3::Zero();
    EXPECT_FALSE(prepareMate(Mate::parallel(axis, zeroDir)).ok());

    Mate unprepared = Mate::coincident(point, plane);
    EXPECT_FALSE(evaluateMate(unprepared, Transform::identity(), Transform::identity()).ok());

    Assembly a;
    a.addComponent("only", Transform::identity());
    EXPECT_FALSE(a.addMate(Mate::coincident(point, plane)).ok());  // компонента 1 нет
}

}  // namespace
