import numpy as np
from sketch import Constraint, solve, snap_point, ortho_step, snap_angle


def test_distance_and_fix_solves():
    pts = [(0, 0, 0), (100, 3, 0)]
    cons = [Constraint("fix", (0,), target=np.array([0, 0, 0.0]), label="фикс"),
            Constraint("dist", (0, 1), value=50.0, label="L")]
    r = solve(pts, cons)
    assert r.status in ("определено", "недоопределено")
    assert abs(np.linalg.norm(r.points[1] - r.points[0]) - 50) < 1e-6


def test_underdetermined_reports_dof():
    pts = [(0, 0, 0), (10, 0, 0)]
    cons = [Constraint("dist", (0, 1), value=20.0)]
    r = solve(pts, cons)
    assert r.status == "недоопределено" and r.dof > 0


def test_conflict_detected():
    pts = [(0, 0, 0), (10, 0, 0)]
    cons = [Constraint("fix", (0,), target=np.zeros(3), label="A"),
            Constraint("fix", (1,), target=np.array([10.0, 0, 0]), label="B"),
            Constraint("dist", (0, 1), value=20.0, label="L20")]
    r = solve(pts, cons)
    assert r.status == "конфликт" and "L20" in r.conflicts


def test_coincide_with_feature_and_plane_distance():
    pts = [(1, 1, 1), (50, 50, 50)]
    cons = [Constraint("coincide", (0,), target=np.array([0, 0, 0.0]), label="порт"),
            Constraint("plane_dist", (1,), value=30.0, normal=np.array([0, 0, 1.0]),
                       origin=np.zeros(3), label="до плоскости")]
    r = solve(pts, cons)
    assert np.allclose(r.points[0], [0, 0, 0], atol=1e-6)
    assert abs(r.points[1][2] - 30) < 1e-6


def test_perp_and_parallel():
    pts = [(0, 0, 0), (10, 5, 0)]
    r = solve(pts, [Constraint("parallel", (0, 1), normal=np.array([1.0, 0, 0]), label="∥X"),
                    Constraint("fix", (0,), target=np.zeros(3), label="f")])
    assert abs(r.points[1][1]) < 1e-6


def test_snap_priority_port_first():
    p, kind = snap_point((1, 0, 0), ports=[(0, 0, 0)], features=[(1, 0, 0)], radius=5)
    assert kind == "порт" and p == (0, 0, 0)
    p2, kind2 = snap_point((100, 0, 0), ports=[(0, 0, 0)], features=[(1, 0, 0)], radius=5)
    assert kind2 == "свободно"


def test_ortho_step_moves_one_axis():
    assert ortho_step((0, 0, 0), (10, 3, 0)) == (10, 0, 0)


def test_snap_angle_to_profile_values():
    assert snap_angle(44.0, [30, 45, 60, 90]) == 45


# ---------- сверка с 1.2, 1.3 (18.10.2026) ----------
import math
import numpy as np
from sketch import snap_bend_angle, measure, make_reference, delete_constraint, SCALE_WARNING


def test_snap_priority_tiers_1_2():
    t = (10.2, 0.1, 0)
    assert snap_point(t, vertices=[(10, 0, 0)], features=[(10.3, 0, 0)])[1] == "вершина"
    assert snap_point(t, features=[(10.3, 0, 0)], guides=[((0, 0, 0), (1, 0, 0))])[1] == "признак"
    p, kind = snap_point(t, guides=[((0, 0, 0), (1, 0, 0))])
    assert kind == "направляющая" and abs(p[1]) < 1e-12
    p, kind = snap_point((9.8, 0.3, 0), grid=((0, 0, 0), (1, 0, 0), (0, 1, 0), 5.0), radius=1.0)
    assert kind == "сетка" and p[:2] == (10.0, 0.0)
    assert snap_point((50, 50, 0), ports=[(0, 0, 0)])[1] == "свободно"


def test_bend_angle_snapping_manual_and_mechanized():
    pref = [15, 22.5, 30, 45, 60, 67.5, 75, 90]
    assert snap_bend_angle(41, manual=True, preferred=pref, ticks=pref) == (45, None)        # жёстко
    assert snap_bend_angle(41, manual=True, preferred=pref, ticks=pref, typed=True) == (41, SCALE_WARNING)
    assert snap_bend_angle(41, manual=False, preferred=pref) == (41, None)                    # мягко: далеко
    assert snap_bend_angle(44, manual=False, preferred=pref) == (45, None)
    assert snap_bend_angle(37.3, manual=False, step=2.5) == (37.5, None)
    assert snap_bend_angle(37.3, manual=False) == (37.3, None)                                # без шага — свободно


def test_straight_S_dimension_accounts_for_tangents():
    R = 20.0
    pts = [(0, 0, 0), (100, 0, 0), (100, 100, 0)]
    c = Constraint("straight_S", (-1, 0, 1, 2), value=0, R=R)
    # S от торца до гиба: L − T, T = R·tan(45°) = 20
    assert abs(measure(np.array(pts, float), c) - 80.0) < 1e-9
    cons = [Constraint("fix", (0,), target=np.zeros(3)),
            Constraint("fix", (2,), target=np.array([100.0, 100, 0])),
            Constraint("straight_S", (-1, 0, 1, 2), value=60.0, R=R, label="S1"),
            Constraint("plane_dist", (1,), value=0.0, normal=np.array([0, 0, 1.0]), origin=np.zeros(3))]
    r = solve(pts, cons)
    assert r.status != "конфликт"
    assert abs(measure(r.points, cons[2]) - 60.0) < 1e-6


def test_bend_angle_coplanar_symmetry_line_dist():
    P = np.array([(0, 0, 0), (10, 0, 0), (10, 10, 0), (0, 10, 0.5)], float)
    assert abs(measure(P, Constraint("bend_angle", (0, 1, 2))) - 90) < 1e-9
    r = solve(P, [Constraint("coplanar", (0, 1, 2, 3))])
    Q = r.points
    nrm = np.cross(Q[1] - Q[0], Q[2] - Q[0])
    assert r.status != "конфликт" and abs(np.dot(Q[3] - Q[0], nrm / np.linalg.norm(nrm))) < 1e-6
    r = solve([(1, 1, 0), (3, 2, 0)], [Constraint("symmetry", (0, 1), normal=np.array([1.0, 0, 0]), origin=np.zeros(3))])
    a, b = r.points
    assert abs(a[0] + b[0]) < 1e-6 and abs(a[1] - b[1]) < 1e-6
    c = Constraint("line_dist", (0,), value=5.0, origin=np.zeros(3), normal=np.array([0, 0, 1.0]))
    r = solve([(3, 0, 7)], [c])
    assert abs(measure(r.points, c) - 5.0) < 1e-6


def test_reference_dimension_and_conflict_dialog():
    cons = [Constraint("fix", (0,), target=np.zeros(3), label="F"),
            Constraint("fix", (1,), target=np.array([10.0, 0, 0]), label="F2"),
            Constraint("dist", (0, 1), value=12.0, label="d0", source="user_dim")]
    r = solve([(0, 0, 0), (10, 0, 0)], cons)
    assert r.status == "конфликт" and "d0" in r.conflicts
    cons2 = make_reference(cons, "d0")                    # «Сделать справочным»
    r2 = solve([(0, 0, 0), (10, 0, 0)], cons2)
    assert r2.status == "определено" and abs(r2.reference_values["d0"] - 10.0) < 1e-6
    assert len(delete_constraint(cons, "d0")) == 2         # «Удалить»
    try:
        make_reference(cons, "F")
    except ValueError:
        pass
    else:
        raise AssertionError("ограничение не делается справочным")


def test_drag_priority_user_dimension_beats_auto_constraint():
    # точка 1 тянется: размер пользователя 10 и авто-ограничение «на 12» конфликтуют — побеждает размер
    cons = [Constraint("fix", (0,), target=np.zeros(3), source="explicit"),
            Constraint("dist", (0, 1), value=10.0, source="user_dim", label="d0"),
            Constraint("dist", (0, 1), value=12.0, source="auto", label="auto")]
    r = solve([(0, 0, 0), (11, 0, 0)], cons, preserve_shape=True)
    d = float(np.linalg.norm(r.points[1] - r.points[0]))
    assert abs(d - 10.0) < 0.05


def test_entry_arc_conflict_18_10():
    from sketch import entry_arc_to_vertex
    ok = entry_arc_to_vertex(50.05, 50.0)
    assert ok["status"] == "ok" and ok["R"] == 50.0 and "отличается" in ok["report"]
    bad = entry_arc_to_vertex(60.0, 50.0)
    assert bad["status"] == "conflict" and bad["default"] == "keep_body" and bad["R"] is None
