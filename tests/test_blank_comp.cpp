// Эталонные значения получены из Python: blank.py и compensation.py (19.10.2026).
#include "bendcore/compensation.hpp"
#include "bendcore/blank.hpp"

#include <cmath>
#include <cstdio>

using namespace bendcore;

static int failures = 0;

static void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}

static bool near(double a, double b, double eps = 1e-9) { return std::abs(a - b) <= eps; }

static bool has_issue(const std::vector<Issue>& v, const char* kind, int index) {
    for (const auto& x : v) {
        if (x.kind == kind && x.index == index) return true;
    }
    return false;
}

static Path sample() {
    return Path{Straight{100}, Bend{50, 90}, Straight{100}, Bend{40, 60, 20}, Straight{80}};
}

static void test_blank_length() {
    const auto r = blank_length(sample(), EndRule{10, 30, true}, EndRule{8, 25, true}, 5);
    check(r.delta_start && near(*r.delta_start, 5) && r.delta_end && near(*r.delta_end, 5), "blank_length: припуски Δ = Δ_мин");
    check(r.blank_length && near(*r.blank_length, 428.42771838760876, 1e-9), "blank_length: длина");
    check(r.issues.empty(), "blank_length: без замечаний");

    const auto nc = blank_length(Path{Straight{10}, Bend{50, 90}, Straight{100}}, EndRule{0, 30, false},
                                 EndRule{0, 30, true}, 5);
    check(nc.delta_start && near(*nc.delta_start, 0) && nc.delta_end && near(*nc.delta_end, 5), "без подрезки: Δ начала 0");
    check(nc.blank_length && near(*nc.blank_length, 193.53981633974485, 1e-9), "без подрезки: длина");
    check(has_issue(nc.issues, "technological", -1), "без подрезки: S + E меньше N — технологическая");
}

static void test_min_straights() {
    const auto iss = check_min_straights(Path{Straight{10}, Bend{50, 90}, Straight{20}, Bend{50, 90}, Straight{100}}, 30);
    check(has_issue(iss, "technological", 3), "check_min_straights: короткий участок между гибами");
}

static void test_blank_by_order() {
    const auto r = blank_by_order(sample(), EndSpec{10, 60}, EndSpec{8, std::nullopt}, 30, 25, 'A', true, 5);
    // ручной участок меньше минимума: ошибка, Δ и длина не показываются
    check(!r.delta_start && !r.delta_end && !r.blank_length, "blank_by_order: при ошибке значения не показываются");
    check(has_issue(r.issues, "technological", -1), "blank_by_order: ошибка ручного участка");

    const auto r2 = blank_by_order(sample(), EndSpec{10, 20}, EndSpec{8, std::nullopt}, 30, 25, 'A', false, 5);
    check(!r2.delta_start && !r2.blank_length, "blank_by_order: один порядок, ручной 20 — без значений");

    const auto ok = blank_by_order(sample(), EndSpec{10, 120}, EndSpec{8, std::nullopt}, 30, 25, 'A', false, 5);
    check(ok.delta_start && near(*ok.delta_start, 10.0), "blank_by_order: корректный ручной участок — Δ считается");
}

static void test_blank_limits() {
    check(check_blank_limits(1000, std::nullopt, 6000, std::nullopt).empty(), "limits: в пределах хлыста");
    check(has_issue(check_blank_limits(7000, std::nullopt, 6000, 8000), "technological", -1),
          "limits: длиннее хлыста — технологическая");
    check(has_issue(check_blank_limits(1000, std::nullopt, std::nullopt, std::nullopt), "info", -1),
          "limits: хлыст не задан — информация");
}

static void test_compensation() {
    // эталон Python: K = 0.9 (B/K и S_ст по 3.6, n/K)
    const Path st = compensated_path(sample(), 0.9);
    check(near(std::get<Straight>(st[0]).S, 94.44444444444444, 1e-9), "compensated_path: S первого участка");
    check(near(std::get<Bend>(st[1]).B, 100.0, 1e-9), "compensated_path: B/K первого гиба");
    check(near(std::get<Straight>(st[2]).S, 91.87844324804611, 1e-9), "compensated_path: S между гибами");
    check(near(std::get<Bend>(st[3]).B, 66.66666666666667, 1e-9), "compensated_path: B/K второго гиба");
    check(near(std::get<Straight>(st[4]).S, 77.43399880360167, 1e-9), "compensated_path: S последнего участка");

    check(near(delta_T(Bend{50, 90}, 0.9), 5.555555555555556, 1e-9), "delta_T");

    const auto cb = compensated_blank(sample(), EndRule{10, 30, true}, EndRule{8, 25, true}, 0.9, 5);
    check(cb.blank_length && near(*cb.blank_length, 425.56546248232416, 1e-9), "compensated_blank: длина с компенсацией");

    // теоретический B_ст имеет приоритет над B/K
    const Theoretical fixed = [](const Bend&) -> std::optional<double> { return 95.0; };
    check(near(b_st(Bend{50, 90}, 0.9, fixed), 95.0), "b_st: теоретический угол приоритетнее");
    check(near(b_st(Bend{50, 90}, 0.9), 100.0), "b_st: без теоретического — B/K");

    const auto ang = check_station_angles(Path{Straight{10}, Bend{50, 90}}, 80, 0.9);
    check(has_issue(ang, "technological", 1), "check_station_angles: B_ст больше макс. угла профиля");
}

int main() {
    test_blank_length();
    test_min_straights();
    test_blank_by_order();
    test_blank_limits();
    test_compensation();
    if (failures == 0) std::printf("OK blank_comp\n");
    return failures == 0 ? 0 : 1;
}
