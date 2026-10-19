from calib import Trial, Calibration, CalibrationJournal, fit_ab, corrected_angle


def test_fit_single_trial_gives_a_only():
    a, b = fit_ab([Trial(B_model=90, B_set=92.0, B_fact=90.0)])
    assert abs(a - 92.0 / 90.0) < 1e-9 and b == 0.0


def test_fit_two_angles_gives_a_and_b():
    ts = [Trial(60, 61.0, 60.0), Trial(120, 122.0, 120.0)]
    a, b = fit_ab(ts)
    assert abs(a * 60 + b - 61.0) < 1e-6 and abs(a * 120 + b - 122.0) < 1e-6


def test_journal_newest_replaces_and_history_kept():
    j = CalibrationJournal()
    tool = ("T-01", "ТГ-22")
    j.record(tool, Calibration("2026-10-01", "Иванов", 22.0, 1.0, 1.02, 0.0))
    j.record(tool, Calibration("2026-10-18", "Петров", 22.0, 1.0, 1.03, 0.1))
    cur = j.current(tool, 22.0, 1.05)                  # стенка в допуске 0,1
    assert cur.operator == "Петров"
    assert len(j.history(tool)) == 2
    assert j.current(tool, 22.0, 1.5) is None           # другая стенка — расчёт
    assert corrected_angle(90, None) is None
    assert abs(corrected_angle(90, cur) - (1.03 * 90 + 0.1)) < 1e-9
