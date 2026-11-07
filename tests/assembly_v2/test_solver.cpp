// Решатель на эталонных механизмах: вал в отверстии, плита с двумя болтами, кривошипно-шатунный
// механизм, касания, жёсткая связь; диагностика степеней свободы, избыточности и конфликтов;
// перетаскивание; сходимость из случайных возмущений.

#include "test_helpers.hpp"

#include <algorithm>
#include <vector>

using namespace bendcore::assembly_v2;
using namespace bendcore::assembly_v2::testing;

namespace {

const SubsystemReport* reportFor(const SolveResult& r, ComponentId c) {
    for (const auto& s : r.subsystems) {
        if (std::find(s.components.begin(), s.components.end(), c) != s.components.end()) return &s;
    }
    return nullptr;
}

int sumIndependent(const SubsystemReport& s) {
    int n = 0;
    for (const auto& d : s.mateDiagnostics) n += d.independent;
    return n;
}

const MateDiagnostic* diagFor(const SubsystemReport& s, MateId id) {
    for (const auto& d : s.mateDiagnostics) {
        if (d.id == id) return &d;
    }
    return nullptr;
}

std::vector<MateId> sorted(std::vector<MateId> v) {
    std::sort(v.begin(), v.end());
    return v;
}

// Угол поворота вокруг мировой оси z по образу локальной оси x.
double yawOf(const Transform& t) {
    const Vec3 x = t.applyToDirection(Vec3::UnitX());
    return std::atan2(x.y(), x.x());
}

// ---------------------------------------------------------------------------------------------
// Вал в отверстии

struct ShaftInHole {
    Assembly a;
    ComponentId housing = 0, shaft = 0;
    Feature hole, top, housingX, shaftCyl, shoulder, shaftX;
    MateId concentric = 0, faces = 0;

    explicit ShaftInHole(const Transform& shaftStart, const Transform& housingPose = Transform::identity()) {
        housing = a.addComponent("housing", housingPose, true);
        shaft = a.addComponent("shaft", shaftStart);
        hole = {housing, Entity::makeCylinder(Vec3::Zero(), Vec3::UnitZ(), 10)};
        top = {housing, Entity::makePlane(Vec3::Zero(), Vec3::UnitZ())};
        housingX = {housing, Entity::makeAxis(Vec3::Zero(), Vec3::UnitX())};
        shaftCyl = {shaft, Entity::makeCylinder(Vec3(0, 0, -40), Vec3::UnitZ(), 10)};
        shoulder = {shaft, Entity::makePlane(Vec3(0, 0, 0), -Vec3::UnitZ())};
        shaftX = {shaft, Entity::makeAxis(Vec3::Zero(), Vec3::UnitX())};
        concentric = addOk(a, Mate::concentric(hole, shaftCyl));
        faces = addOk(a, Mate::coincident(top, shoulder, Alignment::AntiAligned));
    }
};

const Transform kShaftStart = Transform::fromAxisAngle(Vec3(1, 1, 0.3), 0.25, Vec3(3, -2, 5));

TEST(AssemblyV2Solver, ShaftInHoleLeavesOneRotationalDof) {
    ShaftInHole s(kShaftStart);
    const SolveResult r = solve(s.a);
    ASSERT_EQ(r.subsystems.size(), 1u);
    const SubsystemReport& sub = r.subsystems[0];
    EXPECT_EQ(sub.status, SubsystemStatus::Solved);
    EXPECT_LT(sub.residualNorm, 1e-9);
    EXPECT_EQ(sub.dof, 1);
    EXPECT_EQ(r.totalDof, 1);
    EXPECT_EQ(sumIndependent(sub), sub.rank);
    EXPECT_TRUE(sub.redundantMates.empty());
    EXPECT_TRUE(sub.conflictingMates.empty());
    // Соосность даёт 4 независимых уравнения, совпадение торцов поверх неё — одно (осевой сдвиг).
    EXPECT_EQ(diagFor(sub, s.concentric)->independent, 4);
    EXPECT_EQ(diagFor(sub, s.faces)->independent, 1);
    EXPECT_EQ(diagFor(sub, s.faces)->rank, 3);
    // Аналитика: ось вала совпадает с осью z, начало вала — на торце z = 0.
    const Transform& p = r.poses[s.shaft];
    EXPECT_LT((p.applyToDirection(Vec3::UnitZ()) - Vec3::UnitZ()).norm(), 1e-9);
    EXPECT_LT(p.position.norm(), 1e-9);
    // Фиксированный корпус не сдвинулся.
    EXPECT_LT(r.poses[s.housing].position.norm(), 1e-15);
}

TEST(AssemblyV2Solver, ShaftInHoleWithAngleIsFullyConstrained) {
    ShaftInHole s(kShaftStart);
    const MateId angle = addOk(s.a, Mate::angle(s.housingX, s.shaftX, deg(30)));
    const SolveResult r = solve(s.a);
    const SubsystemReport& sub = r.subsystems[0];
    EXPECT_EQ(sub.status, SubsystemStatus::Solved);
    EXPECT_EQ(sub.dof, 0);
    EXPECT_EQ(diagFor(sub, angle)->independent, 1);
    const Transform& p = r.poses[s.shaft];
    EXPECT_LT((p.applyToDirection(Vec3::UnitZ()) - Vec3::UnitZ()).norm(), 1e-9);
    EXPECT_LT(p.position.norm(), 1e-9);
    EXPECT_NEAR(std::abs(yawOf(p)), deg(30), 1e-9);

    // Применение результата к сборке и повторное решение: 0 итераций, положение то же.
    ASSERT_TRUE(s.a.applyPoses(r).ok());
    const SolveResult again = solve(s.a);
    EXPECT_EQ(again.totalIterations, 0);
    EXPECT_LT((again.poses[s.shaft].position - p.position).norm(), 1e-15);
}

TEST(AssemblyV2Solver, ShaftInRotatedHousingFollowsHousingFrame) {
    // Корпус в произвольном положении: решение — в его СК.
    const Transform housingPose = Transform::fromAxisAngle(Vec3(0.2, -1, 0.5), 1.1, Vec3(100, 50, -20));
    ShaftInHole s(housingPose * Transform::fromAxisAngle(Vec3(1, 0, 0), 0.2, Vec3(2, 1, 4)), housingPose);
    addOk(s.a, Mate::angle(s.housingX, s.shaftX, deg(45)));
    const SolveResult r = solve(s.a);
    ASSERT_TRUE(r.success());
    const Transform rel = housingPose.inverse() * r.poses[s.shaft];
    EXPECT_LT(rel.position.norm(), 1e-9);
    EXPECT_LT((rel.applyToDirection(Vec3::UnitZ()) - Vec3::UnitZ()).norm(), 1e-9);
    EXPECT_NEAR(std::abs(yawOf(rel)), deg(45), 1e-9);
}

// ---------------------------------------------------------------------------------------------
// Плита с двумя болтами

struct PlateWithBolts {
    Assembly a;
    ComponentId base = 0, plate = 0, bolt1 = 0, bolt2 = 0;
    // Решение: плита повёрнута на 90° вокруг z и стоит в (20, 30, 0), толщина 10.
    Transform plateSolution = Transform::fromAxisAngle(Vec3::UnitZ(), deg(90), Vec3(20, 30, 0));
    std::vector<MateId> mates;

    explicit PlateWithBolts(std::mt19937* rng = nullptr, double shift = 0.0, double angle = 0.0) {
        auto start = [&](const Transform& t) { return rng ? perturb(t, *rng, shift, angle) : t; };
        base = a.addComponent("base", Transform::identity(), true);
        plate = a.addComponent("plate", start(plateSolution));
        bolt1 = a.addComponent("bolt1", start(Transform::fromTranslation(Vec3(0, 0, 10))));
        bolt2 = a.addComponent("bolt2", start(Transform::fromTranslation(Vec3(100, 0, 10))));
        const Vec3 h1(0, 0, 0), h2(100, 0, 0), z = Vec3::UnitZ();
        const Feature baseTop{base, Entity::makePlane(Vec3::Zero(), z)};
        const Feature baseHole1{base, Entity::makeCylinder(h1, z, 6)};
        const Feature baseHole2{base, Entity::makeCylinder(h2, z, 6)};
        const Feature plateBottom{plate, toLocal(Entity::makePlane(Vec3(50, 0, 0), -z), plateSolution)};
        const Feature plateTop{plate, toLocal(Entity::makePlane(Vec3(50, 0, 10), z), plateSolution)};
        const Feature plateHole1{plate, toLocal(Entity::makeCylinder(h1, z, 6), plateSolution)};
        const Feature plateHole2{plate, toLocal(Entity::makeCylinder(h2, z, 6), plateSolution)};
        auto shank = [](ComponentId b) { return Feature{b, Entity::makeCylinder(Vec3(0, 0, -15), -Vec3::UnitZ(), 6)}; };
        auto head = [](ComponentId b) { return Feature{b, Entity::makePlane(Vec3::Zero(), -Vec3::UnitZ())}; };
        mates.push_back(addOk(a, Mate::coincident(plateBottom, baseTop, Alignment::AntiAligned)));
        mates.push_back(addOk(a, Mate::concentric(baseHole1, shank(bolt1))));
        mates.push_back(addOk(a, Mate::concentric(plateHole1, shank(bolt1))));
        mates.push_back(addOk(a, Mate::coincident(plateTop, head(bolt1), Alignment::AntiAligned)));
        mates.push_back(addOk(a, Mate::concentric(baseHole2, shank(bolt2))));
        mates.push_back(addOk(a, Mate::concentric(plateHole2, shank(bolt2))));
        mates.push_back(addOk(a, Mate::coincident(plateTop, head(bolt2), Alignment::AntiAligned)));
    }

    void expectSolved(const SolveResult& r) const {
        ASSERT_TRUE(r.success());
        EXPECT_LT((r.poses[plate].position - plateSolution.position).norm(), 1e-8);
        EXPECT_LT(rotationDistance(r.poses[plate], plateSolution), 1e-8);
        EXPECT_LT((r.poses[bolt1].position - Vec3(0, 0, 10)).norm(), 1e-8);
        EXPECT_LT((r.poses[bolt2].position - Vec3(100, 0, 10)).norm(), 1e-8);
        EXPECT_NEAR(std::abs(r.poses[bolt1].applyToDirection(Vec3::UnitZ()).z()), 1.0, 1e-9);
        EXPECT_NEAR(std::abs(r.poses[bolt2].applyToDirection(Vec3::UnitZ()).z()), 1.0, 1e-9);
    }
};

TEST(AssemblyV2Solver, PlateWithTwoBolts) {
    std::mt19937 rng(11);
    PlateWithBolts p(&rng, 4.0, 0.2);
    const SolveResult r = solve(p.a);
    p.expectSolved(r);
    ASSERT_EQ(r.subsystems.size(), 1u);
    const SubsystemReport& sub = r.subsystems[0];
    // Плита определена полностью; каждый болт может вращаться вокруг своей оси.
    EXPECT_EQ(sub.dof, 2);
    EXPECT_EQ(sumIndependent(sub), sub.rank);
    EXPECT_TRUE(sub.redundantMates.empty());
    const std::vector<int> expectedIndependent = {3, 4, 2, 1, 4, 1, 1};
    for (std::size_t i = 0; i < p.mates.size(); ++i) {
        EXPECT_EQ(diagFor(sub, p.mates[i])->independent, expectedIndependent[i]) << "mate " << i;
    }
}

// ---------------------------------------------------------------------------------------------
// Кривошипно-шатунный механизм: кривошип r = 30, шатун l = 100, ползун на оси x.

struct CrankSlider {
    static constexpr double r = 30.0;
    static constexpr double l = 100.0;
    Assembly a;
    ComponentId ground = 0, crank = 0, rod = 0, slider = 0;
    Feature groundX, crankX;
    std::vector<MateId> mates;

    static double sliderX(double theta) {
        const double s = r * std::sin(theta);
        return r * std::cos(theta) + std::sqrt(l * l - s * s);
    }
    // Положения звеньев при угле кривошипа theta (аналитика).
    static Transform crankPose(double theta) { return Transform::fromAxisAngle(Vec3::UnitZ(), theta); }
    static Transform rodPose(double theta) {
        const Vec3 pin(r * std::cos(theta), r * std::sin(theta), 0);
        const double phi = std::atan2(-pin.y(), sliderX(theta) - pin.x());
        return Transform::fromAxisAngle(Vec3::UnitZ(), phi, pin);
    }
    static Transform sliderPose(double theta) { return Transform::fromTranslation(Vec3(sliderX(theta), 0, 0)); }

    CrankSlider(double thetaStart, std::mt19937& rng, double shift, double angle) {
        const Vec3 z = Vec3::UnitZ();
        ground = a.addComponent("ground", Transform::identity(), true);
        crank = a.addComponent("crank", perturb(crankPose(thetaStart), rng, shift, angle));
        rod = a.addComponent("rod", perturb(rodPose(thetaStart), rng, shift, angle));
        slider = a.addComponent("slider", perturb(sliderPose(thetaStart), rng, shift, angle));
        const Feature groundPivot{ground, Entity::makeAxis(Vec3::Zero(), z)};
        const Feature groundPlane{ground, Entity::makePlane(Vec3::Zero(), z)};
        const Feature rail{ground, Entity::makePlane(Vec3::Zero(), Vec3::UnitY())};
        groundX = {ground, Entity::makeAxis(Vec3::Zero(), Vec3::UnitX())};
        const Feature crankHole{crank, Entity::makeCylinder(Vec3::Zero(), z, 8)};
        const Feature crankFace{crank, Entity::makePlane(Vec3::Zero(), z)};
        const Feature crankPin{crank, Entity::makeCylinder(Vec3(r, 0, 0), z, 5)};
        crankX = {crank, Entity::makeAxis(Vec3::Zero(), Vec3::UnitX())};
        const Feature rodHole1{rod, Entity::makeCylinder(Vec3::Zero(), z, 5)};
        const Feature rodHole2{rod, Entity::makeCylinder(Vec3(l, 0, 0), z, 5)};
        const Feature rodFace{rod, Entity::makePlane(Vec3::Zero(), z)};
        const Feature sliderPin{slider, Entity::makeCylinder(Vec3::Zero(), z, 5)};
        const Feature sliderSide{slider, Entity::makePlane(Vec3::Zero(), Vec3::UnitY())};
        const Feature sliderBottom{slider, Entity::makePlane(Vec3::Zero(), z)};
        mates.push_back(addOk(a, Mate::concentric(groundPivot, crankHole)));
        mates.push_back(addOk(a, Mate::coincident(groundPlane, crankFace, Alignment::Aligned)));
        mates.push_back(addOk(a, Mate::concentric(crankPin, rodHole1)));
        mates.push_back(addOk(a, Mate::coincident(crankFace, rodFace, Alignment::Aligned)));
        mates.push_back(addOk(a, Mate::concentric(rodHole2, sliderPin)));
        mates.push_back(addOk(a, Mate::coincident(rail, sliderSide, Alignment::Aligned)));
        mates.push_back(addOk(a, Mate::coincident(groundPlane, sliderBottom, Alignment::Aligned)));
    }

    // Положения согласованы с аналитикой при угле кривошипа из решения.
    void expectKinematics(const SolveResult& res, double tol) const {
        const double theta = yawOf(res.poses[crank]);
        EXPECT_NEAR(res.poses[slider].position.x(), sliderX(theta), tol);
        EXPECT_NEAR(res.poses[slider].position.y(), 0.0, tol);
        EXPECT_NEAR(res.poses[slider].position.z(), 0.0, tol);
        EXPECT_LT(res.poses[crank].position.norm(), tol);
        const Transform rp = rodPose(theta);
        EXPECT_LT((res.poses[rod].position - rp.position).norm(), tol);
        EXPECT_NEAR(std::remainder(yawOf(res.poses[rod]) - yawOf(rp), 2 * kPi), 0.0, tol);
    }
};

TEST(AssemblyV2Solver, CrankSliderHasOneDof) {
    std::mt19937 rng(5);
    CrankSlider m(deg(50), rng, 2.0, 0.05);
    const SolveResult res = solve(m.a);
    ASSERT_TRUE(res.success());
    ASSERT_EQ(res.subsystems.size(), 1u);
    EXPECT_EQ(res.subsystems[0].dof, 1);
    EXPECT_EQ(sumIndependent(res.subsystems[0]), res.subsystems[0].rank);
    EXPECT_TRUE(res.subsystems[0].redundantMates.empty());
    m.expectKinematics(res, 1e-8);
    // Решение остаётся рядом с исходной сборкой (угол около 50°).
    EXPECT_NEAR(yawOf(res.poses[m.crank]), deg(50), 0.2);
}

TEST(AssemblyV2Solver, CrankSliderDrivenByAngleMatchesAnalyticPositions) {
    for (double thetaDeg : {30.0, 75.0, 120.0, 160.0}) {
        std::mt19937 rng(static_cast<unsigned>(thetaDeg));
        const double theta = deg(thetaDeg);
        CrankSlider m(theta + 0.12, rng, 1.5, 0.04);
        addOk(m.a, Mate::angle(m.groundX, m.crankX, theta));
        const SolveResult res = solve(m.a);
        ASSERT_TRUE(res.success()) << thetaDeg;
        EXPECT_EQ(res.subsystems[0].dof, 0) << thetaDeg;
        EXPECT_NEAR(yawOf(res.poses[m.crank]), theta, 1e-9) << thetaDeg;
        EXPECT_NEAR(res.poses[m.slider].position.x(), CrankSlider::sliderX(theta), 1e-8) << thetaDeg;
        const Transform rp = CrankSlider::rodPose(theta);
        EXPECT_LT((res.poses[m.rod].position - rp.position).norm(), 1e-8) << thetaDeg;
        EXPECT_NEAR(std::remainder(yawOf(res.poses[m.rod]) - yawOf(rp), 2 * kPi), 0.0, 1e-9) << thetaDeg;
    }
}

TEST(AssemblyV2Solver, DragCrankPinTowardsTarget) {
    std::mt19937 rng(17);
    CrankSlider m(deg(60), rng, 0.0, 0.0);
    // Цель вне траектории пальца: ближайшая допустимая точка — на луче к цели.
    DragGoal goal;
    goal.component = m.crank;
    goal.localPoint = Vec3(CrankSlider::r, 0, 0);
    goal.target = Vec3(-50, 80, 0);
    auto res = drag(m.a, goal);
    ASSERT_TRUE(res.ok());
    ASSERT_TRUE(res.value().success());
    EXPECT_LT(res.value().residualNorm, 1e-9);
    EXPECT_NEAR(yawOf(res.value().poses[m.crank]), std::atan2(80.0, -50.0), 1e-6);
    m.expectKinematics(res.value(), 1e-8);
    EXPECT_EQ(res.value().subsystems[0].dof, 1);

    // Мягкая цель не перебивает жёсткие сопряжения: цель над плоскостью z = 0 — палец остаётся в ней.
    goal.target = Vec3(0, -30, 25);
    auto lifted = drag(m.a, goal);
    ASSERT_TRUE(lifted.ok());
    ASSERT_TRUE(lifted.value().success());
    EXPECT_NEAR(yawOf(lifted.value().poses[m.crank]), -kPi / 2, 1e-6);
    EXPECT_NEAR(lifted.value().poses[m.crank].applyToPoint(goal.localPoint).z(), 0.0, 1e-9);

    goal.component = m.ground;
    EXPECT_FALSE(drag(m.a, goal).ok());
}

// ---------------------------------------------------------------------------------------------
// Касания и жёсткая связь

TEST(AssemblyV2Solver, TangentMatesMatchAnalyticDistances) {
    Assembly a;
    const ComponentId floor = a.addComponent("floor", Transform::identity(), true);
    const ComponentId roller = a.addComponent("roller", Transform::fromAxisAngle(Vec3(0, 1, 1), 0.3, Vec3(5, 5, 30)));
    const ComponentId ball = a.addComponent("ball", Transform::fromTranslation(Vec3(40, 0, -20)));
    const ComponentId gear = a.addComponent("gear", Transform::fromTranslation(Vec3(0, 50, 2)), true);
    const ComponentId pinion = a.addComponent("pinion", Transform::fromTranslation(Vec3(25, 60, 5)));
    const ComponentId ring = a.addComponent("ring", Transform::fromTranslation(Vec3(-150, 0, 0)), true);
    const ComponentId planet = a.addComponent("planet", Transform::fromTranslation(Vec3(-150, 25, 4)));
    const Feature floorPlane{floor, Entity::makePlane(Vec3::Zero(), Vec3::UnitZ())};
    const Feature rollerCyl{roller, Entity::makeCylinder(Vec3::Zero(), Vec3::UnitX(), 15)};
    const Feature ballSphere{ball, Entity::makeSphere(Vec3::Zero(), 7)};
    addOk(a, Mate::tangent(floorPlane, rollerCyl, Alignment::Aligned));
    addOk(a, Mate::tangent(ballSphere, floorPlane, Alignment::AntiAligned));
    // Внешнее зацепление: r = 20 и 10, межосевое 30; плоскости торцов совпадают.
    addOk(a, Mate::tangent({gear, Entity::makeCylinder(Vec3::Zero(), Vec3::UnitZ(), 20)},
                           {pinion, Entity::makeCylinder(Vec3::Zero(), Vec3::UnitZ(), 10)}));
    addOk(a, Mate::coincident({gear, Entity::makePlane(Vec3::Zero(), Vec3::UnitZ())},
                              {pinion, Entity::makePlane(Vec3::Zero(), Vec3::UnitZ())}, Alignment::Aligned));
    // Внутреннее: кольцо r = 40, сателлит r = 12 — межосевое 28.
    addOk(a, Mate::tangent({ring, Entity::makeCylinder(Vec3::Zero(), Vec3::UnitZ(), 40)},
                           {planet, Entity::makeCylinder(Vec3::Zero(), Vec3::UnitZ(), 12)}, Alignment::Closest, true));
    addOk(a, Mate::parallel({ring, Entity::makeAxis(Vec3::Zero(), Vec3::UnitZ())},
                            {planet, Entity::makeAxis(Vec3::Zero(), Vec3::UnitZ())}));

    const SolveResult r = solve(a);
    ASSERT_TRUE(r.success());
    // Ролик: ось параллельна полу на высоте 15; 4 степени свободы.
    const Transform& rp = r.poses[roller];
    EXPECT_NEAR(rp.position.z(), 15.0, 1e-9);
    EXPECT_NEAR(rp.applyToDirection(Vec3::UnitX()).z(), 0.0, 1e-9);
    EXPECT_EQ(reportFor(r, roller)->dof, 4);
    // Шар под полом: центр на z = −7; 5 степеней свободы.
    EXPECT_NEAR(r.poses[ball].position.z(), -7.0, 1e-9);
    EXPECT_EQ(reportFor(r, ball)->dof, 5);
    // Шестерни.
    const Vec3 dg = r.poses[pinion].position - Vec3(0, 50, 2);  // торец шестерни на z = 2
    EXPECT_NEAR(dg.head<2>().norm(), 30.0, 1e-9);
    EXPECT_NEAR(dg.z(), 0.0, 1e-9);
    EXPECT_EQ(reportFor(r, pinion)->dof, 2);  // обкатка вокруг шестерни и вращение вокруг своей оси
    const Vec3 dp = r.poses[planet].position - Vec3(-150, 0, 0);
    EXPECT_NEAR(dp.head<2>().norm(), 28.0, 1e-9);
    EXPECT_EQ(reportFor(r, planet)->dof, 3);  // обкатка, вращение, осевой сдвиг
}

TEST(AssemblyV2Solver, RigidMateCopiesRelativePose) {
    Assembly a;
    const Transform basePose = Transform::fromAxisAngle(Vec3(1, 2, 3), 0.8, Vec3(10, -20, 5));
    const ComponentId base = a.addComponent("base", basePose, true);
    const ComponentId part = a.addComponent("part", Transform::fromTranslation(Vec3(1, 1, 1)));
    const ComponentId tool = a.addComponent("tool", Transform::fromAxisAngle(Vec3::UnitX(), 0.4, Vec3(30, 0, 0)));
    const Transform rel = Transform::fromAxisAngle(Vec3(0, 1, 0), 1.3, Vec3(50, 0, 25));
    addOk(a, Mate::rigid(base, part, rel));
    // Без заданного relative фиксируется текущее относительное положение tool относительно part.
    const Transform toolRel = a.component(part).pose.inverse() * a.component(tool).pose;
    addOk(a, Mate::rigid(part, tool));
    const SolveResult r = solve(a);
    ASSERT_TRUE(r.success());
    EXPECT_EQ(r.totalDof, 0);
    const Transform expectedPart = basePose * rel;
    EXPECT_LT((r.poses[part].position - expectedPart.position).norm(), 1e-9);
    EXPECT_LT(rotationDistance(r.poses[part], expectedPart), 1e-9);
    const Transform expectedTool = expectedPart * toolRel;
    EXPECT_LT((r.poses[tool].position - expectedTool.position).norm(), 1e-9);
    EXPECT_LT(rotationDistance(r.poses[tool], expectedTool), 1e-9);
}

// ---------------------------------------------------------------------------------------------
// Избыточность и противоречия

TEST(AssemblyV2Solver, RedundantConsistentMatesAreReportedAndSolutionIsCorrect) {
    ShaftInHole s(kShaftStart);
    const MateId duplicate = addOk(s.a, Mate::concentric(s.hole, s.shaftCyl));
    const MateId parallel = addOk(s.a, Mate::parallel(s.top, s.shoulder));
    const SolveResult r = solve(s.a);
    const SubsystemReport& sub = r.subsystems[0];
    EXPECT_EQ(sub.status, SubsystemStatus::Solved);
    EXPECT_EQ(sub.dof, 1);
    EXPECT_EQ(sorted(sub.redundantMates), sorted({duplicate, parallel}));
    EXPECT_TRUE(sub.conflictingMates.empty());
    EXPECT_EQ(sumIndependent(sub), sub.rank);
    const Transform& p = r.poses[s.shaft];
    EXPECT_LT((p.applyToDirection(Vec3::UnitZ()) - Vec3::UnitZ()).norm(), 1e-9);
    EXPECT_LT(p.position.norm(), 1e-9);
}

TEST(AssemblyV2Solver, ContradictoryDistancesAreReportedWithTheirIds) {
    Assembly a;
    const ComponentId base = a.addComponent("base", Transform::identity(), true);
    const ComponentId plate = a.addComponent("plate", Transform::fromAxisAngle(Vec3(1, 0, 0), 0.1, Vec3(2, 3, 9)));
    const Feature baseTop{base, Entity::makePlane(Vec3::Zero(), Vec3::UnitZ())};
    const Feature plateBottom{plate, Entity::makePlane(Vec3::Zero(), -Vec3::UnitZ())};
    const MateId d10 = addOk(a, Mate::distance(plateBottom, baseTop, 10.0, Alignment::AntiAligned));
    const MateId pin = addOk(a, Mate::concentric({base, Entity::makeAxis(Vec3(5, 5, 0), Vec3::UnitZ())},
                                                 {plate, Entity::makeCylinder(Vec3::Zero(), Vec3::UnitZ(), 3)}));
    const MateId d15 = addOk(a, Mate::distance(plateBottom, baseTop, 15.0, Alignment::AntiAligned));
    // Независимая согласованная подсистема в той же сборке.
    const ComponentId housing2 = a.addComponent("housing2", Transform::fromTranslation(Vec3(500, 0, 0)), true);
    const ComponentId shaft2 = a.addComponent("shaft2", Transform::fromTranslation(Vec3(503, 1, 2)));
    addOk(a, Mate::concentric({housing2, Entity::makeAxis(Vec3::Zero(), Vec3::UnitZ())},
                              {shaft2, Entity::makeAxis(Vec3::Zero(), Vec3::UnitZ())}));

    const SolveResult r = solve(a);
    EXPECT_FALSE(r.success());
    const SubsystemReport* bad = reportFor(r, plate);
    ASSERT_NE(bad, nullptr);
    EXPECT_EQ(bad->status, SubsystemStatus::Conflicting);
    EXPECT_EQ(sorted(bad->conflictingMates), sorted({d10, d15}));
    // МНК-компромисс двух равновесных уравнений: z = 12.5, невязки ±2.5.
    EXPECT_NEAR(r.poses[plate].position.z(), 12.5, 1e-8);
    EXPECT_NEAR(bad->residualNorm, 2.5 * std::sqrt(2.0), 1e-8);
    EXPECT_LT(diagFor(*bad, pin)->residualNorm, 1e-9);
    const SubsystemReport* good = reportFor(r, shaft2);
    ASSERT_NE(good, nullptr);
    EXPECT_EQ(good->status, SubsystemStatus::Solved);
    EXPECT_EQ(good->dof, 2);  // вращение вокруг оси и осевой сдвиг
}

TEST(AssemblyV2Solver, ContradictoryOrientationIsReported) {
    ShaftInHole s(kShaftStart);
    const MateId perp = addOk(s.a, Mate::perpendicular(s.top, s.shaftCyl));
    const SolveResult r = solve(s.a);
    const SubsystemReport& sub = r.subsystems[0];
    EXPECT_EQ(sub.status, SubsystemStatus::Conflicting);
    // Минимум суммы квадратов — вал соосен (две согласованные оси против одной): ненулевая невязка
    // только у перпендикулярности (cos = 1), она и указана как противоречащая.
    EXPECT_EQ(sub.conflictingMates, std::vector<MateId>{perp});
    EXPECT_NEAR(diagFor(sub, perp)->residualNorm, 1.0, 1e-9);
    EXPECT_LT(diagFor(sub, s.concentric)->residualNorm, 1e-7);  // ниже порога conflictAbsolute
}

TEST(AssemblyV2Solver, MatesBetweenFixedComponentsAreChecked) {
    Assembly a;
    const ComponentId c1 = a.addComponent("c1", Transform::identity(), true);
    const ComponentId c2 = a.addComponent("c2", Transform::fromTranslation(Vec3(0, 0, 4)), true);
    const MateId ok = addOk(a, Mate::parallel({c1, Entity::makeAxis(Vec3::Zero(), Vec3::UnitZ())},
                                              {c2, Entity::makeAxis(Vec3::Zero(), Vec3::UnitZ())}));
    const MateId bad = addOk(a, Mate::coincident({c1, Entity::makePlane(Vec3::Zero(), Vec3::UnitZ())},
                                                 {c2, Entity::makePlane(Vec3::Zero(), Vec3::UnitZ())}));
    const SolveResult r = solve(a);
    ASSERT_EQ(r.subsystems.size(), 1u);
    const SubsystemReport& sub = r.subsystems[0];
    EXPECT_TRUE(sub.components.empty());
    EXPECT_EQ(sub.status, SubsystemStatus::Conflicting);
    EXPECT_EQ(sub.conflictingMates, std::vector<MateId>{bad});
    EXPECT_EQ(sub.redundantMates, std::vector<MateId>{ok});
    EXPECT_NEAR(sub.residualNorm, 4.0, 1e-12);
}

// ---------------------------------------------------------------------------------------------
// Разбиение и сходимость

TEST(AssemblyV2Solver, IndependentSubsystemsAreSolvedSeparately) {
    Assembly a;
    const ComponentId ground = a.addComponent("ground", Transform::identity(), true);
    std::vector<ComponentId> shafts;
    for (int i = 0; i < 2; ++i) {
        const ComponentId s = a.addComponent("shaft", Transform::fromTranslation(Vec3(100.0 * i + 2, 1, 3)));
        shafts.push_back(s);
        addOk(a, Mate::concentric({ground, Entity::makeAxis(Vec3(100.0 * i, 0, 0), Vec3::UnitZ())},
                                  {s, Entity::makeAxis(Vec3::Zero(), Vec3::UnitZ())}));
        addOk(a, Mate::coincident({ground, Entity::makePlane(Vec3::Zero(), Vec3::UnitZ())},
                                  {s, Entity::makePlane(Vec3::Zero(), Vec3::UnitZ())}));
    }
    const ComponentId loose = a.addComponent("loose", Transform::fromTranslation(Vec3(7, 7, 7)));
    const auto parts = partitionSubsystems(a);
    ASSERT_EQ(parts.size(), 3u);
    const SolveResult r = solve(a);
    ASSERT_TRUE(r.success());
    EXPECT_EQ(reportFor(r, shafts[0])->dof, 1);
    EXPECT_EQ(reportFor(r, shafts[1])->dof, 1);
    EXPECT_EQ(reportFor(r, loose)->dof, 6);
    EXPECT_EQ(r.totalDof, 8);
    EXPECT_LT((r.poses[shafts[1]].position - Vec3(100, 0, 0)).norm(), 1e-9);
    EXPECT_LT((r.poses[loose].position - Vec3(7, 7, 7)).norm(), 1e-15);
}

TEST(AssemblyV2Solver, ConvergesFromRandomPerturbations) {
    std::mt19937 rng(424242);
    for (int trial = 0; trial < 40; ++trial) {
        PlateWithBolts p(&rng, 5.0, 0.3);
        const SolveResult r = solve(p.a);
        p.expectSolved(r);
        EXPECT_EQ(r.totalDof, 2);
    }
    for (int trial = 0; trial < 40; ++trial) {
        ShaftInHole s(perturb(Transform::fromAxisAngle(Vec3::UnitZ(), deg(40)), rng, 5.0, 0.3));
        addOk(s.a, Mate::angle(s.housingX, s.shaftX, deg(40)));
        const SolveResult r = solve(s.a);
        ASSERT_TRUE(r.success()) << trial;
        EXPECT_NEAR(yawOf(r.poses[s.shaft]), deg(40), 1e-9) << trial;
        EXPECT_LT(r.poses[s.shaft].position.norm(), 1e-9) << trial;
    }
}

}  // namespace
