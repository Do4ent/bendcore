// Тело трубы на Open CASCADE (geom::make_solid_tube): валидность и объём π·r²·L,
// где L — длина оси из ядра bendcore (axis_length). Протяжка без скручивания: объём точный.
#include "bendcore/tube_body.hpp"
#include "bendcore/tubegeom.hpp"

#include <cmath>
#include <cstdio>

using namespace bendcore;

static int failures = 0;

static void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}

static void check_body(const Path& path, double od, const char* name, double rel_tol = 1e-4) {
    const double pi = std::acos(-1.0);
    const double r = od / 2.0;
    const double L = axis_length(path);
    const geom::TubeSolidResult res = geom::make_solid_tube(path, od);
    const double V_expected = pi * r * r * L;
    std::printf("%s: объём %.4f мм³, π·r²·L %.4f мм³, валидно=%d\n", name, res.volume, V_expected, res.valid ? 1 : 0);
    check(res.valid, name);
    check(std::abs(res.axis_length - L) < 1e-6, name);
    check(std::abs(res.volume - V_expected) <= rel_tol * V_expected, name);
}

int main() {
    check_body({Straight{100}, Bend{50, 90}, Straight{100}}, 20.0, "плоский гиб 90°");
    check_body({Straight{50}, Bend{40, 60, 0}, Straight{30}, Bend{40, -70, 90}, Straight{40}}, 16.0,
               "два гиба, второй с поворотом A");
    check_body({Straight{20}, Bend{30, 45, 45}, Straight{20}, Bend{30, 45, 0}, Straight{10}}, 12.0,
               "пространственный путь");
    // Спираль: отсчёты и сплайн дают приближение длины, допуск 1e-3.
    check_body({Straight{50}, Spiral{50, 30, 2, +1}, Straight{50}}, 20.0, "спираль в прямых", 1e-3);
    check_body({Straight{40}, Bend{50, 90}, Spiral{50, 30, 1.5, -1}, Straight{20}}, 20.0, "гиб и спираль", 1e-3);
    if (failures == 0) std::printf("OK tube_occt\n");
    return failures == 0 ? 0 : 1;
}
