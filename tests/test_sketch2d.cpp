// Тесты 2D-эскиза (7.9) поверх PlaneGCS: степени свободы, определённость, конфликт, избыточность.
#include "bendcore/sketch2d.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

using namespace bendcore::sketch2d;

static int failures = 0;

static void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}

static bool has(const std::vector<Id>& v, Id x) {
    return std::find(v.begin(), v.end(), x) != v.end();
}

static void test_square_is_determined() {
    Sketch s;
    Id p0 = s.add_point(0, 0);
    Id p1 = s.add_point(10.5, 0.4);
    Id p2 = s.add_point(10.2, 9.8);
    Id p3 = s.add_point(0.3, 10.1);
    s.fix_point(p0);
    s.constrain_horizontal(p0, p1);
    s.constrain_vertical(p1, p2);
    s.constrain_horizontal(p2, p3);
    s.constrain_vertical(p3, p0);
    s.constrain_distance(p0, p1, 10);
    s.constrain_distance(p1, p2, 10);
    Report r = s.solve();
    check(r.status == "определено" && r.dof == 0, "квадрат: определено, dof = 0");
    Point2 q1 = s.point(p1), q2 = s.point(p2), q3 = s.point(p3);
    check(std::abs(q1.x - 10) < 1e-6 && std::abs(q1.y) < 1e-6, "квадрат: p1 = (10, 0)");
    check(std::abs(q2.x - 10) < 1e-6 && std::abs(q2.y - 10) < 1e-6, "квадрат: p2 = (10, 10)");
    check(std::abs(q3.x) < 1e-6 && std::abs(q3.y - 10) < 1e-6, "квадрат: p3 = (0, 10)");
}

static void test_free_segment_dof() {
    Sketch s;
    Id a = s.add_point(0, 0);
    Id b = s.add_point(5, 5);
    s.add_line(a, b);
    Report r = s.solve();
    check(r.status == "недоопределено" && r.dof == 4, "два свободных конца: dof = 4");
    s.fix_point(a);
    check(s.solve().dof == 2, "закрепление одной точки: dof = 2");
}

static void test_conflict_reported_by_tag() {
    Sketch s;
    Id a = s.add_point(0, 0);
    Id b = s.add_point(5, 0);
    s.fix_point(a);
    s.fix_point(b);
    Id d = s.constrain_distance(a, b, 10);
    Report r = s.solve();
    check(r.status == "конфликт", "размер против закреплений: конфликт");
    check(has(r.conflicting, d), "конфликт: размер в списке");
}

static void test_redundant_reported() {
    Sketch s;
    Id a = s.add_point(0, 0);
    Id b = s.add_point(10, 0.2);
    s.fix_point(a);
    s.constrain_horizontal(a, b);
    Id h2 = s.constrain_horizontal(a, b);
    Report r = s.solve();
    check(has(r.redundant, h2) && r.status == "избыточно", "повтор горизонтали: избыточно");
    s.remove_constraint(h2);
    Report after = s.solve();
    check(after.status == "недоопределено" && after.dof == 1, "после удаления лишнего: одна степень свободы (x точки b)");
}

static void test_circle_radius() {
    Sketch s;
    Id c = s.add_point(3, 4);
    Id cir = s.add_circle(c, 2.0);
    s.fix_point(c);
    s.constrain_radius(cir, 5.0);
    Report r = s.solve();
    check(r.status == "определено" && r.dof == 0, "окружность: радиус задан, определено");
    check(std::abs(s.circle_radius(cir) - 5.0) < 1e-6, "окружность: радиус = 5");
}

static void test_parallel_and_perpendicular() {
    Sketch s;
    Id a = s.add_point(0, 0), b = s.add_point(10, 0);
    s.fix_point(a);
    s.fix_point(b);
    Id l1 = s.add_line(a, b);
    Id c = s.add_point(0, 5);
    Id d = s.add_point(10, 7);
    s.fix_point(c);
    Id l2 = s.add_line(c, d);
    s.constrain_parallel(l1, l2);
    Report r = s.solve();
    check(r.status == "недоопределено" && r.dof == 1, "параллельность: одна степень свободы у d");
    check(std::abs(s.point(d).y - 5) < 1e-6, "параллельность: d.y = 5");

    Sketch q;
    Id qa = q.add_point(0, 0), qb = q.add_point(10, 0);
    q.fix_point(qa);
    q.fix_point(qb);
    Id m1 = q.add_line(qa, qb);
    Id qc = q.add_point(0, 5);
    Id qd = q.add_point(10, 7);
    q.fix_point(qc);
    Id m2 = q.add_line(qc, qd);
    q.constrain_perpendicular(m1, m2);
    q.solve();
    check(std::abs(q.point(qd).x - 0) < 1e-6, "перпендикулярность: d.x = 0");
}

static void test_equal_length_and_angle() {
    Sketch s;
    Id a = s.add_point(0, 0), b = s.add_point(10, 0);
    s.fix_point(a);
    s.fix_point(b);
    Id l1 = s.add_line(a, b);
    Id c = s.add_point(0, 5);
    Id d = s.add_point(3, 9);
    s.fix_point(c);
    Id l2 = s.add_line(c, d);
    s.constrain_equal_length(l1, l2);
    s.solve();
    Point2 q = s.point(d);
    check(std::abs(std::hypot(q.x - 0, q.y - 5) - 10) < 1e-6, "равенство длин: |cd| = 10");

    Sketch t;
    Id ta = t.add_point(0, 0), tb = t.add_point(10, 0);
    t.fix_point(ta);
    t.fix_point(tb);
    Id m1 = t.add_line(ta, tb);
    Id tc = t.add_point(0, 0);
    Id td = t.add_point(9, 5.5);
    t.fix_point(tc);
    Id m2 = t.add_line(tc, td);
    t.constrain_angle(m1, m2, 30.0);
    Report r = t.solve();
    Point2 e = t.point(td);
    double ang = std::atan2(e.y, e.x) * 180.0 / std::acos(-1.0);
    check(r.status == "недоопределено" && r.dof == 1 && std::abs(ang - 30.0) < 1e-6, "угол 30°: d на луче под 30°, длина свободна");
}

static void test_point_on_line_and_symmetry() {
    Sketch s;
    Id a = s.add_point(0, 0), b = s.add_point(10, 0);
    s.fix_point(a);
    s.fix_point(b);
    Id l = s.add_line(a, b);
    Id p = s.add_point(5, 7);
    s.constrain_point_on_line(p, l);
    Report r = s.solve();
    check(r.status == "недоопределено" && r.dof == 1, "точка на отрезке: одна степень свободы");
    check(std::abs(s.point(p).y) < 1e-6, "точка на отрезке: y = 0");

    Sketch t;
    Id ax1 = t.add_point(0, 0), ax2 = t.add_point(0, 10);
    t.fix_point(ax1);
    t.fix_point(ax2);
    Id axis = t.add_line(ax1, ax2);
    Id p1 = t.add_point(3, 4);
    t.fix_point(p1);
    Id p2 = t.add_point(-5, 9);
    t.constrain_symmetric(p1, p2, axis);
    t.solve();
    check(std::abs(t.point(p2).x + 3) < 1e-6 && std::abs(t.point(p2).y - 4) < 1e-6,
          "симметрия относительно оси: p2 = (-3, 4)");
}

static void test_point_on_circle_and_equal_radius() {
    Sketch s;
    Id c1 = s.add_point(0, 0);
    s.fix_point(c1);
    Id k1 = s.add_circle(c1, 5.0);
    Id c2 = s.add_point(10, 0);
    s.fix_point(c2);
    Id k2 = s.add_circle(c2, 3.0);
    s.constrain_equal_radius(k1, k2);
    s.constrain_radius(k1, 5.0);
    Report r = s.solve();
    check(r.status == "определено" && std::abs(s.circle_radius(k2) - 5.0) < 1e-6,
          "равный радиус: второй радиус стал 5");

    Sketch t;
    Id o = t.add_point(0, 0);
    t.fix_point(o);
    Id k = t.add_circle(o, 5.0);
    Id q = t.add_point(4, 2);
    t.constrain_point_on_circle(q, k);
    t.constrain_radius(k, 5.0);
    Report rt = t.solve();
    Point2 qq = t.point(q);
    check(std::abs(std::hypot(qq.x, qq.y) - 5.0) < 1e-6, "точка на окружности: |q| = 5");
    check(rt.status != "конфликт", "точка на окружности: без конфликта");
}

int main() {
    test_square_is_determined();
    test_free_segment_dof();
    test_conflict_reported_by_tag();
    test_redundant_reported();
    test_circle_radius();
    test_parallel_and_perpendicular();
    test_equal_length_and_angle();
    test_point_on_line_and_symmetry();
    test_point_on_circle_and_equal_radius();
    if (failures == 0) std::printf("OK sketch2d\n");
    return failures == 0 ? 0 : 1;
}
