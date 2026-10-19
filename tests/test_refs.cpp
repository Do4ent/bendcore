// Эталоны — из refs.py (19.10.2026).
#include "bendcore/refs.hpp"

#include <cstdio>
#include <stdexcept>

using namespace bendcore;

static int failures = 0;

static void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}

static Tube sample_tube(const std::string& mode = "адаптивна") {
    Tube t;
    t.points = {{0, 0, 0}, {100, 0, 0}, {200, 0, 0}};
    t.refs = {Ref{"R1", 2, {200, 0, 0}, kValid}};
    t.mode = mode;
    return t;
}

// Заглушка решателя для тестов: возвращает точки без изменений (проверяем только вызов и аргументы).
static PointSolver stub_solver(bool& called) {
    return [&called](const std::vector<Vec3>& start, const std::vector<Coincide>& cons, const std::vector<int>& fixed) {
        called = true;
        check(cons.size() == 1 && cons[0].vertex == 2, "solver: совпадение для вершины ссылки");
        check(fixed.size() == 2 && fixed[0] == 0 && fixed[1] == 1, "solver: остальные вершины фиксированы");
        return start;
    };
}

static void test_states() {
    Tube t = sample_tube();
    auto r = update_component(t, {{"R1", {200, 0, 0}}}, nullptr);
    check(r.status == "все в норме" && t.refs[0].state == kValid && r.lost.empty(), "все в норме");

    Tube lost = sample_tube();
    auto rl = update_component(lost, {}, nullptr);
    check(rl.status == "потеряны ссылки" && lost.refs[0].state == kLost && rl.lost.size() == 1, "потеряна ссылка");

    Tube nl = sample_tube();
    auto rn = update_component(nl, {{"R1", {200, 0, 0}}}, nullptr, 1e-6, {"R1"});
    check(rn.status == "не загружена" && nl.refs[0].state == kNotLoaded, "не загружена");

    Tube fr = sample_tube("заморожена");
    auto rf = update_component(fr, {}, nullptr);
    check(rf.status == "заморожена" && rf.points.size() == 3, "заморожена — значения из кэша");
}

static void test_moved_calls_solver() {
    Tube t = sample_tube();
    bool called = false;
    auto r = update_component(t, {{"R1", {200, 5, 0}}}, stub_solver(called));
    check(called, "сдвиг — вызывается решатель");
    check(r.status == "вершины сдвинулись" && r.moved.size() == 1, "сдвиг — статус");
    check(t.refs[0].state == kValid && t.refs[0].cached[1] == 5.0, "после пересчёта кэш обновлён");
}

static void test_initial_mode() {
    check(initial_mode("assembly") == "адаптивна", "initial_mode: сборка — адаптивна");
    check(initial_mode("recognized") == "заморожена", "initial_mode: распознанная — заморожена");
    bool thrown = false;
    try {
        initial_mode("неизвестно");
    } catch (const std::invalid_argument&) {
        thrown = true;
    }
    check(thrown, "initial_mode: неизвестное происхождение — ошибка");
}

int main() {
    test_states();
    test_moved_calls_solver();
    test_initial_mode();
    if (failures == 0) std::printf("OK refs\n");
    return failures == 0 ? 0 : 1;
}
