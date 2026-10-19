// Эталонные значения получены из Python-реализации tubegeom.py (18.10.2026, 21:00).
#include "bendcore/spiral.hpp"
#include "bendcore/tubegeom.hpp"

#include <cmath>
#include <optional>
#include <cstdio>
#include <string>
#include <stdexcept>

using namespace bendcore;

static int failures = 0;

static void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}

static bool near(double a, double b, double eps = 1e-9) { return std::abs(a - b) <= eps; }

static bool near_vec(const Vec3& a, const Vec3& b, double eps = 1e-9) {
    return near(a[0], b[0], eps) && near(a[1], b[1], eps) && near(a[2], b[2], eps);
}

static void test_tangent_length() {
    check(near(tangent_length(50, 90), 50.0, 1e-9), "tangent_length(50, 90) = 50");
}

static void test_axis_length() {
    const Path p{Straight{100}, Bend{50, 90}, Straight{100}};
    check(near(axis_length(p), 278.53981633974485, 1e-9), "axis_length: 200 + 50·π/2");
    const Path degenerate{Straight{100}, Bend{50, 0.3}, Straight{100}};
    check(near(axis_length(degenerate), 200.0, 1e-12), "вырожденный гиб не добавляет длины");
}

static void test_table_row() {
    // A = 170° при «Минимизировать поворот» → A − 180° = −10°, гиб в другую сторону
    const TableRow r1 = table_row(Bend{50, 90, 170}, Profile::Both, true);
    check(near(r1.A, -10.0) && near(r1.B, 90.0) && r1.side == Side::Left, "table_row: A 170 → −10, Л");

    // профиль «право»: гиб в другую сторону даёт поворот +180°
    const TableRow r2 = table_row(Bend{50, -90, 30}, Profile::Right, true);
    check(near(r2.A, -150.0) && near(r2.B, 90.0) && r2.side == Side::None, "table_row: профиль право, B < 0");

    // A = −120° → +60°, гиб в основную сторону (П)
    const TableRow r3 = table_row(Bend{40, -30, -120}, Profile::Both, true);
    check(near(r3.A, 60.0) && near(r3.B, 30.0) && r3.side == Side::Right, "table_row: A −120 → 60, П (B < 0 → Л до минимизации)");
}

static void test_normalize_rotation() {
    const Bend n = normalize_rotation(Bend{50, 45, 120}, Profile::Both);
    check(near(n.A, -60.0) && near(n.B, -45.0), "normalize_rotation: A > 90 → A − 180, B меняет знак");
    const Bend same = normalize_rotation(Bend{50, 45, 120}, Profile::Right);
    check(near(same.A, 120.0) && near(same.B, 45.0), "normalize_rotation: профиль с одним направлением не меняет гиб");
}

static void test_trace_endpoints() {
    // эталон Python: [S100, B(50, 90), S100] → конец (150, 150, 0)
    const auto pts = trace(Path{Straight{100}, Bend{50, 90}, Straight{100}}, 64);
    check(near_vec(pts.back(), Vec3{150.0, 150.0, 0.0}, 1e-9), "trace: конец простого гиба (150, 150, 0)");

    // эталон Python (steps_per_bend = 8): [S80, B(40, −60, A45), S50, B(30, 45, A−20), S70]
    const auto pts2 = trace(Path{Straight{80}, Bend{40, -60, 45}, Straight{50}, Bend{30, 45, -20}, Straight{70}}, 8);
    check(pts2.size() == 15, "trace: число точек при 8 шагах на гиб");
    check(near_vec(pts2.back(), Vec3{222.42796001102738, -54.60241027810077, -82.79390177000602}, 1e-9),
          "trace: конец пространственного пути с поворотами A");
}

static void test_trace_rejects_negative_straight() {
    bool thrown = false;
    try {
        trace(Path{Straight{-1.0}});
    } catch (const std::invalid_argument&) {
        thrown = true;
    }
    check(thrown, "trace: S < 0 — исключение");
}

static void test_spiral_length_and_trace() {
    // эталон Python: spiral_axis_length(30, 10, 2.5) = 471.90157768809223
    const Path p{Straight{50}, Spiral{30, 10, 2.5, +1}, Straight{40}};
    check(near(axis_length(p), 561.9015776880922, 1e-9), "axis_length с спиралью: 50 + L_спир + 40");
    check(near(spiral_axis_length(30, 10, 2.5), 471.90157768809223, 1e-9), "spiral_axis_length");

    // эталон Python: ось спирали по направлению (1,0,0) и верху (0,0,1)
    const Vec3 a = spiral_axis(Vec3{1, 0, 0}, 30, 10, +1, Vec3{0, 0, 1});
    check(near_vec(a, Vec3{0.05297714858780147, 0.0, 0.9985957248694318}, 1e-9), "spiral_axis");

    // эталон Python (steps_per_turn = 8): конец и касательная отдельно от трубы
    const auto sc = spiral_centreline(Vec3{0, 0, 0}, Vec3{1, 0, 0}, 30, 10, 2.5, +1, a, 8);
    check(sc.pts.size() == 21, "spiral_centreline: 21 точка при 8 шагах на виток");
    check(near_vec(sc.pts.back(), Vec3{1.324428714695055, 60.0, 24.964893121735795}, 1e-9), "spiral_centreline: конец");
    check(near_vec(sc.tend, Vec3{-0.994386843455012, 6.114635290517903e-16, 0.10580550819110242}, 1e-9),
          "spiral_centreline: касательная в конце");

    // эталон Python: полная труба [S50, Spiral, S40] при 8 шагах на виток
    const auto pts = trace(p, 8);
    check(pts.size() == 23, "trace со спиралью: 23 точки");
    check(near_vec(pts.back(), Vec3{11.548954976494578, 60.00000000000002, 29.19711344937989}, 1e-9),
          "trace со спиралью: конец");
}

static void test_spiral_tangency_error() {
    bool thrown = false;
    try {
        // ось, не касающаяся входящей прямой
        spiral_centreline(Vec3{0, 0, 0}, Vec3{1, 0, 0}, 30, 10, 2.5, +1, Vec3{0, 0, 1}, 8);
    } catch (const std::invalid_argument&) {
        thrown = true;
    }
    check(thrown, "spiral_centreline: ось не касается прямой — ошибка");
}

static void test_validate() {
    auto kinds = [](const std::vector<Issue>& v) {
        std::string out;
        for (const auto& x : v) out += x.kind + "@" + std::to_string(x.index) + ";";
        return out;
    };
    // эталоны Python: validate
    check(kinds(validate(Path{Straight{100}, Bend{50, 90}, Straight{100}})).empty(), "validate: чистый путь");
    check(kinds(validate(Path{Straight{-5}, Bend{50, 90}, Straight{100}})) == "geometric@0;", "validate: S < 0");
    check(kinds(validate(Path{Straight{100}, Straight{20}, Bend{50, 90}, Straight{100}})) == "suggestion@1;",
          "validate: коллинеарные участки");
    check(kinds(validate(Path{Straight{100}, Bend{50, 0.3}, Straight{100}})) == "warning@1;suggestion@1;suggestion@2;",
          "validate: вырожденный гиб");
    check(kinds(validate(Path{Straight{100}, Bend{50, 120}, Straight{100}}, 90.0)) == "technological@1;",
          "validate: B > B_max");
    check(kinds(validate(Path{Straight{100}, Bend{50, 90}, Bend{50, 90, 30}, Straight{100}})) == "geometric@2;",
          "validate: A при нулевом участке между гибами");
    check(kinds(validate(Path{Straight{100}, Spiral{30, 5, 2, +1}, Straight{50}}, 180.0, 0.0, 20.0, 8.0)) ==
              "technological@1;technological@1;technological@-1;", "validate: шаг спирали");
    check(kinds(validate(Path{Straight{100}, Spiral{30, 10, 2, +1}}, 180.0, 0.0, std::nullopt, std::nullopt, false)) ==
              "technological@1;", "validate: навивка без разрешения");
    check(kinds(validate(Path{Straight{100}, Bend{20, 180}, Straight{20}, Bend{20, 180}, Straight{100}}, 180.0, 0.0, 30.0)) ==
              "technological@-1;", "validate: самопересечение");
}

static void test_self_intersections() {
    const auto hits = self_intersections(Path{Straight{100}, Bend{20, 180}, Straight{20}, Bend{20, 180}, Straight{100}}, 30.0);
    check(!hits.empty(), "self_intersections: есть пересечение");
    if (!hits.empty()) {
        check(near(std::get<2>(hits.front()), 28.59249977808081, 1e-6), "self_intersections: расстояние");
    }
    check(self_intersections(Path{Straight{100}, Bend{50, 90}, Straight{100}}, 20.0).empty(), "self_intersections: чистый путь");
}

static void test_manual_angle_and_table() {
    check(near(manual_angle_tolerance(45, {30, 45, 60}, {90}), 0.5), "manual_angle_tolerance: риска");
    check(near(manual_angle_tolerance(90, {30, 45, 60}, {90}), 0.2), "manual_angle_tolerance: упор");
    check(near(manual_angle_tolerance(50, {30, 45, 60}, {90}), 2.0), "manual_angle_tolerance: между рисками");

    const Path p = path_from_table({{100, 50, 90, 0}, {80, 40, 60, 20}, {50, 0, 0, 0}});
    check(p.size() == 5, "path_from_table: 5 элементов, последний прямой без гиба");
}

static void test_end_drift() {
    // эталоны Python
    const Path p{Straight{100}, Bend{50, 90}, Straight{100}, Bend{40, 60, 20}, Straight{80}};
    const Drift d = end_drift(p, {0.5, 0.5}, 0.5, 0.3);
    check(near(d.worst_pos, 4.012141152, 1e-6), "end_drift: худший случай, положение");
    check(near(d.worst_dir, 1.4105759610332662, 1e-6), "end_drift: худший случай, направление");
    check(near(d.probable_pos, 1.9739444, 1e-6), "end_drift: вероятный, положение");
    check(near(d.probable_dir, 0.8158226930408174, 1e-6), "end_drift: вероятный, направление");
    check(!d.warning, "end_drift: предупреждения нет");
    check(!d.shares.empty() && std::get<0>(d.shares.front()) == 1 && std::get<1>(d.shares.front()) == "B" &&
              near(std::get<2>(d.shares.front()), 0.4513982064406422, 1e-6),
          "end_drift: главный вклад — B первого гиба");
}

int main() {
    test_tangent_length();
    test_axis_length();
    test_table_row();
    test_normalize_rotation();
    test_trace_endpoints();
    test_trace_rejects_negative_straight();
    test_spiral_length_and_trace();
    test_spiral_tangency_error();
    test_validate();
    test_self_intersections();
    test_manual_angle_and_table();
    test_end_drift();
    if (failures == 0) std::printf("OK tubegeom\n");
    return failures == 0 ? 0 : 1;
}
