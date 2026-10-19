import math
from tubegeom import Straight, Bend, Spiral, axis_length
from blank import blank_length, EndRule
from compensation import b_st, compensated_blank, compensated_path


def close(a, b, tol=1e-6):
    assert abs(a - b) <= tol, f"{a} != {b}"


def test_no_compensation_matches_blank():
    path = [Straight(100), Bend(50, 90), Straight(100), Bend(50, 60), Straight(60)]
    a = blank_length(path, EndRule(N=40), EndRule(N=40), dmin=0)
    b = compensated_blank(path, EndRule(N=40), EndRule(N=40), K=1.0, dmin=0)
    close(a.blank_length, b.blank_length)


def test_b_st_fallback_and_theoretical():
    bend = Bend(50, 90)
    close(b_st(bend, 0.95), 90 / 0.95)                       # B_ст = B/K (3.6)
    close(b_st(bend, 0.95, theoretical=lambda b: 96.0), 96.0)
    close(b_st(bend, 0.95, theoretical=lambda b: None), 90 / 0.95)


def test_compensation_increases_arc_length():
    path = [Straight(100), Bend(50, 90), Straight(100)]
    base = compensated_blank(path, EndRule(), EndRule(), K=1.0).blank_length
    comp = compensated_blank(path, EndRule(), EndRule(), K=0.95).blank_length   # K < 1 по формуле 3.6
    # дуги: R·(B/K − B); прямые: −ΔT с каждой стороны гиба (3.6)
    from compensation import delta_T
    dT = delta_T(Bend(50, 90), 0.95)
    close(comp - base, 50 * math.radians(90 / 0.95 - 90) - 2 * dT, 1e-9)


def test_springback_example_3_6_dT():
    # пример 3.6: медь 22×1, R=77, B=90 → B_ст=92,2°, ΔT≈1,9 мм с каждой стороны
    from compensation import delta_T
    K = 90 / 92.2
    close(delta_T(Bend(77, 90), K), 1.9, 0.05)


def test_spiral_turns_divided_by_K():
    st = compensated_path([Spiral(20, 10, 5)], K=1.25)
    close(st[0].n, 4.0)
