import math
import numpy as np

from bendmap import bend_rows, xyz_points, lra_rows, control_recalc, output_blocked, release_gate, rounded, has_spiral
from bendmap import bend_sequence, ORDER_PATH, ORDER_TECH
from tubegeom import Bend, Straight, Spiral, Issue, validate


def sample_path():
    return [Straight(100), Bend(50, 90), Straight(100), Bend(50, 90, 90.0), Straight(60)]


def test_rounded_step():
    assert abs(rounded(92.23, 0.1) - 92.2) < 1e-9
    assert rounded(92.23, 0) == 92.23


def test_bend_rows_compensation_and_feed_3_6():
    rows = bend_rows(sample_path(), K=90 / 92.2, offset_start=10.0)
    assert len(rows) == 2
    assert abs(rows[0].B_st - 92.2) < 0.01
    assert rows[0].source == "расчёт"
    # S_ст первого прямого = S − ΔT (в строке второго гиба — S_ст первого участка между гибами)
    assert rows[1].S_st < rows[1].S
    # подача первого гиба: offset + S_ст первого участка
    assert abs(rows[0].feed - (10.0 + rows[0].S_st)) < 1e-9
    # подача второго гиба — позиция после дуги первого
    assert rows[1].feed > rows[0].feed + math.radians(rows[0].B_st) * 50


def test_manual_b_st_takes_priority():
    rows = bend_rows(sample_path(), K=0.97, manual={1: 95.0})
    assert rows[0].B_st == 95.0 and rows[0].source == "вручную"
    assert rows[1].source == "расчёт"


def test_xyz_and_lra_without_spiral():
    p = [Straight(50), Bend(50, 90), Straight(50)]
    pts = xyz_points(p, E_A=5.0, E_B=3.0)
    assert np.allclose(pts[0], [-5, 0, 0], atol=1e-6)            # физический торец: E назад от порта
    assert pts[-1][1] > 50                                       # конец Б: за последней вершиной на E_Б
    lra = lra_rows(p)
    assert lra[0][0] == 50 and lra[1][1] == 50 and lra[1][3] == 90


def test_spiral_blocks_xyz_and_lra():
    p = [Straight(50), Spiral(20, 10, 5), Straight(50)]
    assert has_spiral(p)
    for fn in (xyz_points, lra_rows):
        try:
            fn(p)
            raise AssertionError("спираль должна блокировать XYZ/LRA")
        except ValueError:
            pass


def test_control_recalc_half_step_tolerance_3_6():
    p = sample_path()
    K = 90 / 92.2
    rows = bend_rows(p, K=K, step_B=0.1)
    assert control_recalc(p, K, rows, step_B=0.1, max_dev=5.0) == []
    rows_bad = bend_rows(p, K=K, manual={1: 96.0}, step_B=0.1)
    issues = control_recalc(p, K, rows_bad, step_B=0.1, max_dev=5.0)
    assert any(i.kind == "warning" for i in issues)


def test_output_and_release_blocked_by_technological():
    bad = [Issue("technological", 0, "B_ст больше макс. угла профиля")]
    assert output_blocked(bad)
    ok, _ = release_gate(bad)
    assert not ok
    ok2, warn = release_gate([Issue("warning", 0, "x")], outdated=True, mode="soft")
    assert ok2 and warn
    ok3, _ = release_gate([Issue("warning", 0, "x")], outdated=True, mode="strict")
    assert not ok3
    assert not release_gate([], unresolved_refs=1)[0]


def test_calibration_source_and_dL_replace_dT_5():
    from calib import Calibration
    p = sample_path()
    cal = Calibration("2026-10-18", "Петров", 22.0, 1.0, a=1.0, b=0.5, dL={1: 1.0})
    rows = bend_rows(p, K=0.97, cal=cal, step_B=0.0)
    assert rows[0].source == "калибровка" and abs(rows[0].B_st - (90 + 0.5)) < 1e-9
    from compensation import delta_T
    s_no_cal = bend_rows(p, K=0.97)
    # прямой между гибами №1 и №2: 100 − ΔL₁ (калибровка) − ΔT₂ (расчёт)
    assert abs(rows[1].S_st - (100 - 1.0 - delta_T(p[3], 0.97))) < 1e-9
    assert abs(s_no_cal[1].S_st - (100 - delta_T(p[1], 0.97) - delta_T(p[3], 0.97))) < 1e-9


def test_control_default_is_3_mm():
    import bendmap
    assert bendmap.MAX_DEV_MM == 3.0


def test_production_issue_counters_and_admin_cancel_3_6():
    from bendmap import ProductionLog
    p = sample_path()
    rows = bend_rows(p, K=0.97)
    log = ProductionLog()
    ev = log.issue([], True, rows, {"гибка": "ТГ-22-07"}, qty=10, user="Иванов", variant="с конца А", date="2026-10-18")
    assert log.counters["ТГ-22-07"] == 2 * 10
    # техническая ошибка блокирует выдачу
    try:
        log.issue([Issue("technological", 0, "x")], True, rows, {"гибка": "ТГ-22-07"}, 1, "Иванов", "А", "2026-10-18")
        raise AssertionError("должна быть блокировка")
    except ValueError:
        pass
    # не выпущен — блок в строгом, предупреждение в мягком
    try:
        log.issue([], False, rows, {"гибка": "ТГ-22-07"}, 1, "Иванов", "А", "2026-10-18")
        raise AssertionError("строгий режим должен блокировать")
    except ValueError:
        pass
    log.issue([], False, rows, {"гибка": "ТГ-22-07"}, 1, "Иванов", "А", "2026-10-18", soft_release=True)
    # отмена — только администратор, счётчик уменьшается, запись остаётся
    try:
        log.cancel(0, by_admin=False)
        raise AssertionError("не администратор не должен отменять")
    except PermissionError:
        pass
    log.cancel(0, by_admin=True)
    assert ev.cancelled and len(log.events) == 2


def test_map_export_csv_and_pdf_3_6():
    import os, tempfile
    from bendmap import map_csv, map_pdf
    rows = bend_rows(sample_path(), K=90 / 92.2, offset_start=10.0)
    text = map_csv(rows)
    lines = text.strip().splitlines()
    assert lines[0].startswith("№;S;S_ст") and len(lines) == 1 + len(rows)
    assert "расчёт" in lines[1]
    path = os.path.join(tempfile.mkdtemp(), "map.pdf")
    map_pdf(rows, path, "Карта гибки: тест")
    with open(path, "rb") as fh:
        assert fh.read(5) == b"%PDF-"


def test_bend_numbers_fixed_by_path_tech_order_reversed_8_scheme():
    # номера гибов фиксируются по пути; в технологическом порядке строки идут 3, 2, 1 (3.6)
    rows = bend_rows([Straight(100), Bend(50, 90), Straight(100), Bend(40, 60), Straight(80), Bend(30, 45), Straight(20)], K=1.0)
    assert [r.no for r in rows] == [1, 2, 3]
    assert [r.no for r in bend_sequence(rows, ORDER_PATH)] == [1, 2, 3]
    assert [r.no for r in bend_sequence(rows, ORDER_TECH)] == [3, 2, 1]
    try:
        bend_sequence(rows, "неизвестный")
        raise AssertionError("ожидалась ValueError")
    except ValueError:
        pass


def test_assembly_map_one_csv_per_tube_8():
    from bendmap import assembly_map_csv, tube_file_name
    rows = bend_rows(sample_path(), K=1.0)
    files = assembly_map_csv({"T-1/A": rows, "T 2": rows})
    assert set(files) == {"T-1_A.csv", "T 2.csv"}
    from bendmap import MAP_HEADER
    assert files["T 2.csv"].splitlines()[0] == ";".join(MAP_HEADER)
    assert tube_file_name("  ab:c. ") == "ab_c"
    try:
        assembly_map_csv({"T:1": rows, "T_1": rows})
        raise AssertionError("ожидалась ValueError")
    except ValueError:
        pass


def test_assembly_map_pdf_one_per_tube_8():
    import os
    import tempfile
    from bendmap import assembly_map_pdf
    rows = bend_rows(sample_path(), K=1.0)
    with tempfile.TemporaryDirectory() as d:
        paths = assembly_map_pdf({"T-1/A": rows, "T 2": rows}, d)
        assert sorted(os.path.basename(p) for p in paths) == ["T 2.pdf", "T-1_A.pdf"]
        assert all(os.path.getsize(p) > 0 for p in paths)
