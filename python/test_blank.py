import math
from tubegeom import Straight, Bend, axis_length
from blank import blank_length, EndRule, check_min_straights


def close(a, b, tol=1e-6):
    assert abs(a - b) <= tol, f"{a} != {b}"


def test_auto_allowance_adds_when_short():
    path = [Straight(20), Bend(50, 90), Straight(200)]
    r = blank_length(path, EndRule(E=0, N=40), EndRule(E=0, N=40), dmin=0)
    close(r.delta_start, 20)                              # N − S = 20
    close(r.delta_end, 0)                                 # S = 200 > N
    close(r.blank_length, axis_length(path) + 20)


def test_dmin_applies_when_larger():
    path = [Straight(100), Bend(50, 90), Straight(100)]
    r = blank_length(path, EndRule(N=40), EndRule(N=40), dmin=5)
    close(r.delta_start, 5)                               # max(5, 40 − 100)
    close(r.delta_end, 5)


def test_end_insertion_adds_to_blank():
    path = [Straight(100), Bend(50, 90), Straight(100)]
    r = blank_length(path, EndRule(E=10, N=0), EndRule(E=15, N=0), dmin=0)
    close(r.blank_length, axis_length(path) + 25)


def test_no_cut_is_hard_limit():
    path = [Straight(30), Bend(50, 90), Straight(100)]
    r = blank_length(path, EndRule(E=0, N=40, cut_allowed=False), EndRule(N=0), dmin=5)
    close(r.delta_start, 0)
    assert any(i.kind == "technological" for i in r.issues)
    r2 = blank_length([Straight(50), Bend(50, 90), Straight(100)],
                      EndRule(E=0, N=40, cut_allowed=False), EndRule(N=0))
    assert not r2.issues


def test_min_straight_between_bends():
    path = [Straight(100), Bend(50, 90), Straight(10), Bend(50, 90), Straight(100)]
    issues = check_min_straights(path, Lmin=30)
    assert len(issues) == 1 and issues[0].kind == "technological"
    assert check_min_straights([Straight(100), Bend(50, 90), Straight(40), Bend(50, 90)], 30) == []


# ---------- сверка с 2.5, 3.5 ----------
from blank import EndSpec, end_segment, end_minimum, blank_by_order, check_blank_limits


def test_manual_segment_rules():
    # S = 20, E = 5, N = 60, Δ_мин = 3 → минимум = max(28, 60) = 60
    assert end_minimum(20, 5, 60, 3, True) == 60
    d, iss = end_segment(20, EndSpec(E=5, manual=70), 60, 3, True)
    assert d == 45 and not iss
    d, iss = end_segment(20, EndSpec(E=5, manual=50), 60, 3, True)
    assert d is None and iss and iss[0].kind == "technological"                  # ошибка вместо Δ, не 0
    d, iss = end_segment(20, EndSpec(E=5, manual=60), 60, 3, True)              # равно минимуму = «Авто»
    assert d == end_segment(20, EndSpec(E=5), 60, 3, True)[0]
    d, iss = end_segment(80, EndSpec(manual=90), 60, 0, False)
    assert d == 0 and any("недоступен" in i.text for i in iss)


def test_manual_below_minimum_shows_error_not_value():
    path = [Straight(100), Bend(50, 90), Straight(80)]
    r = blank_by_order(path, EndSpec(E=10, manual=60), EndSpec(), N_first=30, N_last=25, feed_from="A")
    assert r.delta_start is None and r.blank_length is None
    assert any(i.kind == "technological" for i in r.issues)


def test_order_swaps_N_and_both_orders_take_max():
    path = [Straight(30), Bend(40, 90, 0), Straight(100)]
    a = blank_by_order(path, EndSpec(), EndSpec(), N_first=80, N_last=20, feed_from="A")
    b = blank_by_order(path, EndSpec(), EndSpec(), N_first=80, N_last=20, feed_from="B")
    assert (a.delta_start, a.delta_end) == (50, 0) and (b.delta_start, b.delta_end) == (0, 0)
    both = blank_by_order(path, EndSpec(), EndSpec(), N_first=80, N_last=20, both_orders=True)
    assert both.delta_start == 50 and both.blank_length == a.blank_length


def test_blank_limits():
    assert check_blank_limits(6100, 6000, None, None)[0].kind == "technological"
    assert check_blank_limits(6100, None, 5000, None)[0].kind == "technological"     # по умолчанию 6.13
    assert check_blank_limits(100, None, None, None)[0].kind == "info"
    assert check_blank_limits(9000, None, None, None, coil=True) == []                # бухта
    assert check_blank_limits(3000, 6000, None, 2500)[0].text.startswith("заготовка 3000")
