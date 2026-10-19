import math
import numpy as np
from tubegeom import (Straight, Bend, Spiral, tangent_length, spiral_length, axis_length,
                      normalize_rotation, trace, polyline_length, validate, path_from_table)


def close(a, b, tol=1e-6):
    assert abs(a - b) <= tol, f"{a} != {b} (tol {tol})"


def test_tangent_length():
    close(tangent_length(50, 90), 50.0, 1e-9)            # tan(45°) = 1
    close(tangent_length(40, 60), 40 * math.tan(math.radians(30)), 1e-9)


def test_axis_length_formula():
    path = [Straight(100), Bend(50, 90), Straight(200), Bend(50, 90), Straight(100)]
    close(axis_length(path), 100 + 50 * math.pi / 2 + 200 + 50 * math.pi / 2 + 100, 1e-9)


def test_degenerate_bend_is_straight():
    close(axis_length([Straight(100), Bend(50, 0.3), Straight(100)]), 200, 1e-9)


def test_spiral_length():
    R, p, n = 20.0, 10.0, 5
    close(spiral_length(R, p, n), n * math.sqrt((2 * math.pi * R) ** 2 + p ** 2), 1e-9)


def test_trace_length_matches_formula():
    path = [Straight(100), Bend(50, 90, 0), Straight(200), Bend(50, 60, 30), Straight(80), Bend(40, -45, 0)]
    pts = trace(path, steps_per_bend=2000)
    close(polyline_length(pts), axis_length(path), 1e-2)


def test_tangent_vertex_geometry():
    # одна гибка: вершина V лежит на касательных; конец дуги = V + T·(новое направление)
    S0, R, B = 100.0, 50.0, 60.0
    path = [Straight(S0), Bend(R, B), Straight(0.0)]
    pts = trace(path, steps_per_bend=4000)
    start_bend = np.array([S0, 0, 0])
    V = np.array([S0 + tangent_length(R, B), 0, 0])
    T = tangent_length(R, B)
    end = pts[-1]
    # конец дуги находится на расстоянии T от вершины V
    close(np.linalg.norm(end - V), T, 1e-2)
    # угол между хордой от начала дуги и касательной — B/2 (проверка по геометрии)
    dirv = np.array([math.cos(math.radians(B)), math.sin(math.radians(B)), 0])
    close(float(np.dot(end - V, dirv) / T), 1.0, 1e-3)


def test_u_turn_returns_parallel():
    # две гибки 180° в одной плоскости (A = 0) — U-образная петля, конец параллелен началу
    path = [Straight(100), Bend(50, 180), Straight(100)]
    pts = trace(path, steps_per_bend=4000)
    end_dir = pts[-1] - pts[-2]
    assert end_dir[0] < 0 and abs(end_dir[1]) < 1e-6
    close(pts[-1][1], 100.0, 1e-2)                       # смещение по Y = 2R


def test_square_loop_closes():
    # четыре гибки по 90°, одна плоскость: квадрат из прямых и дуг
    S, R = 100.0, 20.0
    path = []
    for _ in range(4):
        path += [Straight(S), Bend(R, 90)]
    path.append(Straight(0))
    pts = trace(path, steps_per_bend=2000)
    # за квадрат труба возвращается в начало по направлению (с учётом смещения на R в углах)
    assert np.linalg.norm(pts[-1][:2] - np.array([0.0, 0.0])) < S + 2 * R


def test_normalize_rotation_flips_bend():
    b = Bend(50, 60, 170)
    nb = normalize_rotation(b)
    close(nb.A, -10, 1e-9)
    close(nb.B, -60, 1e-9)
    # оставляем в пределах [-90, 90] без изменений
    assert normalize_rotation(Bend(50, 60, 45)) == Bend(50, 60, 45)


def test_validate_issues():
    issues = validate([Straight(-1), Bend(50, 0.2), Bend(50, 190)], B_max=180)
    kinds = sorted(i.kind for i in issues if i.kind != "suggestion")      # классы 4.5
    assert kinds == ["geometric", "technological", "warning"], kinds


def test_path_from_table():
    rows = [(100, 50, 90, 0), (200, 50, 60, 30), (80, 0, 0, 0)]
    path = path_from_table(rows)
    assert path[0] == Straight(100) and path[1] == Bend(50, 90, 0)
    assert path[-1] == Straight(80)


def test_normalize_rotation_keeps_geometry():
    for A, B in [(170, 60), (-150, 60), (120, -45)]:
        p = [Straight(100), Bend(50, B, A), Straight(100)]
        q = [normalize_rotation(i) if isinstance(i, Bend) else i for i in p]
        assert np.abs(trace(p, 2000)[-1] - trace(q, 2000)[-1]).max() < 1e-6


def test_trace_spiral_joins_tangentially():
    from tubegeom import trace, polyline_length
    import numpy as np
    S, R, p, n = 50.0, 20.0, 10.0, 2.0
    pts = trace([Straight(S=S), Spiral(R=R, p=p, n=n), Straight(S=S)], steps_per_bend=200)
    # вход в спираль: первая прямая направлена вдоль X, стык без излома
    d_in = pts[1] - pts[0]
    assert np.allclose(d_in / np.linalg.norm(d_in), (1, 0, 0), atol=1e-9)
    # длина полилинии = S + n·√((2πR)²+p²) + S
    total = S + n * math.sqrt((2 * math.pi * R) ** 2 + p ** 2) + S
    assert abs(polyline_length(pts) - total) < 0.5
    # выход: последняя прямая идёт по касательной к концу спирали (излома нет)
    k = len(pts) - 1 - int(round(S))            # приблизительно начало последней прямой
    seg_out = pts[-1] - pts[-2]
    seg_last = pts[-2] - pts[-3]
    cosang = np.dot(seg_out, seg_last) / np.linalg.norm(seg_out) / np.linalg.norm(seg_last)
    assert cosang > 0.9998                     # погрешность хорды при 200 шагах на виток


# ---------- сверка с 1.5, 3.1, 3.4, 4.5 (18.10.2026) ----------
import numpy as np
from tubegeom import (table_row, normalize_rotation, validate, self_intersections, end_drift,
                      manual_angle_tolerance)


def test_minimize_rotation_only_for_both_direction_profile():
    b = Bend(R=40, B=90, A=150)
    assert normalize_rotation(b, "right") == b
    assert normalize_rotation(b, "both") == Bend(40, -90, -30)


def test_table_row_rules_3_4():
    assert table_row(Bend(40, -90, 30), "right") == (-150.0, 90, None)        # A += 180°, B ≥ 0
    assert table_row(Bend(40, 90, 150), "both") == (-30.0, 90, "Л")          # минимизация поворота
    assert table_row(Bend(40, 90, 150), "both", minimize=False) == (150.0, 90, "П")
    assert table_row(Bend(40, -45, 180), "left") == (0.0, 45, None)


def test_validate_profile_range_spiral_and_merge():
    p = [Straight(50), Bend(40, 10, 0), Straight(50)]
    assert any(i.kind == "technological" and "мин." in i.text for i in validate(p, B_min=15))
    p = [Straight(50), Straight(30)]
    assert any(i.kind == "suggestion" for i in validate(p))
    p = [Straight(50), Spiral(R=20, p=8, n=2), Straight(50)]
    kinds = [i.text for i in validate(p, od=10, spiral_min_pitch=12, spiral_allowed=False)]
    assert any("без разрешения" in t for t in kinds) and any("мин. шага" in t for t in kinds)
    assert any("самопересечение витков" in t for t in kinds)


def test_self_intersection_rule_4_5():
    u = [Straight(100), Bend(10, 180, 0), Straight(100)]                       # 2R = 20 < ∅ 25
    assert self_intersections(u, od=25)
    assert not self_intersections(u, od=12)
    assert not self_intersections([Straight(100), Bend(40, 10, 0), Straight(100)], od=12)   # без ложных


def test_end_drift_3_4():
    path = [Straight(200), Bend(40, 90, 0), Straight(300), Bend(40, 90, 90), Straight(200)]
    r = end_drift(path, tol_B=[0.5, 0.5], tol_A=3.0, tol_S=1.0)
    assert r["worst_pos"] >= r["probable_pos"] > 0
    assert r["warning"]                                                        # ручной ±3° по A на 200 мм > 3 мм
    top = r["shares"][0]
    assert top[1] == "A" and top[0] == 2                                      # больше всего — поворот на гибе 2
    assert abs(sum(s[2] for s in r["shares"]) - 1.0) < 1e-9
    r2 = end_drift(path, tol_B=[0.2, 0.2], tol_A=0.2, tol_S=0.2)
    assert not r2["warning"]


def test_manual_angle_tolerance():
    ticks = [45, 90]
    assert manual_angle_tolerance(90, ticks, stops=[90]) == 0.2
    assert manual_angle_tolerance(45, ticks) == 0.5
    assert manual_angle_tolerance(52, ticks) == 2.0


def test_bend_after_spiral_at_A0_lies_in_exit_tangent_axis_plane():
    from spiral import spiral_axis
    R, p, n = 20.0, 10.0, 1.25
    path = [Straight(50), Spiral(R=R, p=p, n=n), Straight(40), Bend(30, 90, 0), Straight(40)]
    pts = trace(path, steps_per_bend=400)
    axis = spiral_axis((1, 0, 0), R, p, +1, up=(0, 0, 1))
    # участок после гиба и касательная выхода лежат в плоскости с осью спирали: нормаль ⟂ участку после гиба
    k = len(pts) - 1
    after = pts[-1] - pts[-2]
    seg = trace(path[:3], steps_per_bend=400)
    tend = seg[-1] - seg[-2]
    nrm = np.cross(tend / np.linalg.norm(tend), axis)
    assert abs(np.dot(after / np.linalg.norm(after), nrm / np.linalg.norm(nrm))) < 1e-6


def test_angle_A_needs_nonzero_straight_between_bends_18_10():
    # решение 18.10.2026: S = 0 при заданном A — ошибка; первый гиб A не учитывает
    bad = [Straight(50), Bend(50, 90, 0.0), Bend(50, 90, 30.0)]
    assert any(i.kind == "geometric" for i in validate(bad))
    ok = [Straight(50), Bend(50, 90, 0.0), Straight(10), Bend(50, 90, 30.0)]
    assert not any(i.kind == "geometric" for i in validate(ok))
    first = [Bend(50, 90, 45.0), Straight(10)]
    assert not any(i.kind == "geometric" for i in validate(first))
