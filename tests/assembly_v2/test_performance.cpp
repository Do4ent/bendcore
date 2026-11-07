// Критерий фазы 3: сборка из 30 компонентов и 80 сопряжений решается быстрее 0,5 с.
// Сборка строится из известного эталонного положения (случайное дерево шарниров с фиксированным
// seed), затем положения возмущаются и решаются заново.

#include "test_helpers.hpp"

#include <chrono>
#include <cstdio>
#include <algorithm>
#include <string>
#include <vector>

using namespace bendcore::assembly_v2;
using namespace bendcore::assembly_v2::testing;

namespace {

struct Reference {
    Assembly a;
    std::vector<Transform> truth;
    std::vector<bool> determined;  // положение однозначно определено сопряжениями
    int expectedDof = 0;
};

// Компонент i (1..29) крепится к случайному родителю j < i (дерево): соосность + совпадение (или расстояние)
// торцов — шарнир; у 22 компонентов вращение запирает третье сопряжение (параллельность, угол или
// перпендикулярность). Итого 22·3 + 7·2 = 80 сопряжений, 7 степеней свободы.
Reference build(std::mt19937& rng) {
    Reference ref;
    const int n = 30;
    const int revolute = 7;  // компоненты с двумя сопряжениями
    ref.truth.push_back(Transform::identity());
    ref.determined.push_back(true);
    ref.a.addComponent("base", Transform::identity(), true);
    std::uniform_int_distribution<int> kind(0, 2);
    for (int i = 1; i < n; ++i) {
        // Компонент 1 крепится к основанию, остальные — к нефиксированным: одна подсистема из 29 тел.
        std::uniform_int_distribution<int> parentDist(1, std::max(1, i - 1));
        const int j = i == 1 ? 0 : parentDist(rng);
        const Transform gi = Transform::fromAxisAngle(randomUnit(rng), 1.0, randomVec(rng, 200));
        ref.truth.push_back(gi);
        const ComponentId ci = ref.a.addComponent("c" + std::to_string(i), gi);
        const Transform& gj = ref.truth[j];
        const Vec3 o = 0.5 * (gi.position + gj.position) + randomVec(rng, 20);
        const Vec3 d = randomUnit(rng);
        Vec3 e = randomUnit(rng);
        e = (e - d * d.dot(e)).normalized();
        const auto onJ = [&](const Entity& w) { return Feature{static_cast<ComponentId>(j), toLocal(w, gj)}; };
        const auto onI = [&](const Entity& w) { return Feature{ci, toLocal(w, gi)}; };

        addOk(ref.a, Mate::concentric(onJ(Entity::makeCylinder(o, d, 5)), onI(Entity::makeCylinder(o + 7 * d, d, 5))));
        if (i % 2 == 0) {
            addOk(ref.a, Mate::coincident(onJ(Entity::makePlane(o, d)), onI(Entity::makePlane(o, -d)),
                                          Alignment::AntiAligned));
        } else {
            addOk(ref.a, Mate::distance(onI(Entity::makePlane(o + 4 * d, -d)), onJ(Entity::makePlane(o, d)), 4.0,
                                        Alignment::AntiAligned));
        }
        const bool locked = i < n - revolute;  // последние 7 — шарниры с одной степенью свободы
        if (locked) {
            const Vec3 f = d.cross(e);
            switch (kind(rng)) {
                case 0:
                    addOk(ref.a, Mate::parallel(onJ(Entity::makeAxis(o, e)), onI(Entity::makeAxis(o, e))));
                    break;
                case 1: {
                    const Vec3 e40 = Transform::fromAxisAngle(d, deg(40)).applyToDirection(e);
                    addOk(ref.a, Mate::angle(onJ(Entity::makeAxis(o, e)), onI(Entity::makeAxis(o, e40)), deg(40)));
                    break;
                }
                default:
                    addOk(ref.a, Mate::perpendicular(onJ(Entity::makeAxis(o, e)), onI(Entity::makePlane(o, f))));
                    break;
            }
        } else {
            ++ref.expectedDof;
        }
        ref.determined.push_back(locked && ref.determined[j]);
    }
    return ref;
}

TEST(AssemblyV2Performance, ThirtyComponentsEightyMatesUnderHalfSecond) {
    std::mt19937 rng(30080);
    Reference ref = build(rng);
    ASSERT_EQ(ref.a.components().size(), 30u);
    ASSERT_EQ(ref.a.mates().size(), 80u);
    // Эталон удовлетворяет всем сопряжениям.
    {
        const SolveResult exact = solve(ref.a);
        ASSERT_TRUE(exact.success());
        EXPECT_EQ(exact.totalIterations, 0);
        EXPECT_EQ(exact.totalDof, ref.expectedDof);
    }
    for (std::size_t c = 1; c < ref.truth.size(); ++c) {
        ref.a.setPose(static_cast<ComponentId>(c), perturb(ref.truth[c], rng, 1.0, 0.05));
    }
    const auto t0 = std::chrono::steady_clock::now();
    const SolveResult r = solve(ref.a);
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    std::printf("assembly_v2: 30 components, 80 mates: %.4f s, %d iterations, residual %.3e, dof %d\n", seconds,
                r.totalIterations, r.residualNorm, r.totalDof);
    ASSERT_TRUE(r.success());
    ASSERT_EQ(r.subsystems.size(), 1u);
    EXPECT_LT(r.residualNorm, 1e-9);
    EXPECT_EQ(r.totalDof, ref.expectedDof);
    EXPECT_TRUE(r.subsystems[0].redundantMates.empty());
    EXPECT_LT(seconds, 0.5);
    int checked = 0;
    for (std::size_t c = 1; c < ref.truth.size(); ++c) {
        if (!ref.determined[c]) continue;
        ++checked;
        EXPECT_LT((r.poses[c].position - ref.truth[c].position).norm(), 1e-7) << c;
        EXPECT_LT(rotationDistance(r.poses[c], ref.truth[c]), 1e-8) << c;
    }
    EXPECT_GT(checked, 5);
}

}  // namespace
