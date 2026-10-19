// Эталоны и сценарии — из test_ports.py (19.10.2026).
#include "bendcore/ports.hpp"

#include <cmath>
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

static bool has_kind(const std::vector<Issue>& v, const char* kind) {
    for (const auto& x : v) {
        if (x.kind == kind) return true;
    }
    return false;
}

static Port base_port(const std::string& name = "П1", double od = 20.0, double min_straight = 30.0) {
    Port p;
    p.name = name;
    p.pos = {0, 0, 0};
    p.direction = {1, 0, 0};
    p.od = od;
    p.min_straight = min_straight;
    return p;
}

static void test_checks() {
    const Path ok{Straight{100}, Bend{50, 90}, Straight{100}};
    check(check_connection(base_port(), ok, 20.0, "T1").empty(), "совпадающий порт — без замечаний");

    check(has_kind(check_connection(base_port(), ok, 16.0, "T1"), "warning") &&
              check_connection(base_port(), ok, 16.0, "T1").size() == 1,
          "∅ не совпадает — предупреждение");

    Port mp = base_port("П1", 20.0, 50.0);
    check(has_kind(check_connection(mp, Path{Straight{20}, Bend{50, 90}, Straight{100}}, 20.0, "T1"), "technological"),
          "вылет меньше минимального — технологическая");

    Port tp = base_port();
    tp.tol_ang = 1.0;
    const Vec3 end_dir{0.9962, 0.0872, 0.0};   // ~5° от оси X
    check(has_kind(check_connection(tp, ok, 20.0, "T1", 'A', std::nullopt, end_dir), "warning"),
          "отклонение направления — предупреждение");
}

static void test_occupancy() {
    Port p = base_port();
    p.occupied_by = "T9";
    check(has_kind(check_connection(p, Path{Straight{100}, Bend{50, 90}, Straight{100}}, 20.0, "T1"), "refused"),
          "занятый порт — отказ");
    check(p.occupied_by && *p.occupied_by == "T9", "отказ не меняет занятость");

    Port q = base_port();
    connect(q, "T1", Path{Straight{100}, Bend{50, 90}, Straight{100}}, 20.0);
    check(q.occupied_by && *q.occupied_by == "T1", "connect занимает порт");
}

static void test_candidates_and_start() {
    std::vector<Port> ports{base_port("A"), base_port("B", 16.0), base_port("C")};
    ports[2].occupied_by = "T2";
    const auto c = candidates(ports, 20.0);
    check(c.size() == 1 && c[0]->name == "A", "candidates: только свободный порт подходящего ∅");
    const auto [pos, dir] = start_from_port(ports[0]);
    check(std::abs(pos[0]) < 1e-12 && std::abs(dir[0] - 1.0) < 1e-12, "start_from_port: позиция и направление");
    check(auto_name("Станок", {"Станок.П1"}) == "Станок.П2", "auto_name: следующий свободный номер");
}

// Ручной порт «по точке + направлению + ∅»: направление нормируется; неверные данные — исключение.
static void test_manual_port() {
    const Port p = manual_port({1.0, 2.0, 3.0}, {0.0, 0.0, 5.0}, 20.0, "ручной");
    check(p.name == "ручной" && p.od == 20.0, "manual_port: имя и ∅");
    check(std::abs(p.direction[2] - 1.0) < 1e-12 && std::abs(norm(p.direction) - 1.0) < 1e-12, "manual_port: направление нормировано");
    check(p.pos[0] == 1.0 && p.pos[1] == 2.0 && p.pos[2] == 3.0, "manual_port: положение");
    bool threw_od = false;
    bool threw_dir = false;
    try { manual_port({0, 0, 0}, {1, 0, 0}, 0.0, "x"); } catch (const std::invalid_argument&) { threw_od = true; }
    try { manual_port({0, 0, 0}, {0, 0, 0}, 20.0, "x"); } catch (const std::invalid_argument&) { threw_dir = true; }
    check(threw_od, "manual_port: ∅ <= 0 — исключение");
    check(threw_dir, "manual_port: нулевое направление — исключение");
}

int main() {
    test_manual_port();
    test_checks();
    test_occupancy();
    test_candidates_and_start();
    if (failures == 0) std::printf("OK ports\n");
    return failures == 0 ? 0 : 1;
}
