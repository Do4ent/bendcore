// Эталонные значения получены из Python: calib.py (19.10.2026).
#include "bendcore/calib.hpp"

#include <cmath>
#include <cstdio>
#include <stdexcept>

using namespace bendcore;

static int failures = 0;

static void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}

static bool near(double a, double b, double eps = 1e-9) { return std::abs(a - b) <= eps; }

static void test_fit() {
    const auto one = fit_ab({{90, 90, 92}});
    check(near(one.first, 0.9782608695652174) && near(one.second, 0.0), "fit_ab: одна проба — только a");

    const auto three = fit_ab({{30, 30, 31}, {60, 60, 61.5}, {90, 90, 92.5}});
    check(near(three.first, 0.975588491717524, 1e-9) && near(three.second, -0.15650583707232357, 1e-9),
          "fit_ab: три пробы — МНК");

    const auto same = fit_ab({{60, 60, 61}, {60, 60, 61.2}});
    check(near(same.first, 0.981999357119897) && near(same.second, 0.0), "fit_ab: все пробы на одном угле — только a");

    const auto two = fit_ab({{30, 31, 30.5}, {90, 90, 92}});
    check(near(two.first, 0.9591945830363506, 1e-9) && near(two.second, 1.7159657875980017, 1e-9),
          "fit_ab: две пробы на разных углах — a и b");

    bool thrown = false;
    try {
        fit_ab({});
    } catch (const std::invalid_argument&) {
        thrown = true;
    }
    check(thrown, "fit_ab: пустой набор — ошибка");
}

static void test_journal() {
    CalibrationJournal j;
    const CalibrationJournal::Tool t{"P1", "profA"};
    Calibration c1{"2026-10-01", "op", 22.0, 1.0, 1.02, 0.1};
    j.record(t, c1);
    check(j.current(t, 22.0, 1.05).has_value(), "journal: стенка в допуске 0,1 — запись подходит");
    check(!j.current(t, 22.0, 1.2).has_value(), "journal: стенка вне допуска — расчёт");

    Calibration c2{"2026-10-05", "op", 22.0, 1.0, 1.03, 0.0};
    j.record(t, c2);
    const auto cur = j.current(t, 22.0, 1.0);
    check(cur && near(cur->a, 1.03), "journal: действующая — последняя");
    check(!j.current(t, 25.0, 1.0).has_value(), "journal: другой ∅ — расчёт");

    j.cancel(t, 1);
    const auto after = j.current(t, 22.0, 1.0);
    check(after && near(after->a, 1.02), "journal: отменённая запись не действует, предыдущая — действует");
    check(j.history(t).size() == 2, "journal: отменённая запись остаётся в истории");
}

static void test_corrected_angle() {
    const Calibration c{"d", "op", 22.0, 1.0, 1.02, 0.1};
    const auto v = corrected_angle(90, c);
    check(v && near(*v, 91.9, 1e-9), "corrected_angle: a·B + b");
    check(!corrected_angle(90, std::nullopt).has_value(), "corrected_angle: без калибровки — расчёт");
}

int main() {
    test_fit();
    test_journal();
    test_corrected_angle();
    if (failures == 0) std::printf("OK calib\n");
    return failures == 0 ? 0 : 1;
}
