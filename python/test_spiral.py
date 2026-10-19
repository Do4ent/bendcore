import math
import numpy as np
from spiral import spiral_centreline, spiral_axis_length, spiral_axis


def _length(pts):
    return float(np.sum(np.linalg.norm(np.diff(pts, axis=0), axis=1)))


def test_length_matches_formula():
    pts, _, _ = spiral_centreline((0, 0, 0), (1, 0, 0), R=20, p=10, n=5, steps_per_turn=2000)
    assert abs(_length(pts) - spiral_axis_length(20, 10, 5)) < 1e-2


def test_start_tangent_equals_heading():
    h = np.array([0.6, 0.8, 0.0])
    pts, _, _ = spiral_centreline((5, 5, 5), tuple(h), R=20, p=10, n=2, steps_per_turn=4000)
    d = _unit(pts[1] - pts[0])
    assert np.dot(d, h) > 1 - 1e-6


def _unit(v):
    return v / np.linalg.norm(v)


def test_end_displacement_along_axis_is_n_pitch():
    h = np.array([1.0, 0.0, 0.0])
    pts, tend, a = spiral_centreline((0, 0, 0), tuple(h), R=20, p=10, n=3, steps_per_turn=720)
    assert abs(np.dot(pts[-1] - pts[0], a) - 3 * 10) < 1e-6


def test_end_tangent_is_consistent_with_points():
    h = np.array([1.0, 0.0, 0.0])
    pts, tend, _ = spiral_centreline((0, 0, 0), tuple(h), R=20, p=10, n=2, steps_per_turn=4000)
    d = _unit(pts[-1] - pts[-2])
    assert np.dot(d, tend) > 1 - 1e-6


def test_direction_sets_handedness():
    # правая: вращение против часовой вокруг оси a совпадает с продвижением вдоль +a;
    # левая: продвижение противоположно направлению вращения → знак продвижения = направлению
    h = (1.0, 0.0, 0.0)
    for direction in (+1, -1):
        pts, _, a = spiral_centreline((0, 0, 0), h, R=20, p=10, n=1, direction=direction,
                                      steps_per_turn=64)
        advance = float(np.dot(pts[-1] - pts[0], a))
        assert advance * direction > 0
        c = pts - pts.mean(axis=0)
        sense = float(np.sum(np.cross(c[:-1], c[1:]) @ a))
        assert sense > 0


def test_explicit_axis_not_tangent_is_rejected():
    try:
        spiral_centreline((0, 0, 0), (1, 0, 0), R=20, p=10, n=1, axis=(0, 0, 1))
    except ValueError:
        return
    raise AssertionError("касание не проверено")
