#include "bendcore/scene.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <stdexcept>

namespace bendcore {

namespace {

constexpr double kTwoPi = 6.28318530717958647692;

// Тяжесть класса замечания: больше — тяжелее. 0 — состояния не меняет.
int severity(const std::string& kind) {
    if (kind == "geometric") return 6;
    if (kind == "technological") return 5;
    if (kind == "mark_cross") return 4;
    if (kind == "mark_gap") return 3;
    if (kind == "info" || kind == "suggestion") return 1;
    if (kind == "ref_lost") return 0;
    return 2;   // warning, refused и неизвестные классы
}

const char* state_of_severity(int s) {
    switch (s) {
        case 6: return "err_geom";
        case 5: return "err_tech";
        case 4: return "mark_cross";
        case 3: return "mark_gap";
        case 2: return "warn";
        case 1: return "info";
        default: return "norm";
    }
}

struct Box {
    Vec3 lo{};
    Vec3 hi{};
    bool empty = true;

    void add(const Vec3& p, double pad) {
        for (int k = 0; k < 3; ++k) {
            if (empty) {
                lo[k] = p[k] - pad;
                hi[k] = p[k] + pad;
            } else {
                lo[k] = std::min(lo[k], p[k] - pad);
                hi[k] = std::max(hi[k], p[k] + pad);
            }
        }
        empty = false;
    }

    std::optional<ViewFit> fit() const {
        if (empty) return std::nullopt;
        ViewFit f;
        f.lo = lo;
        f.hi = hi;
        f.center = mul(add_vec(lo, hi), 0.5);
        f.radius = 0.5 * norm(sub(hi, lo));
        return f;
    }

    static Vec3 add_vec(const Vec3& a, const Vec3& b) { return {a[0] + b[0], a[1] + b[1], a[2] + b[2]}; }
};

std::string num(double v) {
    if (!std::isfinite(v)) return "null";
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.10g", v);
    return buf;
}

std::string quote(const std::string& s) {
    std::string out = "\"";
    for (unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof buf, "\\u%04x", c);
                    out += buf;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    return out + "\"";
}

std::string vec(const Vec3& v) { return "[" + num(v[0]) + "," + num(v[1]) + "," + num(v[2]) + "]"; }

std::string vec_list(const std::vector<Vec3>& pts) {
    std::string out = "[";
    for (std::size_t i = 0; i < pts.size(); ++i) out += (i ? "," : "") + vec(pts[i]);
    return out + "]";
}

std::string str_or_null(const std::optional<std::string>& s) { return s ? quote(*s) : "null"; }

std::string fit_json(const std::optional<ViewFit>& f) {
    if (!f) return "null";
    return "{\"lo\":" + vec(f->lo) + ",\"hi\":" + vec(f->hi) + ",\"center\":" + vec(f->center) +
           ",\"radius\":" + num(f->radius) + "}";
}

PortGlyph make_glyph(const Port& port, const SceneOptions& opt, const std::map<std::string, const SceneTube*>& by_id) {
    PortGlyph g;
    g.name = port.name;
    g.pos = port.pos;
    g.dir = port.unit_dir();   // нулевое направление — std::invalid_argument
    g.od = port.od;
    g.occupied_by = port.occupied_by;
    g.ref_ok = port.ref_ok;

    const int n = std::max(8, opt.ring_segments);
    const Frame f = frame_from_port(port);   // y, z — оси плоскости окружности
    const double r = 0.5 * port.od;
    g.ring.reserve(static_cast<std::size_t>(n) + 1);
    for (int k = 0; k <= n; ++k) {
        const double t = k == n ? 0.0 : kTwoPi * k / n;   // последняя точка совпадает с первой точно
        g.ring.push_back(add(port.pos, add(mul(f.y, r * std::cos(t)), mul(f.z, r * std::sin(t)))));
    }
    const double arrow = opt.arrow_len > 0.0 ? opt.arrow_len : 1.5 * port.od;
    g.arrow_tip = add(port.pos, mul(g.dir, arrow));

    g.show_zone = opt.trace || opt.selected_ports.count(port.name) > 0;
    if (g.show_zone) {
        if (port.min_straight > 0.0) g.zone = std::make_pair(port.pos, add(port.pos, mul(g.dir, port.min_straight)));
        if (port.occupied_by) {
            const auto it = by_id.find(*port.occupied_by);
            if (it != by_id.end() && !it->second->points.empty()) {
                const auto& pts = it->second->points;
                const Vec3& first = pts.front();
                const Vec3& last = pts.back();
                const Vec3& end = norm(sub(first, port.pos)) <= norm(sub(last, port.pos)) ? first : last;
                g.link = std::make_pair(port.pos, end);
            }
        }
    }
    return g;
}

}  // namespace

std::string state_from_issues(const std::vector<Issue>& issues) {
    int worst = 0;
    for (const auto& i : issues) worst = std::max(worst, severity(i.kind));
    return state_of_severity(worst);
}

std::vector<std::string> flags_from_issues(const std::vector<Issue>& issues) {
    std::vector<std::string> flags;
    for (const auto& i : issues) {
        if (i.kind == "ref_lost") {
            flags.push_back("ref_lost");
            break;
        }
    }
    return flags;
}

std::optional<ViewFit> fit_points(const std::vector<Vec3>& points, double pad) {
    Box b;
    for (const auto& p : points) b.add(p, pad);
    return b.fit();
}

Scene build_scene(const std::vector<SceneTubeInput>& tubes, const std::vector<Port>& ports, const SceneOptions& options) {
    Scene scene;

    std::set<std::string> seen;
    std::vector<PlacedTube> placed;
    placed.reserve(tubes.size());
    for (const auto& in : tubes) {
        if (!seen.insert(in.tube.id).second) throw std::invalid_argument("повтор идентификатора трубы: " + in.tube.id);
        placed.push_back(in.tube);
    }
    std::set<std::string> port_names;
    for (const auto& p : ports) {
        if (!port_names.insert(p.name).second) throw std::invalid_argument("повтор имени порта: " + p.name);
    }

    scene.marks = clearance_marks(placed, options.gap, options.skip_pairs);

    scene.tubes.reserve(tubes.size());
    for (const auto& in : tubes) {
        SceneTube t;
        t.id = in.tube.id;
        t.points = in.tube.centerline;
        t.od = in.tube.od;
        t.issues = in.issues;
        for (const auto& m : scene.marks) {
            if (m.tube_a == t.id || m.tube_b == t.id) t.issues.push_back({m.kind, -1, clearance_text(m)});
        }
        t.state = state_from_issues(t.issues);
        t.flags = flags_from_issues(t.issues);
        t.selected = options.selected_tubes.count(t.id) > 0;
        scene.tubes.push_back(std::move(t));
    }

    std::map<std::string, const SceneTube*> by_id;
    for (const auto& t : scene.tubes) by_id[t.id] = &t;

    scene.ports.reserve(ports.size());
    for (const auto& p : ports) scene.ports.push_back(make_glyph(p, options, by_id));

    Box all;
    Box sel;
    for (const auto& t : scene.tubes) {
        for (const auto& p : t.points) {
            all.add(p, 0.5 * t.od);
            if (t.selected) sel.add(p, 0.5 * t.od);
        }
    }
    for (const auto& g : scene.ports) {
        all.add(g.pos, 0.5 * g.od);
        if (options.selected_ports.count(g.name) > 0) sel.add(g.pos, 0.5 * g.od);
    }
    scene.fit_all = all.fit();
    scene.fit_selected = sel.fit();
    return scene;
}

std::string scene_to_json(const Scene& scene) {
    std::string out = "{\"format\":\"bendcore-scene\",\"version\":1,\"tubes\":[";
    for (std::size_t i = 0; i < scene.tubes.size(); ++i) {
        const SceneTube& t = scene.tubes[i];
        out += i ? "," : "";
        out += "{\"id\":" + quote(t.id) + ",\"od\":" + num(t.od) + ",\"state\":" + quote(t.state) + ",\"flags\":[";
        for (std::size_t k = 0; k < t.flags.size(); ++k) out += (k ? "," : "") + quote(t.flags[k]);
        out += "],\"selected\":" + std::string(t.selected ? "true" : "false") + ",\"points\":" + vec_list(t.points) +
               ",\"issues\":[";
        for (std::size_t k = 0; k < t.issues.size(); ++k) {
            out += (k ? "," : "") + std::string("{\"kind\":") + quote(t.issues[k].kind) +
                   ",\"text\":" + quote(t.issues[k].text) + "}";
        }
        out += "]}";
    }
    out += "],\"ports\":[";
    for (std::size_t i = 0; i < scene.ports.size(); ++i) {
        const PortGlyph& g = scene.ports[i];
        out += i ? "," : "";
        out += "{\"name\":" + quote(g.name) + ",\"pos\":" + vec(g.pos) + ",\"dir\":" + vec(g.dir) +
               ",\"od\":" + num(g.od) + ",\"ring\":" + vec_list(g.ring) + ",\"arrowTip\":" + vec(g.arrow_tip) +
               ",\"occupiedBy\":" + str_or_null(g.occupied_by) + ",\"refOk\":" + (g.ref_ok ? "true" : "false") +
               ",\"showZone\":" + (g.show_zone ? "true" : "false") + ",\"zone\":";
        out += g.zone ? "{\"from\":" + vec(g.zone->first) + ",\"to\":" + vec(g.zone->second) +
                            ",\"radius\":" + num(0.5 * g.od) + "}"
                      : "null";
        out += ",\"link\":";
        out += g.link ? "{\"from\":" + vec(g.link->first) + ",\"to\":" + vec(g.link->second) + "}" : "null";
        out += "}";
    }
    out += "],\"marks\":[";
    for (std::size_t i = 0; i < scene.marks.size(); ++i) {
        const ClearanceMark& m = scene.marks[i];
        const Vec3 at = mul(add(m.point_a, m.point_b), 0.5);
        out += i ? "," : "";
        out += "{\"kind\":" + quote(m.kind) + ",\"tubeA\":" + quote(m.tube_a) + ",\"tubeB\":" + quote(m.tube_b) +
               ",\"pointA\":" + vec(m.point_a) + ",\"pointB\":" + vec(m.point_b) + ",\"at\":" + vec(at) +
               ",\"surfaceGap\":" + num(m.surface_gap) + ",\"norm\":" + num(m.norm) + "}";
    }
    out += "],\"fitAll\":" + fit_json(scene.fit_all) + ",\"fitSelected\":" + fit_json(scene.fit_selected) + "}";
    return out;
}

}  // namespace bendcore
