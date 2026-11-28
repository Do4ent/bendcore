#include <bendcore/drawing_v2/model.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <string>
#include <utility>

namespace bendcore::drawing_v2 {

namespace {

constexpr double kEps = 1e-9;

bool finite(Point2 p) { return std::isfinite(p.x) && std::isfinite(p.y); }

Status invalid(std::string message) { return Error{ErrorCode::InvalidInput, std::move(message)}; }

bool validLayerName(const std::string& name) {
    if (name.empty() || name.size() > 255) return false;
    for (char c : name) {
        const bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                        (c >= '0' && c <= '9') || c == '_' || c == '-';
        if (!ok) return false;
    }
    return true;
}

Status validateText(const Text& t) {
    if (!finite(t.position) || !std::isfinite(t.rotationDeg)) return invalid("text: non-finite value");
    bool heightOk = false;
    for (double h : kGostTextHeights) heightOk = heightOk || std::fabs(h - t.height) < kEps;
    if (!heightOk) {
        return invalid("GOST 2.304: text height must be one of 2.5, 3.5, 5, 7, 10, 14, 20 mm, got " +
                       std::to_string(t.height));
    }
    if (!std::isfinite(t.widthFactor) || t.widthFactor < 0.5 - kEps || t.widthFactor > 1.0 + kEps) {
        return invalid("text: width factor must be 0.5..1.0");
    }
    if (t.utf8.empty()) return invalid("text: empty string");
    auto decoded = decodeUtf8(t.utf8);
    if (!decoded) return decoded.error();
    for (char32_t cp : decoded.value()) {
        if (cp < 0x20 || cp == 0x7F) return invalid("text: control characters are not allowed");
    }
    return okStatus();
}

Status validateGeometry(const Entity& e, const LineTypeSpec& spec) {
    return std::visit(
        [&](const auto& g) -> Status {
            using T = std::decay_t<decltype(g)>;
            if constexpr (std::is_same_v<T, LineSeg>) {
                if (!finite(g.a) || !finite(g.b)) return invalid("line: non-finite point");
                const double len = std::hypot(g.b.x - g.a.x, g.b.y - g.a.y);
                if (len < kEps) return invalid("line: zero length");
                if (e.lineType == LineType::Open &&
                    (len < spec.strokeMin - kEps || len > spec.strokeMax + kEps)) {
                    return invalid("GOST 2.303: open line stroke must be 8..20 mm long");
                }
            } else {
                if (e.lineType == LineType::Open) {
                    return invalid("GOST 2.303: open line is drawn only as separate strokes (LineSeg)");
                }
            }
            if constexpr (std::is_same_v<T, Arc>) {
                if (!finite(g.center) || !std::isfinite(g.radius) || !std::isfinite(g.startDeg) ||
                    !std::isfinite(g.endDeg)) {
                    return invalid("arc: non-finite value");
                }
                if (g.radius <= kEps) return invalid("arc: radius must be positive");
                double sweep = std::fmod(g.endDeg - g.startDeg, 360.0);
                if (sweep < 0) sweep += 360.0;
                if (sweep < kEps) return invalid("arc: zero sweep (use Circle for a full circle)");
            } else if constexpr (std::is_same_v<T, Circle>) {
                if (!finite(g.center) || !std::isfinite(g.radius)) return invalid("circle: non-finite value");
                if (g.radius <= kEps) return invalid("circle: radius must be positive");
            } else if constexpr (std::is_same_v<T, Polyline>) {
                const std::size_t need = g.closed ? 3 : 2;
                if (g.points.size() < need) {
                    return invalid("polyline: needs at least " + std::to_string(need) + " points");
                }
                for (const auto& p : g.points) {
                    if (!finite(p)) return invalid("polyline: non-finite point");
                }
            } else if constexpr (std::is_same_v<T, Text>) {
                return validateText(g);
            } else if constexpr (std::is_same_v<T, SolidFill>) {
                if (g.points.size() < 3 || g.points.size() > 64) {
                    return invalid("solid fill: needs 3..64 points");
                }
                for (const auto& p : g.points) {
                    if (!finite(p)) return invalid("solid fill: non-finite point");
                }
                // Выпуклость: все векторные произведения соседних рёбер одного знака.
                const std::size_t n = g.points.size();
                double area2 = 0.0;
                int sign = 0;
                for (std::size_t i = 0; i < n; ++i) {
                    const Point2 a = g.points[i], b = g.points[(i + 1) % n], c = g.points[(i + 2) % n];
                    area2 += a.x * b.y - b.x * a.y;
                    const double cr = (b.x - a.x) * (c.y - b.y) - (b.y - a.y) * (c.x - b.x);
                    if (std::abs(cr) < 1e-12) continue;
                    const int sg = cr > 0 ? 1 : -1;
                    if (sign != 0 && sg != sign) return invalid("solid fill: polygon must be convex");
                    sign = sg;
                }
                if (std::abs(area2) < 1e-9) return invalid("solid fill: zero area");
            } else if constexpr (std::is_same_v<T, Hatch>) {
                if (e.lineType != LineType::SolidThin) {
                    return invalid("GOST 2.306: hatch lines are solid thin lines");
                }
                if (!std::isfinite(g.angleDeg)) return invalid("hatch: non-finite angle");
                if (!std::isfinite(g.spacing) || g.spacing < 1.0 - kEps || g.spacing > 10.0 + kEps) {
                    return invalid("GOST 2.306: hatch spacing must be 1..10 mm");
                }
                if (g.loops.empty()) return invalid("hatch: no boundary loops");
                for (const auto& loop : g.loops) {
                    if (loop.size() < 3) return invalid("hatch: boundary loop needs at least 3 points");
                    for (const auto& p : loop) {
                        if (!finite(p)) return invalid("hatch: non-finite point");
                    }
                }
            }
            return okStatus();
        },
        e.geometry);
}

}  // namespace

Drawing::Drawing(Sheet sheet, double s) : sheet_(std::move(sheet)), s_(s) {
    layers_.push_back(Layer{"0", 7});
    for (LineType t : kAllLineTypes) specs_.emplace(t, defaultLineTypeSpec(t));
}

Result<Drawing> Drawing::create(Sheet sheet, double s) {
    if (auto st = validateMainThickness(s); !st) return st.error();
    if (!(sheet.widthMm > 0.0) || !(sheet.heightMm > 0.0) || !std::isfinite(sheet.widthMm) ||
        !std::isfinite(sheet.heightMm)) {
        return Error{ErrorCode::InvalidInput, "sheet size must be positive"};
    }
    return Drawing(std::move(sheet), s);
}

Status Drawing::addLayer(Layer layer) {
    if (!validLayerName(layer.name)) {
        return invalid("layer name must be non-empty ASCII [A-Za-z0-9_-]: '" + layer.name + "'");
    }
    if (layer.aciColor < 1 || layer.aciColor > 255) return invalid("layer color must be ACI 1..255");
    if (findLayer(layer.name)) return invalid("layer already exists: " + layer.name);
    layers_.push_back(std::move(layer));
    return okStatus();
}

const Layer* Drawing::findLayer(const std::string& name) const {
    for (const auto& l : layers_) {
        if (l.name == name) return &l;
    }
    return nullptr;
}

Status Drawing::setLineTypeSpec(const LineTypeSpec& spec) {
    if (auto st = validateLineTypeSpec(spec); !st) return st;
    for (const auto& [type, other] : specs_) {
        if (type != spec.type && !other.pattern.empty() && other.dxfName == spec.dxfName) {
            return invalid("DXF line type name already used: " + spec.dxfName);
        }
    }
    specs_[spec.type] = spec;
    return okStatus();
}

const LineTypeSpec& Drawing::lineTypeSpec(LineType type) const { return specs_.at(type); }

double Drawing::lineWidth(LineType type) const { return specs_.at(type).widthFactor * s_; }

Status Drawing::add(Entity entity) {
    if (!findLayer(entity.layer)) return invalid("unknown layer: " + entity.layer);
    if (auto st = validateGeometry(entity, specs_.at(entity.lineType)); !st) return st;
    entities_.push_back(std::move(entity));
    return okStatus();
}

Result<Polyline> makeWavyLine(Point2 a, Point2 b, double amplitude, double wavelength) {
    if (!finite(a) || !finite(b) || !std::isfinite(amplitude) || !std::isfinite(wavelength)) {
        return Error{ErrorCode::InvalidInput, "wavy line: non-finite value"};
    }
    const double dx = b.x - a.x, dy = b.y - a.y;
    const double len = std::hypot(dx, dy);
    if (len < kEps) return Error{ErrorCode::InvalidInput, "wavy line: zero length"};
    if (amplitude <= 0.0 || wavelength <= 0.0) {
        return Error{ErrorCode::InvalidInput, "wavy line: amplitude and wavelength must be positive"};
    }
    const int waves = std::max(1, int(std::lround(len / wavelength)));
    const int samples = waves * 32;
    const double ux = dx / len, uy = dy / len;
    Polyline pl;
    pl.points.reserve(std::size_t(samples) + 1);
    for (int i = 0; i <= samples; ++i) {
        const double t = double(i) / samples;
        const double off = amplitude * std::sin(2.0 * std::numbers::pi * waves * t);
        pl.points.push_back({a.x + dx * t - uy * off, a.y + dy * t + ux * off});
    }
    // Концы — точно в a и b (синус в концах равен нулю, убираем погрешность округления).
    pl.points.front() = a;
    pl.points.back() = b;
    return pl;
}

Result<Polyline> makeZigzagLine(Point2 a, Point2 b, int count, double zigHeight, double zigWidth) {
    if (!finite(a) || !finite(b) || !std::isfinite(zigHeight) || !std::isfinite(zigWidth)) {
        return Error{ErrorCode::InvalidInput, "zigzag line: non-finite value"};
    }
    const double dx = b.x - a.x, dy = b.y - a.y;
    const double len = std::hypot(dx, dy);
    if (count < 1) return Error{ErrorCode::InvalidInput, "zigzag line: count must be >= 1"};
    if (zigHeight <= 0.0 || zigWidth <= 0.0) {
        return Error{ErrorCode::InvalidInput, "zigzag line: sizes must be positive"};
    }
    if (len < count * zigWidth + kEps) {
        return Error{ErrorCode::InvalidInput, "zigzag line: too short for the requested breaks"};
    }
    const double ux = dx / len, uy = dy / len;
    auto at = [&](double s, double off) {
        return Point2{a.x + ux * s - uy * off, a.y + uy * s + ux * off};
    };
    Polyline pl;
    pl.points.push_back(a);
    for (int k = 0; k < count; ++k) {
        const double c = len * (k + 0.5) / count;  // центр излома
        const double w = zigWidth;
        pl.points.push_back(at(c - w / 2.0, 0.0));
        pl.points.push_back(at(c - w / 6.0, zigHeight / 2.0));
        pl.points.push_back(at(c + w / 6.0, -zigHeight / 2.0));
        pl.points.push_back(at(c + w / 2.0, 0.0));
    }
    pl.points.push_back(b);
    return pl;
}

Result<std::vector<LineSeg>> hatchSegments(const Hatch& hatch) {
    if (!std::isfinite(hatch.spacing) || hatch.spacing <= 0.0 || !std::isfinite(hatch.angleDeg)) {
        return Error{ErrorCode::InvalidInput, "hatch: invalid spacing or angle"};
    }
    const double ang = hatch.angleDeg * std::numbers::pi / 180.0;
    const double c = std::cos(ang), s = std::sin(ang);
    // Локальная система: u — вдоль линий штриховки, v — по нормали.
    struct UV {
        double u, v;
    };
    std::vector<std::vector<UV>> loops;
    double vmin = INFINITY, vmax = -INFINITY;
    for (const auto& loop : hatch.loops) {
        if (loop.size() < 3) return Error{ErrorCode::InvalidInput, "hatch: loop needs >= 3 points"};
        std::vector<UV> l;
        for (const auto& p : loop) {
            if (!finite(p)) return Error{ErrorCode::InvalidInput, "hatch: non-finite point"};
            const UV q{p.x * c + p.y * s, -p.x * s + p.y * c};
            vmin = std::min(vmin, q.v);
            vmax = std::max(vmax, q.v);
            l.push_back(q);
        }
        loops.push_back(std::move(l));
    }
    std::vector<LineSeg> out;
    const long kFirst = long(std::ceil(vmin / hatch.spacing - 0.5));
    const long kLast = long(std::floor(vmax / hatch.spacing - 0.5));
    std::vector<double> xs;
    for (long k = kFirst; k <= kLast; ++k) {
        const double v = (double(k) + 0.5) * hatch.spacing;
        xs.clear();
        for (const auto& l : loops) {
            for (std::size_t i = 0; i < l.size(); ++i) {
                const UV p = l[i];
                const UV q = l[(i + 1) % l.size()];
                // Полуоткрытое правило: ребро учитывается, если v ∈ [min, max).
                const bool crosses = (p.v <= v && v < q.v) || (q.v <= v && v < p.v);
                if (!crosses) continue;
                const double t = (v - p.v) / (q.v - p.v);
                xs.push_back(p.u + t * (q.u - p.u));
            }
        }
        std::sort(xs.begin(), xs.end());
        for (std::size_t i = 0; i + 1 < xs.size(); i += 2) {
            const double u0 = xs[i], u1 = xs[i + 1];
            if (u1 - u0 < kEps) continue;
            out.push_back({{u0 * c - v * s, u0 * s + v * c}, {u1 * c - v * s, u1 * s + v * c}});
        }
    }
    return out;
}

}  // namespace bendcore::drawing_v2
