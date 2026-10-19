import numpy as np
from refs import Tube, Ref, update_component, VALID, LOST, OUTDATED


def make_tube(mode="адаптивна"):
    pts = np.array([[0, 0, 0], [100, 0, 0], [100, 50, 0]], dtype=float)
    refs = [Ref("P1", 0, (0.0, 0.0, 0.0)), Ref("P2", 2, (100.0, 50.0, 0.0))]
    return Tube(pts, refs, mode)


def test_unchanged_all_ok():
    t = make_tube()
    r = update_component(t, {"P1": (0, 0, 0), "P2": (100, 50, 0)})
    assert r.status == "все в норме"


def test_moved_feature_recomputes():
    t = make_tube()
    r = update_component(t, {"P1": (0, 0, 0), "P2": (100, 60, 0)})
    assert r.status == "вершины сдвинулись" and r.moved == ["P2"]
    assert np.allclose(r.points[2], [100, 60, 0], atol=1e-4)
    assert all(ref.state == VALID for ref in t.refs)


def test_lost_reference_listed():
    t = make_tube()
    r = update_component(t, {"P1": (0, 0, 0)})
    assert r.status == "потеряны ссылки" and r.lost == ["P2"]
    assert t.refs[1].state == LOST


def test_frozen_ignores_refs():
    t = make_tube("заморожена")
    r = update_component(t, {"P1": (5, 5, 5)})
    assert r.status == "заморожена" and np.allclose(t.points, make_tube().points)


def test_not_loaded_and_initial_mode_5_7():
    import numpy as np
    from refs import Tube, Ref, update_component, initial_mode, NOT_LOADED
    assert initial_mode("assembly") == "адаптивна"
    for origin in ("recognized", "imported", "dragged", "table"):
        assert initial_mode(origin) == "заморожена"
    pts = np.array([[0.0, 0, 0], [10, 0, 0], [20, 0, 0]])
    t = Tube(points=pts.copy(), refs=[Ref("A", 0, (0.0, 0, 0)), Ref("B", 2, (20.0, 0, 0))])
    res = update_component(t, {"B": (20.0, 0, 0)}, unloaded=frozenset({"A"}))
    assert res.status == "не загружена"
    assert [r.state for r in t.refs if r.ref_id == "A"] == [NOT_LOADED]
    assert res.lost == []
