// Сценарии test_sketch.py, порт на C++ (19.10.2026). Допуски — как в Python-тестах.
#include "bendcore/sketch.hpp"

#include <cmath>
#include <cstdio>
#include <stdexcept>

using namespace bendcore;
using namespace bendcore::sketch;

static int failures = 0;

static void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}

static Constraint fix(int i, Vec3 target, std::string label = "") {
    Constraint c;
    c.kind = "fix";
    c.idx = {i};
    c.target = target;
    c.label = label;
    return c;
}

static Constraint coincide(int i, Vec3 target, std::string label = "") {
    Constraint c = fix(i, target, label);
    c.kind = "coincide";
    return c;
}

static Constraint dist(int a, int b, double value, std::string label = "", std::string source = "explicit") {
    Constraint c;
    c.kind = "dist";
    c.idx = {a, b};
    c.value = value;
    c.label = label;
    c.source = source;
    return c;
}

static Constraint with_normal(Constraint c, Vec3 n) {
    c.normal = n;
    return c;
}

static Constraint with_origin(Constraint c, Vec3 o) {
    c.origin = o;
    return c;
}

static double len(const Vec3& a, const Vec3& b) { return norm(sub(a, b)); }

static void test_distance_and_fix_solves() {
    auto r = solve({{0, 0, 0}, {100, 3, 0}},
                   {fix(0, {0, 0, 0}, "фикс"), dist(0, 1, 50.0, "L")});
    check(r.status == "определено" || r.status == "недоопределено", "distance+fix: статус");
    check(std::abs(len(r.points[1], r.points[0]) - 50) < 1e-6, "distance+fix: длина 50");
}

static void test_underdetermined_reports_dof() {
    auto r = solve({{0, 0, 0}, {10, 0, 0}}, {dist(0, 1, 20.0)});
    check(r.status == "недоопределено" && r.dof > 0, "недоопределено: dof > 0");
}

static void test_conflict_detected() {
    auto r = solve({{0, 0, 0}, {10, 0, 0}},
                   {fix(0, {0, 0, 0}, "A"), fix(1, {10, 0, 0}, "B"), dist(0, 1, 20.0, "L20")});
    check(r.status == "конфликт", "конфликт: статус");
    bool has = false;
    for (const auto& s : r.conflicts) has = has || s == "L20";
    check(has, "конфликт: L20 в списке");
}

static void test_coincide_with_feature_and_plane_distance() {
    auto plane = with_origin(with_normal(Constraint{}, {0, 0, 1}), {0, 0, 0});
    plane.kind = "plane_dist";
    plane.idx = {1};
    plane.value = 30.0;
    plane.label = "до плоскости";
    auto r = solve({{1, 1, 1}, {50, 50, 50}}, {coincide(0, {0, 0, 0}, "порт"), plane});
    check(norm(sub(r.points[0], Vec3{0, 0, 0})) < 1e-6, "coincide: точка в начале");
    check(std::abs(r.points[1][2] - 30) < 1e-6, "plane_dist: z = 30");
}

static void test_perp_and_parallel() {
    Constraint par;
    par.kind = "parallel";
    par.idx = {0, 1};
    par.normal = Vec3{1, 0, 0};
    par.label = "∥X";
    auto r = solve({{0, 0, 0}, {10, 5, 0}}, {par, fix(0, {0, 0, 0}, "f")});
    check(std::abs(r.points[1][1]) < 1e-6, "parallel: dy = 0");
}

static void test_snap_priority_port_first() {
    auto a = snap_point({1, 0, 0}, {{0, 0, 0}}, {{1, 0, 0}}, 5);
    check(a.kind == "порт" && a.point == Vec3{0, 0, 0}, "snap: порт первым");
    auto b = snap_point({100, 0, 0}, {{0, 0, 0}}, {{1, 0, 0}}, 5);
    check(b.kind == "свободно", "snap: вне радиуса — свободно");
}

static void test_ortho_step_moves_one_axis() {
    check(ortho_step({0, 0, 0}, {10, 3, 0}) == Vec3{10, 0, 0}, "ortho_step: одна ось");
}

static void test_snap_angle_to_profile_values() {
    check(snap_angle(44.0, {30, 45, 60, 90}) == 45, "snap_angle: 44 → 45");
}

static void test_snap_priority_tiers_1_2() {
    Vec3 t{10.2, 0.1, 0};
    check(snap_point(t, {}, {{10.3, 0, 0}}, 5.0, {{10, 0, 0}}).kind == "вершина", "1.2: вершина");
    check(snap_point(t, {}, {{10.3, 0, 0}}, 5.0, {}, {Guide{{0, 0, 0}, {1, 0, 0}}}).kind == "признак",
          "1.2: признак");
    auto g = snap_point(t, {}, {}, 5.0, {}, {Guide{{0, 0, 0}, {1, 0, 0}}});
    check(g.kind == "направляющая" && std::abs(g.point[1]) < 1e-12, "1.2: направляющая");
    auto s = snap_point({9.8, 0.3, 0}, {}, {}, 1.0, {},
                        {}, Grid{{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, 5.0});
    check(s.kind == "сетка" && s.point[0] == 10.0 && s.point[1] == 0.0, "1.2: сетка");
    check(snap_point({50, 50, 0}, {{0, 0, 0}}).kind == "свободно", "1.2: свободно");
}

static void test_bend_angle_snapping_manual_and_mechanized() {
    const std::vector<double> pref{15, 22.5, 30, 45, 60, 67.5, 75, 90};
    auto m = snap_bend_angle(41, true, pref, pref);
    check(m.angle == 45 && !m.warning, "гиб вручную: жёсткое притяжение к 45");
    auto t = snap_bend_angle(41, true, pref, pref, {}, std::nullopt, true);
    check(t.angle == 41 && t.warning && *t.warning == kScaleWarning, "гиб вручную, ввод: предупреждение");
    auto far = snap_bend_angle(41, false, pref);
    check(far.angle == 41 && !far.warning, "механизированный: далеко — свободно");
    check(snap_bend_angle(44, false, pref).angle == 45, "механизированный: мягко к 45");
    check(snap_bend_angle(37.3, false, {}, {}, {}, 2.5).angle == 37.5, "механизированный: шаг 2,5");
    check(snap_bend_angle(37.3, false).angle == 37.3, "без шага — свободно");
}

static void test_straight_S_dimension_accounts_for_tangents() {
    const double R = 20.0;
    std::vector<Vec3> pts{{0, 0, 0}, {100, 0, 0}, {100, 100, 0}};
    Constraint S;
    S.kind = "straight_S";
    S.idx = {-1, 0, 1, 2};
    S.value = 0;
    S.R = R;
    check(std::abs(measure(pts, S) - 80.0) < 1e-9, "straight_S: 80 при R = 20");

    Constraint S1 = S;
    S1.value = 60.0;
    S1.label = "S1";
    Constraint plane = with_origin(with_normal(Constraint{}, {0, 0, 1}), {0, 0, 0});
    plane.kind = "plane_dist";
    plane.idx = {1};
    plane.value = 0.0;
    auto r = solve(pts, {fix(0, {0, 0, 0}), fix(2, {100, 100, 0}), S1, plane});
    check(r.status != "конфликт", "straight_S: без конфликта");
    check(std::abs(measure(r.points, S1) - 60.0) < 1e-6, "straight_S: решение S = 60");
}

static void test_bend_angle_coplanar_symmetry_line_dist() {
    std::vector<Vec3> P{{0, 0, 0}, {10, 0, 0}, {10, 10, 0}, {0, 10, 0.5}};
    Constraint ang;
    ang.kind = "bend_angle";
    ang.idx = {0, 1, 2};
    check(std::abs(measure(P, ang) - 90) < 1e-9, "bend_angle: 90°");

    Constraint cop;
    cop.kind = "coplanar";
    cop.idx = {0, 1, 2, 3};
    auto r = solve(P, {cop});
    const Vec3 nrm = cross(sub(r.points[1], r.points[0]), sub(r.points[2], r.points[0]));
    const Vec3 nu = mul(nrm, 1.0 / norm(nrm));
    check(r.status != "конфликт" && std::abs(dot(sub(r.points[3], r.points[0]), nu)) < 1e-6,
          "coplanar: четвёртая точка в плоскости");

    Constraint sym = with_origin(with_normal(Constraint{}, {1, 0, 0}), {0, 0, 0});
    sym.kind = "symmetry";
    sym.idx = {0, 1};
    auto s = solve({{1, 1, 0}, {3, 2, 0}}, {sym});
    const Vec3 a = s.points[0], b = s.points[1];
    check(std::abs(a[0] + b[0]) < 1e-6 && std::abs(a[1] - b[1]) < 1e-6, "symmetry: относительно плоскости");

    Constraint ld = with_origin(with_normal(Constraint{}, {0, 0, 1}), {0, 0, 0});
    ld.kind = "line_dist";
    ld.idx = {0};
    ld.value = 5.0;
    auto l = solve({{3, 0, 7}}, {ld});
    check(std::abs(measure(l.points, ld) - 5.0) < 1e-6, "line_dist: 5 мм до оси");
}

static void test_reference_dimension_and_conflict_dialog() {
    std::vector<Constraint> cons{fix(0, {0, 0, 0}, "F"), fix(1, {10, 0, 0}, "F2"),
                                 dist(0, 1, 12.0, "d0", "user_dim")};
    auto r = solve({{0, 0, 0}, {10, 0, 0}}, cons);
    check(r.status == "конфликт", "диалог: конфликт");
    bool has = false;
    for (const auto& s : r.conflicts) has = has || s == "d0";
    check(has, "диалог: d0 в конфликтах");

    auto cons2 = make_reference(cons, "d0");
    auto r2 = solve({{0, 0, 0}, {10, 0, 0}}, cons2);
    check(r2.status == "определено", "справочный: определено");
    check(r2.reference_values.count("d0") && std::abs(r2.reference_values["d0"] - 10.0) < 1e-6,
          "справочный: значение 10");
    check(delete_constraint(cons, "d0").size() == 2, "удалить: два ограничения остались");

    bool threw = false;
    try {
        make_reference(cons, "F");
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "справочный: фиксацию нельзя сделать справочной");
}

static void test_drag_priority_user_dimension_beats_auto_constraint() {
    std::vector<Constraint> cons{fix(0, {0, 0, 0}, ""), dist(0, 1, 10.0, "d0", "user_dim"),
                                 dist(0, 1, 12.0, "auto", "auto")};
    auto r = solve({{0, 0, 0}, {11, 0, 0}}, cons, true);
    const double d = len(r.points[1], r.points[0]);
    check(std::abs(d - 10.0) < 0.05, "перетаскивание: размер пользователя побеждает авто");
}

static void test_entry_arc_conflict_18_10() {
    auto ok = entry_arc_to_vertex(50.05, 50.0);
    check(ok.status == "ok" && ok.R && *ok.R == 50.0 && ok.report.find("отличается") != std::string::npos,
          "дуга на входе: в допуске");
    auto bad = entry_arc_to_vertex(60.0, 50.0);
    check(bad.status == "conflict" && bad.default_option == "keep_body" && !bad.R,
          "дуга на входе: конфликт, по умолчанию оставить тело");
}

int main() {
    test_distance_and_fix_solves();
    test_underdetermined_reports_dof();
    test_conflict_detected();
    test_coincide_with_feature_and_plane_distance();
    test_perp_and_parallel();
    test_snap_priority_port_first();
    test_ortho_step_moves_one_axis();
    test_snap_angle_to_profile_values();
    test_snap_priority_tiers_1_2();
    test_bend_angle_snapping_manual_and_mechanized();
    test_straight_S_dimension_accounts_for_tangents();
    test_bend_angle_coplanar_symmetry_line_dist();
    test_reference_dimension_and_conflict_dialog();
    test_drag_priority_user_dimension_beats_auto_constraint();
    test_entry_arc_conflict_18_10();
    if (failures == 0) std::printf("OK sketch\n");
    return failures == 0 ? 0 : 1;
}
