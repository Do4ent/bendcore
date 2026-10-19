import numpy as np
from body3d import sweep_tube, bbox, SpatialIndex, Document, Command


def test_straight_tube_bbox_and_faces():
    line = np.array([[0, 0, 0], [100, 0, 0]], dtype=float)
    V, F = sweep_tube(line, od=20, segments=16)
    lo, hi = bbox(V)
    assert np.allclose(lo, [0, -10, -10], atol=1e-6) and np.allclose(hi, [100, 10, 10], atol=1e-6)
    assert F.shape == (2 * 16, 3)


def test_tube_surface_radius_is_constant():
    line = np.array([[0, 0, 0], [50, 0, 0], [50, 50, 0]], dtype=float)
    V, _ = sweep_tube(line, od=10, segments=32)
    centre = np.repeat(line, 32, axis=0)
    d = np.linalg.norm(V - centre, axis=1)
    assert np.allclose(d, 5.0, atol=1e-6)


def test_spatial_index_matches_brute_force():
    rng = np.random.default_rng(1)
    idx = SpatialIndex(cell=50.0)
    boxes = {}
    for i in range(5000):
        lo = rng.uniform(0, 5000, 3)
        hi = lo + rng.uniform(1, 100, 3)
        boxes[f"c{i}"] = (lo, hi)
        idx.insert(f"c{i}", lo, hi)
    for _ in range(20):
        qlo = rng.uniform(0, 5000, 3)
        qhi = qlo + rng.uniform(50, 400, 3)
        brute = sorted(k for k, (a, b) in boxes.items() if np.all(a <= qhi) and np.all(b >= qlo))
        assert idx.query(qlo, qhi) == brute


def test_undo_redo_and_rollback():
    doc = Document()
    state = {"x": 0}
    def setx(v):
        old = state["x"]
        return Command(lambda: state.__setitem__("x", v), lambda: state.__setitem__("x", old))
    doc.begin(); doc.apply(setx(5)); doc.commit()
    doc.begin(); doc.apply(setx(9)); doc.commit()
    doc.undo(); assert state["x"] == 5
    doc.redo(); assert state["x"] == 9
    doc.begin(); doc.apply(setx(42)); doc.rollback(); assert state["x"] == 9


def test_recompute_order_and_cycle():
    doc = Document()
    doc.set_node("a", [], lambda v: 2)
    doc.set_node("b", ["a"], lambda v: v["a"] * 3)
    doc.set_node("c", ["a", "b"], lambda v: v["a"] + v["b"])
    doc.recompute()
    assert doc.values["c"] == 8
    doc.set_node("a", ["c"], lambda v: 1)
    try:
        doc.recompute(); raise AssertionError("цикл не найден")
    except ValueError as e:
        assert "цикл" in str(e) and "a" in str(e) and "c" in str(e)
