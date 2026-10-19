import numpy as np
from tubegeom import Straight, Bend
from ports import Port, check_connection, candidates, connect, start_from_port


def kinds(issues):
    return sorted(i.kind for i in issues)


def base_port(**kw):
    d = dict(name="П1", pos=(0.0, 0.0, 0.0), direction=(1.0, 0.0, 0.0), od=20.0, min_straight=30.0)
    d.update(kw)
    return Port(**d)


def test_matching_port_has_no_issues():
    p = base_port()
    path = [Straight(100), Bend(50, 90), Straight(100)]
    assert check_connection(p, path, od=20.0, tube_id="T1") == []


def test_od_mismatch_is_warning():
    p = base_port()
    path = [Straight(100), Bend(50, 90), Straight(100)]
    assert kinds(check_connection(p, path, od=16.0, tube_id="T1")) == ["warning"]


def test_min_straight_is_technological():
    p = base_port(min_straight=50)
    path = [Straight(20), Bend(50, 90), Straight(100)]
    assert "technological" in kinds(check_connection(p, path, od=20.0, tube_id="T1"))


def test_direction_deviation_is_warning():
    p = base_port(tol_ang=1.0)
    path = [Straight(100), Bend(50, 90), Straight(100)]
    p2 = base_port(tol_ang=1.0)                                       # направление порта — ось X
    end_dir = (0.9962, 0.0872, 0.0)                                   # ~5° от оси X
    assert "warning" in kinds(check_connection(p2, path, od=20.0, tube_id="T1", end_dir=end_dir))


def test_occupied_port_cannot_be_connected():
    p = base_port(occupied_by="T9")
    path = [Straight(100), Bend(50, 90), Straight(100)]
    assert any(i.kind == "refused" for i in connect(p, "T1", path, 20.0))
    assert p.occupied_by == "T9"


def test_connect_marks_port_busy():
    p = base_port()
    path = [Straight(100), Bend(50, 90), Straight(100)]
    connect(p, "T1", path, 20.0)
    assert p.occupied_by == "T1"


def test_candidates_and_start_from_port():
    ports = [base_port(name="A"), base_port(name="B", od=16.0), base_port(name="C", occupied_by="T2")]
    assert [p.name for p in candidates(ports, 20.0)] == ["A"]
    pos, d = start_from_port(ports[0])
    assert np.allclose(pos, [0, 0, 0]) and np.allclose(d, [1, 0, 0])
