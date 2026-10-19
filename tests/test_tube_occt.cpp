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


// Порты по кромкам концов: положение, наружное направление и ∅ сверяются с аналитикой пути.
static void check_end_ports() {
    const double r = 10.0;
    const Path path{Straight{100}, Bend{50, 90}, Straight{100}};
    const auto ports = geom::end_ports(path, 2.0 * r);
    std::printf("порты на концах: %zu\n", ports.size());
    for (const auto& port : ports) {
        std::printf("  %s: pos (%.3f, %.3f, %.3f), dir (%.3f, %.3f, %.3f), od %.3f\n", port.name.c_str(),
                    port.pos[0], port.pos[1], port.pos[2], port.direction[0], port.direction[1], port.direction[2], port.od);
    }
    check(ports.size() == 2, "два порта на концах");
    if (ports.size() != 2) return;
    // начало: (0,0,0), наружу −X; конец: (150,150,0)... по трассировке конец — последняя точка оси
    const auto pts = trace(path, 64);
    const Vec3 start = pts.front();
    const Vec3 end = pts.back();
    const Vec3 endDir = {0.0, 1.0, 0.0};   // касательная в конце (после гиба и прямой вдоль +Y)
    for (const auto& port : ports) check(std::abs(port.od - 2.0 * r) < 1e-6, "∅ порта = ∅ трубы");
    const auto& pa = ports[0];
    const auto& pb = ports[1];
    const bool startFirst = norm(sub(pa.pos, start)) < 1e-6;
    const auto& ps = startFirst ? pa : pb;
    const auto& pe = startFirst ? pb : pa;
    check(norm(sub(ps.pos, start)) < 1e-6, "порт в начале совпадает с началом оси");
    check(norm(sub(pe.pos, end)) < 1e-6, "порт в конце совпадает с концом оси");
    check(norm(sub(ps.direction, Vec3{-1.0, 0.0, 0.0})) < 1e-6, "наружу в начале = −касательная");
    check(norm(sub(pe.direction, endDir)) < 1e-6, "наружу в конце = касательная");
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
    check_end_ports();
    // Порты только на концах: швы и стыки спирали и гибов портов не дают.
    check(geom::end_ports({Straight{50}, Spiral{50, 30, 2, +1}, Straight{50}}, 20.0).size() == 2, "спираль: два порта");
    check(geom::end_ports({Straight{40}, Bend{50, 90}, Spiral{50, 30, 1.5, -1}, Straight{20}}, 20.0).size() == 2,
          "гиб и спираль: два порта");
    if (failures == 0) std::printf("OK tube_occt\n");
    return failures == 0 ? 0 : 1;
}
