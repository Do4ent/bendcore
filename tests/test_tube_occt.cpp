// Тело трубы на Open CASCADE (geom::make_solid_tube): валидность и объём π·r²·L,
// где L — длина оси из ядра bendcore (axis_length). Протяжка без скручивания: объём точный.
#include "bendcore/tube_body.hpp"
#include "bendcore/tubegeom.hpp"

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

// Труба со стенкой: объём π·(r² − r_в²)·L (точно при r < радиуса гиба), кольцевые торцы дают порты по наружной окружности.
static void check_wall() {
    const double pi = std::acos(-1.0);
    const double r = 10.0;
    const double wall = 2.0;
    const double ri = r - wall;
    const Path path{Straight{100}, Bend{50, 90}, Straight{100}};
    const double L = axis_length(path);
    const auto res = geom::make_tube(path, 2.0 * r, wall);
    const double V_expected = pi * (r * r - ri * ri) * L;
    std::printf("стенка 2 мм: объём %.4f мм³, π·(r²−r_в²)·L %.4f мм³, валидно=%d\n", res.volume, V_expected,
                res.valid ? 1 : 0);
    check(res.valid, "стенка: тело валидно");
    check(std::abs(res.volume - V_expected) <= 1e-4 * V_expected, "стенка: объём");

    const auto ports = geom::end_ports(path, 2.0 * r, wall);
    check(ports.size() == 2, "стенка: два порта на концах (кольца дают по одному)");
    for (const auto& port : ports) check(std::abs(port.od - 2.0 * r) < 1e-6, "стенка: ∅ порта = наружный ∅");
}

// Порт на грани: ближайшая точка наружной поверхности к (50, 5, 0) — (50, 10, 0), наружу — +Y.
static void check_face_port() {
    const double r = 10.0;
    const Path path{Straight{100}};
    const Port port = geom::face_port(path, 2.0 * r, Vec3{50.0, 5.0, 0.0}, "грань");
    std::printf("порт на грани: pos (%.3f, %.3f, %.3f), dir (%.3f, %.3f, %.3f), od %.3f\n", port.pos[0], port.pos[1],
                port.pos[2], port.direction[0], port.direction[1], port.direction[2], port.od);
    check(norm(sub(port.pos, Vec3{50.0, 10.0, 0.0})) < 1e-4, "порт на грани: точка на поверхности");
    check(norm(sub(port.direction, Vec3{0.0, 1.0, 0.0})) < 1e-4, "порт на грани: нормаль наружу");
    check(std::abs(port.od - 2.0 * r) < 1e-9, "порт на грани: ∅");
}

// Полная модель: ручной порт + концевые порты; авто-имена не совпадают с ручным.
static void check_complete_model() {
    const double r = 10.0;
    TubeModel m;
    m.id = "t";
    m.source = "csv";
    m.od = 2.0 * r;
    m.path = Path{Straight{100}, Bend{50, 90}, Straight{100}};
    m.ports.push_back(manual_port({50.0, 0.0, 0.0}, {0.0, -1.0, 0.0}, 2.0 * r, "tube.П1"));
    const TubeModel full = geom::complete_model(m);
    check(full.ports.size() == 3, "полная модель: ручной порт и два концевых");
    if (full.ports.size() == 3) {
        check(full.ports[0].name == "tube.П1", "полная модель: ручной порт сохранён");
        check(full.ports[1].name != "tube.П1" && full.ports[2].name != "tube.П1", "полная модель: имена не совпадают");
        check(full.ports[1].name != full.ports[2].name, "полная модель: концевые имена различаются");
    }
    TubeModel stepModel;
    stepModel.source = "step";
    stepModel.od = 20.0;
    check(!geom::complete_model(stepModel).path.has_value(), "полная модель: труба из STEP без изменений");
}

// Круговой проход: тело → STEP → load_step. ∅ и число концевых портов сохраняются.
static void check_step_roundtrip(double wall, const char* file, const char* name) {
    const double od = 20.0;
    const Path path{Straight{100}, Bend{50, 90}, Straight{100}};
    geom::export_step(path, od, wall, file);
    const TubeModel m = geom::load_step(file, "из STEP");
    std::printf("%s: портов %zu, ∅ %.4f\n", name, m.ports.size(), m.od);
    check(m.source == "step" && m.id == "из STEP", "STEP: источник и id");
    check(m.ports.size() == 2, name);
    check(std::abs(m.od - od) < 1e-6, name);
    check(std::abs(m.wall - wall) < 1e-6, name);
    check(!m.path.has_value(), name);
}

static void check_step_errors() {
    bool threw = false;
    try {
        geom::load_step("no_such_file_bendcore.step", "x");
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "STEP: несуществующий файл — исключение");
}

// Сетка: объём, посчитанный по треугольникам (теорема о дивергенции), совпадает с объёмом тела.
static double mesh_volume(const geom::TubeMesh& mesh) {
    double v = 0.0;
    for (std::size_t t = 0; t + 2 < mesh.indices.size(); t += 3) {
        const double* a = &mesh.vertices[3 * mesh.indices[t]];
        const double* b = &mesh.vertices[3 * mesh.indices[t + 1]];
        const double* c = &mesh.vertices[3 * mesh.indices[t + 2]];
        v += a[0] * (b[1] * c[2] - b[2] * c[1]) - a[1] * (b[0] * c[2] - b[2] * c[0]) + a[2] * (b[0] * c[1] - b[1] * c[0]);
    }
    return v / 6.0;
}

static void check_mesh(const Path& path, double od, double wall, const char* name) {
    const geom::TubeMesh mesh = geom::make_mesh(path, od, wall, 0.01);
    const std::size_t nv = mesh.vertices.size() / 3;
    bool indicesOk = !mesh.indices.empty() && mesh.indices.size() % 3 == 0;
    for (int i : mesh.indices) indicesOk = indicesOk && i >= 0 && static_cast<std::size_t>(i) < nv;
    const double vMesh = mesh_volume(mesh);
    const double vBody = geom::make_tube(path, od, wall).volume;
    std::printf("%s: вершин %zu, треугольников %zu, объём сетки %.4f, объём тела %.4f\n", name, nv,
                mesh.indices.size() / 3, vMesh, vBody);
    check(indicesOk, name);
    check(std::abs(vMesh - vBody) <= 1e-2 * vBody, name);
}

int main() {
    check_mesh({Straight{100}, Bend{50, 90}, Straight{100}}, 20.0, 0.0, "сетка: гиб 90°");
    check_mesh({Straight{100}, Bend{50, 90}, Straight{100}}, 20.0, 2.0, "сетка: гиб 90° со стенкой");
    check_complete_model();
    check_step_roundtrip(0.0, "tube_roundtrip_solid.step", "STEP сплошной: два порта");
    check_step_roundtrip(2.0, "tube_roundtrip_wall.step", "STEP со стенкой: два порта по наружной окружности");
    check_step_errors();
    check_body({Straight{100}, Bend{50, 90}, Straight{100}}, 20.0, "плоский гиб 90°");
    check_body({Straight{50}, Bend{40, 60, 0}, Straight{30}, Bend{40, -70, 90}, Straight{40}}, 16.0,
               "два гиба, второй с поворотом A");
    check_body({Straight{20}, Bend{30, 45, 45}, Straight{20}, Bend{30, 45, 0}, Straight{10}}, 12.0,
               "пространственный путь");
    // Спираль: отсчёты и сплайн дают приближение длины, допуск 1e-3.
    check_body({Straight{50}, Spiral{50, 30, 2, +1}, Straight{50}}, 20.0, "спираль в прямых", 1e-3);
    check_body({Straight{40}, Bend{50, 90}, Spiral{50, 30, 1.5, -1}, Straight{20}}, 20.0, "гиб и спираль", 1e-3);
    check_end_ports();
    check_wall();
    check_face_port();
    // Порты только на концах: швы и стыки спирали и гибов портов не дают.
    check(geom::end_ports({Straight{50}, Spiral{50, 30, 2, +1}, Straight{50}}, 20.0).size() == 2, "спираль: два порта");
    check(geom::end_ports({Straight{40}, Bend{50, 90}, Spiral{50, 30, 1.5, -1}, Straight{20}}, 20.0).size() == 2,
          "гиб и спираль: два порта");
    if (failures == 0) std::printf("OK tube_occt\n");
    return failures == 0 ? 0 : 1;
}
