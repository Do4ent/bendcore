// Пакетная проверка сборки (4.2, 4.3, 4.5).
#include "bendcore/batch_check.hpp"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>

using namespace bendcore;
using namespace bendcore::check;

static int failures = 0;
static void check_(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}
static bool near(double a, double b, double eps = 1e-6) { return std::abs(a - b) <= eps; }

static TubeInput tube(const std::string& id, Path p, double od = 12.0) {
    TubeInput t;
    t.id = id;
    t.path = std::move(p);
    t.od = od;
    t.B_max = 180;
    t.L_min = 20;
    return t;
}
static TubeInput straight_at(const std::string& id, double y, double len = 200, double od = 12) {
    TubeInput t = tube(id, Path{Straight{len}}, od);
    t.placement.origin = {0, y, 0};
    return t;
}
static int count_rule(const Result& r, const std::string& rule) {
    int n = 0;
    for (const auto& v : r.violations) n += v.rule == rule;
    return n;
}

static void test_clean() {
    const auto r = run({tube("T1", Path{Straight{100}, Bend{40, 90}, Straight{100}})});
    check_(r.violations.empty(), "чистая труба без нарушений");
    check_(r.tubes.at("T1").state == TubeState::Passed && !r.tubes.at("T1").blocks_output, "состояние «пройдено»");
}

static void test_classes() {
    TubeInput bmax = tube("B", Path{Straight{100}, Bend{40, 150}, Straight{100}});
    bmax.B_max = 120;
    TubeInput geo = tube("G", Path{Straight{100}, Bend{40, 90}, Straight{-5}, Bend{40, 90}, Straight{100}});
    TubeInput lmin = tube("L", Path{Straight{100}, Bend{40, 90}, Straight{10}, Bend{40, 90}, Straight{100}});
    TubeInput degen = tube("D", Path{Straight{100}, Bend{40, 0.3}, Straight{100}});
    const auto r = run({bmax, geo, lmin, degen});
    check_(r.tubes.at("B").worst == Severity::Technological && r.tubes.at("B").blocks_output, "B > B_max — техн. ошибка");
    check_(r.tubes.at("G").worst == Severity::Geometric, "S < 0 — геометрическая ошибка");
    check_(r.tubes.at("D").worst == Severity::Warning && !r.tubes.at("D").blocks_output, "вырожденный гиб — предупреждение");
    bool lfound = false;
    for (const auto& v : r.violations) {
        if (v.rule == "L_min") {
            lfound = v.tube_id == "L" && v.value && near(*v.value, 10) && v.limit && near(*v.limit, 20) && v.element == 3;
        }
        if (v.rule == "B_max") check_(v.value && near(*v.value, 150) && v.limit && near(*v.limit, 120), "факт и норма B");
    }
    check_(lfound, "L_min: факт 10, норма 20, элемент 3");
    check_(r.violations.front().severity == Severity::Geometric, "сортировка по тяжести");
    check_(r.counts.at(Severity::Geometric) == 1, "счётчики классов");
}

static void test_ends() {
    TubeInput t = tube("N", Path{Straight{30}, Bend{40, 90}, Straight{100}});
    t.N_first = 50;
    t.cut_allowed = false;
    const auto r = run({t});
    check_(count_rule(r, "N_end") == 1 && r.tubes.at("N").blocks_output, "N при запрете подрезки — ошибка");
    t.cut_allowed = true;
    check_(count_rule(run({t}), "N_end") == 0, "с подрезкой N не ошибка");
}

static void test_clearance() {
    // ∅12, оси на 20 мм: зазор 8 ≥ 5.
    check_(run({straight_at("A", 0), straight_at("B", 20)}).violations.empty(), "зазор 8 мм в норме");
    const auto r = run({straight_at("A", 0), straight_at("B", 15)});
    check_(count_rule(r, "clearance") == 2, "зазор 3 мм — пометка у обеих труб");
    for (const auto& v : r.violations) {
        check_(v.severity == Severity::Mark && v.value && near(*v.value, 3) && v.limit && near(*v.limit, 5),
               "пометка: факт 3, норма 5");
    }
    check_(!r.tubes.at("A").blocks_output, "пометка не блокирует");
    const auto x = run({straight_at("A", 0), straight_at("B", 10)});
    check_(count_rule(x, "tube_intersection") == 2, "пересечение труб");
    check_(x.violations[0].value && near(*x.violations[0].value, 2), "глубина пересечения 2 мм");
    Options off;
    off.tube_to_tube = false;
    check_(run({straight_at("A", 0), straight_at("B", 10)}, off).violations.empty(), "проверку между трубами можно выключить");
    // Встык торцами — не коллизия.
    TubeInput c = straight_at("C", 0);
    c.placement.origin = {200, 0, 0};
    check_(run({straight_at("A", 0), c}).violations.empty(), "трубы встык не пересекаются");
}

static void test_fingerprint() {
    TubeInput a = tube("T", Path{Straight{100}, Bend{40, 90}, Straight{100}});
    const auto f1 = fingerprint(a);
    a.path[0] = Straight{101};
    check_(fingerprint(a) != f1, "отпечаток меняется при правке пути");
    check_(run({a}).tubes.at("T").fingerprint == fingerprint(a), "отпечаток в результате");
}

static void test_scale() {
    // 3000 труб сеткой 60×50 с шагом 40 мм: нарушений нет, широкая фаза отсекает почти все пары.
    std::vector<TubeInput> ts;
    for (int i = 0; i < 60; ++i)
        for (int j = 0; j < 50; ++j) {
            TubeInput t = tube("T" + std::to_string(i) + "_" + std::to_string(j),
                               Path{Straight{100}, Bend{30, 90}, Straight{60}, Bend{30, 90, 90}, Straight{80}});
            t.placement.origin = {i * 400.0, j * 400.0, 0};
            ts.push_back(t);
        }
    const auto t0 = std::chrono::steady_clock::now();
    const auto r = run(ts);
    const double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    check_(r.tubes.size() == 3000 && r.violations.empty(), "3000 труб без нарушений");
    check_(r.pairs_tested < 3000, "широкая фаза: мало пар");
    std::printf("  3000 труб: %.2f с, пар после широкой фазы %d\n", sec, r.pairs_tested);
    check_(sec < 10.0, "3000 труб быстрее 10 с");
}

static void test_duplicate_id() {
    bool threw = false;
    try {
        (void)run({straight_at("A", 0), straight_at("A", 100)});
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check_(threw, "повтор id — ошибка");
}

int main() {
    test_clean();
    test_classes();
    test_ends();
    test_clearance();
    test_fingerprint();
    test_scale();
    test_duplicate_id();
    if (failures) {
        std::printf("batch_check: %d FAIL\n", failures);
        return 1;
    }
    std::printf("batch_check: OK\n");
    return 0;
}
