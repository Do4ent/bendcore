#include "bendcore/ports.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace bendcore {

void Port::flip() { direction = mul(direction, -1.0); }

Vec3 Port::unit_dir() const { return unit(direction); }

double first_straight(const Path& path) {
    double total = 0.0;
    for (const auto& it : path) {
        if (const auto* s = std::get_if<Straight>(&it)) {
            total += s->S;
        } else {
            break;
        }
    }
    return total;
}

double last_straight(const Path& path) {
    double total = 0.0;
    for (auto it = path.rbegin(); it != path.rend(); ++it) {
        if (const auto* s = std::get_if<Straight>(&*it)) {
            total += s->S;
        } else {
            break;
        }
    }
    return total;
}

namespace {

// Угол между векторами, градусы; atan2 — как в ports.py (точность при малых углах).
double angle_deg(const Vec3& a, const Vec3& b) {
    return std::atan2(norm(cross(a, b)), dot(a, b)) * 180.0 / 3.14159265358979323846;
}

}  // namespace

std::vector<Issue> check_connection(const Port& port, const Path& path, double od, const std::string& tube_id,
                                    char end, std::optional<Vec3> end_pos, std::optional<Vec3> end_dir) {
    std::vector<Issue> issues;
    if (port.occupied_by && *port.occupied_by != tube_id) {
        issues.push_back({"refused", -1, "порт занят другой трубой"});
    }
    if (port.associative && !port.ref_ok) {
        issues.push_back({"ref_lost", -1, "ссылка порта потеряна"});
    }
    if (std::abs(port.od - od) > 1e-9) {
        issues.push_back({"warning", -1, "∅ порта не совпадает с ∅ трубы"});
    }
    const double straight = end == 'A' ? first_straight(path) : last_straight(path);
    if (straight < port.min_straight) {
        issues.push_back({"technological", -1, "вылет прямого участка меньше минимального у порта"});
    }
    if (end_pos) {
        const double dev = norm(sub(*end_pos, port.pos));
        if (dev > port.tol_pos) issues.push_back({"warning", -1, "конец трубы смещён от порта"});
    }
    if (end_dir) {
        const double ang = angle_deg(*end_dir, port.unit_dir());
        if (ang > port.tol_ang) issues.push_back({"warning", -1, "направление конца отклонено от порта"});
    }
    return issues;
}

std::string auto_name(const std::string& component, const std::vector<std::string>& existing) {
    // «П» — кириллица, UTF-8
    for (int k = 1;; ++k) {
        const std::string name = component + ".П" + std::to_string(k);
        if (std::find(existing.begin(), existing.end(), name) == existing.end()) return name;
    }
}

std::optional<Issue> socket_hint(const Port& port, double E_current, bool decided) {
    if (!port.socket_depth || decided || std::abs(E_current - *port.socket_depth) < 1e-9) return std::nullopt;
    return Issue{"info", -1, "По раструбу: предложено применить глубину"};
}

std::pair<Vec3, Vec3> start_from_port(const Port& port) { return {port.pos, port.unit_dir()}; }

std::vector<const Port*> candidates(const std::vector<Port>& ports, double od) {
    std::vector<const Port*> out;
    for (const auto& p : ports) {
        if (!p.occupied_by && p.ref_ok && std::abs(p.od - od) < 1e-9) out.push_back(&p);
    }
    return out;
}

std::vector<Issue> connect(Port& port, const std::string& tube_id, const Path& path, double od) {
    auto issues = check_connection(port, path, od, tube_id);
    const bool refused = std::any_of(issues.begin(), issues.end(), [](const Issue& i) { return i.kind == "refused"; });
    if (!refused) port.occupied_by = tube_id;
    return issues;
}

}  // namespace bendcore
