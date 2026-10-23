// Вывод XYZ / LRA / таблицы гибов (3.5, 3.6, 7.6).
#include "bendcore/bend_output.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

using namespace bendcore;
using namespace bendcore::output;

static int failures = 0;
static void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}
static bool near(double a, double b, double eps = 1e-6) { return std::abs(a - b) <= eps; }
static bool near_vec(const Vec3& a, const Vec3& b, double eps = 1e-6) {
    return near(a[0], b[0], eps) && near(a[1], b[1], eps) && near(a[2], b[2], eps);
}
template <class F>
static bool throws(F f, const std::string& part = "") {
    try {
        f();
    } catch (const OutputError& e) {
        return part.empty() || std::string(e.what()).find(part) != std::string::npos;
    }
    return false;
}

// Пространственная труба: три гиба в разных плоскостях.
static Path spatial() {
    return Path{Straight{120}, Bend{40, 90, 0}, Straight{80}, Bend{40, 45, 90}, Straight{60},
                Bend{40, 60, -30}, Straight{150}};
}

// Попарные расстояния точек — инвариант жёсткого движения.
static bool congruent(const SharpPolyline& a, const SharpPolyline& b) {
    if (a.points.size() != b.points.size()) return false;
    for (std::size_t i = 0; i < a.points.size(); ++i)
        for (std::size_t j = i + 1; j < a.points.size(); ++j)
            if (!near(norm(sub(a.points[i], a.points[j])), norm(sub(b.points[i], b.points[j])), 1e-6)) return false;
    // Попарные расстояния не отличают зеркальное отражение: сверяем знак объёмов соседних четвёрок точек.
    auto vol = [](const std::vector<Vec3>& P, std::size_t i) {
        return dot(cross(sub(P[i + 1], P[i]), sub(P[i + 2], P[i])), sub(P[i + 3], P[i]));
    };
    for (std::size_t i = 0; i + 3 < a.points.size(); ++i)
        if (!near(vol(a.points, i), vol(b.points, i), 1e-3)) return false;
    return true;
}

static void test_simple_l() {
    const Path p{Straight{100}, Bend{50, 90}, Straight{100}};
    const auto poly = sharp_polyline(p);
    check(poly.points.size() == 3, "L-труба: 3 точки");
    check(near_vec(poly.points[1], {150, 0, 0}), "вершина гиба на 100 + T");
    check(near_vec(poly.points[2], {150, 150, 0}), "торец Б");
    const auto rows = lra(p);
    check(rows.size() == 2 && near(rows[0].L, 100) && near(rows[0].angle, 90) && near(rows[0].rotation, 0) &&
              near(rows[0].radius, 50),
          "LRA: L=100, R=0, A=90");
    check(rows[1].is_end && near(rows[1].L, 100), "LRA: последний участок 100");
}

static void test_roundtrip() {
    const Path p = spatial();
    const auto rows = lra(p);
    check(rows.size() == 4, "три гиба + конец");
    check(near(rows[0].L, 120) && near(rows[1].L, 80) && near(rows[2].L, 60) && near(rows[3].L, 150),
          "L совпадают с S");
    check(near(rows[1].rotation, 90) && near(rows[2].rotation, -30), "повороты совпадают с A пути");
    check(near(rows[1].angle, 45) && near(rows[2].angle, 60), "углы совпадают с B");
    const Path back = path_from_lra(rows);
    check(congruent(sharp_polyline(back), sharp_polyline(p)), "путь из LRA конгруэнтен исходному");
    check(near(axis_length(back), axis_length(p), 1e-9), "длина по оси сохраняется");
}

static void test_feed_b() {
    const Path p = spatial();
    const auto rb = lra(p, Feed::B);
    check(rb.size() == 4, "с конца Б: те же 4 строки");
    check(rb[0].bend_no == 3 && rb[1].bend_no == 2 && rb[2].bend_no == 1, "номера гибов закреплены по пути");
    check(near(rb[0].L, 150) && near(rb[1].L, 60) && near(rb[2].L, 80) && near(rb[3].L, 120),
          "участки переставлены");
    check(near(rb[0].angle, 60) && near(rb[2].angle, 90), "углы гибов те же");
    check(near(rb[0].rotation, 0), "первый поворот с конца Б = 0");
    // Труба из обратного LRA конгруэнтна исходной, развёрнутой концами.
    SharpPolyline rev = sharp_polyline(p);
    std::reverse(rev.points.begin(), rev.points.end());
    check(congruent(sharp_polyline(path_from_lra(rb)), rev), "повороты пересчитаны из геометрии");
}

static void test_insertion_depth() {
    const Path p{Straight{100}, Bend{50, 90}, Straight{100}};
    const auto rows = lra(p, Feed::A, 12, 8);
    check(near(rows[0].L, 112) && near(rows[1].L, 108), "E удлиняет концевые участки");
    const auto x = xyz(p, Feed::A, 12, 8);
    check(near_vec(x.front().p, {-12, 0, 0}) && near_vec(x.back().p, {150, 158, 0}), "XYZ торцов с E");
    check(throws([&] { (void)lra(p, Feed::A, -1, 0); }, "отрицательной"), "E < 0 — ошибка");
}

static void test_xyz_frame() {
    const Path p{Straight{100}, Bend{50, 90}, Straight{100}};
    Frame f;
    f.origin = {150, 0, 0};
    f.x = {0, 1, 0};
    f.y = {-1, 0, 0};
    f.z = {0, 0, 1};
    const auto rows = xyz(p, Feed::A, 0, 0, f);
    check(rows.size() == 3 && rows[0].label == "А" && rows[1].label == "1" && rows[2].label == "Б", "метки строк");
    check(near_vec(rows[0].p, {0, 150, 0}), "торец А в выбранной СК");
    check(near_vec(rows[2].p, {150, 0, 0}), "торец Б в выбранной СК");
    check(rows[1].radius && near(*rows[1].radius, 50) && !rows[0].radius, "R только у вершин");
    Frame bad = f;
    bad.y = {0.6, 0.8, 0};
    check(throws([&] { (void)xyz(p, Feed::A, 0, 0, bad); }, "перпендикуляр"), "неортогональная СК — ошибка");
    Frame left = f;
    left.y = {1, 0, 0};
    check(throws([&] { (void)xyz(p, Feed::A, 0, 0, left); }, "правой"), "левая СК — ошибка");
    const auto rb = xyz(p, Feed::B);
    check(rb.front().label == "Б" && rb.back().label == "А", "с конца Б метки в обратном порядке");
}

static void test_errors() {
    const Path sp{Straight{50}, Spiral{30, 10, 2, 1}, Straight{50}};
    check(throws([&] { (void)lra(sp); }, "спираль"), "спираль: LRA недоступен");
    const Path overlap{Straight{100}, Bend{50, 90}, Straight{-10}, Bend{50, 90}, Straight{100}};
    check(throws([&] { (void)lra(overlap); }), "S < 0 — ошибка");
    const Path degenerate{Straight{100}, Bend{50, 0.2}, Straight{100}};
    const auto rows = lra(degenerate);
    check(rows.size() == 1 && near(rows[0].L, 200), "вырожденный гиб — прямой участок");
}

static void test_bend_table() {
    const Path p = spatial();
    const auto t = bend_table(p);
    check(t.rows.size() == 3, "таблица: 3 гиба");
    check(near(t.rows[0].S, 120) && near(t.rows[1].A, 90) && near(t.rows[2].B, 60), "значения по пути");
    check(near(t.last_S, 150), "итоговая строка: последний участок");
    check(near(t.finished_length, axis_length(p), 1e-9), "длина готовой трубы = длине по оси");
    // Первый гиб с поворотом от исходной плоскости эскиза.
    const Path q{Straight{100}, Bend{50, 90, 30}, Straight{100}};
    check(near(bend_table(q).rows[0].A, 30), "A₁ от исходной плоскости (3.1)");
    // Гиб в другую сторону у профиля с одним направлением: поворот на 180°.
    const Path neg{Straight{100}, Bend{50, 90, 0}, Straight{50}, Bend{50, -45, 0}, Straight{100}};
    const auto tn = bend_table(neg);
    check(near(tn.rows[1].B, 45) && near(std::abs(tn.rows[1].A), 180), "B ≥ 0, A += 180°");
    const auto tb = bend_table(neg, Feed::A, Profile::Both, true);
    check(tb.rows[1].side == Side::Left && near(tb.rows[1].A, 0), "профиль «оба»: П/Л вместо поворота");
}

static void test_csv() {
    const Path p{Straight{100}, Bend{50, 90}, Straight{100}};
    const std::string c = lra_csv(lra(p), {';', 1});
    check(c == "Гиб;L;R;A;Радиус\n1;100.0;0.0;90.0;50.0\nКонец;100.0;;;\n", "LRA CSV");
    const std::string x = xyz_csv(xyz(p), {',', 2});
    check(x == "Точка,X,Y,Z,R\nА,0.00,0.00,0.00,\n1,150.00,0.00,0.00,50.00\nБ,150.00,150.00,0.00,\n", "XYZ CSV");
}

// Свойство: для случайных пространственных труб (гибы в обе стороны, любые повороты)
// LRA с конца А и с конца Б восстанавливают трубу, конгруэнтную исходной.
static void test_random_roundtrip() {
    unsigned seed = 12345;
    auto rnd = [&](double lo, double hi) {
        seed = seed * 1103515245u + 12345u;
        return lo + (hi - lo) * ((seed >> 8) & 0xFFFF) / 65535.0;
    };
    int bad = 0;
    for (int k = 0; k < 200; ++k) {
        Path p{Straight{rnd(20, 200)}};
        const int n = 1 + static_cast<int>(rnd(0, 6));
        for (int i = 0; i < n; ++i) {
            const double B = rnd(5, 170) * (rnd(0, 1) < 0.3 ? -1.0 : 1.0);
            p.push_back(Bend{rnd(20, 60), B, rnd(-180, 180)});
            p.push_back(Straight{rnd(10, 150)});
        }
        const auto fa = sharp_polyline(p);
        if (!congruent(sharp_polyline(path_from_lra(lra(p))), fa)) ++bad;
        SharpPolyline rev = fa;
        std::reverse(rev.points.begin(), rev.points.end());
        if (!congruent(sharp_polyline(path_from_lra(lra(p, Feed::B))), rev)) ++bad;
        if (std::abs(bend_table(p).finished_length - axis_length(p)) > 1e-6) ++bad;
    }
    check(bad == 0, "200 случайных труб: LRA с обоих концов восстанавливает форму");
}

int main() {
    test_random_roundtrip();
    test_simple_l();
    test_roundtrip();
    test_feed_b();
    test_insertion_depth();
    test_xyz_frame();
    test_errors();
    test_bend_table();
    test_csv();
    if (failures) {
        std::printf("bend_output: %d FAIL\n", failures);
        return 1;
    }
    std::printf("bend_output: OK\n");
    return 0;
}
