// Правила позиционирования (positioning.hpp). Ожидаемые значения посчитаны вручную по геометрии,
// не получены прогоном кода: пути из прямых и гибов 90° считаются в уме.
#include "bendcore/positioning.hpp"

#include <cmath>
#include <cstdio>
#include <limits>
#include <stdexcept>

using namespace bendcore;

static int failures = 0;

static void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}

static bool near(double a, double b, double tol = 1e-6) { return std::abs(a - b) <= tol; }

static bool near3(const Vec3& a, const Vec3& b, double tol = 1e-6) {
    return near(a[0], b[0], tol) && near(a[1], b[1], tol) && near(a[2], b[2], tol);
}

static bool has_kind(const std::vector<Issue>& v, const char* kind) {
    for (const auto& x : v) {
        if (x.kind == kind) return true;
    }
    return false;
}

static const double kPi = 3.14159265358979323846;

// Путь «100 — гиб 90° (R50) — 100»: локальный конец (150, 150, 0), касательная в конце (0, 1, 0).
static Path bent_path() { return Path{Straight{100}, Bend{50, 90}, Straight{100}}; }

static Port origin_port(const Vec3& dir = {1, 0, 0}) { return manual_port({0, 0, 0}, dir, 20.0, "П1"); }

static void test_frame() {
    const Frame f = frame_from_port(manual_port({10, 20, 30}, {1, 0, 0}, 20.0, "П1"));
    check(near3(f.apply({0, 0, 0}), {10, 20, 30}), "рамка: начало в точке порта");
    check(near3(f.rotate({1, 0, 0}), {1, 0, 0}) && near3(f.rotate({0, 1, 0}), {0, 1, 0}) &&
              near3(f.rotate({0, 0, 1}), {0, 0, 1}),
          "рамка порта +X — единичный поворот");

    const Frame fz = frame_from_port(origin_port({0, 0, 1}));
    check(near3(fz.rotate({1, 0, 0}), {0, 0, 1}), "порт +Z: касательная вдоль +Z");
    check(near3(cross(fz.x, fz.y), fz.z), "порт +Z: правая тройка");

    const Frame fn = frame_from_port(origin_port({0, 0, -1}));
    check(near3(fn.rotate({1, 0, 0}), {0, 0, -1}), "порт -Z: касательная вдоль -Z");
    check(near3(cross(fn.x, fn.y), fn.z), "порт -Z: правая тройка");

    const Frame fr = frame_from_port(origin_port(), 90.0);
    check(near3(fr.rotate({0, 1, 0}), {0, 0, 1}) && near3(fr.rotate({0, 0, 1}), {0, -1, 0}),
          "поворот 90° вокруг оси: y -> z, z -> -y");

    const Frame fa = frame_from_port(origin_port({1, 2, 3}), 33.0);
    check(near(norm(fa.x), 1.0) && near(norm(fa.y), 1.0) && near(norm(fa.z), 1.0), "произвольный порт: единичные оси");
    check(near(dot(fa.x, fa.y), 0.0) && near(dot(fa.y, fa.z), 0.0) && near(dot(fa.x, fa.z), 0.0),
          "произвольный порт: оси ортогональны");
    check(near3(cross(fa.x, fa.y), fa.z), "произвольный порт: правая тройка");

    Port zero;
    zero.direction = {0, 0, 0};
    bool threw = false;
    try {
        frame_from_port(zero);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "нулевое направление порта — исключение");
}

static void test_place() {
    const Placement ps = place_from_port(Path{Straight{100}}, manual_port({10, 20, 30}, {0, 1, 0}, 20.0, "П1"));
    check(near3(ps.end_pos, {10, 120, 30}) && near3(ps.end_dir, {0, 1, 0}), "прямая 100 вдоль +Y из (10,20,30)");
    check(ps.centerline.size() == 2 && near3(ps.centerline.front(), {10, 20, 30}), "ось: две точки, начало на порту");

    const Placement p0 = place_from_port(bent_path(), origin_port());
    check(near3(p0.end_pos, {150, 150, 0}) && near3(p0.end_dir, {0, 1, 0}), "гиб 90°: конец (150,150,0), касательная +Y");
    check(near3(p0.centerline.back(), p0.end_pos, 1e-9), "последняя точка оси совпадает с концом");

    const Placement p90 = place_from_port(bent_path(), origin_port(), 90.0);
    check(near3(p90.end_pos, {150, 0, 150}) && near3(p90.end_dir, {0, 0, 1}), "поворот 90°: конец (150,0,150)");

    const Placement pz = place_from_port(bent_path(), origin_port({0, 0, 1}));
    check(near3(pz.end_pos, {150, 0, 150}) && near3(pz.end_dir, {1, 0, 0}), "порт +Z: конец (150,0,150), касательная +X");

    const Placement p37 = place_from_port(bent_path(), manual_port({5, -7, 11}, {1, 2, 3}, 20.0, "П1"), 37.0);
    check(near(norm(sub(p37.end_pos, {5, -7, 11})), 150.0 * std::sqrt(2.0), 1e-6), "перенос и поворот сохраняют расстояние");

    // Путь, кончающийся гибом: хорда последнего шага дуги не касательная, а end_dir — точная.
    const Placement pb = place_from_port(Path{Straight{100}, Bend{50, 90}}, origin_port());
    check(near3(pb.end_pos, {150, 50, 0}, 1e-9) && near3(pb.end_dir, {0, 1, 0}, 1e-9), "путь кончается гибом: точный конец");
    const Vec3 chord = unit(sub(pb.centerline[pb.centerline.size() - 1], pb.centerline[pb.centerline.size() - 2]));
    check(std::abs(dot(chord, pb.end_dir)) < 1.0 - 1e-6, "хорда последнего шага отличается от касательной");
}

static void test_end_connection() {
    const Path path = bent_path();
    const Port a = origin_port();
    const Placement pl = place_from_port(path, a, 37.0);

    Port b = manual_port(pl.end_pos, mul(pl.end_dir, -1.0), 20.0, "П2");
    b.min_straight = 30.0;
    check(end_connection_issues(pl, b, path, 20.0, "Т1").empty(), "конец Б на своём порту — без замечаний");

    Port shifted = b;
    shifted.pos = add(b.pos, {10, 0, 0});
    check(has_kind(end_connection_issues(pl, shifted, path, 20.0, "Т1"), "warning"), "смещение 10 мм при допуске 3 — предупреждение");

    Port tilted = b;
    tilted.direction = unit(add(unit(b.direction), mul(pl.frame.x, std::tan(5.0 * kPi / 180.0))));   // ось x ⟂ end_dir
    check(has_kind(end_connection_issues(pl, tilted, path, 20.0, "Т1"), "warning"), "наклон 5° при допуске 1° — предупреждение");

    Port reversed = b;
    reversed.flip();
    check(has_kind(end_connection_issues(pl, reversed, path, 20.0, "Т1"), "warning"), "порт повёрнут на 180° — предупреждение");

    Port tight = b;
    tight.min_straight = 150.0;
    check(has_kind(end_connection_issues(pl, tight, path, 20.0, "Т1"), "technological"), "вылет 100 < 150 у конца Б — технологическая ошибка");
}

static void test_solve_roll() {
    const Path path = bent_path();
    const Port a = origin_port();
    const double rolls[] = {0.0, 37.0, -120.0, 179.0, 90.0, -1.5};
    for (const double roll : rolls) {
        const Placement pl = place_from_port(path, a, roll);
        const Port b = manual_port(pl.end_pos, mul(pl.end_dir, -1.0), 20.0, "П2");
        const RollSolution s = solve_roll(path, a, b);
        check(s.pos_dev < 1e-4 && s.dir_dev < 1e-3 && s.within_tol, "поворот: конец приходит на порт Б");
        check(near(wrap180(s.roll_deg - roll), 0.0, 1e-3), "поворот: найден исходный угол");
    }

    // Порт Б вне досягаемости: поворот найден, но вне допуска.
    const Port far_port = manual_port({1000, 0, 0}, {-1, 0, 0}, 20.0, "П3");
    check(!solve_roll(path, a, far_port).within_tol, "порт вне досягаемости — вне допуска");

    // Прямая вдоль оси: поворот ничего не меняет — берётся ближайший к нулю.
    const Port straight_end = manual_port({100, 0, 0}, {-1, 0, 0}, 20.0, "П4");
    const RollSolution flat = solve_roll(Path{Straight{100}}, a, straight_end);
    check(near(flat.roll_deg, 0.0, 1e-9) && flat.pos_dev < 1e-9 && flat.within_tol, "прямая по оси: поворот 0");

    bool threw = false;
    try {
        solve_roll(Path{Straight{0}}, a, straight_end);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "нулевая длина оси — исключение");
}

static void test_segment_distance() {
    check(near(segment_distance({0, 0, 0}, {10, 0, 0}, {0, 5, 0}, {10, 5, 0}), 5.0), "параллельные отрезки: 5");
    check(near(segment_distance({0, 0, 0}, {10, 0, 0}, {15, 0, 0}, {20, 0, 0}), 5.0), "коллинеарные разделённые: 5");
    check(near(segment_distance({0, 0, 0}, {10, 0, 0}, {5, -5, 3}, {5, 5, 3}), 3.0), "скрещивающиеся: 3");
    check(near(segment_distance({0, 0, 0}, {1, 0, 0}, {2, 1, 0}, {2, 5, 0}), std::sqrt(2.0)), "конец к концу: √2");
    check(near(segment_distance({1, 1, 1}, {1, 1, 1}, {4, 5, 1}, {4, 5, 1}), 5.0), "две точки: 5");
    check(near(segment_distance({0, 0, 0}, {10, 0, 0}, {4, 0, 0}, {6, 0, 0}), 0.0), "вложенные коллинеарные: 0");

    const ClosestPoints sk = segment_closest({0, 0, 0}, {10, 0, 0}, {5, -5, 3}, {5, 5, 3});
    check(near(sk.distance, 3.0) && near3(sk.a, {5, 0, 0}) && near3(sk.b, {5, 0, 3}), "скрещивающиеся: ближайшие точки (5,0,0) и (5,0,3)");
    const ClosestPoints ee = segment_closest({0, 0, 0}, {1, 0, 0}, {2, 1, 0}, {2, 5, 0});
    check(near3(ee.a, {1, 0, 0}) && near3(ee.b, {2, 1, 0}), "конец к концу: точки (1,0,0) и (2,1,0)");

    const std::vector<Vec3> a{{0, 0, 0}, {10, 0, 0}, {10, 10, 0}};
    const std::vector<Vec3> b{{20, 10, 0}, {30, 10, 0}};
    check(near(polyline_distance(a, b), 10.0), "ломаные: 10");
    check(std::isinf(polyline_distance(a, {})), "пустая ломаная: бесконечность");
    check(near(polyline_distance({{0, 0, 0}}, {{3, 4, 0}}), 5.0), "ломаные из одной точки: 5");
}

static PlacedTube along_x(const std::string& id, double y, double z, double od = 20.0) {
    return PlacedTube{id, {{0, y, z}, {100, y, z}}, od};
}

static PlacedTube along_y(const std::string& id, double x, double z, double od = 20.0) {
    return PlacedTube{id, {{x, -50, z}, {x, 50, z}}, od};
}

static void test_clearance() {
    check(check_clearance({along_x("А", 0, 0), along_x("Б", 30, 0)}).empty(), "зазор 10 при норме 5 — без пометок");

    const auto gap = check_clearance({along_x("А", 0, 0), along_x("Б", 24, 0)});
    check(gap.size() == 1 && gap[0].kind == "mark_gap", "зазор 4 < 5 — mark_gap");

    const auto touch = check_clearance({along_x("А", 0, 0), along_x("Б", 20, 0)});
    check(touch.size() == 1 && touch[0].kind == "mark_gap", "касание — зазор, не пересечение");

    const auto cross_ = check_clearance({along_x("А", 0, 0), along_x("Б", 15, 0)});
    check(cross_.size() == 1 && cross_[0].kind == "mark_cross", "перекрытие 5 — mark_cross");

    check(check_clearance({along_x("А", 0, 0), along_x("Б", 24, 0)}, 3.0).empty(), "норма 3: зазор 4 допустим");
    check(check_clearance({along_x("А", 0, 0), along_x("Б", 15, 0)}, kDefaultGapMm, {{"Б", "А"}}).empty(),
          "пара в списке пропуска — без пометок (порядок id не важен)");

    const auto skew_cross = check_clearance({along_x("А", 0, 0), along_y("Б", 50, 19)});
    check(skew_cross.size() == 1 && skew_cross[0].kind == "mark_cross", "скрещивающиеся на 19 при ∅20: пересечение");
    const auto skew_gap = check_clearance({along_x("А", 0, 0), along_y("Б", 50, 22)});
    check(skew_gap.size() == 1 && skew_gap[0].kind == "mark_gap", "скрещивающиеся на 22: зазор 2");
    check(check_clearance({along_x("А", 0, 0), along_y("Б", 50, 40)}).empty(), "скрещивающиеся на 40: зазор 20");

    // Пометка знает, где стоять: ближайшие точки осей, зазор между поверхностями, норму и порядок труб.
    const auto marks = clearance_marks({along_x("А", 0, 0), along_y("Б", 50, 22)});
    check(marks.size() == 1 && marks[0].kind == "mark_gap" && marks[0].tube_a == "А" && marks[0].tube_b == "Б" &&
              near3(marks[0].point_a, {50, 0, 0}) && near3(marks[0].point_b, {50, 0, 22}) &&
              near(marks[0].surface_gap, 2.0) && near(marks[0].norm, kDefaultGapMm),
          "пометка зазора: точки (50,0,0) и (50,0,22), зазор 2, норма 5");
    const auto over = clearance_marks({along_x("А", 0, 0), along_x("Б", 15, 0)});
    check(over.size() == 1 && over[0].kind == "mark_cross" && near(over[0].surface_gap, -5.0) &&
              near(std::abs(over[0].point_b[1] - over[0].point_a[1]), 15.0),
          "пометка пересечения: зазор -5, оси на 15 мм друг от друга");
    check(clearance_text(over[0]).find("Пересечение") == 0 && clearance_text(marks[0]).find("Зазор") == 0, "тексты пометок");

    // Разные ∅: сумма радиусов 15.
    const auto mixed_cross = check_clearance({along_x("А", 0, 0, 20.0), along_x("Б", 14, 0, 10.0)});
    check(mixed_cross.size() == 1 && mixed_cross[0].kind == "mark_cross", "∅20 и ∅10 на 14: пересечение");
    const auto mixed_gap = check_clearance({along_x("А", 0, 0, 20.0), along_x("Б", 19, 0, 10.0)});
    check(mixed_gap.size() == 1 && mixed_gap[0].kind == "mark_gap", "∅20 и ∅10 на 19: зазор 4");

    // Три трубы: далёкая не даёт пометок, пара А–В даёт одну.
    const auto three = check_clearance({along_x("А", 0, 0), along_x("Б", 1000, 0), along_x("В", 22, 0)});
    check(three.size() == 1 && three[0].text.find("«А»") != std::string::npos &&
              three[0].text.find("«В»") != std::string::npos,
          "три трубы: одна пометка по паре А–В");

    // Пустая ось не падает и не даёт пометок.
    check(check_clearance({PlacedTube{"Пустая", {}, 20.0}, along_x("А", 0, 0)}).empty(), "пустая ось пропускается");
}

// Сквозной сценарий: поставить трубу на порт, подобрать поворот, проверить конец и зазор с соседней трубой.
static void test_scenario() {
    const Path path = bent_path();
    const Port a = origin_port();
    const Placement truth = place_from_port(path, a, 63.0);
    const Port b = manual_port(truth.end_pos, mul(truth.end_dir, -1.0), 20.0, "П2");

    const RollSolution sol = solve_roll(path, a, b);
    const Placement pl = place_from_port(path, a, sol.roll_deg);
    check(end_connection_issues(pl, b, path, 20.0, "Т1").empty(), "сценарий: конец Б сел на порт");

    // Сосед параллельно первому прямому участку, с противоположной от гиба стороны (гиб уходит в +y при повороте 63°):
    // на расстоянии 30 зазор 10 — пометок нет; на 22 зазор 2 — одна пометка.
    const PlacedTube t1{"Т1", pl.centerline, 20.0};
    check(check_clearance({t1, along_x("Сосед", -30, 0)}).empty(), "сценарий: сосед на 30 — без пометок");
    const auto near_issue = check_clearance({t1, along_x("Сосед", -22, 0)});
    check(near_issue.size() == 1 && near_issue[0].kind == "mark_gap", "сценарий: сосед на 22 — зазор 2");
}

int main() {
    test_frame();
    test_place();
    test_end_connection();
    test_solve_roll();
    test_segment_distance();
    test_clearance();
    test_scenario();
    if (failures == 0) std::printf("positioning: ok\n");
    return failures == 0 ? 0 : 1;
}
